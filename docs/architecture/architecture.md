# 骰子大亨 (Dice Tycoon) — Master 架构文档（Architecture）

> **本文件是项目的权威架构总纲。** 它把 16 个 Approved MVP 系统 GDD 聚合成一致的分层架构、模块所有权、数据流、API 边界、引擎集成策略与待裁 ADR 清单。GDD 是设计的权威源；本文件是**架构契约的权威源**。凡本文件与 owner GDD 冲突，以 owner GDD 为准，本文件须 propagate 修正。

---

## Document Status

| Field | Value |
|-------|-------|
| **Version** | 1.0 |
| **Date** | 2026-06-06 |
| **Status** | **Approved**（`TD-ARCHITECTURE` gate 自审通过 2026-06-06） |
| **Technical Director Sign-Off** | 2026-06-06 — **APPROVED**（16/16 GDD 系统全映射入层 · 依赖图无环〔5↔6/6↔8/2↔9 已打破〕· 引擎兼容 Input/UI 对照 engine-ref 核验 · 12 Required ADR 全识别 + 依赖拓扑 · 332 TR 域级覆盖 12/12） |
| **Lead Programmer Feasibility** | SKIPPED — Lean review mode（非 PHASE-GATE，依 `production/review-mode.txt`） |
| **Author** | Technical Director |
| **Engine** | Unreal Engine 5.7（Blueprint 为主 + C++ 框架） |
| **Target Platform** | PC（Steam），60 FPS / 16.6 ms 帧预算 |
| **GDDs Covered** | 16 个 Approved MVP 系统（#1/2/3/4/5/6/7/8/9/10/16/17/19/20/21/22） |
| **ADRs Referenced** | **12 Accepted**（ADR-0001~0012 全部 2026-06-06 Accepted；ADR-0012 CommonUI 含 HIGH post-cutoff 风险，实现首日须对照引擎源码核验签名） |
| **Source of Truth** | `design/gdd/systems-index.md`（Dependency Map / Inherited Test Obligations / registry）+ 16 个 Approved GDD |

> **核验声明**：本文件 Input/UI/Rendering/Audio 的 UE 5.7 范式均对照 `docs/engine-reference/unreal/modules/{input,ui,rendering,audio}.md` + `current-best-practices.md` + `deprecated-apis.md`（pinned UE 5.7，verified 2026-02-13）逐条核实，**未采信模型 ~5.3 训练记忆**。所有所有权裁定、接口签名、防环约束均对照各 GDD Dependencies 节与 systems-index registry 逐条核实，未臆造接口名。

---

## Engine Knowledge Gap Summary（引擎知识缺口小结）

UE 5.7（2025-11 发布）远超模型 ~5.3 训练记忆。本文件凡涉以下域均以 engine-reference 快照为准。各域知识风险分级与本项目采用的**混合 A/B 策略**：

| 引擎域 | 知识风险 | 本项目策略 | 关键反差 / 裁定 |
|--------|----------|-----------|-----------------|
| **Input** | 🔴 HIGH | **B 域：5.7 现代范式** Enhanced Input | legacy `BindAction`/`BindAxis` 已弃（deprecated-apis L11-18）；Input Action + IMC + `EnhancedInputComponent`，为手柄/客厅模式预留 |
| **UI** | 🔴 HIGH | **B 域：5.7 现代范式** UMG + CommonUI | `CommonActivatableWidget`/`CommonButtonBase` 手柄友好、禁 hover-only；`meta=(BindWidget)` BP↔C++ 绑定边界→ADR |
| **Rendering** | 🟡 MED | **A 域：逆默认做减法** | UE 5.7 默认开 Lumen/Nanite/Megalights/Substrate（面向复杂 3D）；卡通 2.5D **显式关闭**重型特性；Niagara 非 Cascade；卡通材质 legacy-vs-Substrate→ADR |
| **Audio** | 🟡 MED | **A 域：5.7 现代范式 + 减法** | MetaSound 非 Sound Cue（复杂逻辑 Sound Cue 已弃）；Sound Class/Submix 三路混音；2D 为主无空间化 |
| **Persistence** | 🟡 MED | 原生 `USaveGame` | `SaveGameToSlot/LoadGameFromSlot` pre-5.3 稳定；写盘原子性 + 宿主→ADR |
| **Physics** | 🟢 LOW | **A 域：Chaos 仅表现** | 骰子/棋子表现用 Chaos；点数由种子化 RNG 裁定，非物理 |
| **Navigation** | 🟢 LOW | **A 域：无 NavMesh** | AI 启发式打分无寻路；棋子沿固定格序移动 |
| **Networking** | 🟢 LOW（MVP 无） | **A 域：MVP 无，Full Vision 预留** | 状态承载 GameState/PlayerState、RNG 可重放、AI return-only 为联网预留 |
| **Animation** | 🟢 LOW | **A 域：无骨骼** | UMG Animation/Timeline/程序化 Tween，无 SkeletalMesh |

> **混合 A/B 总纲**：A 域（Rendering/Audio/Physics/Nav/Net/Anim）按 2.5D 回合制低需求**逆默认裁剪**；B 域（Input/UI）采 5.7 现代范式为手柄/客厅模式预留。

---

## System Layer Map（系统层映射）

> **Source of truth**: `design/gdd/systems-index.md`（Dependency Map / Category / Inherited Test Obligations / registry）+ 16 个 Approved MVP GDD
> **Scope**: 16 MVP 系统映射进 5 层，新增 4 个 Foundation 引擎集成模块；10 个 Alpha/Full Vision 系统轻量预留
> **风险标注口径**: Core/Foundation 层系统若触 HIGH/MED 引擎域（Input/UI/Rendering/Audio）→ inline `⚠ risk`

### 0. 分层原则（Layering Principles）

本架构在 systems-index 的 4 层设计分层（Foundation / Core / Feature / Presentation，外加 Polish）之上，**插入一层引擎集成 Foundation 模块**，把 GDD 里"谁拥有这个状态/事件"映射为"哪个 UE 宿主对象持有它"。映射规则：

1. **依赖方向 = 层方向**。下层不得依赖上层；同层可横向消费（经 snapshot/事件，非反向回调）。systems-index 已核实 **依赖图有向无环**（Circular Dependencies: None）。
2. **状态归属唯一**。每个运行态字段恰好一个 owner 系统持有（GDD registry 已注册）；跨系统读 = snapshot/pull 或订阅 multicast delegate，**禁反向写**。
3. **return-only 编排**。回合2 向下 invoke 9/10，被调方 return-only 不回调 2（防 2↔9、2↔10 环）。
4. **enable-not-own 横切**。Save(21)/Audio(22)/VFX(19) 不拥有被序列化/被表现的字段语义，只"采集→落盘 / 订阅→播放"。
5. **呈现层纯叶子**。HUD(16)/卡牌UI(17)/VFX(19)/Audio(22)/主菜单(20) 只读、订阅、显示、转发意图，不写状态、不驱动流程。

### 1. ASCII 层图

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  PLATFORM LAYER  (UE5.7 引擎能力 — 不写游戏代码,仅策略约束)                       │
│  Enhanced Input ⚠HIGH │ UMG+CommonUI ⚠HIGH │ Deferred Renderer(Lumen/Nanite/    │
│  Megalights=OFF)⚠MED │ MetaSound+Submix ⚠MED │ Chaos(仅表现) │ UBT/Content Pipe  │
└───────────────────────────────────▲────────────────────────────────────────────┘
                                     │ (引擎 API,经 Foundation 模块封装)
┌────────────────────────────────────┴───────────────────────────────────────────┐
│  FOUNDATION LAYER  (引擎集成 + 近零依赖框架 — "memory is the file/subsystem")      │
│  ┌─ 引擎集成模块(本架构新增,无 GDD owner,TD 拥有) ──────────────────────────┐  │
│  │  ① Subsystem 宿主    ② Event Bus(delegate 注册表)  ③ Save 框架(USaveGame) │  │
│  │  ④ Board DA Holder(DA_Board 解析/持有)                                      │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│  GDD 系统:  棋盘数据(1)   │   骰子(3) RNG服务   │   玩家与回合(2,依赖3)            │
└───────────────────────────────────▲────────────────────────────────────────────┘
                                     │
┌────────────────────────────────────┴───────────────────────────────────────────┐
│  CORE LAYER  (依赖 Foundation — 经济 bottleneck + 空间/所有权)                     │
│  经济与现金(5)⚠bottleneck  │  移动(4)  │  地产所有权(6)  │  事件格(7)               │
└───────────────────────────────────▲────────────────────────────────────────────┘
                                     │
┌────────────────────────────────────┴───────────────────────────────────────────┐
│  FEATURE LAYER  (依赖 Core — 增值机制)                                            │
│  建房升级(8)  │  破产与胜负(9, 2→9 return-only)  │  AI 对手(10,⚠消费几乎全部)      │
│  〔Alpha 预留: 交易(11) 拍卖(12) 命运之轮(13) 俄罗斯轮盘(14) 策略卡(15)〕            │
└───────────────────────────────────▲────────────────────────────────────────────┘
                                     │ (订阅 owner multicast delegate / 转发意图)
