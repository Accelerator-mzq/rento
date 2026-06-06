# ADR-0001: UObject 宿主与生命周期边界

## Status

Accepted

> 2026-06-06 由 msc（用户）裁定 §Decision 的 DECISION FORK，确认 per-match 服务挂
> **UWorldSubsystem** + 跨局持久挂 **UGameInstanceSubsystem** + 联网 game state 留 Full Vision
> （GameState）。本 ADR 是依赖根，已最先 Accepted——解锁 ADR-0002/0003/0004/0005 与引用它的全部 story。

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

msc（用户）+ Technical Director（主笔）— 2026-06-06 Accepted

## Summary

为对局期运行态服务（回合状态机2 / 经济5 / 地产6 / 事件7 / 骰子RNG3 / 棋盘DA持有 /
GameStateSnapshot 生成方）裁定统一的 UObject 宿主形态与生命周期边界。决定：per-match
服务挂 **UWorldSubsystem**（一局=World 边界、PIE 隔离、`OnWorldBeginPlay` 注入 Seed），
跨局持久（Save/Audio/Setup 入口）挂 **UGameInstanceSubsystem**，联网复制的 game state
留待 Full Vision 走 GameState——不在 MVP 用 UActorComponent 形态。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7（Blueprint 为主 + C++ 框架） |
| **Domain** | Core / Scripting（UObject 生命周期、Subsystem 框架） |
| **Knowledge Risk** | **LOW** — Subsystem 框架（`UWorldSubsystem`/`UGameInstanceSubsystem`/`Initialize`/`Deinitialize`/`OnWorldBeginPlay`/`ShouldCreateSubsystem`）为 UE 4.22+ 引入、pre-5.3 稳定 API，落在模型训练数据内 |
| **References Consulted** | `docs/engine-reference/unreal/current-best-practices.md`（Subsystem 无专项条目——按引擎通用 Subsystem 生命周期）；`docs/engine-reference/unreal/breaking-changes.md`（无 Subsystem 破坏性变更命中）；`architecture.md` §2.1 模块①、§8 ADR-0001 条目、§D.4 启动序列 |
| **Post-Cutoff APIs Used** | **None** — 本 ADR 不依赖任何 5.4–5.7 新增 API。`UWorldSubsystem`/`UGameInstanceSubsystem` 自 UE4.22/4.24 起稳定。`FStreamableManager` 异步加载（ADR-0002 用）须由 ADR-0002 自行核验 5.4+ `UAssetManager` 变化，**不在本 ADR 范围** |
| **Verification Required** | ① `-nullrhi` headless 下 `UWorldSubsystem::Initialize/Deinitialize` 随 PIE World 创建/销毁正确触发（门控 dice 确定性 fixture 跑绿）；② `OnWorldBeginPlay` 在 PIE Stop→Start 之间确实重新触发（Seed 重注入正确性）；③ `Deinitialize()` 中 `CancelHandle` 后无悬挂异步回调写空棋盘 |

> **Note**: Knowledge Risk = LOW。Subsystem 框架稳定，引擎升级时本 ADR 一般无需重裁；但
> 若未来引入 World Partition / streaming level 边界（MVP 单关卡不涉及），须重新评估
> "一局 == 一个 World" 这一前提。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | **None（依赖根，须最先 Accepted）** |
| **Enables** | ADR-0002（棋盘 DA 持有者宿主）· ADR-0003（Event Bus delegate 注册/重绑宿主）· ADR-0004（RNG 服务宿主 + Seed 注入时机）· ADR-0005（SaveGame 宿主 + 读档重建时序）· ADR-0006（GameStateSnapshot 生成方宿主，间接）· ADR-0008（HUD 状态机 `IGameClock` DI 前提） |
| **Blocks** | 全部 MVP 实现 story（#1/2/3/5/6/7/8/9/10）——`/story-readiness` 在本 ADR 进入 Accepted 前对引用宿主的 story 自动阻塞 |
| **Ordering Note** | Sprint 0 第一份。Accepted 后 0002/0003/0004/0005 可由 lead-programmer/engine-programmer 并行起草（互不触同文件） |

## Context

### Problem Statement

16 个 Approved GDD 反复把"这个状态/服务挂在哪个 UObject 上、何时创建/销毁、如何注入
Seed 与 `IGameClock`"defer 到"架构期 ADR"，并标为**下游开工硬前提**（见 §GDD Requirements
Addressed 的 OQ 台账）。这些 OQ 各自指向同一个根问题：**对局期服务的 UObject 宿主形态未定**。
不裁定的代价：

