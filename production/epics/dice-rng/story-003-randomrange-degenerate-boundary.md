---
Epic: dice-rng
Status: Complete
Layer: Foundation
Type: Logic
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-07
---

# Story 003 — RandomRange 退化与边界契约

## Context

- **GDD**: `design/gdd/dice.md` — F-2（RandomRange 闭区间整数均匀 + 退化与边界契约）/ Edge Cases（Min>Max / Min==Max / 跨度溢出）/ F-4 前提④（Min==Max 不推进 Seed）。
- **Requirement TR-ID**: TR-dice-003。
- **ADR Governing**:
  - **ADR-0004（确定性 RNG，primary）** — 钉死封装层退化契约：`Min==Max` early-return 返回 Min 不调流（Seed 不推进）；`Min>Max` ensure 不自动交换；`Range = Max−Min+1 < 2^24`（走 `FRand()` 浮点中介，超 2^24 精度崩）。
  - **ADR-0007（BP-C++ 边界）** — 封装层退化硬门落 C++，可加 ensure/UE_LOG。
- **Engine**: UE 5.7。Risk: **MEDIUM**（`RandRange` 内部走 `FRand()` 浮点中介是 2^24 约束的根因；`Min>Max` 时引擎层行为未明文保证 —— 封装层加硬门拦截，须实现期查源码）。
- **Engine Notes**（ADR-0004 / dice F-2，逐字保真）:
  - `RandRange` 内部走 `FRand()*Range` 浮点中介（见 UE5.7 标注 L172 / F-4），当 `Range ≥ 2²⁴` 时浮点乘积精度崩溃（float 尾数 23 位）、均匀性破坏——与 F-3 `floor(f*N)` 的 `N<2²⁴` 上界同因同源（整数路径实走浮点，与 F-3 对称，非"整数比浮点安全"）。
  - `FRandomStream.RandRange` 在 `Min>Max` 时引擎层行为未明文保证，封装层加硬门拦截。
- **Control Manifest Rules**（Foundation 层，2026-06-06）:
  - **Required**：退化安全（ADR-0004）——`Min==Max` early-return 返回 Min 不调流（Seed 不推进）；`Min>Max` ensure 不自动交换；`RandomRange` 约束 `Range = Max-Min+1 < 2^24`，封装层 `ensure((int64)Max-(int64)Min+1 < (1<<24))`。
  - **Forbidden**：**Never 在 `Min>Max` 时自动 swap** —— swap 静默掩盖参数反向 bug（ADR-0004）。
  - **Guardrail**：退化 early-return 路径不消耗 Seed（守已固化 fixture 序列，story-006 依赖）。

## Acceptance Criteria

> scoped 到本 story（RandomRange 退化/边界/范围不变式）。

- [ ] **AC-10** [Logic] 固定种子 `S`：路径 A=`[RollDice()]`；路径 B=`[RandomRange(5,5), RollDice()]`；断言 `seqA[0] == seqB[1]` 逐字段相等（证明 `Min==Max` 退化调用**未推进游标**）。
- [ ] **AC-11** [Logic] 调 `RandomRange(10,3)`（Min>Max）：断言返回 `10`（=Min，**非 3、非随机**）+ `UE_LOG(LogTemp, Error, "...Min>Max...")` 触发（UE Automation `AddExpectedError(Contains "Min>Max", 1)` 捕获）（防实现者"好心"加 swap 掩盖 bug）。〔🟡 logged decision 2026-06-07：原文「ensure 触发 / shipping UE_LOG Warning」doc-sync 为 `UE_LOG(Error)`——循 FF-003→DR-001 AC-34→BD-003→BD-004 AC-27→BD-005 既定惯例：headless Automation 中 ensure callstack 不稳定致 AddExpectedError 计数不可靠，验证错误路径一律 UE_LOG(Error)。可测语义（返回 Min 不 swap + 记一行 Error 被 AddExpectedError 吞）完全满足本 AC。〕
- [ ] **AC-12b** [Logic·范围不变式] 固定种子，对多组 `(Min,Max)`（`(0,5)`/`(1,6)`/`(-3,3)`/`(10,10)`）各调 `RandomRange(Min,Max)` N≥20 次：断言**每次** `Min <= result <= Max` 无一例外（确定性范围不变式，守"结果恒落闭区间内"）。

