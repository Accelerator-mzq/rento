# ADR-0003: 事件总线与 Delegate 契约（Event Bus & Delegate Contract）

## Status

Accepted

> 2026-06-06 由 msc（用户）裁定关键选型（见 Decision §「设计裁定」）：拓扑 = **去中心化 owner-held delegate**（Foundation Event Bus = 纪律层）；破产双事件源 = **方案1 职责切分 + 下游主反馈统一订经济5 `OnBankruptcyDeclared`**。引用本 ADR 的 story 解锁。

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

msc（用户）+ Technical Director（起草）— 2026-06-06 Accepted

## Summary

全系统跨层解耦依赖 12+ 个 multicast delegate（`OnDiceRolled`/`OnPhaseChanged`/`OnCashChanged`/`OnRentPaid`/`OnOwnershipChanged`/`OnBuildingChanged`/`OnBankruptcyDeclared` 等），owner 系统广播、呈现层（HUD16/卡UI17/VFX19/Audio22）订阅。本 ADR 裁定**事件解耦的拓扑形态**（集中式 Event Bus 注册表 vs 各 owner 直接持有 Multicast Delegate）、统一 delegate 规范（宏/payload/命名/方向派生），并收口**破产双事件源**（`OnBankruptcyDeclared` 经济5 2字段 vs `OnPlayerBankrupt` 破产9 3字段）的职责切分与下游订阅纪律。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core / Scripting（delegate 框架，非渲染/输入域） |
| **Knowledge Risk** | LOW — `DYNAMIC_MULTICAST_DELEGATE` / `BlueprintAssignable` / `UFUNCTION` payload USTRUCT 自 UE4 起稳定，pre-5.3 训练覆盖；无 post-cutoff 行为变更 |
| **References Consulted** | `docs/engine-reference/unreal/modules/ui.md`（BindWidget/CommonUI 订阅侧）· `current-best-practices.md` · `architecture.md` §Data Flow D.2（事件总览表 22 事件 owner→consumer） |
| **Post-Cutoff APIs Used** | None（delegate 宏与 `BlueprintAssignable` 均非 5.4–5.7 新增；订阅侧若由 CommonUI widget 持有则涉 5.7 HIGH 域，但绑定机制本身不变，归 ADR-0012） |
| **Verification Required** | (1) `DYNAMIC_MULTICAST_DELEGATE` 仍要求 payload 为单一 `USTRUCT(BlueprintType)` 或 ≤ 引擎参数上限的裸参（裸 `TArray` 作 BlueprintAssignable 参数仍编译失败——`OnTurnOrderResolved` 须包 USTRUCT，见 architecture.md 事件表）；(2) BP 端 `AddDynamic`/`RemoveDynamic` 在读档重绑路径下的解绑幂等性 |

> **Note**: Knowledge Risk = LOW。delegate 框架稳定，本 ADR 升级引擎无须重validate；但若订阅侧 widget 迁 CommonUI 激活栈（ADR-0012），重绑时序须连带复核。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | **ADR-0001**（UObject 宿主与生命周期边界——若选「轻量注册表」方案，注册表对象须由 ADR-0001 裁定的 `UWorldSubsystem` 宿主持有，且其 `Initialize/Deinitialize` 时序决定订阅生命周期边界） |
| **Enables** | ADR-0005（存档契约——读档 `OnGameLoaded` 后 delegate 重绑纪律由本 ADR 定义）· ADR-0006（GameStateSnapshot——AI hook 非事件路径，但 `OnAIActionExecuted` 广播契约在此）· ADR-0010（音频架构——Audio22 订阅侧 C++/BP 边界承接本 ADR 命名/payload 规范） |
| **Blocks** | 任何含跨系统事件广播/订阅的 story（HUD16/卡UI17/VFX19/Audio22 全部呈现层 story、economy5/property6/building8 的事件发射 story） |
| **Ordering Note** | 0001 必须先 Accepted。本 ADR 与 0004/0005 同属 Sprint 0 Foundation 硬前提，可与 0002/0004 并行裁定，但读档重绑细节须与 0005 协同收口 |

