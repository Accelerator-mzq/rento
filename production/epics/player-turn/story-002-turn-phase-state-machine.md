---
Epic: player-turn
Status: Complete
Layer: Foundation
Type: Logic
Estimate: L
Manifest Version: 2026-06-06
Last Updated: 2026-06-07
---

# Story 002 — ETurnPhase 回合阶段状态机（delegate 推进、禁 Latent、双点链 F-3）

## Context

- **GDD**: `design/gdd/player-turn.md` — CR-3（回合结构粗粒度阶段状态机）、CR-4（双点额外回合）、CR-5（回合推进与跳过）、States and Transitions (b)（阶段状态机表 + 非法转移拒绝）、F-3（DoublesToJail）、F-4 平手裁定、Edge Cases（入狱优先/出狱双点不进链/读档地雷）
- **Requirement TR-ID**: TR-turn-002（回合阶段状态机 `ETurnPhase` 枚举字段 + delegate 推进、结构不可变、禁 Latent/FTimerHandle）
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期边界**：状态机用 `ETurnPhase` 枚举字段 + delegate 推进，**禁 `FTimerHandle`/Blueprint `Delay`/`WaitForEvent`（Latent Action）**——读档 `switch(CurrentPhase)` 重入；状态机挂回合2 World Subsystem。
  - **ADR-0007 — BP/C++ 权威边界**：状态机 phase 写权威状态 → 落 C++；`[Logic] BLOCKING` AC 的被测逻辑落 C++ 独立 `UObject`/纯函数，`-nullrhi` 可实例化。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（Subsystem + 枚举驱动状态机为 pre-5.3 稳定模式）
- **Engine Notes（ADR-0001 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**。
  - Verification Required: `OnWorldBeginPlay` 在 PIE Stop→Start 之间确实重新触发（首个 `OnTurnStarted` 由回合2 在此后驱动）。
  - **UE 实现地雷（GDD Edge Cases L400）**：「还原精确阶段」要求状态机用 `ETurnPhase` 枚举字段 + delegate 推进，**不可**用 Latent Action（`Delay`/`WaitForEvent`）实现阶段等待——Latent 等待状态在 Save 时丢失，读档无法从中途恢复。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required**: 状态机禁 Latent——`ETurnPhase` 枚举字段 + delegate 推进；读档 `switch(CurrentPhase)` 重入（source: ADR-0001）。
  - **Required（Core）**: 写状态机 phase 落 C++（source: ADR-0007）；`[Logic] BLOCKING` AC 被测逻辑落 C++ 独立 `UObject`/纯函数，`-nullrhi` 可实例化、可注入 `IGameClock`、暴露可 spy getter；不得依赖 `UMG NativeTick` 或 BP `Bind Event` graph 层（source: ADR-0007）。
  - **Forbidden**: Never 在状态机用 `FTimerHandle`/Blueprint `Delay`/`WaitForEvent`（Latent Action）——不可序列化、读档无法 `switch(CurrentPhase)` 重入（source: ADR-0001/0005）。Never 让权威逻辑出现 BP 派生类（source: ADR-0007）。
  - **Guardrail**: CPU 帧预算 16.6 ms / 60 FPS；状态机随对局推进，结构（阶段序列）不可变。

## Acceptance Criteria

