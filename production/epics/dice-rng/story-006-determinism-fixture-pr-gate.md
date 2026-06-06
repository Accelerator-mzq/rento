---
Epic: dice-rng
Status: Ready
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 006 — F-5 确定性种子序列 fixture（PR-gate headless）

## Context

- **GDD**: `design/gdd/dice.md` — F-4（确定性契约 性质 D）/ F-5（统计可测性 层一 [Logic] 确定性种子序列测试）/ AC-8（核心 PR gate）/ AC-9（跨种子固化）/ F-5 层二 [Advisory] chi-square。
- **Requirement TR-ID**: TR-dice-017（F-5 层一 fixture，PR-gate headless）, TR-dice-002（性质 D 确定性）, TR-dice-004（跨平台/跨编译配置一致风险登记）。
- **ADR Governing**:
  - **ADR-0004（确定性 RNG，primary）** — 性质 D 落地：单权威流 + 固定种子 + 确定有序抽取序列→精确序列断言；权威流序列纯净（不含表现抽取）才能让 [Logic] 回归门成立。
  - **ADR-0007（BP-C++ 边界）** — `[Logic] BLOCKING` AC 的被测逻辑→C++ 独立 `UObject`/纯函数，`-nullrhi` 可实例化、可 spy。
- **Engine**: UE 5.7。Risk: **MEDIUM**（fixture 的 expected 常量绑定 `FRandomStream::RandRange`/`FRand()` 当前实现；若 5.7 改 `RandHelper` 内部实现则 fixture 须重固化 —— 失败"当且仅当 `FRandomStream` API 破坏性变更"是设计意图）。
- **Engine Notes**（ADR-0004 / dice F-4/F-5，逐字保真）:
  - Verification Required ①：实现前查 5.7 源码确认 `RandHelper`/`RandRange` 是否仍走 `FRand()` 浮点中介（影响跨平台位级一致性结论）。
  - **⚠ 跨平台位级一致性不被性质 D 保证**（CD 裁定）：`Seed` 的 LCG 整数滚动跨平台一致（流推进确定），但每次抽取的输出映射在浮点截断边界上跨编译器/优化/平台可能产生极罕见 off-by-one —— MVP 单机不触发，降 Full Vision OQ-3。**实现阶段须查 UE5.7 源码确认 `RandHelper` 是否仍走 `FRand()` 浮点。**
  - off-by-one 风险不限跨平台 —— **同平台 Debug vs Shipping**（`/fp:precise` vs `/fp:fast`）亦可分歧；Full Vision 重放验证须在 Shipping 配置下做（OQ-3）。
- **Control Manifest Rules**（Foundation 层，2026-06-06）:
  - **Required**：`[Logic] BLOCKING` AC 的被测逻辑→C++ 独立 `UObject`/纯函数，须 `-nullrhi` 可实例化、暴露可 spy getter（ADR-0007）；全部 gameplay 随机走唯一权威 `FRandomStream`（ADR-0004）。
  - **Forbidden**：Never 让 `[Logic] BLOCKING` AC 的被测逻辑依赖 `UMG NativeTick` 或 BP `Bind Event` graph 层（`-nullrhi` 下 false-green）（ADR-0007）；Never 取消独立流让 juice 抖动调权威流（污染序列）（ADR-0004）。
  - **Guardrail**：统计/频率断言永不作 [Logic] gate（即便 n=36000+α=0.001 日构建仍偶发误触发）→ 一律 [Advisory]；确定性种子序列断言才是 [Logic]（dice 核心原则，player-turn 8 轮代价）。CI 引擎 CMD：headless `-nullrhi`。

## Acceptance Criteria

> scoped 到本 story（确定性序列 fixture，PR-gate 骨干）。

- [ ] **AC-8** [Logic·核心 PR gate] 固定种子（如 12345）+ 固定有序调用序列 `[RollDice×5, RandomRange(0,5)×3, RandomFloat01×2, …]` 总抽取≥20；fixture 先运行打印真实序列、硬编码为 `expected[]`；两次冷启动运行均逐字段精确等于 `expected[]`。失败当且仅当 `FRandomStream` API 破坏性变更（可检测、有意义）。
- [ ] **AC-9** [Logic] seed=12345 与 seed=99999 各跑 `[RollDice×5]`，两组序列各自固化为 `expected_12345[]`/`expected_99999[]` 常量；断言运行结果逐字段精确等于各自固化常量（覆盖"忽略种子退化"+ 确定性）。
- [ ] **AC-3** [Advisory] n=36000 次：`bIsDouble` 频率 ∈ [0.155,0.178]、**chi-square(df=1)<10.83**（二分类，α=0.001 临界≈10.828）。
- [ ] **AC-4** [Advisory] n=36000 次：Total PMF chi-square(df=10)<29.59（α=0.001），expected[7]=6000/expected[2]=expected[12]=1000。
- [ ] **AC-5** [Advisory] n=36000 次：单面均匀 chi-square(df=5)<20.52；两骰独立性（联合频率 vs 边缘乘积）chi-square(df=25)<52.62。
- [ ] **AC-12** [Advisory] `RandomRange(1,6)` 闭区间边界 1 与 6 可达性（若固化 fixture 确认边界出现位置可升 [Logic]）。