## Context

### Problem Statement

《骰子大亨》16 个 MVP 系统横跨 5 层（Foundation/Core/Feature/Presentation + 横切）。架构铁律「结算同步先行，呈现事件后随」要求：owner 系统先同步算定权威结果，再广播事件；呈现层（HUD/卡UI/VFX/Audio）作**纯叶子消费者**只订阅、只显示、绝不回写。这套解耦完全依赖 multicast delegate。

现在必须裁定的不是「是否用 delegate」（已定），而是 delegate 的**拓扑形态与契约规范**：

1. **拓扑**：是引入一个集中式 Event Bus 注册表对象（所有事件经它中转）还是各 owner 系统直接持有自己的 multicast delegate（呈现层直接订阅 owner）？这决定耦合方向、可测性、读档重绑的复杂度。
2. **规范**：12+ 事件的宏类型、payload 容器、命名、方向语义若不统一，会出现双触发（同一语义两个事件源）、payload 裸 `TArray` 编译失败、消费方各自臆测方向等问题。
3. **破产双事件源收口**：`OnBankruptcyDeclared`（经济5）与 `OnPlayerBankrupt`（破产9）并存，下游若误订双发会导致破产 juice/音效播两次。这是 OQ-VFX-13 / OQ-Audio-2 登记的待裁接缝。

**不决定的代价**：呈现层 story 无法开工（不知订哪个事件、订几次）；读档重绑无统一纪律会留野指针（EC-8 双订阅）；破产双发是已知 bug 温床。

### Current State

`architecture.md` §Data Flow D.2 已做方向性裁定：「**没有集中式 event bus 对象**——用 UE `DYNAMIC_MULTICAST_DELEGATE` 分散在各 owner 系统实现解耦」，并列出 22 个 MVP 事件的 owner→consumer 表。各 owner GDD（economy/bankruptcy/building 等）已逐条声明自己持有的事件签名。Foundation 层「② Event Bus」模块被定义为**非全局 event aggregator**——「delegate 仍归各 owner 持有，Bus 只统一绑定/解绑（读档重绑、防双订阅 EC-8）纪律」。

本 ADR 把这个**架构总纲层的方向裁定**正式化为可执行契约，并把总纲未收口的两点（破产双事件源、掷骰意图事件来源）落地或显式登记。

### Constraints

- **技术**：`BlueprintAssignable` 的 multicast delegate payload 必须是单一 `USTRUCT(BlueprintType)` 或受限裸参；裸 `TArray` 不可直接作参（`OnTurnOrderResolved` 实证须包 `FTurnOrderResult` USTRUCT）。
- **架构不变式**：呈现层 Exposes 为空（不回写、不被反调）；单一事件源（每个状态变更恰一个 owner 广播，禁双触发）。
- **生命周期**：对局期 delegate 的订阅须在宿主（ADR-0001 `UWorldSubsystem`）`Deinitialize` 时全部解绑，否则 PIE Stop / 读档残留野绑定。
- **可测性**：事件次数/payload 字段须可 headless 断言（economy AC-34/36/37 已写「恰 1 次」「2 字段」spy 断言）——拓扑选择不得使这些断言失效。
- **Full Vision 预留**：联网（25）阶段事件可能需复制/RPC 化；MVP 拓扑不得堵死该路径。

### Requirements

- 统一 12+ MVP 事件的 delegate 规范（宏 / payload 容器 / 命名 / 方向派生）。
- 收口破产双事件源（OQ-VFX-13 / OQ-Audio-2）：明确两事件职责切分 + 下游订阅唯一性。
- 定义读档重绑纪律（与 ADR-0005 `OnGameLoaded` 协同），防双订阅 EC-8。
- 登记掷骰意图事件来源（OQ-VFX-16）——MVP fallback 已具名 `OnPhaseChanged(RollPhase)`。
- 不引入新耦合环、不破坏呈现层纯叶子不变式、不使既有事件 spy AC 失效。

## Decision

