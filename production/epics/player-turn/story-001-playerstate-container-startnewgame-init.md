---
Epic: player-turn
Status: Complete
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-07
---

# Story 001 — PlayerState 容器 + StartNewGame 开局入口 + 开局定序

## Context

- **GDD**: `design/gdd/player-turn.md` — CR-1（PlayerState 11 字段实体）、CR-2/CR-7（开局掷骰定序、玩家规模 2–4）、「开局入口契约」(L256-263)、F-4（TurnOrderInitRank）
- **Requirement TR-ID**: TR-turn-001（中心 PlayerState 11 字段运行时宿主 + 随局生灭，挂 World Subsystem）、TR-turn-015（开局入口 `StartNewGame(const FGameSetupConfig&)` 宿主 + `FGameSetupConfig`/`FPlayerSetupEntry` USTRUCT 归属）
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期边界**：回合状态机（含 PlayerState 容器宿主）挂 `UWorldSubsystem`，生命周期=一局，PIE Stop 即销毁、Start 即重建；`StartNewGame(const FGameSetupConfig&)` 入口挂跨局 `UGameInstanceSubsystem`（GameSetup），但 PlayerState 实体随局在 World Subsystem 内构建；宿主只持生命周期，不持状态写语义。
  - **ADR-0004 — 确定性 RNG**：开局定序掷骰走骰子3 唯一权威 `FRandomStream`（定序只取点数和、忽略 `bIsDouble`，不进双点链），禁旁路引擎 RNG。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（ADR-0001 Subsystem 框架 pre-5.3 稳定，落在模型训练数据内）
- **Engine Notes（ADR-0001 / ADR-0004 Engine Compatibility）**:
  - ADR-0001 Post-Cutoff APIs Used: **None** — `UWorldSubsystem`/`UGameInstanceSubsystem`/`Initialize`/`Deinitialize`/`OnWorldBeginPlay`/`ShouldCreateSubsystem` 自 UE4.22/4.24 起稳定。
  - ADR-0001 Verification Required: `-nullrhi` headless 下 `Initialize/Deinitialize` 随 PIE World 创建/销毁正确触发。
  - PlayerState 容器形态（轻量 `UObject` 引用语义 vs `USTRUCT` 值语义）与受控写强度的最终裁定挂 OQ-1 BLOCKING-ADR（见 GDD OQ-1 因子①/⑮）；本 story 实现字段构成与开局入口，强度门由 story-005 承接。
  - `FGameSetupConfig`/`FPlayerSetupEntry` 须标 `USTRUCT(BlueprintType)`，`TArray<FPlayerSetupEntry> Players` 字段包进 USTRUCT（裸 `TArray` 不可作 BP 边界传参）。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required**: per-match 服务一律继承 `UWorldSubsystem`；生命周期边界=一局，PIE Stop 即销毁、Start 即重建（source: ADR-0001）。宿主不持状态写语义（source: ADR-0001）。Full Vision 迁移预留——MVP 状态以普通 `UPROPERTY` 承载，字段命名对齐 `PlayerState`/`GameState`（source: ADR-0001）。
  - **Required**: 全部 gameplay 随机（含开局定序掷骰）走唯一权威 `FRandomStream`，挂 DiceService；禁旁路引擎全局 RNG（source: ADR-0004）。
  - **Forbidden**: Never 把 per-match 对局态全挂 `UGameInstanceSubsystem`（PIE 隔离破裂）（source: ADR-0001）。Never 旁路引擎全局 RNG（`FMath::Rand`/`UKismetMathLibrary::RandomInteger`/BP `Random Integer in Range`）做需重放的 gameplay 随机（source: ADR-0004）。
  - **Guardrail**: UObject 引用成员用 `TObjectPtr<T>` + `UPROPERTY()` 防 GC（source: current-best-practices）；CPU 帧预算 16.6 ms / 60 FPS。

## Acceptance Criteria