## Implementation Notes

> 来自 ADR-0004 + dice F-4/F-5，逐字保真不改写语义。

- **fixture 构建法**（dice F-5 层一）：固定种子下 `FRandomStream` 输出序列完全确定；fixture 固化一段已知序列（≥20 次抽取），逐字段精确相等断言。**不用频率断言**——单局掷骰次数有限、频率置信区间宽、CI 会 flaky。失败当且仅当 `FRandomStream` API 破坏性变更（可检测、有意义）。fixture 构建：固定种子运行一次→打印序列→硬编码为 expected 常量。
- **AC-9 离线判断"两常量不同"**（dice AC-9，R1 修 D-1）：不再用"两组不完全相同"作运行期断言（那是概率命题）；"两组确不相同"的判断移到测试编写期离线完成（固化时人工确认两常量不同），运行期只做确定性的"等于固化常量"断言。本条同时覆盖"忽略种子退化"与确定性。
- **核心原则**（dice F-5 / AC 节）：**PR gate 只放确定性断言；频率/统计断言作定期 Advisory，不阻塞合并。** 这是本项目反复失效模式（player-turn 8 轮代价）。
- **退化值入 fixture 须稳定**（dice F-4④）：进入确定性 fixture 的 `RandomRange` Min/Max 须为跨运行稳定值（编译期常量/测试期已固化值）；否则运行时 `Min==Max` 与否会改变是否推进 `Seed`，使固化序列错位。
- **层二 chi-square [Advisory]**（dice F-5）：n=36000，自由度勘误 —— Total 11 类别拟合优度 df=10、临界 29.59；`bIsDouble` 二分类 df=1、临界≈10.83。失败→记录日志、不阻塞 PR。

## Out of Scope

- RollDice/RandomRange/RandomFloat01 抽取本体实现 → story-001/002/003（本 story 组合它们成确定序列断言）。
- 退化契约正确性（AC-10/11/12b）→ story-003（本 fixture 假定其已正确）。
- CI 静态扫描禁全局 RNG 硬门 + 流隔离 spy → story-007。
- Full Vision 跨平台重放正解（服务器权威 / 纯整数 LCG）→ OQ-3，不在 MVP（本 story 只在单机/同配置断言确定性，跨平台一致登记 TR-dice-004 不实现）。

## QA Test Cases

> 每 AC 一条 Given/When/Then。`-nullrhi` headless、确定性。

- **AC-8（核心 PR gate 骨干 fixture）**
  - Given: `SetSeed(12345)`，固定有序调用序列 `[RollDice×5, RandomRange(0,5)×3, RandomFloat01×2, …]` 总抽取≥20；`expected[]` 由首次运行打印硬编码。
  - When: 两次冷启动跑该序列。
  - Then: 每次逐字段精确等于 `expected[]`。
  - Edge: 失败当且仅当 `FRandomStream` API 破坏性变更（可检测、有意义）—— 此时须查 5.7 源码（Verification ①）并重固化 expected。
- **AC-9（跨种子固化，忽略种子退化）**
  - Given: `expected_12345[]` / `expected_99999[]` 两组固化常量（编写期已离线确认互不相同）。
  - When: seed=12345 与 seed=99999 各跑 `[RollDice×5]`。
  - Then: 各组逐字段精确等于各自固化常量。
  - Edge: 若实现忽略种子（如 lazy-init 落固定 0）→ 至少一组对不上 → FAIL。**禁**用"两组运行结果不同"概率断言。
- **AC-3/4/5（统计均匀性，Advisory）**
  - Given: 固定种子，n=36000 掷。
  - When: 统计 `bIsDouble` 频率 / Total PMF / 单面 / 双骰独立性。
  - Then: 各 chi-square < 临界（df=1<10.83 / df=10<29.59 / df=5<20.52 / df=25<52.62）。
  - Pass: 失败→记录日志、**不阻塞 PR**（频率统计、CI 偶发误触发）。
- **AC-12（边界可达性，Advisory）**
  - Given: 固定种子，`RandomRange(1,6)` 多次。
  - When: 收集是否出现 1 与 6。
  - Then: 边界 1 与 6 可达。
  - Edge: 降 [Advisory] 因种子依赖性（某种子前 N 次未现边界）；若固化 fixture 确认边界出现位置可升 [Logic]。

## Test Evidence

- **Path**: `tests/unit/dice/determinism_fixture_test.cpp`（[Logic·PR gate] AC-8/9 —— 所有 [Logic] gate 骨干 fixture）。
- **Path**: `tests/unit/dice/distribution_chisquare_test.cpp`（[Advisory] AC-3/4/5/12，记录日志不阻塞）。
- **Status**: 未创建。

## Dependencies

- **Depends on**: story-001（宿主+原语）、story-002（RollDice）、story-003（退化契约，fixture 序列含退化调用须语义正确）须 DONE。**Sprint0 引擎验证（story-007）须先确认 `RandHelper` 仍走 `FRand()` 浮点中介，否则 expected 固化基线可能漂移。**
- **Unlocks**: 本 story 是 DoD 的核心门（F-5 确定性序列 headless 通过）；为下游移动(4)/AI(10) 的可重放回归门奠基。