**采用方案 A：各 Owner 直接持有 Multicast Delegate（去中心化），Foundation 「② Event Bus」模块退化为纪律层（命名/payload 规范 + 读档重绑统一入口），非中转对象。** 统一规范如下，并对破产双事件源采**方案 1：职责切分 + 下游单订经济5**。

下方先逐个列拓扑选项的代价与推荐理由，再给规范细则与破产收口。

### 设计裁定一：事件拓扑（集中 Bus vs 去中心 Delegate）

**选项 A — 去中心化 Multicast Delegate（推荐）**
- **含义**：每个事件作为 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` 成员，由其唯一 owner 系统持有；呈现层经 owner 的 `Exposes`（`AddDynamic`/BP `Bind Event`）直接订阅。Foundation「② Event Bus」**不是对象**，只是一套规范 + 一个集中的「读档重绑/解绑」工具函数集（遍历已知 owner 重新 bind）。
- **服务的支柱/不变式**：单一事件源（owner 即广播者，无中转层模糊归属）；呈现层纯叶子（直接订 owner，无反向通道）；可测性（spy 直接挂 owner delegate，AC-34/36/37 断言天然成立）。
- **下游后果**：呈现层须知道每个事件的 owner（已由 architecture.md 事件表 + 各 GDD 钉死）；读档重绑须逐 owner 解绑+重绑（工具函数集中，但物理上分散）。
- **风险与缓解**：风险=「订阅点分散，难一眼看全谁订了谁」→ 缓解：architecture.md D.2 事件总览表 = 权威订阅矩阵，review 据此核验；新增订阅须更新该表。
- **先例**：UE 原生范式（Gameplay 框架、GAS 的 `OnAttributeChanged` 均为 owner 持有 delegate，无全局 bus）；与引擎惯例一致 = 低认知成本、好招人。

**选项 B — 集中式 Event Bus 注册表对象**
- **含义**：一个 `UGameEventBus`（UWorldSubsystem）持所有事件 delegate，owner 调 `Bus->BroadcastX(payload)`，consumer 调 `Bus->OnX.AddDynamic`。事件定义集中一处。
- **服务的目标**：单点总览（所有事件一个文件可见）；读档重绑只需重置一个对象。
- **牺牲**：**模糊单一事件源**（Bus 成了所有事件的名义广播者，真 owner 退到幕后，违「owner 即广播者」不变式，易诱发"任何系统都能 Broadcast"的越权）；**多一层间接**（owner→Bus→consumer，spy 须挂 Bus 而非 owner，部分 economy AC 的「owner 广播一次」语义须改写）；与 UE 惯例相悖（招来的 UE 程序员第一反应找 owner delegate）。
- **风险**：Bus 易演变成上帝对象 + 隐式全局耦合；联网阶段 Bus 单点复制反而比 owner-local 复制更难切分权威。
- **拒绝理由**：本项目仅 22 个 MVP 事件、owner 关系已逐档钉死且无环，集中 Bus 的「总览」收益用一张事件表即可替代，却要付「模糊事件源 + 多一层 + 逆引擎惯例」的结构性代价。**Reversibility 不对称**：A→B 易（包一层），B→A 难（拆全局耦合）。

**选项 C — 混合（高频走 delegate，低频/跨域走集中 Bus）**
- **含义**：呈现 juice 类高频事件走 owner delegate；少数跨域协调事件（如破产、胜利）走集中 Bus。
- **牺牲**：两套心智模型并存，programmer 须记「哪类走哪条」→ 维护性最差；破产事件恰是双事件源接缝，放进 Bus 不解决职责切分、反而把接缝藏进中转层。
- **拒绝理由**：复杂度不偿。简单性原则（Decision Framework #2）直接淘汰。

**推荐：选项 A。** 理由：与架构总纲 D.2 既有方向裁定一致（无须 propagate 反向修正）、守单一事件源不变式、贴合 UE 原生惯例（可维护/可招人）、Reversibility 顺向（未来要集中只需包一层，反之难拆）。Foundation「② Event Bus」模块据此**正名为纪律层**：它拥有的是 *命名/payload 契约 + 读档重绑工具 + 防双订阅纪律*，不是事件中转对象。

### 设计裁定二：破产双事件源收口（OQ-VFX-13 / OQ-Audio-2）

两个事件并存且语义不同：

| 事件 | Owner | Payload | 广播时机 | 权威 AC |
|------|-------|---------|----------|---------|
| `OnBankruptcyDeclared(Debtor, Creditor)` | 经济(5) | 2 字段 | **现金侧转移完成后** | economy AC-36（移交完成后恰 1 次，时机在资产转移之后） |
| `OnPlayerBankrupt(debtor, creditor, reason)` | 破产(9) | 3 字段（+reason） | **清算编排末**（in-kind/银行回退逐格完成后） | bankruptcy AC-39 |

**方案 1：职责切分 + 下游统一订经济5（推荐）**
- `OnBankruptcyDeclared`（经济5）= **现金侧完成信号**，承载「破产已宣告」的最早确定点；HUD16/VFX19/Audio22 的**破产主反馈**（出局黯然 + 破产音 + BGM duck）统一订此，与已 Approved 姊妹档（HUD16 V-Enable-03/AC-16、VFX19 R-8 logged decision、economy AC-36）一致。
- `OnPlayerBankrupt`（破产9，3字段）= **编排末信号 + reason/creditor 语境**，仅供**需要 reason 或须等全部清算落定**的消费（如终局 reason 文案、需 creditor 身份的差异化出局 juice）。下游若订此，**不得重复播放已由 `OnBankruptcyDeclared` 触发的主反馈**。
- 资产逐格易主 juice 不订这两者，**复用所有权6 `OnOwnershipChanged` 逐格**（architecture.md D.2 已裁），同帧级联节流 `T_sfx_min_retrigger=0.06`。
- **纪律**：同一破产事件，主反馈订经济5 一次；reason/编排末订破产9 仅作语境增量，**禁双播主反馈**。review 据此核（一次破产 → 主反馈恰 1 次）。

**方案 2：合并为单事件（破产9 唯一广播 3 字段）**
- 取消经济5 `OnBankruptcyDeclared`，下游全订破产9 `OnPlayerBankrupt`。
- **牺牲**：经济5 AC-36 须删（破坏已 Approved 经济档的事件契约 + spy AC）；破产主反馈被迫等到「编排末」才触发，丧失「现金侧完成」这个更早的确定点（破产音可能滞后于清算动画）；跨档 propagate 面更大（动 economy/bankruptcy 两个 Approved 档）。
- **拒绝理由**：破坏两个已 Approved 姊妹档的既定契约，propagate 成本高且无收益——两事件语义**确实不同**（现金完成 ≠ 编排完成），强行合并丢失时序信息。

**推荐：方案 1。** 理由：与全部已 Approved 姊妹档（HUD16/VFX19/economy5/bankruptcy9）当前一致裁定对齐，零跨档反向 propagate；两事件语义正交（现金完成信号 vs 编排末+reason），切分而非合并保留时序精度；下游订阅唯一性靠纪律 + review spy 守，可证伪（一次破产主反馈恰 1 次）。

### 设计裁定三：掷骰意图事件来源（OQ-VFX-16）

「点击掷骰后」的意图事件 owner 在骰子3 vs 回合2 之间未定。**本 ADR 不强行裁死，登记为架构期可延后项**：MVP fallback `OnPhaseChanged(RollPhase)`（回合2，已具名、已被 VFX19 R-7 采纳）足以覆盖掷骰 juice 触发。若后续需「点击瞬间」与「Phase 进入」分离的更细粒度，再于本 ADR 增订。Reversibility 高（加一个 owner-local 事件即可），不阻 MVP。

### 统一 Delegate 规范（全事件适用）

1. **宏**：一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_OneParam/_TwoParams/...]` + `UPROPERTY(BlueprintAssignable, Category="Events")`。BP 与 C++ 双向可订（呈现层 BP widget 经 `Bind Event`，C++ 系统经 `AddDynamic`）。禁用非 dynamic delegate 暴露给呈现层（BP 不可订）。
2. **Payload 容器**：
   - 单参或语义内聚的多字段 → 包 `USTRUCT(BlueprintType)`（如 `FDiceRollResult`、`FPhaseChangedInfo{Phase,ConsecutiveDoubles}`、`FTurnOrderResult{OrderedPlayerIds,bResolvedBySeatTiebreak}`）。
   - **裸 `TArray` 禁作 BlueprintAssignable 参数**（编译失败）——`OnTurnOrderResolved` 必须 USTRUCT 封装（architecture.md 已钉）。
   - 字段「只增不改语义」（economy 接口稳定承诺延伸为全事件纪律）；枚举 append-only（与 ADR-0005 协同）。
