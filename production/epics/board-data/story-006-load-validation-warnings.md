---
Epic: board-data
Status: Complete
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-07
---

# Story 006 — 加载期完整性校验（警告类）：加载成功 + 结构化警告

## Context

- **GDD**: `design/gdd/board-data.md` — Edge Cases（警告但不拒绝类）、AC-24a~24d / AC-35 / AC-36
- **Requirement TR-IDs**: TR-board-013（警告类分量，与 story-005 拒绝类共同覆盖 Validated 校验）
- **ADR Governing**:
  - **ADR-0002（primary）— 棋盘数据容器**：校验在 `Validated` 状态执行；警告类不阻止进入 Active（CD 裁定：单调性/成员数/抵押率/角格数等是平衡或自定义棋盘允许的属性）。
  - **ADR-0001 — UObject 宿主与生命周期**：校验由持有者加载后触发；警告不致 `LoadFailed`。
- **Engine**: Unreal Engine 5.7 — Risk: LOW（纯逻辑，fixture struct，无 post-cutoff API）
- **Engine Notes**: 警告类校验须返回结构化警告（枚举/struct，如 `RentTableNotMonotonic{TileIndex}`），非自由文本 log，以便测试断言 pass/fail（GDD 测试分层）。[Logic] `-nullrhi` headless，无文件 I/O，与载体形态解耦。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：警告类校验须返回结构化警告（枚举/struct），非自由文本 log（source: GDD 测试分层 / ADR-0002 校验契约）。
  - **Required（ADR-0007）**：是 `[Logic] BLOCKING` AC 的被测逻辑 → C++ 独立 `UObject`/纯函数，`-nullrhi` 可实例化（source: ADR-0007）。
  - **Guardrail**：警告类加载成功（不进 `LoadFailed`）——与拒绝类区分；测试须断言「加载成功 + 产生指定警告码」（source: GDD CD 裁定）。

## Acceptance Criteria

- [ ] **AC-24a（警告 空名回退）** [Logic] GIVEN 某格 `DisplayName` 为空，WHEN 校验，THEN 通过并产生结构化警告 `EmptyDisplayName{TileIndex}`，且 `GetTileData` 回退显示 `"Tile {index}"`。
- [ ] **AC-24b（警告 色组成员数异常）** [Logic] GIVEN 某色组成员数与声明不符，WHEN 校验，THEN 通过并产生结构化警告 `ColorGroupMemberCountMismatch{ColorGroup}`。
- [ ] **AC-24c（警告 抵押率偏高）** [Logic] GIVEN `PurchasePrice=100, MortgageValue=61`（即 `61 > 100×0.6=60`），WHEN 校验，THEN 通过并产生结构化警告 `MortgageRateHigh{TileIndex}`；边界对照：`MortgageValue=60`（=60% 下界）不触发警告；阈值用 `MortgageValue > floor(PurchasePrice×0.6)`（`>` 非 `≥`）。
- [ ] **AC-24d（警告 角格数量异常）** [Logic] GIVEN fixture 含 2 个 `FreeParking` 格，WHEN 校验，THEN 通过并产生结构化警告 `CornerTileCountUnusual{FreeParking, 2}`。
- [ ] **AC-35（RentTable 单调性警告）** [Logic] GIVEN Property 格 `RentTable` 存在 `RentTable[i] > RentTable[i+1]`，WHEN 校验，THEN 加载成功并产生结构化警告 `RentTableNotMonotonic{TileIndex}`。
- [ ] **AC-36（BuildingCost 同组不一致警告）** [Logic] GIVEN 同一色组（如 Red）三格 `BuildingCost` 为 `[150,150,200]`，WHEN 校验，THEN 加载成功并产生结构化警告 `BuildingCostMismatchInGroup{Red}`。

## Implementation Notes

（逐字取自 GDD §Edge Cases 警告类 + CD 裁定，语义不改写）

1. **警告但不拒绝**类（加载成功 + 结构化警告）：
   - `DisplayName` 为空：发出警告 `EmptyDisplayName{TileIndex}`；`GetTileData` 回退显示 `"Tile {index}"`，不阻止开局（便于编辑器草稿态）。
   - Property 格 `RentTable` 非单调不减（存在 `RentTable[i] > RentTable[i+1]`）：发出警告但不拒绝（CD 裁定：单调性是平衡属性，归经济(5)/playtest；严格递增 vs 非递减由经济(5)裁决）。警告码 `RentTableNotMonotonic{TileIndex}`。
   - `Jail`/`FreeParking`/`GoToJail` 任一类型格数 ≠ 1：发出警告但不拒绝（自定义棋盘可能有 0 或多个；经典四角盘应各 1）。警告码 `CornerTileCountUnusual{TileType, Count}`。注：`GoToJail` 存在但无 `Jail` 仍是硬拒绝（story-005 AC-23c，目标未定义）；此条覆盖「数量异常但不致命」（如 2 个 FreeParking）。
   - 同色组内各格 `BuildingCost` 不完全相同：发出警告 `BuildingCostMismatchInGroup{ColorGroup}`（经典盘同组造价应一致，由资产单测守住；自定义棋盘差异化需建房(8)显式处理）。
   - 某色组成员格数与声明不符：发出警告 `ColorGroupMemberCountMismatch{ColorGroup}`（自定义棋盘允许；经典盘由 story-007 AC-6/AC-7 资产测试守住）。
   - `MortgageValue > PurchasePrice × 0.6`（超 60% 抵押率，但 ≤ 100%）：发出警告 `MortgageRateHigh{TileIndex}`。阈值来源/归属：60% 是 board 数据质量保守警戒线（非套利防范——经济 AC-42 已数学证伪套利）；此阈值是经济(5)的配置旋钮（registry `mortgage_warning_threshold`，source 临时归棋盘待经济5接管），非改本系统硬编码。
