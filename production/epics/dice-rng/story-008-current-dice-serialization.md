---
Epic: dice-rng
Status: Complete
Layer: Foundation
Type: Integration
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-08
---

# Story 008 — 当前骰序列化契约（完整 FDiceRollResult，MVP 不存 Seed）

## Context

- **GDD**: `design/gdd/dice.md` — States and Transitions（当前 Seed 序列化、读档确定性）/ Detailed Design B1（序列化必须携带 Die1/Die2）/ Edge Cases（读档后当前 Seed 重设非确定种子、不可重设回开局种子）/ Interactions 存档(21) 行 / OQ-T-Dice-5（RESOLVED）/ OQ-2。
- **Requirement TR-ID**: TR-dice-012（当前回合骰结果序列化为完整 `FDiceRollResult`，读档不重掷）, TR-dice-013（MVP 不序列化当前 Seed；Full Vision 经 GetCurrentSeed/SetSeed 显式存取）。
- **ADR Governing**:
  - **ADR-0005（存档序列化契约，primary）** — Never 重复存 MVP 骰子 Seed；存 DA 引用不存布局；读档重建拓扑序 …→骰子 SetSeed→重绑；round-trip identity。
  - **ADR-0004（确定性 RNG）** — Seed 序列化：MVP 不序列化（当前骰由 player-turn 序列化完整 `FDiceRollResult` 保护）；Full Vision 重放经 `GetCurrentSeed()` 显式存取（勿靠 struct 反射）。
- **Engine**: UE 5.7。Risk: **LOW**（`UPROPERTY(SaveGame)` 嵌套 USTRUCT 递归序列化 pre-5.3 稳定；本 story 不引入新 RNG API）。
- **Engine Notes**（ADR-0005 / ADR-0004 / dice UE5.7 标注，逐字保真）:
  - `FRandomStream` 无显式"游标计数器",内部状态仅 `InitialSeed` + 当前滚动 `Seed`；序列化当前进度 = 序列化当前 `Seed`。**实现须用公有 accessor `GetCurrentSeed()` 显式存取,勿依赖 `UPROPERTY(SaveGame)` struct 反射**（反射可能只持久化 `InitialSeed` 而丢当前 `Seed`、静默破坏 OQ-2 续算）。
  - Sprint0 验证 #8（ADR-0005，LOW）：✅ **已验证（2026-06-08 player-turn pt-008）** `UPROPERTY(SaveGame)` 嵌套 USTRUCT/`TArray` 递归序列化 round-trip 恒等（含 `FDiceRollResult` Die1/Die2/Total/bIsDouble）；`FObjectAndNameAsStringProxyArchive` 过滤行为=**内置 `SaveGameToMemory`/`SaveGameToSlot` 不按 SaveGame 过滤、全量序列化**（修正，见 ADR-0005 Implementation Finding）。本 story 序列化 `FDiceRollResult` 走 player-turn `UPlayerTurnSaveData` 容器（per-player 记录），已 round-trip 验证。
- **Control Manifest Rules**（Foundation 层，2026-06-06）:
  - **Required**：Seed 序列化——MVP 不序列化 Seed（当前骰由 player-turn 序列化完整 `FDiceRollResult` 保护）；Full Vision 重放经 `GetCurrentSeed()` 显式存取（ADR-0004/0005）。读档重建拓扑序：DA →（经济/地产/建房/事件格牌堆 互不依赖）→ 回合2 → 骰子 SetSeed → 重绑 → `switch(CurrentPhase)`；禁重建 A 时读尚未重建的 B（ADR-0005）。round-trip identity：存档前快照 == 读档后快照，逐字段 identity-check（可证伪，无"调参直到通过"逃逸）（ADR-0005）。序列化字段名须逐字对齐各 owner GDD（ADR-0005）。
  - **Forbidden**：**Never 重复存派生量/`bIsBankrupt`/`bIsInJail`/MVP 骰子 Seed**（ADR-0005）；Never 序列化全量棋盘布局（存 DA 引用不存布局）（ADR-0005）。
  - **Guardrail**：读档反序列化 + 重建 < 1s；存档非帧路径。

## Acceptance Criteria

> scoped 到本 story（骰子侧序列化契约：完整 FDiceRollResult、MVP 不存 Seed、读档 SetSeed 非确定值）。