3. **命名**：事件 `On<PastTense>`（`OnRentPaid`/`OnCashChanged`/`OnBuildingChanged`）；payload struct `F<Event>Info` 或语义名。与 technical-preferences 命名规范一致。
4. **方向由消费方派生**：payload 携带**类型语境**（`EChangeReason`/`EArrivalContext`/Payer/Payee 视角），**方向（正负/收付）由消费方从 delta 符号 + Payer/Payee 视角派生**，owner 不为每个方向各发一个事件。例：`OnCashChanged(Player,Old,New,EChangeReason)` 由消费方据 `New-Old` 符号判增减、据 reason 选音色。
5. **单一事件源**：每个状态变更恰一个 owner 广播。`OnGameWon` 由**回合2 触发**（破产9 仅返回 winnerId，return-only，不直接广播）；`OnBuildingChanged` 2 字段（actingPlayerId 由回合2 上下文取，非塞进事件第3字段）；`OnAIActionExecuted` 由**回合2** 执行 AI 动作时广播（非 AI 发）。
6. **读档重绑纪律（与 ADR-0005 协同）**：读档 `OnGameLoaded` 后，先**全量解绑**呈现层既有订阅（防 EC-8 双订阅叠加），再按权威订阅矩阵重绑。解绑/重绑经 Foundation「② Event Bus」纪律层的集中工具函数，物理上遍历各 owner delegate。订阅生命周期边界 = 宿主（ADR-0001）`Initialize`（绑）/`Deinitialize`（解绑），PIE Stop 不留野绑定。
7. **未注册枚举值兜底**：消费方对未知 `EChangeReason`/`EArrivalContext`/`EJailReason` 等枚举值走**中性兜底**（运行期安全，不崩不静默错播）。

