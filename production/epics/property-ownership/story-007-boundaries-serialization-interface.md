# Story 007: 职责边界 + 接口稳定 + 序列化/读档重绑

> **Epic**: 地产所有权 Property Ownership
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/property-ownership.md`（CR-7，序列化，接口稳定承诺）
**Requirement**: `TR-prop-004`、`TR-prop-005`、`TR-prop-013`、`TR-prop-015`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0005（序列化/读档重绑，primary）· ADR-0003（事件）· ADR-0007（接口）
**ADR Decision Summary**: owner map + bIsMortgaged 序列化随存档21；派生量不存读档后重算（PostLoad 晚于棋盘1 加载）；监听器读档重绑（先解后绑，防 dangling delegate）；公共接口 UFUNCTION/事件 BlueprintAssignable。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: ⚠ G7：内置 SaveGameToMemory 不按 SaveGame 过滤（全量序列化，见 ADR-0005）；round-trip in-memory 只断言恒等。读档拓扑序：DA → 经济/地产/建房/事件格互不依赖 → 回合2。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0005/0007)**:
- Required: owner map+bIsMortgaged `UPROPERTY(SaveGame)` 序列化；派生量不存读档重算；PostLoad 晚于 board 加载；读档重绑先 Unbind 后 Bind；公共接口 UFUNCTION(BlueprintCallable)。
- Forbidden: 序列化派生量（脏状态）；读档重建 A 时读尚未重建的 B；不持 Cash/决策点/静态数据/租金公式（CR-7）。
- Guardrail: PostLoad 派生量重算须 `GetTilesInGroup` 可用（board 已加载）。

---

## Acceptance Criteria

- [ ] **AC-36** [Advisory·code-review] 6 不持 Cash（实现内无 PlayerState.Cash 读写）。
- [ ] **AC-37** [Advisory·code-review] 6 不持决策点（入口无"是否应该买/抵押"策略判断）。
- [ ] **AC-38** [Advisory·code-review] 6 不持静态棋盘数据（colorGroup 成员/TileType 来自 board GetTileData，不硬编码）。
- [ ] **AC-39** [Advisory·code-review] 6 不持租金公式（谓词只返归属，无 RentTable 查找/乘法）。
- [ ] **AC-41** [Logic·CI 构建] 公共接口标 `UFUNCTION(BlueprintCallable)`、`OnOwnershipChanged`/`OnMortgageChanged` 标 `BlueprintAssignable`、payload `USTRUCT(BlueprintType)`、headless `-nullrhi` 编译通过。
- [ ] **序列化 round-trip（prop-005）**：owner map + bIsMortgaged round-trip 恒等（SaveGameToMemory/LoadGameFromMemory，逐格 GetOwner/IsMortgaged 恒等）。
- [ ] **PostLoad 派生量重算（prop-004）**：读档后派生量（is_monopoly/count）不存、实时重算；重算晚于棋盘1 加载（GetTilesInGroup 可用）。
- [ ] **读档重绑（prop-013）**：监听器读档后重绑（先 Unbind 后 Bind），不依赖序列化保存的绑定（防 dangling delegate）。

---

## Implementation Notes

*Derived from ADR-0005/0007/0003:*

- **序列化（prop-005）**：`TileStates`（owner map + bIsMortgaged，每可购买格）`UPROPERTY(SaveGame)` 随存档21序列化（以 TileIndex 为键，story-001 容器已标）。round-trip 走 in-memory（G7：内置路径全量序列化，只断言恒等，参 pt-008/economy story-010）。
- **派生量不存（prop-004）**：`is_monopoly`/`station_count`/`utility_count` **不序列化**；读档后实时重算（story-004 谓词）。**PostLoad 重算顺序**：须晚于棋盘1 加载完成（`GetTilesInGroup` 可用，读档拓扑序 DA→...→本系统，ADR-0005）。本系统无回合内瞬态、无中途存档点问题。
- **读档重绑（prop-013）**：监听器（VFX19 等）的 `OnOwnershipChanged` 绑定**不随序列化保存**；读档后由存档21 广播 `OnGameLoaded`、监听者先 Unbind 后 Bind（防 dangling delegate，ADR-0005 CR-6；非本系统职责，本系统保证读档后容器/事件可用）。
- **职责边界（CR-7，AC-36~39）**：本系统只产出归属真值+抵押标记+派生垄断/计数。不持 Cash（经济5）、房屋数/建造规则（建房8）、决策点（回合2/AI10）、静态数据（棋盘1）、租金公式（经济5）。code-review grep 核验。
- **接口稳定（AC-41）**：`Buy`/`Mortgage`/`Unmortgage`/`GetOwner`/`IsMortgaged`/`IsMonopoly`/`GetStationCount`/`GetUtilityCount`/`BuildOwnershipSnapshot`/`TransferOwnership`/`ReturnTileToBank` 标 `UFUNCTION(BlueprintCallable)`；事件 `BlueprintAssignable`；payload `USTRUCT(BlueprintType)`；headless 编译通过（CI 构建）。

---

## Out of Scope

- 存档21 的 USaveGame 容器/完整性门（persistence，ADR-0005）；呈现层 delegate 读档重绑实现（VFX/HUD）。
- Story 001–006：容器/事务/谓词/快照/破产移交。

---

## QA Test Cases

- **AC-36~39**：[code-review] 实现内无 Cash 读写 / 无决策点策略 / 无硬编码静态数据 / 无 RentTable 查找乘法。
- **AC-41**：[CI 构建] 公共接口 UFUNCTION(BlueprintCallable)、事件 BlueprintAssignable、payload USTRUCT(BlueprintType)、headless 编译通过。
- **序列化**：Given owner map {21→P1, 23→P2(mortgaged)}；When SaveGameToMemory→LoadGameFromMemory；Then 逐格 GetOwner/IsMortgaged 恒等。
- **PostLoad**：读档后 IsMonopoly/count 实时重算（不存）；重算前 board 已加载（GetTilesInGroup 可用）。
- **读档重绑**：监听器读档后先 Unbind 后 Bind（不 dangling）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/property-ownership/boundaries_serialization_test.cpp` + `tests/integration/property-ownership/owner_map_save_roundtrip_test.cpp`（类目 `Rento.Property.BoundariesSerialization`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（容器序列化）、Story 004（派生量重算）
- Unlocks: None（汇总边界/接口/序列化）