- AI(10) 无法实现——`GameStateSnapshot` 注入面（OQ-AI-3/OQ-1⑦）依赖宿主提供的 DI 锚点；
- HUD headless 测试（AC-12/24a/30/57）false-green——状态机若挂 UMG `NativeTick`，`-nullrhi`
  下不触发；须抽独立 UObject + `IGameClock` DI，而"独立 UObject"挂哪须本 ADR 定；
- 存档原子性（OQ-Save-1）/ 棋盘 DA 防 GC（OQ-6）/ RNG Seed 注入时机（dice OQ-1）三者
  共享同一宿主生命周期假设，必须**一处统一裁定**，否则各系统各挂各的，读档重绑/PIE 隔离
  破裂。

本决策必须现在做：它是依赖图的根，阻断全部 Sprint 0 之后的实现。

### Current State

`docs/architecture/` 下尚无任何 ADR；`technical-preferences.md` 的 Architecture Decisions
Log 为空。`architecture.md`（v1.0 Approved）§2.1 已把宿主收敛为 Foundation 模块①，并在
§8 给出**推荐拓扑**（per-match→`UWorldSubsystem`，cross-level→`UGameInstanceSubsystem`），
但显式标注存在一处**未决张力**：board OQ-6 倾向 `UGameInstanceSubsystem`（DA 跨局复用、
避免反复加载），而 World 边界提供更干净的 PIE 隔离与一局生命周期。本 ADR 须 reconcile
此张力并把推荐拓扑正式定为决策。

### Constraints

- **引擎**：UE5.7，Blueprint 为主 + C++ 框架。Subsystem 框架 pre-5.3 稳定（知识风险 LOW）。
- **可测性硬线**（coding-standards / 架构原则3 DI over Singleton）：被测逻辑须能在
  `-nullrhi` headless 脱离渲染运行；状态机时序须经 `IGameClock` 注入而非直读
  `GetWorld()->GetTimeSeconds()`。宿主须提供 DI 注入点。
- **确定性硬线**（架构原则2）：RNG Seed 须在对局开始的确定时机注入（`OnWorldBeginPlay`），
  跨平台同源可重放。
- **无环 + 单一 owner**（架构原则5）：宿主只持生命周期与 DI 注入点，**不持游戏状态语义**——
  状态语义仍归各 owner GDD。宿主裁定不得改任何已定的 owner 关系或函数签名（§API Boundaries
  明示"ADR 不改函数签名，只决定它们挂在哪个 UObject 上"）。
- **PIE 隔离**：编辑器内反复 Play/Stop 不得残留上一局状态、不得悬挂异步加载回调写已销毁对象。
- **Full Vision 联网预留**（不在 MVP 实现）：状态须能平滑迁往 `GameState`/`PlayerState`
  复制；本 ADR 的 MVP 形态不得做出阻断该迁移的设计。

### Requirements

- 为以下 5 类宿主目标各裁定挂载点：① 棋盘 DA 持有者 ② RNG 服务 ③ owner map / 各运行态
  服务 ④ 回合状态机 ⑤ GameStateSnapshot 生成方。
- per-match 服务生命周期严格等于"一局"边界，PIE Stop 即销毁、Start 即重建。
- 提供统一 DI 注入点（`IGameClock`、Seed、mock 破产9 等），供 headless fixture 注入。
- `Initialize()` 中发起的异步加载 handle，须在 `Deinitialize()` 显式 `CancelHandle`
  （防 PIE Stop 后回调写空棋盘）。
- 状态机**禁** `FTimerHandle`/Latent Action，用 `ETurnPhase` 枚举 + delegate 推进
  （可序列化、读档 `switch(CurrentPhase)` 重入——player-turn EC L400 / B-R5-5 钉死）。

## Decision

> **本节列出真分叉的 2–3 选项逐个代价 + 推荐 + 推荐理由。** per-match 宿主形态的三选一
> 已于 2026-06-06 由 msc 裁定采纳 TD 推荐方案，以下为完整论证（备选分析保留作记录）。

**裁定（Accepted 2026-06-06）**：按生命周期边界**分类挂载**，而非全项目单一宿主：

