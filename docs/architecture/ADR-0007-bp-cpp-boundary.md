# ADR-0007: BP-vs-C++ 边界（Blueprint / C++ Authority Boundary）

## Status

Accepted

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

msc（用户）+ Technical Director — 2026-06-06 Accepted。用户裁定：分叉一选 **B（关键逻辑 C++ + 边缘 BP）**，分叉二选 **硬门分级落地**（MVP：C++ grep 禁全局 RNG + 权威模块目录级「无 BP 类」断言；逐 BP 节点静态扫描降 Alpha）。

## Summary

本项目「Blueprint 为主」的引擎策略与多份 GDD 的「确定性 / headless 可测 / 防 RNG 漂移」硬要求之间存在张力——BP 图是 `uasset` 二进制，无法机器审计、无 `const` 语义、随机节点可绕过权威 RNG 流。本 ADR 裁定 **C++/BP 职责边界**：AI 决策、RNG、经济/建房/破产公式、回合状态机、存档序列化等**确定性权威逻辑落 C++**（可审、可测、可加静态符号扫描硬门），呈现 / 交互 / UMG widget / 内容装配落 BP，并裁定「禁 BP 随机节点」的强制强度（静态符号扫描硬门 vs code-review 软约束），把散落在 AI / HUD / 卡牌 UI 多档的「BP 软约束 → C++ 硬门」诚实降级条款一次性升格为可执行的项目级边界。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core / Scripting |
| **Knowledge Risk** | LOW — BP/C++ 互操作（`UFUNCTION`/`UPROPERTY`/`UCLASS`/`BlueprintCallable`/`BlueprintImplementableEvent`）是 UE 长期稳定基础设施，pre-5.3 与 5.7 行为一致；本 ADR 不依赖任何 post-cutoff API |
| **References Consulted** | `docs/engine-reference/unreal/current-best-practices.md`（BP/C++ 边界范式）、`docs/engine-reference/unreal/VERSION.md`（知识缺口）；架构原则 4（Authority Split） |
| **Post-Cutoff APIs Used** | None — 本 ADR 是组织/边界裁定，不引入 5.4+ 新 API。`BindWidget`/MVVM 等具体绑定形式的 5.7 范式核验下放给 ADR-0008/ADR-0006（widget 侧）。 |
| **Verification Required** | 静态符号扫描门若采用，须在 CI 验证：(a) 能解析 `uasset`/导出文本以检出 BP 随机节点（`Random Integer in Range` / `Random Float` 等 K2Node）；(b) 能区分白名单（经 #3 骰子封装的 `UFUNCTION`）与裸引擎随机节点。MVP 可先以「权威逻辑无 BP 类」的目录级断言替代逐节点扫描（见 Implementation Guidelines）。 |

> **Note**: Knowledge Risk = LOW，引擎升级一般无须重审本 ADR；但若 5.x 改变 BP K2Node 导出格式致静态扫描失效，须重验 Verification Required (a)。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001（Subsystem 宿主 — 确定 C++ 权威逻辑挂载的 `UWorldSubsystem`/`UGameInstanceSubsystem` 宿主，本 ADR 据此约定「权威逻辑活在 C++ 宿主，非 BP Actor 图」） |
| **Enables** | ADR-0008（HUD 动画驱动 — 须先有「状态机/驱动落 C++」边界才能定 UObject 抽离 + `IGameClock` DI）；ADR-0010（音频架构 — 事件订阅 C++ vs BP 随本 ADR）；ADR-0006（GameStateSnapshot — hook 签名 C++ 落点随本 ADR）；ADR-0012（CommonUI 路由 — 交互层 BP 边界随本 ADR） |
| **Blocks** | AI(10) 决策实现、HUD(16) / 卡牌 UI(17) 状态机实现、经济(5)/建房(8)/破产(9) 公式实现——这些系统的「哪层落 C++」前提是本 ADR Accepted |
| **Ordering Note** | 本 ADR 是 §8 Sprint 1+ 拓扑根之一：`ADR-0007 → {ADR-0006, ADR-0008 → ADR-0012}`。须在任一 gameplay 权威系统或呈现状态机开工前 Accepted。RNG 节点的*封装实现*归 ADR-0004；本 ADR 只裁*边界与强制强度*，二者协同（OQ-AI-3b 同时挂 0004/0007）。 |

## Context

### Problem Statement