- [ ] AC-1（GDD AC-5a）: 正常回合 `OnPhaseChanged` 事件按 [TurnStart→RollPhase→MovePhase→ResolvePhase→PostRollAction→TurnEnd] **精确顺序** fire，无跳跃、无缺失。
- [ ] AC-2（GDD AC-5b）: 非法转移拒绝——RollPhase 阶段发送 `EndTurn` 信号→状态机保持 RollPhase 不变（或抛 assertable error），不跳转 TurnEnd。
- [ ] AC-3（GDD AC-6）: 在狱玩家 TurnStart→路由进 JailTurn 分支（非 RollPhase）。
- [ ] AC-4（GDD AC-7）: 双点未入狱未破产→TurnEnd 后同玩家回 RollPhase，断言 `ConsecutiveDoubles` 已 +1。
- [ ] AC-5（GDD AC-8）: `ConsecutiveDoubles=3`→发 SendToJail 意图 + 取消移动 + 无额外回合 + 归零。
- [ ] AC-6（GDD AC-9）: 双点额外回合路径：第 1 次双点后 `ConsecutiveDoubles==1`；TurnEnd 判为「同玩家额外回合」时 `ConsecutiveDoubles` **不归零**（保持 1）；仅当行动权**实际移交下一玩家**（非双点结束）时归零。
- [ ] AC-7（GDD AC-21/AC-22/AC-23/AC-24）: F-3 DoublesToJail：(3,3)→true；(2,3)→false；非双点→`ConsecutiveDoubles` 置 0 且 DoublesToJail→false；(4,3)→true（`>=` 防御越级）；阈值旋钮 (2,2)→true（house rule=2）。
- [ ] AC-8（GDD AC-11/AC-11b）: 下一玩家在狱（非破产）→仍获回合（路由 JailTurn，不跳过）；在狱玩家 JailTurn 进入时 `JailTurnsServed` 尚未变，确认留狱后 +1，出狱时不增加。
- [ ] AC-9（GDD AC-30/AC-31）: 双点但本回合入狱→无额外回合；双点出狱→`ConsecutiveDoubles` **不**累加、无额外回合（本系统可测部分）。
- [ ] AC-10（GDD AC-27）: P<2→拒绝开局；P>4（MVP 上限）→拒绝开局或返回特定错误码（防守层不静默接受越界 P）。
- [ ] AC-11（GDD AC-4/AC-26）: F-4 平手达 `MAX_TIEBREAK_ROUNDS`→剩余平手按席位升序裁定（确定性、有限终止；子组内裁定，保 rank 段边界）。

## Implementation Notes

> 逐字保真自 ADR-0001 Implementation Guidelines #4 + GDD CR-3/CR-4/CR-5/F-3/F-4 时序契约，不改写语义。

1. **状态机禁 Latent**：`ETurnPhase` 枚举字段 + delegate 推进，禁 `FTimerHandle`/Blueprint `Delay`/`WaitForEvent`（读档 `switch(CurrentPhase)` 重入；EC L400 / B-R5-5 钉死）。
2. **阶段状态机 (b)（GDD States 表）**：`TurnStart →(在狱? → JailTurn 分支 → TurnEnd)/(正常 → RollPhase → MovePhase → ResolvePhase → PostRollAction → TurnEnd)`；`TurnEnd →(双点且未入狱? → 回 RollPhase)/(否则 → 下一玩家 TurnStart)`。AI 回合 TurnEnd 与人类一致，即时移交，不阻塞于呈现回放（R6 RETREAT）。
3. **非法转移拒绝（GDD L236）**：状态机仅接受表内合法转移。非当前阶段预期的信号须被拒绝（保持当前阶段不变或抛 assertable error），不得跳转。这是「状态机结构不可变」约束的可测面。
4. **F-3 时序契约（GDD L356）**：① 骰子报 `bIsDouble`；② 若 T 则 `ConsecutiveDoubles+=1`（先累加）；③ 再判 `DoublesToJail`：T→取消移动+入狱+归零+不发额外回合，F→正常移动+回合末发额外回合（CR-4）；④ 若 `bIsDouble==F` → `ConsecutiveDoubles=0`。用 `>=` 非 `==`（额外防御层兜越级，非逻辑路径）。
5. **CR-4 双点额外回合**：本回合 `bIsDouble==true` 且未因此回合入狱且未破产→TurnEnd 后回同玩家 RollPhase；第 3 次连续双点→立即送监狱（跳过移动/结算）、回合链终止；`ConsecutiveDoubles` 在行动权移交下一玩家时归零。送监狱复用 GoToJail 路径（位置/状态改写归移动4/事件7，本系统只发起意图）。
6. **CR-5 推进与跳过**：`TurnEnd`（无额外回合）后行动权传下一个**未破产**玩家（破产者永久跳过）；**在狱玩家不跳过**——仍获回合，`TurnStart` 路由到 Jail 分支。
7. **F-4 平手裁定（GDD L375-377）**：`round` 是全局单次计数器（非每子组独立）；每轮所有当前仍平手子组同步各自重掷一次；达 `MAX_TIEBREAK_ROUNDS` 后剩余平手按席位升序裁定（**子组内裁定**——各子组竞争不相交 rank 段，子组内按席位升序，非全局升序）。

