---
Epic: board-data
Status: Complete
Layer: Foundation
Type: Logic
Estimate: L
Manifest Version: 2026-06-06
Last Updated: 2026-06-07
---

# Story 005 — 加载期完整性校验（拒绝类）+ 结构化错误码

## Context

- **GDD**: `design/gdd/board-data.md` — Edge Cases（加载期完整性校验拒绝类）、States and Transitions（`Validated`→`LoadFailed`）、AC-18~23j / AC-32
- **Requirement TR-IDs**: TR-board-013
- **ADR Governing**:
  - **ADR-0002（primary）— 棋盘数据容器**：加载后跑 GDD §Edge Cases 全部校验（`Validated`），失败返回结构化错误码（`BoardTooSmall`/`Index0NotGo`/…）并 `LoadFailed`，不进 Active。
  - **ADR-0001 — UObject 宿主与生命周期**：校验由持有者 `Initialize`/`OnWorldBeginPlay` 在加载后一次性触发（状态机驱动见 story-002）。
- **Engine**: Unreal Engine 5.7 — Risk: LOW（纯逻辑校验，fixture struct 构造，无 post-cutoff API）
- **Engine Notes**: [Logic] 校验用代码构造 fixture struct，`-nullrhi` headless 跑、无文件 I/O、与载体形态解耦（ADR-0002 R6；本 GDD 测试分层 [Logic] 可作 PR merge gate BLOCKING）。错误须返回结构化枚举/struct（如 `DiceMultiplierOutOfRange{TileIndex}`），非自由文本 log，以便测试断言 pass/fail。`DICE_MULT_MAX` 须保证 `12 × DICE_MULT_MAX ≤ INT32_MAX`（防经济(5) Utility 租 `骰点 × mult` int32 溢出）。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：校验——加载后跑 GDD §Edge Cases 全部校验（`Validated`），失败返回结构化错误码（`BoardTooSmall`/`Index0NotGo`/…）并 `LoadFailed`，不进 Active（source: ADR-0002）。
  - **Required（ADR-0007）**：是 `[Logic] BLOCKING` AC 的被测逻辑 → C++ 独立 `UObject`/纯函数，`-nullrhi` 可实例化（source: ADR-0007）。
  - **Guardrail**：警告类校验须返回结构化警告（枚举/struct），非自由文本 log（本 story 只含拒绝类；警告类见 story-006）（source: GDD 测试分层）。

## Acceptance Criteria

