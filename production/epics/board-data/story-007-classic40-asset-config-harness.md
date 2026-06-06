---
Epic: board-data
Status: Ready
Layer: Foundation
Type: Config-Data
Estimate: L
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 007 — DA_Board_Classic40 temp-fill + [Config] 资产校验 harness + bIsPlaceholderData cook 门控

## Context

- **GDD**: `design/gdd/board-data.md` — CR-6（经典 40 格布局 + temp-fill）、CR-5.1（色键表）、测试分层（[Config] 须编辑器/commandlet 环境，非 -nullrhi）、AC-3/4-asset/4b/4c/5/6/7/7b
- **Requirement TR-IDs**: TR-board-004、TR-board-014、TR-board-015
- **ADR Governing**:
  - **ADR-0002（primary）— 棋盘数据容器**：`[Config]` 资产校验测试须有清晰 harness（commandlet/编辑器环境）；temp-fill `DA_Board_Classic40` 经典公开参考值，`bIsPlaceholderData=true`；`bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` + cook 前 commandlet 检测；地图编辑器(26)经 Details 面板 `TArray` 元素 UI 输入多值（无 CSV 限制）。
  - **ADR-0001 — UObject 宿主与生命周期**：资产为设计期 `Authored` 态，运行时由持有者加载（story-002）；本 story 聚焦资产内容 + 资产层测试。
  - **ADR-0005 — 存档序列化契约**（参考，非 primary）：`bIsPlaceholderData==true` 的棋盘不得进入 Alpha 构建（producer/gate-check 守门），与本 story AC-4c cook 门控对齐。
- **Engine**: Unreal Engine 5.7 — Risk: MEDIUM（`UAssetManager` 枚举扫描 + DataTable CSV `TArray` 限制二次确认 post-cutoff）
- **Engine Notes**: ADR-0002 **Verification Required ①（2026-06-06 spike 更正）**：DataTable CSV 类型层 `FArrayProperty` 受支持（**不报** `Unsupported Property type`），DataTable 调参不便属**实务层**（数组字面量 `(a,b,c)`/逗号冲突、转义复杂）；本 story 编辑器实测确认 `UBoardDataAsset` Details 面板 `TArray` 多值输入正常（DA 选型实务可用性坐实）。**Sprint0 引擎验证 #6**：`UAssetManager` 加载签名 + DataTable CSV `TArray` 列报错二次确认。**[Config] 测试约束**：需编辑器/commandlet 环境，**不能在 `-nullrhi` headless 跑**——因此只能作 nightly/staging gate，不能作 PR merge gate（须传递给 devops-engineer 配置 CI pipeline）。`bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离，gate-check 在 cook **前** commandlet 读取（非运行时）。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：`bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离，cook 前 commandlet 检测（source: ADR-0002）。
  - **Required**：`BoardIdentifier: FName` 作顶层 `UPROPERTY` 作者手填，与路径解耦（source: ADR-0002）。
  - **Required**：数据驱动——gameplay 数值与可调参数外置 Data Asset，绝不硬编码（source: ADR-0002/coding-standards）。
  - **Forbidden**：Never use DataTable 作棋盘载体（CSV 调参 TArray 列**实务受限**——2026-06-06 spike 更正：类型层 `FArrayProperty` 受支持、不报 `Unsupported Property type`，DataTable 不便属实务层〔数组字面量/逗号冲突〕；DA 选型不变）（source: ADR-0002）。
  - **Naming**：Data Asset 用 `DA_` 前缀（`DA_Board_Classic40`）（source: Global Rules）。

## Acceptance Criteria