### Architecture

```
  ┌─────────── Foundation「② Event Bus」= 纪律层（非对象）────────────┐
  │  · Delegate 命名/payload USTRUCT 契约（全事件统一）              │
  │  · 读档重绑工具函数集（遍历各 owner 解绑→重绑，与 ADR-0005）      │
  │  · 防双订阅 / 单一事件源 review 纪律                             │
  └────────────────────────────────────────────────────────────────┘
            （规范约束，不中转事件）
                         │
   OWNER 系统（各自持有 delegate，唯一广播者）        PRESENTATION（纯叶子订阅）
   ┌──────────────────────────────────┐            ┌──────────────────────┐
   │ 骰子3   OnDiceRolled              │──↝event──▶ │ HUD16  各订各显示      │
   │ 回合2   OnPhaseChanged/OnTurn*    │──↝event──▶ │ 卡UI17 各订各刷新       │
   │ 经济5   OnCashChanged/OnRentPaid  │──↝event──▶ │ VFX19  各订各播视觉      │
   │         OnBankruptcyDeclared(2)   │──↝event──▶ │ Audio22 各订各播听觉     │
   │ 所有权6 OnOwnershipChanged        │──↝event──▶ │                        │
   │ 建房8   OnBuildingChanged(2)      │            │ (订阅矩阵=D.2 事件表)   │
   │ 破产9   OnPlayerBankrupt(3,reason)│──语境增量─▶ │ (主反馈订经济5,不双播)  │
   └──────────────────────────────────┘            └──────────────────────┘
        owner 先同步算定结果 → 再 ↝event 广播（结算同步先行，呈现事件后随）
```

### Key Interfaces