`technical-preferences.md` 把本项目定为「Blueprint 为主（gameplay/UI），C++（框架 & 性能关键系统）」。但 `coding-standards.md` 要求「所有 public 方法可单元测试（DI over singleton）」「gameplay 数值数据驱动」，而多份 Approved GDD 把若干 `[Logic] BLOCKING` AC 的 headless 可测性、确定性重放显式登记为**待架构期裁定的硬前提**：

- **ai-opponent OQ-AI-3b**：「BP 为主项目无法机器强制禁 BP 随机节点（`uasset` 二进制 diff 不可审），AC-44 软约束不足；ADR 须裁定 AI 决策核心是否强制落 C++ 以加静态符号扫描（真硬门）」——这是**确定性义务②的可执行性前置**。
- **hud OQ-HUD-3 / AC-47 / AC-36c**：「juice RNG 隔离 + 固定常量不可改」当前为 `[Advisory·code-review]` 软约束，「BP 无语言级 `const` 语义」；若架构选 C++ 强封装 / `constexpr`，可升 `[Logic]` 静态扫描 / `static_assert`。
- **property-card-ui OQ-PC-4 / AC-3 / AC-34b**：BP 路径下「`Render*` 方法名不可靠 spy」「BP `Bind Event` 双绑 footgun，`-nullrhi` 下 C++ spy 看不见 BP 绑定层」——绑定层落 BP 还是 C++ 直接决定 headless 可覆盖性。
- **property-card-ui OQ-PC-2**：HUD↔#17 handoff 接口（`OpenCard` + TileIndex）落 event/delegate/直调，是 BP↔C++ 边界设计问题。

不在架构期一次性裁定，则每个系统各自即兴决定「这段落 BP 还是 C++」，导致：权威逻辑散落 BP 图不可审、RNG 漂移（绕过 #3 骰子流破坏确定性重放与联网预留）、headless 测试 false-green（名义 `[Logic]` 实则跑不到）。**成本**：确定性义务（架构原则 2）与可测性义务（原则 3）在实现期系统性塌方，且 BP 二进制使回归不可审。

### Current State

尚无 C++ 代码。架构原则 4（Authority Split）已确立方向性原则——「游戏规则/经济公式/状态机权威实现于 C++；呈现、交互路由、内容装配在 Blueprint」，并显式声明「边界须 ADR（ADR-0007）」。本 ADR 即把原则 4 从方向性陈述**细化为可执行边界 + 强制机制**。Foundation ADR-0001~0005 已 Accepted，宿主/RNG/事件/存档框架已定 C++ 落点，为本 ADR 提供承载基座。

### Constraints

- **引擎/团队**：项目策略「BP 为主」——纯 C++ 会拖慢迭代、抬高 UI/内容装配门槛，且放弃 BP 热重载与策划可视化编辑优势。
- **可审计性**：BP 图存为 `uasset` 二进制，git diff 不可读、code-review 无法可靠审查随机节点 / 硬编码数值 / 绑定双订阅。C++ 是文本，可 grep / 可静态扫描 / 可 `static_assert`。
- **确定性（原则 2 硬线）**：所有 gameplay 随机性须走 #3 骰子单一种子化 `FRandomStream`；BP 内置 `Random Integer in Range` / `Random Float` 节点走引擎全局 RNG，**绕过**权威流 → 破坏重放与联网状态同步根基。
- **可测性（原则 3 硬线）**：`[Logic] BLOCKING` AC 须 headless（`-nullrhi`）可跑。BP 图在 `-nullrhi` 下逻辑可执行但 `UMG NativeTick` 不触发、BP `Bind Event` graph 层 C++ spy 不可见 → 名义可测实则 false-green。
- **CI 能力**：静态符号扫描门须 CI 能解析 BP 资产或其导出文本——MVP CI 成熟度未知（`/test-setup` 待定），硬门可行性须分级。

### Requirements

- **R1（边界裁定）**：明确枚举哪些系统/子系统的逻辑**必须**落 C++，哪些**应当**落 BP，给出可据以 review 的判据（不留「视情况」灰区）。
- **R2（确定性强制）**：裁定「禁 BP 随机节点旁路 #3 骰子流」的强制强度——硬门（CI 拒绝）还是软约束（reviewer 签字）。
- **R3（可测性兑现）**：边界须使 `[Logic] BLOCKING` AC（AI 决策 / 经济公式 / 状态机转换）真正 headless 可测，不依赖 UMG/BP graph 层。
- **R4（BP↔C++ 接口形态）**：定 BP↔C++ 双向调用的标准形态（`UFUNCTION(BlueprintCallable)` 暴露权威接口、`BlueprintImplementableEvent` / delegate 回呈现层、widget 绑定形式选型边界）。
- **R5（务实性）**：不牺牲「BP 为主」的迭代速度与内容装配优势；C++ 范围限定在「确定性权威 + 可测性硬前提」最小集，不蔓延到呈现/装配。