- [ ] **AC-3（CR-2 类型完整 精确计数）** [Config] GIVEN 经典盘 DA，WHEN 遍历 40 格 `TileType`，THEN 每格均属 `ETileType` 合法枚举成员，且各枚举值出现次数精确为：`Go=1`、`Jail=1`、`FreeParking=1`、`GoToJail=1`、`Tax=2`、`Chance=3`、`CommunityChest=3`、`Railroad=4`、`Utility=2`、`Property=22`。
- [ ] **AC-4-asset（CR-6 temp-fill 已填）** [Config] GIVEN `DA_Board_Classic40`，WHEN 遍历所有 Property 格，THEN 每格 `PurchasePrice>0` 且 `BuildingCost>0`（验证 temp-fill 已实际写入资产，而非默认 0）。
- [ ] **AC-4b（CR-3 Utility schema 资产）** [Config] GIVEN 任一 Utility 格，WHEN 读 `FBoardTileData`，THEN `PurchasePrice>0` 且 `DiceMultiplierTable.Num()==2` 且 `RentTable.Num()==0`。
- [ ] **AC-4c（temp-fill 硬门控）** [Config] GIVEN `DA_Board_Classic40`，WHEN 读 DA 顶层 `bIsPlaceholderData`，THEN 在 Alpha 里程碑构建中必须为 `false`；若为 `true` 则测试失败、Alpha gate 不通过。标 `gate_level: alpha_blocking`（MVP/nightly 允许 `true`，Alpha 门控强制 `false`）。
- [ ] **AC-5（CR-4 数组语义长度）** [Config] GIVEN 经典盘，WHEN 按类型查数组长度，THEN Property `RentTable`==6、Railroad `RentTable`==4、Utility `DiceMultiplierTable`==2 且 `RentTable`==0、非购买格 `RentTable`==0。
- [ ] **AC-6（CR-5 色组成员）** [Config] GIVEN 经典盘，WHEN 对 8 色组调 `GetTilesInGroup`，THEN 数量为 棕2/浅蓝3/粉3/橙3/红3/黄3/绿3/深蓝2，合计 22。
- [ ] **AC-7（CR-6 经典布局精确匹配）** [Config] GIVEN `DA_Board_Classic40`（已 temp-fill），WHEN 逐格比对 `TileType`（及 Property 的 ColorGroup）与 CR-6 规格表，THEN 全 40 格匹配；统计=22地产+4车站+2公用+4角+2税+3机会+3命运。
- [ ] **AC-7b（Go SalaryAmount schema 资产）** [Config] GIVEN `DA_Board_Classic40` 索引 0 的 Go 格，WHEN 读 `FBoardTileData`，THEN `SalaryAmount > 0` 且 `SpecialAction==CollectSalary`。
- [ ] **Harness 就位** `[Config]` 资产校验 harness（commandlet/编辑器，非 -nullrhi）可执行上述 AC，且 cook 前 commandlet 能读取 `bIsPlaceholderData` 实现 Alpha gate。

## Implementation Notes

（逐字取自 ADR-0002 §Migration Plan / §Implementation Guidelines + GDD CR-6 temp-fill，语义不改写）

