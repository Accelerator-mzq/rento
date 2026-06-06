---
Epic: player-turn
Status: Ready
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 006 — RollPhase 消费 FDiceRollResult + 当前程上下文 holder + AI 决策 RNG 走骰子流

## Context

- **GDD**: `design/gdd/player-turn.md` — CR-3 RollPhase（接收完整 `FDiceRollResult`）、CR-3.1（当前程骰上下文 holder + 程间原子性/非重入）、CR-8 确定性（AI 内部随机走骰子3 可种子化 RNG）、Edge Cases（程间非重入 / SentToJail 落地抑制）、AC-46/AC-47
- **Requirement TR-ID**: TR-turn-010（AI 同步决策随机须走骰子3 可种子化权威 RNG 流、禁自调引擎 RNG，为重放预留）、TR-turn-011（回合消费完整 `FDiceRollResult`〔Die1/Die2/Total/bIsDouble〕；RNG 服务与回合2 同生命周期层）
- **ADR Governing**:
  - **ADR-0004（primary）— 确定性 RNG**：全部 gameplay 随机（含 AI 决策扰动）走唯一权威 `FRandomStream`，挂 DiceService（宿主 ADR-0001）；禁旁路引擎全局 RNG；表现层 juice 随机与 AI 内部扰动若属表现层走独立非权威流。`RollDice` 执行序铁律：流抽 Die1,Die2→本地固化 result→广播 `OnDiceRolled`→返回同一固化 result。
  - **ADR-0001 — UObject 宿主与生命周期**：DiceService 与回合2 同挂 World Subsystem（同生命周期层）；程间非重入承接「禁 Latent 天然成立不覆盖同步嵌套」路径。
  - **ADR-0003 — 事件总线**：回合消费 `OnDiceRolled(FDiceRollResult)`；`OnDiceRolled` 回调内禁同步调任何抽取 API（单线程重入破坏 dice F-4 前提②）。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: MEDIUM**（ADR-0004：`FRandomStream`/`RandRange` 内部实现与默认种子行为是 post-cutoff〔LLM ~5.3〕待实测）
- **Engine Notes（ADR-0004 Engine Compatibility）**:
  - Knowledge Risk: 🟡 MEDIUM — `FRandomStream` 本身 pre-5.3 稳定（LOW），但 UE 5.4–5.7 是否新增官方确定性/网络 RNG 子系统、`RandHelper` 是否仍走 `FRand()` 浮点中介，超出模型知识截止，须实现期查 5.7 源码/Release Notes。
  - Verification Required（Sprint 0）: ① `FRandomStream::RandRange(int,int)` 是否仍走 `RandHelper: Min + TruncToInt(FRand()*Range)` 浮点中介；② `FRandomStream` 经 `UPROPERTY` 序列化是否持久化当前滚动 `Seed`；③ 默认构造种子是否仍恒 0；④ 是否新增官方确定性/网络 RNG 子系统。
  - Post-Cutoff APIs Used: 无新增——核心类型 `FRandomStream`/`RandRange(int,int)`/`FRand()` 均 pre-5.3 稳定；风险点是「5.7 是否改了 `RandRange` 内部实现」。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required**: 全部 gameplay 随机（骰点/牌堆洗牌/AI 决策扰动）走唯一权威 `FRandomStream`，挂 DiceService；禁旁路引擎全局 RNG（source: ADR-0004）。流隔离——表现层 juice 随机与 AI 内部扰动不得复用骰子3 权威流，各持独立非权威 `FRandomStream`（source: ADR-0004）。`OnDiceRolled` 回调内禁同步调任何抽取 API（source: ADR-0004）。
  - **Forbidden**: Never 旁路引擎全局 RNG（`FMath::Rand`/`UKismetMathLibrary::RandomInteger`/BP `Random Integer in Range`）做需重放的 gameplay 随机（source: ADR-0004）。Never 取消独立流让 VFX/Audio/HUD juice 抖动也调权威流（致命破坏重放）（source: ADR-0004）。
  - **Cross-Cutting**: CI 随机硬门——对权威 C++ 模块（AI/经济/建房/破产/回合/RNG）grep 禁 `FMath::Rand`/`FMath::RandRange`/`FMath::FRand`/`rand(`/`std::rand`，白名单仅 `UDiceRngService` 内 `FRandomStream` 调用（source: ADR-0007）。RNG 流隔离——权威随机走唯一权威流、表现层 juice 各走独立非权威流，两者绝不混用（source: ADR-0004）。
  - **Guardrail**: RNG `FRandomStream` LCG O(1) 抽取，非性能热点；CPU 帧预算 16.6 ms / 60 FPS。

