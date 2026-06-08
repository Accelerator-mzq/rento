# Technical Preferences

<!-- Populated by /setup-engine. Updated as the user makes decisions throughout development. -->
<!-- All agents reference this file for project-specific standards and conventions. -->

## Engine & Language

- **Engine**: Unreal Engine 5.7
- **Language**: Blueprint (primary — gameplay/UI), C++ (framework & performance-critical systems)
- **Rendering**: Unreal default (deferred). 卡通 2.5D 棋盘大概率用不到 Lumen/Nanite —— 架构阶段评估是否关闭以降成本与构建体积。
- **Physics**: Chaos (UE 默认)。回合制棋盘对物理需求极低,主要用于棋子/骰子表现。

## Input & Platform

<!-- Written by /setup-engine. Read by /ux-design, /ux-review, /test-setup, /team-ui, and /dev-story -->
<!-- to scope interaction specs, test helpers, and implementation to the correct input methods. -->

- **Target Platforms**: PC (Steam)
- **Input Methods**: Keyboard/Mouse
- **Primary Input**: Keyboard/Mouse
- **Gamepad Support**: Partial（PC 推荐支持，便于后续手柄/客厅模式）
- **Touch Support**: None
- **Platform Notes**: 鼠标点击为主交互（掷骰、买地、建房、菜单）。避免 hover-only 交互，以利后续手柄适配。使用 UE Enhanced Input 管理输入映射。

## Naming Conventions

<!-- UE 5 C++ + Blueprint 社区标准 -->

- **Classes**: 前缀 PascalCase（`A`=Actor、`U`=UObject、`F`=struct、`E`=enum），如 `ABoardPawn`、`UPropertyData`
- **Variables**: PascalCase；布尔加 `b` 前缀，如 `bIsBankrupt`、`MoveSpeed`
- **Signals/Events**: PascalCase（C++ Multicast Delegate / Blueprint Event），如 `OnRentPaid`、`OnTurnEnded`
- **Files**: 匹配类名去前缀，如 `BoardPawn.h` / `BoardPawn.cpp`
- **Scenes/Prefabs（UE Assets）**: Blueprint 用 `BP_` 前缀（`BP_BoardManager`）；UMG Widget 用 `WBP_` 前缀（`WBP_HUD`）；关卡 `.umap`；数据资产 `DA_` 前缀
- **Constants**: UPPER_SNAKE_CASE，或 `static constexpr` PascalCase

## Performance Budgets

- **Target Framerate**: 60 FPS
- **Frame Budget**: 16.6 ms
- **Draw Calls**: 适中（回合制单场景，远低于动作游戏需求）—— 具体上限待目标硬件确定
- **Memory Ceiling**: 待目标硬件确定（可后调）

## Testing

- **Framework**: UE Automation System（Functional Tests / Spec tests），CI 用 headless `-nullrhi`
- **Minimum Coverage**: 待 `/test-setup` 确定
- **Required Tests**: 规则/经济公式（租金、建房、破产判定）、回合状态机、AI 决策；联网（如适用）。细节由 `/test-setup` 搭建。

## Forbidden Patterns

<!-- Add patterns that should never appear in this project's codebase -->
- [None configured yet — add as architectural decisions are made]

## Allowed Libraries / Addons

<!-- Add approved third-party dependencies here -->
- [None configured yet — add as dependencies are approved]

## Architecture Decisions Log

<!-- Quick reference linking to full ADRs in docs/architecture/ -->

**Foundation 层（Accepted 2026-06-06）— coding 硬前提，ADR-0001 依赖根须最先：**
- **[ADR-0001](../../docs/architecture/ADR-0001-uobject-host-lifecycle.md)** — UObject 宿主与生命周期：per-match 服务挂 `UWorldSubsystem`（World=一局边界/PIE 隔离）、跨局持久挂 `UGameInstanceSubsystem`、联网 game state 留 Full Vision
- **[ADR-0002](../../docs/architecture/ADR-0002-board-data-container.md)** — 棋盘数据容器：`UPrimaryDataAsset`（DataTable CSV 不支持 `TArray` 列，已实证）；`AdvanceIndex` 返回 struct
- **[ADR-0003](../../docs/architecture/ADR-0003-event-bus-delegate.md)** — 事件总线：去中心化 owner-held `DYNAMIC_MULTICAST_DELEGATE`（Foundation Event Bus=命名/payload 纪律层）；破产职责切分（下游主反馈订经济5 `OnBankruptcyDeclared`）
- **[ADR-0004](../../docs/architecture/ADR-0004-deterministic-rng.md)** — 确定性 RNG：单权威 `FRandomStream`（dice 拥有）+ 各系统独立非权威流（AI 决策扰动走权威流须重放，juice 走独立流）
- **[ADR-0005](../../docs/architecture/ADR-0005-save-serialization-contract.md)** — 存档契约：`USaveGame`+`UPROPERTY(SaveGame)`+四重完整性门+存 DA 引用不存布局+枚举 append-only+MVP 单槽