┌────────────────────────────────────┴───────────────────────────────────────────┐
│  PRESENTATION LAYER  (纯叶子,包裹 gameplay — 不写状态)                            │
│  HUD(16)⚠UI  │  地产卡UI(17)⚠UI  │  VFX反馈(19)⚠Render  │  主菜单大厅(20)⚠UI       │
│  ─── 横切(cross-cutting,非严格层): Save/Load(21)持久化 · Audio(22)⚠Audio ───      │
│  〔Alpha 预留: 交易/拍卖UI(18)〕  〔Full 预留: 联网(25) 地图编辑器(26)〕            │
└──────────────────────────────────────────────────────────────────────────────┘
```

> **横切层说明**：Save(21) 与 Audio(22) 在 systems-index 归 **Polish**，但二者是 *enable-not-own 横切关注点*（Save 横跨所有状态系统、Audio 订阅 dice3/turn2/move4/economy5/property6/building8/event7）。图中置于 Presentation 行下方独立带，表示其作用域横跨全部下层而非被 Presentation 依赖。

### 2. 分层系统表

#### 2.1 Foundation Layer — 引擎集成模块（本架构新增，TD 拥有，无 GDD）

这 4 个模块是 GDD 里反复出现的"待架构期裁定"OQ 的落点（OQ-1 GameStateSnapshot、OQ-Save-1 USaveGame 宿主、OQ-Event-7 RNG/宿主、OQ-HUD-2/3 绑定）。它们**没有 GDD owner**，由 Technical Director 经 ADR 拥有。

| 模块 | 所属层 | 模块边界（UE 落点） | 独占 owns | 引擎域 risk |
|------|--------|-------------------|-----------|-------------|
| **① Subsystem 宿主** | Foundation | `UGameInstanceSubsystem`（跨关卡：Save/Audio/Setup）+ `UWorldSubsystem`（对局生命周期：回合/经济/棋盘）。回合2/经济5 等运行态系统的 C++ 宿主 | 系统单例生命周期、`Initialize/Deinitialize` 时序、DI 注入点（`IGameClock` 等）。**不持游戏状态语义** | — （纯框架，但门控所有下游 headless 可测性） |
| **② Event Bus** | Foundation | owner GDD 声明的 multicast delegate 的注册/绑定约定（`OnRentPaid`/`OnPhaseChanged`/`OnDiceRolled`/`OnOwnershipChanged`/`OnBankruptcyDeclared`/`OnGameWon` 等，各 owner 持有、消费方订阅）。**非全局 event aggregator** — delegate 仍归各 owner 持有，Bus 只统一绑定/解绑（读档重绑、防双订阅 EC-8）纪律 | delegate 命名/payload 契约一致性、读档后 re-bind 时序、防双触发约定 | ⚠ MED（BP-vs-C++ delegate 暴露边界 → OQ-HUD-2/3、OQ-AI-3b ADR） |
| **③ Save 框架** | Foundation | `USaveGame` 派生 + 原子写盘（OQ-Save-1）。采集各状态系统 `Serialize` 契约（CR-3 清单）→ 落盘；读盘→重建→重绑（经 ② Event Bus） | 序列化容器格式、版本判据（F-2 严格相等不迁移）、round-trip identity 校验骨架。**不拥有任何被序列化字段语义**（enable-not-own） | — （磁盘 I/O，非渲染/输入域） |
| **④ Board DA Holder** | Foundation | 解析/持有 `DA_Board`（Data Asset），向棋盘数据(1)供 `GetTileData`/`GetTilesInGroup`/`GetTileWorldTransform`/环序 N。棋盘存 DA 引用 `BoardIdentifier` 而非全量布局（Save CR-2） | `DA_Board` 加载生命周期、tile 静态布局只读访问。运行态（owner map / house_count）**不在此** | — |

> **为何独立成模块**：systems-index 多处把"宿主/RNG/绑定/写盘原子性"标为 *架构期 ADR 前提*（OQ-1 是 AI 开工硬前提、OQ-Save-1、OQ-Event-7、OQ-HUD-2/3 是 AC-12/24a/30/57 headless 硬前提）。集中为 4 个 Foundation 模块，使 programmer 有明确实现锚点，且每个模块对应一份 ADR（见 §8）。

#### 2.2 Foundation Layer — GDD 系统（近零依赖框架）

| # | 系统 | 所属层 | 模块边界 | 独占 owns（数据/状态/行为） | 引擎域 risk |
|---|------|--------|----------|---------------------------|-------------|
| 1 | 棋盘数据 Board/Tile Data | Foundation | 经 ④ Board DA Holder 读 `DA_Board`；纯静态空间地基 | 格子布局/`TileType`/组成员/`RentTable` 档位结构（*值*归经济）/`GetTileWorldTransform`/环序 N。零依赖 | — |
| 3 | 骰子 Dice | Foundation | 种子化 RNG 服务（`RandomRange`/`RandomFloat01`），`FDiceRollResult{Die1,Die2,Total,bIsDouble}` | RNG 流（AI/VFX/Audio 须经此取随机，禁引擎全局 random；CR-5④ 流隔离）。**骰子点数由种子化 RNG 裁定，非 Chaos 物理**。零依赖 | — （Chaos 仅做骰子*表现*，不裁点数） |
| 2 | 玩家与回合管理 Player & Turn | Foundation | 回合状态机宿主（`UWorldSubsystem`）；流程框架 + 向下编排 | 回合顺序/`CurrentPlayerId`/`RollPhase`/`ConsecutiveDoubles`/`bIsBankrupt`（经9返回值写）/`PlayerState` 列表/监狱态字段。**depends-on: 3** | — |

#### 2.3 Core Layer（依赖 Foundation）

| # | 系统 | 所属层 | 模块边界 | 独占 owns（数据/状态/行为） | 引擎域 risk |
|---|------|--------|----------|---------------------------|-------------|
| 5 | 经济与现金 Economy & Cash | Core ⚠**bottleneck** | 租金/抵押/建房公式发源地；`Debit`/`Credit`/`is_insolvent`/F-9 `net_liquidation_value` | 每玩家 `Cash`、租金/发薪/抵押现金侧执行、估值口径。被 6 调（6→5）、被9调，**不反调 6/9**。depends-on: 1,2 | — |
| 4 | 移动 Movement | Core | 棋子推进；`TeleportTo`/`(passedGo,SalaryAmount)` push 经济（编排下） | 棋子 `CurrentTileIndex` 推进、`OnPawnMoveStarted`/`OnPawnLanded(arrivalContext)`。orchestrated→5（push 发薪），不成环。depends-on: 1,2,3 | — |
| 6 | 地产所有权 Property Ownership | Core | 买地/抵押事务 owner；`is_monopoly`/`station_count`/`utility_count` | owner map、垄断/抵押状态、买地/抵押事务（调5执行现金侧）。**house_count 归8不归6**（防6↔8环）。depends-on: 1,5 | — |
| 7 | 事件格 Tile Events | Core | 机会/命运/税/监狱/起点；牌堆（model B + Fisher-Yates 种子） | 牌堆状态/`EJailReason`/`EJailReleaseMethod`/`OnCardDrawn`。监狱*态字段*归回合2，7 拥*规则*。depends-on: 1,2,3,4,5,8(soft) | — |

#### 2.4 Feature Layer（依赖 Core）

| # | 系统 | 所属层 | 模块边界 | 独占 owns（数据/状态/行为） | 引擎域 risk |
|---|------|--------|----------|---------------------------|-------------|
| 8 | 建房升级 Building & Rent Scaling | Feature | 建/卖房；per-tile `house_count[0,5]` owner | per-tile `house_count`、`GetTotalHouseCount`/`GetTotalHotelCount`、`LiquidateAllBuildings`（银行破产无偿清算）。depends-on: 1,5,6 | — |
| 9 | 破产与胜负 Bankruptcy & Win | Feature | return-only 编排子程序；`ResolveBankruptcy(debtor,creditor,snapshot)→{eliminated,winnerId,rankingEntry}` | `net_worth` 估值、creditor 资产 in-kind 移交编排、排名。**2→9 invoke、9 绝不回调2**（防2↔9环）、向下调5/6/8。depends-on: 2,5,6,8 | — |
| 10 | AI 对手 AI Opponent | Feature ⚠**消费几乎全部** | player-turn CR-8 三 hook 背后决策；启发式加权打分无前瞻 | Buy/Build/Jail 决策逻辑、F-7 三档参数。**纯叶子消费者**（经 snapshot 读1/2/3/5/6/7/8，不回调2、不被反调）；随机走骰子3 隔离流（CR-5④）。depends-on: 1,2,3,5,6,7,8 | ⚠ MED（GameStateSnapshot 注入 = OQ-1 ADR 硬前提；BP-vs-C++ = OQ-AI-3b） |

> **Alpha 预留（轻量）**：交易(11,依赖5,6)/拍卖(12,依赖5,6)/命运之轮(13,依赖5,7)/俄罗斯轮盘(14,依赖2,9)/策略卡(15,依赖5,6,7)。均落 Feature 层，复用既有 Core 接口，**不引入新 Foundation 模块**（垂直切片复用 MVP 系统集 + 拍卖一项）。

#### 2.5 Presentation Layer（纯叶子，不写状态）

| # | 系统 | 所属层 | 模块边界 | 独占 owns（数据/状态/行为） | 引擎域 risk |
|---|------|--------|----------|---------------------------|-------------|
| 16 | HUD | Presentation | UMG+CommonUI；只读/订阅/显示玩家面板/现金/回合 | UMG widget 树、订阅→显示映射（`OnPhaseChanged`/`OnTurnStarted`/`OnBuildingAnnounced` 等）。无下游、无 registry 跨档事实 | ⚠ **HIGH**（UMG+CommonUI 手柄友好/禁 hover-only；BP-vs-C++ 绑定边界=OQ-HUD-2/3 ADR；渲染时刻 AC headless 陷阱） |
| 17 | 地产卡与卡牌 UI | Presentation | UMG；契据卡显示 + 建房/抵押意图转发 | 卡片 widget、显示映射（读1/6/8，house_count 取8防6↔8环）。转发意图不写状态 | ⚠ **HIGH**（UMG 绑定 OQ-PC-4 ADR；等宽字体/色组归 art-bible） |
| 19 | 游戏反馈 VFX | Presentation | Niagara 粒子 + Timeline/Tween；视觉 juice owner | 视觉 juice（掷骰/金币/建房/破产清算）、本地 `house_count_cache` delta、hop path 自建。订阅 owner 事件，各订各播 | ⚠ **MED**（Rendering: Lumen/Nanite/Megalights=OFF 卡通2.5D；Niagara 非 Cascade；胜利暖金 Post Process Material；legacy-vs-Substrate 卡通材质=ADR） |
| 20 | 主菜单与房间大厅 | Presentation | UMG+CommonUI；本地热座 2–4 配置→`FGameSetupConfig` 移交回合2 | setup 编辑态（席位/颜色/AI 难度选择 UI）、`BuildSetupConfig`。#20→#2 单向无环 | ⚠ **HIGH**（UMG+CommonUI；`StartNewGame(FGameSetupConfig)` 入口归回合2=OQ-Lobby-1） |

#### 2.6 横切层（cross-cutting，作用域横跨全部下层）

| # | 系统 | systems-index 层 | 模块边界 | 独占 owns（数据/状态/行为） | 引擎域 risk |
|---|------|------------------|----------|---------------------------|-------------|
| 21 | 存档/读档 Save/Load | Polish（横切） | 经 ③ Save 框架；采集→序列化→重建→重绑 | round-trip identity 校验、版本判据。**enable-not-own**：不拥有任何被序列化字段语义。depends-on: 1,2,5,6 | — （磁盘 I/O） |
| 22 | 音频 Audio SFX/BGM | Polish（横切） | MetaSound（非 Sound Cue）；订阅 owner 事件→播放+三路混音 | 声音资产/混音总线（SFX/BGM/Master Submix）/事件→声音映射/`PlayUISound` API。**声音唯一 owner**（VFX 拥视觉、各订各播）。depends-on: 2,3,4,5,6,7,8 | ⚠ **MED**（MetaSound 非 Sound Cue；Sound Class/Submix 三路混音；BGM duck=音量阶跃） |

> **Full Vision 预留（轻量）**：联网(25,依赖几乎全部，Networking 层，**全项目最大技术风险**，严格隔离 — MVP 状态已承载于 `GameState`/`PlayerState` 为其预留)/ 地图编辑器(26,依赖1，Meta 层，经 ④ Board DA Holder 产 `DA_Board`)。

---

## Module Ownership（模块所有权）

> **本节地位**：架构契约的规范性来源。每个模块声明 **Owns / Exposes / Consumes / Engine APIs** 四面。programmer 实现时，**写入某状态前必须确认本模块是其 owner**；跨模块读取一律走 Owner 的 Exposes 接口（查询函数或事件订阅），**禁止直接持有他模块的内部字段引用**。所有权裁定的权威依据是各 GDD 的 Dependencies 节 + systems-index registry；本节为其聚合视图。
>
> **核心不变式（Invariants，实现期硬约束）**：
> 1. **单一 owner**：每个运行态字段恰有一个写者模块。下表 Owns 列即写权威；其他模块对该字段只读（经受控写接口除外，且受控写仍由 owner 定义边界）。
> 2. **无环**：依赖图有向无环。三处潜在环已在设计期打破——见 §依赖图下方「破环裁定」。
> 3. **容器 vs 语义分离**：`PlayerState` 是回合(2)拥有的**字段容器**；`Cash`/`bIsInJail` 等字段的**业务语义**归各自系统（经济5/事件7），它们经回合(2)暴露的**受控写接口**改值，而非直接 `Set`。容器 owner ≠ 语义 owner 时，下表在 Owns 标注「(容器,语义归 N)」。
> 4. **呈现层纯叶子**：HUD(16)/地产卡 UI(17)/VFX(19)/音频(22) **Exposes 列为空**——不写任何运行态、不被 gameplay 反调、不驱动流程。它们只订阅 + 显示 + 转发玩家意图。

### 图例

- **Owns**：本模块独占写权威的数据/状态。括注 `(容器,语义归 N)` 表示字段存储在本模块但写语义归他模块，经受控写接口落地。
- **Exposes**：对外可读（查询函数）或可订阅（multicast delegate）的接口。`fn()` = 同步查询；`OnX` = 事件广播。
- **Consumes**：读自他模块的数据（经其 Exposes），标 `[模块号]`。`(snapshot)` = 经回合(2)聚合的只读快照读取，非直接硬引用。
- **Engine APIs**：直接调用的 UE 引擎类/子系统，标 `[5.7]` 版本 + risk。

### Foundation Layer（近零依赖地基）

| 模块 | Owns | Exposes | Consumes | Engine APIs |
|---|---|---|---|---|
| **1 棋盘数据 Board Data** | `FBoardTileData` 全静态字段（`TileType`/`ColorGroup`/`PurchasePrice`/`MortgageValue`/`RentTable`/`BuildingCost`/`DiceMultiplierTable`/`TaxAmount`/`SalaryAmount`/`EventDeck`/`SpecialAction`/`JailIndex`/`DisplayName`）；棋盘标识 `BoardId`；环序总格数 `N` | `GetTileData(i)`、`GetTileCount()`、`AdvanceIndex(from,steps)→(newIndex,passedGo)`（原子，无独立 PassedGoCount）、`GetTilesInGroup(group)→[indices]`（升序）、`StepsBetween(from,t)`（环绕）、`GetTileWorldTransform(i)`、`GetBoardId()` | 无（零上游） | `UDataAsset`/`UPrimaryDataAsset` `[5.7 LOW]`；`UDataTable` `[5.7 LOW]`（可选 RentTable 表） |
| **2 玩家与回合 Player & Turn** | `PlayerState[]` 字段容器：`CurrentTileIndex`(语义归移动4)、`Cash`(语义归经济5)、`bIsInJail`/`JailTurnsServed`(语义归事件7)、`bIsBankrupt`(经破产9 返回值驱动写)、`ConsecutiveDoubles`、`DisplayName:FText`、`bIsAI`/`AIDifficulty`(语义归 AI10)；回合顺序 / 当前玩家 / 当前 `Phase`；开局入口 `StartNewGame(FGameSetupConfig)`；`FGameSetupConfig`/`FPlayerSetupEntry` USTRUCT；`GameStateSnapshot` 聚合（供 AI/算租） | `GetCurrentPlayer()`、`GetPlayerState(i)`、受控写接口（`SetPosition`/Cash 受控写/监狱态受控写）、`BuildSnapshot()`；`OnTurnStarted(bIsAI,epoch)`、`OnPhaseChanged(Phase,ConsecutiveDoubles,语境)`、`OnTurnEnded(bGrantsExtraTurn)`、`OnTurnOrderResolved`、`OnAIActionExecuted(ActionIndex)`、`OnGameWon(winnerId)` | `[3]` `FDiceRollResult`；编排调用 `[4][5][6][7][8][9][10]` | `APlayerState`/`AGameStateBase` `[5.7 LOW]`（为 Full Vision 联网预留）；`UGameInstanceSubsystem`/`UWorldSubsystem` `[5.7 LOW]`（宿主待 OQ-1 ADR） |
| **3 骰子 Dice** | RNG 流（可种子化）、`FDiceRollResult`（`Die1`/`Die2`/`Total`/`bIsDouble`）、当前 `Seed`（Full Vision 序列化） | `RollDice()→FDiceRollResult`、`RandomRange(min,max)`、`RandomFloat01()`、`GetCurrentSeed()`（OQ-2）；`OnDiceRolled(FDiceRollResult)` | 无（零上游纯 RNG 服务） | `FRandomStream` `[5.7 LOW]`（种子化确定性，跨平台同源；**禁** `FMath::Rand`/全局 RNG 旁路） |

> **依赖修正（已落地）**：回合(2)硬依赖骰子(3)——`RollPhase`/双点 F-3/定序 F-4 均消费 `FDiceRollResult`。骰子(3)仍为纯零依赖服务，先于或并行 2 实现。

### Core Layer（依赖 Foundation）

| 模块 | Owns | Exposes | Consumes | Engine APIs |
|---|---|---|---|---|
| **5 经济与现金 Economy** | 现金算账权威：`Debit`/`Credit` 执行、租金公式、`net_liquidation_value`(F-9)、`is_insolvent`(F-10)、F-11 移交现金侧、`building_sellback` 比率(F-8)；`EChangeReason` 枚举 | `Debit(p,amt)`/`Credit(p,amt)`、`CalcRent(snapshot,...)`、`GetNetLiquidationValue(p,snapshot)`、`IsInsolvent(p,preaggregated_nlv)`、`GetUnmortgageCostForDisplay(MV)`；`OnCashChanged(p,EChangeReason)`、`OnRentPaid(...)`、`OnBankruptcyDeclared(...)` | `[1]` `GetTileData`；`[2]` 当前玩家 + `Cash` 受控写容器 + ResolvePhase 触发 + `Total`(PULL)；归属快照（经2/6 push 参数，**不硬引用6**） | 无额外引擎类（纯 C++ 逻辑层，数据驱动） |
| **4 移动 Movement** | 移动编排逻辑（逐格推进 / `TeleportTo`）；**不拥有 `CurrentTileIndex` 存储**（经回合2 `SetPosition` 受控写） | `Advance(player,steps)`、`TeleportTo(player,target,paysGo,context,advanceOnZero)`；`OnPawnMoveStarted(...)`、`OnPawnLanded(arrivalContext)` | `[1]` `AdvanceIndex`/`StepsBetween`/`GetTileCount`/`SalaryAmount`；`[2]` 当前玩家 + `CurrentTileIndex` 受控写容器；`[3]` `Total`(经2)；**orchestrated→[5]** push `(passedGo,SalaryAmount)` | 无（逻辑层；棋子可视位移归 VFX19） |
| **6 地产所有权 Property Ownership** | `owner` map、`bIsMortgaged`(per-tile)、`is_monopoly`(由 board 组成员 + owner map 算)、`station_count`/`utility_count`；买地/抵押/赎回**事务** | `GetOwner(t)`、`IsMonopoly(group)`、`IsMortgaged(t)`、`GetStationCount(p)`、`GetUtilityCount(p)`、`TransferOwnership(t,to)`、`ReturnTileToBank(t)`；`OnOwnershipChanged(t,from,to)`、`OnMortgageChanged(t,bMortgaged)` | `[1]` `GetTileData`/`GetTilesInGroup`；`[5]` 调 `Credit`/`Debit`（6→5，**5 不反调6**）；**push 归属快照供5算租** | 无（逻辑层） |
| **7 事件格 Tile Events** | 牌堆数组顺序（model B + Fisher-Yates 种子）、`bHoldsChanceOutCard`/`bHoldsChestOutCard`；**监狱态规则**（字段值存回合2）；`EJailReason`/`EJailReleaseMethod` 枚举；`JAIL_BAIL_AMOUNT`/`MAX_JAIL_TURNS` | `ResolveTileEvent(...)`、`SendToJail(p)`、`IsInJail(p)`、`HoldsAnyOutCard(p)`；`OnCardDrawn`、`OnTileEventResolved`、`OnPlayerJailed`、`OnPlayerReleased` | `[1]` `TileType`/`EventDeck`/`TaxAmount`/`SpecialAction`/`JailIndex`；`[2]` ResolvePhase + 监狱态受控写；`[3]` `bIsDouble`/`Total`；`[4]` `TeleportTo`/`Advance`；`[5]` `Credit`/`Debit`、Utility `dice_total`→F-4 | 无（逻辑层） |

### Feature Layer（依赖 Core）

| 模块 | Owns | Exposes | Consumes | Engine APIs |
|---|---|---|---|---|
| **8 建房升级 Building** | `house_count`（**per-tile [0,5]**，唯一 owner）；建筑清单；`GetTotalHouseCount`/`GetTotalHotelCount` | `GetHouseCount(t)`、`CanBuildHouse(t)`、`BuildHouse(t)`/`SellHouse(t)`、`GetPlayerBuildings(p)→[(tile,count)]`、`GetTotalHouseCount(p)`/`GetTotalHotelCount(p)`、`ForcedSellNextBuilding(p)`、`LiquidateAllBuildings(debtor)`（无偿清算，不调 Credit）；`OnBuildingChanged(tile,newCount)`（2 字段） | `[1]` `BuildingCost`/`ColorGroup`/`GetTilesInGroup`；`[5]` 建/卖房现金腿（8→5）；`[6]` `GetOwner`/`is_monopoly`/`is_mortgaged` 门控（8→6） | 无（逻辑层） |
| **9 破产与胜负 Bankruptcy** | `net_worth`(F-1=Cash+经济F-9)；排名/出局裁决逻辑；**return-only 编排子程序** | `ResolveBankruptcy(debtor,creditor,snapshot)→{eliminated,winnerId\|NONE,rankingEntry}`（被2调，**绝不回调2**）；`OnPlayerBankrupt`、`OnPlayerRanked`（`OnGameWon` 由回合2 触发） | `[2]` 被 ResolveBankruptcy 调 + `activePlayersSnapshot`；`[5]` `is_insolvent`/F-11/`net_liquidation_value`；`[6]` `TransferOwnership`/`ReturnTileToBank`；`[8]` `LiquidateAllBuildings` + in-kind 随格 | 无（逻辑层） |
| **10 AI 对手 AI Opponent** | 启发式打分决策逻辑（F-1..F-7，无前瞻）；三档参数差异(F-7)；独立 juice RNG 流隔离约定 | `DecideBuyProperty(snapshot)`、`DecidePostRollActions(snapshot)`、`DecideJailAction(snapshot)`（被回合2 CR-8 同步调，**return-only**） | **纯叶子，全经 snapshot 只读**：`[1]` price/group/type；`[2]` 被调 + snapshot + `bIsAI`/`AIDifficulty`；`[3]` `RandomRange`/`RandomFloat01`；`[5]` rent/nlv；`[6]` is_monopoly/count；`[8]` house_count/CanBuildHouse；`[7]` 出狱卡/JAIL 常量。**移动4 降 Alpha** | `FRandomStream` `[5.7 LOW]`（经骰子3，确定性重放） |

> **下游为空**：AI(10) 是依赖图纯叶子，无 MVP 系统依赖其输出。`OnAIActionExecuted` 由**回合2 执行 AI 动作时广播**（非 AI 发），故 HUD/VFX/音频依赖 2 不依赖 10。

### Presentation Layer（纯叶子，Exposes 为空）

> 以下模块**不写任何运行态、不被 gameplay 反调、不驱动流程**。它们只**订阅 owner 的 multicast delegate + 显示 + 转发玩家意图**。owner 先算定结果再广播，叶子落后只丢呈现不回灌。

| 模块 | Owns（仅呈现态） | Consumes（订阅 / 读） | Engine APIs |
|---|---|---|---|
| **16 HUD** | UMG widget 树、本地呈现缓存（`E_cur` 回合代次等）；**无运行态** | `[1]` DisplayName/ColorGroup/PurchasePrice；`[2]` PlayerState 全字段 + `OnPhaseChanged`/`OnTurnStarted`/`OnTurnEnded`/`OnTurnOrderResolved`/`OnAIActionExecuted`/`OnGameWon`/`OnBuildingAnnounced`；`[5]` `OnCashChanged` | `UUserWidget`/`UCommonActivatableWidget` `[5.7 HIGH — CommonUI 手柄路由]`；`UWidgetAnimation` `[5.7 LOW]`；`meta=(BindWidget)`；**禁 hover-only**；BP↔C++ 绑定边界→ADR |
| **17 地产卡 UI** | 卡片 widget 树、本地缓存；**无运行态** | `[1]` DisplayName/RentTable/ColorGroup/MortgageValue；`[6]` `OnOwnershipChanged`/owner/is_mortgaged/is_monopoly；`[8]` `GetHouseCount`/`OnBuildingChanged`（**house_count 取自8不取6**）；`[5]` `GetUnmortgageCostForDisplay`；转发建房/抵押**意图** | `UCommonActivatableWidget`/`UCommonButtonBase` `[5.7 HIGH — CommonUI]`；`meta=(BindWidget)` |
| **19 游戏反馈 VFX** | 视觉 juice 资产、`house_count_cache`/pawn registry（本地 delta 派生）；**无运行态** | `[3]` `OnDiceRolled`；`[4]` `OnPawnMoveStarted`/`OnPawnLanded`；`[5]` `OnRentPaid`/`OnCashChanged`/`OnBankruptcyDeclared`；`[6]` `OnOwnershipChanged`/`OnMortgageChanged`；`[8]` `OnBuildingChanged`；`[2]` `OnPhaseChanged`/`OnTurnStarted`/`OnGameWon`；`[1]` `GetTileWorldTransform`/N | `Niagara` `[5.7 MED — 非 Cascade]`；**Post Process Material**（胜利暖金）`[5.7 MED — Substrate vs legacy 待 ADR]`；**Lumen/Nanite/Megalights 关闭** |
| **22 音频 Audio** | 声音资产、混音总线（SFX/BGM/Master 三路）、事件→声音映射、`house_count_cache`；**无运行态** | `[3]` `OnDiceRolled`；`[4]` `OnPawnMoveStarted`/`OnPawnLanded`；`[5]` `OnRentPaid`/`OnCashChanged`/`OnBankruptcyDeclared`；`[6]` `OnOwnershipChanged`/`OnMortgageChanged`；`[7]` `OnPlayerJailed`/`OnPlayerReleased`/`OnCardDrawn`；`[8]` `OnBuildingChanged`；UI 经 `PlayUISound` | **MetaSound**（`UMetaSoundSource`）`[5.7 MED — 非 Sound Cue]`；`USoundClass`/`USoundSubmix` `[5.7 LOW]`（三路混音 + BGM duck） |
| **20 主菜单与大厅 Lobby** | 配置编辑态（`FString` 编辑 → `FText` 边界）；**对局开始后不持运行态** | `[2]` 经 `StartNewGame(FGameSetupConfig)` 移交；写 `PlayerState.DisplayName`/`AIDifficulty`（开局构造）；`EPlayerColor` 枚举 | `UCommonActivatableWidget` `[5.7 HIGH — CommonUI]`；席位/颜色互斥分配 UI |

### Persistence Layer（横切）

| 模块 | Owns | Exposes | Consumes | Engine APIs |
|---|---|---|---|---|
| **21 存档/读档 Save/Load** | 序列化容器 + round-trip identity 契约；**enable-not-own** | `SaveGame()`、`LoadGame()`、`DoesSaveGameExist()`；`OnGameLoaded`（下游重绑） | 逐 owner 可序列化契约：`[2]` PlayerState 全字段 + 阶段 + 定序 + ConsecutiveDoubles + 完整 `FDiceRollResult`；`[1]` `GetBoardId`；`[5]` 每玩家 Cash；`[6]` owner map + bIsMortgaged；`[8]` per-tile house_count[0,5] | `USaveGame` `[5.7 LOW]`；写盘原子性 + 宿主 → ADR（OQ-Save-1，与回合2 OQ-1 协调） |

### ASCII 依赖图（MVP，箭头 = "依赖/读"）

```
                        ┌─────────────── Foundation ───────────────┐
                        │                                          │
                   [1 棋盘数据]                              [3 骰子]
                    (零依赖)                                (零依赖 RNG)
                        ▲                                       ▲
                        │                                       │
                   [2 玩家回合]◄──────────────────────────────────┘
                  (PlayerState 容器 / 回合编排 / 状态机)
                        ▲ ▲ ▲ ▲ ▲
          ┌─────────────┘ │ │ │ └──────────────┐
          │      ┌────────┘ │ └────────┐        │
     ┌────────── Core ──────────────────────────────────┐
     │    │          │              │          │        │
 [4 移动]  [5 经济]◄────[6 地产]    [7 事件]   (5↔6 见破环) │
   │push→[5]  ▲ │只调不反调  ▲         ││        │        │
   │         │ └─────6→5───┘         │└──调4/5──┘        │
     └────────┼────────────────────────────────────────┘
              │
     ┌─────── Feature ───────────────────────────────────┐
     │   [8 建房]───8→5,8→6──►(5,6)                       │
     │      ▲   (house_count 唯一 owner;6 不反读防 6↔8 环) │
     │   [9 破产]──9→5,9→6,9→8──►(5,6,8)                  │
     │      ▲  2→9 orchestration invoke,9 return-only      │
     │   [10 AI]──纯叶子,经 snapshot 只读 1/2/3/5/6/7/8──►  │
     └────────────────────────────────────────────────────┘
              ▲ (下游为空)
     ┌─────── Presentation(纯叶子,只订阅,Exposes 空)──────┐
     │  [16 HUD] [17 卡UI] [19 VFX] [22 音频] [20 大厅]      │
     │   订阅 owner multicast delegate → 显示 → 转发意图       │
     └────────────────────────────────────────────────────┘
              ▲
     ┌─────── Persistence(横切) ──────────────────────────┐
     │  [21 存档]── enable-not-own,序列化 1/2/5/6/8 字段 ───►│
     └────────────────────────────────────────────────────┘