## Implementation Notes

> 来自 ADR-0004 Implementation Guidelines + Key Interfaces + dice F-2，逐字保真不改写语义。

- **退化与边界**（ADR-0004 Guideline 3 / Key Interfaces，封装层硬门）:
  ```cpp
  UFUNCTION(BlueprintCallable) int32 RandomRange(int32 Min, int32 Max);
  //   Min==Max → early-return Min, 不调流(Seed 不推进, dice F-2/F-4④)
  //   Min>Max  → UE_LOG(LogTemp,Error,"...Min>Max..."), 返回 Min, 绝不自动交换
  //   约束: Range = Max-Min+1 < 2^24 (RandRange 走 FRand() 浮点中介, 跨度超 2^24 精度崩)
  //         封装层 (int64)Max-(int64)Min+1 >= (1<<24) 时 UE_LOG(Error)+直通
  ```
  > 🟡 logged decision 2026-06-07：上方原 `ensure(...)`/`UE_LOG(Warning)` doc-sync 为 `UE_LOG(LogTemp,Error)`（既定惯例 FF-003→DR-001→BD-003→BD-004→BD-005：headless ensure callstack 不稳）。2^24 门 Error+直通忠实 ensure-fallthrough（MVP 跨度极小永不触发；大跨度纯整数 LCG 路径 OQ-3 留 Full Vision）。
  - `Min==Max` → early-return 不推进 Seed（否则破坏已固化 fixture 序列）。
  - `Min>Max` → `UE_LOG(Error)` + 返回 Min，**绝不 swap**（swap 静默掩盖参数反向 bug）。
- **2^24 上界**（dice F-2，R2 修正）：真实约束「比不溢出更紧」—— `Range ≥ 2^24` 时 `FRand()*Range` 浮点乘积精度崩；MVP 实际跨度极小（AI 三选一、骰面 6），此为接口完整性边界。超 2^24 须改用纯整数 LCG 路径（同 OQ-3）。
- **进入确定性 fixture 的 Min/Max 须跨运行稳定值**（dice F-4④）：编译期常量 / 测试期已固化值；否则运行时 `Min==Max` 与否会改变是否推进 `Seed`，使固化序列错位。

## Out of Scope

- 宿主、`SetSeed`、`RandomFloat01`、通用原语基本路径 → story-001。
- RollDice 主 API → story-002（本 story 仅借 `[RollDice()]` 作"游标推进探针"验退化）。
- 长序列确定性 fixture（AC-8/9）→ story-006。
- `floor(f*N)` 不越界（AC-12c）→ story-001（属 RandomFloat01）。
- Alpha 大 N（≥2^24）路径开放时补 AC → OQ-4，不在 MVP。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

> 每 AC 一条 Given/When/Then。`-nullrhi` headless、固定种子、确定性。

- **AC-10（Min==Max 不推进游标）**
  - Given: 固定种子 `S`，构造两条独立同种子流。
  - When: 路径 A 流执行 `[RollDice()]`；路径 B 流执行 `[RandomRange(5,5), RollDice()]`。
  - Then: `seqA[0]`（A 的 RollDice 结果）== `seqB[1]`（B 的 RollDice 结果）逐字段相等。
  - Edge: 若 `RandomRange(5,5)` 误调流推进 Seed → B 的 RollDice 落在 A 的下一位 → 不等 → FAIL。
- **AC-11（Min>Max 不 swap）**
  - Given: 固定种子，注册 `AddExpectedError(Contains "Min>Max", 1)` 捕获 `UE_LOG(Error)`（doc-sync：ensure→UE_LOG(Error) 既定惯例，见 AC-11 logged decision）。
  - When: `RandomRange(10,3)`。
  - Then: 返回 `10`（=Min，非 3、非随机）；`UE_LOG(LogTemp, Error, "...Min>Max...")` 触发（被 `AddExpectedError` 捕获）。
  - Edge: 实现者若"好心"加 swap → 返回值落 [3,10] 随机 ≠ 10 → FAIL（这正是本条防的）。