| 宿主目标 | 挂载点（Accepted） | 生命周期边界 | 理由 |
|---------|-----------|-------------|------|
| ① 棋盘 DA 持有者（Board DA Holder） | **UWorldSubsystem** | 一局 | DA 引用随局加载/释放；`Initialize` 发起 `FStreamableManager` 异步加载、`Deinitialize` `CancelHandle`（OQ-6 防 GC + 防空棋盘）。详见下方"OQ-6 张力 reconcile" |
| ② RNG 服务（DiceService，系统3） | **UWorldSubsystem** | 一局 | `OnWorldBeginPlay` 注入 Seed（确定性原则2）；一局一流，PIE 隔离保证两局互不串流 |
| ③ owner map / 经济5 / 地产6 / 事件7 等运行态服务 | **UWorldSubsystem** | 一局 | 全部对局态随局生灭；PIE Stop 即清空，无残留 |
| ④ 回合状态机（系统2 spine-owner） | **UWorldSubsystem** | 一局 | `ETurnPhase` 枚举驱动；状态机抽为宿主持有的成员或独立 UObject（HUD 状态机经 `IGameClock` DI），禁 Latent |
| ⑤ GameStateSnapshot 生成方 | 回合2 宿主内（**UWorldSubsystem**） | 一局 | 回合2 在 AI 决策阶段生成只读快照；生成方 = 回合2 宿主，无独立宿主 |
| —— 跨局持久：Save/Load 框架(21)、Audio(22) 资产/混音、Setup/主菜单(20) 入口 | **UGameInstanceSubsystem** | 跨关卡 | 存档槽访问、音频总线、`StartNewGame(FGameSetupConfig)` 入口须跨局存活；`SaveGameToSlot` 调用方挂此处 |
| —— Full Vision 联网 game state | **GameState / PlayerState**（预留，MVP 不实现） | 复制域 | MVP 状态以普通 `UPROPERTY` 承载于 World Subsystem，预留迁往 GameState 复制；**MVP 不做 UActorComponent 形态** |

**OQ-6 张力 reconcile（board `UGameInstanceSubsystem` 倾向 vs World 边界 PIE 隔离）**：
采**职责拆分**化解——`DA_Board`（不可变静态资产）由 `UAssetManager`/`FStreamableManager`
缓存，跨局复用、不随局释放底层资产（满足 OQ-6 "避免反复加载"诉求）；但**"当前局正在用
哪块棋盘"这个引用 + 解析后的 tile 访问服务**挂 per-match World Subsystem（满足 PIE 隔离）。
即：**资产缓存在 GameInstance 级 AssetManager，棋盘服务实例在 World 级**。二者不矛盾——
OQ-6 想避免的是磁盘反复 IO，World Subsystem 想要的是实例隔离，拆开后两全。

### Architecture

```
┌─ UGameInstanceSubsystem（跨关卡，进程内单例，跨局存活）──────────────────┐
│  · SaveLoadSubsystem(21)   SaveGameToSlot/LoadGameFromSlot                │
│  · AudioSubsystem(22)      MetaSound 资产 / 三路混音总线                   │
│  · GameSetupSubsystem(20)  StartNewGame(const FGameSetupConfig&) 入口      │
│  · (UAssetManager 缓存 DA_Board 底层资产 — 跨局不释放)                     │
└───────────────────────────────▲──────────────────────────────────────────┘
                                 │ StartNewGame → 创建 World → 触发下方 Initialize
┌─ UWorldSubsystem（一局 = 一个 World，PIE Stop 即 Deinitialize）───────────┐
│  Initialize(Collection):  DI 注入点（IGameClock / mock）就绪              │
│  OnWorldBeginPlay():       RNG.SetSeed(...)  注入 Seed（确定性起点）       │
│  ┌────────────┬───────────┬──────────┬──────────┬─────────────────────┐   │
│  │ BoardHolder│ DiceRNG(3)│ Economy(5)│ Turn(2)  │ Property6/Event7/    │   │
│  │ (解析DA)   │ FRandom   │           │ 状态机   │ Building8/Bankruptcy9│   │
│  └────────────┴───────────┴──────────┴──────────┴─────────────────────┘   │
│  Deinitialize():  CancelHandle(异步加载)  → 防 PIE Stop 写空棋盘           │
└───────────────────────────────────────────────────────────────────────────┘
        ▲ 订阅 owner delegate（不写状态）
┌─ Presentation（HUD16/卡17/VFX19）─ 由 PlayerController/Widget 持有 ────────┐
│  HUD 时序状态机 = 独立 UObject + IGameClock DI（headless 可注入 mock 时钟）│
└───────────────────────────────────────────────────────────────────────────┘
```