```

### 破环裁定（三处已打破，实现期必须维持）

1. **5↔6（经济↔地产）**：地产(6)拥买地/抵押**事务**，调经济(5)`Credit`/`Debit` 执行现金腿(6→5)；**经济(5)绝不反调地产(6)**。算租所需归属事实经回合(2)/ResolvePhase **push 快照参数**传入经济算租调用——经济不硬引用6接口。环被打破。
2. **6↔8（地产↔建房）**：`house_count` **唯一归建房(8)**，地产(6)**不读**8；`is_monopoly` 由6自算（board 组成员 + owner map）。建房(8)单向读6门控(8→6)。算租/算 NLV 所需 house_count 经**回合(2)聚合传入**经济（防 5→8 环）。
3. **2↔9（回合↔破产）**：回合(2)调 `9.ResolveBankruptcy(...)`(2→9 orchestration invoke)；破产(9)**return-only**，返回裁决，**绝不回调2、不直写字段**。`bIsBankrupt`/`net_worth` 经返回值驱动写（字段容器归2，排名/估值逻辑归9）。

> **systems-index「Circular Dependencies: None found」声明的实现期保障**：上述三条不是"碰巧无环"，而是**主动设计的方向性约束**。任何 PR 若引入 5→6 / 6→8 / 9→2 的反向调用，即破坏架构不变式，review 必须拒绝。

### Alpha / Full Vision 模块（轻量带过）

| 模块 | 预期 Owns | 预期 Consumes | 备注 |
|---|---|---|---|
| **11 交易 / 12 拍卖** | 交易/竞价会话态 | `[5]` 结算、`[6]` owner 转移、`[10]` AI 决策 hook | Economy 类，各获 AI hook |
| **13 命运之轮 / 14 俄罗斯轮盘 / 15 策略卡** | 各机制规则态 | `[5][6][7]` / `[2][9]`（轮盘致死调9出局） | Gameplay 特色机制 |
| **18 交易拍卖 UI / 23 设置 / 24 教程** | 呈现态 / House Rule 覆盖 / 引导态 | 各对应 gameplay（纯叶子模式延续） | 23 可覆盖旋钮（开局锁定） |
| **25 联网 Online** | 状态同步 / 回合权威 / 重连 | **几乎全部** | **全项目最大技术风险**。MVP 已预留：状态承载于 `GameState`/`PlayerState`、RNG 种子化可重放、AI return-only |
| **26 地图编辑器** | `DA_` 实例产出 | `[1]` 完整 schema | 复用棋盘1 数据契约 |

---

## Data Flow（数据流）

> **本节定位**：定义 MVP 关键运行时数据流的权威路径——producer/consumer、传递机制（同步调用 / 事件多播 / 共享状态），供 programmer 据此实现。所有事件签名、payload 字段、owner 关系**逐字对齐**各系统 GDD 与 `systems-index.md`。

### D.0 三种传递机制

| 机制 | 标记 | 语义 | 何时用 |
|------|------|------|--------|
| **同步调用 Sync Call** | `→call` | 调用方阻塞等返回值；结果在返回时已确定 | 跨系统**权威结算**（算租、Debit/Credit、ResolveBankruptcy）。结算必须先于呈现完成。 |
| **事件多播 Event** | `↝event` | `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`；owner 单向广播，consumer 各订各播 | **呈现层解耦**（HUD/VFX/Audio 订阅 juice）。owner 先算定结果再广播。 |
| **共享状态 Shared State** | `≋state` | 经受控写接口改 `PlayerState`/owner map/`house_count`，他系统经只读接口或快照读 | 回合编排聚合状态（snapshot）、序列化采集。**禁直接写他系统字段**。 |

**铁律：结算同步先行，呈现事件后随。** owner GDD 一律「先算定权威结果（sync）→ 再广播事件（event）」。呈现层（16/17/19/22）是**纯叶子消费者**——只订阅、只读、只显示，绝不回写状态、绝不阻塞框架推进。这是全部 60FPS 帧预算与无环依赖的根本保证。

### D.1 回合循环路径（Turn Loop）

**编排 owner = 玩家回合(2)**。回合2 是 spine-owner，持状态机 `ETurnPhase`，在每个阶段**同步委派**下游系统执行，并在阶段边界广播事件。状态机转换：

```
TurnStart →(在狱? JailTurn → TurnEnd)
          →(正常: RollPhase → MovePhase → ResolvePhase → PostRollAction → TurnEnd)