- **AC-12b（范围不变式）**
  - Given: 固定种子，多组 `(Min,Max)` = `(0,5)`/`(1,6)`/`(-3,3)`/`(10,10)`。
  - When: 每组 `RandomRange` 调 ≥20 次。
  - Then: 每次 `Min <= result <= Max`，无一例外。
  - Edge: `(10,10)` 组每次恒返回 10（与 AC-10 退化路径一致）；`(-3,3)` 验负区间封装/截断正确。

## Test Evidence

- **Path**: `tests/unit/dice/randomrange_degenerate_boundary_test.cpp`（[Logic] AC-10/11/12b）。
- **Status**: 未创建。

## Dependencies

- **Depends on**: story-001（`RandomRange` 封装 + 宿主）、story-002（`RollDice` 作 AC-10 游标探针）须 DONE。
- **Unlocks**: story-006（fixture 序列含 `RandomRange` 退化调用，依赖本退化语义已正确）。

## Completion Notes
**Completed**: 2026-06-07
**Criteria**: 3/3 passing（AC-10/11/12b 全 [Logic] COVERED；无 deferred）
**Test Evidence**: Logic — `Source/rentoTests/Private/RandomRangeDegenerateBoundaryTest.cpp`（3 测试函数，类目 `Rento.Dice.RandomRangeDegenerateBoundary`）。主会话独立重编译（UBT Succeeded）+ 独立重跑全量 Rento. **140/140 Success（133+7 warn）, 0 Fail, EXIT 0**（证据 `Saved/TestReport_dr003_final/index.json`）。
**Code Review**: Complete — 首轮 CHANGES REQUIRED → APPROVED。生产三道门逻辑 CLEAN（主会话亲读：Min==Max 早退不调流 / Min>Max Error 返 Min 不 swap / int64 Range≥2²⁴ Error+直通，顺序短路正确）。**unreal 报 3 BLOCKING（缺 ensure / 应 Warning 非 Error）经主会话独立裁定全 deflate**——系既定 ensure→UE_LOG(Error) logged decision（FF-003→DR-001→BD-003→BD-004→BD-005），reviewer 不知此惯例，实现 CORRECT。**唯 1 真 must-fix（qa G-002）已闭合**：AC-12b (-3,3) 组下界断言 `result>=-3` 无法捕获「负 Min 截断为正」bug（0>=-3 仍 true）+ 注释 factually 错误 → 加 `bSawNegative` 截断捕获断言（变异论证：截断→全正→bSawNegative 恒 false→FAIL）+ 订正注释。
**Deviations**: None（git diff 证仅 RandomRange 体+注释 + 新测试，story-001/002 函数体未动）。
**Logged decisions（非债，已 doc-sync 本 story AC-11/Impl Notes/QA Test Cases）**:
- Min>Max / 2²⁴ 门用 `UE_LOG(LogTemp, Error)` 替 `ensure()`（既定惯例 headless 稳定）；2²⁴ 门 Error+直通忠实 ensure-fallthrough（MVP 跨度极小永不触发；大跨度纯整数 LCG OQ-3 留 Full Vision）。
**Tech debt logged**: 1 项入 register（**control-manifest §确定性 RNG + ADR-0004 Key Interfaces 仍携 stale「ensure(...)+UE_LOG(Warning)」文本**——logged decision「ensure→UE_LOG(Error)」已应用 5+ story 却从未传播至 manifest/ADR，各 story 仅 doc-sync 自身 AC → producer propagate 统一更新 manifest/ADR 文本）。
**Advisory（未折叠）**: AC-11 返回值防线种子 42 未文档化验 ≠10（error-log 防线才是真兜底，swap→无 Error→AddExpectedError 必 FAIL，返回值仅 bonus）；(1<<24) 可提具名常量；几处注释措辞精度。
