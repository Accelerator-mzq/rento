---
Epic: dice-rng
Status: Ready
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 002 — RollDice 核心契约 + 执行序铁律

## Context

- **GDD**: `design/gdd/dice.md` — CR-1（标准双骰掷骰核心 API）/ CR-4（掷骰与含义解耦）/ F-1（RollDice 标准双骰）/ Detailed Design「广播/返回顺序」/ Edge Cases「payload 不变式」。
- **Requirement TR-ID**: TR-dice-001（RollDice 部分）, TR-dice-006（广播/返回同源顺序）。
- **ADR Governing**:
  - **ADR-0004（确定性 RNG，primary）** — 钉死 `RollDice` 执行序铁律（流抽→固化→广播→返回同一固化值），保证同步返回值 == 事件 payload；`Die1`/`Die2` 两次独立顺序抽取。
  - **ADR-0003（事件总线）** — `OnDiceRolled(FDiceRollResult)` payload 形态（本 story 只验「返回值==payload」同源性；事件订阅机制本体见 story-005）。
  - **ADR-0007（BP-C++ 边界）** — `RollDice` 含 gameplay 随机→C++ + `UFUNCTION(BlueprintCallable)`。
- **Engine**: UE 5.7。Risk: **MEDIUM**（`FRandomStream::RandRange(1,6)` 闭区间均匀 pre-5.3 稳定；MEDIUM 来自 `RandHelper` 是否仍走 `FRand()` 浮点中介、5.7 是否改 `RandRange` 内部实现 —— 须实现期查源码）。
- **Engine Notes**（ADR-0004 / dice UE5.7 标注，逐字保真）:
  - `FRandomStream.RandRange` 闭区间 / `FRand()` ∈[0,1)（pre-5.3 稳定）。
  - `FRandomStream::RandRange(int,int)` **内部实现走浮点中介**（`RandHelper: Min + TruncToInt(FRand()*Range)`），**非纯整数取模** —— 故整数 `RandRange` 与 `FRand()` 同源、跨平台位级风险同源。
  - Verification Required ①：实现前查 5.7 源码确认 `RandHelper`/`RandRange` 是否仍走 `FRand()` 浮点中介。
- **Control Manifest Rules**（Foundation 层，2026-06-06）:
  - **Required**：`RollDice` 执行序铁律（dice F-4 前提②）：① 流抽 Die1,Die2 → ② 本地固化 result → ③ 广播 `OnDiceRolled(result)` → ④ 返回同一固化 result（非广播后重读流）；保证 同步返回值 == 事件 payload（ADR-0004）。含 gameplay 随机→C++ 且经 #3 `UFUNCTION`（ADR-0007）；owner 先同步算定权威结果再广播事件（结算同步先行，呈现事件后随）（ADR-0003）。
  - **Forbidden**：Never 旁路引擎全局 RNG（`FMath::Rand` 等）做需重放的 gameplay 随机（ADR-0004）；Never 用 BP 内置 Random 节点触碰 gameplay（ADR-0007）。
  - **Guardrail**：`FRandomStream` LCG O(1) 抽取，非性能热点。

## Acceptance Criteria

> scoped 到本 story（RollDice 字段完整性 + 不变式 + 两次独立抽取 + 返回==payload 同源）。

- [ ] **AC-1** [Logic] 固定种子调 `RollDice()` 一次：断言 `Die1∈[1,6]` ∧ `Die2∈[1,6]` ∧ `Total==Die1+Die2` ∧ `bIsDouble==(Die1==Die2)`（单次字段完整性 + payload 不变式）。
- [ ] **AC-2** [Logic] 固定种子连续 `RollDice()` N≥20 次：每次 `Total==Die1+Die2` ∧ `bIsDouble==(Die1==Die2)` 无一例外（不变式覆盖整序列，防单次碰巧）。
- [ ] **AC-6** [Logic] 固定种子 `S` 新建流直接展开两次 `RandRange(1,6)` 得 `expected_d1`/`expected_d2`；相同 `S` 重建同一新流（同 `S`、同 0 起点、抽 Die1 前无任何额外抽取消耗 `Seed`）调 `RollDice()`：断言 `Die1==expected_d1` ∧ `Die2==expected_d2`（证明实现是两次独立顺序抽取，非"先取 Total 再拆解"）。
- [ ] **AC-7** [Logic] 固定种子掷骰序列中每次 `Die1`/`Die2` 均 ∈[1,6]（CR-5 范围边界确定性覆盖）。
- [ ] **AC-16c** [Logic] 订阅 `OnDiceRolled`、回调内捕获 payload；`RollDice()` 返回后断言**返回值四字段 == 回调捕获的 payload 四字段**（证明返回的是广播前固化值、广播与返回同源，非广播后重读流）。

## Implementation Notes

> 来自 ADR-0004 Implementation Guidelines + Key Interfaces，逐字保真不改写语义。

