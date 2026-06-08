# Story 005: F-2 nearest + MoveToNearest dice_total 注入 + F-3 RepairFee

> **Epic**: 事件格 Tile Events
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/tile-events.md`（CR-8 MoveToNearest，F-2 nearest，F-3 RepairFee）
**Requirement**: `TR-event-009`（RepairFee 全盘聚合，待 building8）、`TR-event-010`（dice_total holder 注入）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0015（dice_total holder 注入，primary）· ADR-0002（StepsBetween）· ADR-0007/0001
**ADR Decision Summary**: MoveToNearest Utility 额外掷骰经 player-turn `SetCurrentRollContext(FDiceRollResult)` 注入当前程 holder（供经济 F-4 PULL，不串程）；RepairFee 读建房8 `GetTotalHouseCount`/`GetTotalHotelCount`（全盘房/酒店分开计，与 per-tile 区分）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: F-2 环绕搜索整数（StepsBetween）；RepairFee 经 **mock** GetTotalHouseCount（建房8 未实现，event-009 待 building8 epic 真实接线）；F-3 整数纯净。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0015)**:
- Required: MoveToNearest Utility 额外骰经 player-turn 受控写 SetCurrentRollContext 注入 holder（单源不串程）；RepairFee 用全盘聚合接口（房/酒店分开）；F-3 整数纯净。
- Forbidden: RepairFee 误用 per-tile house_count[0,5] 口径（混用）；F-3 用 float；事件格直传经济绕过 holder。
- Guardrail: `12×DiceMult` 不溢出（board AC-23j 校验）。

---

## Acceptance Criteria

- [ ] **AC-27** 环绕最近 Railroad：N=40,from=39,RR={5,15,25,35} → 返回 5（StepsBetween=6 最小，环绕正确）。
- [ ] **AC-28** 多候选取最小步：from=39→5（steps=6）；from=26→35（steps=9）。
- [ ] **AC-29** F-2 确定性 + 单元素：N=40,from=3,{5,15,25,35} 连调两次 → 同值==5 ∧ 与遍历内部排列无关；单元素 {15} → 15。
- [ ] **AC-30** advanceOnZero（from 即目标类型）：N=40,from=5(本身 RR) → StepsBetween(5,5)==0 → 返回 5 ∧ TeleportTo 的 advanceOnZero==true ∧ paysGo==true。
- [ ] **AC-31** 空集 no-op：TypeIndices(type)=={} → 返回 INDEX_NONE ∧ 无 TeleportTo ∧ 无 RollDice ∧ 不崩溃。
- [ ] **AC-49** MoveToNearest 空集不崩溃：TypeIndices(Utility)=={} → INDEX_NONE ∧ 无 TeleportTo ∧ 无 RollDice ∧ no-op。
- [ ] **AC-52** MoveToNearest Utility 额外掷骰 dice_total 注入：抽到 MoveToNearest(Utility) ∧ mock nearest=28 → mock RollDice 调 1 次（额外骰 Total=X）∧ **本系统经 player-turn `SetCurrentRollContext(FDiceRollResult)` 更新 holder 为 X**（mock SetCurrentRollContext 调 1 次、Total==X）∧ TeleportTo(28,paysGo=true,AdvanceCard) ∧ 经济 F-4 结算 dice_total==X（非回合初始骰）。
- [ ] **AC-54** TeleportTo 接口正确（movement OQ-T-Move-2）：① 前进到最近X：TeleportTo(nearestIndex,**paysGo=true**,AdvanceCard)；② 去坐牢：TeleportTo(JailIndex,**paysGo=false**,SentToJail)；③ MoveToTile：TeleportTo(target,paysGo=牌面,AdvanceCard)；三路 target∈[0,N-1]。
- [ ] **AC-32** RepairFee 基本计算：house=3,hotel=1,25/100 → total==175 ∧ Debit(playerId,175)×1。
- [ ] **AC-33** 退化（无建筑）：house=0,hotel=0 → total==0 ∧ Debit 调或不调（零额合法）。
- [ ] **AC-34** 全盘口径区分：mock 来自建房8 `GetTotalHouseCount`/`GetTotalHotelCount`（全盘房/酒店分开计，**非 economy F-2 单地产 [0,5] 口径**），注入聚合 mock 防静默混用。
- [ ] **AC-60** F-3 整数纯净：house=3,hotel=1,25/100 → 两次冷启动/跨 Debug-Shipping 位级一致 ==175（无 float）。

---

## Implementation Notes

*Derived from ADR-0015/0002:*

- **F-2 `nearest_tile_of_type(from, type, N)`**：环绕搜索 `StepsBetween(from, t)` 最小的同类型格（AC-27/28）。**确定性**（与遍历排列无关，AC-29，`mod(t−from,N)` 对不同 t 单射，无真平局）；空集返 INDEX_NONE（AC-31）。
- **MoveToNearest（CR-8）**：① F-2 找 nearest；② Utility 额外掷骰：调骰子3 `RollDice()` 取新 `Total`；③ 经 player-turn 受控写 `SetCurrentRollContext(FDiceRollResult)` **注入当前程 holder**（event-010，ADR-0015，**单源不串程**），使经济 F-4 在 ResolvePhase PULL 到额外骰值；④ `TeleportTo(nearestUtilityIndex, paysGo=true, AdvanceCard)`（AC-52）。`from` 即目标类型 → advanceOnZero=true 整圈（AC-30）。Railroad 按经典可能 2× 租（归经济 Railroad 租，本系统只移动）。
- **F-3 RepairFee（event-009，待 building8）**：`repair_fee_total = GetTotalHouseCount × per_house_fee + GetTotalHotelCount × per_hotel_fee`（典型 25/100）→ 经济 `Debit`。读建房8 `GetTotalHouseCount`（全盘普通房，档1-4）/`GetTotalHotelCount`（全盘酒店，档5），**与 per-tile `house_count[0,5]` 口径区分**（AC-34，防静默混用）。`per_house_fee`/`per_hotel_fee` owner = 事件牌 CardData DA。**建房8 未实现 → 经 mock 接口测试**（AC-21/32/33/34 [Logic] via mock；真实接线待 building8 epic）。F-3 整数纯净（AC-60）。

---

## Out of Scope

- Story 003：MoveToTile/MoveRelative（本 story 只 MoveToNearest）。Story 002：路由。
- 建房8 的 GetTotalHouseCount/GetTotalHotelCount 实现（building epic，event-009 真实接线待此）；经济 F-4 dice_total PULL（economy story-005）+ Debit（economy）；移动 TeleportTo（movement）；player-turn SetCurrentRollContext holder（已实现 pt-005）。

---

## QA Test Cases

- **AC-27/28/29/30/31**：F-2 环绕最近/多候选最小步/确定性/单元素/空集（见定值）。
- **AC-49**：MoveToNearest(Utility) 空集 → INDEX_NONE ∧ 无 Teleport/RollDice ∧ no-op。
- **AC-52**：MoveToNearest(Utility) nearest=28 → RollDice×1(Total=X) ∧ mock SetCurrentRollContext×1(Total==X) ∧ TeleportTo(28,true,AdvanceCard) ∧ F-4 dice_total==X。
- **AC-54**：三路 TeleportTo paysGo/context 正确，target∈[0,N-1]。
- **AC-32/33/34**：RepairFee house=3,hotel=1 → 175（mock GetTotalHouseCount=3/GetTotalHotelCount=1，全盘口径）；house=0,hotel=0 → 0。
- **AC-60**：F-3 两次冷启动/Debug-Shipping 位级一致 175（无 float）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/tile-events/nearest_moveto_repairfee_test.cpp`（类目 `Rento.TileEvents.NearestMoveToRepairFee`）— 存在且通过（RepairFee 经 mock 建房8 接口）。AC-52 holder 注入不串程须坐实。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（牌堆）、Story 002（路由）；引用 ADR-0015 holder（pt-005 已 done）
- Unlocks: None（event-009 真实接线待 building8 epic）
