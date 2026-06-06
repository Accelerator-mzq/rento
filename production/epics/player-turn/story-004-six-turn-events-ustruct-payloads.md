---
Epic: player-turn
Status: Ready
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 004 — 6 回合事件契约 + USTRUCT payload + AI 行动可观察 hook

## Context

- **GDD**: `design/gdd/player-turn.md` — CR-3（阶段转换处广播事件）、「事件广播形态」(L265-275)、CR-2 席位裁定可见（`OnTurnOrderResolved`）、AI 回合可观察性 hook（`OnAIActionExecuted`，R6 RETREAT 非阻塞）、CR-3.5 建房通告 beat（`OnBuildingAnnounced`）
- **Requirement TR-ID**: TR-turn-004（6 个回合事件用 dynamic multicast delegate、`BlueprintAssignable`、1-对-多去中心化）、TR-turn-005（delegate payload 必须 USTRUCT、裸 `TArray` 须包 USTRUCT）
- **ADR Governing**:
  - **ADR-0003（primary）— 事件总线**：宏一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_NParams]` + `UPROPERTY(BlueprintAssignable, Category="Events")`；多字段 payload 必包 `USTRUCT(BlueprintType)`；裸 `TArray` 禁作 BlueprintAssignable 参数；事件 `On<PastTense>` 命名；字段「只增不改语义」、枚举 append-only；单一事件源（`OnAIActionExecuted` 由回合2 执行 AI 动作时广播，非 AI 发）。
  - **ADR-0007 — BP/C++ 权威边界**：事件由 C++ owner 广播、`BlueprintAssignable` 供 BP 叶子订阅（仅收事件→播动画/SetText，禁回写）。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（ADR-0003 delegate 框架自 UE4 起稳定）
- **Engine Notes（ADR-0003 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**（delegate 宏与 `BlueprintAssignable` 均非 5.4–5.7 新增）。
  - Verification Required: (1) `DYNAMIC_MULTICAST_DELEGATE` 仍要求 payload 为单一 `USTRUCT(BlueprintType)`——裸 `TArray` 作 BlueprintAssignable 参数仍编译失败（`OnTurnOrderResolved` 须包 USTRUCT）。
  - **payload 字段扩展的 UE 特有代价（GDD L274）**：dynamic multicast delegate 参数 USTRUCT 字段改名会致下游 BP 引脚**静默断线**（runtime null，不主动报编译错），仅打开该 BP 时出 "out of date node" 警告——「字段扩展」真正代价 = 重新打开并检查绑定该 delegate 的全部下游 BP 图（HUD16/AI10/存档21）引脚连接。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required**: 宏一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_NParams]` + `UPROPERTY(BlueprintAssignable, Category="Events")`；BP 与 C++ 双向可订（source: ADR-0003）。多字段 payload 必包 `USTRUCT(BlueprintType)`（如 `FPhaseChangedInfo`/`FTurnOrderResult`）；`OnTurnOrderResolved` 必须 USTRUCT 封装（source: ADR-0003）。事件 `On<PastTense>`；payload struct `F<Event>Info`（source: ADR-0003）。单一事件源——`OnAIActionExecuted` 由回合2 执行 AI 动作时广播（非 AI 发）；`OnBuildingChanged` 2 字段、actingPlayerId 由回合2 上下文取（source: ADR-0003）。
  - **Forbidden**: Never 把裸 `TArray` 作 BlueprintAssignable 参数（编译失败——`OnTurnOrderResolved` 须包 USTRUCT）（source: ADR-0003）。Never 把非 dynamic delegate 暴露给呈现层（BP 不可订）（source: ADR-0003）。Never 让呈现层回写或被反调（呈现层纯叶子不变式）（source: ADR-0003）。
  - **Guardrail**: Event delegate broadcast O(订阅数)，MVP 回合制低频可忽略；CPU 帧预算 16.6 ms / 60 FPS。

## Acceptance Criteria