## Acceptance Criteria

- [ ] AC-1（TR-turn-011，GDD CR-3 RollPhase）: RollPhase 接收完整 `FDiceRollResult`（Die1/Die2/Total/bIsDouble），消费 `Total`+`bIsDouble` 做逻辑；本系统作为「当前程掷骰上下文」holder 持有该完整结果。
- [ ] AC-2（GDD CR-3.1）: 本系统拥有当前程 `Total` 单源——一回合内多程位移（如机会牌额外掷骰）时保证经济5 读到「正在结算的这一程」的骰点、不串程；事件额外掷骰经受控写 `SetCurrentRollContext(FDiceRollResult)` 更新 holder，再由经济在 ResolvePhase PULL。
- [ ] AC-3（GDD AC-47）: 程间非重入——spy 在 `OnPawnLanded` 回调入口置 `bInLandedCallback=true`、出口置 `false`；GIVEN 第一程 `Advance` 触发 `OnPawnLanded`（spy 在回调内同步尝试发起第二程），WHEN 本系统编排多程位移，THEN 本系统发起第二程 `Advance` 时 `bInLandedCallback==false`（第二程在第一程落地回调完全同步返回之后），验证多程严格串行、无同步嵌套重入。
- [ ] AC-4（GDD AC-46）: SentToJail 落地抑制——GIVEN `OnPawnLanded(arrivalContext=SentToJail)`，WHEN `ResolvePhase`，THEN spy 买地决策 hook（`DecideBuyProperty`）与收租结算入口调用次数==0；对照组 `arrivalContext=DiceMove` 落无主地产时 `DecideBuyProperty`==1（证明抑制由 `arrivalContext` 驱动）。
- [ ] AC-5（TR-turn-010，GDD CR-8 确定性 + OQ-T-3②）: AI 内部任何随机性走骰子3 可种子化 RNG（经 `RandomRange`/`Float01`），禁自调引擎随机（为 OQ-3 Full Vision 重放预留）。BP-only 下为软约束（code-review 守）；C++ 强封装可对 AI 模块加静态扫描禁用引擎 RNG 符号作硬门。

## Implementation Notes

> 逐字保真自 ADR-0004 Implementation Guidelines + GDD CR-3.1/CR-8 确定性，不改写语义。

1. **`OnDiceRolled` 回调内禁同步调任何抽取 API**（ADR-0004）——单线程重入会插入权威序列，破坏 dice F-4 前提②。
2. **RNG 唯一权威流（ADR-0004）**：全部 gameplay 随机走唯一权威 `FRandomStream`（挂 DiceService）；AI 决策扰动（F-3 Epsilon、tiebreak）走权威流经 `RandomRange`/`Float01`（可重放）；AI 内部 juice/扰动若属表现层走独立非权威流，严禁经 DiceService 权威 API 取随机。
3. **CR-3.1 当前程骰上下文 holder（GDD L132）**：`RollPhase` 收到的完整 `FDiceRollResult` 由本系统持有，在 `ResolvePhase` 期间将其 `Total` 暴露给经济5 Utility 租 **PULL**（经济显式参数消费、不缓存、不向移动索取）。事件额外掷骰的 `dice_total` 由事件7 经受控写 `SetCurrentRollContext(FDiceRollResult)` 更新 holder，再由经济在 ResolvePhase PULL。
4. **程间非重入（GDD L132，承接移动 CR-4）**：本系统**不得**在前一程移动的 `OnPawnLanded` 同步返回前发起第二程 `Advance`/`TeleportTo`——多程位移严格串行，前程落地结算（ResolvePhase）未完不开下一程，杜绝同步嵌套重入（`OnPawnLanded` 同步 Broadcast，监听者回调内同步触发新 `Advance`=同步嵌套，「禁 Latent 天然成立」不覆盖此路径，见 AC-47）。
5. **SentToJail 落地抑制（GDD Edge Cases L392）**：`OnPawnLanded.arrivalContext == SentToJail` 时 `ResolvePhase` 不进买地/收租/事件结算分支——被传送入狱非「正常落地」。本系统据 `arrivalContext` 路由：`SentToJail`→跳过全部落地结算分支，直接置 `bIsInJail` 相关状态（进/出狱规则归7）并推进至 `PostRollAction`/`TurnEnd`。
6. **AI RNG 强度坦诚标注（GDD L196）**：BP-only 下软约束（code-review 守，OQ-T-3②）——BP 项目无法引擎层阻止 AI 蓝图节点直接调 `RandomFloat`/`FMath::Rand`；若 OQ-1 ADR 选 C++ 强封装，可对 AI 模块加静态扫描禁用引擎 RNG 符号作硬门。MVP AI 简单、重放是 OQ-3 才真需要，MVP 软约束足够。