- **执行序铁律**（ADR-0004 Key Interfaces / Guideline，dice F-4 前提②）:
  ```cpp
  UFUNCTION(BlueprintCallable) FDiceRollResult RollDice();
  //   执行序铁律: ① 流抽 Die1,Die2 → ② 本地固化 result
  //   → ③ 广播 OnDiceRolled(result) → ④ 返回同一固化 result(非广播后重读流)
  //   保证: 同步返回值 == 事件 payload; Total==Die1+Die2; bIsDouble==(Die1==Die2)
  ```
- **两次独立顺序抽取**（dice F-1 / CR-1）：`Die1 = RandRange(1,6)`；`Die2 = RandRange(1,6)`（两次独立抽取、游标各推进一次）；`Total = Die1+Die2`；`bIsDouble = (Die1==Die2)`。**是"两颗独立 d6"而非"先掷 Total 再拆解"**——三角分布（2/12 各 1/36、7 为 6/36）由两独立骰自然产生（经典大富翁忠实①）。
- **payload 不变式**（dice Edge Cases）：`Total==Die1+Die2` 且 `bIsDouble==(Die1==Die2)` 是构造时不变式；本系统产出时恒满足。
- **掷骰与含义解耦**（CR-4）：本 story 只产出 `FDiceRollResult`，**不解释** —— "走几步/是否额外回合/定序谁先手"全归调用方（移动4/回合2/事件7），本系统不含定序单骰模式。
- **禁旁路**（Manifest Forbidden）：抽取只经权威 `FRandomStream`，禁 `FMath::Rand`/BP Random 节点。

## Out of Scope

- 宿主、`SetSeed`、通用原语封装 → story-001。
- `OnDiceRolled` 的 USTRUCT/BlueprintAssignable 声明、触发次数、重入禁止 → story-005（本 story 仅用其验证「返回==payload」同源，不重复实现事件机制）。
- 确定性长序列 fixture（AC-8/9）→ story-006。
- 统计均匀性 chi-square（AC-3/4/5）→ Advisory，归 story-006。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

> 每 AC 一条 Given/When/Then。`-nullrhi` headless、固定种子、确定性。

- **AC-1（单次字段完整性 + 不变式）**
  - Given: `SetSeed(12345)`。
  - When: `RollDice()` 一次。
  - Then: `Die1∈[1,6]` ∧ `Die2∈[1,6]` ∧ `Total==Die1+Die2` ∧ `bIsDouble==(Die1==Die2)`。
  - Edge: 任一字段越界或不变式破裂 = FAIL（封装 bug）。
- **AC-2（不变式覆盖整序列）**
  - Given: `SetSeed(12345)`。
  - When: 连续 `RollDice()` 20 次。
  - Then: 每次 `Total==Die1+Die2` ∧ `bIsDouble==(Die1==Die2)`，无一例外。
  - Edge: 序列中至少出现一次 `bIsDouble==true`（覆盖双点分支）与多次 false。
- **AC-6（两次独立抽取，非先取 Total 拆解）**
  - Given: 固定 `S`，**新建流 A** 直接 `RandRange(1,6)` ×2 → `expected_d1`/`expected_d2`；**重建同 `S` 流 B**（同起点、抽 Die1 前零额外抽取）。
  - When: 流 B 宿主调 `RollDice()`。
  - Then: `Die1==expected_d1` ∧ `Die2==expected_d2`。
  - Edge（D-2 前提）: 两路径须从同一 `S` 同一起点出发、`RollDice` 抽 Die1 前零额外抽取 —— fixture 构建时显式保证（新建流、不复用已用过的流），否则游标错位使比对失效。
- **AC-7（Die1/Die2 范围确定性覆盖）**
  - Given: AC-2 同序列。
  - When: 遍历每次结果。
  - Then: `Die1∈[1,6]` ∧ `Die2∈[1,6]` 恒成立（"均匀"属统计层 AC-3~5，本条只验范围）。
- **AC-16c（广播=返回同源）**
  - Given: 订阅 `OnDiceRolled` 的回调捕获 payload 到局部变量；`SetSeed(12345)`。
  - When: `RollDice()` 返回。
  - Then: 返回值四字段 == 回调捕获 payload 四字段（证明返回的是广播前固化值，非广播后重读流）。
  - Edge: 若实现"广播后重读流返回" → 返回值会比 payload 多消耗 Seed → 四字段不等 → FAIL。

## Test Evidence

- **Path**: `tests/unit/dice/rolldice_core_contract_test.cpp`（[Logic] AC-1/2/6/7/16c）。
- **Status**: 未创建。

## Dependencies

- **Depends on**: story-001（RNG 服务宿主 + `RandRange` 原语 + `FRandomStream` 权威流）须 DONE。
- **Unlocks**: story-005（事件触发次数/重入依赖 RollDice 主路径）/ story-006（fixture 含 RollDice 抽取）/ story-008（序列化 RollDice 产出的完整结果）。