- [ ] AC-1（TR-turn-004，GDD L265）: 6 回合事件（`OnTurnStarted`/`OnPhaseChanged`/`OnTurnEnded`/`OnTurnOrderResolved`/`OnGameWon`/`OnAIActionExecuted`）均声明为 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` + `UPROPERTY(BlueprintAssignable, Category="Events")`，BP 与 C++ 均可绑定（不用 `BlueprintImplementableEvent`）。
- [ ] AC-2（TR-turn-005，GDD L266-273）: 多字段 payload 包 USTRUCT(BlueprintType)——`FTurnOrderResult { TArray<int32> OrderedPlayerIds; bool bResolvedBySeatTiebreak; }`、`FTurnStartedInfo { int32 PlayerId; bool bIsAI; }`、`FPhaseChangedInfo { ETurnPhase OldPhase; ETurnPhase NewPhase; int32 ConsecutiveDoubles; }`、`FTurnEndedInfo { int32 PlayerId; bool bGrantsExtraTurn; }`、`FAIActionDetails { int32 ActionIndex; int32 ActingPlayerId; int32 TargetTileIndex; int32 Amount; EActionType ActionType; }`；`OnGameWon(int32 WinnerId)` 单 int 参数无须包 USTRUCT。裸 `TArray<int32>` 不作 dynamic delegate 参数（`OrderedPlayerIds` 已包入 `FTurnOrderResult`）。
- [ ] AC-3（GDD AC-5c）: `OnPhaseChanged` 进入 RollPhase 时 payload `ConsecutiveDoubles` 与框架当前状态一致——三变体精确值：① 首次 RollPhase→`==0`；② 第一次双点后额外回合 RollPhase→`==1`；③ 第二次双点后额外回合 RollPhase→`==2`。前提：mock 骰子3 返回 `bIsDouble=true` 推进双点链；若 OQ-1⑤ 选不可注入 hook 形态则降 [Advisory]。
- [ ] AC-4（GDD AC-7b）: `OnTurnEnded` payload `bGrantsExtraTurn`——双点未入狱未破产路径→`==true`（同玩家额外回合）；非双点正常移交→`==false`（移交新玩家）。验证 payload 字段在源头正确（防死字段）。前提：mock 骰子3 返回两值驱动两分支。
- [ ] AC-5（GDD AC-41）: 定序完成→`OnTurnOrderResolved` 被广播且 payload 含最终 TurnOrder 数组——① 无平手正常定序也广播；② 平手达 `MAX_TIEBREAK_ROUNDS`→席位裁定后广播（席位裁定不静默）。
- [ ] AC-6（GDD AC-44）: AI 行动可观察 hook（非阻塞）——mock AI 返回含 N≥1 条可行动作→① 全部可行（M=N）框架对每条实际执行动作广播 `OnAIActionExecuted`，payload `ActionIndex` 按实际执行顺序自增 `0..M-1`（框架拥有的轻量排序契约，BP 不得填 `-1` 空值）；② 含 K 条不可行（M=N-K）被跳过动作**不广播**，`ActionIndex` 在执行序列上连续不占号；③ 框架推进不被任何呈现回调门控——TurnEnd/移交路径无 `WaitFor*`/`Delay`/`FTimerHandle` 等待调用，也不读 `AIActionDisplayMode`（该旋钮归 HUD16）。
- [ ] AC-7（GDD AC-53）: 建房通告 beat——GIVEN `CurrentPlayerId==1` ∧ 建房8 在 `ResolvePhase`/`PostRollAction` 广播 `OnBuildingChanged(tile, newCount=2)`（2 字段、无 actingPlayerId），WHEN 该阶段进行，THEN ① 本系统广播 `OnBuildingAnnounced` ==1 次；② payload 含 `TileIndex`、`NewHouseCount==2`、`ActingPlayerId==1`（取自当前回合上下文 `CurrentPlayerId`，非 `OnBuildingChanged` 字段）；③ 不修改任何状态（纯信息事件）。对照组：`OnBuildingChanged` 在 `TurnStart`/`TurnEnd` 触发→`OnBuildingAnnounced` 不广播。

## Implementation Notes

> 逐字保真自 ADR-0003 Implementation Guidelines + 统一 Delegate 规范 + GDD「事件广播形态」，不改写语义。

1. **宏 + payload（ADR-0003 规范 1/2）**：一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_OneParam/_TwoParams/...]` + `UPROPERTY(BlueprintAssignable, Category="Events")`；单参或语义内聚多字段包 `USTRUCT(BlueprintType)`；**裸 `TArray` 禁作 BlueprintAssignable 参数**（编译失败）——`OnTurnOrderResolved` 必须 USTRUCT 封装；字段「只增不改语义」、枚举 append-only。
2. **命名（ADR-0003 规范 3）**：事件 `On<PastTense>`；payload struct `F<Event>Info` 或语义名。
3. **单一事件源（ADR-0003 规范 5）**：每个状态变更恰一个 owner 广播。`OnGameWon` 由回合2 触发（破产9 return-only）；`OnBuildingChanged` 2 字段（actingPlayerId 由回合2 上下文取，非塞事件第3字段）；`OnAIActionExecuted` 由回合2 执行 AI 动作时广播（非 AI 发）。
4. **`FAIActionDetails.ActionIndex`（GDD L187/L273）**：框架拥有的轻量排序契约——框架保证按实际执行顺序自增 `0..M-1`（M=实际执行动作数，被跳过动作不广播亦不占号），供 HUD 排序展示。R6 RETREAT 删除的仅是「框架消费它做 gating 收敛」，自增值域本身仍是框架的 [Logic] 可测契约（非可选提示）。`EActionType` 枚举（`BuyProperty`/`BuildHouse`/`Mortgage`/`Unmortgage`/`Move`/`PayRent`/`Bankrupt`…）为 HUD16 R-2 propagate 落定（OQ-HUD-12），owner=本系统。
5. **CR-3.5 建房通告 beat（GDD L163-167）**：`ResolvePhase` 监听建房8 `OnBuildingChanged(tile, newCount)`（owner=建房8 的 2 字段权威契约，本系统只读 owner 实际产出字段），收到时本系统广播全玩家可见 `OnBuildingAnnounced`——至少携带触发格 tile 名称、新 `house_count`、建筑归属玩家 id。**建筑归属玩家 id 由本系统从当前回合上下文取（=`CurrentPlayerId`），不从 `OnBuildingChanged` 读 actingPlayerId 第3字段**（2026-06-06 用户裁定方案②）。触发时机仅 `ResolvePhase`/`PostRollAction`，`TurnStart`/`TurnEnd` 不 emit。通告为信息层事件，不修改任何游戏状态；呈现职责归 HUD16。
6. **payload 扩展纪律（ADR-0003 字段只增不改语义 + GDD L274）**：任何字段变更（增/改名/改类型）波及绑定该 delegate 的全部下游 BP 图（HUD16/AI10/存档21）——须重新打开并检查绑定该 delegate 的全部 BP 图 payload 引脚连接，防静默断线。

