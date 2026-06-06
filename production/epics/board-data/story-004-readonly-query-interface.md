---
Epic: board-data
Status: Ready
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 004 — 只读查询接口集：GetTileCount / GetTileData / GetTilesInGroup / GetBoardId

## Context

- **GDD**: `design/gdd/board-data.md` — Interactions（只读查询接口 + 接口语义补充）、Edge Cases（运行时位移边界 GetTilesInGroup/GetTileData 越界）、AC-26/27/28/30
- **Requirement TR-IDs**: TR-board-005、TR-board-009、TR-board-010、TR-board-017
- **ADR Governing**:
  - **ADR-0002（primary）— 棋盘数据容器**：`GetTileData`/`GetTilesInGroup`/`GetBoardId` 由持有者暴露为只读查询；`GetTilesInGroup` 须显式 `Sort`（AC-30）；`GetBoardId` 取顶层 `BoardIdentifier` 字段、与路径解耦（AC-28）；运行时只读访问 O(1)；接口隔离（下游不硬引用 DA，可逆迁移）。
  - **ADR-0001 — UObject 宿主与生命周期**：查询接口挂持有者宿主（World Subsystem）；宿主不持状态写语义、运行时只读。
- **Engine**: Unreal Engine 5.7 — Risk: LOW（数组访问/`FName`/`TArray::Sort` 均稳定 API）
- **Engine Notes**: `GetTileData(index)` 为 `Tiles[index]` O(1) 数组访问（ADR-0002 Performance）。`FName` 比较 case-insensitive（`"Classic40"==`"classic40"`）——存档(21)经 `FName` 比较安全；若降级为 `FString` 比较须先 `ToLower()` 归一化（AC-28 R3 注，unreal-specialist）。无 post-cutoff API。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：接口隔离——所有下游经持有者的 `GetTileData`/`GetTilesInGroup` 访问，不得各自硬引用 `UBoardDataAsset`（迁移载体时下游接口不变）（source: ADR-0002）。
  - **Required**：`BoardIdentifier: FName` 作顶层 `UPROPERTY` 作者手填，与 `PrimaryAssetId`/路径解耦（AC-28）（source: ADR-0002）。
  - **Required**：运行时只读访问 `GetTileData(index)` 须 O(1) 或近 O(1)（source: ADR-0002）。
  - **Required（ADR-0007）**：是 `[Logic] BLOCKING` AC 的被测逻辑 → C++ 独立 `UObject`/纯函数，`-nullrhi` 可实例化、暴露可 spy getter（source: ADR-0007）。
  - **Forbidden**：Never 让下游各自硬引用 `UBoardDataAsset`——破坏接口隔离/可逆性（source: ADR-0002）。
  - **Guardrail**：`GetTileData` ~0ms（O(1)）（source: ADR-0002）。

## Acceptance Criteria

- [ ] **AC-1（CR-1 GetTileCount）** [Logic] GIVEN 已校验经典盘（N=40），WHEN `GetTileCount()`，THEN 返回 40 且索引 0 的 `TileType==Go`。
- [ ] **AC-26（GetTilesInGroup(None)）** [Logic] WHEN `GetTilesInGroup(None)`，THEN 返回空数组，不报错。
- [ ] **AC-27（越界访问防护）** [Logic] N=40，WHEN `GetTileData(40)` 或 `GetTileData(-1)`，THEN 触发 `ensure(false)`（开发期报错+继续，非 `check()` 硬崩溃）+ 记 error 日志，返回默认空 `FBoardTileData`，不静默返回脏数据。
- [ ] **AC-28（GetBoardId 类型与同进程稳定性）** [Logic] GIVEN `DA_Board_Classic40`，WHEN 同进程内多次调 `GetBoardId()`，THEN 返回非空 `FName`，等于 DA 内显式 `BoardIdentifier` 字段（如 `"Classic40"`），与资产路径解耦，且每次结果完全一致。
- [ ] **AC-30（GetTilesInGroup 升序契约）** [Logic] GIVEN fixture 棋盘某色组（如 LightBlue）成员索引以非升序录入 `[9,6,8]`，WHEN `GetTilesInGroup(LightBlue)`，THEN 返回 `[6,8,9]`（严格 `TileIndex` 升序；验证显式 `Sort` 而非依赖录入顺序）。

## Implementation Notes

（逐字取自 ADR-0002 §Key Interfaces / §Implementation Guidelines + GDD §Interactions 接口语义补充，语义不改写）

1. `GetTileData`/`GetTilesInGroup`/`AdvanceIndex` 由持有者（或其委托的 `UBoardMathLibrary`）暴露为只读查询接口。
2. `GetTilesInGroup(group)` 按 `TileIndex` 升序返回成员索引；实现必须对收集结果显式 `Sort`，不得依赖 DataTable/数组录入顺序（否则升序不成立，AC-30 失败）。`GetTilesInGroup(None)` 返回空数组（非地产格无组），不报错。
3. `GetBoardId()` 返回 DA 内显式 `BoardIdentifier: FName` 字段（如 `"Classic40"`），由内容作者手写、与资产路径/文件名/`PrimaryAssetId` 解耦。**禁止**从资产路径或 `PrimaryAssetId` 派生（资产改名/移动会静默损坏所有旧存档）。
4. `GetTileData(index)` 为 `Tiles[index]` O(1) 数组访问；index 越界返回校验失败/`ensure(false)`（报错+继续）并记 error 日志，返回默认空 `FBoardTileData`，调用方应先用 `GetTileCount()` 约束范围（AC-27，与 AC-34 防护语言一致）。
5. 接口隔离（R7 可逆性）：所有下游经持有者访问，不得各自硬引用 `UBoardDataAsset`——若未来迁移载体，仅改持有者内部取值，下游接口不变。
6. `FName` case-insensitive 陷阱：存档(21)经 `FName` 比较安全；任何降级为 `FString` 比较须先 `ToLower()` 归一化（AC-28 R3 注）。

## Out of Scope

- `AdvanceIndex`/`StepsBetween` 拓扑算法实现 → story-003（本 story 仅供 N via `GetTileCount`）。
- DA 加载/状态机推进 → story-002。
- 加载期校验（决定何时进 Active 才允许查询） → story-005/006。
- `GetBoardId` 跨会话/序列化往返稳定性（AC-28b）→ 存档(21) Integration（见 story-008 登记）。
- `GetTileWorldTransform`（VFX 锚点，软依赖呈现层）非 MVP 本 epic 核心 AC；如需占位接口随本 story 但无 BLOCKING AC。

## QA Test Cases

| AC | Given | When | Then | Edge |
|---|---|---|---|---|
| AC-1 | 已校验 N=40 经典盘 | GetTileCount() | 40 且 Tile[0].TileType==Go | — |
| AC-26 | 任意盘 | GetTilesInGroup(None) | 空数组、不报错 | 非地产格调用 |
| AC-27 | N=40 | GetTileData(40) / GetTileData(-1) | ensure(false) 报警+继续 + error 日志 + 返回默认空 struct | 上界/下界各一 |
| AC-28 | DA_Board_Classic40 | 同进程多次 GetBoardId() | 非空 FName == "Classic40"，与路径解耦，多次完全一致 | 资产改名后取值不变（手动验证子句） |
| AC-30 | LightBlue 成员录入 `[9,6,8]` | GetTilesInGroup(LightBlue) | `[6,8,9]` 严格升序 | 含中间值，验证 Sort 非数组反转 |

- 测试确定性：fixture struct 构造，无文件 I/O、无随机。

## Test Evidence

- **Path**: `tests/unit/board/board_query_interface_test.cpp`（[Logic] fixture，`-nullrhi` headless，可作 BLOCKING PR gate）
- **Status**: 未创建
- AC-28 资产改名取值不变（手动子句）→ `production/qa/evidence/`（[Advisory]）

## Dependencies

- **Depends on**: story-001（DONE，`FBoardTileData`/`UBoardDataAsset`）、story-002（DONE，持有者宿主暴露查询）。可与 story-003 并行（math library 解耦）。
- **Unlocks**: 移动(4)/所有权(6)/事件格(7)/建房(8)/HUD(16)/地产卡(17) 读字段；story-008（`GetBoardId` 存档引用）。
