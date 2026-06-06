---
Epic: foundation-framework
Status: Ready
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 001 — UMatchSubsystemBase per-match 宿主基类与生命周期

## Context

- **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 模块①）
- **Requirement TR-ID**: 框架面支撑 TR-turn-001（PlayerState 运行时宿主 + 随局生灭，挂 World Subsystem）、TR-dice-008（DiceService UE 宿主形态，与 player-turn 同生命周期层）、TR-board-007（运行时 DA 持有者宿主）。本 story 提供这些 TR 的 **per-match 宿主框架底座**；各 TR 的功能 owns 归各系统 epic。
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期边界**：per-match 服务一律挂 `UWorldSubsystem`（一局=World 边界、PIE 隔离）；`ShouldCreateSubsystem` 排除 editor-preview World；宿主只持生命周期与 DI 注入点，**不持游戏状态写语义**。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（Subsystem 框架 UE4.22+ 引入、pre-5.3 稳定，落在模型训练数据内）
- **Engine Notes（ADR-0001 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None** — `UWorldSubsystem`/`UGameInstanceSubsystem`/`Initialize`/`Deinitialize`/`OnWorldBeginPlay`/`ShouldCreateSubsystem` 自 UE4.22/4.24 起稳定，无 5.4–5.7 新增依赖。
  - Verification Required（本 story 部分前置，完整验证见 story-007）：① `-nullrhi` headless 下 `Initialize/Deinitialize` 随 PIE World 创建/销毁正确触发。
  - 若未来引入 World Partition / streaming level 边界（MVP 单关卡不涉及），须重新评估"一局 == 一个 World"前提。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required**: per-match 服务一律继承 `UWorldSubsystem`；`ShouldCreateSubsystem` 排除 editor-preview World，避免编辑器误建对局态（source: ADR-0001）。per-match 服务挂 `UWorldSubsystem`（棋盘 DA 持有者 / RNG 服务 / owner map / 经济5 / 地产6 / 事件7 / 回合状态机 / GameStateSnapshot 生成方）；生命周期边界=一局，PIE Stop 即销毁、Start 即重建（source: ADR-0001）。宿主不持状态写语义：World Subsystem 持服务实例与生命周期，但 `Cash`/`house_count`/owner map 的写语义仍归各 owner（source: ADR-0001）。
  - **Required**: Full Vision 迁移预留——MVP 状态以普通 `UPROPERTY` 承载，字段命名与结构对齐 `PlayerState`/`GameState`，便于联网期平移（source: ADR-0001）。
  - **Forbidden**: Never 把 per-match 对局态全挂 `UGameInstanceSubsystem`（PIE 隔离破裂、World 生命周期事件拿不到、Seed 注入时机被迫挪走）（source: ADR-0001）。Never 在 MVP 用 `UActorComponent`/`AGameState` 上的 Component 形态承载对局服务（过早复杂度，违反 Simplicity）（source: ADR-0001）。
  - **Guardrail**: Subsystem 查找 `GetSubsystem<T>()` O(1) 哈希，非热路径；per-match Subsystem 随局生灭，无跨局泄漏；CPU 帧预算 16.6 ms / 60 FPS（source: ADR-0001）。
  - **Global**: UObject 引用成员用 `TObjectPtr<T>` + `UPROPERTY()` 防 GC，不用裸指针/`TSharedPtr<T>`（source: current-best-practices）。

## Acceptance Criteria

- [ ] AC-1: 存在 `UMatchSubsystemBase : public UWorldSubsystem` 基类，固化 per-match 宿主模式（`ShouldCreateSubsystem` / `Initialize` / `OnWorldBeginPlay` / `Deinitialize` 四 override 骨架）。
- [ ] AC-2: `ShouldCreateSubsystem(UObject* Outer)` 对 editor-preview / 非游戏 World 返回 false，仅游戏局 World 返回 true（不在编辑器预览 World 误建对局态）。
- [ ] AC-3: per-match 服务（DiceService / 回合状态机 / Economy / Board Holder 等）经本基类或直接继承 `UWorldSubsystem`，**无一挂 `UGameInstanceSubsystem`**（per-match 类目录内 grep 不出 `UGameInstanceSubsystem` 基类）。
- [ ] AC-4: 在 `-nullrhi` headless 下，对游戏 World 触发一次 PIE Start，本基类派生子类 `Initialize` 恰调用 1 次；PIE Stop 时 `Deinitialize` 恰调用 1 次。
- [ ] AC-5: 基类不声明任何游戏状态写字段（`Cash`/`house_count`/owner map 等）；仅持服务实例引用（`TObjectPtr` + `UPROPERTY()`）与 DI 注入点。
- [ ] AC-6: 状态机相关派生类不使用 `UActorComponent` 形态承载对局服务。

