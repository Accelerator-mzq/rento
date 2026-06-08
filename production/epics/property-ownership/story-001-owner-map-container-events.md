# Story 001: owner map 容器 (AoS) + 受控写封装 + 事件契约

> **Epic**: 地产所有权 Property Ownership
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/property-ownership.md`（CR-1/CR-7，事件节）
**Requirement**: `TR-prop-001`、`TR-prop-002`、`TR-prop-003`、`TR-prop-006`、`TR-prop-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0013（运行时容器/封装，primary）· ADR-0001（宿主）· ADR-0007（封装）· ADR-0003（事件）
**ADR Decision Summary**: 运行时状态 = **AoS** `TArray<FTileOwnershipState>`（owner+bIsMortgaged 同记录、稠密 N、非可购买格哨兵 `{INDEX_NONE,false}`、连续 0..N-1）；C++ private + 只读访问器；`OnOwnershipChanged`/`OnMortgageChanged` owner-held delegate。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `FTileOwnershipState` USTRUCT（`UPROPERTY(SaveGame)`）；`-nullrhi` 可测。`TArray<bool>` bitfield 前提已证伪（ADR-0013，本设计 bool 内含于记录）。

**Control Manifest Rules (Core/Foundation)**:
- Required: per-match 服务挂 `UWorldSubsystem`；写权威状态（owner map）→ C++ private；事件 owner-held delegate + BlueprintAssignable；`On<PastTense>` 命名。
- Forbidden: 外部系统直写 owner map / 记录字段；集中式 Event Bus；非 dynamic delegate 暴露呈现层。
- Guardrail: 非可购买格哨兵使全盘遍历不越界。

---

## Acceptance Criteria

- [ ] **AC-1** 初始状态：空局初始化，读任意可购买格 owner → `owner==INDEX_NONE` ∧ `bIsMortgaged==false`。
- [ ] **AC-2** 无主格守门：可购买格 `owner==INDEX_NONE`，`GetOwner`/`IsMortgaged` → 返 `INDEX_NONE`/`false`，不崩、不触发事件。
- [ ] **AC-3** [Advisory·code-review / OQ-Prop-1 未决按最严，ADR-0013 已升 Logic] 受控写软约束：全代码库无外部系统直写 owner map / `bIsMortgaged`，只经受控入口；C++ private 封装静态扫描。
- [ ] **AC-4** 非可购买格不进 owner map：`TileType ∈ {Go/Tax/Jail/FreeParking/Chance/CommunityChest}`，`GetOwner(tile)` → `INDEX_NONE` + dev ensure。
- [ ] **事件契约（prop-010）**：`OnOwnershipChanged(TileIndex,OldOwner,NewOwner)`/`OnMortgageChanged(TileIndex,bIsMortgaged)` 标 `DYNAMIC_MULTICAST_DELEGATE`+`BlueprintAssignable`，payload USTRUCT/blittable，编译通过。

---

## Implementation Notes

*Derived from ADR-0013/0001/0007/0003:*

- 容器 = `TArray<FTileOwnershipState> TileStates`（C++ `private` 成员，ADR-0001 宿主 `UWorldSubsystem`）；`FTileOwnershipState { UPROPERTY(SaveGame) int32 OwnerPlayerId = INDEX_NONE; UPROPERTY(SaveGame) bool bIsMortgaged = false; }`。
- **初始化**：`TileStates.SetNum(N)`（N=棋盘格数）→ 全槽默认哨兵 `{INDEX_NONE,false}`（`SetNum` 默认构造即得）。**非可购买格槽位亦存哨兵**（AC-4：`GetOwner` 非可购买格返 INDEX_NONE + dev ensure；哨兵使全盘遍历 `TileStates[t]` 不越界）。前提：TileIndex 连续 `0..N-1`（board 加载校验保证）。
- **封装（AC-3）**：`TileStates` C++ private、无 `BlueprintReadWrite`（仅 `UPROPERTY(SaveGame)`）；只读访问器 `GetOwner(tile)`/`IsMortgaged(tile)`（`BlueprintPure/Callable`）；写仅经 Buy/Mortgage/Unmortgage/TransferOwnership/ReturnTileToBank（后续 story）。据此 AC-3 静态扫描升 [Logic]。
- **事件（prop-010）**：`OnOwnershipChanged(TileIndex,OldOwner,NewOwner)`/`OnMortgageChanged(TileIndex,bIsMortgaged)` owner-held `DYNAMIC_MULTICAST_DELEGATE`+`BlueprintAssignable`，payload blittable int32/bool。**收租不触发本系统事件**（落地收租不改归属，事件源是经济 OnRentPaid）。本 story 定义容器+访问器+事件契约；实际广播由 Buy/Mortgage/transfer（后续 story）。

---

## Out of Scope

- Story 002/003：Buy/Mortgage/Unmortgage 写路径（本 story 只建容器+访问器+事件契约）。
- Story 006：TransferOwnership/ReturnTileToBank。Story 007：序列化/读档重绑。
- 派生谓词（story-004）；快照（story-005）。

---

## QA Test Cases

- **AC-1**：空局初始化 → 任意可购买格 GetOwner==INDEX_NONE ∧ IsMortgaged==false。
- **AC-2**：owner==INDEX_NONE → GetOwner/IsMortgaged 返 INDEX_NONE/false，不崩、无事件。
- **AC-3**：[静态扫描] 无外部直写 TileStates/记录字段（C++ private + 无 BP 写属性）。
- **AC-4**：TileType ∈ 非可购买格 → GetOwner(tile)==INDEX_NONE + dev ensure（AddExpectedError）。
- **事件契约**：[CI 构建] 两 delegate BlueprintAssignable/DYNAMIC_MULTICAST，payload blittable 编译通过。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/property-ownership/owner_map_container_test.cpp`（类目 `Rento.Property.OwnerMapContainer`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None（ADR-0013/0001 已 Accepted）
- Unlocks: Story 002–007（全部经容器/访问器/事件）