TurnEnd  →(双点且未入狱? 回 RollPhase[同玩家额外回合])
          →(否则: 下一玩家 TurnStart)
```

#### 序列图：一个正常回合（人类玩家落无主地）

```
回合(2)        骰子(3)       移动(4)       经济(5)/所有权(6)/事件(7)   HUD(16)/VFX(19)/Audio(22)
  │
  ├─ ↝event OnTurnStarted(PlayerId, bIsAI=false) ─────────────────────────────▶ 高亮当前玩家 / "轮到你"音
  │   [≋state 重置 ConsecutiveDoubles 缓存]
  │
  │  ── RollPhase ──
  ├─ ↝event OnPhaseChanged(RollPhase, ConsecutiveDoubles) ────────────────────▶ (定序语境抑制双点强化)
  ├─ →call RollDice() ──▶│
  │                      ├─ ↝event OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble}) ─▶ 两骰翻滚 juice + 掷骰音
  │◀── return 同一固化 FDiceRollResult（== 事件 payload）
  │
  │  ── MovePhase ──
  ├─ →call Advance(player, Total, paysGo=true, context=DiceMove) ──▶│
  │                                                 ├─ ↝event OnPawnMoveStarted(...) ─▶ 逐格 hop + 过 GO 高亮
  │   [移动经 SetTileIndex 受控写 ≋PlayerState.CurrentTileIndex]
  │   [若 passedGo>0: →call 经济.发薪 push (passedGo, SalaryAmount)]──▶ Credit(Salary) ↝OnCashChanged
  │                                                 ├─ ↝event OnPawnLanded(Player,TileIndex,EArrivalContext=DiceMove) ─▶ 落地 juice
  │◀── 控制交回 ResolvePhase
  │
  │  ── ResolvePhase ──（落地结算：算租/买地/抽牌/税）
  ├─ [≋聚合归属快照: →call 所有权6 is_monopoly/station_count + 建房8 GetHouseCount → 传经济算租]
  ├─ →call 经济.SettleLanding(...) ──▶│ 算租→Debit(payer)+Credit(payee)
  │                                    ├─ ↝event OnRentPaid(Payer,Payee,Amount,TileIndex) ─▶ 金币飞溅 + 收租音
  │                                    ├─ ↝event OnCashChanged ×2 (payer/payee, reason=Rent) ─▶ 现金数字跳动
  │   [若落无主地: 暴露买地决策点 →见 D.1a]
  │
  │  ── PostRollAction ──（自愿建房/抵押/结束）
  │   [人类: 等 UI 发 EndTurn 意图；AI: 见 D.1a]
  │
  │  ── TurnEnd ──
  ├─ ↝event OnTurnEnded(bGrantsExtraTurn) ───────────────────────────────────▶ 回合切换过场
  └─ [bIsDouble ∧ 未入狱 → 回 RollPhase（OnTurnStarted 不重发，仅 OnPhaseChanged(RollPhase) 重发）]
      [否则 → NextActivePlayerIndex（跳过 bIsBankrupt 出局者）→ 下一玩家 TurnStart]
```

**关键架构裁定**：
- **`SentToJail` 落地抑制**（movement OQ-T-Move-1 / player-turn AC-46）：`OnPawnLanded.EArrivalContext==SentToJail` 时，ResolvePhase **不触发**买地/收租结算分支（AC-46：结算 hook 调用次数==0）。去坐牢经 `TeleportTo(JailIndex, paysGo=false)`，不发薪。
- **Utility 租单源去歧义**（economy AC-18 / player-turn CR-3.1）：连续双点链 / 事件额外骰下，经济须从回合2 持有的「当前正在结算这一程」`Total` PULL，holder=回合2。

#### D.1a 决策点分流：人类 vs AI（同一 ResolvePhase / PostRollAction）

```
ResolvePhase 落无主地：
  人类 ──▶ HUD 弹买地 UI ──▶ 玩家点击 ──▶ 回合2 →call 所有权6.Buy() →call 经济.Debit()
  AI   ──▶ 回合2 →call AI.DecideBuyProperty(GameStateSnapshot, PropertyData) → bool（同步纯函数）
            └─ 返回 true 但现金不足/已被买 → 执行期经济5/所有权6 校验 → 视同 false + UE_LOG（AC-38b）

PostRollAction：
  人类 ──▶ UI 发 EndTurn 意图
  AI   ──▶ 回合2 →call AI.DecidePostRollActions(GameStateSnapshot) → TArray<FTurnAction>（有序动作）
            └─ 框架逐动作重校执行（不可行静默跳过+dev日志，不中断）→ 执行毕即 EndTurn
            └─ 空数组 [] = 直接 EndTurn（AC-37d）
            └─ 每条执行的动作 ↝event OnAIActionExecuted(ActionIndex) ─▶ HUD 非阻塞逐步呈现
```

**关键裁定（R6 RETREAT）**：AI 决策是**同步纯函数**（只读 `GameStateSnapshot`，无前瞻、无副作用、不回调回合2）。AI 回合 TurnEnd 与人类一致——**即时移交，不阻塞于呈现回放**。HUD/VFX 自主非阻塞播放 AI 行动 juice，框架不 gating。

**`GameStateSnapshot`**（AI 唯一读取面）：AI 在自身决策阶段生成只读快照（读 1/3/5/6/7/8），含 `Rent_top1/top2`、`owner_map`、`starting_cash`、`board_tile_count` 等字段。**snapshot 装配方 / 宿主待架构期 ADR**（OQ-AI-3 → OQ-1 ADR / ADR-0006）。

### D.2 事件总线路径（Event Bus / 解耦广播）

**没有集中式 event bus 对象**——本架构用 **UE `DYNAMIC_MULTICAST_DELEGATE` 分散在各 owner 系统**实现解耦：每个状态变更事件归其**唯一 owner** 拥有并广播，呈现层订阅。这是「单一事件源」纪律——避免双触发。

#### 事件总览表（MVP，owner → consumer）

| 事件（payload） | Owner | Consumer | 备注 |
|----------------|-------|----------|------|
| `OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})` | 骰子(3) | VFX, HUD, Audio | 两骰呈现禁从 Total 拆解 |
| `OnPhaseChanged(FPhaseChangedInfo{Phase,ConsecutiveDoubles})` | 回合(2) | HUD, VFX, Audio | payload 取值非轮询 |
| `OnTurnStarted(FTurnStartedInfo{PlayerId,bIsAI})` | 回合(2) | HUD, VFX, Audio, AI | 额外回合链不重发 |
| `OnTurnEnded(bGrantsExtraTurn)` | 回合(2) | HUD, VFX | 两分支各有动画 AC |
| `OnTurnOrderResolved(FTurnOrderResult{OrderedPlayerIds,bResolvedBySeatTiebreak})` | 回合(2) | HUD, VFX | **须包 USTRUCT**（裸 TArray 编译失败） |
| `OnAIActionExecuted(ActionIndex)` | 回合(2) | HUD | 非阻塞逐步播放 |
| `OnPawnMoveStarted(Player,From,To,Steps,PassedGo)` | 移动(4) | VFX, HUD, Audio | hop 回放 |
| `OnPawnLanded(Player,TileIndex,EArrivalContext)` | 移动(4) | VFX, HUD, Audio | `EArrivalContext∈{DiceMove,AdvanceCard,SentToJail}` |
| `OnCashChanged(Player,OldCash,NewCash,EChangeReason)` | 经济(5) | HUD, VFX, Audio | reason ∈ {Salary,Rent,Purchase,Mortgage,Unmortgage,Tax,Build,Trade,Bankruptcy} |
| `OnRentPaid(Payer,Payee,Amount,TileIndex)` | 经济(5) | VFX, Audio | 不按金额缩放（MVP） |
| `OnInsufficientFunds(Player,AmountDue,AmountShort)` | 经济(5) | HUD | 触发 Raising Funds 瞬态 UI |
| `OnBankruptcyDeclared(Debtor,Creditor)` | 经济(5) 广播（裁决归9） | VFX, HUD, Audio | 现金侧转移完成后广播（2 字段） |
| `OnOwnershipChanged(TileIndex,Old,New)` | 所有权(6) | VFX, Audio, (HUD/卡17) | 买地/破产移交逐格；触发 cache re-seed |
| `OnMortgageChanged(TileIndex,bIsMortgaged)` | 所有权(6) | VFX, Audio, 卡17 | 抵押/赎回 |
| `OnBuildingChanged(tile,newCount)` | 建房(8) | VFX, HUD, Audio, 卡17 | **2 字段**（actingPlayerId 由回合2 上下文取，非读事件第3字段） |
| `OnBuildingAnnounced(TileIndex,NewHouseCount,ActingPlayerId)` | 回合(2) | HUD | 全员可见建房通告 banner（支柱②） |
| `OnCardDrawn(playerId,EEventDeck,cardId)` | 事件格(7) | Audio, (VFX/HUD) | 抽牌 |
| `OnTileEventResolved(playerId,tileIndex,ECardEffectType)` | 事件格(7) | Audio | 结算提示 |
| `OnPlayerJailed(playerId,EJailReason)` / `OnPlayerReleased(playerId,EJailReleaseMethod)` | 事件格(7) | Audio | 入/出狱音 |
| `OnPlayerBankrupt(debtor,creditor,reason)` | 破产(9) | VFX, HUD, Audio | 出局 juice（清算编排末广播，3 字段） |
| `OnPlayerRanked(player,rank)` | 破产(9) | HUD | 终局排名 |
| `OnGameWon(WinnerId)` | **回合(2)** 触发广播 | VFX, HUD, Audio | **9 仅返回 winnerId，回合2 触发**（9 不直接广播——return-only） |

#### 已知跨档接缝（架构期需裁定，不阻 MVP 开工）

1. **破产事件接缝 OQ-VFX-13 / OQ-Audio-2 ✅ RESOLVED（2026-06-08，ADR-0003 §3 收口）**：`OnBankruptcyDeclared`（经济5，2字段）vs `OnPlayerBankrupt`（破产9，3字段）并存。**producer 裁定**：HUD16 / VFX19 / Audio22 **统一订经济5 `OnBankruptcyDeclared`**（现金侧时机=破产 juice/音效语义锚点；与已 Approved 姊妹档对齐）；破产9 `OnPlayerBankrupt` 不被呈现层订阅（防误订双发）。职责切分=现金完成（呈现层订）vs 编排完成（不订）。
2. **掷骰意图事件来源 OQ-VFX-16**：「点击掷骰后」的意图事件 owner 待定（骰子3 / 回合2），MVP fallback `OnPhaseChanged(RollPhase)` 已具名。
3. **`OnOwnershipChanged` 复用为破产清算事件源**：破产逐格易主**不引入新事件**，复用所有权6 的逐格 `OnOwnershipChanged`；同帧级联多格须节流（`T_sfx_min_retrigger=0.06`）防爆音/爆 juice。

**呈现层订阅纪律**：同一 owner 事件，VFX 拥视觉、Audio 拥听觉、HUD 拥信息——**各订各播、声画分离、互不双触发**（enable-not-own）。未注册枚举值一律走**中性兜底**（运行期安全）。

### D.3 存档 / 读档路径（Save / Load）

**owner = 存档(21)，横切层 enable-not-own**：只「采集→序列化写盘 / 读盘→反序列化→重建→**重绑 delegate**」。核心防漏机制 = **CR-3 逐状态系统可序列化契约清单** + 金标准 round-trip identity。

#### D.3a 序列化什么 / 谁拥有（CR-3 清单）

| Owner | 序列化字段 | 重建（写） | 机制 |
|-------|-----------|----------|------|
| 棋盘(1) | `BoardIdentifier`(FName, DA 引用 ID，**非全量布局**) | 按 ID 加载 DA | ≋state |
| 回合(2) | 11 字段 `PlayerState` + 定序 + `CurrentPlayerIndex` + `CurrentPhase`(ETurnPhase) + `ConsecutiveDoubles` + 阈值 + 完整 `FDiceRollResult`（含 `bIsBankrupt`/`CurrentTileIndex`） | `switch(CurrentPhase)` 还原精确阶段 | ≋state |
| 经济(5) | 每玩家 `Cash` | 写回 | ≋state |
| 所有权(6) | per-tile `owner` + `bIsMortgaged` | 重建 owner map | ≋state |
| 建房(8) | per-tile `house_count` [0,5]（归8 不归6） | 写回 per-tile | ≋state |
| 骰子(3) | **MVP 不序列化 Seed**（当前骰由回合2 `FDiceRollResult` 保护） | 读档 `SetSeed(非确定值)` | — |
| 破产(9) | **无独立存档腿**（`bIsBankrupt` 在回合2 行覆盖） | — | — |

**关键裁定**：
- 棋盘**存 DA 引用 ID 不存全量布局**——读档按 ID 加载 DA；若 DA 缺失 → **拒绝读档不回退默认棋盘**（防 TileIndex 错位静默损坏，EC-3）。
- `FTimerHandle` / Latent Action **不可序列化**——回合状态机须用 `ETurnPhase` 枚举字段 + delegate 推进（`switch(CurrentPhase)` 可重入还原），**不可用 Latent Action 实现阶段等待**（归回合2 OQ-1 ADR）。

#### D.3b 合法可存档点 / 禁止存档瞬态（CR-4）

存档只能在**稳定回合边界**触发。禁止中途存档的瞬态：Raising Funds 筹款瞬态、开局定序 / 破产清算编排进行中 → 拒绝，返回 `SaveRejected_TransientState`。可存档点裁决归回合2（查询接口 OQ-Save-2 待定，MVP 默认锁权威路径）。

#### D.3c 读档重建 + 重绑序列（CR-5 / CR-6）

```
LoadGameFromSlot()
  ├─ 1. 校验存档头（魔数/版本/校验和/CR-3 全字段 manifest 完整，F-4）
  │      └─ 任一假 → 拒绝整个读档，对局不被破坏（EC-2）
  ├─ 2. 按 BoardIdentifier →call 加载棋盘 DA(1)（缺失→拒绝，EC-3）
  ├─ 3. 重建经济(5) 每玩家 Cash ≋
  ├─ 4. 重建回合(2): PlayerState + 定序 + CurrentPlayerIndex + CurrentPhase + ConsecutiveDoubles + 阈值 + FDiceRollResult ≋
  ├─ 5. 重建所有权(6) owner map + 建房(8) per-tile house_count ≋
  ├─ 6. 重绑 delegate 监听器（CR-6）：存档21 不直接 AddDynamic
  │      └─ ↝event OnGameLoaded 广播 ──▶ HUD16/VFX19/卡17/Audio22 各自重绑订阅
  │           （每个 delegate 先 Unbind 再 Bind，防双订阅，AC-21）
  └─ 7. OnGameLoaded 发出后才允许二次保存（EC-7）
