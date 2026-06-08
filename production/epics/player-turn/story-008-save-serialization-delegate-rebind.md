---
Epic: player-turn
Status: Complete
Layer: Foundation
Type: Integration
Estimate: L
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 008 — 中途读档序列化 + delegate 重绑拓扑序 + 枚举 append-only + 可存档点查询

## Context

- **GDD**: `design/gdd/player-turn.md` — Edge Cases（读档中途还原精确阶段 L398-401、开局定序进行中禁存档、UE 实现地雷 Latent、读档恢复时序 delegate 重绑）、「枚举扩展 append-only」(L277)、AC-33（FSM resume 语义）、AC-34（完整存档 + 重绑监听器）、AC-43（append-only 枚举）
- **Requirement TR-ID**: TR-turn-012（中途读档序列化：精确阶段 CurrentPhase + 全 11 PlayerState 字段 + ConsecutiveDoubles + 锁定阈值 + 完整 FDiceRollResult）、TR-turn-013（读档后 delegate 重绑〔invocation list 不序列化〕+ 拓扑序重建：反序列化→重绑→才广播）、TR-turn-014（枚举 append-only：ETurnPhase/EPlayerColor/EAIDifficulty 整数索引不可重排）、TR-turn-017（可存档点查询接口〔瞬态/定序进行中拒存〕签名冻结）— **Partial**（可存档点签名待实现期细化，非 Gap）
- **ADR Governing**:
  - **ADR-0005（primary）— 存档序列化契约**：全字段标 `UPROPERTY(SaveGame)`；读档重建拓扑序 DA→(经济/地产/建房/事件格牌堆)→回合2→骰子 SetSeed→重绑→`switch(CurrentPhase)`；delegate 重绑——存档21 只广播 `OnGameLoaded`，呈现层各自先 Unbind 后 Bind；枚举 append-only（整数索引不可重排）；可存档点门 CR-4（查询回合2 是否合法可存档点为 GIVEN）。
  - **ADR-0001 — UObject 宿主与生命周期**：状态机 `ETurnPhase` 枚举字段 + delegate 推进，禁 Latent（读档 `switch(CurrentPhase)` 重入）；订阅生命周期锚宿主 `Initialize`/`Deinitialize`。
  - **ADR-0003 — 事件总线**：读档 `OnGameLoaded` 后先全量解绑呈现层既有订阅（防 EC-8 双订阅），再按权威订阅矩阵重绑。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（ADR-0005：`USaveGame`/`SaveGameToSlot`/`LoadGameFromSlot` 是 pre-5.3 稳定 API，post-cutoff 无破坏性变更）
- **Engine Notes（ADR-0005 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None** — 全部用 pre-5.3 稳定持久化 API。
  - Verification Required: ① `UPROPERTY(SaveGame)` 对嵌套 USTRUCT（`FPlayerState`/`FDiceRollResult`）与 `TArray`/`TMap` 递归序列化在 5.7 仍按预期工作（写 round-trip 冒烟）；② `FObjectAndNameAsStringProxyArchive` 对 `UPROPERTY(SaveGame)` 过滤行为不变。
  - **读档恢复时序陷阱（GDD L401）**：dynamic multicast delegate 绑定列表（invocation list）不序列化——只序列化 `ETurnPhase` 等数据字段，读档后由各下游重新 `AddDynamic`/`AddUniqueDynamic` 重建绑定。关键顺序：① 反序列化所有 PlayerState/阶段字段 → ② 各下游重绑 delegate → ③ 才允许 TurnManager 续行/广播任何事件。若 ② 之前 TurnManager 在 `PostLoad` 即按 `CurrentPhase` 广播 `OnPhaseChanged`，监听者尚未重绑→该次广播丢失。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required**: 全字段标 `UPROPERTY(SaveGame)`；`SaveGameToSlot` 经 `FObjectAndNameAsStringProxyArchive` 自动过滤 SaveGame 标记属性（source: ADR-0005）。读档重建拓扑序：DA→(经济/地产/建房/事件格牌堆 互不依赖)→回合2→骰子 SetSeed→重绑→`switch(CurrentPhase)`；禁重建 A 时读尚未重建的 B（source: ADR-0005）。delegate 重绑——存档21 只广播 `OnGameLoaded`，呈现层监听后各自先 Unbind 后 Bind（source: ADR-0005）。读档重绑纪律——`OnGameLoaded` 后先全量解绑呈现层既有订阅（防 EC-8 双订阅），再按权威订阅矩阵重绑（source: ADR-0003）。
  - **Forbidden**: Never 重排枚举整数索引（破坏旧存档兼容）（source: ADR-0005）。Never 重复存派生量/`bIsBankrupt`/`bIsInJail`/MVP 骰子 Seed（source: ADR-0005）。Never 在状态机用 `FTimerHandle`/Latent Action——不可序列化、读档无法 `switch(CurrentPhase)` 重入（source: ADR-0001/0005）。Never 手写 `FArchive Serialize()` 流（读写顺序须人工对齐、错位即静默损坏）（source: ADR-0005）。
  - **Guardrail**: 存档非帧路径，gameplay 16.6ms 帧不受影响；读档反序列化 + 重建 < 1s（source: ADR-0005）。

