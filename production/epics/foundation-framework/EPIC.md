# Epic: Foundation 框架基础设施

> **Layer**: Foundation
> **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 ①②）
> **Architecture Module**: ① Subsystem 宿主 + ② Event Bus
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories foundation-framework`
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

## Next Step

Run `/create-stories foundation-framework` to break this epic into implementable stories.