## Implementation Notes

> 逐字保真自 ADR-0001 Implementation Guidelines #1/#5/#6 + Key Interfaces，不改写语义。

1. **per-match 服务一律继承 `UWorldSubsystem`**；`ShouldCreateSubsystem` 排除 editor-preview World，避免编辑器误建对局态。
2. 基类骨架（ADR-0001 Key Interfaces 伪代码）：
   ```cpp
   class UMatchSubsystemBase : public UWorldSubsystem
   {
       // 仅当本 World 为游戏局（非编辑器预览）时创建
       virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
       virtual void Initialize(FSubsystemCollectionBase& Collection) override;
       // ↑ 取得依赖 Subsystem、建立 IGameClock 注入点；此时 World 尚未 BeginPlay，禁读玩家态
       virtual void OnWorldBeginPlay(UWorld& InWorld) override;
       // ↑ 确定性起点：RNG.SetSeed(...)；广播首个 OnTurnStarted 由回合2 在此后驱动
       virtual void Deinitialize() override;
       // ↑ StreamableHandle->CancelHandle()；解绑本宿主持有的 delegate（与 ADR-0003 协同）
   };
   ```
3. **宿主不持状态语义**：World Subsystem 持服务实例与生命周期，但 `Cash`/`house_count`/owner map 的写语义仍归各 owner（§Module Ownership 不变式3）。宿主裁定不改任何 owner 关系或函数签名（ADR 不改函数签名，只决定它们挂在哪个 UObject 上）。
4. **Full Vision 迁移预留**：MVP 状态以普通 `UPROPERTY` 承载，字段命名与结构对齐 `PlayerState`/`GameState`，便于联网期平移；**不**在 MVP 引入 `UActorComponent` 形态。
5. `Initialize` 此时 World 尚未 BeginPlay，禁读玩家态；Seed 注入时机不在 `Initialize`（见 story-002/007 与 ADR-0004，本 story 只搭骨架不注入 Seed）。

## Out of Scope

- `IGameClock` 接口与生产/测试实现 → **story-002**。
- 异步加载 handle 存储 + `Deinitialize` `CancelHandle` 的具体实现 → **story-003**。
- 跨局 `UGameInstanceSubsystem`（Save/Audio/Setup）宿主 → **story-004**。
- PIE Stop→Start 隔离 / `OnWorldBeginPlay` 重触发的完整 headless 验证 → **story-007**。
- 任何具体游戏状态字段、Seed 数值注入、delegate 声明。

## QA Test Cases

> 本 story 为 Integration（宿主生命周期跨 World 边界）。每 AC 一条 Given/When/Then；headless `-nullrhi`。

- **TC-1 (AC-1/AC-2)**: GIVEN 一个测试用游戏 World 与一个 editor-preview World，WHEN 查询 `UMatchSubsystemBase::ShouldCreateSubsystem`，THEN 游戏 World 返回 true、editor-preview World 返回 false。
- **TC-2 (AC-3)**: GIVEN per-match 服务类集合，WHEN 静态检查各类基类，THEN 全部继承 `UWorldSubsystem`（或本基类），无一继承 `UGameInstanceSubsystem`。
- **TC-3 (AC-4)**: GIVEN `-nullrhi` headless，WHEN 对游戏 World 触发 PIE Start 然后 Stop，THEN 派生子类 `Initialize` 计数==1、`Deinitialize` 计数==1。
- **TC-4 (AC-5)**: GIVEN 基类 reflection 字段表，WHEN 枚举 `UPROPERTY`，THEN 无 `Cash`/`house_count`/owner-map 类游戏状态写字段，仅服务引用与 DI 注入点。
- **Edge cases**: 非游戏 World（utility/preview）下 `Initialize` 不应被调（`ShouldCreateSubsystem` 拦截）；同一进程内连续两局 World，第二局 `Initialize` 仍恰 1 次（无跨局残留实例）。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/foundation/match_subsystem_base_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: 无（依赖根，ADR-0001 已 Accepted）。
- **Unlocks**: story-002（IGameClock DI 挂本基类 Initialize）、story-003（异步加载纪律挂本基类 Deinitialize）、story-005（Event Bus 订阅生命周期锚本基类 Init/Deinit）、story-007（PIE 隔离验证基于本基类）。
