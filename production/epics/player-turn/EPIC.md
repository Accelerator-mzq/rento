# Epic: 玩家与回合管理

> **Layer**: Foundation
> **GDD**: design/gdd/player-turn.md
> **Architecture Module**: #2 玩家与回合 Player & Turn + GameStateSnapshot 装配
> **Status**: Ready
> **Stories**: 9 stories created (覆盖 TR-turn-001~017 全部 17 TR) — 见下方 ## Stories
> **Manifest Version**: 2026-06-06

## Stories

| # | Story | Type | Status | Covers TR | ADR (primary 首位) |
|---|-------|------|--------|-----------|--------------------|
| 001 | PlayerState 容器 + StartNewGame 开局入口 + 开局定序 | Logic | Ready | TR-turn-001, TR-turn-015 | ADR-0001, ADR-0004 |
| 002 | ETurnPhase 回合阶段状态机（delegate 推进、禁 Latent、双点链 F-3） | Logic | Ready | TR-turn-002 | ADR-0001, ADR-0007 |
| 003 | F-1 回合推进 + 破产移出 + OnGameWon 单一事件源（return-only） | Logic | Ready | TR-turn-006 | ADR-0003, ADR-0007, ADR-0001 |
| 004 | 6 回合事件契约 + USTRUCT payload + AI 行动可观察 hook | Logic | Ready | TR-turn-004, TR-turn-005 | ADR-0003, ADR-0007 |
| 005 | 受控写接口面（SetPosition/SetCash/SetJailState/SetBankrupt）C++ 强封装 | Logic | Ready | TR-turn-003 | ADR-0007, ADR-0001 |
| 006 | RollPhase 消费 FDiceRollResult + 当前程 holder + AI 决策 RNG 走骰子流 | Logic | Ready | TR-turn-010, TR-turn-011 | ADR-0004, ADR-0001, ADR-0003 |
| 007 | GameStateSnapshot 装配/冻结 + AI 决策 hook + ResolvePhase 聚合编排 | Integration | Ready | TR-turn-007, TR-turn-008, TR-turn-009 | ADR-0006, ADR-0001, ADR-0007 |
| 008 | 中途读档序列化 + delegate 重绑拓扑序 + 枚举 append-only + 可存档点查询 | Integration | Ready | TR-turn-012, TR-turn-013, TR-turn-014, TR-turn-017 | ADR-0005, ADR-0001, ADR-0003 |
| 009 | 回合状态机/编排落 C++ 权威模块 + CI 目录无 BP 派生类断言 | Config-Data | Ready | TR-turn-016 | ADR-0007, ADR-0001 |

**TR 覆盖核对**：TR-turn-001~017 全 17 个 TR 各至少被一个 story Covers（001→s1 / 002→s2 / 003→s5 / 004→s4 / 005→s4 / 006→s3 / 007→s7 / 008→s7 / 009→s7 / 010→s6 / 011→s6 / 012→s8 / 013→s8 / 014→s8 / 015→s1 / 016→s9 / 017→s8）。

## Overview