- [ ] AC-1（GDD AC-1）: PlayerState fixture 含全 11 字段、正确类型与默认（`bIsAI=false`→`AIDifficulty=None`；`bIsBankrupt`/`bIsInJail=false`；`JailTurnsServed`/`ConsecutiveDoubles=0`）。
- [ ] AC-2（GDD AC-13）: 经具名入口 `StartNewGame(const FGameSetupConfig&)` 注入 `Players.Num()∈{2,4}` 的 `FGameSetupConfig`，生成的 `PlayerState` 列表尺寸 == `Players.Num()`、`TurnOrderIndex` 覆盖 `0..P-1` 唯一（无硬编码 4）。
- [ ] AC-3（GDD AC-2）: 定序无平手 rolls=[5,9,3]→TurnOrderIndex{seat1:0, seat0:1, seat2:2}（点数降序）。
- [ ] AC-4（GDD AC-25）: F-4 排名 rolls=[5,9,3]→{seat1:0, seat0:1, seat2:2}（与 AC-3 同源验证 rank 分配）。
- [ ] AC-5（GDD AC-28）: 生成的 `PlayerState` 各字段 `PlayerId`/`TokenColor` 唯一分配（开局后恒定）；`TurnOrderIndex` 未覆盖 `0..P-1` 唯一→拒绝（与 board-data 索引唯一性校验同模式）。

## Implementation Notes

> 逐字保真自 ADR-0001 Implementation Guidelines + GDD「开局入口契约」/CR-2，不改写语义。

1. **per-match 服务一律继承 `UWorldSubsystem`**；`ShouldCreateSubsystem` 排除 editor-preview World。PlayerState 列表随局在 World Subsystem 构建。
2. **宿主不持状态语义**：World Subsystem 持服务实例与生命周期，但 `Cash`/`CurrentTileIndex`/`bIsInJail`/`bIsBankrupt` 的写语义仍归各 owner（移动4/经济5/事件7/破产9）；本 story 只声明字段容器与默认值，不实现委派字段的改写规则。
3. **Full Vision 迁移预留**：字段以普通 `UPROPERTY` 承载，命名对齐 `PlayerState`/`GameState`。
4. **开局入口签名（GDD L257）**：`void StartNewGame(const FGameSetupConfig& Setup)` —— 大厅装配后单次调用；本系统据此初始化 `PlayerState` 列表（分配 `PlayerId`/`TokenColor` 唯一）、定序、置初始阶段。返回 void；开局拒绝路径（P<2/P>4/越界）按 story-002 的 AC-27 防守。
5. **`FGameSetupConfig`**（`USTRUCT(BlueprintType)`，owner=回合2）：`TArray<FPlayerSetupEntry> Players`（尺寸即 P，动态无硬编码 4）；可选旋钮覆盖 `int32 DoublesJailThreshold;` / `int32 MaxTiebreakRounds;`（开局锁定，缺省取本系统默认）。
6. **`FPlayerSetupEntry`**（`USTRUCT(BlueprintType)`，owner=回合2）：`FText DisplayName;`（与 `PlayerState.DisplayName:FText` 一致）/ `EPlayerColor TokenColor;` / `bool bIsAI;` / `EAIDifficulty AIDifficulty;`（非 AI 为 None）。
7. **CR-2 定序**：每位玩家各掷一次骰，点数高者取 `TurnOrderIndex=0`，其余按点数降序排布；定序掷骰只取点数和、忽略 `bIsDouble`，不进双点链（`ConsecutiveDoubles` 开局为 0）。定序掷骰走骰子3 权威流（ADR-0004）。

## Out of Scope

- 平手重掷 / `MAX_TIEBREAK_ROUNDS` 席位裁定终止 → **story-002**（F-4 平手分支）。
- 受控写接口面（`SetPosition`/`SetCash`/`SetJailState`/`SetBankrupt`）强度 → **story-005**。
- 回合阶段状态机 `ETurnPhase` 转换与开局拒绝路径 AC-27 → **story-002**。
- PlayerState 字段序列化/读档 → **story-008**。
- `StartNewGame` 入口宿主（`UGameInstanceSubsystem`）骨架由 foundation-framework epic story-004 提供。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。⚠ `StartNewGame` 入口宿主依赖 FF-004（Sprint1），见 plan「跨 Sprint 依赖缺口」。

> 本 story 为 Logic；每 AC 一条 Given/When/Then；确定性 fixture、headless `-nullrhi` 可跑。定序掷骰用 mock 骰子3 返回固定点数序列。