## Out of Scope

- `FDiceRollResult` struct 定义与骰子3 `RollDice` 实现（执行序铁律）→ dice-rng epic（本系统只消费 `OnDiceRolled` 返回的完整结果）。
- `SetCurrentRollContext` setter 的封装强度 → **story-005**（本 story 验 holder 单源消费语义）。
- 经济5 Utility 租 F-4 算租公式 → economy epic（本系统只暴露 `Total` 供 PULL）。
- 移动4 `Advance`/`TeleportTo`/`OnPawnLanded` 实现 → movement epic（本系统编排程间串行）。
- `FDiceRollResult` 含 Die1/Die2 的序列化（读档不重掷）→ **story-008**。

## QA Test Cases

> 本 story 为 Logic（含确定性 RNG/holder 单源/非重入 fixture）；每 AC 一条 Given/When/Then；确定性 fixture、headless `-nullrhi`。骰子3 用 mock 注入受控 `FDiceRollResult` 序列；移动4 `OnPawnLanded` 用 spy。

- **TC-1 (AC-1)**: GIVEN mock 骰子3 在 RollPhase 返回 `FDiceRollResult{Die1=2,Die2=2,Total=4,bIsDouble=true}`，WHEN RollPhase，THEN 本系统 holder 持完整结果、消费 `Total=4`+`bIsDouble=true`。
- **TC-2 (AC-2)**: GIVEN 一回合内多程位移（机会牌额外掷骰经 `SetCurrentRollContext` 更新 holder），WHEN 经济 ResolvePhase PULL `Total`，THEN 读到正在结算这一程的骰点（不串程）。
- **TC-3 (AC-3)**: GIVEN 第一程 `Advance` 触发 `OnPawnLanded`，spy 在回调内同步尝试第二程，WHEN 本系统编排，THEN 第二程 `Advance` 发起时 `bInLandedCallback==false`（严格串行）。
- **TC-4 (AC-4)**: GIVEN `OnPawnLanded(SentToJail)`，WHEN ResolvePhase，THEN `DecideBuyProperty`/收租入口调用次数==0；对照组 `DiceMove` 落无主地产→`DecideBuyProperty`==1。
- **TC-5 (AC-5)**: GIVEN AI 决策含随机扰动，WHEN 静态检查 AI 模块（grep）/code-review，THEN 随机仅经 `UDiceRngService::RandomRange`/`Float01`，无 `FMath::Rand`/`FMath::FRand`/BP Random 节点。
- **Edge cases**: `OnDiceRolled` 回调内不得二次抽取（重入插入权威序列）；表现层 juice 随机走独立非权威流（不复用权威流）；多程位移每程 holder 更新后 PULL 取当前程值；SentToJail 抑制由 `arrivalContext` 驱动而非恒不调用。

## Test Evidence

- **Type**: Logic
- **Path**: `tests/unit/player-turn/dicerollresult_consumption_ai_rng_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-002（状态机 RollPhase/ResolvePhase 阶段）、story-005（`SetCurrentRollContext` 受控写 holder）；dice-rng epic（`FDiceRollResult` + `OnDiceRolled`）。
- **Unlocks**: story-007（ResolvePhase 聚合 + AI hook 消费 snapshot 在本系统 holder/RNG 基础上）、story-008（`FDiceRollResult` 完整序列化）。