- [ ] **[序列化字段]** 当前回合骰结果序列化为完整 `FDiceRollResult{Die1,Die2,Total,bIsDouble}`（含 Die1/Die2，**不可只存 Total/bIsDouble**）；MVP **不序列化当前 Seed**（DiceService 无 `UPROPERTY(SaveGame)` Seed 字段）。
- [ ] **[读档 SetSeed]** 读档时骰子 `SetSeed(非确定值)`（重设新非确定种子，`Seed` 不续算）；**不可重设回开局种子**（否则读档后整条后续掷骰序列与开局可复现地重复，可被观测的统计异常）。
- [ ] **[读档不重掷]** 读档后当前回合骰结果由 player-turn 序列化的完整 `FDiceRollResult`（含 Die1/Die2）保护、**不重掷当前骰**；下游 VFX 可从 Die1/Die2 忠实重现骰面。
- [ ] **[round-trip]** 存档前 `FDiceRollResult` 快照 == 读档后该字段快照，逐字段 identity-check（Die1/Die2/Total/bIsDouble 全等）。

## Implementation Notes

> 来自 ADR-0005 + ADR-0004 Implementation Guidelines + dice States/B1，逐字保真不改写语义。

- **Seed 序列化（与 ADR-0005 协同）**（ADR-0004 Guideline 7）：MVP 不序列化 Seed（当前骰由 player-turn 序列化完整 `FDiceRollResult` 保护，读档 `SetSeed(非确定值)` 不重掷）；Full Vision 重放经 `GetCurrentSeed()` 显式存当前 Seed。
- **B1 序列化必带 Die1/Die2**（dice States/B1）：序列化必须携带 `Die1`/`Die2`，**不可只存 `(Total,bIsDouble)`**：VFX 契约强制用 `Die1`/`Die2` 分别画两颗骰面、禁从 `Total` 拆解；若读档只存 `(Total,bIsDouble)`，Die1/Die2 丢失→VFX 无法忠实重现骰面（Total=4 无法判定是 1+3 还是 2+2）。
- **owner 边界**（dice Interactions）：当前骰结果的**序列化字段归 player-turn(2)** 持有并序列化（player-turn AC-33/34，OQ-T-Dice-5 已 RESOLVED：player-turn 7 处 RECEIVE+SERIALIZE 已扩为完整 `FDiceRollResult`）。本 story 的 DiceService 侧义务边界 = **不持权威骰子状态、不序列化 Seed、读档 `SetSeed(非确定值)`**；字段序列化本体的实现归 player-turn epic。
- **读档重设种子陷阱**（dice Edge Cases）：读档重设为新非确定种子（`Seed` 不续算）—— 注意**不可重设回开局种子**，否则读档后整条后续掷骰序列与开局可复现地重复（可被玩家/测试观测的统计异常）。
- **读档拓扑序**（ADR-0005 CR-5）：…→ 回合2 → 骰子 `SetSeed` → 重绑 → `switch(CurrentPhase)`；骰子 SetSeed 在回合2 重建之后。
- **MVP 不存 Seed 理由**（dice States）：player-turn 已序列化当前回合完整 `FDiceRollResult`、读档不重掷当前骰，故 MVP **即使读档后重设种子（`Seed` 不续）也不触发重复掷骰 bug**。

## Out of Scope

- player-turn AC-33/34 序列化字段本体的实装（RECEIVE+SERIALIZE 完整 `FDiceRollResult`）→ player-turn epic（OQ-T-Dice-5 已 RESOLVED，跨档闭合）。
- 存档系统完整性门（magic/version/checksum/manifest）、写盘原子性、`OnGameLoaded` 重绑 → save-load epic（ADR-0005 本体）。
- Full Vision 重放序列化当前 `Seed`（`GetCurrentSeed()` 续算）→ OQ-2，不在 MVP。
- DiceService 宿主/原语/RollDice/事件 → story-001~005。

## QA Test Cases

> Integration（存读 round-trip）给 Given/When/Then。`-nullrhi` headless。

- **[序列化字段]（不存 Seed、存完整结果）**
  - Given: DiceService 类定义 + player-turn 当前骰字段。
  - When: 编译期反射检查 + 字段审查。
  - Then: DiceService 无 `UPROPERTY(SaveGame)` Seed 字段；当前骰字段为完整 `FDiceRollResult`（四字段，含 Die1/Die2）。
  - Edge: 若骰字段只存 `(Total,bIsDouble)` → FAIL（B1：读档 VFX 无法重现骰面）。