```

**重绑数据流（关键，CR-6）**：存档21**不持有也不直接订阅**任何 gameplay 事件——它只在重建完成后广播 `OnGameLoaded`。呈现层（16/17/19/22）订阅 `OnGameLoaded`，**自行重绑各自的 owner delegate 订阅**（每个先 `Unbind` 后 `Bind`，防双绑）。回合状态机 `switch(CurrentPhase)` 还原精确阶段 + **重绑该阶段 delegate**。

**版本纪律**：每次改 CR-3 字段集/序列化格式 → `CURRENT_SAVE_VERSION +1`，`USaveGame` 子类全字段标 `UPROPERTY(SaveGame)`。

### D.4 初始化顺序（Bootstrap / Initialization Order）

**初始化是严格的依赖偏序**：上游状态系统先于下游、下游呈现层最后绑定。

```
启动 / 开始新局
  ├─ 1. 主菜单大厅(20)：玩家装配 FGameSetupConfig → →call 回合2 StartNewGame(const FGameSetupConfig&)（OQ-Lobby-1）
  ├─ 2. 棋盘数据(1)：加载 Board DA（BoardIdentifier → DA_Board_*），零依赖最先就绪
  ├─ 3. GameState / GameMode 层：构造对局容器（为 Full Vision 联网预留，MVP 不复制）
  ├─ 4. 各 Subsystem 初始化（按依赖顺序）：
  │      经济(5)[读 board] → 所有权(6)[1,5] → 建房(8)[1,5,6] → 事件格(7)/破产(9) 编排子程序就绪
  │      骰子(3) 纯 RNG 服务（零依赖，可任意早）
  ├─ 5. 回合(2) 据 FGameSetupConfig 初始化 PlayerState 列表：
  │      └─ 分配 PlayerId/TokenColor，置 CurrentTileIndex=0 / Cash=STARTING_CASH
  │      └─ 定序（席位掷骰 → 席位升序裁定，↝OnTurnOrderResolved）
  │      └─ 开局拒绝路径防守（P<2 / P>4 / 越界，AC-27）
  ├─ 6. 呈现层绑定（最后）：HUD(16)/卡(17)/VFX(19)/Audio(22) AddDynamic 订阅各 owner delegate
  └─ 7. 回合2 广播首个 OnTurnStarted → 进入 D.1 回合循环
```

**继续游戏路径**：步骤 1 改为主菜单20 查 `DoesSaveGameExist` → `LoadGameFromSlot()`，走 **D.3c 重建+重绑序列**（步骤 2-6 由读档重建替代）。

**架构硬前提（OQ-1 / ADR-0001）**：Subsystem 宿主、回合状态机的 UObject 宿主 + `IGameClock` DI + RNG 封装，是 AI(10)/HUD headless 测试/存档原子性的共同前提，须在架构期 ADR 统一裁定。

### D.5 跨流不变式（programmer 实现红线）

1. **无环依赖**：UI→gameplay 单向；AI 纯叶子；破产9 return-only（防 2↔9 环）；事务归6（6→5，5 不反调6，防 5↔6 环）；`house_count` 归8（防6↔8 环）。
2. **结算先行**：每个 owner「先算定权威结果（sync）→ 后广播事件（event）」。`RollDice()` 返回值 == `OnDiceRolled` payload。
3. **单一事件源**：每个状态变更仅一个 owner 广播；破产清算复用 `OnOwnershipChanged`；同帧级联多格须节流防爆。
4. **受控写边界**：他系统字段经受控写接口改（`SetTileIndex`/`SetBankrupt`/`Credit`/`Debit`），破产移交期 `bIsMidBankruptcyTransfer` 锁拦截监听器回写。
5. **读档重绑闭环**：`OnGameLoaded` 后呈现层每 delegate 先 Unbind 后 Bind；瞬态期拒绝存档。

---

## API Boundaries（API 边界）

> **本节定位。** 定义 MVP 各模块间的**公共契约**——programmer 据此实现的接口签名、调用约束与保证。每个边界给出四要素：**暴露接口** · **入口语义** · **调用方不变式**（caller MUST hold） · **模块保证**（callee guarantees）。签名逐字对齐各系统已 Approved 的 GDD；凡 GDD 与本节冲突，**以 owner GDD 为准**。
>
> **引擎类型版本基线（UE 5.7）。**
> - `USTRUCT(BlueprintType)` + 字段 `UPROPERTY(BlueprintReadOnly)`（`FDiceRollResult`/`FBoardTileData` 等）：**pre-5.3 稳定 API**，5.7 无破坏性变更。
> - 事件统一 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` + `UPROPERTY(BlueprintAssignable)`：BP/C++ 双向可订阅，**pre-5.3 稳定**。本项目所有跨模块事件均为 dynamic multicast（兼容 Blueprint 为主策略）。
> - 服务宿主形态（`UWorldSubsystem` vs `UGameInstanceSubsystem` vs Component）：**待 ADR-0001**。本节接口签名与宿主无关——ADR 不改函数签名，只决定它们挂在哪个 UObject 上。
> - `UFUNCTION(BlueprintCallable)`：所有跨模块同步调用接口标此宏。`FRandomStream` 等非 BP-callable 原生类型须经 C++ 封装暴露。
>
> **全局架构不变式。** ① 无环纪律（5↛6 / 6↛8 / 9↛2）；② 受控写（状态字段只经其 owner 受控接口修改）；③ 同步结算 + 事件后置（结果在呈现动画开始**前**已产出；事件在状态已变更**后**广播；呈现层订阅回调内禁产生副作用，尤禁二次抽随机/二次结算）。

### 边界 1 — DiceService（骰子 3）

随机数源头，零依赖纯 RNG 服务。持唯一权威 `FRandomStream`。

```cpp
USTRUCT(BlueprintType)
struct FDiceRollResult { int32 Die1; int32 Die2; int32 Total; bool bIsDouble; };

UFUNCTION(BlueprintCallable) FDiceRollResult RollDice();              // 同步、单帧返回
UFUNCTION(BlueprintCallable) int32 RandomRange(int32 Min, int32 Max); // 闭区间 [Min,Max]
UFUNCTION(BlueprintCallable) float RandomFloat01();                   // 半开 [0.0,1.0)
UFUNCTION(BlueprintCallable) void  SetSeed(int32 InitialSeed);
UFUNCTION(BlueprintCallable) int32 GetCurrentSeed() const;           // 显式存取,勿靠 struct 反射(OQ-2)
UPROPERTY(BlueprintAssignable) FOnDiceRolled OnDiceRolled;           // (FDiceRollResult)
```

- **入口语义**：`turn(2)` RollPhase 调 `RollDice()`；定序复用只取 `Total`；`tile-events(7)` 双点出狱取 `bIsDouble`；`AI(10)` 走 `RandomRange`/`RandomFloat01`。
- **caller MUST**：开局必先 `SetSeed`（默认构造种子恒 0 禁落 lazy-init）；**禁旁路** `FMath::Rand`/BP `Random Integer in Range`；`OnDiceRolled` 订阅者禁回调内同步调抽取 API；`RandomRange` 须 `Min<=Max`。
- **callee guarantees**：`Total==Die1+Die2 ∧ bIsDouble==(Die1==Die2)`、每面 1/6（两独立 d6）；**同步返回值 == `OnDiceRolled` payload**（抽取→固化→广播固化值→返回同一值）；同 `InitialSeed` + 同抽取序 → 同输出（同平台/编译配置；跨平台 off-by-one 降 Full Vision OQ-3）。

### 边界 2 — Economy（经济 5）

现金权威。持每玩家 `Cash`。所有金钱转移单一入口。

```cpp
UFUNCTION(BlueprintCallable) bool Credit(int32 PlayerId, int32 Amount, EChangeReason Reason);
UFUNCTION(BlueprintCallable) bool Debit (int32 PlayerId, int32 Amount, EChangeReason Reason); // 不足→不落负,转 Raising Funds
UFUNCTION(BlueprintCallable) int32 GetCash(int32 PlayerId) const;
UFUNCTION(BlueprintCallable) bool  IsInsolvent(int32 PlayerId, int32 AmountDue, int32 PreaggregatedNlv) const; // 严格 <
UFUNCTION(BlueprintCallable) int32 GetUnmortgageCostForDisplay(int32 MortgageValue) const;
UPROPERTY(BlueprintAssignable) FOnCashChanged        OnCashChanged;        // (Player,OldCash,NewCash,EChangeReason)
UPROPERTY(BlueprintAssignable) FOnRentPaid           OnRentPaid;           // (Payer,Payee,Amount,TileIndex)
UPROPERTY(BlueprintAssignable) FOnInsufficientFunds  OnInsufficientFunds;
UPROPERTY(BlueprintAssignable) FOnBankruptcyDeclared OnBankruptcyDeclared; // (Debtor,Creditor) 现金侧完成
```

- **caller MUST**：`Credit`/`Debit` 是**纯现金腿**，业务门控（`is_mortgaged`/`house_count==0`/垄断）归决策点/owner 校验；`IsInsolvent` 的 `PreaggregatedNlv` 须用 F-9 同口径聚合（identity-check，禁「总额==F-9」估算）；成对转移用**同一局部变量**做 `Debit(payer)`+`Credit(payee)`。
- **callee guarantees**：`Cash>=0`（`Debit` 超额转 Raising Funds 不破产）；`IsInsolvent` 用**严格 `<`**（付到恰好 0 不破产）；`Amount==0` 早返不广播；一次收租广播恰 1×`OnRentPaid` + 2×`OnCashChanged`；`EChangeReason` 只扩不改既有项。

### 边界 3 — PropertyOwnership（地产所有权 6）

归属权威。持 owner map + `bIsMortgaged`。垄断/计数为派生态（实时算，不存储）。

```cpp
UFUNCTION(BlueprintCallable) bool Buy(int32 TileIndex, int32 PlayerId);        // 校验→Debit→置owner→广播
UFUNCTION(BlueprintCallable) bool Mortgage(int32 TileIndex, int32 PlayerId);   // 校验→Credit(MV)→置标记→广播
UFUNCTION(BlueprintCallable) bool Unmortgage(int32 TileIndex, int32 PlayerId); // 校验→Debit(赎回费)→清标记→广播
UFUNCTION(BlueprintCallable) int32 GetOwner(int32 TileIndex) const;            // 无主→INDEX_NONE
UFUNCTION(BlueprintCallable) bool  IsMortgaged(int32 TileIndex) const;
UFUNCTION(BlueprintCallable) bool  IsMonopoly(int32 PlayerId, EColorGroup Group) const; // 空组→false
UFUNCTION(BlueprintCallable) int32 GetStationCount(int32 PlayerId) const;
UFUNCTION(BlueprintCallable) int32 GetUtilityCount(int32 PlayerId) const;
UFUNCTION(BlueprintCallable) FOwnershipSnapshot BuildOwnershipSnapshot(int32 TileIndex) const; // 回合2 push 给经济
UFUNCTION(BlueprintCallable) void TransferOwnership(int32 TileIndex, int32 NewOwner); // 破产9 调,in-kind 保留 bIsMortgaged
UFUNCTION(BlueprintCallable) void ReturnTileToBank(int32 TileIndex);                  // owner=NONE + 清标记(同格原子)
UPROPERTY(BlueprintAssignable) FOnOwnershipChanged OnOwnershipChanged; // (TileIndex,OldOwner,NewOwner)
UPROPERTY(BlueprintAssignable) FOnMortgageChanged  OnMortgageChanged;  // (TileIndex,bIsMortgaged)
```

- **caller MUST**：抵押前 `house_count==0` 前置由决策点（读 building 8）校验——6 读不到 `house_count`（防 6↔8 环）；算租须经 `BuildOwnershipSnapshot` push 给 economy（economy 不反调 6）；`TransferOwnership`/`ReturnTileToBank` 仅破产 9 可调，批量移交全或全无。
- **callee guarantees**：事务原子序「前置校验→现金腿(6→5)→改标记/owner→广播」；`IsMonopoly` 空组守门不 vacuous-true；破产批量移交逐格各广播一次；银行回退 owner 与 `bIsMortgaged` **同格原子清零**。

### 边界 4 — Building（建房 8）

房屋数权威。持 `house_count[tile] ∈ [0,5]`（5=酒店折档）。