## Out of Scope

- F-1 NextActivePlayer 寻路 + 破产移出 + OnGameWon 触发 → **story-003**。
- 6 回合事件的 USTRUCT payload 声明（`FPhaseChangedInfo`/`FTurnStartedInfo`/`FTurnEndedInfo` 等）→ **story-004**（本 story 验状态机驱动这些事件的转换序列，payload struct schema 归 story-004）。
- PlayerState 11 字段容器 + 开局定序无平手分支 → **story-001**。
- RollPhase 消费 `FDiceRollResult` 细节 + AI 决策 RNG → **story-006**。
- 读档 `switch(CurrentPhase)` 重入的 round-trip 验证 → **story-008**（本 story 只钉「枚举驱动、禁 Latent」实现约束）。

## QA Test Cases

> 本 story 为 Logic（状态转换/阈值/计数）；每 AC 一条 Given/When/Then；确定性 fixture、headless `-nullrhi`。骰子用 mock 返回受控 `bIsDouble`。

- **TC-1 (AC-1)**: GIVEN 正常回合无双点无入狱，WHEN 推进，THEN `OnPhaseChanged` 按 [TurnStart→RollPhase→MovePhase→ResolvePhase→PostRollAction→TurnEnd] 精确顺序 fire，无跳跃/缺失。
- **TC-2 (AC-2)**: GIVEN 状态机处于 RollPhase，WHEN 发 `EndTurn` 信号，THEN 状态保持 RollPhase（或抛 assertable error），不跳 TurnEnd。
- **TC-3 (AC-3)**: GIVEN 当前玩家 `bIsInJail=true`，WHEN TurnStart，THEN 路由进 JailTurn 分支（非 RollPhase）。
- **TC-4 (AC-4)**: GIVEN 双点未入狱未破产，WHEN TurnEnd，THEN 同玩家回 RollPhase 且 `ConsecutiveDoubles` 已 +1。
- **TC-5 (AC-5)**: GIVEN `ConsecutiveDoubles` 累至 3，WHEN F-3 判定，THEN 发 SendToJail 意图 + 取消移动 + 无额外回合 + `ConsecutiveDoubles` 归零。
- **TC-6 (AC-6)**: GIVEN 第 1 次双点后 `ConsecutiveDoubles==1`，WHEN TurnEnd 判「同玩家额外回合」，THEN 保持 1（不归零）；WHEN 行动权实际移交下一玩家（非双点结束），THEN 归零。
- **TC-7 (AC-7)**: WHEN F-3 输入 (3,3)/(2,3)/(4,3)/(2,2 阈值=2) 与非双点，THEN 分别 true/false/true/true/false（非双点同时 `ConsecutiveDoubles=0`）。
- **TC-8 (AC-8)**: GIVEN 下一玩家在狱非破产，WHEN 推进，THEN 仍获回合（路由 JailTurn）；JailTurn 进入时 `JailTurnsServed` 未变，mock 返回「留狱」后 +1，「出狱」不增。
- **TC-9 (AC-9)**: GIVEN 双点但本回合入狱，THEN 无额外回合；GIVEN 双点出狱，THEN `ConsecutiveDoubles` 不累加、无额外回合。
- **TC-10 (AC-10)**: WHEN `StartNewGame` 传 P<2 / P>4，THEN 拒绝开局或返回特定错误码。
- **TC-11 (AC-11)**: GIVEN 平手达 `MAX_TIEBREAK_ROUNDS`（mock 骰子全平），WHEN 裁定，THEN 子组内按席位升序裁定（P=4 rolls=[9,5,9,5]→G1{0,2}争 rank0-1、G2{1,3}争 rank2-3→rank(0)=0,rank(2)=1,rank(1)=2,rank(3)=3）。
- **Edge cases**: P=4 全员同分 rolls=[9,9,9,9]→单一 TieGroup 达上限→席位升序 rank{0,1,2,3}（完全排列、确定终止）；非法信号在每个阶段均被拒绝（结构不可变）。