## Acceptance Criteria

- [ ] AC-1（TR-turn-012，GDD AC-33）: 构造处于 `PostRollAction`（currentPlayer=1, ConsecutiveDoubles=2, 含完整 `FDiceRollResult`{Die1/Die2/Total/bIsDouble}）的 TurnManager fixture→序列化→反序列化到新实例→断言 ① phase==PostRollAction；② currentPlayer==1；③ ConsecutiveDoubles==2；④ 向新实例发 `EndTurn`→状态机进入 TurnEnd（而非错误阶段忽略信号或崩溃）。④ 验证 FSM 真正处于正确状态。
- [ ] AC-2（TR-turn-012，GDD AC-34/Edge L398）: 完整存档=11 个 PlayerState 字段（逐一含 `JailTurnsServed`）+ `ConsecutiveDoubles` + 当前阶段 + 完整 `FDiceRollResult`（含 Die1/Die2/Total/bIsDouble）+ 锁定的 `DOUBLES_JAIL_THRESHOLD`→读档于 PostRollAction 还原精确阶段；不回退 TurnStart 重掷。Die1/Die2 供 VFX 读档后忠实重现两骰面值（仅存 Total=4 无法判 1+3 还是 2+2）。
- [ ] AC-3（TR-turn-013，GDD AC-34/Edge L401）: 读档后重绑该阶段 delegate 监听器——枚举字段恢复值不够，须重 `AddDynamic` 该阶段事件绑定；恢复顺序 ① 反序列化所有 PlayerState/阶段字段 → ② 各下游重绑 delegate → ③ 才广播任何事件（② 前不在 `PostLoad` 即广播 `OnPhaseChanged`）。
- [ ] AC-4（TR-turn-013，ADR-0005 IG#5）: 读档重建拓扑序——DA→(经济/地产/建房/事件格牌堆 互不依赖)→回合2→骰子 SetSeed→重绑→`switch(CurrentPhase)`；禁重建 A 时读尚未重建的 B。
- [ ] AC-5（TR-turn-014，GDD AC-43）: `ETurnPhase`/`EPlayerColor`/`EAIDifficulty` 已有枚举值整数索引未变化（防重排破坏旧存档）；C++ 实现加 `static_assert` 固定既有枚举值整数。append-only 例外：审稿阶段引入、在任何引擎资产正式使用前即删除的草稿值（未分配持久化索引）删除不违反 append-only；`ETurnPhase` 当前合法值=7 个（TurnStart/RollPhase/MovePhase/ResolvePhase/PostRollAction/TurnEnd/JailTurn）。
- [ ] AC-6（TR-turn-017，GDD Edge L399 + ADR-0005 CR-4）: 开局定序进行中（`TurnOrderInitRank` 阶段，可能多轮重掷）禁止存档（避免序列化「哪些席位已定 rank/当前 round」瞬态）；存档仅在定序完成、进入首个 `TurnStart` 后可用。可存档点查询接口（`CanSaveNow`/`bIsSafeToSave`）为存档21 提供 GIVEN，本系统不自裁阶段语义（守 enable-not-own）；签名冻结。

## Implementation Notes

> 逐字保真自 ADR-0005 Implementation Guidelines #5/#7/#8/#9 + GDD Edge Cases 读档恢复时序，不改写语义。