1. temp-fill `DA_Board_Classic40` 经典公开参考值（地价/租金/造价/抵押/税额/薪水），`bIsPlaceholderData=true`。这些值显式标注为占位、归属经济(5)、待重新平衡——它们是测试 fixture，非平衡决策。验证：`[Config]` AC-3/4-asset/4b/5/6/7/7b 跑绿（nightly）。
2. 布局按 CR-6 索引→类型映射表（与标准大富翁盘一致）：0=Go、10=Jail、20=FreeParking、30=GoToJail 四角；Railroad 在 5/15/25/35；Utility 在 12/28；色组沿边顺时针由低价向高价递增。测试 oracle（AC-7）：Property 的 ColorGroup 期望值由 CR-6 中文色标 + CR-5.1 枚举映射唯一确定（棕→Brown、浅蓝→LightBlue、粉→Magenta、橙→Orange、红→Red、黄→Yellow、绿→Green、深蓝→DarkBlue）。
3. `bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离，gate-check 在 cook 前用 commandlet 读取资产顶层字段检测；Primary DA 顶层字段可被 `UAssetManager` 直接枚举扫描。
4. 地图编辑器(26)（R4）：基于 Details 面板或自定义编辑器 UI 产出新 `UBoardDataAsset` 实例；`RentTable` 多值通过 Details 面板 `TArray` 元素 UI 输入（无 CSV `TArray` 列限制问题）——本 story 验证经典盘可经此方式填入多值（TR-board-015 schema 可输入多值的对端证明）。
5. `[Config]` 测试需编辑器/commandlet 环境，不能 `-nullrhi` headless 跑；AC-4c 标 `gate_level: alpha_blocking`（传递给 devops-engineer：MVP/nightly 允许 `true`，Alpha 门控强制 `false`）。
6. AC-4-asset 与 AC-4（story-001 [Logic]）区分：前者依赖资产 temp-fill 状态（nightly），后者纯 fixture（PR gate）——使失败原因可辨（资产未填 vs 逻辑 bug）。
7. **playtest 有效性限制**（CR-6）：任何基于 `bIsPlaceholderData==true` 数据跑出的 playtest，其经济平衡/局长节奏/支柱③结论不得作为设计有效性证据（占位值非目标平衡值）；早期 playtest 只验证结构/流程跑通。

## Out of Scope

- `UBoardDataAsset`/`FBoardTileData` 类型定义 → story-001。
- 通用 fixture 校验逻辑（拒绝/警告，非资产层）→ story-005 / story-006。
- 运行时加载/状态机 → story-002。
- `GetTilesInGroup` 实现（本 story 调用它做 AC-6 资产校验）→ story-004。
- 经济(5) 真实数值推导 + 置 `bIsPlaceholderData=false`（OQ-1 关闭条件）→ 经济 epic。

## QA Test Cases（[Config] Setup/Verify/Pass）

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。⚠ 完整 harness 依赖 BD-004/006（Sprint1），见 plan「跨 Sprint 依赖缺口」。

- **AC-3**：Setup：加载 `DA_Board_Classic40`。Verify：统计 40 格各 `TileType` 出现次数。Pass：精确计数 Go=1/Jail=1/FreeParking=1/GoToJail=1/Tax=2/Chance=3/CommunityChest=3/Railroad=4/Utility=2/Property=22，无非法整数枚举。
- **AC-4-asset**：Setup：同上。Verify：遍历 Property 格。Pass：每格 `PurchasePrice>0 && BuildingCost>0`。
- **AC-4b**：Verify：读 Utility 格。Pass：`PurchasePrice>0 && DiceMultiplierTable.Num()==2 && RentTable.Num()==0`。
- **AC-4c**：Setup：cook 前 commandlet 读顶层 `bIsPlaceholderData`。Verify：Alpha 构建上下文。Pass：Alpha 阶段 `==false`（否则 gate 失败）；nightly/MVP 阶段允许 `true`。
- **AC-5**：Verify：按类型查数组长度。Pass：Property RentTable==6 / Railroad==4 / Utility DiceMultiplierTable==2 且 RentTable==0 / 非购买格 RentTable==0。
- **AC-6**：Verify：8 色组 `GetTilesInGroup` 计数。Pass：棕2/浅蓝3/粉3/橙3/红3/黄3/绿3/深蓝2，合计 22。
- **AC-7**：Verify：逐格比对 `(TileType, ColorGroup)` 与 CR-6 oracle。Pass：全 40 格匹配 + 统计校验 40。
- **AC-7b**：Verify：读索引 0 Go 格。Pass：`SalaryAmount>0 && SpecialAction==CollectSalary`。

## Test Evidence

- **Path**: `tests/asset-validation/board-data/classic40_asset_validation_test.cpp`（[Config]，commandlet/编辑器环境，**非 -nullrhi**；nightly/staging gate，非 PR merge gate）
- **Status**: 未创建
- 资产 temp-fill 截图 + cook 门控 commandlet 输出 → `production/qa/evidence/`

## Dependencies

- **Depends on**: story-001（DONE，`UBoardDataAsset` 类型）、story-004（DONE，`GetTilesInGroup` 用于 AC-6）、story-005/006（DONE，校验通过的盘才作资产基线）。
- **Unlocks**: Alpha 里程碑门控（AC-4c）；下游系统集成（真实经典盘可跑通一局）；经济(5) 数值覆盖入口（OQ-1）。
