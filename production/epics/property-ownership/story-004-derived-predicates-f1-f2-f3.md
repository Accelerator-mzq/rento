# Story 004: 派生谓词 F-1/F-2/F-3 (垄断+计数) + 确定性无缓存

> **Epic**: 地产所有权 Property Ownership
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/property-ownership.md`（CR-4/CR-5，F-1/F-2/F-3）
**Requirement**: `TR-prop-014`（派生量实时重算无缓存）、`TR-prop-001`（owner map 读）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0002（GetTilesInGroup，primary）· ADR-0006（snapshot 输入）· ADR-0013
**ADR Decision Summary**: `is_monopoly`/`station_count`/`utility_count` 非存储态，每次从 owner map + 棋盘1 实时派生（确定性整数，无缓存）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 纯整数/集合，无浮点无随机；board `GetTilesInGroup`/`GetTileData(t).TileType` 上游接口（ADR-0002 已 done）。`-nullrhi` 可测。

**Control Manifest Rules (Core)**:
- Required: 派生量实时重算（不存储/无缓存）；全盘遍历 TileType 过滤先于 owner 比较；空集守门。
- Forbidden: 缓存派生量（脏状态）；vacuous-true 空组垄断；查 `RentTable[-1]`（守 count≥1）。
- Guardrail: count ≤ N-1 无 int32 溢出。

---

## Acceptance Criteria

- [ ] **AC-15** 垄断正例：红组 {21,23,24} 全 owner==playerId → `IsMonopoly(playerId,RedGroup)==true`。
- [ ] **AC-16** 他人破坏：{21,23}==playerId、{24}==other → false。
- [ ] **AC-17** INDEX_NONE 破坏：{21,23}==playerId、{24}==INDEX_NONE → false。
- [ ] **AC-18** [关键] 抵押不破坏垄断：红组全归 playerId、21 bIsMortgaged==true → `IsMonopoly==true`。
- [ ] **AC-19** `colorGroup==None` 早出 → false（不进 vacuous-true）。
- [ ] **AC-20** [关键空集守门] 空组 vacuous-true 防护：colorGroup 无任何登记 TileIndex → false（不得返 true，不降 Advisory）。
- [ ] **AC-21** `playerId==INDEX_NONE` 早出 false（不查 map）。
- [ ] **AC-22** `StationCount` 基础：Railroad {5,15,25,35} 中 {5,15}==playerId → 2。
- [ ] **AC-23** `StationCount` 全持：全 4 持 → 4（不溢出）。
- [ ] **AC-24** `StationCount(INDEX_NONE)` → 0（不查 map）。
- [ ] **AC-25** 非 Railroad 不计：持 tile 1(Property)+5(Railroad) → StationCount==1。
- [ ] **AC-26** `UtilityCount` 基础：Utility {12,28} 中 {12}==playerId → 1。
- [ ] **AC-27** `UtilityCount` 全持：全 2 → 2。
- [ ] **AC-28** `UtilityCount(INDEX_NONE)` → 0。
- [ ] **AC-29** [关键] 计数不低报 + 全盘遍历不丢格：player1 逐格拥有 Railroad {5,15,25,35}+Utility {12,28}（owner_map 赋值，**不 mock count**），走全盘遍历 → 严格 ==4/==2（证遍历不丢格）。
- [ ] **AC-29b** 中间数量不低报：逐格拥有 Railroad {5,15}（2 站），经遍历 → 严格 ==2（绝不返 1）。
- [ ] **AC-40** [确定性] owner map 全整数操作；固定 fixture 连调 `IsMonopoly`/`StationCount`/`UtilityCount` 各两次，位级一致、幂等。
- [ ] **AC-40b** [派生非存储] 无错误缓存：第一次 `IsMonopoly(P1,Red)`==false（缺一格），修 owner map 补齐再调 → 返 **true**（反映新 map，非旧缓存）。

---

## Implementation Notes

*Derived from ADR-0002/0006:*

- **F-1 `is_monopoly(playerId,colorGroup)`**：`playerId==INDEX_NONE`/`colorGroup==None` 早出 false；`tiles=GetTilesInGroup(colorGroup)`；**`|tiles|==0 → false`（空集守门，AC-20，C++ `std::all_of` 对空 range 返 true 必加此守门）**；`for t in tiles: if owner_map[t]!=playerId: return false`；else true。**抵押不影响**（垄断=拥有整组，AC-18）。
- **F-2 `station_count(playerId)`**：`INDEX_NONE→0`；`count{t in all_tiles | GetTileData(t).TileType==Railroad AND owner_map[t]==playerId}`。**TileType 过滤先于 owner_map 比较**（求值顺序约束，非可购买格短路，AC-29）。**保证不低报**（实持 2 绝不返 1）。
- **F-3 `utility_count(playerId)`**：结构同 F-2，`TileType==Utility`。
- **派生非存储（AC-40b，prop-014）**：三谓词不作字段存储，每次从 `TileStates` + board 实时重算（避免买地/破产改 owner 后忘同步垄断标记的脏状态）。**无内部缓存**（AC-40b：修 owner map 后重算反映新值）。
- **确定性（AC-40）**：纯整数/集合、无浮点无随机；owner map 快照输入 → 输出确定（联网回放安全）。
- 单格色组（|tiles|==1）F-1 正确返 true（已接受行为）；无组内重复 TileIndex（board AC-19b 拒重号）；组成员归属正确性是 board 数据质量问题（本系统忠实按 GetTilesInGroup 判定，不纠错）。

---

## Out of Scope

- Story 005：BuildOwnershipSnapshot（聚合本 story 谓词）。Story 001/002/003：容器/Buy/Mortgage。
- board 的 GetTilesInGroup/TileType（board epic，已 done）；经济算租消费谓词（economy story-004/005）。

---

## QA Test Cases

- **AC-15~21**：垄断正/破坏/抵押不破坏/None早出/空组守门/INDEX_NONE早出（见 AC 定值）。AC-20 直接断言"空集不得返 true"，不降 Advisory。
- **AC-22~28**：StationCount/UtilityCount 基础/全持/INDEX_NONE/非类型不计（见定值）。
- **AC-29/29b**：逐格 owner_map 赋值（不 mock count）走全盘遍历 → 严格 ==4/==2 / ==2（防丢格/低报）。
- **AC-40**：连调各两次位级一致、幂等。
- **AC-40b**：IsMonopoly 首返 false（缺格）→ 补齐 owner map → 再调返 true（无缓存）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/property-ownership/derived_predicates_test.cpp`（类目 `Rento.Property.DerivedPredicates`）— 存在且通过。AC-20 空集守门 + AC-40b 无缓存须变异坐实。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（容器/GetOwner）
- Unlocks: Story 005（快照聚合谓词）