## Decision

> **本节按真分叉逐选项列代价 + 理由。最终选型：分叉一 = B，分叉二 = 硬门分级落地（2026-06-06 由 msc Accepted）。备选项分析保留作记录。**

### 分叉一：权威逻辑切分粒度

确立「逻辑落 C++ / 呈现落 BP」的**切分线画在哪**。

| 选项 | 含义 | 服务的标准 | 代价（牺牲） |
|------|------|-----------|-------------|
| **A. 权威逻辑全 C++** | AI 决策 + RNG + 经济/建房/破产全部公式 + 回合 & UI 状态机 + 存档序列化 + 全部跨系统接口契约 **全部** C++；BP 仅 widget 树/动画/材质/菜单流/内容装配（纯叶子呈现） | Testability（全权威逻辑 headless 可跑、可静态审）、Correctness（规则单一权威源、零 BP 漂移）、Performance（热路径全 C++） | BP 迭代优势仅剩纯呈现；UI 状态机（HUD 高亮/卡片刷新）落 C++ 抬高 UMG 程序员门槛、widget 与其驱动逻辑跨语言；开发初速较慢 |
| **B. 关键逻辑 C++ + 边缘 BP（务实）** | **确定性 & 可测性硬前提**落 C++：AI 决策、RNG 封装、经济/建房/破产公式、回合状态机、存档序列化、HUD/卡片**状态机与求值逻辑**（抽独立 `UObject`）；BP 承载 widget 树 + 动画播放 + 事件订阅转发 + 菜单流 + 内容装配 + 非确定性纯呈现交互 | Testability（确定性 + headless 硬前提那部分 C++ 可测）、Maintainability（BP 留给可视化收益最大处）、务实迭代速度 | 边界须靠判据维持，存在「这段算逻辑还是呈现」的灰区争议；HUD 状态机 C++ 化但 widget BP，仍跨语言（但 ADR-0008 已规划此抽离） |
| **C. BP 为主 + 约定** | gameplay 与 UI 逻辑**默认 BP**，仅性能/确定性极端处下沉 C++；RNG 隔离、const、绑定靠 code-review 约定 | 开发初速最快、UI 全 BP 可视化、策划可改图 | **确定性义务②不可执行**（BP 随机节点不可机器禁）；`[Logic] BLOCKING` AC 大面积 false-green（BP graph `-nullrhi` 不可靠）；权威逻辑 `uasset` 不可审 → 回归不可信。**直接抵触原则 2/3 硬线** |

**裁定：B（关键逻辑 C++ + 边缘 BP）。（Accepted）**

**理由**：A 用「全 C++」买到极致可审可测，但牺牲了项目立项时明确选择的「BP 为主」迭代优势——尤其 UI/内容装配是 BP 收益最大处，全 C++ 是过度防御。C 开发最快但**直接违反架构原则 2（确定性）与原则 3（可测性）的硬线**：BP 随机节点不可机器禁 = 确定性义务②（OQ-AI-3b）落空，`uasset` 不可审 = 回归不可信，是不可接受的技术债。B 把 C++ 范围**精确限定在确定性权威 + headless 可测硬前提**（这正是 GDD 反复登记为「待 ADR」的那批 AC 的根因），其余全留 BP——既兑现原则 2/3 硬线，又保住 BP 为主的迭代收益。B 与现有架构原则 4 的措辞、ADR-0008（HUD 状态机抽 `UObject`）、ADR-0006（hook 签名 C++）已规划的方向**完全一致**，是阻力最小、债务最低的边界。灰区争议用下方 Implementation Guidelines 的判据收口。

精确边界（B 选项下的归属表）：