- [ ] **AC-18（拒绝 N<4）** [Logic] GIVEN N=3 棋盘，WHEN 校验，THEN 失败返回 `BoardTooSmall`，对局不得启动。
- [ ] **AC-19a（拒绝 索引缺号）** [Logic] GIVEN 索引序列 `[0,1,3]`（缺 2），WHEN 校验，THEN 失败 `MissingTileIndex{2}`。
- [ ] **AC-19b（拒绝 索引重号）** [Logic] GIVEN 索引序列 `[0,1,1]`（重号），WHEN 校验，THEN 失败 `DuplicateTileIndex{1}`。
- [ ] **AC-20（拒绝 idx0 非 Go）** [Logic] GIVEN 索引 0 `TileType!=Go`，WHEN 校验，THEN 失败 `Index0NotGo`。
- [ ] **AC-21（拒绝 Property 无色组）** [Logic] GIVEN Property 格 `ColorGroup==None`，WHEN 校验，THEN 失败并指明 TileIndex。
- [ ] **AC-22（拒绝 Property RentTable 长度不符）** [Logic] GIVEN Property 格 `RentTable.Num()==4`，WHEN 校验，THEN 失败并指明期望长度 6、实际 4 及 `TileIndex`。
- [ ] **AC-22b（拒绝 Railroad RentTable 长度不符）** [Logic] GIVEN Railroad 格 `RentTable.Num()==6`，WHEN 校验，THEN 失败并指明期望长度 4、实际 6 及 `TileIndex`。
- [ ] **AC-23（拒绝 可购买格价格≤0）** [Logic] GIVEN Property/Railroad/Utility 各一 fixture `PurchasePrice<=0`，WHEN 校验，THEN 失败并指明 `TileIndex`（三类型均覆盖）。
- [ ] **AC-23b（拒绝 EventDeck 缺失）** [Logic] GIVEN Chance/CommunityChest 格 `EventDeck==None`，WHEN 校验，THEN 失败 `EventDeckMissing{TileIndex}`。
- [ ] **AC-23c（拒绝 GoToJail 无 Jail）** [Logic] GIVEN 含 `GoToJail` 格但全盘无 `Jail` 格，WHEN 校验，THEN 失败 `NoJailTile`。
- [ ] **AC-23d（拒绝 Go 数 ≠ 1）** [Logic] GIVEN 0 个或 ≥2 个 `Go` 格，WHEN 校验，THEN 失败 `GoTileCountInvalid`。
- [ ] **AC-23e（拒绝 MortgageValue>PurchasePrice）** [Logic] GIVEN 可购买格 `MortgageValue > PurchasePrice`，WHEN 校验，THEN 失败。
- [ ] **AC-23f（拒绝 Utility 倍率长度）** [Logic] GIVEN Utility 格 `DiceMultiplierTable.Num()!=2`，WHEN 校验，THEN 失败并指明期望长度 2。
- [ ] **AC-23g-a（拒绝 Property/Railroad 带倍率表）** [Logic] GIVEN Property 或 Railroad 格 `DiceMultiplierTable.Num()>0`，WHEN 校验，THEN 失败 `DiceMultiplierTableNotAllowed{TileIndex}`。
- [ ] **AC-23g-b（拒绝 Utility 带租金表）** [Logic] GIVEN Utility 格 `RentTable.Num()>0`，WHEN 校验，THEN 失败 `RentTableNotAllowed{TileIndex}`。
- [ ] **AC-23g-c（拒绝 非可购买格带数组）** [Logic] GIVEN 非可购买格（Tax/Chance/Go）`RentTable.Num()>0` 或 `DiceMultiplierTable.Num()>0`，WHEN 校验，THEN 失败 `RentArrayOnNonPurchasable{TileIndex}`。
- [ ] **AC-23h（拒绝 非 Go 格带薪额）** [Logic] GIVEN 非 `Go` 格 `SalaryAmount != 0`，WHEN 校验，THEN 失败 `SalaryOnNonGoTile{TileIndex}`。
- [ ] **AC-23i（拒绝 非 Tax 格带税额）** [Logic] GIVEN 非 `Tax` 格 `TaxAmount != 0`，WHEN 校验，THEN 失败 `TaxOnNonTaxTile{TileIndex}`。
- [ ] **AC-23j（拒绝 Utility 倍率越界）** [Logic] GIVEN Utility 格 `DiceMultiplierTable` 含元素 `<= 0`（如 `[0,10]`）或 `> DICE_MULT_MAX`（如 `[4, 200000000]`），WHEN 校验，THEN 失败 `DiceMultiplierOutOfRange{TileIndex}` 并指明越界元素与合法区间 `[1, DICE_MULT_MAX]`。
- [ ] **AC-32（非地产格无色组反向校验）** [Logic] GIVEN 含「非 `Property` 格带 `ColorGroup!=None`」的 fixture，WHEN 校验，THEN 失败（拒绝规则）。

## Implementation Notes

（逐字取自 ADR-0002 §Implementation Guidelines + GDD §Edge Cases 拒绝类，语义不改写）