## Out of Scope

- `OnGameWon` 触发时机/次数/边沿幂等语义（return-only 驱动）→ **story-003**（本 story 只声明 `OnGameWon` delegate 形态）。
- 状态机阶段转换序列触发 `OnPhaseChanged`/`OnTurnStarted`/`OnTurnEnded` 的精确顺序 → **story-002**（本 story 验 payload schema + 字段值正确性）。
- `OnTurnOrderResolved` 的定序逻辑与席位裁定算法 → **story-001/story-002**（本 story 验事件广播 + payload）。
- AI 动作的执行/可行性重校验/批处理跳过逻辑 → **story-007**（本 story 验 `OnAIActionExecuted` 广播契约）。
- 读档后 delegate 重绑拓扑序 → **story-008**。

## QA Test Cases

> 本 story 为 Logic（事件契约 + payload 字段值 + 广播次数）；每 AC 一条 Given/When/Then；确定性 fixture、headless `-nullrhi`。事件订阅用 C++ spy 计数器。

- **TC-1 (AC-1)**: GIVEN 6 回合事件声明，WHEN 静态检查，THEN 均为 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` + `BlueprintAssignable`，无 `BlueprintImplementableEvent`。
- **TC-2 (AC-2)**: GIVEN payload struct 集合，WHEN 编译验证，THEN 各多字段 payload 标 `USTRUCT(BlueprintType)`、`OrderedPlayerIds` 包在 `FTurnOrderResult` 内（无裸 `TArray` 作 dynamic delegate 参数），`OnGameWon` 单 int 无 USTRUCT。
- **TC-3 (AC-3)**: GIVEN mock 骰子3 推进双点链，WHEN 进入 RollPhase，THEN payload `ConsecutiveDoubles`==0/1/2（首次/第一次双点后/第二次双点后三变体精确值）。
- **TC-4 (AC-4)**: GIVEN 双点未入狱未破产 vs 非双点正常移交，WHEN TurnEnd，THEN `OnTurnEnded.bGrantsExtraTurn`==true / ==false。
- **TC-5 (AC-5)**: GIVEN 无平手定序 / 平手达上限定序，WHEN 定序完成，THEN `OnTurnOrderResolved` 均广播、payload 含最终 TurnOrder 数组。
- **TC-6 (AC-6)**: GIVEN mock AI 返回 [买地, 建房]（全可行）/含 1 条不可行，WHEN 执行，THEN 全可行时每条广播 `OnAIActionExecuted` `ActionIndex` 0,1；不可行被跳过不广播、ActionIndex 连续不占号；TurnEnd 路径无 `WaitFor*`/`Delay`/`FTimerHandle` 等待、不读 `AIActionDisplayMode`（可加 grep 守门）。
- **TC-7 (AC-7)**: GIVEN `CurrentPlayerId==1` + `OnBuildingChanged(tileA, 2)` 于 `ResolvePhase`，WHEN 阶段进行，THEN `OnBuildingAnnounced` 广播 1 次、payload `ActingPlayerId==1`/`NewHouseCount==2`、不改状态；对照组 `OnBuildingChanged` 于 `TurnStart`/`TurnEnd`→不广播。
- **Edge cases**: payload struct 字段改名场景在 code-review 须核绑定 BP 引脚（防静默断线）；`OnAIActionExecuted` 在双点额外回合链中每回合内独立从 0 自增；`OnBuildingAnnounced` 纯信息事件不可触发任何 PlayerState/地产状态写。

## Test Evidence

- **Type**: Logic
- **Path**: `tests/unit/player-turn/six_turn_events_ustruct_payloads_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-002（状态机阶段转换驱动事件）、story-003（`OnGameWon` 触发语义；本 story 声明其 delegate 形态）。
- **Unlocks**: story-007（`OnAIActionExecuted` 在 AI 动作执行时广播）、story-008（读档后各事件 delegate 重绑）。
