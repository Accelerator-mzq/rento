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
- [No ADRs yet — use /architecture-decision to create one]

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