```cpp
UFUNCTION(BlueprintCallable) EBuildResult BuildHouse(int32 TileIndex, int32 PlayerId); // 校验→Debit→++→广播
UFUNCTION(BlueprintCallable) bool         SellHouse (int32 TileIndex, int32 PlayerId); // 均衡卖→--→Credit(半价)→广播
UFUNCTION(BlueprintCallable) int32 GetHouseCount(int32 TileIndex) const;               // per-tile [0,5]
UFUNCTION(BlueprintCallable) bool  CanBuildHouse(int32 TileIndex, int32 PlayerId) const;
UFUNCTION(BlueprintCallable) int32 GetTotalHouseCount(int32 PlayerId) const;           // 仅档 1–4
UFUNCTION(BlueprintCallable) int32 GetTotalHotelCount(int32 PlayerId) const;           // 仅档 5
UFUNCTION(BlueprintCallable) TArray<FPlayerBuilding> GetPlayerBuildings(int32 PlayerId) const;
UFUNCTION(BlueprintCallable) int32 ForcedSellNextBuilding(int32 PlayerId);  // 返半价(调 Credit);无建筑→INDEX_NONE
UFUNCTION(BlueprintCallable) void  LiquidateAllBuildings (int32 Debtor);    // 银行破产专用:逐格清零、不调 Credit
UPROPERTY(BlueprintAssignable) FOnBuildingChanged OnBuildingChanged; // (TileIndex,NewHouseCount) — 2 字段
```

- **caller MUST**：抵押接缝 `GetHouseCount(tile)==0` 前置由决策点校验；`ForcedSellNextBuilding` 返 `INDEX_NONE`（无建筑）时清算驱动方转抵押腿（非错误）；RepairFee 统计须调 `GetTotalHouseCount`/`GetTotalHotelCount`（房/酒店分开）；区分 `LiquidateAllBuildings`（无偿）与 `ForcedSellNextBuilding`（返半价）。
- **callee guarantees**：`BuildHouse` 原子序「校验→Debit→`++`→广播」（Debit 失败不改状态，建房自愿买不起不触发破产）；`LiquidateAllBuildings` 逐格清零、**不调 Credit**（守 economy F-11，使破产 9「Credit==0」断言有对端支撑）；`OnBuildingChanged` 固定 2 字段。

### 边界 5 — Bankruptcy（破产 9）

return-only 编排子程序。不持久状态、不广播 `OnGameWon`（返回 winnerId 由回合2 触发）。

```cpp
UFUNCTION(BlueprintCallable)
FBankruptcyResult ResolveBankruptcy(int32 Debtor, int32 Creditor, const TArray<int32>& ActivePlayersSnapshot);
// creditor==INDEX_NONE 即银行破产; 绝不回调 2(防 2↔9 环)
USTRUCT(BlueprintType)
struct FBankruptcyResult { bool bDebtorEliminated; int32 WinnerId; FRankingEntry RankingEntry; };
UPROPERTY(BlueprintAssignable) FOnPlayerBankrupt OnPlayerBankrupt; // (Debtor,Creditor,Reason) 清算编排末
UPROPERTY(BlueprintAssignable) FOnPlayerRanked   OnPlayerRanked;   // (Player,Rank)
// 注:OnGameWon(winnerId) 由【回合2】触发广播,非本系统
```

- **caller MUST**：回合2 须在调用**前**自身确认 `IsInsolvent==true`（付到恰好 0 不破产，不调 9）；`creditor==INDEX_NONE` 表银行破产；`bIsBankrupt`/胜负信号写归回合2，**9 不直接广播 OnGameWon**。
- **callee guarantees**：**无环纪律**（对 2 公开接口调用 spy 计数恒 ==0）；向下调 5/6/8 清算全或全无；`WinnerId`：APC==1→其 Id，>1→INDEX_NONE，退化态→dev assert+fatal log；`OnPlayerBankrupt` 在资产移交**之后**广播恰 1 次；幂等守卫（对已出局 debtor 再调返回 `bDebtorEliminated=false`）。

### 边界 6 — Turn（玩家与回合 2）

流程编排者 + PlayerState owner。持回合状态机（`ETurnPhase` 枚举驱动，禁 Latent Action）。

```cpp
UFUNCTION(BlueprintCallable) void StartNewGame(const FGameSetupConfig& Config); // 接口名待 OQ-Lobby-1 冻结
// AI 决策三 hook(CR-8 同步契约;传入只读 GameStateSnapshot,return-only)
UFUNCTION(BlueprintCallable) bool                DecideBuyProperty(const FGameStateSnapshot& S, const FPropertyData& P);
UFUNCTION(BlueprintCallable) TArray<FTurnAction> DecidePostRollActions(const FGameStateSnapshot& S); // []=直接 EndTurn
UFUNCTION(BlueprintCallable) EJailAction         DecideJailAction(const FGameStateSnapshot& S);
UPROPERTY(BlueprintAssignable) FOnTurnStarted       OnTurnStarted;       // 双点链整链只发一次
UPROPERTY(BlueprintAssignable) FOnPhaseChanged      OnPhaseChanged;      // (Phase,ConsecutiveDoubles)
UPROPERTY(BlueprintAssignable) FOnTurnEnded         OnTurnEnded;         // (bGrantsExtraTurn)
UPROPERTY(BlueprintAssignable) FOnTurnOrderResolved OnTurnOrderResolved;
UPROPERTY(BlueprintAssignable) FOnAIActionExecuted  OnAIActionExecuted;  // (ActionIndex)
UPROPERTY(BlueprintAssignable) FOnGameWon           OnGameWon;           // (WinnerId) 承接 9 返回值
UPROPERTY(BlueprintAssignable) FOnBuildingAnnounced OnBuildingAnnounced; // (TileIndex,NewHouseCount,ActingPlayerId)
```

- **caller MUST**：AI hook 是**同步确定性调用**（单帧内返回，不阻塞）；AI hook return-only（只读 snapshot、不回调2）；呈现层订阅阶段事件须用 payload 携带语境（`ConsecutiveDoubles`/`bGrantsExtraTurn`），不可仅凭裸 `bIsDouble`。
- **callee guarantees**：状态机用 `ETurnPhase` 枚举 + delegate 推进（可序列化、`switch` 重入），**不用** `FTimerHandle`/Latent Action；`bIsBankrupt` 经 bankruptcy 返回值驱动写；`OnGameWon` 由本系统触发；`OnBuildingChanged` 只 2 字段，建房通告 `ActingPlayerId` 由 ResolvePhase 当前回合上下文 `CurrentPlayerId` 取（建房只在持有者自己回合）。

### 边界 7 — Board（棋盘数据 1）

静态数据 owner，零依赖 Foundation。持环序拓扑 + 每格 `FBoardTileData`。

```cpp
USTRUCT(BlueprintType)
struct FBoardTileData {
    ETileType TileType; EColorGroup ColorGroup; int32 PurchasePrice; int32 MortgageValue;
    int32 BuildingCost; TArray<int32> RentTable; int32 TaxAmount; int32 SalaryAmount; /* ... */ };
UFUNCTION(BlueprintCallable) int32          GetTileCount() const;                     // N
UFUNCTION(BlueprintCallable) FBoardTileData GetTileData(int32 TileIndex) const;       // 越界→断言
UFUNCTION(BlueprintCallable) TArray<int32>  GetTilesInGroup(EColorGroup Group) const; // 升序;None→空
UFUNCTION(BlueprintCallable) FName          GetBoardId() const;                       // BoardIdentifier,供存档21
UFUNCTION(BlueprintCallable) FAdvanceResult AdvanceIndex(int32 From, int32 Steps) const; // {NewIndex, PassedGo}
UFUNCTION(BlueprintCallable) FTransform     GetTileWorldTransform(int32 TileIndex) const; // 供 VFX19 hop path
```

- **入口语义**：纯只读服务，无受控写、无事件、无状态机（棋盘对局内不变）。被 movement/property/building/events/UI/VFX 读。
- **caller MUST**：`GetTileData(index)` 前用 `GetTileCount()` 约束 `index∈[0,N-1]`；`AdvanceIndex` 是**单一原子接口**（`(NewIndex,PassedGo)` 同次调用两分量，无独立 `PassedGoCount`）；`GetBoardId()` 返回作者手写稳定 `BoardIdentifier`，**禁**从资产路径派生。
- **callee guarantees**：`AdvanceIndex` 取余环算法（`AdvanceIndex(39,1)→0`、负步数取正余、多圈正确）；`GetTilesInGroup` 显式 `Sort` 升序；加载期反向校验（非 Property 格带 ColorGroup / 缺号重号 → 拒绝棋盘）。

### 边界 8 — Save（存档 21）

持久化横切层，enable-not-own。不拥有任何被序列化字段语义，只搬运。

```cpp
UFUNCTION(BlueprintCallable) ESaveResult SaveGame();   // 采集 CR-3 全字段→写 manifest→原子写盘
UFUNCTION(BlueprintCallable) ESaveResult LoadGame();   // 校验→加载 DA→反序列化→重建→发 OnGameLoaded
UFUNCTION(BlueprintCallable) bool        DoesSaveGameExist() const;
UPROPERTY(BlueprintAssignable) FOnGameLoaded OnGameLoaded; // 重建完成恰广播一次,呈现层据此重绑
```

CR-3 序列化清单见 §Data Flow D.3a（owner 逐字对齐）。

- **caller MUST**：任何状态系统**新增可序列化字段须登记 CR-3 表**（未登记 = 序列化遗漏 bug 源头）+ `CURRENT_SAVE_VERSION += 1`；Owner 字段经 owner 受控接口重建；呈现层读档后监听 `OnGameLoaded` 重绑各自 delegate。
- **callee guarantees**：**round-trip identity**（存档前快照 == 读档后快照逐字段 identity-check）；不存全量布局（DA 缺失→拒绝读档）；Raising Funds 瞬态拒绝保存；版本/魔数/校验和/manifest 任一不符 → 拒绝读档、当前对局不被破坏；写盘原子性（临时文件→替换）；读档重设新非确定骰子种子。

### Alpha / Full Vision 边界（轻量带过）

- **Trading(11)**：`DecideTradeResponse(GameStateSnapshot, FTradeOffer)` AI 被动响应 hook；现金/归属复用 `Credit`/`Debit` + `TransferOwnership`。
- **Auction(12)**：`DecideAuctionBid(GameStateSnapshot, PropertyData)→int32`（sentinel 负/INDEX_NONE=放弃）。
- **Fortune Wheel / Russian Roulette(13/14)**：经 DiceService `RandomRange`/`RandomFloat01` 抽取（可种子化）。
- **Networking(25)**：状态承载 `GameState`/`PlayerState` 已预留；RNG 须服务器单一权威 + 客户端收 RPC `FDiceRollResult`（绕开客户端浮点，OQ-3）。
- **Map Editor(26)**：产出 `DA_Board_*` + `BoardIdentifier`，经 Board 边界既有接口消费，无需新运行时接口。

---

## Engine Integration（引擎集成）

> **核验基准**：已对照 `docs/engine-reference/unreal/modules/{input,ui,rendering,audio}.md` + `current-best-practices.md` + `deprecated-apis.md`（pinned UE 5.7，verified 2026-02-13）。

### 1.1 输入 — Enhanced Input（B 域，5.7 现代范式）

**裁定**：全项目使用 **Enhanced Input**，**禁用 legacy `BindAction`/`BindAxis`/`GetInputAxisValue`**（deprecated-apis L11-18 已弃）。

- **Input Action（IA_）**：按交互意图建 Value Type 明确的资产，回合制棋盘交互绝大多数为 `Digital(bool)`：`IA_Roll`/`IA_BuyProperty`/`IA_BuildHouse`/`IA_EndTurn`/`IA_Confirm`/`IA_Cancel`/`IA_OpenMenu`；棋盘平移/缩放用 `Axis2D`/`Axis1D`。
- **Input Mapping Context（IMC_）**：按交互上下文分层，运行时经 `UEnhancedInputLocalPlayerSubsystem::AddMappingContext(Context, Priority)` 增删切换：`IMC_Gameplay`/`IMC_Menu`/`IMC_Modal`。**要点**：AI 回合移除 `IMC_Gameplay`，从输入层杜绝越权（优于 handler 内 `if(bIsMyTurn)`）。
- **绑定**：`UEnhancedInputComponent->BindAction(IA, ETriggerEvent::Started, this, &Handler)`。Trigger/Modifier 在 IA 资产内配置，不写死代码。长按确认用 `Hold` Trigger。
- **手柄就绪**：同一 IA 追加 Gamepad 映射即可，**无需改代码**——选 Enhanced Input 的核心收益。**避免 hover-only**：所有可操作元素须有显式点击/确认路径。

### 1.2 UI — UMG + CommonUI（B 域，5.7 现代范式）

**裁定**：UI = **UMG（视觉构建）+ CommonUI（输入感知/激活栈）**。所有屏幕级 Widget 继承 `CommonActivatableWidget`，按钮用 `CommonButtonBase`，天然 gamepad + mouse 双输入感知，满足「禁 hover-only」。

**BP-vs-C++ 绑定边界**（须 ADR 固化 — OQ-HUD-2/3、OQ-PC-4）：

| 层 | 归属 | 范式 |
|---|---|---|
| **数据 → 显示** | C++ 权威 → BP 呈现 | C++ 经 `meta=(BindWidget)` 持有 `TObjectPtr<UTextBlock/...>` 引用并设值；Widget 树/布局/动画在 WBP_ 内编辑 |
| **交互 → 意图** | BP 路由 → C++/事件 | 按钮 `OnClicked.AddDynamic` 或 BP `OnClicked`，**仅转发意图事件**，不写游戏逻辑 |
| **状态机/时序** | C++ 权威 | HUD 高亮状态机/VFX juice 状态由独立 C++ `UObject` 持有 + `IGameClock` DI（headless 可测硬前提，OQ-HUD-2/3） |

**呈现层纯叶子约束**（#16/#17/#19 共识）：只读/订阅/显示/转发意图，绝不写游戏状态。**性能**：静态面板用 Invalidation Box；非活动 Widget 用 `SetVisibility(Collapsed)` 而非 Hidden；避免 Widget `Tick` 轮询，改事件驱动。

### 1.3 渲染 — 卡通 2.5D 减法配置（A 域，逆 5.7 默认）

**裁定**：项目设置须**显式关闭** UE 5.7 默认开启的重型渲染特性：