**Core/Presentation/Input 层（全 Accepted 2026-06-06）：** ADR-0006 GameStateSnapshot · 0007 BP-vs-C++ 边界 · 0008 HUD 驱动+IGameClock DI · 0009 卡通材质 · 0010 音频架构 · 0011 Enhanced Input · 0012 CommonUI

**Core 层 Gap ADR（补 untraced Gap TR，2026-06-08 起）：**
- **[ADR-0013](../../docs/architecture/ADR-0013-property-ownership-runtime-container.md)**（Accepted 2026-06-08）— 地产所有权运行时容器与批量移交原子性：owner map = **AoS** `TArray<FTileOwnershipState>`（owner+bIsMortgaged 同记录、稠密 N、非可购买格哨兵、连续 0..N-1 前提）；批量移交先校验后写（全或全无）+单记录赋值同格原子+RAII 重入锁无条件解除；C++ private 封装→AC-3/7/31/32/35 升 [Logic]。**🔴 TArray<bool> bitfield 前提源码证伪**（UE 非特化，位打包是 TBitArray）。解 prop-001/002/012 Gap。
- **[ADR-0014](../../docs/architecture/ADR-0014-economy-integer-determinism-overflow.md)**（Accepted 2026-06-08）— 经济金钱运算整数确定性与 int32 溢出硬防护：金钱运算**整数纯净无 float** + 整数 num/den 显式取整（ceil `(x·num+den−1)/den`/floor 截断）+ **逐栋 floor 先于求和**（F-9 奇数 BuildingCost 差1）→ 位级一致（AC-32）；`passed_go` 运行时 clamp[0,1000]（非仅 ensure）；加载期 **fatal** 边界 `SalaryAmount≤2,000,000`/`DICE_MULT_MAX=1,000,000`（约束值归 economy、执行 board）。解 econ-014/015 Gap。**propagate 债**：SalaryAmount 上界 board 未落。
- **[ADR-0015](../../docs/architecture/ADR-0015-roll-context-holder-pull.md)**（Accepted 2026-06-08）— 回合掷骰上下文 holder（dice_total PULL）与移动越界告警上传：Utility 租 `dice_total` 经回合(2) holder `CurrentRollContext` PULL（经济 `GetCurrentRollTotal()`，移动不投递/不缓存），事件额外骰经回合(2) `SetCurrentRollContext` 注入（单源不串程）；移动不 clamp + 越界告警不静默吞冒泡（AC-24），int32 数值兜底委托 ADR-0014。**机制 pt-005/006/007 已实现，本 ADR 固化契约**。解 move-010/017/event-010 Gap；**move-013 联网迁移 = Full Vision defer 不裁**。

## Engine Specialists

<!-- Written by /setup-engine when engine is configured. -->
<!-- Read by /code-review, /architecture-decision, /architecture-review, and team skills -->
<!-- to know which specialist to spawn for engine-specific validation. -->

- **Primary**: unreal-specialist
- **Language/Code Specialist**: ue-blueprint-specialist（Blueprint 图）或 unreal-specialist（C++）
- **Shader Specialist**: unreal-specialist（无专属 shader specialist —— 由 primary 覆盖材质）
- **UI Specialist**: ue-umg-specialist（UMG widgets、CommonUI、输入路由、widget 样式）
- **Additional Specialists**: ue-gas-specialist（Gameplay Ability System、属性、gameplay effects）、ue-replication-specialist（属性复制、RPC、客户端预测、netcode —— 联网阶段）
- **Routing Notes**: 本项目以 Blueprint 为主。Invoke primary 处理 C++ 架构与广泛引擎决策。Invoke Blueprint specialist 处理 Blueprint 图架构与 BP/C++ 边界设计（本项目最常用）。Invoke GAS specialist 处理能力/属性代码。Invoke replication specialist 处理任何多人/联网系统（Full Vision 阶段）。Invoke UMG specialist 处理所有 UI 实现。

### File Extension Routing

<!-- Skills use this table to select the right specialist per file type. -->
<!-- If a row says [TO BE CONFIGURED], fall back to Primary for that file type. -->

| File Extension / Type | Specialist to Spawn |
|-----------------------|---------------------|
| Game code (.cpp, .h files) | unreal-specialist |
| Blueprint graphs (.uasset BP classes) | ue-blueprint-specialist |
| Shader / material files (.usf, .ush, Material assets) | unreal-specialist |
| UI / screen files (.umg, UMG Widget Blueprints) | ue-umg-specialist |
| Scene / prefab / level files (.umap, .uasset) | unreal-specialist |
| Native extension / plugin files (Plugin .uplugin, modules) | unreal-specialist |
| General architecture review | unreal-specialist |
