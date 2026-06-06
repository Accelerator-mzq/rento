# Epic: 棋盘数据

> **Layer**: Foundation
> **GDD**: design/gdd/board-data.md
> **Architecture Module**: #1 棋盘数据 Board Data + §2.1 ④ Board DA Holder
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories board-data`
> **Manifest Version**: 2026-06-06

## Overview

实现棋盘的**静态空间地基**——零依赖、纯只读的格子布局数据系统。采用 `UBoardDataAsset : UPrimaryDataAsset`
作为 `FBoardTileData`（`TileType`/`ColorGroup`/`PurchasePrice`/`MortgageValue`/`RentTable`/
`BuildingCost`/`DiceMultiplierTable`/`TaxAmount`/`SalaryAmount`/`EventDeck`/`SpecialAction`/
`JailIndex`/`DisplayName`）的运行时载体（DataTable CSV 不支持 `TArray` 列已实证）。④ Board DA Holder
（宿主以 `UPROPERTY` 强引用防 GC）向上层供 `GetTileData`/`GetTilesInGroup`/`AdvanceIndex`（原子返回
`(newIndex, passedGo)`）/`GetTileWorldTransform`/环序 N 等只读查询，被移动(4)/所有权(6)/事件(7)/建房(8)
消费。接口隔离保证载体可逆迁移；存档存 `BoardIdentifier:FName` 引用而非全量布局。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0002: 棋盘数据容器 | `UPrimaryDataAsset` 载体（非 DataTable，CSV 不支持 TArray 列）；`AdvanceIndex` 返回 struct；`UBoardMathLibrary` BlueprintPure；接口隔离可逆迁移 | **MEDIUM**（`UAssetManager` 同步/异步加载 Primary DA 的 5.7 API 签名待验证）|
| ADR-0001: UObject 宿主与生命周期 | DA Holder 宿主 + `UPROPERTY` 强引用防 GC；`Deinitialize` `CancelHandle` 防 PIE Stop 空棋盘 | LOW |
| ADR-0005: 存档序列化契约 | 存 DA 引用 `BoardIdentifier` 不存全量布局（TR-board-006）| LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-board-001 | `FBoardTileData` 扁平 struct 含 `TArray<int32>` 列（RentTable/DiceMultiplierTable）| ADR-0002 ✅ |
| TR-board-002 | 运行时载体选定 Primary Data Asset（下游 4/6/7/8 硬依赖）| ADR-0002 ✅ |
| TR-board-003 | 棋盘级顶层元数据 BoardIdentifier/色键表/bIsPlaceholderData 落位 | ADR-0002 ✅ |
| TR-board-004 | bIsPlaceholderData 须可被 cook 前 gate-check 检测 | ADR-0002 ✅ |
| TR-board-005 | `GetBoardId()` 与资产路径/PrimaryAssetId 解耦（FName 改名不损存档）| ADR-0002 ✅ |
| TR-board-006 | 存档存棋盘 DA 引用 ID 而非全量布局 | ADR-0005 ✅ |
| TR-board-007 | 运行时 DA 持有者宿主 + `UPROPERTY` 强引用防 GC | ADR-0001 ✅ |
| TR-board-008 | 热切换边界：Loaded→Validated→Active，禁 Active 热替换 DA | ADR-0001/0002 ✅ |
| TR-board-009 | `GetTileData(index)` 只读 O(1)，载入预算 <100ms | ADR-0002 ✅ |
| TR-board-010 | 只读查询接口集（GetTileCount/GetTileData/GetTilesInGroup 升序/GetBoardId）| ADR-0002 ✅ |
| TR-board-011 | `AdvanceIndex` 单一原子接口返回 (new_index, passed_go) 元组 | ADR-0002 ✅ |
| TR-board-012 | F-1/F-2/F-3 拓扑算法封装 `UBoardMathLibrary` BlueprintPure，签名定型 | ADR-0002 ✅ |
| TR-board-013 | 加载期完整性校验（Validated），失败结构化错误码拒绝进入对局 | ADR-0002 ⚠ Partial |
| TR-board-014 | [Config] 资产校验测试 harness + [Logic] fixture headless | ADR-0002 ✅ |
| TR-board-015 | 地图编辑器(26)产出符合 schema 新棋盘，可输入 RentTable 多值 | ADR-0002 ✅ |
| TR-board-016 | 接口契约只增不改语义（向后兼容）；枚举 append-only | ADR-0002/0005 ⚠ Partial |
| TR-board-017 | 载体可逆迁移：GetTileData 接口隔离载体细节 | ADR-0002 ✅ |

**Untraced Requirements**: None（17/17 有 ADR 覆盖；TR-013/016 标 Partial = 跨 ADR 协同，非 Gap）

## Definition of Done

This epic is complete when:
- All stories implemented, reviewed, closed via `/story-done`
- All acceptance criteria from `design/gdd/board-data.md` verified
- All Logic stories（拓扑算法 F-1/F-2/F-3、AdvanceIndex 原子性、校验错误码）有 headless 通过的测试于 `tests/unit/board-data/`
- [Config] 资产校验 harness（commandlet/编辑器，非 -nullrhi）就位
- **Sprint0 引擎验证 ADR-0002 #6**：`UAssetManager` 同步/异步加载 Primary DA 的 5.7 API 签名 + DataTable CSV `TArray` 列报错二次确认 + Blueprint `Floor(float)→int32` 重载类型推导

## Next Step

Run `/create-stories board-data` to break this epic into implementable stories.
