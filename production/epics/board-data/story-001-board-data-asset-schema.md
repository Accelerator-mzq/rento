---
Epic: board-data
Status: Ready
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 001 — FBoardTileData 结构 + UBoardDataAsset 载体定义

## Context

- **GDD**: `design/gdd/board-data.md` — CR-3（扁平 struct 字段表）、CR-5.1（色键表）、CR-6（顶层元数据 `bIsPlaceholderData`）、Interactions（接口稳定承诺）
- **Requirement TR-IDs**: TR-board-001、TR-board-002、TR-board-003、TR-board-005、TR-board-007、TR-board-016、TR-board-017
- **ADR Governing**:
  - **ADR-0002（primary）— 棋盘数据容器**：裁定载体 = `UBoardDataAsset : public UPrimaryDataAsset`（非 DataTable，CSV 调参 TArray 列**实务受限**——2026-06-06 spike 更正：非类型拒绝，`FArrayProperty` 受支持，详见 ADR-0002），顶层持元数据 + `TArray<FBoardTileData> Tiles`；接口隔离保证可逆迁移。
  - **ADR-0001 — UObject 宿主与生命周期**：DA 由宿主以 `UPROPERTY` 强引用持有防 GC（持有者实现见 story-002）。
- **Engine**: Unreal Engine 5.7 — Risk: MEDIUM（`UPrimaryDataAsset` 本体 LOW/长期稳定；`TArray` 列原生支持需编辑器内确认）
- **Engine Notes（ADR-0002 Engine Compatibility）**: `UPrimaryDataAsset` 自 5.7 长期稳定（LOW）。**Verification Required ①**：UE5.7 编辑器实测 DataTable CSV 导入含 `TArray<int32>` 列报 `Unsupported Property type`（已通过文档核验为仍不支持，须项目内编辑器二次确认以坐实选型）；本 story 的对端验证是确认 `UBoardDataAsset` 的 Details 面板可正常 `+` 添加 `TArray` 元素。`bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：棋盘载体 = `UBoardDataAsset : public UPrimaryDataAsset`，顶层持元数据字段 + 一个 `TArray<FBoardTileData> Tiles`（source: ADR-0002）。
  - **Required**：`BoardIdentifier: FName` 作顶层 `UPROPERTY` 作者手填，与 `PrimaryAssetId`/路径解耦（AC-28）（source: ADR-0002）。
  - **Required**：`bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离，cook 前 commandlet 检测（source: ADR-0002）。
  - **Required**：字段「只增不改语义」；枚举 append-only（source: ADR-0003/0005）。
  - **Required**：接口隔离——下游经持有者的查询访问，不得各自硬引用 `UBoardDataAsset`（source: ADR-0002）。
  - **Forbidden**：Never use DataTable（`UDataTable`，CSV/JSON 行）作棋盘载体（source: ADR-0002）。Never use 纯 C++ 硬编码 / `.ini`/`.json` 自解析作载体（source: ADR-0002）。
  - **Naming**：`F`=struct、`U`=UObject、`E`=enum；Data Asset 用 `DA_` 前缀（source: Global Rules）。

## Acceptance Criteria

- [ ] **AC-4（CR-3 Property schema 结构）** [Logic] GIVEN 用代码构造的 Property `FBoardTileData` fixture（`ColorGroup=Red, PurchasePrice=300, RentTable=[20,60,180,500,700,900], BuildingCost=200, MortgageValue=150`），WHEN 校验 schema 合法性，THEN `ColorGroup!=None` 且 `PurchasePrice>0` 且 `RentTable.Num()==6` 且 `BuildingCost>0` 且 `DiceMultiplierTable.Num()==0`。
- [ ] **AC-4b（CR-3 Utility schema）** [Logic via fixture] GIVEN 任一 Utility 格 fixture，WHEN 读 `FBoardTileData`，THEN `PurchasePrice>0` 且 `DiceMultiplierTable.Num()==2` 且 `RentTable.Num()==0`（倍率不混入 RentTable）。
- [ ] **AC-7b（Go SalaryAmount schema）** [Logic via fixture] GIVEN Go 格 fixture，WHEN 读 `FBoardTileData`，THEN `SalaryAmount > 0` 且 `SpecialAction==CollectSalary`。
- [ ] **结构定义** `FBoardTileData`（USTRUCT，BlueprintType）含全 13 字段（CR-3 表逐字段），类型/默认值与 ADR-0002 Key Interfaces 一致；`ETileType`（10 项）、`EColorGroup`（8+None）、`ETileAction`（4 项）、`EEventDeck` 封闭枚举定义，枚举值 append-only。
- [ ] **顶层载体** `UBoardDataAsset : public UPrimaryDataAsset` 含 `BoardIdentifier: FName`、`ColorKeyTable: TMap<EColorGroup, FColor>`、`Tiles: TArray<FBoardTileData>`，及 `WITH_EDITORONLY_DATA` 包裹的 `bIsPlaceholderData: bool = true`。
- [ ] **编辑器验证** 可在 UE5.7 编辑器创建一个 `UBoardDataAsset` 实例，`RentTable`(6)/`DiceMultiplierTable`(2) 经 Details 面板 `+` 填入无报错（证 `TArray` 列原生支持）。