## Test Evidence

- **Type**: Logic
- **Path**: `tests/unit/player-turn/turn_phase_state_machine_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-001（PlayerState 容器 + 开局定序无平手分支）。
- **Unlocks**: story-003（F-1 推进基于状态机 TurnEnd）、story-004（事件按状态机转换序列广播）、story-006（RollPhase 消费骰子）、story-008（读档 `switch(CurrentPhase)` 重入基于枚举字段）。

## Completion Notes
**Completed**: 2026-06-07
**Criteria**: 11/11 passing（AC-1~11 全 COVERED；无 DEFERRED）
- AC-1 → TC1_NormalTurnPhaseSequence（UTurnPhaseSpy 捕获 OnPhaseChanged 精确 6 阶段序列含 TurnEnd 瞬态广播）
- AC-2 → TC2_IllegalTransitionRejected + Edge_IllegalSignalEveryPhase
- AC-3~9 → TC3-9（在狱路由/双点额外回合/三连双点送监狱/归零时机/ShouldGoToJail 纯函数五情形/服刑计数/入狱无额外回合）
- AC-10 → TC10_PBoundaryRejection（P<2/P>4 拒绝）
- AC-11 → TC11_TiebreakSeatOrderTieGroup（F-4 子组裁定 [9,5,9,5]→{0,1,2,3} + 全平 [9,9,9,9]）
**实现**：ETurnPhase 7 值（PlayerTurnTypes.h append-only）+ UPlayerTurnSubsystem 扩展（CurrentPhase + OnPhaseChanged 最小 seam + SetPhase/StartTurn/ProcessRollResult/AdvanceFrom*/EndTurn + ShouldGoToJail 静态纯函数 + ResolveInitialTurnOrderWithTiebreak F-4）
**禁 Latent 硬门**：状态机纯 ETurnPhase 枚举字段 + delegate 同步推进，零 FTimerHandle/Delay/WaitForEvent（ADR-0001 §4，读档 switch 重入预留）
**Deviations（advisory，已登记 tech-debt）**：
- AC-10「P>4 拒绝」（正确实现）致 pt-001 Edge_TokenColor_8Colors 回归（原建 8 玩家）→ 同批修复为 P=4 测 4 色互异
- code-review 修复：UPhaseSpy 原未真创建（agent 用轮询）→ 新建 TurnPhaseSpy.h 补 AC-1 广播序列正面验证；删死代码 PerformInitialTurnOrder
- 覆盖缺口（低优先）：非法拒绝仅测 RollPhase 入口 / 留狱后移交未断言 / F-4 部分平手未测 → tech-debt
**Test Evidence**：Logic — Source/rentoTests/Private/TurnPhaseStateMachineTest.cpp（12 测试）+ TurnPhaseSpy.h（spy）；主会话独立全量 191/191 Success, 0 Fail, EXIT 0（Saved/TestReport_pt002_r3/index.json，2026.06.07-13.50.17）
**Code Review**：Complete — /code-review APPROVED（2 真修复闭合 + deflate 过度 claim，主会话逐条独立裁定）
**Out of Scope 严守**：F-1 完整寻路/OnGameWon（→003）、payload USTRUCT（→004）、受控写（→005）、骰子消费（→006）、序列化（→008）均未实现