### Key Interfaces

```cpp
// 一局期服务宿主基模式（伪代码，签名不在本 ADR 范围——见 §API Boundaries）
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

// DI 注入点（架构原则3）——宿主提供，headless fixture 可注入 mock
class IGameClock { public: virtual double NowSeconds() const = 0; };
// 生产实现读 World time；测试实现返回受控递增值（HUD AC-12/24a/30/57 headless 前提）
```

### Implementation Guidelines

1. **per-match 服务一律继承 `UWorldSubsystem`**；`ShouldCreateSubsystem` 排除 editor-preview
   World，避免编辑器误建对局态。
2. **Seed 注入唯一时机 = `OnWorldBeginPlay`**（不在 `Initialize`——此时玩家配置尚未由
   `StartNewGame` 落地）。RNG 默认种子恒 0、禁 lazy-init（随 ADR-0004）。
3. **异步加载纪律**：`Initialize` 中 `FStreamableManager::RequestAsyncLoad` 取得的
   `TSharedPtr<FStreamableHandle>` 须存为成员，`Deinitialize` 显式 `CancelHandle()`。
   PIE Stop→回调悬挂写已 GC 对象是本项目登记的具体地雷（OQ-6 / player-turn OQ-1⑩ GC 陷阱）。
4. **状态机禁 Latent**：`ETurnPhase` 枚举字段 + delegate 推进，禁 `FTimerHandle` /
   Blueprint `Delay`/`WaitForEvent`（读档 `switch(CurrentPhase)` 重入；player-turn EC L400 /
   AC-43 append-only 枚举 / B-R5-5 钉死）。
5. **宿主不持状态语义**：World Subsystem 持服务实例与生命周期，但 `Cash`/`house_count`/
   owner map 的写语义仍归各 owner（§Module Ownership 不变式3）。宿主裁定不改任何 owner 关系。
6. **Full Vision 迁移预留**：MVP 状态以普通 `UPROPERTY` 承载，字段命名与结构对齐
   `PlayerState`/`GameState`，便于联网期平移；**不**在 MVP 引入 `UActorComponent` 形态
   （见 Alternative 2 拒绝理由）。

## Alternatives Considered

### Alternative 1: 全项目单一 UGameInstanceSubsystem（per-match 也挂 GameInstance）

- **Description**: 回合/经济/地产/RNG 等对局态全挂 `UGameInstanceSubsystem`，进程内单例跨局存活，开局时手动 reset。
- **Pros**: 单一宿主层级，board OQ-6 "DA 跨局复用避免反复加载" 天然满足；无 World 创建/销毁时序顾虑。
- **Cons**: **PIE 隔离破裂**——编辑器反复 Play/Stop 须手动 reset 全部对局态，漏一个字段即跨局污染（回归测试噩梦）；World 生命周期事件（`OnWorldBeginPlay`）拿不到，Seed 注入时机被迫挪到自定义 `StartNewGame`，与 World 解耦后 headless fixture 须自造 World mock；与"一局 = 一个 World 边界"的心智模型背离。
- **Estimated Effort**: 与推荐相当，但 reset 逻辑与污染防护是持续维护税。
- **Rejection Reason**: PIE 隔离是日常开发与 headless 回归的根基；手动 reset 全状态是易漏的高风险模式。OQ-6 的合理诉求已由"AssetManager 缓存底层 DA + World 级棋盘服务实例"职责拆分满足，无须为它牺牲全局隔离。

### Alternative 2: GameState UActorComponent（MVP 即用联网友好形态）

- **Description**: 对局态作为 `AGameState` 上的 `UActorComponent`，MVP 即采用最终联网拓扑。
- **Pros**: 直达 Full Vision 联网形态，无后续迁移；`GameState` 天然每局新建（随 World）。
- **Cons**: **过早复杂度**——MVP 无复制需求，却要承担 Component 注册/Owner 引用/Actor 生命周期时序的额外面；`UActorComponent` DI 注入点不如 Subsystem 的 `Collection.InitializeDependency` 顺手；Subsystem 的自动单例发现（`GetSubsystem<T>()`）比手动找 Component 引用更稳。违反决策框架"Simplicity：最简可行解"。
- **Estimated Effort**: 高于推荐——MVP 阶段为不需要的联网形态付时序复杂度税。
- **Rejection Reason**: Reversibility 足够——推荐方案把状态以普通 `UPROPERTY` 承载、字段对齐 `GameState`/`PlayerState`，Full Vision 迁移是"把 World Subsystem 字段搬上 GameState 复制"的机械工作，不是重写。MVP 不为尚未到来的需求买单。联网仍是全项目最大技术风险，留待 ADR（Full Vision 专项）专门裁定。