| 落 **C++**（权威 / 确定性 / 可测硬前提） | 落 **BP**（呈现 / 交互 / 装配） |
|------|------|
| AI 决策评分（10，F-1..F-7 启发式打分、`Decide*` hook）— OQ-AI-3b | HUD/卡牌 widget 树、绑定、布局（16/17，`WBP_*`） |
| RNG 服务（3，`FRandomStream` 封装为 `BlueprintCallable`）— ADR-0004 协同 | UMG 动画播放、Tween、Timeline 触发（呈现侧） |
| 经济/建房/破产公式（5/8/9，`Debit`/`Credit`/`CalcRent`/`ResolveBankruptcy`/`net_worth`） | VFX 编排、Niagara 触发、Post Process（19，`BP_*`） |
| 回合状态机（2，`ETurnPhase` 推进、定序、双点链） | 菜单流 / 大厅配置编辑 UI（20） |
| HUD/卡片**状态机与求值逻辑**抽独立 `UObject`（16/17）— ADR-0008/0006 落具体 | 事件订阅 → 显示映射的**转发**（呈现叶子，订阅 C++ delegate、播 BP 动画） |
| 存档序列化契约（21，`UPROPERTY(SaveGame)` + round-trip）— ADR-0005 协同 | 数据驱动内容装配（`DA_`/`DataTable` 引用挂接） |
| 跨系统接口契约 / multicast delegate 声明（各 owner，`USTRUCT` payload）— ADR-0003 协同 | 音频事件订阅 → MetaSound 播放（22，可随 ADR-0010 选 C++ 订阅） |

### 分叉二：禁 BP 随机节点的强制强度

裁定「gameplay 逻辑禁用 BP 内置随机节点（绕过 #3 骰子流）」如何强制。

| 选项 | 含义 | 服务的标准 | 代价（牺牲） |
|------|------|-----------|-------------|
| **硬门（静态符号扫描）** | CI 步骤解析 BP 资产/导出文本，检出权威逻辑路径上的 `Random Integer in Range`/`Random Float` 等裸 K2Node → **CI 拒绝合并**；白名单仅 #3 骰子封装的 `UFUNCTION` | Testability/Correctness（确定性义务②**真硬门**，机器强制不可漂移）、Reversibility（违规即时拦截，债不累积） | 须建 BP 静态扫描工具（CI 复杂度↑）；MVP CI 成熟度未知，工具开发是前置成本；`uasset` 解析脆弱（5.x 格式变可能失效） |
| **软约束（code-review）** | RNG 隔离靠 reviewer 静态审 BP 图 + 签字（现状 AC-44/AC-47 `[Advisory·code-review]`） | 零工具成本、即刻可用 | **不可机器强制**——reviewer 漏审即漏（BP 图易藏随机节点）；确定性义务②实质不可执行（OQ-AI-3b 的根本担忧未解）；与「B 选项下 AI 决策已落 C++」叠加才勉强缓解 |

**裁定：硬门，分级落地——以「权威逻辑无 BP 类」目录级断言为 MVP 起点，逐节点扫描作 Alpha 增强。（Accepted）**

**理由**：OQ-AI-3b 的核心论点是「软约束不足」——若停在 code-review，确定性义务②（AI 决策内随机可重放）就只是纸面要求，reviewer 一漏即破，且 `uasset` 二进制使漏审概率高。但「逐 BP 节点静态扫描」工具在 MVP CI 未定型时是重前置投资。**关键洞察**:在分叉一选 B 后，AI 决策 / 经济 / RNG 已**全部落 C++**，则「BP 随机节点旁路」的攻击面大幅收窄——权威逻辑根本没有 BP 类。故 MVP 阶段硬门可退化为**廉价的目录级断言**：CI 断言 `Source/.../Authority/` 等权威模块**无 BP 派生类**、且权威 C++ 代码 grep 无 `FMath::Rand`/`FMath::RandRange`/全局 RNG（C++ 文本可 grep，这是免费硬门）。逐 BP 节点扫描留给 Alpha（届时若有 BP 触碰 gameplay 边缘，再上工具）。这样**既兑现「真硬门」的实质**（权威随机性机器保证走 #3 流），又不在 MVP 背上 BP 静态分析工具的前置成本。纯软约束（选项右）被否——它正是 OQ-AI-3b 判定「不足」的现状。

强度落地分级：

| 阶段 | 强制手段 | 覆盖 |
|------|---------|------|
| **MVP** | ① 权威逻辑落 C++（分叉一 B 的结构性保证）② C++ 权威模块 CI `grep` 禁 `FMath::Rand*`/全局 RNG（文本硬门，免费）③ 目录级断言：权威模块无 BP 派生类 | AI/RNG/经济/状态机的随机性 100% 走 #3 流（结构上无 BP 旁路） |
| **Alpha** | 增 BP 资产静态符号扫描（若届时 BP 触碰 gameplay 边缘机制如交易/拍卖 AI hook） | 边缘 BP 逻辑的随机节点检出 |