1. **读档重建拓扑序（ADR-0005 IG#5）严格**：DA→(经济/地产/建房/事件格牌堆 互不依赖)→回合2→骰子 SetSeed→重绑→`switch(CurrentPhase)`。**禁止**重建 A 时读尚未重建的 B。
2. **delegate 重绑（ADR-0005 IG#7，与 ADR-0003 协同）**：存档21 不直接 AddDynamic，只广播 `OnGameLoaded`；呈现层（16/17/19/22）监听后各自先 Unbind 后 Bind（防双绑）。回合状态机 `switch(CurrentPhase)` 还原阶段须重绑该阶段 delegate（枚举字段恢复值不够，AC-34）。
3. **可存档点门（ADR-0005 IG#8，CR-4）**：查询回合2「是否在合法可存档点」（接口名待回合2 提供，OQ-Save-2）。MVP 默认锁权威路径：以回合2 报告为 GIVEN，本系统不自裁阶段语义（守 enable-not-own）。自判 fallback 为临时降级、无 AC 守护、回合2 接口到位后移除——勿默认实现。
4. **枚举 append-only（ADR-0005 IG#9）**：`ETurnPhase`/`EPlayerColor`/`EAIDifficulty`/`ESaveResult` 的整数索引不可重排（AC-43），否则旧存档读出错误枚举。改动只能尾部 append。
5. **状态机禁 Latent（ADR-0001）**：`ETurnPhase` 枚举字段 + delegate 推进，禁 `FTimerHandle`/`Delay`/`WaitForEvent`——读档 `switch(CurrentPhase)` 重入（EC L400）。
6. **读档恢复时序陷阱（GDD L401）**：dynamic multicast delegate 绑定列表不序列化（序列化会捕获旧实例悬空 weak 引用）——只序列化 `ETurnPhase` 等数据字段，读档后由各下游重新 `AddDynamic`/`AddUniqueDynamic` 重建绑定。关键顺序：反序列化字段 → 重绑 delegate → 才允许续行/广播。

## Out of Scope

- 存档21 框架（magic/version/checksum/manifest 完整性门 + 原子写盘）→ save-load epic（本系统提供被序列化字段 + 可存档点查询 + 读档重建时序契约）。
- PlayerState 11 字段定义 → **story-001**（本 story 序列化它们）。
- ETurnPhase 状态机阶段转换逻辑 → **story-002**（本 story 验读档后 `switch(CurrentPhase)` 重入 + FSM resume）。
- `FDiceRollResult` struct 定义与骰子 Seed 序列化策略 → dice-rng epic（本系统序列化当前程完整 `FDiceRollResult`，含 Die1/Die2）。
- 各事件 delegate 声明形态 → **story-004**（本 story 验读档后这些 delegate 的重绑）。

## QA Test Cases

> 本 story 为 Integration（序列化 round-trip + delegate 重绑拓扑序）；每 AC 一条 Given/When/Then；headless `-nullrhi`。delegate 重绑用 spy 验广播未丢失；枚举 append-only 用 `static_assert`/静态检查。

- **TC-1 (AC-1)**: GIVEN PostRollAction（currentPlayer=1, ConsecutiveDoubles=2, 完整 FDiceRollResult）fixture，WHEN 序列化→反序列化到新实例，THEN phase==PostRollAction、currentPlayer==1、ConsecutiveDoubles==2；发 `EndTurn`→进入 TurnEnd（FSM 真在正确状态）。
- **TC-2 (AC-2)**: GIVEN 完整存档（11 字段 + ConsecutiveDoubles + 阶段 + FDiceRollResult{Die1=1,Die2=3,Total=4} + 锁定阈值），WHEN 读档，THEN 还原精确阶段、Die1/Die2 保留（区分 1+3 vs 2+2）、不回退 TurnStart 重掷。
- **TC-3 (AC-3)**: GIVEN 读档恢复，WHEN 反序列化字段后，THEN 各下游重绑 delegate（重 `AddDynamic`），② 前不在 `PostLoad` 广播 `OnPhaseChanged`；spy 断言重绑后首次广播未丢失。
- **TC-4 (AC-4)**: GIVEN 读档重建，WHEN 执行拓扑序，THEN DA→经济/地产/建房/牌堆→回合2→骰子 SetSeed→重绑→`switch(CurrentPhase)`；断言不在重建 A 时读未重建的 B。
- **TC-5 (AC-5)**: GIVEN `ETurnPhase`/`EPlayerColor`/`EAIDifficulty`，WHEN 静态检查/`static_assert`，THEN 既有枚举值整数索引未变；`ETurnPhase` 合法值=7 个；草稿值（未分配持久化索引）删除不违反 append-only。
- **TC-6 (AC-6)**: GIVEN `TurnOrderInitRank` 进行中（多轮重掷），WHEN 查询可存档点，THEN 拒存；GIVEN 进入首个 TurnStart，THEN 可存。可存档点查询签名冻结，本系统不自裁阶段语义。
- **Edge cases**: invocation list 不序列化（捕获悬空 weak 引用）；MVP 不序列化骰子 Seed（当前骰由完整 FDiceRollResult 保护）；`bIsBankrupt`/`bIsInJail` 不重复存（随 PlayerState 字段）；读档后 `OnGameLoaded` 重建完成恰广播一次。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/player-turn/save_serialization_delegate_rebind_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-001（PlayerState 11 字段）、story-002（状态机 `switch(CurrentPhase)` 重入 + 禁 Latent）、story-004（各事件 delegate 重绑）、story-006（`FDiceRollResult` 完整序列化）。
- **Unlocks**: save-load epic（消费本系统被序列化字段 + 可存档点查询 + 读档重建拓扑序契约）。