```cpp
// —— 统一规范示例（owner-held，非集中 Bus）——

// payload 一律 USTRUCT(BlueprintType)；裸 TArray 禁作参数
USTRUCT(BlueprintType)
struct FPhaseChangedInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ETurnPhase Phase;
    UPROPERTY(BlueprintReadOnly) int32 ConsecutiveDoubles;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, const FPhaseChangedInfo&, Info);

// owner（回合2）持有，唯一广播者
UPROPERTY(BlueprintAssignable, Category="Events")
FOnPhaseChanged OnPhaseChanged;

// —— 破产双事件源职责切分 ——
// 经济5：现金侧完成信号（主反馈订此）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBankruptcyDeclared, int32, Debtor, int32, Creditor);
// 破产9：编排末 + reason 语境（仅 reason/creditor 增量订此，禁重播主反馈）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerBankrupt, int32, Debtor, int32, Creditor, EBankruptcyReason, Reason);

// —— 读档重绑（纪律层工具，与 ADR-0005 OnGameLoaded 协同）——
// 伪代码：先全量解绑防 EC-8 双订阅，再按权威订阅矩阵重绑
void RebindPresentationDelegates(/* owner refs */);  // 由宿主 Initialize / OnGameLoaded 调
```

### Implementation Guidelines

- 呈现层订阅以 **architecture.md §Data Flow D.2 事件总览表为权威矩阵**；新增订阅必须同步更新该表（review 据此核单一事件源 + 防双订阅）。
- 破产主反馈（出局 juice / 破产音 / BGM duck）**只订经济5 `OnBankruptcyDeclared`**；`OnPlayerBankrupt` 仅供需 reason/creditor 或须等编排末的增量呈现，**严禁重播主反馈**——测试须断言「一次破产 → 主反馈恰 1 次」。
- delegate 暴露给 BP 一律 `BlueprintAssignable`；非 dynamic delegate 不得暴露给呈现层。
- payload 多字段必包 USTRUCT；提交前编译验证裸 `TArray` 未直接作 BlueprintAssignable 参数。
- 读档重绑走纪律层集中工具，先解绑后重绑；订阅生命周期锚 ADR-0001 宿主的 `Initialize/Deinitialize`。
- BP-vs-C++ 订阅侧归属（哪些事件 C++ 订、哪些 BP widget 订）随 **ADR-0007** 边界裁定；本 ADR 只定 delegate 形态，不定订阅方语言。

## Alternatives Considered

### Alternative 1: 集中式 Event Bus 注册表对象（选项 B）

- **Description**: 单一 `UGameEventBus`（UWorldSubsystem）持全部事件 delegate，owner 调 Bus broadcast，consumer 订 Bus。
- **Pros**: 所有事件单文件总览；读档重绑只重置一个对象。
- **Cons**: 模糊「owner 即广播者」单一事件源不变式；多一层间接致部分 economy spy AC 须改写；逆 UE 惯例；易演变上帝对象 + 隐式全局耦合；联网阶段单点复制难切权威。
- **Estimated Effort**: 与 A 相当（多一个 Bus 类，但少各 owner 的 delegate 声明）。
- **Rejection Reason**: 「总览」收益用一张事件表即可替代，却付结构性代价；Reversibility 不对称（A→B 易，B→A 难）。

### Alternative 2: 混合拓扑（选项 C）

- **Description**: 高频 juice 走 owner delegate，低频跨域走集中 Bus。
- **Pros**: 表面上兼顾。
- **Cons**: 两套心智模型，维护性最差；破产接缝放进 Bus 不解决职责切分反而藏接缝。
- **Estimated Effort**: 最高（两套机制）。
- **Rejection Reason**: 违简单性原则，复杂度不偿。

### Alternative 3: 破产事件合并为单事件（破产收口方案 2）

- **Description**: 删经济5 `OnBankruptcyDeclared`，下游全订破产9 3字段 `OnPlayerBankrupt`。
- **Pros**: 单一破产事件源，无双订阅风险。
- **Cons**: 破坏已 Approved 经济5 AC-36 契约；主反馈丧失「现金侧完成」更早确定点；跨档 propagate 动两个 Approved 档。
- **Rejection Reason**: 两事件语义正交（现金完成 ≠ 编排完成），合并丢时序信息且 propagate 成本高无收益。