此分级使 hud AC-47/AC-36c、ai AC-44 从 `[Advisory·code-review]` **升格依据**明确：凡逻辑已落 C++ 的部分，其 RNG 隔离 / const 由 C++ `static_assert`/`constexpr`/grep 门兑现 → 可升 `[Logic]`；仍在 BP 的纯呈现部分维持 code-review。

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  C++ AUTHORITY CORE  (确定性 / 可审 / headless 可测)               │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐    │
│  │ AI 决策   │ │ RNG 服务  │ │ 经济/建房 │ │ 回合 & UI 状态机  │    │
│  │ (10)     │ │ (3)      │ │ /破产公式 │ │ (2 / 16·17 UObj) │    │
│  │          │ │ FRandom  │ │ (5/8/9)  │ │  ETurnPhase 驱动  │    │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────────┬─────────┘    │
│       │  UFUNCTION(BlueprintCallable) 暴露权威接口  │              │
│       │  DYNAMIC_MULTICAST_DELEGATE(BlueprintAssignable) 广播事件  │
└───────┼────────────┼────────────┼─────────────────┼──────────────┘
        │ ▲ 同步调用   │            │ ↝ 事件多播(单向,禁反向写)         │
        ▼ │ return     ▼            ▼                 ▼
┌─────────────────────────────────────────────────────────────────┐
│  BLUEPRINT PRESENTATION / INTERACTION  (纯叶子 — 订阅·显示·转发意图) │
│  WBP_HUD / WBP_PropertyCard │ BP_VFX(Niagara) │ WBP_Lobby │ 装配  │
│  订阅 C++ delegate → 播 UMG 动画/Niagara → 转发玩家意图回 C++ 接口    │
└─────────────────────────────────────────────────────────────────┘
        边界铁律: BP 禁写权威状态 · BP 禁裸随机节点(走 #3 UFUNCTION) ·
                  权威逻辑禁出现 BP 派生类
```

### Key Interfaces

```cpp
// 1) C++ 权威逻辑暴露给 BP 呈现层的标准形态：BlueprintCallable 查询 + Assignable 事件
//    （具体 payload USTRUCT 归 ADR-0003；此处示边界形态）
UCLASS()
class UEconomySubsystem : public UWorldSubsystem  // 宿主归 ADR-0001
{
    // 权威结算：同步调用，结果在返回时已确定（铁律：结算先行，呈现后随）
    UFUNCTION(BlueprintCallable, Category="Economy")
    int32 CalcRent(const FRentContext& Context) const;   // 纯函数,可 headless 单测

    // 呈现层订阅：owner 先算定再广播,BP 各订各播,禁回写
    UPROPERTY(BlueprintAssignable, Category="Economy")
    FOnCashChanged OnCashChanged;
};

// 2) RNG 唯一入口：BP 须经此 UFUNCTION,禁用 BP 内置 Random 节点(分叉二硬门)
UCLASS()
class UDiceRngService : public UWorldSubsystem
{
    UFUNCTION(BlueprintCallable, Category="RNG")   // 白名单:唯一允许的 BP 随机来源
    int32 RandomRange(int32 Min, int32 Max);       // 内部 FRandomStream,种子化可重放
    // 禁止:BP graph 内 K2Node "Random Integer in Range" 触碰 gameplay
};

// 3) 呈现状态机抽独立 UObject(非 UMG widget),IGameClock DI → headless 可测
//    (具体驱动机制 NativeTick/Timeline/C++ 三选一归 ADR-0008;此处定其为 C++ UObject 边界)
UCLASS()
class UHudHighlightStateMachine : public UObject   // 非 UUserWidget → -nullrhi 可实例化
{
public:
    void Init(TScriptInterface<IGameClock> InClock);     // DI,非直读 GetWorld()->GetTimeSeconds()
    void OnTurnStarted(int32 PlayerId);                  // 状态转换,spy 可断言转换帧数==1
    UFUNCTION(BlueprintCallable) EHighlightState GetState(int32 PlayerId) const; // OQ-PC-4(i) spy 输出接口
    // WBP_HUD(BP) 持有本 UObject,每帧读 GetState() 驱动 widget 视觉(呈现侧)
};

// 4) BP↔C++ handoff(OQ-PC-2): HUD→#17 OpenCard 经 C++ 接口直调(非 BP 直连),TileIndex 显式参数
UFUNCTION(BlueprintCallable, Category="UI")
void OpenCard(int32 TileIndex);   // C++ 暴露,WBP_HUD 调用,可 spy 调用次数/参数
```

### Implementation Guidelines

**边界判据（review 据此裁「该落 C++ 还是 BP」，消灭灰区）：**

1. **写权威状态？→ C++**。任何写 `PlayerState`/owner map/`house_count`/`Cash`/牌堆顺序/状态机 phase 的逻辑必落 C++（原则 4 + 单一 owner 不变式）。
2. **含 gameplay 随机性？→ C++ 且经 #3 `UFUNCTION`**。绝不用 BP 内置 Random 节点。juice/呈现随机（VFX 抖动）走独立非权威流（ADR-0004），仍经 C++ 封装。
3. **是 `[Logic] BLOCKING` AC 的被测逻辑？→ C++ 独立 `UObject`/纯函数**。须 `-nullrhi` 可实例化（裸 `NewObject` 或 mock World）、可注入 `IGameClock`、暴露可 spy getter（OQ-PC-4 (i)）。不得依赖 `UMG NativeTick` 或 BP `Bind Event` graph 层。
4. **是 widget 树/动画播放/材质/菜单流/内容装配？→ BP**。这是 BP 收益最大处。
5. **是事件订阅→显示的转发？→ BP 叶子**，但订阅的是 C++ `BlueprintAssignable` delegate；BP 仅「收事件→播动画/SetText」，禁回写。
6. **BP↔C++ handoff（如 OpenCard）→ C++ 暴露 `BlueprintCallable`**，BP 调用方经此入口（非 BP 直连 BP），使调用可 spy（OQ-PC-2）。

**禁 BP 随机节点硬门（分叉二，MVP 分级）：**

- CI 对权威 C++ 模块 `grep` 禁 `FMath::Rand`、`FMath::RandRange`、`FMath::FRand`、`rand(`、`std::rand`（文本硬门，免费）。白名单：`UDiceRngService` 内 `FRandomStream` 调用。
- CI 目录级断言：权威模块（AI/经济/建房/破产/回合/RNG）目录下**无 BP 派生类资产**（结构保证无 BP 随机旁路）。
- Alpha：若 BP 触碰 gameplay 边缘（交易/拍卖 AI hook），增 BP 资产静态符号扫描检出裸随机 K2Node。

**const / 固定常量（hud AC-36c/AC-47 升格依据）：**

- 凡逻辑落 C++ 处，「不可改的固定时长/参数」用 `static constexpr` + `static_assert`，对应 AC 从 `[Advisory·code-review]` 升 `[Logic]`。
- 仍在 BP 的呈现参数（无 `const` 语义）维持 `[Advisory·code-review]`（reviewer 签字）。

## Alternatives Considered

### Alternative 1: 权威逻辑全 C++（分叉一 A）

- **Description**: AI/RNG/经济/状态机/存档**及全部 UI 状态机与逻辑** 全部 C++，BP 仅纯呈现资产。
- **Pros**: 极致可审可测，零 BP 漂移，热路径全 C++。
- **Cons**: 牺牲项目立项明确选择的「BP 为主」迭代优势；UI/内容装配（BP 收益最大处）被迫 C++，抬高门槛、拖慢初速；属过度防御。
- **Estimated Effort**: 高于 B（UI 状态机全 C++ + widget 跨语言绑定成本）。
- **Rejection Reason**: C++ 范围超出「确定性 + 可测硬前提」最小必要集，蔓延到无确定性需求的呈现/装配，违背务实性要求 R5。

### Alternative 2: BP 为主 + 约定（分叉一 C）

- **Description**: gameplay/UI 逻辑默认 BP，仅极端处下沉 C++，RNG/const/绑定靠 code-review。
- **Pros**: 开发初速最快，UI 全 BP 可视化，策划可改图。
- **Cons**: 确定性义务②不可机器执行（OQ-AI-3b 判定「软约束不足」的正是此现状）；`[Logic] BLOCKING` AC 大面积 false-green；权威逻辑 `uasset` 不可审 → 回归不可信。
- **Estimated Effort**: 初期最低，但确定性/可测债在实现期与联网阶段集中爆发，总成本最高。
- **Rejection Reason**: 直接抵触架构原则 2（确定性）与原则 3（可测性）硬线，不可接受。

### Alternative 3: 纯 code-review 软约束禁随机（分叉二右）

- **Description**: BP 随机节点禁用仅靠 reviewer 静态审图 + 签字。
- **Pros**: 零工具成本，即刻可用。
- **Cons**: 不可机器强制，reviewer 漏审即破（`uasset` 二进制易藏随机节点），OQ-AI-3b 根本担忧未解。
- **Rejection Reason**: 即 OQ-AI-3b 判定「不足」的现状；被「C++ 文本 grep 硬门 + 目录级断言」廉价取代（推荐方案在 B 之上几乎免费实现真硬门）。

## Consequences

### Positive

- 确定性义务②（RNG 可重放）由结构（权威逻辑无 BP 类）+ C++ grep 硬门机器保证，OQ-AI-3b 收口为可执行边界。
- AI 决策 / 经济公式 / 状态机转换的 `[Logic] BLOCKING` AC 真正 headless 可测（C++ `UObject` + `IGameClock` DI），不再 false-green。
- hud AC-47/AC-36c、ai AC-44 等「`[Advisory·code-review]`→`[Logic]`」升格有了明确依据（C++ 部分用 `static_assert`/grep 门）。
- 保住「BP 为主」迭代优势：UI/VFX/装配留 BP，C++ 范围限定在最小必要集。
- 为联网预留（25）打基：权威逻辑全 C++ + 确定性 RNG = 状态同步根基。

### Negative

- 边界存在「这段算逻辑还是呈现」灰区，依赖 Implementation Guidelines 判据 + TD review 维持，非机器自动判定。
- HUD/卡片状态机 C++ 化 + widget BP，存在跨语言协作成本（由 ADR-0008 的 `UObject` 抽离 + DI 规约消化）。
- C++ 文本 grep 硬门覆盖 C++ 旁路，但 BP 边缘随机的逐节点扫描留到 Alpha——MVP 期 BP 触碰 gameplay 的边缘（若有）暂靠目录级断言 + review。

### Neutral

- 团队须同时具备 BP 与 C++ 能力（项目 specialist 路由已区分 unreal-specialist / ue-blueprint-specialist / ue-umg-specialist）。
- widget 数据绑定具体形式（`BindWidget` vs MVVM）的 5.7 选型不在本 ADR 收口，下放 ADR-0008/0006（本 ADR 只定「绑定逻辑可 spy、状态机为 C++ `UObject`」边界）。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| 灰区判定争议致边界侵蚀（逻辑悄悄进 BP） | 中 | 高（确定性/可测塌方） | Implementation Guidelines 6 条判据 + 权威模块目录级「无 BP 类」断言（结构防侵蚀）+ TD review gate |
| BP 静态扫描工具 MVP 不可建，硬门退化为软约束 | 中 | 中 | MVP 用 C++ grep + 目录断言替代（免费硬门，因权威逻辑已无 BP 类，攻击面已收窄）；逐节点扫描降 Alpha |
| HUD/卡片状态机 C++ 化抬高 UMG 协作成本 | 中 | 中 | ADR-0008 规约 `UObject` 抽离 + `IGameClock` DI + spy 接口；ue-umg-specialist 处理 widget 侧 |
| `uasset` 格式 5.x 变更致未来 BP 扫描失效 | 低 | 低（MVP 不依赖逐节点扫描） | Verification Required (a) 登记；MVP 硬门走 C++ 文本不依赖 BP 解析 |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | N/A（无代码） | 权威逻辑 C++ 热路径优于等价 BP VM（BP 节点解释执行开销）；回合制低频，差异非瓶颈 | 16.6 ms / 60 FPS |
| Memory | N/A | 中性（C++ 类 vs BP 类内存近似） | 待目标硬件 |
| Load Time | N/A | 中性（少量 BP 图 → C++ 略减 BP 编译/加载，可忽略） | 待定 |
| Network (if applicable) | N/A | MVP 无网；本边界为 Full Vision 联网状态同步**预留确定性根基**（权威逻辑 C++ + RNG 可重放） | Full Vision 定 |

> 性能非本 ADR 主驱动（回合制 + 卡通 2.5D 帧预算宽松）；C++ 权威的首要收益是**可审可测可重放**，非性能。BP VM 解释开销在回合制低频逻辑下不构成瓶颈。

## Migration Plan

无既有代码迁移（grdeenfield）。落地为开工约束：

1. 建权威 C++ 模块目录骨架（AI/经济/建房/破产/回合/RNG），挂 ADR-0001 宿主。验证：模块编译通过、`UFUNCTION(BlueprintCallable)` 接口在 BP 可见。
2. CI 增 C++ 权威模块 `grep` 禁全局 RNG 门 + 目录级「无 BP 派生类」断言。验证：故意提交一个 `FMath::Rand` / 一个权威目录 BP 类 → CI 拒绝。
3. 呈现层 BP（`WBP_*`/`BP_*`）订阅 C++ delegate，按判据 5 实现纯叶子转发。验证：spy 断言 BP 无回写权威状态。

**Rollback plan**: 边界是组织约束非运行时机制，「回滚」= 放宽某子系统从 C++ 退回 BP——须 TD 评估该子系统是否触确定性/可测硬线；若否（纯呈现误划 C++），放宽无害。触硬线者不可回滚。

## Validation Criteria

- [ ] 权威模块（AI/RNG/经济/建房/破产/回合状态机）100% 为 C++，目录内无 BP 派生类（CI 断言通过）。
- [ ] C++ 权威代码 grep 无 `FMath::Rand*`/全局 RNG（白名单 `UDiceRngService` 除外）；CI 门对故意违规提交拒绝。
- [ ] AI 决策 / 经济公式 / 状态机转换的 `[Logic] BLOCKING` AC 在 `-nullrhi` 真实跑通（C++ `UObject` 可实例化 + `IGameClock` 可注入 + spy getter 可断言），非 false-green。
- [ ] hud AC-47/AC-36c、ai AC-44 中逻辑已落 C++ 的部分升格为 `[Logic]`（`static_assert`/grep 门），仍 BP 的呈现部分维持 `[Advisory·code-review]`。
- [ ] BP 呈现层 spy 断言无回写权威状态（纯叶子纪律）。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/ai-opponent.md` | AI(10) | **OQ-AI-3b**：AI 决策核心是否强制 C++ 以加静态符号扫描真硬门（BP 软约束 AC-44 不足，确定性义务②前置） | 分叉一 B：AI 决策落 C++；分叉二硬门：权威模块无 BP 类 + C++ grep 禁全局 RNG → 静态符号扫描真硬门（MVP 分级落地），确定性义务②可执行 |
| `design/gdd/hud.md` | HUD(16) | **OQ-HUD-2**（状态机抽 `UObject` headless 前提）/ **OQ-HUD-3**（juice RNG 隔离强度 AC-47）/ AC-36c（固定 const） | 边界定 HUD 状态机为 C++ 独立 `UObject`（ADR-0008 落具体驱动）；RNG 经 C++ #3 封装可升 `[Logic]`；C++ `constexpr`+`static_assert` 兑现 AC-36c const |
| `design/gdd/property-card-ui.md` | 卡牌 UI(17) | **OQ-PC-2**（HUD↔#17 handoff 接口形态）/ **OQ-PC-4**（widget 绑定形式 + `Render*` BP 路径不可靠 spy + handler 抽 `UObject`） | OQ-PC-2：`OpenCard` 经 C++ `BlueprintCallable` 暴露（可 spy）；OQ-PC-4：卡片状态机/求值逻辑落 C++ `UObject` + 暴露可 spy getter（(i) 输出接口边界），具体绑定形式 BindWidget/MVVM 下放 ADR-0008/0006 |
| `docs/architecture/architecture.md` | 原则 4 | Authority Split「边界须 ADR（ADR-0007）」 | 本 ADR 把原则 4 方向性陈述细化为可执行边界（归属表 + 6 条判据）+ 强制机制（分级硬门） |

## Related

- **Depends on**: ADR-0001（Subsystem 宿主 — C++ 权威逻辑挂载基座）
- **Enables**: ADR-0006（GameStateSnapshot hook 签名 C++）、ADR-0008（HUD 状态机 `UObject` + `IGameClock` DI 驱动机制）、ADR-0010（音频事件订阅 C++/BP）、ADR-0012（CommonUI 交互层 BP 边界）
- **Coordinates with**: ADR-0004（RNG 服务封装实现 — OQ-AI-3b 同挂 0004/0007，本 ADR 定边界与强制强度，0004 定 `FRandomStream` 封装细节）、ADR-0003（delegate 契约 — BP↔C++ 事件暴露形态）、ADR-0005（存档序列化 C++ 落点）
- **Architecture Principle**: architecture.md 原则 4（Authority Split）、原则 2（确定性）、原则 3（DI over Singleton）