1. 加载后跑 GDD §Edge Cases 全部拒绝/警告校验（`Validated` 状态），失败返回结构化错误码（`BoardTooSmall`/`Index0NotGo`/…）并 `LoadFailed`，不进入 Active。错误展示与重试由调用方负责；本系统不进入半加载态。
2. **结构正确性边界**：本系统加载期校验只保证结构正确性（数据类型、字段约束、枚举合法性、长度匹配），不保证体验可玩性（色组节奏、经济平衡等属地图编辑器(26)体验层设计债）。
3. **拒绝类清单**（逐条对应 AC）：N<4 → `BoardTooSmall`；索引缺/重号 → `MissingTileIndex`/`DuplicateTileIndex`；idx0 非 Go → `Index0NotGo`；Property 无色组 / 非 Property 带色组 → 反向校验拒绝；RentTable 长度不符（Property≠6/Railroad≠4）；可购买格 `PurchasePrice<=0`（三类型）；EventDeck 缺失；GoToJail 无 Jail → `NoJailTile`；Go 数≠1 → `GoTileCountInvalid`；`MortgageValue>PurchasePrice`；Utility 倍率长度≠2 / 元素 `<=0` 或 `> DICE_MULT_MAX` → `DiceMultiplierOutOfRange`；Property/Railroad 带 DiceMultiplierTable 或 Utility 带 RentTable → 反向校验；非可购买格带任一数组 → `RentArrayOnNonPurchasable`；非 Go 带 SalaryAmount → `SalaryOnNonGoTile`；非 Tax 带 TaxAmount → `TaxOnNonTaxTile`。
4. `DICE_MULT_MAX`（AC-23j）：边界 `12 × DICE_MULT_MAX ≤ INT32_MAX` 保证经济(5) Utility 租 `骰点(≤12) × mult`（F-4）不溢出 int32；最终数值与 `SalaryAmount` 上界（经济 OQ-Econ-10）并轨裁定，初值建议 `1_000_000`（留 ~178× 裕度）。
5. 错误码须结构化（枚举/struct，含 `TileIndex` 等定位字段），非自由文本 log，以便测试断言 pass/fail。

## Out of Scope

- 警告类校验（加载成功 + 结构化警告：RentTable 单调性、色组成员数异常、抵押率偏高、角格数异常、BuildingCost 同组不一致、空名回退）→ story-006。
- 校验的触发挂载点与 `Loaded→Validated→Active`/`LoadFailed` 状态机推进 → story-002（本 story 提供纯校验函数 + 错误码枚举）。
- `DA_Board_Classic40` 资产实例的 [Config] 测试 → story-007。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

每 AC 一条 Given/When/Then（fixture struct 构造，无文件 I/O）：

- **AC-18**：GIVEN 3 格盘 WHEN Validate THEN `LoadFailed` + `BoardTooSmall`。
- **AC-19a/b**：GIVEN `[0,1,3]` / `[0,1,1]` WHEN Validate THEN `MissingTileIndex{2}` / `DuplicateTileIndex{1}`。
- **AC-20**：GIVEN Tiles[0].TileType=Property WHEN Validate THEN `Index0NotGo`。
- **AC-21 / AC-32**：GIVEN Property `ColorGroup==None` / 非 Property 带 `ColorGroup!=None` WHEN Validate THEN 拒绝 + 指明 TileIndex。
- **AC-22 / AC-22b**：GIVEN Property `RentTable.Num()==4` / Railroad `RentTable.Num()==6` WHEN Validate THEN 拒绝 + 期望/实际长度 + TileIndex。
- **AC-23**：三 fixture（Property/Railroad/Utility `PurchasePrice=0` 或负）WHEN Validate THEN 各拒绝 + TileIndex。Edge：三类型必须各覆盖，不止 Railroad。
- **AC-23b/c/d**：EventDeck=None on Chance / GoToJail 无 Jail / Go 数 0 或 2 → `EventDeckMissing`/`NoJailTile`/`GoTileCountInvalid`。
- **AC-23e**：`MortgageValue=PurchasePrice+1` → 拒绝。Edge：`MortgageValue==PurchasePrice` 是**合法边界**（**不拒绝**）——GDD §Edge Cases 第 272 行明文「`MortgageValue ≤ PurchasePrice` 是硬数据约束」，仅 `>` 触发拒绝（code-review BD-005 doc-sync：原 edge 措辞「`==` 由 AC-23e 拒绝」与 GDD + AC-23e 正文「`>`」相反，系孤立 stale 误述，已更正为 GDD 权威口径）。`MortgageValue > PurchasePrice×0.6` 但 `≤ PurchasePrice` 走警告 `MortgageRateHigh`（story-006，不在本 story）。
- **AC-23f/g-a/g-b/g-c**：Utility 倍率长度≠2 / Property 带倍率 / Utility 带租金 / Tax 带数组 → 各对应错误码。
- **AC-23h/i**：Property `SalaryAmount=200` / Property `TaxAmount=100`（非 Tax）→ `SalaryOnNonGoTile`/`TaxOnNonTaxTile`。
- **AC-23j**：`[0,10]` / `[4,200000000]` → `DiceMultiplierOutOfRange{TileIndex}` + 区间 `[1, DICE_MULT_MAX]`。Edge：`[1, DICE_MULT_MAX]` 合法不拒绝。
- 全部确定性，无随机/时间依赖。

