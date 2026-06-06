---
Epic: board-data
Status: Ready
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 002 — Board DA 持有者宿主：加载 / 生命周期 / 热切换边界

## Context

- **GDD**: `design/gdd/board-data.md` — States and Transitions（资产生命周期 + 热切换边界）、Interactions（运行时 DA 持有者）
- **Requirement TR-IDs**: TR-board-007、TR-board-008
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期**：per-match 服务挂 `UWorldSubsystem`（一局=World 边界、PIE 隔离）；`Initialize` 发起异步加载、`Deinitialize` 显式 `CancelHandle`/`ReleaseHandle` 防 PIE Stop 空棋盘；DA 底层资产由 `UAssetManager` 跨局缓存，棋盘服务实例挂 World 级（OQ-6 reconcile）。
  - **ADR-0002 — 棋盘数据容器**：宿主以 `UPROPERTY UBoardDataAsset*` 强引用持有防 GC；`UAssetManager` 同步加载（`GetPrimaryAssetObject` 或 `LoadPrimaryAsset` + 等待）满足 R5（<100ms）；换盘走完整 `Loaded→Validated→Active`、不在 Active 对局热替换。
- **Engine**: Unreal Engine 5.7 — Risk: MEDIUM（`UAssetManager`/`FStreamableManager` 在 5.4+ 的加载 API 签名 post-cutoff）
- **Engine Notes**: ADR-0001 Post-Cutoff APIs = **None**（Subsystem 框架 4.22+ 稳定，LOW）。ADR-0002 **Verification Required ②**：`UAssetManager` 同步/异步加载 Primary DA 的 5.7 API 签名须实测（知识缺口 (c)：`FStreamableManager::RequestAsyncLoad`/`UAssetManager` 在 5.4+ 的 API 变化）。**Sprint0 引擎验证 #6/#7**（control-manifest 末表）：`UAssetManager` 加载签名；`UWorldSubsystem::Initialize/Deinitialize` 随 PIE World 正确触发、`OnWorldBeginPlay` 在 PIE Stop→Start 重触发、`Deinitialize` `CancelHandle` 后无悬挂回调。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：per-match 服务一律继承 `UWorldSubsystem`；`ShouldCreateSubsystem` 排除 editor-preview World，避免编辑器误建对局态（source: ADR-0001）。
  - **Required**：per-match 服务（含棋盘 DA 持有者）挂 `UWorldSubsystem`，生命周期边界=一局，PIE Stop 即销毁、Start 即重建（source: ADR-0001）。
  - **Required**：异步加载纪律——`Initialize` 中 `FStreamableManager::RequestAsyncLoad` 取得的 `TSharedPtr<FStreamableHandle>` 须存为成员，`Deinitialize` 显式 `CancelHandle()`（source: ADR-0001）。
  - **Required**：运行时由宿主以 `UPROPERTY UBoardDataAsset*` 强引用持有，防 GC（source: ADR-0002）。
  - **Required**：加载方式——单棋盘 ~数 KB，用 `UAssetManager` 同步加载（`GetPrimaryAssetObject` 或 `LoadPrimaryAsset` + 等待）；异步仅未来多棋盘预加载场景（source: ADR-0002）。
  - **Required**：宿主不持状态写语义——World Subsystem 持服务实例与生命周期，写语义归各 owner（source: ADR-0001）。
  - **Forbidden**：Never 把 per-match 对局态全挂 `UGameInstanceSubsystem`（PIE 隔离破裂）（source: ADR-0001）。Never 让 `Deinitialize` 后残留悬挂异步加载回调写已 GC 对象（PIE Stop 写空棋盘）（source: ADR-0001）。Never 在 Active 对局中热替换 DA——须走完整 `Loaded→Validated→Active` 重初始化（source: ADR-0002）。Never 在 MVP 用 `UActorComponent`/`AGameState` 上的 Component 形态承载对局服务（source: ADR-0001）。
  - **Guardrail**：Board DA Load Time < 100ms（对局初始化一次性）；`GetTileData` ~0ms（O(1)）；Memory < 0.1MB/棋盘（source: ADR-0002）。

## Acceptance Criteria

- [ ] **AC-L1（宿主 + 强引用持有）** 持有者为 `UWorldSubsystem` 子类，以 `UPROPERTY() TObjectPtr<UBoardDataAsset> LoadedBoard` 强引用持有当前局棋盘；`ShouldCreateSubsystem` 排除 editor-preview World。
- [ ] **AC-L2（加载 + 状态推进）** `Initialize`/`OnWorldBeginPlay` 经 `UAssetManager` 加载选定棋盘 DA，状态机推进 `Loaded → Validated → Active`；校验失败（见 story-005）进 `LoadFailed`、不进 Active、返回结构化错误码给调用方。
- [ ] **AC-L3（PIE 隔离 / handle 取消）** PIE Play→Stop→Play 三轮，第二局正确重新加载棋盘，无第一局残留、无悬挂异步加载回调写已 GC 对象（`Deinitialize` 显式 `CancelHandle`/`ReleaseHandle`）。
- [ ] **AC-L4（热切换边界）** 换盘不在 `Active` 对局中热替换 DA：地图编辑器(26)的预览/换盘在独立预览模式经完整 `Loaded→Validated→Active` 重新初始化（code-review 守门：无 public 在 Active 态替换 `LoadedBoard` 的接口）。
- [ ] **AC-L5（载入预算）** 单棋盘 DA 加载 + 校验在对局初始化一次性完成，载入时间 < 100ms。