| 特性 | 5.7 默认 | 本项目 | 配置点 |
|---|---|---|---|
| **Lumen GI** | 开 | **关** | `Rendering > Dynamic Global Illumination Method = None` |
| **Nanite** | 开 | **关** | Static Mesh 不启用 Nanite Support；棋盘/棋子/骰子低面数 |
| **Megalights** | 开(5.5+) | **关** | `Rendering > Megalights = Disabled` |
| **Substrate vs Legacy 材质** | Substrate(5.7 production) | **待 ADR** | 卡通材质二选一（ADR-0009） |

- **卡通材质选型（须 ADR-0009）**：legacy unlit/自定义节点对卡通描边/色阶更直接且团队熟悉；Substrate 提供未来一致性但 2.5D 收益有限。决策标准：Simplicity × Reversibility（材质框架后期迁移成本高）。
- **粒子 VFX**：一律 **Niagara，禁 Cascade**（Cascade fully deprecated）。
- **胜利暖金**：**Post Process Material**，经 Post Process Volume（`unbound=true`）或相机 `PostProcessSettings` 注入。
- 委派渲染管线实现交 `technical-artist` + `unreal-specialist`；材质 ADR 由 technical-director gate。

### 1.4 音频 — MetaSound + 三路混音（A 域）

**裁定**：音效合成用 **MetaSound Source** 程序化（非 Sound Cue，复杂逻辑下 Sound Cue 已弃）；三路混音总线 Sound Class 层级 `Master > {Music, SFX, UI}`，动态压低用 Sound Mix（`PushSoundMixModifier`/`PopSoundMixModifier`）；2D 为主（`PlaySound2D` 非空间化）；连续掷骰/金币爆发用 Sound Concurrency（Stop Oldest）防同帧叠音爆音。

### 1.5 物理 — Chaos 仅表现（A 域）

**裁定**：Chaos（UE 默认）**仅用于表现层**，不承载游戏逻辑。骰子/棋子滚动落定为视觉表现；**骰子点数由种子化 RNG 裁定（系统 #3），绝非物理模拟结果**——物理表现可与 RNG 结果解耦（确定性优先原则的直接体现）。

### 1.6 缺省系统标注（MVP 不启用）