## Consequences

### Positive

- 一处统一裁定关闭 8 条 GDD 登记的"待 ADR"宿主债（dice OQ-1 / board OQ-6 持有者分量 /
  player-turn OQ-1 状态机宿主 / property OQ-Prop-1 / tile-events OQ-Event-7 / save OQ-Save-1
  宿主分量 / HUD OQ-HUD-2 状态机 UObject / AI OQ-AI-3 注入面），解锁 0002–0005 并行起草。
- PIE 隔离 + `OnWorldBeginPlay` Seed 注入 = dice 确定性 fixture 可在 `-nullrhi` 跑绿
  （成功判据③）；`IGameClock` DI 锚点落地 = HUD AC-12/24a/30/57 摆脱 NativeTick false-green。
- 异步加载 `CancelHandle` 纪律消除 PIE Stop 写空棋盘的具体地雷。
- World 级隔离使每局确定性可独立重放，为 Full Vision 联网状态同步打基础。

### Negative

- per-match 与 cross-level 两类宿主并存，programmer 须清楚"这个服务该挂哪层"——本 ADR
  的分类表是唯一裁判，须在 control-manifest 固化。
- OQ-6 的职责拆分（AssetManager 缓存 vs World 服务实例）引入一处额外约定，须 ADR-0002
  在棋盘容器决策中承接落实。

### Neutral

- 状态机禁 Latent、用枚举驱动——这是 player-turn 已钉死的约束，本 ADR 只是确认其宿主落点，
  非新增负担。
- Full Vision 联网形态留待专项 ADR，本决策不预判其拓扑（仅保证 MVP 不阻断迁移）。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| PIE Stop 后异步加载回调写已 GC 棋盘宿主 | 中 | 高（空棋盘崩溃） | `Deinitialize` 显式 `CancelHandle`；Verification Required ③ headless 验证 |
| World Subsystem 在 editor-preview World 误建对局态 | 中 | 中 | `ShouldCreateSubsystem` 排除非游戏 World |
| MVP 状态结构未对齐 GameState，Full Vision 迁移变重写 | 低 | 中 | 字段以普通 UPROPERTY 承载、命名对齐 PlayerState/GameState；迁移评估留联网专项 ADR |
| `OnWorldBeginPlay` 在 PIE Stop→Start 之间未重触发致 Seed 不重注 | 低 | 高（确定性破裂） | Verification Required ② headless 验证重触发 |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | N/A（地基） | Subsystem 查找 `GetSubsystem<T>()` O(1) 哈希，非热路径 | 16.6 ms / 60 FPS |
| Memory | N/A | per-match Subsystem 随局生灭，无跨局泄漏；DA 底层资产 AssetManager 缓存（一份） | 待目标硬件确定 |
| Load Time | N/A | `FStreamableManager` 异步加载 DA，不阻塞主线程 | 待定 |
| Network | N/A（MVP 无） | World Subsystem 状态预留迁 GameState 复制 | Full Vision 专项 |

> 本 ADR 是框架地基，自身无可观测帧开销；其性能意义在于**保证下游可在 C++ 热路径
> 实现规则逻辑**（架构原则4），并提供 headless 可测性（确定性回归门）。

## Migration Plan

无既有系统迁移——本 ADR 是从零起的依赖根。落地步骤：

1. 建 `UMatchSubsystemBase : UWorldSubsystem` 基类，固化 `ShouldCreateSubsystem` /
   `OnWorldBeginPlay` Seed 注入 / `Deinitialize` `CancelHandle` 模式。验证：headless
   下 PIE Start/Stop 触发 Initialize/Deinitialize 各一次。
2. 建 `IGameClock` 接口 + 生产实现（读 World time）+ 测试实现（受控时钟）。验证：HUD
   状态机 fixture 注入 mock 时钟，`-nullrhi` 推进相位。
3. 跨局服务（Save/Audio/Setup）建 `UGameInstanceSubsystem`。验证:`StartNewGame` 入口
   跨 PIE 局存活。

