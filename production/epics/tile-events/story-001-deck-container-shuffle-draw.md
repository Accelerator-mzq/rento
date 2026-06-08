# Story 001: 牌堆容器 + Fisher-Yates 洗牌 + 抽牌机制

> **Epic**: 事件格 Tile Events
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/tile-events.md`（CR-3 抽牌 / CR-5 出狱卡持有）
**Requirement**: `TR-event-001`、`TR-event-003`、`TR-event-004`、`TR-event-005`、`TR-event-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0004（确定性 RNG，primary）· ADR-0002（牌堆容器）· ADR-0001（宿主）· ADR-0005（序列化）
**ADR Decision Summary**: 牌堆 model B 有序 `TArray<FCardData>`（抽=取 index0、非出狱卡 append 回尾，无独立指针）；开局 Fisher-Yates 经骰子3 `FRandomStream` 权威流 + 种子序列化；禁 `FMath::Rand`。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `FRandomStream` 权威流（DiceService 注入）；`FCardData` `UPROPERTY(SaveGame)`；`-nullrhi` 可测（固定种子→精确牌序）。

**Control Manifest Rules (Core/Foundation/ADR-0004/0001)**:
- Required: per-match 宿主 `UWorldSubsystem`；洗牌经骰子3 `FRandomStream` + 种子序列化；禁全局随机 `FMath::Rand`。
- Forbidden: `FMath::Rand`/`rand()`/`std::rand`（权威纯净门 pt-009 CI 硬门）；独立抽牌指针（牌序即数组顺序）。
- Guardrail: 出狱卡离堆期 `Num()` 减 1、回堆恢复。

---

## Acceptance Criteria

- [ ] **AC-10** 抽牌回底循环：Chance 堆 [A,B,C]，抽两次 → 第一次抽 A（堆→[B,C,A]）∧ 第二次抽 B（堆→[C,A,B]）∧ 堆长恒==3（非出狱卡场景）。
- [ ] **AC-11** 出狱卡离堆：Chance 堆 GetOutOfJailFree 在堆顶，抽到 → `bHoldsChanceOutCard==true` ∧ Chance 堆 `Num()`==CHANCE_DECK_SIZE-1（离堆不回底）∧ 后续抽牌跳过它。
- [ ] **AC-12** 出狱卡用后回底：`bHoldsChanceOutCard==true` 且玩家消费该卡出狱 → `bHoldsChanceOutCard==false` ∧ Chance 堆 `Num()`==CHANCE_DECK_SIZE（回底）∧ 数组末元素是该出狱卡。
- [ ] **AC-13** 牌堆不抽空（出狱卡被持有时）：出狱卡已持有（堆剩 15），连抽 15 次 → 每次取得非 null ∧ 第 16 次轮回堆首 ∧ 不崩溃。
- [ ] **AC-14** 每堆各 1 张出狱卡：初始化完成两堆 → 各堆 GetOutOfJailFree count==1。
- [ ] **洗牌确定性（event-004）**：固定种子 Fisher-Yates → 精确牌序（两次冷启动一致）；种子随存档序列化。
- [ ] **序列化（event-005/008）**：牌堆数组顺序（model B 无指针）+ `bHoldsChanceOutCard`/`bHoldsChestOutCard` round-trip 恒等。

---

## Implementation Notes

*Derived from ADR-0004/0002/0001:*

- 宿主 = `UWorldSubsystem`（per-match）。两牌堆 Chance/CommunityChest 各 16 张（`CHANCE_DECK_SIZE`/`CHEST_DECK_SIZE`），各为有序 `TArray<FCardData>`（`FCardData` 标 `UPROPERTY(SaveGame)`）。
- **抽牌（model B，AC-10）**：取 index0 → 执行效果（story-003）→ 非出狱卡 `append` 回尾、出狱卡移出不 append。**无独立抽牌指针**（牌序即数组顺序，存档只序列化数组顺序即精确重现）。
- **出狱卡（CR-5，AC-11/12/14）**：抽到 Chance 堆 `GetOutOfJailFree` → `bHoldsChanceOutCard=true`、该牌离 Chance 堆（不回底直到被用）；消费出狱 → 清标记 + 牌回**该堆**底。Chest 同理。两标记为本系统(7)自有状态（不在 player-turn PlayerState），随存档序列化。`HoldsAnyOutCard := bHoldsChanceOutCard ∨ bHoldsChestOutCard`。
- **洗牌（CR-3，event-004）**：开局用骰子(3) `FRandomStream` + **Fisher-Yates**（两堆各洗一次）；种子在定序后首回合开始时确定、随存档序列化（可重放）。**禁 `FMath::Rand`**（权威纯净门 pt-009 CI 覆盖）。
- **序列化**：牌堆数组顺序（model B 无指针）+ `bHolds*OutCard` round-trip（in-memory，G7 只断言恒等）。`bIsInJail`/`JailTurnsServed` 不在此（归 player-turn 序列化，story-006）。

---

## Out of Scope

- Story 002：落地路由（本 story 只建牌堆+抽牌机制）。Story 003：牌效果执行（抽到后做什么）。
- Story 006：监狱态字段（player-turn 序列化）。Story 007：事件广播。
- 骰子3 的 FRandomStream/RollDice（dice epic 已 done）。

---

## QA Test Cases

- **AC-10**：Chance [A,B,C] 抽两次 → A（→[B,C,A]）、B（→[C,A,B]），堆长恒 3。
- **AC-11**：堆顶 GetOutOfJailFree 抽到 → bHoldsChanceOutCard==true ∧ Num()==15 ∧ 后续跳过。
- **AC-12**：消费出狱卡 → bHoldsChanceOutCard==false ∧ Num()==16 ∧ 末元素是该卡。
- **AC-13**：出狱卡持有（剩 15），连抽 15 → 非 null ∧ 第 16 轮回 ∧ 不崩。
- **AC-14**：两堆各 GetOutOfJailFree count==1。
- **洗牌**：固定种子 → 精确牌序（两次冷启动一致）；禁 FMath::Rand（grep 权威纯净）。
- **序列化**：牌堆顺序 + bHolds*OutCard round-trip 恒等。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/tile-events/deck_shuffle_draw_test.cpp`（类目 `Rento.TileEvents.DeckShuffleDraw`）— 存在且通过。洗牌确定性固定种子序列。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None（骰子3 FRandomStream 已 done）
- Unlocks: Story 002（路由触发抽牌）、003（效果执行）、006（出狱卡消费）、007（序列化）