2. 阈值实现（AC-24c）：`MortgageValue > floor(PurchasePrice×0.6)`（`>` 非 `≥`），int 截断边界以 `(100,61)→警告 / (100,60)→不警告` fixture 锁定。`MortgageValue==PurchasePrice` 由 story-005 AC-23e 拒绝、不适用本警告。
3. 警告码须结构化（枚举/struct，含定位字段），非自由文本 log。

## Out of Scope

- 拒绝类校验（致 `LoadFailed`）→ story-005。
- 经典盘 `DA_Board_Classic40` 的精确成员数/造价一致性（资产层）→ story-007（[Config]，由资产单测守住，本 story 只覆盖通用 fixture 警告逻辑）。
- 校验触发挂载点与状态机推进 → story-002。

## QA Test Cases

| AC | Given | When | Then | Edge |
|---|---|---|---|---|
| AC-24a | 某格 DisplayName 空 | Validate | 通过 + `EmptyDisplayName{TileIndex}` + GetTileData 回退 `"Tile {index}"` | 不阻止开局 |
| AC-24b | 色组成员数与声明不符 | Validate | 通过 + `ColorGroupMemberCountMismatch{ColorGroup}` | 自定义盘允许 |
| AC-24c | (PurchasePrice=100, MortgageValue=61) | Validate | 通过 + `MortgageRateHigh{TileIndex}` | (100,60) 不触发；(100,100) 由 AC-23e 拒绝不适用 |
| AC-24d | 2 个 FreeParking | Validate | 通过 + `CornerTileCountUnusual{FreeParking, 2}` | GoToJail 无 Jail 仍 AC-23c 硬拒绝 |
| AC-35 | Property RentTable 存在 [i]>[i+1] | Validate | 加载成功 + `RentTableNotMonotonic{TileIndex}` | 警告非拒绝 |
| AC-36 | Red 组 BuildingCost `[150,150,200]` | Validate | 加载成功 + `BuildingCostMismatchInGroup{Red}` | 警告非拒绝 |

- 每条须同时断言「加载成功（未进 LoadFailed）」+「产生指定结构化警告码」。确定性，无随机/时间依赖。

## Test Evidence

- **Path**: `tests/unit/board/board_validation_warnings_test.cpp`（[Logic] fixture，`-nullrhi` headless，BLOCKING PR gate）；物理实现 `Source/rentoTests/Private/BoardValidationWarningsTest.cpp`（项目既定 UE 惯例，循 bd-001~005）
- **Status**: 已创建并通过（15 测试函数，独立重跑全量 Rento. 132/132 Success, 0 Fail, EXIT 0，证据 `Saved/TestReport_bd006_r2/index.json`）

## Dependencies

- **Depends on**: story-001（DONE，struct/枚举）、story-005（DONE 或并行，共用校验框架与 `Validated` 流程）。
- **Unlocks**: story-007（资产层精确计数引用警告码语义）；地图编辑器(26) 体验层设计债（警告码供其消费）。

## Completion Notes
**Completed**: 2026-06-07
**Criteria**: 6/6 [Logic] COVERED（AC-24a/24b/24c/24d/35/36；15 测试函数；无 deferred——纯逻辑无 ff-007 类延迟）
**Deviations**:
- 🟡 **AC-24a 显示回退（tech-debt 登记）**：AC-24a 第二从句「GetTileData 回退显示 "Tile {index}"」非 bd-006 校验器交付——bd-006 产 EmptyDisplayName 警告（已测）；回退「显示」是显示层行为，GetTileData 返 raw 数据，「Tile {index}」是玩家可见串须本地化归 HUD/显示层。登 docs/tech-debt-register.md（2026-06-07），不越权改已批 story-004。
- 🟡 **AC-24b 参数注入（logged decision）**：经典精确计数（棕2/浅蓝3/…）归 story-007 资产层（AC-6），不硬编码进 generic 校验器；「声明」=调用方注入 ExpectedGroupSizes TMap；空 map=跳过（自定义盘允许）。数据驱动。
- **W-1 int64 硬化（code-review）**：MortgageRateHigh 阈值 (price*6)/10 用 int64 中间量，消除自定义盘大值的理论 int32 溢出。
- **设计**：警告类全收集（非 fail-fast）返 TArray<FBoardValidationWarning>，警告不拒绝加载；状态机推进 = story-002 域（Out of Scope）。
**Test Evidence**: Logic — `Source/rentoTests/Private/BoardValidationWarningsTest.cpp`（15 测试，含多警告共存证全收集语义 + AC-24d Count=0/=2 双分支 + null 兜底 + Railroad 路径）；独立重跑 132/132 Success, 0 Fail（`Saved/TestReport_bd006_r2/index.json`）
**Code Review**: Complete（本会话 /code-review = APPROVED；首轮 CHANGES REQUIRED → int64硬化 + 4测试补强 + AC-24a tech-debt deflate 全闭）
**Scope**: CLEAN — 追加警告类符号到 BoardValidator.h/.cpp，**拒绝类 + ValidateTiles 一字未改**（diff + 117 旧测试无回归核实）；未触碰 Host/Subsystem/EHostLoadState/GetTileData