**Rollback plan**: 本 ADR 已 Accepted（2026-06-06）。若实现期发现
World Subsystem 致命缺陷，回退路径 = 改挂 GameInstance（Alternative 1）+ 手动 reset；
因状态以普通 UPROPERTY 承载、未绑死 World 事件语义，回退成本中等（Reversibility 可控）。

## Validation Criteria

- [ ] dice 确定性 fixture（固定 Seed → 固定序列）在 `-nullrhi` headless 跑绿
      （证明 World Subsystem + `OnWorldBeginPlay` Seed 注入可测）
- [ ] PIE Play→Stop→Play 三轮，第二局无第一局残留状态（PIE 隔离）
- [ ] PIE Stop 时异步加载中途，无悬挂回调崩溃（`CancelHandle` 生效）
- [ ] HUD 状态机注入 mock `IGameClock`，headless 下相位按受控时钟推进（AC-12/24a/30/57 前提）
- [ ] `/story-readiness` 对 #1/2/3/5 story 不再因"无 ADR / Proposed ADR"阻塞（本 ADR Accepted 后）

## GDD Requirements Addressed

| GDD Document | System | Requirement (OQ) | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/dice.md` | 骰子(3) | OQ-1 / IG-3 — RNG 服务宿主 + Seed 注入位置 | DiceService 挂 UWorldSubsystem，Seed 经 `OnWorldBeginPlay` 注入（确定性起点） |
| `design/gdd/board-data.md` | 棋盘(1) | OQ-6（持有者分量）— DA 持有者宿主 + 防 GC 热切换 | DA 底层资产 AssetManager 跨局缓存 + 棋盘服务实例挂 World Subsystem；`Deinitialize` `CancelHandle` 防空棋盘（reconcile OQ-6 GameInstance 倾向 vs PIE 隔离） |
| `design/gdd/player-turn.md` | 回合(2) | OQ-1 — 回合状态机宿主 + `ETurnPhase` 驱动禁 Latent | 状态机挂 World Subsystem，`ETurnPhase` 枚举 + delegate 推进，禁 FTimerHandle/Latent（EC L400 / B-R5-5） |
| `design/gdd/property-ownership.md` | 地产(6) | OQ-Prop-1 — owner map 生命周期 | owner map 随局生灭，挂 World Subsystem |
| `design/gdd/tile-events.md` | 事件格(7) | OQ-Event-7 — 非重入/宿主/RNG（指向 OQ-1） | 事件格服务挂 World Subsystem，RNG 经 DiceService(3) 隔离流 |
| `design/gdd/save-load.md` | 存档(21) | OQ-Save-1（宿主分量）— SaveGame 宿主 | SaveLoad 框架挂 UGameInstanceSubsystem（跨局存档槽访问）；序列化契约/原子写盘细节归 ADR-0005 |
| `design/gdd/hud.md` | HUD(16) | OQ-HUD-2 — 状态机抽独立 UObject + `IGameClock` DI | 提供 `IGameClock` DI 注入点；HUD 时序状态机为独立 UObject，headless 可注入 mock 时钟 |
| `design/gdd/ai-opponent.md` | AI(10) | OQ-AI-3 / OQ-1⑦ — GameStateSnapshot 注入面前提 | snapshot 生成方 = 回合2 World Subsystem，提供 AI 注入面；struct 字段构成归 ADR-0006 |
| `architecture.md` §8 | 开局入口 | OQ-Lobby-1 协同 — `StartNewGame` 入口宿主 | `StartNewGame(const FGameSetupConfig&)` 入口挂 GameSetup UGameInstanceSubsystem（接口名冻结 + USTRUCT 归属随回合2，本 ADR 只定宿主） |

## Related

- `docs/architecture/architecture.md` §2.1 Foundation 模块①、§8 Required ADR 清单、§D.4
  启动序列、§Architecture Principles 原则2/3/4/5（本 ADR 是其宿主落点）
- **Enables**: ADR-0002（棋盘容器）· ADR-0003（事件总线）· ADR-0004（确定性 RNG）·
  ADR-0005（存档契约）· ADR-0006（GameStateSnapshot）· ADR-0008（HUD 驱动 + IGameClock DI）
- `.claude/docs/technical-preferences.md` Architecture Decisions Log（Accepted 后登记本 ADR）
- 代码文件链接（实现后补）