## Consequences

### Positive

- 与架构总纲 D.2 既有方向裁定一致，零反向 propagate。
- 守单一事件源 + 呈现层纯叶子不变式；spy AC（economy AC-34/36/37 等）天然成立。
- 贴合 UE 原生惯例，可维护、好招人。
- 破产双事件源职责切分明确，主反馈订阅唯一性可证伪（review 可核「主反馈恰 1 次」）。
- Reversibility 顺向：未来需集中只须包一层。

### Negative

- 订阅点物理分散，「谁订了谁」须靠 architecture.md 事件表（外部矩阵）维护——表与代码漂移则失准。缓解：新增订阅强制更新表 + review 门控。
- 破产「主反馈订经济5、增量订破产9」是**纪律约束**而非编译期强制，依赖 review spy 守双播。缓解：负向 AC（一次破产主反馈恰 1 次）。

### Neutral

- Foundation「② Event Bus」模块正名为纪律层（命名/payload 规范 + 读档重绑工具），不再被误解为中转对象——与 architecture.md 模块表既有描述一致，无须改总纲。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| 订阅矩阵（事件表）与代码漂移 | MED | MED | 新增订阅强制更新 D.2 事件表；review 门控核对 |
| 破产主反馈双播（误同时订两事件） | MED | MED | 纪律 + 负向 spy AC「一次破产主反馈恰 1 次」 |
| 读档重绑遗漏解绑 → EC-8 双订阅叠加 | MED | HIGH | 纪律层集中工具「先全量解绑后重绑」；生命周期锚宿主 Init/Deinit |
| 裸 TArray 作 BlueprintAssignable 参数编译失败 | LOW | LOW | 提交前编译验证；USTRUCT 封装纪律（OnTurnOrderResolved 已钉） |
| 联网阶段事件须 RPC 化 | LOW（MVP 无） | MED | owner-local delegate 比集中 Bus 更易按 owner 切复制权威；Full Vision OQ |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | N/A（新建） | delegate broadcast O(订阅数)，MVP 单局事件频率低（回合制），可忽略 | 16.6 ms / 60 FPS |
| Memory | N/A | 每事件一个 delegate 成员 + 订阅列表，KB 级 | 待目标硬件确定 |
| Load Time | N/A | 读档重绑遍历各 owner，一次性，毫秒级 | — |
| Network (if applicable) | N/A（MVP 无联网） | owner-local delegate 为 Full Vision 按 owner 切复制留路径 | — |

> 回合制低事件频率，delegate broadcast 不构成帧预算风险。唯一注意点：破产/清算同帧级联多格 `OnOwnershipChanged` 须节流 `T_sfx_min_retrigger=0.06`（已在 VFX19/Audio22 GDD 钉，非本 ADR 新增）。

## Migration Plan

新建系统，无既有迁移。落地顺序：

1. ADR-0001 Accepted（宿主裁定）后，定义 Foundation「② Event Bus」纪律层（命名/payload 规范文档 + 读档重绑工具函数骨架）。
2. 各 owner 系统按规范声明 delegate（payload USTRUCT），呈现层按 D.2 事件表订阅。
3. 破产双事件源按方案 1 落地：经济5 `OnBankruptcyDeclared` 主反馈、破产9 `OnPlayerBankrupt` 增量；写负向 spy AC。
4. 读档重绑与 ADR-0005 `OnGameLoaded` 协同实现。

**Rollback plan**: 若去中心化暴露「订阅矩阵不可维护」问题，可增量引入集中 Bus 包一层（A→B 顺向，不必重写 owner delegate，只改 consumer 订阅指向）。

## Validation Criteria