- **[读档 SetSeed]（非确定值、不回开局种子）**
  - Given: 开局 `SetSeed(S0)`，掷若干骰、存档。
  - When: 读档触发拓扑序，骰子 `SetSeed(新非确定值)`。
  - Then: 读档后 `GetInitialSeed() != S0`（不回开局种子）；后续掷骰序列与开局不可复现地相同。
  - Edge: 若读档误 `SetSeed(S0)` → 读档后掷骰序列与开局逐字段重复 → 可观测统计异常 → FAIL。
- **[读档不重掷]**
  - Given: 当前回合已掷出 `FDiceRollResult{D1,D2,T,bD}`，存档（player-turn 序列化全字段）。
  - When: 读档。
  - Then: 当前回合骰结果 == 存档前（含 Die1/Die2），**未重掷**；VFX 可从 Die1/Die2 重现骰面。
  - Edge: 读档后若当前骰被重掷（新值）→ FAIL（player-turn 序列化应保护）。
- **[round-trip identity]**
  - Given: 存档前 `FDiceRollResult` 快照。
  - When: 存档 → 读档 → 取该字段快照。
  - Then: 逐字段 identity（Die1/Die2/Total/bIsDouble 全等），无"调参直到通过"逃逸。
  - Edge: 任一字段不等 = 序列化损坏。

## Test Evidence

- **Path**: `tests/integration/dice/dice_serialization_roundtrip_test.cpp`（[Integration] 存读 round-trip：完整 FDiceRollResult identity + 不存 Seed + 读档 SetSeed 非确定值）。
- **Status**: 未创建。

## Dependencies

- **Depends on**: story-001（`SetSeed`/`GetInitialSeed`/`GetCurrentSeed` accessor）、story-002（RollDice 产出 `FDiceRollResult`）、story-005（`FDiceRollResult` USTRUCT 结构定型）须 DONE。**跨 epic**：player-turn AC-33/34 完整 `FDiceRollResult` 序列化（OQ-T-Dice-5 RESOLVED）；save-load 存读框架（ADR-0005）。
- **Unlocks**: 无（叶子序列化契约）；为 Full Vision OQ-2（重放序列化 Seed）预留 `GetCurrentSeed()` 接口。

## Completion Notes
**Completed**: 2026-06-08
**Criteria**: 4/4 passing（全 COVERED，0 UNTESTED，0 DEFERRED）
**Deviations**: None（blocking）。信息性：物理测试路径 `Source/rentoTests/Private/DiceSerializationRoundtripTest.cpp` 与 story 逻辑路径 `tests/integration/dice/...` 差异 = repo UE Automation 惯例，非偏离。
**Test Evidence**: Integration — `Source/rentoTests/Private/DiceSerializationRoundtripTest.cpp`（4 TC，Automation 类目 `Rento.Dice.Serialization`）。主会话独立全证：Build Succeeded（clang G6 误报忽略，UBT 为准）；4/4 Success；全量 tests 数组 266/266 Success 0 Fail 0 notRun（基线262+4零回归，`Saved/TestReport_dr008_full`）。TC2 变异坐实非-vacuous（S1==S0 → 发散断言精确 FAIL，`Saved/TestReport_dr008_mut`，还原复绿）。
**Code Review**: Complete — `/code-review` APPROVED（无 Required Changes；ADR-0004/0005 合规）。
**实现要点**：纯验证型零 src 改动。序列化本体由 pt-008（player-turn story-008，同名异 epic）落地；本 story 专攻骰子侧义务 —— AC-1 反射断言 DiceService 无 `UPROPERTY(SaveGame)` Seed 字段（权威 `FRandomStream` 不序列化）+ FixedSeed 非-vacuous 守卫 + FDiceRollResult 四字段；AC-2 `SetSeed` 非确定值 reseed 契约（不回开局种子 + 序列发散，edge 揭示回 S0 复现的 bug 根因）；AC-3 读档不重掷（restore+resume 期 dice OnDiceRolled 0 次 + Die1/Die2 保留）；AC-4 隔离 `UPlayerTurnSaveData` round-trip 逐字段 identity。读档拓扑序中骰子 SetSeed 真接线属 save-load epic（存档21）跨系统编排腿，OoS。
