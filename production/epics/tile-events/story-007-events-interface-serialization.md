# Story 007: 事件广播 + 接口稳定 + 序列化 + 继承集成

> **Epic**: 事件格 Tile Events
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/tile-events.md`（事件广播 K，接口 L，序列化 M，继承义务 J）
**Requirement**: `TR-event-006`（4 事件 delegate）、`TR-event-008`（出狱卡序列化）、`TR-event-005`（牌堆序列化）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0003（事件总线，primary）· ADR-0005（序列化）· ADR-0007
**ADR Decision Summary**: 4 事件 owner-held `DYNAMIC_MULTICAST_DELEGATE`+`BlueprintAssignable`，enum uint8 基底；出狱卡持有 + 牌堆数组序列化（FCardData 标 SaveGame）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: ⚠ unreal Issue-3：仅"编译通过"不够，int32-backed enum/缺 UPROPERTY 会编译过但 BP 绑定失效 → UHT 零警告核验；G7 序列化 in-memory round-trip。`-nullrhi` 可测。Integration：AC-50/64 集成 + 序列化 round-trip。

**Control Manifest Rules (Foundation/ADR-0003/0005)**:
- Required: 4 事件 owner-held delegate + BlueprintAssignable + enum uint8；牌堆+出狱卡 `UPROPERTY(SaveGame)` 序列化；公共接口 UFUNCTION(BlueprintCallable)。
- Forbidden: 非 dynamic delegate 暴露呈现层；int32-backed enum（BP 绑定失效）；重排枚举整数索引（破坏旧档）。
- Guardrail: UHT 生成零警告。

---

## Acceptance Criteria

- [ ] **AC-55** OnCardDrawn：抽到 Chance BankCredit cardId=C3 → OnCardDrawn(playerId,Chance,C3) 广播 1 次，字段正确，标 BlueprintAssignable+DYNAMIC_MULTICAST_DELEGATE。
- [ ] **AC-56** OnPlayerJailed：落 GoToJail→reason=TileEffect；抽 GoToJailCard→reason=CardEffect；回合2双点链 SendToJail→reason=ConsecutiveDoubles。三 reason 各 1 次。
- [ ] **AC-57** OnPlayerReleased：releaseMethod∈{Card,BailPaid,DoubleDice,ForcedBail} 各出狱路径值正确。
- [ ] **AC-58** [Logic·CI 构建] 四事件均标 BlueprintAssignable、payload USTRUCT(BlueprintType)、enum 字段 uint8 基底；UHT 生成 + headless CI `-nullrhi` 编译**零 UHT 警告**通过。
- [ ] **AC-59** `IsInJail(playerId)` 标 UFUNCTION(BlueprintCallable) ∧ bIsInJail==true→true/false→false ∧ 编译通过。
- [ ] **AC-61** [Advisory·smoke] Config/Data smoke：JAIL_BAIL_AMOUNT=50/MAX_JAIL_TURNS=3/CHANCE_DECK_SIZE=16/CHEST_DECK_SIZE=16 从数据源加载非硬编码；per_house_fee/per_hotel_fee 从事件牌 CardData DA 加载。
- [ ] **AC-64** [Advisory·integration] 出狱卡持有序列化还原：bHoldsChanceOutCard=true ∧ 存档 → 读档 → bHoldsChanceOutCard==true ∧ Chance 堆数组顺序与存档前一致 ∧ 还原后该卡可经 AC-36 路径消费。
- [ ] **AC-50** [Advisory·integration] 双点出狱不进双点链（集成 7+3+2）：bIsInJail=true，mock RollDice bIsDouble=true → bIsInJail==false ∧ ConsecutiveDoubles==0 ∧ 无 bGrantsExtraTurn ∧ TeleportTo 以骰点移动。
- [ ] **AC-51** 出狱掷骰纯消费（dice OQ-T-Dice-3）：出狱掷骰路径 → mock RollDice 调 1 次（无参 `RollDice()`）∧ 仅读 bIsDouble+Total ∧ 无第二次 RollDice。

---

## Implementation Notes

*Derived from ADR-0003/0005:*

- **4 事件（owner-held `DYNAMIC_MULTICAST_DELEGATE`+`BlueprintAssignable`，enum uint8）**：`OnCardDrawn(playerId,deck,cardId)`、`OnTileEventResolved(...)`、`OnPlayerJailed(playerId,reason)`、`OnPlayerReleased(playerId,releaseMethod)`。reason/releaseMethod enum uint8 基底（AC-56/57）。
- **UHT 零警告（AC-58，unreal Issue-3）**：仅"编译通过"不够——int32-backed enum / 缺 UPROPERTY 会编译过但 BP 绑定失效；须 UHT 生成零警告核验（CI headless）。
- **接口（AC-59）**：`IsInJail(playerId)` 等公共接口标 `UFUNCTION(BlueprintCallable)`。
- **序列化（AC-64/AC-61，event-005/008）**：牌堆数组顺序（model B 无指针）+ `bHoldsChanceOutCard`/`bHoldsChestOutCard`（本系统自有）`UPROPERTY(SaveGame)` round-trip（in-memory，G7 只断言恒等）；还原后出狱卡可经 AC-36 消费。`bIsInJail`/`JailTurnsServed` 不在此（player-turn 序列化）。常量/费率从数据源加载（AC-61 smoke）。
- **继承义务集成（AC-50/51，J 节）**：双点出狱不进双点链（集成 7+3+2，AC-50 集成层 + story-006 AC-37 单元层互补）；出狱掷骰纯消费 RollDice 一次（AC-51）。

---

## Out of Scope

- Story 001–006：牌堆/路由/效果/零和/MoveToNearest/监狱裁决（本 story 广播其事件 + 序列化 + 接口）。
- player-turn 的 bIsInJail/JailTurnsServed 序列化（player-turn epic）；存档21 USaveGame 容器（persistence）；下游 UI/VFX/audio 订阅（presentation）。

---

## QA Test Cases

- **AC-55**：抽 Chance BankCredit C3 → OnCardDrawn(p,Chance,C3)×1，字段正确。
- **AC-56**：GoToJail→TileEffect；GoToJailCard→CardEffect；双点链 SendToJail→ConsecutiveDoubles，各×1。
- **AC-57**：OnPlayerReleased releaseMethod {Card/BailPaid/DoubleDice/ForcedBail} 各路径值正确。
- **AC-58**：[CI 构建] 四事件 BlueprintAssignable + USTRUCT(BlueprintType) + enum uint8，UHT 零警告。
- **AC-59**：IsInJail UFUNCTION(BlueprintCallable)，true/false 正确，编译通过。
- **AC-61**：[smoke] 常量 + per_house_fee/per_hotel_fee 从数据源/CardData DA 加载。
- **AC-64**：bHoldsChanceOutCard 存档→读档恒等 ∧ 堆顺序一致 ∧ 可经 AC-36 消费。
- **AC-50/51**：集成双点出狱 ConsecutiveDoubles==0 + 无 ExtraTurn；RollDice 调 1 次纯消费。

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/unit/tile-events/events_interface_test.cpp` + `tests/integration/tile-events/jail_serialization_inheritance_test.cpp`（类目 `Rento.TileEvents.EventsInterfaceSerialization`）— 存在且通过（AC-58 CI 构建 / AC-50/64 集成）。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（牌堆/出狱卡序列化）、Story 006（监狱事件 + 双点出狱）
- Unlocks: None（汇总事件/接口/序列化/继承集成）
