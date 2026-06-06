# Epic: Foundation 框架基础设施

> **Layer**: Foundation
> **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 ①②）
> **Architecture Module**: ① Subsystem 宿主 + ② Event Bus
> **Status**: Ready
> **Stories**: 7 created (see ## Stories below)
> **Manifest Version**: 2026-06-06

## Overview

实现所有 Foundation/Core 运行态系统的**共同底座**——UE 宿主生命周期与去中心化事件总线纪律层。
两个架构模块均无 GDD owner，由 Technical Director 经 ADR 拥有，是 GDD 里反复出现的"待架构期裁定"
OQ（OQ-1 宿主、OQ-Event-7 RNG/宿主、OQ-HUD-2/3 绑定）的落点。① Subsystem 宿主提供
per-match `UWorldSubsystem`（回合/经济/棋盘/RNG/事件/GameStateSnapshot 生成方）与跨局
`UGameInstanceSubsystem`（Save/Audio/Setup）的生命周期框架、`Initialize/Deinitialize` 时序、
`IGameClock` 等 DI 注入点；**不持任何游戏状态语义**（写语义归各 owner）。② Event Bus 是去中心化
owner-held `DYNAMIC_MULTICAST_DELEGATE` 的命名/payload/单一事件源**纪律层** + 读档集中解绑/重绑
工具函数（物理遍历各 owner delegate，防 EC-8 双订阅）。本 epic 门控所有下游系统的 headless 可测性。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0001: UObject 宿主与生命周期 | per-match 服务挂 `UWorldSubsystem`（一局边界/PIE 隔离）、跨局挂 `UGameInstanceSubsystem`；Seed 注入唯一时机 `OnWorldBeginPlay`；状态机禁 Latent；HUD 状态机 + `IGameClock` DI | LOW（门控下游 headless 可测性）|
| ADR-0003: 事件总线与 Delegate 契约 | 去中心化 owner-held `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`；多字段 payload 必包 USTRUCT；单一事件源；读档集中解绑/重绑纪律层 | LOW |

## GDD Requirements

> 本 epic 是 TD-owned 引擎集成模块，**无独立 GDD TR**。它实现下游各系统 TR 的共同框架底座；
> 以下 TR 的宿主/事件面由本 epic 提供框架，TR 的功能 DoD 归各系统 epic：

| 支撑的下游 TR | 面 | ADR Coverage |
|-------|-----|--------------|
| TR-turn-001/002 | 回合 World Subsystem 宿主 + 状态机框架 | ADR-0001 ✅ |
| TR-turn-004/005 | 6 回合事件 + USTRUCT payload 契约 | ADR-0003 ✅ |
| TR-turn-013 | 读档 delegate 重绑 + 拓扑序 | ADR-0003/0005 ✅ |
| TR-dice-005/008 | OnDiceRolled 契约 + DiceService 宿主 | ADR-0003/0001 ✅ |
| TR-board-007/008 | DA 持有者宿主 + 热切换生命周期边界 | ADR-0001 ✅ |

## Definition of Done

This epic is complete when:
- per-match `UWorldSubsystem` 与跨局 `UGameInstanceSubsystem` 宿主框架可在 `-nullrhi` headless 下
  `NewObject` 实例化；`ShouldCreateSubsystem` 排除 editor-preview World
- PIE Stop→Start 隔离正确（**Sprint0 引擎验证 ADR-0001 #7**：`Initialize/Deinitialize` 随 PIE World
  触发、`OnWorldBeginPlay` 重触发、`Deinitialize` `CancelHandle` 后无悬挂回调）
- `IGameClock` DI 注入骨架就位（生产 `FWorldGameClock` / 测试 `FMockGameClock`）
- Event Bus 集中解绑/重绑工具函数实现（读档 `OnGameLoaded` 后先全量解绑再按权威矩阵重绑）
- 所有 Logic 故事有 headless 通过的测试文件于 `tests/`
- All acceptance criteria from architecture.md §2.1 ①② module boundaries verified

## Stories

> 拆分自架构 §2.1 模块① Subsystem 宿主 + ② Event Bus + 治理 ADR-0001/0003 实现锚点。
> 排序：宿主基础底座（001–004）→ Event Bus 纪律层（005–006）→ Sprint0 引擎验证（007）。

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | UMatchSubsystemBase per-match 宿主基类与生命周期 | Integration | Ready | ADR-0001 |
| 002 | IGameClock DI 注入骨架（生产/测试时钟） | Integration | Ready | ADR-0001 |
| 003 | 异步加载纪律 + Deinitialize CancelHandle（防 PIE 空棋盘） | Integration | Ready | ADR-0001 / ADR-0002 |
| 004 | 跨局 UGameInstanceSubsystem 宿主框架（Save/Audio/Setup 入口） | Integration | Ready | ADR-0001 |
| 005 | Event Bus 纪律层：统一 delegate 规范 + USTRUCT payload 契约 | Integration | Ready | ADR-0003 / ADR-0001 |
| 006 | Event Bus 读档集中解绑/重绑工具函数（防 EC-8 双订阅） | Integration | Ready | ADR-0003 / ADR-0005 |
| 007 | Sprint0 引擎验证：PIE 隔离 + OnWorldBeginPlay 重触发 + CancelHandle | Integration | Ready | ADR-0001 / ADR-0004 |

**TR 覆盖**（每 TR 至少被一个 story Covers）：TR-turn-001/002（001 宿主框架 + 007 验证）· TR-turn-004/005（005 delegate 规范）· TR-turn-013（006 重绑）· TR-dice-005（005 OnDiceRolled 契约）· TR-dice-008（001 宿主 + 007 Seed 注入验证）· TR-board-007（003 DA 持有者宿主 + 防 GC）· TR-board-008（003 热切换边界 + CancelHandle，007 验证）。

## Next Step

Stories ready. Run `/story-readiness production/epics/foundation-framework/story-001-match-subsystem-base.md`
to validate before `/dev-story`. story-007 是 **Sprint0 开工首步**（引擎验证门控全部下游）。