## Test Evidence

- **Path**: `tests/unit/board/board_validation_rejections_test.cpp`（[Logic] fixture，`-nullrhi` headless，BLOCKING PR gate）；物理实现 `Source/rentoTests/Private/BoardValidationRejectionsTest.cpp`（项目既定 UE 惯例，循 bd-001~004）
- **Status**: 已创建并通过（32 测试函数，独立重跑全量 Rento. 117/117 Success, 0 Fail, EXIT 0，证据 `Saved/TestReport_bd005_r2/index.json`）

## Dependencies

- **Depends on**: story-001（DONE，`FBoardTileData`/枚举）。可与 story-002/003/004 并行（纯逻辑）。
- **Unlocks**: story-002（状态机据校验返回推进 `Validated`/`LoadFailed`）；story-006（警告类共用校验框架）；下游系统安全消费（无效盘被拒绝）。

## Completion Notes
**Completed**: 2026-06-07
**Criteria**: 21/21 [Logic] COVERED（32 测试函数；无 deferred——纯逻辑，无真磁盘/ff-007 类延迟项）
**Deviations**:
- 🟡 **doc-sync（logged decision）**：本节 QA Test Cases 第 79 行 AC-23e edge note 原写「`MortgageValue==PurchasePrice` 由 AC-23e 拒绝」与 GDD §Edge Cases 第 272 行「`MortgageValue ≤ PurchasePrice` 是硬数据约束」+ AC-23e 正文「`>`」相反，系孤立 stale 误述 → code-review 阶段已更正为 GDD 权威口径（`==` 是合法边界，仅 `>` 拒绝）。代码实现 `>` 自始正确。补 `TC_AC23e_MortgageEqualsPurchase_Valid` 正路径锁边界。
- **实现顺序决定（非 GDD/ADR 偏离）**：板级校验顺序中 `GoTileCountInvalid` 前移至 `Index0NotGo` 之前。理由：fail-fast 下若 Index0NotGo 先，AC-23d「0 个 Go」永远被抢先不可达 GoTileCountInvalid。新序职责切分清晰（计数错误 0/≥2→GoTileCountInvalid；计数恰 1 但 Go 不在 index0→Index0NotGo），两 AC（20/23d）均可达。AC-23d 正文「0 个或 ≥2 个」本就涵盖，无需改 AC 文本。`TC_AC23d_ZeroGo` + `TC_AC20_Index0NotGo` 双测守此顺序契约。
- **DICE_MULT_MAX=1_000_000**：取 GDD 初值（留 ~178× int32 溢出裕度）；最终值并入经济 OQ-Econ-10 裁定（既有 OQ，非本 story 新债）。编译期 `static_assert(12×MAX ≤ MAX_int32)` 守不变式。
**Test Evidence**: Logic — `Source/rentoTests/Private/BoardValidationRejectionsTest.cpp`（32 测试）；独立重跑 117/117 Success, 0 Fail（`Saved/TestReport_bd005_r2/index.json`）
**Code Review**: Complete（本会话 /code-review = APPROVED；首轮 CHANGES REQUIRED → reorder + 6 测试补强全闭）
**Scope**: CLEAN — 未触碰 BoardDataAssetHost/DataAssetHostSubsystem/EHostLoadState（状态机推进 = story-002 域，本 story 只交付纯校验函数 + 错误码枚举）