实现游戏的**流程框架与向下编排核心**——回合状态机宿主（`UWorldSubsystem`）。拥有中心 `PlayerState[]`
（11 字段容器：`CurrentTileIndex` 语义归移动4、`Cash` 语义归经济5、监狱态语义归事件7、`bIsBankrupt`
经破产9 返回值驱动写、`ConsecutiveDoubles`/`DisplayName`/`bIsAI`/`AIDifficulty`）、回合顺序/当前玩家/
当前 `Phase`、开局入口 `StartNewGame(FGameSetupConfig)`。`ETurnPhase` 枚举字段 + delegate 推进
（禁 Latent/FTimerHandle，读档 `switch(CurrentPhase)` 重入）。广播 6 回合事件（OnTurnStarted/
OnPhaseChanged/OnTurnEnded/OnTurnOrderResolved/OnAIActionExecuted/OnGameWon），编排调用下游
4/5/6/7/8/9/10。在 AI 决策阶段开头一次性装配冻结的 `FGameStateSnapshot`（值语义只读）供 AI 三 hook。
依赖骰子(3)消费 `FDiceRollResult`。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0001: UObject 宿主与生命周期 | 回合状态机挂 `UWorldSubsystem`；`ETurnPhase` 枚举字段 + delegate 推进，禁 Latent，读档 switch 重入 | LOW |
| ADR-0006: GameStateSnapshot 契约 | 值语义 `USTRUCT`（无活指针）；回合2 装配期一次性聚合 + 决策期冻结；`Rent_top1/2`/`PreaggregatedNlv` 预聚合（AI 严禁自算，防 5→8 反向环）| LOW |
| ADR-0007: BP-C++ 边界 | 回合状态机/编排落 C++ 权威模块（目录内无 BP 派生类，CI 断言）；受控写接口强封装 | LOW |
| ADR-0003: 事件总线 | 6 回合事件 + USTRUCT payload；OnGameWon 单一事件源（破产9 return-only）| LOW |
| ADR-0004: RNG | AI 同步决策随机走骰子3 权威流（重放预留）| MEDIUM |
| ADR-0005: 存档 | 序列化 CurrentPhase + 11 PlayerState 字段；读档 delegate 重绑 + 拓扑序；枚举 append-only | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-turn-001 | 中心 PlayerState（11 字段）运行时宿主 + 随局生灭（World Subsystem）| ADR-0001 ✅ |
| TR-turn-002 | 回合阶段状态机 `ETurnPhase` 枚举 + delegate 推进，禁 Latent/FTimerHandle | ADR-0001 ✅ |
| TR-turn-003 | 受控写接口面（SetPosition/SetCash/SetJailState/SetBankrupt）C++ 强封装 | ADR-0007 ⚠ Partial |
| TR-turn-004 | 6 回合事件契约 | ADR-0003 ✅ |
| TR-turn-005 | delegate payload 必须 USTRUCT（FTurnOrderResult/FTurnStartInfo 等）| ADR-0003 ✅ |
| TR-turn-006 | OnGameWon 单一事件源（回合2 据破产9 return-only winnerId 广播）| ADR-0003 ✅ |
| TR-turn-007 | GameStateSnapshot UE 类型（值语义只读 const&）+ 字段构成 | ADR-0006 ✅ |
| TR-turn-008 | CR-8 三/四 AI 决策 hook 传 `const FGameStateSnapshot&`（决策期冻结）| ADR-0006 ✅ |
| TR-turn-009 | snapshot 装配方/宿主 = 回合2 World Subsystem，跨系统聚合注入 | ADR-0001/0006 ✅ |
| TR-turn-010 | AI 同步决策随机走骰子3 权威流，禁自调引擎 RNG | ADR-0004 ✅ |
| TR-turn-011 | 回合消费完整 `FDiceRollResult`；RNG 服务与回合2 同生命周期层 | ADR-0004 ✅ |
| TR-turn-012 | 中途读档序列化（CurrentPhase + 11 PlayerState 字段 + ConsecutiveDoubles）| ADR-0005 ✅ |
| TR-turn-013 | 读档 delegate 重绑（invocation list 不序列化）+ 拓扑序重建 | ADR-0005 ✅ |
| TR-turn-014 | 枚举 append-only（ETurnPhase/EPlayerColor/EAIDifficulty 索引不可重排）| ADR-0005 ✅ |
| TR-turn-015 | 开局入口 `StartNewGame(const FGameSetupConfig&)` 宿主 + USTRUCT | ADR-0001 ✅ |
| TR-turn-016 | 回合状态机/编排落 C++ 权威模块（目录内无 BP 派生类，CI 断言）| ADR-0007 ✅ |
| TR-turn-017 | 可存档点查询接口（瞬态/定序进行中拒存）签名冻结 | ADR-0005 ⚠ Partial |

**Untraced Requirements**: None（17/17 有 ADR 覆盖；TR-003/017 Partial = 受控写接口/可存档点查询签名待实现期细化，非 Gap）

## Definition of Done

This epic is complete when:
- All stories implemented, reviewed, closed via `/story-done`
- All acceptance criteria from `design/gdd/player-turn.md` verified
- 回合状态机转换、定序 F-4、双点 F-3、GameStateSnapshot 装配/冻结、读档 switch 重入的 Logic 故事有 headless 通过的测试于 `tests/unit/player-turn/`
- 读档 delegate 重绑 + 拓扑序的 Integration 测试通过
- CI 目录级断言：回合模块目录下无 BP 派生类
- **Sprint0 引擎验证**：依赖 foundation-framework epic 的宿主框架（ADR-0001 #7）+ GameStateSnapshot 值语义（ADR-0006 LOW，无 post-cutoff）

## Next Step

Run `/create-stories player-turn` to break this epic into implementable stories.
