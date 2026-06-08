# Story 002: 落地路由 ResolveTileEvent + SentToJail 抑制 + no-op

> **Epic**: 事件格 Tile Events
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/tile-events.md`（CR-2 落地路由）
**Requirement**: `TR-event-001`（宿主路由）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0001（宿主，primary）· ADR-0002（GetTileData 路由）
**ADR Decision Summary**: `ResolveTileEvent` 据棋盘 `GetTileData(index).TileType`/`SpecialAction` 路由（Chance/Chest/Tax/GoToJail/Go/Jail/FreeParking）；`arrivalContext==SentToJail` 抑制全结算。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 路由读 board GetTileData；`-nullrhi` 可测（mock board + spy Debit/TeleportTo/抽牌）。

**Control Manifest Rules (Core)**:
- Required: 路由读 board 静态格数据；SentToJail 自防御抑制；Go/Jail/FreeParking no-op。
- Forbidden: 路由层直接操作金钱（金钱由牌效果执行）；本系统二次发薪（发薪在移动层）。
- Guardrail: SentToJail 时不抽牌/不收租/不缴税。

---

## Acceptance Criteria

- [ ] **AC-1** [Logic]（路由表完整性，承前）正常落地按 TileType 分派到对应处理。
- [ ] **AC-2** `arrivalContext==SentToJail` 抑制全结算：SentToJail → mock Debit/Credit==0 ∧ TeleportTo==0 ∧ 无抽牌 ∧ bIsInJail 不因本次调用改变（本系统自防御；上游不调归 player-turn AC-46）。
- [ ] **AC-3** Chance 路由：TileType=Chance ∧ DiceMove → 从 Chance 堆抽顶张一次 ∧ 路由层不直接操作金钱。
- [ ] **AC-4** CommunityChest 路由：同 AC-3，堆为 CommunityChest。
- [ ] **AC-5** Tax 路由：TileType=Tax ∧ mock TaxAmount=200 → mock Debit(playerId,200) 恰 1 次 ∧ 无 TeleportTo ∧ 无抽牌。
- [ ] **AC-6** GoToJail 路由：SpecialAction=GoToJail → mock TeleportTo(JailIndex,paysGo=false,SentToJail) 恰 1 次 ∧ bIsInJail==true ∧ JailTurnsServed==0 ∧ 无 Debit/Credit ∧ 无抽牌。
- [ ] **AC-7** Go no-op：TileType=Go → 无 Debit/Credit/TeleportTo/抽牌（发薪在移动层，不二次发薪）。
- [ ] **AC-8** Jail 探视 no-op：TileType=Jail ∧ bIsInJail==false → 无副作用。
- [ ] **AC-9** FreeParking no-op（MVP）：TileType=FreeParking ∧ SpecialAction!=TriggerHouseRuleCheck → 无副作用。

---

## Implementation Notes

*Derived from ADR-0001/0002:*

- **`ResolveTileEvent(player, tileIndex, arrivalContext)`**（由回合2 ResolvePhase 调，非地产落地）：读 board `GetTileData(tileIndex)` 的 `TileType`/`EventDeck`/`TaxAmount`/`SpecialAction` 路由：
  - `Chance`/`CommunityChest` → 抽对应牌堆顶张（story-001 抽牌）→ 执行效果（story-003）。路由层本身不操作金钱（AC-3/4）。
  - `Tax` → 调经济 `Debit(playerId, TaxAmount)`（AC-5）。
  - `GoToJail`（`SpecialAction==GoToJail`）→ `SendToJail(player)`（story-006，AC-6）。
  - `Go`/`Jail`（探视）/`FreeParking` → no-op（AC-7/8/9；发薪在移动层不二次发薪）。
- **SentToJail 抑制（AC-2）**：`arrivalContext==SentToJail` → 全结算抑制（不抽牌/不收租/不缴税/不 TeleportTo），本系统自防御。上游 ResolvePhase 不调本系统的实际抑制归 player-turn AC-46（已实现）。
- 入狱转换（SendToJail）/出狱裁决在 story-006；牌效果执行在 story-003；本 story 只负责路由分派 + SentToJail 抑制 + no-op 格。

---

## Out of Scope

- Story 001：牌堆抽牌（本 story 触发抽牌，机制在 001）。Story 003：牌效果执行。Story 006：SendToJail/出狱裁决（本 story 路由 GoToJail→调 SendToJail）。
- player-turn 的 ResolvePhase 调用 + AC-46 抑制（player-turn epic 已实现）；经济 Debit（economy epic）。

---

## QA Test Cases

- **AC-2**：SentToJail → Debit/Credit/TeleportTo/抽牌 全 0 ∧ bIsInJail 不变。
- **AC-3/4**：Chance/Chest + DiceMove → 抽对应堆顶张×1 ∧ 路由不操作金钱。
- **AC-5**：Tax + TaxAmount=200 → Debit(p,200)×1 ∧ 无 Teleport/抽牌。
- **AC-6**：GoToJail → TeleportTo(JailIndex,false,SentToJail)×1 ∧ bIsInJail==true ∧ JailTurnsServed==0 ∧ 无金钱/抽牌。
- **AC-7/8/9**：Go/Jail探视/FreeParking → 无副作用。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/tile-events/tile_event_routing_test.cpp`（类目 `Rento.TileEvents.Routing`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（牌堆抽牌）
- Unlocks: Story 003（路由后效果执行）