## Implementation Notes

（逐字取自 ADR-0001 §Implementation Guidelines + ADR-0002 §Implementation Guidelines，语义不改写）

1. per-match 服务一律继承 `UWorldSubsystem`；`ShouldCreateSubsystem` 排除 editor-preview World，避免编辑器误建对局态。
2. 持有者取 ADR-0001 裁定结果：棋盘 DA 持有者挂 `UWorldSubsystem`（一局边界）；DA 底层资产由 `UAssetManager`/`FStreamableManager` 缓存、跨局复用不随局释放底层资产（OQ-6 reconcile：资产缓存在 GameInstance 级 AssetManager，棋盘服务实例在 World 级）。
3. 持有者以 `UPROPERTY UBoardDataAsset*` 强引用防 GC；`Initialize()` 内若用异步加载，handle 须在 `Deinitialize()` 显式 `CancelHandle`/`ReleaseHandle`（防 PIE Stop 空棋盘）——`Initialize` 中 `FStreamableManager::RequestAsyncLoad` 取得的 `TSharedPtr<FStreamableHandle>` 须存为成员。
4. 加载方式：单棋盘资产 ~数 KB，建议 `UAssetManager` 同步加载（`GetPrimaryAssetObject` 或 `LoadPrimaryAsset` + 等待）即可满足 R5（<100ms）；异步仅在未来多棋盘预加载场景考虑。具体 5.7 API 签名须实测（知识缺口 (c)）。
5. 校验：加载后跑 GDD §Edge Cases 全部拒绝/警告校验（`Validated` 状态），失败返回结构化错误码（`BoardTooSmall`/`Index0NotGo`/…）并 `LoadFailed`，不进入 Active（校验逻辑本体在 story-005/006，本 story 负责调用挂载点与状态机驱动）。
6. 热切换：换盘走完整 `Loaded→Validated→Active` 重初始化，不在 Active 对局中热替换 DA。
7. 宿主不持状态语义：World Subsystem 持服务实例与生命周期，但 `Cash`/`house_count`/owner map 写语义仍归各 owner；宿主裁定不改任何 owner 关系。

## Out of Scope

- `FBoardTileData`/`UBoardDataAsset` 类型定义 → story-001。
- 加载期校验规则（拒绝/警告枚举码） → story-005 / story-006（本 story 只调用并据返回推进状态机）。
- 只读查询接口 `GetTileData`/`GetTilesInGroup`/`GetBoardId` → story-004。
- RNG Seed 注入（`OnWorldBeginPlay`）属骰子 epic，非本 story。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

- **AC-L3（PIE 隔离）**：GIVEN PIE 反复 Start/Stop（含 Stop 发生在异步加载中途）WHEN 第二局 Start THEN 棋盘正确加载、无空棋盘崩溃、无悬挂回调写已 GC 对象。Edge：Stop 恰在 `RequestAsyncLoad` 回调前 → `CancelHandle` 生效。（headless `-nullrhi`：`UWorldSubsystem::Initialize/Deinitialize` 随 PIE World 触发；若 -nullrhi 下 PIE 行为不可观测，降级为 PIE 手动 + code-review，记 `production/qa/evidence/`。）
- **AC-L4（热切换）**：[Advisory·code-review] Verify 代码库无 public 接口在 `Active` 态替换 `LoadedBoard`；换盘唯一路径是重走 `Loaded→Validated→Active`。Pass condition：review sign-off + 记录于 `production/qa/evidence/`。
- **AC-L5（载入预算）**：GIVEN `DA_Board_Classic40` WHEN 对局初始化加载 THEN 加载+校验耗时 < 100ms（性能 Advisory，目标硬件实测）。

## Test Evidence

- **Path**: `tests/integration/board/da_holder_lifecycle_test.cpp`（[Integration]，PIE/World 生命周期；headless 部分用 `-nullrhi`）
- **Status**: 未创建
- 热切换 code-review + 载入预算性能 → `production/qa/evidence/`（[Advisory]）

## Dependencies

- **Depends on**: story-001（DONE）— 需 `UBoardDataAsset` 类型。
- **Unlocks**: story-004（查询接口由持有者暴露）；下游移动(4)/所有权(6)/事件格(7)/建房(8) 实现 story（经持有者获取 DA 引用）。