- **TC-1 (AC-1)**: GIVEN 构造一个 PlayerState，WHEN 读其全 11 字段，THEN 字段类型/默认值正确（`bIsAI=false`→`AIDifficulty=None`，`bIsBankrupt`/`bIsInJail=false`，`JailTurnsServed`/`ConsecutiveDoubles=0`）。
- **TC-2 (AC-2)**: GIVEN `FGameSetupConfig.Players.Num()==2`，WHEN `StartNewGame`，THEN 生成 2 个 PlayerState、`TurnOrderIndex` 覆盖 {0,1} 唯一；对 `Num()==4` 重复→覆盖 {0,1,2,3} 唯一（断言无硬编码 4）。
- **TC-3 (AC-3/AC-4)**: GIVEN mock 骰子3 定序返回 rolls=[5,9,3]（seat0=5/seat1=9/seat2=3），WHEN 定序，THEN TurnOrderIndex{seat1:0, seat0:1, seat2:2}（点数降序）。
- **TC-4 (AC-5)**: GIVEN `StartNewGame` 完成，WHEN 检查 `PlayerId`/`TokenColor`，THEN 各唯一；GIVEN 人为构造 `TurnOrderIndex` 重复/不连续，WHEN 校验，THEN 拒绝开局。
- **Edge cases**: P=2 最小局正常定序；定序掷骰只取点数和（同点不入双点链，`ConsecutiveDoubles` 保持 0）；`TokenColor` 8 色枚举开局唯一分配（不重复）。

## Test Evidence

- **Type**: Logic
- **Path**: `tests/unit/player-turn/playerstate_container_startnewgame_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: foundation-framework story-001（`UMatchSubsystemBase` 宿主基类）、foundation-framework story-004（`StartNewGame` 入口 `UGameInstanceSubsystem`）。
- **Unlocks**: story-002（状态机基于 PlayerState 容器与定序结果驱动）、story-005（受控写接口面封装 PlayerState 委派字段）、story-008（序列化 11 字段 round-trip）。

## Completion Notes
**Completed**: 2026-06-07
**Criteria**: 5/5 passing（AC-1~5 全 COVERED；无 DEFERRED）
- AC-1 → TC1_PlayerStateDefaultValues（11+1 字段默认，含 DisplayName 空 FText）
- AC-2 → TC2_BuildSquad_SizeAndTurnOrderUnique（P=2&P=4，无硬编码 4）
- AC-3/AC-4 → TC3_TurnOrder_Rolls_5_9_3（[5,9,3]→{seat1:0,seat0:1,seat2:2}，同源）
- AC-5 → TC4_Uniqueness_PlayerIdColor_TurnOrderReject + Edge_TokenColorNone_Rejected + Edge_TurnOrderOutOfRange_Rejected（唯一分配 + 重复/None/越界三拒绝 sub-case）
**实现**：URentoPlayerState（轻量 UObject，11 字段+ConsecutiveDoubles）/ UPlayerTurnSubsystem（:UMatchSubsystemBase，建队+排名纯方法+生产定序+AC-5 校验）/ EPlayerColor+EAIDifficulty 枚举 / FGameSetupConfig+FPlayerSetupEntry 填充 / StartNewGame body forward
**Deviations（advisory，已登记 tech-debt）**：
- **seed 接线纳入 pt-001**（story 原文未列）：据 DiceRngService.h L257 契约「生产每局种子由 pt-001 在 OnWorldBeginPlay 后 SetSeed 传入」，InitializeFromConfig 定序前 SetSeed(Config.RandomSeed)，使定序可重放 + 消除 lazy-init Error。RandomSeed 字段置于 FGameSetupConfig（per-match 单种子，ADR-0004 单权威流），非 per-player entry。
- else 防御分支（DiceService==null seat 序兜底）实证恒不触发（DiceService 同为 WorldSubsystem headless 恒存在），保留为非常规 World 装配防御
**Test Evidence**：Logic — Source/rentoTests/Private/PlayerStateContainerStartNewGameTest.cpp（9 测试，逻辑归属 tests/unit/player-turn/）；主会话独立全量 179/179 Success, 0 Fail, EXIT 0（Saved/TestReport_pt001_r3/index.json，2026.06.07-12.32.32）
**Code Review**：Complete — /code-review APPROVED（4 真修复闭合 + deflate 过度 claim，主会话逐条独立裁定）
**Out of Scope 严守**：ETurnPhase / 受控写接口面 / 平手重掷 / 序列化 均未触碰（→ story-002/005/008）