---

## Completion Notes（2026-06-08，mode-A workflow wf_8896c638-fa2 + 主会话独立复跑）

**Status: Complete。6/6 AC 覆盖（TC1-6 全绿）。全量 237/237 Success, 0 Fail, 0 notRun, EXIT 0**
（基线 231 + 6，零回归；主会话独立全证 `Saved/TestReport_pt008_revert` 2026.06.08-06.14.20）。

### 交付（4 文件改 + 2 新建）
- **新建 `Source/rento/PlayerTurnSaveData.h`**：`FPlayerStateRecord`（GDD CR-1 全 11 字段 + ConsecutiveDoubles + per-player CurrentRollContext 共 13 字段值记录）+ `UPlayerTurnSaveData`（`USaveGame` 子类：PlayerRecords/CurrentPhase/CurrentActivePlayerId/DoublesJailThreshold）。全字段标 `UPROPERTY(SaveGame)`。
- **`Source/rento/PlayerTurnTypes.h`**：尾部追加 20 条 `static_assert`（ETurnPhase 7 值 0..6 + EPlayerColor 0..8 + EAIDifficulty 0..3 整数索引钉死）= AC-5 append-only **编译期 BLOCKING 门**。
- **`Source/rento/PlayerTurnSubsystem.h/.cpp`**：`CaptureSaveData`（const 采集）/ `RestoreFromSaveData`（拓扑序回合2 腿：NewObject 重建 PlayerStates 全字段 + **直写 CurrentPhase 不经 SetPhase**，静默）/ `ResumeFromLoadedState`（第③步 switch(CurrentPhase) 重广播）/ `CanSaveNow`（AC-6 可存档点查询，签名冻结）。
- **新建测试 `Source/rentoTests/Private/SaveSerializationDelegateRebindTest.cpp`**：6 TC（round-trip / 11 字段 / 重绑静默 / 拓扑序 / 枚举 / 可存档点）。

### 关键裁定与机制
- **🔴 G7 引擎实证（亲读 UE5.7 源码坐实，证伪 ADR-0005）**：内置 `SaveGameToMemory`/`SaveGameToSlot` **不按 `UPROPERTY(SaveGame)` 过滤**（全量序列化）——`GameplayStatics.cpp:2371` 无 ArIsSaveGame=true / `Archive.h:620` Epic 注释 "not true for any of the built in save methods" / `Property.cpp:1034`。**ADR-0005 §VR② + control-manifest "SaveGameToSlot 自动过滤 SaveGame 标记属性" = 假**。用户裁定方案A：round-trip 走 `SaveGameToMemory`/`LoadGameFromMemory`（in-memory 无磁盘产物），只断言 round-trip 恒等，字段仍标 SaveGame（产品路径对齐）。
- **AC-3 hazard 守卫（GDD L401）**：`RestoreFromSaveData` 直写 `CurrentPhase=` **不经 SetPhase**（SetPhase 会广播，restore 期下游未重绑→广播丢失）；广播只在 `ResumeFromLoadedState`。TC-3 变异坐实非 vacuous（改成 SetPhase → TC-3「restore 静默」精确 FAIL，TC-4 亦 FAIL，主会话独立复跑确认 `Saved/TestReport_pt008_mutation`）。
- **设计偏离登记**：DV-1 `URentoPlayerState` 是 UObject（story-001 选）→ 序列化用 `FPlayerStateRecord` 值记录绕开 UObject 指针；DV-2 roll-context holder per-player（story-005/006）→ `FDiceRollResult` 进每条记录（非 ADR 设想的回合2 单顶层）。信息等价。

### Follow-up（producer propagate，非 BLOCKING）
- **OQ-Save propagate**：ADR-0005 §Verification-Required② + control-manifest "SaveGameToSlot 自动过滤 SaveGame 标记属性" 经引擎源码实证为**假**（内置 save 不过滤）→ producer 更新 ADR-0005/control-manifest 措辞（不在本 story 范围内改 ADR 正文）。
- **save-load epic 集成债**：跨系统读档拓扑序 DA→经济/地产/建房/牌堆→回合2→骰子 SetSeed→重绑→switch 的**上游腿**（DA/经济/…）由存档21 编排；本 story 提供回合2 腿 + Capture/Restore/Resume/CanSaveNow 契约 + 读档重建时序。完整跨系统集成测试归 save-load epic。
