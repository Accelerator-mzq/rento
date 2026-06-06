---
Epic: dice-rng
Status: Ready
Layer: Foundation
Type: Logic
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 004 — lazy-init 兜底种子安全（禁默构 0）

## Context

- **GDD**: `design/gdd/dice.md` — Edge Cases（流未初始化即调；lazy-init 种子来源必须非确定值，禁默构 0；种子=0 或负数合法）/ UE5.7 标注（`FRandomStream()` 默构种子恒 0）。
- **Requirement TR-ID**: TR-dice-014。
- **ADR Governing**:
  - **ADR-0004（确定性 RNG，primary）** — lazy-init 兜底种子用 `FPlatformTime::Cycles()`，禁 `FRandomStream()` 默构 0；Full Vision 重放模式下未 SetSeed = fail-fast。
  - **ADR-0001（宿主）** — RNG 默认种子恒 0、禁 lazy-init（正常路径 Seed 注入唯一时机 = `OnWorldBeginPlay`；lazy-init 是未注入兜底）。
- **Engine**: UE 5.7。Risk: **MEDIUM**（`FRandomStream()` 默认构造种子是否仍恒 0 是 lazy-init 兜底安全的前提，须实现期查源码核实）。
- **Engine Notes**（ADR-0004 / dice UE5.7 标注，逐字保真）:
  - `FRandomStream()` **默认构造种子恒 `0`**（确定常量）——lazy-init **禁**落此默认值。
  - Verification Required ③：实现前查 5.7 源码确认默认构造种子是否仍恒 `0`（影响 lazy-init 兜底安全）。
- **Control Manifest Rules**（Foundation 层，2026-06-06）:
  - **Required**：默认构造种子恒 0 禁落 lazy-init（ADR-0004）；非权威流初始化禁默构（种子恒0），须 `Initialize(FPlatformTime::Cycles())`（ADR-0004）；Seed 注入唯一时机 = `OnWorldBeginPlay`，RNG 默认种子恒 0、禁 lazy-init（ADR-0001）。
  - **Forbidden**：**Never 用 `FRandomStream()` 默认构造（种子恒 0）初始化任何流** —— 忘 SetSeed 的对局掷出相同确定序列（ADR-0004）。
  - **Guardrail**：lazy-init 落固定 0 会砸穿"骰子不可预测"信任底线，且范围断言查不出（返回值范围照样合法）—— 故须 `InitialSeed != 0` 确定性断言。

## Acceptance Criteria

> scoped 到本 story（未 SetSeed 兜底路径安全）。

- [ ] **AC-17** [Logic] 不调 `SetSeed` 直接 `RollDice()`：shipping 不崩、返回值满足 AC-1 约束（`Die1∈[1,6]` 等，lazy-init 兜底）；dev ensure 触发。
- [ ] **AC-17b** [Logic] 不调 `SetSeed` 直接 `RollDice()` 触发 lazy-init 后：经公有 accessor（`GetInitialSeed()`/`GetCurrentSeed()`）断言 **lazy-init 实际写入的 `InitialSeed != 0`**（证明兜底种子取自非确定源 `FPlatformTime::Cycles()`、未退化为 `FRandomStream` 默认构造的固定 `0`）。

## Implementation Notes

> 来自 ADR-0004 Implementation Guidelines + dice Edge Cases，逐字保真不改写语义。

- **lazy-init 兜底种子**（ADR-0004 Guideline 3）：lazy-init 兜底种子用 `FPlatformTime::Cycles()`，**禁** `FRandomStream()` 默构 0（否则忘 SetSeed 的对局掷出相同确定序列，砸穿「骰子不可预测」信任底线，且范围断言查不出）。Full Vision 重放模式下未 SetSeed = fail-fast，不走 lazy-init。
- **未初始化兜底**（dice Edge Cases）：开局必先 `SetSeed`（CR-3）。未初始化调用 → `ensure` dev 诊断 + lazy-init 兜底（shipping 不崩、不返回未定义值）。
- **种子合法值**（dice Edge Cases）：`FRandomStream` 接受任意 int32 种子，**`0` 是合法种子**（非"无种子"哨兵）—— 注意：`0` 作**显式 SetSeed 传入**仍合法（见 story-001 AC-18），本 story 只拦截 lazy-init 退化到默认构造 `0` 的路径。
- **AC-17b 为确定性断言**（R2 修正）：原"两次冷启动序列不完全相同"是概率命题（违反"统计/频率断言永不作 [Logic] gate"原则）；改为确定性单点断言 `InitialSeed != 0`（`Cycles()` 实际恒不返回 0 → 断言结果确定为真、零 flaky），直接覆盖 B4 意图。

## Out of Scope

- 正常 SetSeed 路径、宿主、通用原语 → story-001。
- `seed=0` 经**显式** SetSeed 合法（AC-18）→ story-001（本 story 只拦 lazy-init 退化到 0）。
- RollDice 字段不变式本体（AC-1）→ story-002（本 story 复用其约束作兜底返回值检查）。
- Full Vision 重放 fail-fast 的联网路径实现 → OQ-3，不在 MVP。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

> 每 AC 一条 Given/When/Then。`-nullrhi` headless。

- **AC-17（未 SetSeed 兜底不崩）**
  - Given: `NewObject` DiceService，**不调** `SetSeed`；dev 配置注册 `AddExpectedError` 捕获 ensure。
  - When: 直接 `RollDice()`。
  - Then: shipping 不崩、返回值满足 AC-1 约束（`Die1∈[1,6]` ∧ `Die2∈[1,6]` ∧ `Total==Die1+Die2` ∧ `bIsDouble==(Die1==Die2)`）；dev ensure 触发（被捕获）。
  - Edge: 不返回未定义/越界值。
- **AC-17b（兜底种子非固定 0）**
  - Given: `NewObject` DiceService，不调 `SetSeed`。
  - When: 触发 lazy-init（首次 `RollDice()`），随后读 `GetInitialSeed()`/`GetCurrentSeed()`。
  - Then: `InitialSeed != 0`（证明兜底取自 `FPlatformTime::Cycles()` 非确定源，未退化为默构 0）。
  - Edge: 此为确定性单点断言（`Cycles()` 恒非 0），**禁**用"两次冷启动序列不同"概率断言（同 AC-9 缺陷模式、违核心原则）。

## Test Evidence

- **Path**: `tests/unit/dice/lazy_init_seed_safety_test.cpp`（[Logic] AC-17/17b）。
- **Status**: 未创建。

## Dependencies

- **Depends on**: story-001（宿主 + `GetInitialSeed`/`GetCurrentSeed` accessor）、story-002（RollDice 主路径 + AC-1 约束）须 DONE。
- **Unlocks**: 无（叶子边界 story）；与 story-006 fixture 正交。