## Implementation Notes

（逐字取自 ADR-0002 §Decision / §Key Interfaces / §Implementation Guidelines，语义不改写）

1. 棋盘定义为一个 `UBoardDataAsset : public UPrimaryDataAsset`，顶层持有元数据字段 + 一个 `TArray<FBoardTileData> Tiles`。
2. `FBoardTileData`（GDD CR-3 扁平 struct，载体无关）字段与默认值按 ADR-0002 Key Interfaces：`TileIndex=0`/`TileType=Property`/`DisplayName`/`ColorGroup=None`/`PurchasePrice=0`/`RentTable`（Property=6/Railroad=4）/`DiceMultiplierTable`（Utility=2）/`BuildingCost=0`/`MortgageValue=0`/`TaxAmount=0`/`SalaryAmount=0`（仅 Go 格）/`EventDeck=None`/`SpecialAction=None`。全部用 `UPROPERTY(EditDefaultsOnly)`。
3. `BoardIdentifier: FName` 作顶层 `UPROPERTY` 由作者手填，与 `PrimaryAssetId`/路径解耦——满足 AC-28（取值机制见 story-004）。
4. `bIsPlaceholderData` 建议用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离，gate-check 在 cook 前用 commandlet 读取资产顶层字段检测（cook 门控语义，落地见 story-007）。
5. 接口隔离（R7 可逆性）：`FBoardTileData` struct 载体无关，未来若迁移载体，struct 不变；所有下游经持有者的 `GetTileData`/`GetTilesInGroup` 访问，不得各自硬引用 `UBoardDataAsset`。
6. 接口稳定承诺（GDD §Interactions）：`FBoardTileData` 字段一旦发布只增不改语义（向后兼容）；新增格子类型通过扩展 `ETileType` 枚举，不破坏既有字段；枚举整数索引 append-only。
7. `DisplayName: FText` 的 localization key 控制（OQ-7）：MVP 单语言可 inline FText；若用 C++ CDO 硬编码默认值可用 `NSLOCTEXT("BoardData", "TileName_X", "...")`（ADR-0002 指针，非本 story 强制）。

## Out of Scope

- DA 持有者宿主（UWorldSubsystem）、加载/释放/handle 生命周期 → story-002。
- F-1/F-2/F-3 拓扑算法与 `UBoardMathLibrary` → story-003。
- `GetTileData`/`GetTilesInGroup`/`GetBoardId` 只读查询接口实现 → story-004。
- 加载期校验逻辑（拒绝/警告） → story-005 / story-006。
- `DA_Board_Classic40` 实例 temp-fill 与 `[Config]` 资产测试 → story-007。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

- **AC-4**：GIVEN `FBoardTileData{TileType=Property, ColorGroup=Red, PurchasePrice=300, RentTable={20,60,180,500,700,900}, BuildingCost=200, MortgageValue=150, DiceMultiplierTable={}}` WHEN schema 断言 THEN `ColorGroup!=None && PurchasePrice>0 && RentTable.Num()==6 && BuildingCost>0 && DiceMultiplierTable.Num()==0` 全真。Edge：`RentTable.Num()==5` 应令断言 FAIL（守住长度即语义）。
- **AC-4b**：GIVEN `FBoardTileData{TileType=Utility, PurchasePrice=150, DiceMultiplierTable={4,10}, RentTable={}}` WHEN 读字段 THEN `PurchasePrice>0 && DiceMultiplierTable.Num()==2 && RentTable.Num()==0`。Edge：`RentTable` 非空 → 该 fixture 不应作为合法 Utility（与 story-005 反向校验对接）。
- **AC-7b**：GIVEN `FBoardTileData{TileType=Go, SalaryAmount=200, SpecialAction=CollectSalary}` WHEN 读字段 THEN `SalaryAmount>0 && SpecialAction==CollectSalary`。Edge：`SalaryAmount==0` 的 Go 格 → FAIL。

## Test Evidence

- **Path**: `tests/unit/board/board_data_asset_schema_test.cpp`（[Logic] fixture，`-nullrhi` headless）
- **Status**: 未创建
- 编辑器 `TArray` 列填入验证（手动）→ `production/qa/evidence/`（[Advisory] 截图/sign-off）

## Dependencies

- **Depends on**: 无（Foundation 根 story）。
- **Unlocks**: story-002（持有者需 `UBoardDataAsset` 类型）、story-003（math library 与 struct 无耦合但同 epic）、story-004（查询接口读 `Tiles`/`BoardIdentifier`）、story-005/006（校验读字段）、story-007（实例化资产）、story-008（`BoardIdentifier` 存档引用）。
