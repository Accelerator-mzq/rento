---
Epic: dice-rng
Status: Ready
Layer: Foundation
Type: Logic
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
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
- [ ] **AC-11** [Logic] 调 `RandomRange(10,3)`（Min>Max）：断言返回 `10`（=Min，**非 3、非随机**）+ ensure 触发（dev，UE Automation `AddExpectedError` 捕获）/ shipping UE_LOG Warning（防实现者"好心"加 swap 掩盖 bug）。
- [ ] **AC-12b** [Logic·范围不变式] 固定种子，对多组 `(Min,Max)`（`(0,5)`/`(1,6)`/`(-3,3)`/`(10,10)`）各调 `RandomRange(Min,Max)` N≥20 次：断言**每次** `Min <= result <= Max` 无一例外（确定性范围不变式，守"结果恒落闭区间内"）。

## Implementation Notes

> 来自 ADR-0004 Implementation Guidelines + Key Interfaces + dice F-2，逐字保真不改写语义。

- **退化与边界**（ADR-0004 Guideline 3 / Key Interfaces，封装层硬门）:
  ```cpp
  UFUNCTION(BlueprintCallable) int32 RandomRange(int32 Min, int32 Max);
  //   Min==Max → early-return Min, 不调流(Seed 不推进, dice F-2/F-4④)
  //   Min>Max  → ensure(Min<=Max)+UE_LOG(Warning), 返回 Min, 绝不自动交换
  //   约束: Range = Max-Min+1 < 2^24 (RandRange 走 FRand() 浮点中介, 跨度超 2^24 精度崩)
  //         封装层 ensure((int64)Max-(int64)Min+1 < (1<<24))
  ```
  - `Min==Max` → early-return 不推进 Seed（否则破坏已固化 fixture 序列）。
  - `Min>Max` → ensure + 返回 Min，**绝不 swap**（swap 静默掩盖参数反向 bug）。
- **2^24 上界**（dice F-2，R2 修正）：真实约束「比不溢出更紧」—— `Range ≥ 2^24` 时 `FRand()*Range` 浮点乘积精度崩；MVP 实际跨度极小（AI 三选一、骰面 6），此为接口完整性边界。超 2^24 须改用纯整数 LCG 路径（同 OQ-3）。
- **进入确定性 fixture 的 Min/Max 须跨运行稳定值**（dice F-4④）：编译期常量 / 测试期已固化值；否则运行时 `Min==Max` 与否会改变是否推进 `Seed`，使固化序列错位。

## Out of Scope

- 宿主、`SetSeed`、`RandomFloat01`、通用原语基本路径 → story-001。
- RollDice 主 API → story-002（本 story 仅借 `[RollDice()]` 作"游标推进探针"验退化）。
- 长序列确定性 fixture（AC-8/9）→ story-006。
- `floor(f*N)` 不越界（AC-12c）→ story-001（属 RandomFloat01）。
- Alpha 大 N（≥2^24）路径开放时补 AC → OQ-4，不在 MVP。

## QA Test Cases

> 每 AC 一条 Given/When/Then。`-nullrhi` headless、固定种子、确定性。

- **AC-10（Min==Max 不推进游标）**
  - Given: 固定种子 `S`，构造两条独立同种子流。
  - When: 路径 A 流执行 `[RollDice()]`；路径 B 流执行 `[RandomRange(5,5), RollDice()]`。
  - Then: `seqA[0]`（A 的 RollDice 结果）== `seqB[1]`（B 的 RollDice 结果）逐字段相等。
  - Edge: 若 `RandomRange(5,5)` 误调流推进 Seed → B 的 RollDice 落在 A 的下一位 → 不等 → FAIL。
- **AC-11（Min>Max 不 swap）**
  - Given: 固定种子，dev 配置注册 `AddExpectedError` 捕获 ensure。
  - When: `RandomRange(10,3)`。
  - Then: 返回 `10`（=Min，非 3、非随机）；dev ensure 触发（被 `AddExpectedError` 捕获）；shipping UE_LOG Warning。
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