| 引擎子系统 | MVP 状态 | 说明 |
|---|---|---|
| **Navigation(NavMesh)** | **无** | AI 启发式打分无寻路；棋子沿固定格序移动，不需 NavMesh |
| **Networking** | **无(MVP)** | 状态承载 `GameState`/`PlayerState` 结构为 Full Vision 联网(#25)预留 |
| **Animation(骨骼)** | **无** | 无 SkeletalMesh；棋子移动/UI 反馈用 UMG Animation/Timeline/程序化 Tween |

---

## ADR Audit + Required ADRs（ADR 审计 + 缺口）

> **审计日期**：2026-06-06 · **审计范围**：`docs/architecture/` 全量 · **审计人**：Technical Director

### A. 现状审计

**结论：本项目当前持有 0 个真 ADR。** `docs/architecture/` 下不存在任何 `ADR-NNNN-*.md`；现有文件均非架构决策记录：5 个 `change-impact-*.md`（设计变更跨档传播工单）+ `tr-registry.yaml`（空骨架，`requirements: []`）。

**审计发现的硬阻塞**：
- **F-A1（违反 coding-standards）**：编码标准明文「Every system must have a corresponding ADR in `docs/architecture/`」+「stories referencing a `Proposed` ADR are auto-blocked」。当前 16 个 Approved MVP 系统**零 ADR 背书**——任何 story 进实现将被 `/story-readiness` 阻断。**这是当前开工首要技术债。**
- **F-A2（TR Registry 未填充）**：`tr-registry.yaml` 仍是模板骨架，332 TR 无一落库稳定 ID。`/create-stories` 无法嵌入 TR-ID。须由 `/architecture-review` Phase 8 填充（与 ADR 并行，登记为前置）。
- **F-A3（架构债已在 GDD 显式登记）**：多个 Approved GDD verdict 明文将真问题 defer 到「架构期 ADR」并标为下游开工硬前提——本节即偿还此批债的施工图。

### B. GDD 中已登记的「待 ADR」债务台账

| 源 GDD | OQ 编号 | 债务内容 | GDD 自陈开工门控 |
|--------|---------|---------|-------------------|
| `dice.md` | OQ-1 / IG-3 | RNG 服务宿主 + 中央 `FRandomStream` 封装位置 | 移动(4)/AI(10) 前 |
| `board-data.md` | **OQ-6 ⚠ BLOCKING-ADR** | DataTable vs Primary Data Asset；DA 持有者宿主；`AdvanceIndex` 签名；热切换防 GC | **#4/6/7/8 实现前** |
| `ai-opponent.md` | OQ-AI-3 | `GameStateSnapshot` struct/字段构成（CR-6 字段清单为注入基准） | AI 实现前 |
| `ai-opponent.md` | **OQ-AI-3b** | AI 决策核心 BP vs C++ 裁定 | AI 实现前 |
| `hud.md` | **OQ-HUD-2** | 动画驱动机制 + 状态机抽独立 UObject + `IGameClock` DI（AC-12/24a/30/57 headless 硬前提） | HUD 实现前 |
| `hud.md` | OQ-HUD-3 | juice RNG 封装（私有 `FRandomStream` + DI spy） | HUD 实现前 |
| `property-card-ui.md` | OQ-PC-2/4 | UI handoff + widget 绑定（BindWidget/MVVM） | 卡牌 UI 实现前 |
| `audio.md` | OQ-Audio-4 | 纯原生 vs Wwise/FMOD；事件订阅 C++/BP 边界；`FRandomStream` 封装 | 音频实现前 |
| `tile-events.md` | OQ-Event-7 | engine ADR：非重入/宿主/RNG（指向 OQ-1） | 事件格实现前 |
| `player-turn.md` | OQ-1 | 回合状态机宿主 + `ETurnPhase` 枚举驱动（禁 Latent Action） | 回合实现前 |
| `property-ownership.md` | OQ-Prop-1 | owner map 生命周期 ADR（指向宿主决策） | 下游 8/9 前 |
| `save-load.md` | OQ-Save-1 | SaveGame 宿主 + 序列化契约 + delegate 重绑 + 牌堆种子序列化 | 存档实现前 |

### C. Required ADR 清单（按层分组，Foundation 先）

> **依赖拓扑**：`ADR-0001`（宿主）是 `0002`（Board 容器）/`0003`（Event Bus）/`0004`（RNG）/`0005`（存档）的共同前提。**0001 必须最先 Accepted。** 每条 ADR 走 `Proposed → Accepted`（跳过 Accepted 则引用它的 story 被自动阻塞）。模板 `.claude/docs/templates/architecture-decision-record.md`（必含 Status / Engine Compatibility / ADR Dependencies / GDD Requirements Addressed）。

#### 🔴 Foundation 层 — 开工硬前提（coding 前必须）

**ADR-0001 — UObject 宿主与生命周期边界**
覆盖：dice OQ-1 · board OQ-6（持有者分量）· player-turn OQ-1（状态机宿主）· property-ownership OQ-Prop-1 · tile-events OQ-Event-7 · save-load 宿主分量。**裁定**：统一对局期服务宿主——`UWorldSubsystem`（一局=World 边界 PIE 隔离）vs `UGameInstanceSubsystem`（跨关卡）vs `GameState UActorComponent`（联网友好），分别为 ① 棋盘 DA 持有者 ② RNG 服务 ③ owner map ④ 回合状态机 ⑤ GameStateSnapshot 生成方裁定宿主。关键约束：`Initialize()` 异步加载 handle 须 `Deinitialize()` 显式 `CancelHandle`（防 PIE Stop 空棋盘）；状态机禁 `FTimerHandle`/Latent Action，用 `ETurnPhase` 枚举 + delegate 推进。**最高优先，阻断全部下游。**

**ADR-0002 — 棋盘数据容器（DataTable vs Primary Data Asset）**
覆盖：board **OQ-6 ⚠ BLOCKING-ADR** · OQ-7。**裁定**：`FBoardTileData` 载体选 DataTable 还是 Primary Data Asset。**须先核验 UE5.7 知识缺口**：(a) DataTable CSV 是否支持 `TArray<int32>` 列（`RentTable`/`DiceMultiplierTable`，经典 UE 不支持→若 5.7 仍不支持改 JSON 行导入）；(b) 行内 `FText` 本地化 key；(c) `FStreamableManager`/`UAssetManager` 5.4+ API 变化。一并裁定 `GetBoardId()` 用 `BoardIdentifier:FName`（与资产路径解耦）、棋盘级元数据落位、`AdvanceIndex` 最终签名、地图编辑器多值输入。

**ADR-0003 — 事件总线与 Delegate 契约**
覆盖：economy `OnCashChanged`/`OnRentPaid`/`OnBankruptcyDeclared` · 跨档接缝 OQ-VFX-13 · `OnAIActionExecuted` · 方向派生契约。**裁定**：统一 multicast delegate 规范（全 `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable` + payload `USTRUCT(BlueprintType)`）；**reconcile OQ-VFX-13 破产事件接缝**（across bankruptcy9/economy5/HUD16/VFX19 统一，与已 Approved 姊妹档一致）；确立「payload 携带类型语境、方向由消费方从 delta 符号 + Payer/Payee 视角派生」原则。

**ADR-0004 — 确定性 RNG 服务**
覆盖：dice CR-2/3/F-2/4 · AI CR-5/AC-61 · VFX19 CR-9 / audio CR-7 / HUD CR-16 · OQ-AI-3b。**裁定**：单一权威 `FRandomStream` 服务（宿主由 ADR-0001 裁），C++ 封装为 `UFUNCTION(BlueprintCallable)`；钉死 `Min==Max` early-return / `Min>Max` ensure 不交换 / 默认种子恒 0 禁 lazy-init / 显式 `GetCurrentSeed()`/`SetSeed()` / 跨平台浮点风险登记降 Full Vision OQ-3 / 权威流 vs juice 流物理隔离。裁定 OQ-AI-3b（AI 决策核心是否强制 C++ 加静态符号扫描）。**实现前查 UE5.7 源码**（`RandRange` 是否仍走 `FRand()` 浮点）。

**ADR-0005 — 存档序列化契约**
覆盖：save-load CR-2/3/5/6/F-1/2 · 牌堆 Fisher-Yates 种子序列化 · 枚举 append-only · 49 持久化 TR。**裁定**：`USaveGame` 子类 + `UPROPERTY(SaveGame)` + `SaveGameToSlot/LoadGameFromSlot`；钉死 F-1 读档前置完整性门（magic/version/checksum/manifest）/ 存 DA 引用不存布局 / delegate 重绑 `OnGameLoaded`（与 ADR-0003 协同）/ 枚举 append-only / MVP 不存 Seed / 瞬态禁存档 / 牌堆种子序列化（与 ADR-0004 协同）。

#### 🟡 Core / Presentation 层 — 相关系统建前必须

**ADR-0006 — GameStateSnapshot 聚合契约（AI 只读视图）** | Core | AI(10) 前（依赖 0001/0002）
覆盖 AI OQ-AI-3。裁定 `FGameStateSnapshot` struct 字段构成（回合2 决策阶段生成只读聚合视图）；hook 签名传 `const FGameStateSnapshot&`（避免深拷贝）；CI-stub AC-5/6/7/48 关闭条件（OQ-1 ADR 关闭后同 sprint 转真实 `[Logic]`）。

**ADR-0007 — BP-vs-C++ 边界** | Core | AI/HUD/卡牌 UI 前
覆盖 OQ-AI-3b · OQ-HUD-2/3 · OQ-PC-2/4。裁定 AI 决策/RNG/状态机/经济公式归 **C++**（可审可测，防 BP 随机节点 uasset 二进制 diff 不可审），呈现/交互/widget 归 **BP**。把多个 GDD 的「BP 软约束→C++ 硬门」诚实降级条款一次性升格为可执行边界。

**ADR-0008 — HUD 动画驱动与 IGameClock DI** | Presentation | HUD/卡牌 UI 前（依赖 0001/0007）
覆盖 HUD OQ-HUD-2 · AC-12/24a/30/57。裁定逐帧动态求值动画驱动机制——**不可用烘焙固定时长 `UWidgetAnimation`**，滚动数字须每帧 `SetText(FText::AsNumber)`；状态机抽独立 `UObject` + `IGameClock` DI（否则 UMG `NativeTick` 在 `-nullrhi` 不触发 → false-green）。须裁定 NativeTick / Timeline / C++ 驱动三选一。

**ADR-0009 — 卡通材质管线（Legacy vs Substrate）** | Presentation | 美术资产前（可与 0010 并行）
覆盖 technical-preferences 卡通材质 · rendering.md。裁定 legacy vs Substrate（5.7 production-ready）。前置已定：关 Lumen/Nanite/Megalights、Niagara 非 Cascade、Post Process Material 暖金。**可降优先级**：若 legacy 满足 MVP，Substrate 迁移作 Alpha 评估。

**ADR-0010 — 音频架构（MetaSound vs Sound Cue + 中间件取舍）** | Core | 音频(22) 前（依赖 0003/0004/0007）
覆盖 audio OQ-Audio-4。裁定 ① MetaSound vs Sound Cue（倾向 MetaSound）；② Wwise/FMOD vs 纯原生（MVP 倾向原生）；③ 事件订阅 C++ vs BP（随 0007）；④ 音频随机 `FRandomStream` 封装（独立非权威流，随 0004）；⑤ Sound Class/Submix 三路混音图。须查 audio.md 5.7 API 变更。

#### 🟢 Input / UI 路由层 — 相关系统建前（有 fallback，可适度延后）

**ADR-0011 — Enhanced Input Mapping Context 结构** | Input | 首个交互前
覆盖 technical-preferences Input · input.md。裁定 IA 资产划分、IMC 分层（InGame/Menu/Modal）、`UEnhancedInputComponent` 绑定位置（PlayerController vs Pawn）、避免 hover-only。**有 fallback**：首个交互可单 context 起步，Reversibility 较高。

**ADR-0012 — CommonUI 输入路由与激活栈** | UI | HUD/菜单/卡牌 UI 前（可与 0008 同期，依赖 0007）
覆盖 technical-preferences UI · OQ-PC-2/4 · ui.md。裁定 `CommonActivatableWidget` 激活栈、`CommonButtonBase` 焦点/手柄导航、输入路由优先级（modal 截获）、`meta=(BindWidget)` 绑定边界。须查 ui.md 5.7 CommonUI API。

### D. 施工排程建议

```
Sprint 0（开工硬前提，必须全部 Accepted 才解锁实现）
  ADR-0001 宿主 ──┬──> ADR-0002 棋盘容器
                  ├──> ADR-0003 事件总线
                  ├──> ADR-0004 确定性RNG ──> ADR-0010 音频架构
                  └──> ADR-0005 存档契约
  （并行）/architecture-review Phase 8 填充 tr-registry.yaml（偿还 F-A2）

Sprint 1+（相关系统建前，依赖 Foundation）
  ADR-0007 BP/C++边界 ──┬──> ADR-0006 GameStateSnapshot（AI 前）
                        └──> ADR-0008 HUD驱动 ──> ADR-0012 CommonUI路由
  ADR-0011 Enhanced Input（首个交互前）
  ADR-0009 卡通材质（美术资产前，可降 Alpha 评估）
```

**TD 建议**：`ADR-0001` Accepted 后，`0002/0003/0004/0005` 可由 lead-programmer/engine-programmer 并行起草（互不触同文件，符合 agent-team 适用条件）。**强烈建议 `ADR-0001` 由 Technical Director 亲自主笔**——它是全部下游宿主决策的根，且要 reconcile board OQ-6 的 `UGameInstanceSubsystem` 推荐 vs World 边界 PIE 隔离这一未决张力。

**成功判据（we'll know this was right if）**：① Foundation 5 个 ADR Accepted 后 `/story-readiness` 对 #1/2/3/5 的 story 不再因「无 ADR / Proposed ADR」阻塞；② `tr-registry.yaml` 填充后 332 TR 全可被 story 引用；③ 第一个 [Logic] fixture 测试（dice 确定性序列）能在 `-nullrhi` headless 跑绿——证明 ADR-0001/0004 的宿主+RNG 封装真正可测。

---

## TR 域覆盖核查（Domain-Level Coverage）

> 332 TR 按域逐域确认架构有对应支撑（层 + 模块 + Required ADR）。逐 TR 精确矩阵留 `/architecture-review` Phase 8（填 `tr-registry.yaml`），本文档做**域级覆盖**。结论：**12 域全部有架构支撑，未覆盖域 = 0。**

| 域 | TR 数 | 架构支撑（层 / 模块 / ADR） | 覆盖 |
|----|-------|----------------------------|------|
| **Core** | 149 | Foundation+Core+Feature 全部 gameplay 模块（1/2/3/4/5/6/7/8/9/10）+ 全部 API 边界 + 防环三裁定；宿主 ADR-0001 / 事件 ADR-0003 / RNG ADR-0004 / BP-C++ ADR-0007 | ✅ |
| **Data** | 45 | 棋盘1（`FBoardTileData`）+ Board DA Holder 模块 + 数据驱动原则（原则1）；容器 ADR-0002 | ✅ |
| **UI** | 34 | Presentation 层 16/17/19/20 + Engine Integration §1.2；HUD 驱动 ADR-0008 / CommonUI ADR-0012 / Input ADR-0011 | ✅ |
| **Event** | 33 | Event Bus 模块 ② + Data Flow D.2 事件总览表（22 个 MVP 事件 owner→consumer）；ADR-0003 | ✅ |
| **Save-Load** | 29 | Save 框架模块 ③ + 横切层21 + Data Flow D.3 + CR-3 清单；ADR-0005 | ✅ |
| **Rendering** | 14 | Platform 层（Lumen/Nanite/Megalights=OFF）+ VFX19 + Engine Integration §1.3；卡通材质 ADR-0009 | ✅ |
| **Audio** | 12 | 横切层22 + Engine Integration §1.4（MetaSound + 三路混音）；ADR-0010 | ✅ |
| **AI** | 4 | Feature 层10（纯叶子 snapshot 消费）+ Data Flow D.1a；GameStateSnapshot ADR-0006 / BP-C++ ADR-0007 | ✅ |
| **Physics** | 4 | Platform 层 Chaos（仅表现）+ Engine Integration §1.5（点数归 RNG 非物理） | ✅ |
| **Input** | 4 | Engine Integration §1.1（Enhanced Input）；ADR-0011 | ✅ |
| **Animation** | 3 | Engine Integration §1.6（无骨骼，UMG Animation/Timeline/Tween）+ VFX19 | ✅ |
| **Navigation** | 1 | Engine Integration §1.6（无 NavMesh，固定格序移动） | ✅ |
| **合计** | **332** | — | **12/12 域全覆盖，0 未覆盖** |

> **覆盖小结**：332 TR 全部 12 域均落在某层 + 某模块 + （除 Physics/Nav 纯策略域外）某 Required ADR。重型域（Core 149 / Data 45 / UI 34 / Event 33 / Save 29）由 Foundation 5 个硬前提 ADR + Core/Presentation ADR 共同背书；轻量域（Physics/Input/Animation/Nav）由 Engine Integration 策略节直接收口。**无 orphan TR 域。**

---

## Architecture Principles（架构原则）

以下 5 条为全项目须遵循的架构原则，违反须经 technical-director ADR 豁免。每条标注其服务的决策标准。

### 原则 1 — 数据驱动（Data-Driven Gameplay）

**所有 gameplay 数值外置，绝不硬编码**（coding-standards 硬线）。租金表、建房成本、薪资、保释金、AI 三档参数、牌堆内容一律置于 `DataTable`/`DA_`/配置文件，经 GDD Tuning Knobs 节定义安全范围。**服务**：Maintainability（策划可调不改码）、Reversibility（平衡迭代零代码风险）。

### 原则 2 — 确定性优先（Determinism First）

**所有游戏逻辑随机性走单一种子化 RNG 流（系统 #3 拥有），物理仅表现、绝不裁定结果。** 骰子点数、牌堆抽取（Fisher-Yates 种子）、AI 决策内随机全部种子化可重放。**RNG 流隔离**：gameplay 逻辑流与表现/juice 流（VFX/HUD/audio）独立，禁复用骰子3 主流。确定性回归门冻结阈值，禁「通不过就调参」逃逸条款。**服务**：Testability（固定 seed → 零方差回归）、Correctness（联网状态同步根基）。

### 原则 3 — 依赖注入优于单例（DI over Singleton）

**可被测试覆盖的系统须经接口注入依赖，不直取全局单例。** `IGameClock` 注入而非直读 `GetWorld()->GetTimeSeconds()`（HUD/VFX 时序状态机 headless 可测硬前提）；AI 经 `GameStateSnapshot` 只读快照消费，不直取各系统单例；谨慎使用 `GameInstanceSubsystem`/`WorldSubsystem`（受控单例，被测逻辑须能脱离其运行）。**服务**：Testability（headless `-nullrhi` 可注入 mock）、Maintainability。

### 原则 4 — C++ 权威逻辑 / Blueprint 呈现交互（Authority Split）

**游戏规则/经济公式/状态机权威实现于 C++；呈现、交互路由、内容装配在 Blueprint。** C++：经济/建房/破产公式、回合状态机、AI 决策评分、骰子 RNG、跨系统接口契约。BP：HUD/卡牌 Widget 树、VFX 编排、菜单流、数据驱动内容装配。边界须 ADR（ADR-0007）。依赖方向：呈现层纯叶子，只被逻辑层广播事件驱动，**禁反向写**。**服务**：Performance（热路径在 C++）、Correctness（规则单一权威源）。

### 原则 5 — 事件解耦（Event-Driven Decoupling）

**跨系统协作经命名事件（C++ Multicast Delegate / BlueprintAssignable），非直接互调；依赖图必须无环。** 事件签名逐字对齐 owner GDD，双向一致性核验。**无环纪律**：2↔9 经 return-only 破环、5↔6 经事务归6单向调5、6↔8 经 house_count 归8。**任何新跨系统接口须先验证不引入环**（technical-director gate）。**PULL 优于 PUSH**（下游按需拉取上游数据）。**服务**：Maintainability、Reversibility（订阅方增删不动发布方）。

---

## Open Questions（架构期须裁，本文档未定）

> 以下为本文档识别、须在架构期裁定但本文档未收口的开放项。分两类：**(A) Required ADR 的具体选型**（裁定在对应 ADR 内）；**(B) GDD 登记的 producer 跨档债**（归 producer `/propagate-design-change` 协调，非架构裁定）。**均不阻 MVP 设计批准，但 (A) 类阻对应系统实现开工。**

### (A) Required ADR 内待裁的具体选型

| OQ | 收口于 | 待裁内容 |
|----|--------|----------|
| 服务宿主类型 | ADR-0001 | `UWorldSubsystem` vs `UGameInstanceSubsystem` vs `GameState Component`（reconcile board OQ-6 `UGameInstanceSubsystem` 推荐 vs World 边界 PIE 隔离张力） |
| 棋盘容器 | ADR-0002 | DataTable vs Primary Data Asset（须先核验 5.7 是否支持 CSV `TArray<int32>` 列） |
| 破产事件接缝 OQ-VFX-13 | ADR-0003 | `OnBankruptcyDeclared`(经济5,2字段) vs `OnPlayerBankrupt`(破产9,3字段) 职责切分 |
| 掷骰意图事件 OQ-VFX-16 | ADR-0003 | 「点击掷骰后」意图事件 owner（骰子3 vs 回合2），MVP fallback `OnPhaseChanged(RollPhase)` |
| AI BP-vs-C++ OQ-AI-3b | ADR-0004/0007 | AI 决策核心是否强制 C++ 以加静态符号扫描硬门 |
| GameStateSnapshot OQ-AI-3 | ADR-0006 | struct 字段构成 + 派生量预聚合 vs hook 内现算口径 |
| HUD 动画驱动 OQ-HUD-2 | ADR-0008 | NativeTick / Timeline / C++ 驱动三选一 + `IGameClock` DI |
| 卡通材质 | ADR-0009 | Legacy vs Substrate（卡通 cel-shading 迁移成本权衡） |
| 音频中间件 OQ-Audio-4 | ADR-0010 | MetaSound vs Sound Cue + 纯原生 vs Wwise/FMOD |
| 开局入口名 OQ-Lobby-1 | ADR-0001 协同 | `StartNewGame(const FGameSetupConfig&)` 接口名冻结 + USTRUCT 归属 |

### (B) GDD 登记的 producer 跨档债（归 producer，非架构裁定）

| OQ | 内容 | 状态 |
|----|------|------|
| **OQ-VFX-13** | 破产事件接缝 reconcile across bankruptcy9/economy5/HUD16/VFX19/audio22 | ✅ **RESOLVED 2026-06-08**（producer 裁定=呈现层统一订经济5 `OnBankruptcyDeclared`，破产9 `OnPlayerBankrupt` 不被呈现层订阅防双发；ADR-0003 §3 收口） |
| **OQ-VFX-7** | #2/#6/#1 depended-on-by 应含 #19 + 回链 AC 补全 | ✅ **RESOLVED 2026-06-08**（fresh-grep 实证已兑现：board/player-turn/property 下游表均含 VFX(19) 行；继承义务表 VFX(19) 回链 AC=AC-55/56/CR-4，vfx R-8 Approved；原 line 83/pending 为 stale） |
| **OQ-Audio-2** | 各 owner GDD 措辞补「音频(22)」消费方 + `PlayUISound` 在 UI 屏 GDD 登记被调义务 + 破产接缝 | ① ✅ **RESOLVED 2026-06-08**（owner GDD 已含音频22，fresh-grep 核实）② ⏸ **DEFERRED**（PlayUISound→UI 屏 GDD #16/#20/#23 多 Not Started，待撰写时落实）③ ✅ **RESOLVED**（=OQ-VFX-13 订经济5） |
| **RepairFee 单价 owner（OQ-Event-5）** | `per_house_fee`/`per_hotel_fee`（RepairFee 单价 25/100）owner 归属 | ✅ **RESOLVED 2026-06-08**（producer 裁定 owner=**事件牌 CardData DA**，费率属牌面数据非建筑成本；*非*建房8〔F-7 不拥有数值真值〕；tile-events registry 去 provisional） |
| **ADR-0007 AC 升格 propagate 债** | ADR-0007（2026-06-06 Accepted）裁定 AI 决策/RNG/经济/状态机落 C++ + 权威模块「无 BP 类」目录级断言 + C++ grep 禁全局 RNG 硬门。下游后果：hud（AC-47/AC-36c）、ai-opponent（AC-44）、property-card-ui（相关 RNG/绑定软约束 AC）中**逻辑已落 C++ 的部分**可由「BP 软约束 [Advisory·code-review]」升格为「C++ 硬门 [Logic]」（`static_assert`/`constexpr`/grep 兑现）；仍在 BP 的纯呈现部分维持 [Advisory]。**本次仅登记，不改三档 GDD**——升格落档归 producer，留 `/architecture-review` 阶段处理（执行时须对各档 fresh-grep 全集核对，旧行号仅作线索） | 登记，留 /architecture-review |

---

**关键文件路径（均绝对路径，只读核验依据）**：
- `D:\ClaudeProject\rento\Claude-Code-Game-Studios\design\gdd\systems-index.md`（Dependency Map L129-173 / Inherited Test Obligations L86-99 / Category L104-113 / High-Risk L217+ / registry）
- `D:\ClaudeProject\rento\Claude-Code-Game-Studios\design\gdd\{board-data,player-turn,dice,economy-cash,movement,property-ownership,tile-events,building-upgrade,bankruptcy,ai-opponent,hud,property-card-ui,vfx-feedback,main-menu-lobby,save-load,audio}.md`（16 个 Approved GDD）
- `D:\ClaudeProject\rento\Claude-Code-Game-Studios\.claude\docs\technical-preferences.md`（引擎策略 + Architecture Decisions Log 当前为空 → 本文档产 ADR-0001..0012）
- `D:\ClaudeProject\rento\Claude-Code-Game-Studios\docs\engine-reference\unreal\VERSION.md` + `modules/{input,ui,rendering,audio}.md` + `current-best-practices.md` + `deprecated-apis.md`（5.4–5.7 知识缺口 + HIGH/MED 风险口径来源）
- `D:\ClaudeProject\rento\Claude-Code-Game-Studios\.claude\docs\templates\architecture-decision-record.md`（ADR 模板）
- `D:\ClaudeProject\rento\Claude-Code-Game-Studios\docs\architecture\tr-registry.yaml`（空骨架，待 `/architecture-review` Phase 8 填充 332 TR）