- [ ] 全部 12+ MVP 事件以 `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable` + payload USTRUCT 声明，BP/C++ 双向可订。
- [ ] 裸 `TArray` 无一作 BlueprintAssignable 参数（编译通过 = 验证）。
- [ ] 一次破产 → 破产主反馈（出局/破产音/BGM duck）恰触发 1 次（负向 spy AC，证伪双播）。
- [ ] economy AC-34/36/37 等既有事件次数 spy AC 在本拓扑下全数通过（拓扑未使断言失效）。
- [ ] 读档 `OnGameLoaded` 后呈现层订阅数 == 初次开局订阅数（无 EC-8 双订阅叠加）。
- [ ] PIE Stop 后无野绑定（宿主 Deinitialize 全解绑）。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/economy-cash.md` | 经济5 | `OnCashChanged`/`OnRentPaid`/`OnInsufficientFunds`/`OnBankruptcyDeclared` 事件契约 + AC-34/36/37 spy | 统一 delegate 规范 + 方向由消费方派生；破产收口方案1 保留 AC-36「现金侧完成后恰1次」 |
| `design/gdd/bankruptcy.md` | 破产9 | `OnPlayerBankrupt(3字段)`/`OnPlayerRanked`；`OnGameWon` 由回合2 触发（9 return-only） | 破产双事件源职责切分（编排末+reason 增量）；单一事件源纪律（OnGameWon 回合2 广播） |
| `design/gdd/vfx-feedback.md` | VFX19 | 破产 juice 订阅（OQ-VFX-13）+ 掷骰意图（OQ-VFX-16）+ `OnOwnershipChanged` 复用 | 主反馈订经济5；OQ-VFX-16 登记 fallback `OnPhaseChanged(RollPhase)`；逐格复用所有权6 |
| `design/gdd/audio.md` | Audio22 | 破产音/BGM duck 订阅（OQ-Audio-2）+ 各 owner 事件订阅 | 主反馈订经济5 `OnBankruptcyDeclared`；统一规范覆盖全订阅 |
| `design/gdd/hud.md` | HUD16 | `OnPhaseChanged`/`OnTurnStarted`/`OnBuildingAnnounced`/`OnAIActionExecuted` 订阅 + V-Enable-03/AC-16 | 单一事件源（OnAIActionExecuted/OnBuildingAnnounced 回合2 广播）；与 HUD 既定破产订阅一致 |
| `design/gdd/building-upgrade.md` | 建房8 | `OnBuildingChanged(2字段)` | 2 字段规范（actingPlayerId 由回合2 上下文取，非塞事件第3字段） |
| `design/gdd/dice.md` | 骰子3 | `OnDiceRolled(FDiceRollResult)` | USTRUCT payload；两骰呈现禁从 Total 拆解 |
| `design/gdd/player-turn.md` | 回合2 | `OnPhaseChanged`/`OnTurnStarted`/`OnTurnEnded`/`OnTurnOrderResolved`/`OnGameWon`/`OnAIActionExecuted` | `OnTurnOrderResolved` 须 USTRUCT（裸 TArray 编译失败）；OnGameWon/OnAIActionExecuted 单一事件源裁定 |
| `docs/architecture/architecture.md` | Foundation ②Event Bus | 「非全局 event aggregator，delegate 归各 owner 持有」 | 本 ADR 正式化为去中心化拓扑 + 纪律层定位，与总纲 D.2 一致 |

> 同时覆盖架构总纲 §8 列入 ADR-0003 的待裁项：OQ-VFX-13（破产双事件源，方案1 收口）、OQ-VFX-16 / OQ-Audio-2（掷骰意图，登记 fallback）、`OnAIActionExecuted` 广播者归属（回合2）。Event 域 33 TR 经本 ADR + D.2 事件表背书。

## Related

- **Depends on**: ADR-0001（宿主，注册表/订阅生命周期）
- **Coordinates with**: ADR-0005（读档重绑 `OnGameLoaded`）· ADR-0007（订阅侧 BP/C++ 边界）· ADR-0010（Audio22 订阅）
- **Enables**: ADR-0006（`OnAIActionExecuted` 契约）
- 架构总纲：`docs/architecture/architecture.md` §Data Flow D.2（事件总览表 = 权威订阅矩阵）、§8.C ADR-0003 条目
- 代码文件：待实现后回填
