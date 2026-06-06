# 移动 (Movement)

> **Status**: **APPROVED**（Revised R3 fresh re-review 2026-06-02 完结；4 条 producer BLOCKING propagate 工单经 `/propagate-design-change` + grep 核实全部落地，APPROVE-pending-propagate 解除）
> **propagate 闭合（2026-06-02,producer 协调 `/propagate-design-change`,grep 逐档核实落地）**:① **economy.md** line 57/102/330 旧 push 措辞清理为 PULL + Interactions 玩家回合行补 holder 源(holder owner=回合2);② **player-turn.md** 补 CR-3.1(当前程骰 holder + 程间原子性)+ AC-46(SentToJail 落地抑制)+ AC-47(程间非重入);③ **board-data.md** 补 AC-23j + Validation 拒绝规则 + Tuning 上界(`DiceMultiplierTable[i] ∈ [1, DICE_MULT_MAX]`);④ OQ-Move-5/OQ-Move-6 已 RESOLVED(见 Open Questions)。**R3 改 3 处见下:**E〔AC-21 自矛盾条件化〕+ A〔OQ-Move-5 点名 economy line 102 跨档真矛盾、拆 OQ-Move-6〕+ D〔Edge Cases 显式论证不广播为有意 feature〕。
> **Author**: msc + agents
> **Last Updated**: 2026-06-02
> **Implements Pillar**: ①桌游忠实（掷骰移动是核心循环第一步）、④易上手（鼠标掷骰、棋子沿格前进一眼可读）
> **System**: #4 in systems-index design order | Core | Gameplay | MVP
> **Review Mode**: full re-review（game-designer / systems-designer / qa-lead / unreal-specialist / economy-designer + creative-director 综合裁定 2026-06-02）
> **R1 用户决策（2026-06-02）**：arrivalContext = 透传 + 签名加 context 参；AC-11 冻结为 PROVISIONAL（待 OQ-Move-1）；Player Fantasy 全改写为 enable-not-own Experience Contract。
> **R2 用户决策（2026-06-02 fresh re-review）**：① **Finding E**——CR-3.1 `Total` 由 PUSH-and-cache 改为**解决期 PULL**（移动不缓存/不推送 `Total`，只把它当 `steps` 消费；Utility 租的 `dice_total` 在 ResolvePhase 由经济从回合掷骰上下文读取），与经济 AC-18「不缓存」契约对齐；holder 机制 + 双点/事件额外骰的单源去歧义登记 **OQ-Move-5**（跨 doc，留 `/propagate-design-change`）。② **Finding D**——`TeleportTo` 的 `target==from` 整圈 vs 原地裁决改为**调用方注入参数** `advanceOnZero`（镜像 arrivalContext 透传），把 board F-3 L233 委派给调用方的「0 解释权」交还事件(7)，移动不再 `关死` 此裁决。③ Finding B（去坐牢抑制买地/收租在 player-turn 无 AC）movement 侧已升 [Logic] 透传断言（AC-12b），player-turn 侧补 AC 留**跨 doc followup**。④ 命名规范化（snake/camel→PascalCase+b-bool）**延后**至 control-manifest/dev-story 期。
> **架构定位（用户裁定 2026-06-02）**: 编排层（orchestration）——位置数学全归棋盘(1)，移动不发明新公式，只编排 取骰点→调 AdvanceIndex→push 发薪→触发落地结算。

## Overview

移动系统是《骰子大亨》核心循环的**第一棒**:玩家掷骰后,它负责把棋子从当前格沿环形棋盘前进相应步数,并裁决这一程移动的**结算后果**——经过/停在起点是否发薪、最终落在哪一格、由谁来结算落地事件。它有两个面:对内是一个**编排层(orchestration)**——它本身**不发明任何位置数学**(环路取余落点 `advance_index`、过 GO 净次数 `passed_go_count`、前向距离 `steps_between` 全部归棋盘数据(1)拥有),而是按固定时序把各系统串起来:从骰子(3)取本次位移量 `Total` → 调棋盘(1)的 `AdvanceIndex` 拿到 `(newIndex, passedGo)` → 把 `(passed_go, SalaryAmount)` push 给经济(5)发薪 → 把最终落点交给玩家回合(2)的 `ResolvePhase` 触发落地结算(买地/收租/事件)。对玩家而言,移动**使能(enable)但不拥有(own)**核心循环第一拍的体验:它确定性地产出"掷骰→唯一落点→唯一揭晓时刻"这条**事实**,而"棋子一格格跳着逼近命运、玩家屏息数着还差几步到那块觊觎已久的地"的**逐格期待呈现归 VFX(19)/动画/音频(22)拥有**(详见 Player Fantasy 的 enable-not-own 体验契约)。它**不拥有**位置公式(归棋盘1)、随机点数(归骰子3)、回合流程与决策点(归回合2)、落地后的金钱/事件后果(归经济5/事件7)、逐格期待的呈现(归 VFX19);它只拥有**"一次掷骰如何确定性转化为一程棋子位移 + 触发哪些下游结算"这条编排链**。它做得好时,移动是无缝顺滑、节奏明确的——玩家的注意力全在"会落到哪、那里有什么"的期待上,而非察觉到背后有多少系统在协作。

## Player Fantasy

> *(本节经 creative-director **full re-review 复核改写 2026-06-02**:移动是逻辑编排层,**不直接产出 MDA Aesthetic**;本节写作为 **Experience Contract(体验契约)**——只声明移动逻辑上真正拥有、且可回链 AC 的体验贡献;逐格期待/顺滑手感的**呈现所有权显式归 VFX(19)/动画/音频(22)**,对齐 dice OQ-T-Dice-4 把"掷骰爽感"整体移交呈现层的判例。结清 lean 起草留下的 framing 复核 debt。)*

移动是核心循环最经典的一拍——**"骰子掷出去,看着棋子一步步逼近命运"**——的逻辑骨架。但作为编排层,它**使能(enable)**这拍体验,而**不拥有(own)**它的呈现。诚实拆分如下:

- **移动逻辑上真正拥有的贡献:落点的确定性与揭晓的唯一性。** 一次掷骰被移动**确定性地、零歧义地**转化为唯一落点(`newIndex`)+ 唯一揭晓时刻(`OnPawnLanded` + `arrivalContext`)。"会落到哪、那里有什么"这个**揭晓什么**的信息由移动产出。它服务的不是某种 Aesthetic,而是**玩家对系统因果的信任**(玩家能预测"掷 5 就走 5、停在那一格"——SDT Competence 的认知前提)。这一贡献可回链 AC-1/5/6/17/18(算得对、事件恰好各 1 次、单帧确定结算)。

- **使能但不拥有的体验:逐格期待与无缝顺滑。** "棋子一格格跳着逼近"的**期待节奏**(`hop_duration` 等)由 VFX(19)/动画拥有——移动在逻辑上是单帧瞬时结算(CR-4),棋子"逐格跳"完全是呈现层回放既定结果的产物,移动连决定节奏的旋钮都不持有(见 Tuning Knobs)。"玩家不察觉背后四系统协作"的**无缝顺滑**是一个**服务质量目标(SLA,做好了等于隐形)**,不是 MDA 意义的玩家情感;移动通过"逻辑先于表现、单帧确定、不卡顿"来**使能**它,但其呈现质量由 VFX/HUD 兑现并背 experiential AC。

移动的成功标准因此是**逻辑契约级的**:给定掷骰输入,落点与揭晓时刻**唯一、确定、可重放**;呈现层据此回放,玩家把情绪投向"会落到哪、那里有什么"的空间博弈。**移动不背"玩家是否感觉顺滑/有期待"的 AC**——那是呈现层(VFX19/动画/音频22)的 experiential 职责。

> **与骰子(3)的边界**:骰子拥有"掷出去那一下的随机刺激与翻滚 juice";移动拥有"点数→唯一落点→唯一揭晓时刻"的**确定性事实**;呈现层(VFX19)拥有把这个事实演出成"逐格期待"的 juice。三者接力,不重叠,**无双重所有权**。

> **MDA 锚点(改写)**:本系统**不直接产出 MDA Aesthetic**(它是逻辑编排层)。它**使能**的核心循环第一拍的轻量 **Sensation**(逐格揭晓)由 VFX(19)/音频(22) 拥有并兑现;移动对 Aesthetic 的贡献是**保证因果链确定无歧义**——这是玩家建立系统信任(Competence)的基础,而非一种 Aesthetic 本身。

## Detailed Design

### Core Rules

**CR-1 棋子位置所有权与受控写。** 棋子位置 `CurrentTileIndex`(int32 ∈ [0,N−1])物理存于 `PlayerState`(容器归回合(2)),但**只经移动的受控写接口** `SetTileIndex(player, index)` 修改,沿 player-turn 受控写契约(AC-35,BP 软约束 / C++ 强封装待 OQ-1 ADR)。**不变式 `CurrentTileIndex ∈ [0,N−1]` 恒成立**——落点恒由棋盘(1) `AdvanceIndex` 的环路取余保证,移动不写未经 board 计算的裸值。移动拥有"棋子如何位移"的编排,不拥有位置的物理存储。**`from` 每次 `Advance`/`TeleportTo` 调用时实时读 `PlayerState.CurrentTileIndex`,不跨帧缓存**(防连续双点第二程读旧起点)。`SetTileIndex` 入口守界用 **`ensure(index >= 0 && index < N)` + 早返拒写**(UE 惯用:`ensure` 报告一次后继续执行并返回 bool 供分支——契合"拒绝越界写入但不崩进程"的语义;**不用 `assert`**(非 UE 惯用、无 BP 形态)**或 `check`**(Shipping 编译掉且硬停,无法满足 AC-4/AC-13"`CurrentTileIndex` 不变且继续运行"))。此处"裸值"指**未经 `AdvanceIndex` 计算且未经入口守界**的随意值——事件格(7)提供的已知合法 `targetIndex` 经入口 `ensure` 守界后可直写(见 CR-2 去坐牢路径)。

**CR-2 两种位移入口(语义分离)。** 两入口均带 `context: EArrivalContext` 参数(**透传模型**:调用方注入落地语境,移动不自决分类——见 CR-5/AC-14。理由:"掷骰移动"与"前进 N 格"牌都走 `Advance()`,**只有透传能区分 `DiceMove` vs `AdvanceCard`**,纯入口自决做不到)。
- **`Advance(player, steps, paysGo=true, context=DiceMove)`** — 掷骰移动(`steps`=骰子 `Total`,context 默认 `DiceMove`)与"前进 N 格"类牌(调用方传 `context=AdvanceCard`)。调 `AdvanceIndex(from, steps) → (newIndex, passedGo)`(board 拥有公式);`steps` 可负(后退牌)、可超 N(多圈)。
- **`TeleportTo(player, targetIndex, paysGo, context, advanceOnZero=true)`** — "前进到最近X"类牌(`paysGo=true`,调用方传 `context=AdvanceCard`)与去坐牢(`paysGo=false`,context 强制 `SentToJail`)。`paysGo=true` 时按**顺时针前向**到达,`steps = effective_steps(from, targetIndex, advanceOnZero)`(见 Formulas P-2 命名表达式),其后等价于 `Advance(steps, paysGo=true)`(经过 GO 则发薪,忠实经典"前进到最近车站仍过路发薪")。**`steps_between==0`(目标即当前格)裁决归调用方(`advanceOnZero` 参数)**:board F-3 L233 已明示此 `0` 的语义裁决权归调用方(事件系统7),**移动不自决、不"关死"此裁决**——`advanceOnZero=true`→前进整圈 N 步(`passedGo=1` 发薪,忠实"advance to"必须前向到达);`advanceOnZero=false`→原地驻留(`passedGo=0`、不发薪)。此参数镜像 `context` 透传模型:移动只**执行**调用方注入的裁决,不发明 ludic 规则。⚠ "前进到最近X"经典牌在 `target==from` 时事件(7)注入的**正确默认值**(整圈 vs 原地)仍待 **OQ-Move-1** Rento 实际行为核查(AC-11a 暂取整圈默认)——但无论核查结论如何,*裁决权*已参数化归调用方,movement 两分支均确定性可测(AC-11a/11b 不冻结),只回填事件(7)默认值,不改 movement 契约。`paysGo=false` 时**直达**(**不调 `AdvanceIndex`**)`SetTileIndex(targetIndex)`(入口 `ensure` 守界,CR-1)、`passedGo` 强制 0、不发薪(去坐牢不过 GO;`advanceOnZero` 在 `paysGo=false` 路径不适用——去坐牢恒直达)。
  > **named exception(收口 board-data line 235):** board-data line 235 禁"绕过公式直接改位置索引跳过 PassedGoCount",其**立法意图**(见该句本身)是**防本应发薪的传送漏发薪**。去坐牢 `paysGo=false` 是**经典规则明确不发薪**的语境,**不在该禁令的危害域内**;且直写已经 CR-1 入口 `ensure` 守界、落点合法性由事件格(7)保证。故本路径是 board-data line 235 的**授权例外**(发薪权威归经济消费 `passed_go` 时决定,与 board F-2 设计契约一致),非违规绕过——movement 作为编排层在此自背登记责任。

**CR-3 发薪 push 门(编排,不拥有金额)。** 仅当本次位移 `paysGo==true` **且** `passedGo>0`,移动把 `(passedGo, SalaryAmount)` push 给经济(5)发薪(经济 CR-2 / F-1)。`passedGo≤0`(未过/逆向穿越)或 `paysGo==false` 一律不 push。移动**不拥有**发薪金额(经济 F-1)、不拥有 `passed_go_count` 算法(board);只拥有"何时 push"这道编排门。

**CR-3.1 移动骰 `Total` 仅作位移消费(Utility 租改解决期 PULL,移动不缓存/不推送)。** 移动**每次掷骰位移(`Advance`,context=`DiceMove`)** 只把本次骰 `Total` 当作 `steps` 消费(算落点),**不缓存、不向经济 push `Total`**。Utility 租所需的 `dice_total`(经济 F-4)由经济在 **ResolvePhase 算租时**从**回合掷骰上下文(回合2 持有的本回合 `RollDice()` 结果)PULL**,而非移动 store-and-forward 推送。
> **理由(R2 Finding E,经济 AC-18 对齐):** 经济 F-4/AC-18 把 `dice_total` 定义为**解决期参数、显式不缓存**(AC-18 是 PR-gate)。原 CR-3.1 的"移动 push-and-cache `Total` 供经济稍后消费"模型与之**不兼容**:连续双点一回合内经济会先后收到**两次** `Total`,而移动从未规约"第一程落 Utility 时用哪个 `Total`";叠加机会牌"前进到最近公用须额外掷骰"(economy OQ-T-Econ-3)是 `dice_total` 的**第二写者**——构成 stale-Total / 双写者正确性缺口。改为解决期 PULL 后,经济在每次 ResolvePhase 读"当前正在结算的这一程"对应的 `Total`,无跨程缓存。
> **holder 机制 + 单源去歧义留 OQ-Move-5(跨 doc):** "回合上下文如何暴露当前程 `Total` 供经济 PULL"、"双点链/事件额外骰下 `Total` 的单源界定"涉及回合(2)/经济(5)/事件(7),归 **OQ-Move-5** + `/propagate-design-change`,本档不单方改写跨系统契约。movement 侧契约缩小为:**只消费 `Total` 作 `steps`,不拥有 `Total` 的跨阶段投递**(CR-6:不拥有落地后果/不读 TileType)。

**CR-4 逻辑先于表现(单帧结算)。** 移动同步算出 `(newIndex, passedGo)` 并在**单帧逻辑内按序**依次完成:① 受控写位置 → ② 发薪 push(若 CR-3 成立)→ ③ 广播落地(CR-5)。**三步顺序是契约**(AC-18 须断言 ①<②<③ 的执行序,非仅终态)。棋子逐格 hop 动画由 VFX(19)/动画层**回放**既定结果,**不回授逻辑**(对齐 dice/economy"结果先于动画"原则)。**非重入契约(R2 systems-designer):** `Advance`/`TeleportTo` **非重入**——单次调用的 ①②③ 必须完整返回后,回合(2)方可发起下一程(如连续双点第二程)。回合(2)**不得**在某程 `OnPawnLanded` 广播尚未返回时调用下一程移动;移动不自防重入(单线程 BP + 禁 Latent 下顺序驱动天然成立),但此**程间原子性保证归回合(2)编排**,在此显式登记契约边界。**确定性范围声明(避免过强表述):** 本系统逻辑层满足确定性(纯整数、无随机、无帧依赖);"联网重放"**适配成本低但非零**——`OnPawnMoveStarted`/`OnPawnLanded` 经 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*Params` 声明、当前为**本地广播**,Full Vision 联网阶段须将其触发迁移到 `OnRep_CurrentTileIndex`/`NetMulticast`(技术债,与 OQ-Move-3 宿主形态 ADR 一并处理),**不应表述为"开箱即联网就绪"**。> 注:`OnRep_*` 与 `NetMulticast` **非自由互换**——`OnRep` 仅在复制值实际变化时触发、且不在权威端(listen-server host)触发;`NetMulticast` 处处触发(含 server)。OQ-Move-3 ADR 须据"server 端是否也需该广播驱动自身 VFX/HUD"裁定,非机械 find-replace。

**CR-5 落地移交(不拥有落地后果)。** 位置写定 + 发薪 push 完成后,移动广播 `OnPawnLanded(player, tileIndex, arrivalContext)` 并把控制交回玩家回合(2)的 `ResolvePhase` 触发落地结算。`arrivalContext ∈ {DiceMove, AdvanceCard, SentToJail}` **由 CR-2 入口的 `context` 参数透传而来**(移动不自决分类:`DiceMove`/`AdvanceCard` 由回合2/事件7 经 `context` 参数注入,`SentToJail` 由 `TeleportTo(paysGo=false)` 强制),供下游区分语境(**去坐牢 `SentToJail` 不触发买地/收租结算**)。移动**不拥有**落地后果(买地/收租归经济5、事件格效果归事件7、决策点归回合2)。

**CR-6 职责边界(本系统不拥有什么)。** 移动只产出"一次位移的编排"。不拥有:位置数学(`advance_index`/`passed_go_count`/`steps_between` 归棋盘1)、随机点数(骰子3)、回合流程与决策点(回合2)、落地金钱/事件后果(经济5/事件7)、棋子动画与音效(VFX19/动画/音频22)。决策点(掷骰由谁触发、何时进 ResolvePhase)由回合(2)编排,移动执行其位移后果。

### States and Transitions

**本系统主要是服务,而非逐回合状态机**(同棋盘/骰子/经济)。它**不持有任何持久状态**——棋子位置 `CurrentTileIndex` 物理存于 `PlayerState`(回合2 容器),移动只经受控写修改它。逻辑层无 FSM。

唯一的"瞬态"纯属**表现层**:棋子从起点格到落点格的逐格 hop(`In-Flight`),它**不影响逻辑**——逻辑落点在动画开始前已定(CR-4)。下表描述编排时序(非逻辑状态机):

| 阶段 | 含义 | 转换 |
|---|---|---|
| **Idle** | 无位移进行 | 回合(2)在 RollPhase 后调 `Advance`/`TeleportTo` → 进入位移编排 |
| **位移编排(单帧逻辑)** | 算落点 + 受控写 + 发薪 push + 落地广播 | 单帧内同步完成,无中间可存档态 |
| **In-Flight(纯表现)** | 棋子逐格跳动画回放 | VFX(19)/动画拥有;逻辑已完成,动画结束仅触发呈现层"到位"信号,不改逻辑 |
| **Landed** | 落地广播已发,控制交回 ResolvePhase | 回合(2)接管落地结算;移动回 Idle |

**序列化:** 移动无自有持久状态;`CurrentTileIndex` 随存档(21)由 `PlayerState` 序列化(每玩家一值)。位移编排是单帧同步,**不存在中途存档点**。

### Interactions with Other Systems

| 系统 | 数据流 | 谁拥有 |
|---|---|---|
| 骰子(3) | 收 `RollDice()` 的 `Total` 作 `Advance` 的 `steps`;只取位移量,忽略 `bIsDouble`/双点链(归回合2) | 骰子拥有随机点数;移动消费 |
| 棋盘(1) | 调 `AdvanceIndex(from,steps)→(newIndex,passedGo)`、`steps_between(from,target)`、`GetTileCount()`;读 `GetTileData(0).SalaryAmount` 供 push | 棋盘拥有位置数学;移动编排调用 |
| 玩家回合(2) | RollPhase 后被调 `Advance`/`TeleportTo`;`CurrentTileIndex` 经移动受控写入 `PlayerState`;落地广播交回 `ResolvePhase` | 回合拥有流程/容器/决策点;移动执行位移 |
| 经济(5) | **push** `(passedGo, SalaryAmount)` 发薪(CR-3);Utility 租 `dice_total` 由经济在 ResolvePhase 从回合掷骰上下文 **PULL**(CR-3.1,移动不 push/不缓存 `Total`) | 经济拥有发薪/租金公式;发薪 push 触发,Utility `Total` 经济自取 |
| 事件格(7) | "前进到最近X"卡调移动 `TeleportTo(target, paysGo=true)`;去坐牢调 `TeleportTo(JailIndex, paysGo=false)` | 事件拥有牌效果/触发;移动执行位移 |
| AI(10) | 读 `steps_between` 估值落点(经棋盘);AI 决策不改移动逻辑 | AI 拥有决策;移动只位移 |
| HUD(16) | 监听 `OnPawnMoveStarted`/`OnPawnLanded` 显示移动进度/落点提示 | 呈现层拥有显示 |
| VFX(19)/动画 | 监听 `OnPawnMoveStarted(from,to,path,passedGo)` 回放逐格 hop + 过 GO 高亮;监听 `OnPawnLanded` 落地 juice | 呈现层拥有动画 |
| 存档(21) | 序列化 `PlayerState.CurrentTileIndex`(经回合容器) | 存档拥有序列化 |

**事件(本系统广播,经 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*Params` 声明 + `BlueprintAssignable`;若改用单 struct payload 则其类型须 `USTRUCT(BlueprintType)`——注:当前两事件用位置参数列表声明,非单 struct;`TArray`/`TMap` 不可作 dynamic delegate 裸参,须包进 USTRUCT,见 OQ-Move-2 `HopPath`):**
- `OnPawnMoveStarted(Player, FromIndex, ToIndex, Steps, PassedGo)` — 位移编排完成、动画开始前广播,供 VFX 回放逐格路径 + 过 GO 高亮。
- `OnPawnLanded(Player, TileIndex, EArrivalContext)` — 落点定格、控制交回 ResolvePhase 前广播。`EArrivalContext ∈ {DiceMove, AdvanceCard, SentToJail}`。

**接口稳定承诺:** `Advance`/`TeleportTo`/`SetTileIndex`/`GetTileIndex` 签名稳定;事件 payload 字段只增不改语义;`EArrivalContext` 枚举只扩不改既有项(给下游 2/5/7/16/19/21 的稳定保证)。

**禁绕过受控写:** 任何系统不得直接改 `PlayerState.CurrentTileIndex`,只经移动 `SetTileIndex`(沿 player-turn AC-35 软/硬约束)。**(R-xreview 2026-06-03 交叉引用:player-turn 侧字段级 setter 称 `SetPosition`,仅供移动(4)调用;移动 `SetTileIndex` 为对外公开 API,内部最终经 player-turn `SetPosition` 落位。二者「公开 API→字段 setter」两层 vs 同层别名由 OQ-Move-3b ADR 裁定,实现期收敛单一命名链。见 player-turn 受控写接口面。)**

> 跨系统义务(回链下游 GDD,Phase 5 登记 systems-index 继承义务表):事件格(7) 须经移动 `TeleportTo` 实现"前进到最近X"(paysGo=true)与去坐牢(paysGo=false);VFX(19) 须承接 `OnPawnMoveStarted` 逐格 hop 回放(端到端动画 owner)。

## Formulas

> **本系统无自有公式。** 移动是编排层:全部位置数学归棋盘(1)拥有并已在 registry 验证(`systems-designer` re-check 确认"3 board 公式 + 3 编排谓词"数学封闭、无需自有公式)。移动只**引用** board 三公式 + 持有三条**编排谓词**(布尔逻辑,无变量表)。

### 引用的 board-data 公式(只读,不复制)
| 公式 | 表达式 | 输出 | 移动如何用 |
|---|---|---|---|
| `advance_index` | `mod(from+steps, N)` | [0,N−1] | `Advance` 落点(经 `AdvanceIndex` 原子接口) |
| `passed_go_count` | `floor((from+steps)/N) − floor(from/N)` | 有符号,**理论无界**(正常 −1..2;超界牌可极大,见 P-1 超界注 + 超界防护) | 发薪 push 门的 `passedGo` |
| `steps_between` | `mod(target−from, N)` | [0,N−1] | `TeleportTo(paysGo=true)` 前向步数 |

> **接口形态注(R2 systems-designer):** ① `advance_index` 与 `passed_go_count` **不是两个可分离调用的接口**——二者是 board 单一原子调用 `AdvanceIndex(from,steps)` 返回元组的两个分量(board L171 绑定契约,无独立 `PassedGoCount()` 公开接口)。上表三行是**公式引用**,非三个可分别调用的函数;实现者只调 `AdvanceIndex`(取元组)+ 独立辅助查询 `StepsBetween`。② 本档以 `steps_between` 指代 board F-3 的输出,board 该输出变量名为 **`steps_forward`**(函数名 `StepsBetween`)——同物异名,grep board 输出符号应找 `steps_forward`。

### 移动持有的三条编排谓词(无变量表,布尔逻辑)

**P-1 发薪 push 门**:`push_salary ⟺ (paysGo == true) ∧ (passedGo > 0)`
- `passedGo < 0`(逆向穿越 GO,**含后退多圈如 `passedGo=−2`**)→ 不发(经典忠实,`< 0` 覆盖所有负值);`passedGo = 0` → 不发;`passedGo ≥ 1`(含多圈 `passedGo=2`)→ push `(passedGo, SalaryAmount)`,金额由经济 F-1 算(`passedGo × SalaryAmount`)。movement 不拥有金额、不拥有 `passed_go_count` 算法,只拥有"何时 push"。
- **worked example(多圈 `passedGo=2`,N=40):** `from=0, steps=80` → `passedGo = floor(80/40) − floor(0/40) = 2` → push `(2, SalaryAmount)`,经济算 `2 × SalaryAmount`(经典 `2 × 200 = 400`)。
- **超界注**:board F-2 输出**理论无界**;`|steps|>2N` 异常牌可致 `passedGo` 极大。movement **不 clamp**(防破坏环路落点);push 后由经济侧 `passed_go` clamp[0,1000] **仅部分**兜底(见下方超界防护——`SalaryAmount` 侧上界归 OQ-Econ-10,未闭合)。

**P-2 `TeleportTo(paysGo=true)` 归约**:`steps = effective_steps(from, target, advanceOnZero)`,其后等价 `Advance(steps, paysGo=true)`
- **`effective_steps` 命名表达式(显式化 `0` 分支裁决,消除"隐藏公式"质疑;R2:`0` 解释权由调用方 `advanceOnZero` 注入):**
  `effective_steps(from, target, advanceOnZero) = (steps_between(from, target) == 0) ? (advanceOnZero ? GetTileCount() : 0) : steps_between(from, target)`
  这**不是新位置数学**(位置数学仍归 board:`steps_between`/`advance_index`),而是 P-2 的一条**分支裁决**——把 board F-3 L233 留给调用方的"`0` 解释权"**透传给调用方的 `advanceOnZero` 参数**(移动不自决、不"关死")。变量:`from`/`target ∈ [0,N−1]`,`N = GetTileCount()`(board),`advanceOnZero: bool`(事件7注入);输出 `effective_steps ∈ [0,N]`(`advanceOnZero=true` 时 [1,N] 前向到达;`advanceOnZero=false` 且 `target==from` 时 =0 原地)。
  - **worked example A(N=40,`advanceOnZero=true`):** `from=10, target=10` → `steps_between=0` → `effective_steps=40` → `Advance(40)`:`newIndex = mod(50,40) = 10`、`passedGo = floor(50/40) − floor(10/40) = 1 − 0 = 1`、发薪 1 次。
  - **worked example B(N=40,`advanceOnZero=false`):** `from=10, target=10` → `steps_between=0` → `effective_steps=0` → `Advance(0)`:`newIndex=10`、`passedGo=0`、不发薪、原地。
  - **⚠ OQ-Move-1(仅决定默认值,不决定裁决权归属)**:经典"前进到最近X"牌在 `target==from` 时调用方**应注入 true 还是 false**,待 Rento 实际行为核查;未闭合前 **AC-11 冻结为 PROVISIONAL**(见 AC 节,期望值取 `advanceOnZero=true`/整圈)。无论核查结论,`advanceOnZero` 参数化的 movement 契约不变,仅事件(7)填入的默认值回填。
- `target ∈ [0,N−1]` 全域已验证 reduction 正确(`steps_between` 顺时针前向 + `passed_go_count` 处理跨 GO)。

**P-3 `TeleportTo(paysGo=false)` 直达**:`SetTileIndex(target)`(**不调 `AdvanceIndex`**,入口 `ensure` 守界)、`passedGo` 硬置 0、不发薪。去坐牢路径(target 由事件格(7)保证 ∈[0,N−1])。

> **超界防护**:移动**不 clamp** `steps`(clamp 会破坏环路落点,与 board AC-29 契约一致);board 的"`|steps|>2N` 超界告警"经移动**向上冒泡**回合(2)/记结构化日志,不让异常牌(超大 steps)只在 board 层静默。经济侧 `passed_go` clamp[0,1000]（economy F-1,**乘前 clamp**:`clamp(passed_go,0,1000) × SalaryAmount`,故 1000 上界来自经济侧、非移动侧)**仅部分防护** `passedGo×SalaryAmount` int32 溢出——它**只护 `passed_go` 侧乘数**(clamp 到 1000);**`SalaryAmount` 侧上界未被本系统或经济侧 clamp 守护**:`1000 × SalaryAmount` 在 `SalaryAmount > ⌊(2³¹−1)/1000⌋ = 2,147,483`(如自定义棋盘设 3,000,000)时仍溢出。**发薪(salary)侧溢出兜底依赖 OQ-Econ-10**(`SalaryAmount ≤ 2,000,000` 加载期校验,归棋盘(1)/编辑器(26),**未闭合**)。
> **⚠ 第二条未守护溢出路径(R2 Finding C,systems-designer/economy-designer):** 上述 OQ-Econ-10 **仅覆盖发薪侧,非"完整"溢出兜底**。Utility 租 `dice_total × DiceMultiplierTable[i]`(economy F-4)中,`dice_total ∈ [2,12]` 有界,但 **`DiceMultiplierTable[i]` 无任何上界校验**(board 仅校验其长度==2 + 反向字段污染,不校验元素值上界):`12 × DiceMultiplierTable[i]` 在元素 > `⌊(2³¹−1)/12⌋ = 178,956,970` 时溢出 int32。OQ-Econ-10 **不覆盖** `DiceMultiplierTable`。**movement 诚实指出此第二溢出路径,但 owner 不在 movement**(R3 CD/economy-designer:movement 既不拥有 `DiceMultiplierTable` 也不拥有 Utility 租公式)——`DiceMultiplierTable[i]` 上界校验**独立登记 OQ-Move-6**,owner 派发棋盘(1)/编辑器(26),非塞进 OQ-Move-5。两路径 movement 侧均**不守界**(只读 board base 值消费,clamp 归数据校验层,见 Dependencies)——movement 的职责是**诚实登记两条依赖**,非自行 clamp。

## Edge Cases

**位移与落点**
- **若 `steps == 0`**(掷骰不可能[min 2];仅异常牌可传):`Advance(0)` → 原地不动、`passedGo=0`、不发薪、**不重新触发落地结算**(防重复收租);dev `ensure(掷骰路径 steps≥2)` 暴露异常牌。
- **若 `steps < 0`**(后退牌):正常处理。落点 `mod` 恒在 [0,N−1];`passedGo` 可能 <0(逆向穿 GO)→ 不发薪。后退落地**照常**触发落地结算(后退踩中地产仍收租)。
- **若 `|steps| > 2N`**(异常超大):board 照算 + 发"超界"告警;移动**冒泡**该告警给回合(2)/记日志;落点仍正确;`passedGo` 可能极大,经济 `clamp[0,1000]` 兜底防溢出。
- **若 `TeleportTo` 的 `target == from`(paysGo=true)**:裁决由调用方 `advanceOnZero` 注入(R2 Finding D)——`advanceOnZero=true`→前进整圈 N 步、`passedGo=1` 发薪、重触发落地(P-2);`advanceOnZero=false`→原地驻留、`passedGo=0`、不发薪。移动不自决。**`advanceOnZero=false` 原地驻留路径归约为 `Advance(0)`,故 `OnPawnLanded` 发火 0 次(AC-9a)——这是有意行为、非缺落地信号**:原地驻留不应重触发落地结算(防重复收租);若某张牌需要「前进到当前格仍触发某效果」,那是事件(7)的牌效果,而非移动的落地结算。(R3 systems-designer 指出此游戏规则原仅由「等价 Advance(0)」隐式决定,此处显式论证。)**与异常牌 `Advance(0)` 的语义分裂**:`Advance(0)` 来自异常牌(语义="不位移");`TeleportTo(target==from, advanceOnZero=true)` 来自"前进到X"牌走整圈语义(重触发落地)。实现者遇"机会牌让玩家前进到当前所在格"应走 **`TeleportTo` 路径**并由事件(7)注入 `advanceOnZero`,非 `Advance(0)`。(⚠ 经典牌的 `advanceOnZero` 默认值待 OQ-Move-1 核查;裁决权已参数化交事件(7),AC-11a 仅注默认期望取 true,**非冻结 gate**——见 AC-11b 解冻说明)
- **若 `TeleportTo` 的 `target` 越界**(<0 或 ≥N):`SetTileIndex` 入口 `ensure` 拒绝、不写位置;事件格(7)应保证合法,越界即上游 bug(dev ensure)。

**发薪边界**
- **若 `passedGo ≤ 0`**:不发薪(P-1 门)。
- **若 `passedGo == 2`**(多圈):发 `2 × SalaryAmount`(经济 F-1)。
- **若 `paysGo == false` 但几何路径跨过 GO**(去坐牢从高 index 经 0 到 Jail):**不发薪**(去坐牢强制 `passedGo=0`,即便跨过 GO 也不领钱——经典忠实"去坐牢不过 GO 领薪")。

**连续双点**
- **若连续双点第二程移动**:`from` 实时读 `PlayerState`(第一程已写定的 `newIndex`)、不缓存(CR-1);双点链/三连入狱判定归回合(2),移动只执行位移、不解释双点。**程间原子性(CR-4 非重入契约)**:每程 `Advance`/`TeleportTo` 独立发火各自的 `OnPawnMoveStarted`/`OnPawnLanded`(AC-17);回合(2)须在第一程 `OnPawnLanded` 返回后方发起第二程,**不得**在第一程落地结算(可能含"前进到X"牌中途传送)未完成时插入第二程——该程间排序与重入防护归回合(2)编排,移动不自防。

**去坐牢落地**
- **若 `arrivalContext == SentToJail`**:`ResolvePhase` **不触发**买地/收租/事件结算(玩家进入在狱态,归回合2/事件7);移动职责止于把棋子放到 Jail 格并广播落地。

**N / GO 格边界**
- **若落点 == GO 格(index 0)且停留**:`passedGo=1`,发薪一次(经济 CR-2);GO 格本身无额外结算,`SpecialAction=CollectSalary` 仅 UI 标记、不构成第二次发薪(经济 CR-2 防双发)。
- **若 `from` 越界**(读档损坏等):board 公式对越界 `from` 数学鲁棒(`((A%N)+N)%N`,board AC-34),落点仍正确;dev `ensure` 暴露上游状态损坏。

## Dependencies

### 上游(本系统依赖)
| 系统 | 强度 | 接口 |
|---|---|---|
| 棋盘数据(1) | 硬 | `AdvanceIndex(from,steps)→(newIndex,passedGo)`、`steps_between(from,target)`、`GetTileCount()`、`GetTileData(0).SalaryAmount` |
| 玩家回合(2) | 硬 | 提供当前玩家、`PlayerState.CurrentTileIndex` 受控写容器、RollPhase 后调 `Advance`/`TeleportTo`、落地交回 `ResolvePhase` |
| 骰子(3) | 硬 | `RollDice()` 的 `Total` 作 `Advance` 的 `steps`;只取位移量,忽略 `bIsDouble` |

### 下游(依赖本系统)
| 系统 | 强度 | 读/写本系统 |
|---|---|---|
| 经济(5) | orchestrated | 收移动 push 的 `(passedGo, SalaryAmount)` 发薪;Utility 租 `dice_total` 由经济在 ResolvePhase 从回合掷骰上下文 PULL(CR-3.1,移动不 push/不缓存 `Total`);非严格上下游(回合编排,经济不反向读移动) |
| 事件格(7) | 硬 | 调移动 `TeleportTo` 实现"前进到最近X"(paysGo=true)/去坐牢(paysGo=false) |
| AI(10) | 硬 | 读棋子位置 + `steps_between` 估值落点(经棋盘);AI 决策不改移动逻辑 |
| HUD(16) | 硬 | 监听 `OnPawnMoveStarted`/`OnPawnLanded` 显示移动进度/落点提示 |
| 游戏反馈 VFX(19) | 硬 | 监听 `OnPawnMoveStarted` 回放逐格 hop + 过 GO 高亮;`OnPawnLanded` 落地 juice |
| 音频(22) | 软 | 监听 `OnPawnMoveStarted`/`OnPawnLanded` 播 hop 步进"哒哒"+ 落地"咚"音(呈现侧纯叶子,hop 节流;audio L216 已对齐) |
| 存档(21) | 硬 | 序列化 `PlayerState.CurrentTileIndex`(经回合容器) |

### 移动(4)↔ 骰子/经济关系(orchestrated,非严格上下游)
回合(2)在 RollPhase 后把骰子 `Total` 交移动作位移;移动把 `(passedGo, SalaryAmount)` 在回合编排下 push 给经济发薪;Utility 租 `dice_total` 由经济在 ResolvePhase 从回合掷骰上下文 PULL(CR-3.1,移动不投递 `Total`)。移动不反向读经济状态。

> **双向一致性状态(R2 复核 systems-index):**
> 1. **事件格(7) → 移动(4) 已在 index 标注** ✅:systems-index L112 事件(7)行已含 `depends-on: 1,2,3,4,5`(R-move 2026-06-02 已同步);此前"缺 4"的发现已闭合。
> 2. **AI(10)/VFX(19) 已正确标 depends-on 4**(systems-index 行已含),无需补。
> 3. **移动(4) → 经济(5) 为 orchestrated**(回合编排,非硬依赖):发薪侧 push、Utility `Total` 侧改 PULL(R2 Finding E),不新增环,与经济 GDD"移动 orchestrated 非严格上下游"框架一致。

## Tuning Knobs

| 旋钮 | 作用 | 安全范围 | 过低/过高后果 | 归属 |
|---|---|---|---|---|
| `hop_duration_per_tile`(表现) | 棋子每跳一格的动画时长 | 0.05–0.3s(默认待定) | 过低=移动瞬移看不清路径,削弱"逐格期待";过高=移动拖沓,违反④易上手的顺滑感 | **VFX(19)/动画拥有**,非本系统;此处仅登记其存在与体验影响 |
| `instant_snap_on_long_move`(表现) | 超长位移(多圈)是否跳过逐格动画直接定位 | bool(默认 false) | true 牺牲过程感换节奏;长位移逐格播放可能过久 | **VFX(19)拥有**;移动逻辑无关 |
| `skip_move_animation`(无障碍) | 一键关闭移动动画(无障碍/加速) | bool(默认 false) | 关闭后移动即时呈现 | **设置&House Rules(23)/无障碍拥有** |

> **本系统无自有逻辑调参旋钮。** 移动是编排层——位移量来自骰子(3,`die_faces`/`dice_count` 旋钮归骰子)、落点与单圈长度由棋盘(1,`N` 旋钮归棋盘)决定、发薪额归经济(5)。移动不引入任何影响平衡或玩法数值的旋钮;上表全部是**表现/无障碍**旋钮且归其他系统拥有,在此登记以记录其对移动体验的影响,**不在本系统重复定义**(指向真值来源)。

> **跨系统旋钮影响(只读,不拥有)**:`N`(board)改变单圈格数 → 影响过 GO 频率(经济现金注入节奏)+ 单次位移视觉时长;属 board/经济/移动协同,移动只消费不调。

## Visual/Audio Requirements

**n/a 自有渲染 —— 移动是编排层,不渲染视觉、不触发音频。** 但棋子逐格 hop、过 GO 高亮、落地定格是核心循环的可见时刻,由 VFX(19)/动画/音频(22) 拥有。本系统为其提供数据与事件,并钉最低呈现契约:

- **端到端 owner = VFX(19)/动画/音频(22)**:逐格 hop 动画、过 GO 高亮、落地定格 juice、棋子移动音效。具体动画曲线/时长/音效归 VFX19/动画/audio22。
- **本系统义务边界**:提供 `OnPawnMoveStarted(Player,From,To,Steps,PassedGo)` + `OnPawnLanded(Player,TileIndex,EArrivalContext)`;结果在动画开始前已产出(逻辑先于表现,CR-4),呈现层只回放既定结果,**不影响位移逻辑**。
- **好/坏语境**:`OnPawnLanded.EArrivalContext` 已携带语境(DiceMove 普通落地 / SentToJail 去坐牢),VFX 据此区分反馈(普通定格 vs 入狱押送),不需额外接口。
- **回链**:登记 systems-index 继承义务表(VFX19 行,OQ-T-Move-3);hop path 由 VFX 自建 vs 移动提供 `HopPath` 归 VFX 裁定(OQ-Move-2)。

> 📌 **Asset Spec**:art bible 批准后,可运行 `/asset-spec system:movement` 产出棋子/hop 路径高亮的视觉规格(若需要)。

## UI Requirements

**n/a —— 本系统无任何 UI。** 移动不绘制屏幕元素,只暴露 `Advance`/`TeleportTo`/`SetTileIndex`/`GetTileIndex` 接口与 2 个事件。棋子位置显示、移动进度、落点提示由 **HUD(16)/VFX(19)** 实现;掷骰按钮归 HUD/骰子 UI。本系统供 `OnPawnMoveStarted`/`OnPawnLanded` 事件 + `GetTileIndex` 查询。

> **无移动专属 UX Flag** —— 移动无玩家直接操作的专属界面(掷骰交互归骰子/HUD UI)。棋子移动的呈现规格(若需)在 Pre-Production 由 VFX19/HUD16 的 `/ux-design` 覆盖,非本 GDD。

## Acceptance Criteria

> *由 `qa-lead`（lean 模式 H 节高风险派发）对抗性校验,5 条有效 BLOCKING + RECOMMENDED 已整合;qa-lead 的 F-1（称 AC-7 缺 passedGo=0 边界）经主会话独立核验**驳回**——AC-7 草稿已含 passedGo=0,即 `>` vs `≥` 关键边界。*
> **核心原则(对齐 economy/dice/player-turn):平衡/手感永不作 [Logic] gate。移动无 RNG、无自有公式、无平衡值 → [Logic] 只断言"给定输入,编排谓词/位移引用算得对"(确定性,纯整数索引)。**

### A. 受控写 & 位置不变式（CR-1）
- **AC-1** [Logic] 落点不变式 `CurrentTileIndex ∈ [0,N−1]`:from=38, steps=5, N=40 → `newIndex==3`。
- **AC-2** [Logic] `from` 实时读不缓存(防连续双点读旧起点):连续两次 `Advance`(from=0,steps=5 → newIndex=5;再 steps=3 从 5 起 → 8),证伪帧缓存 bug。**fixture 要求**:第二次 `Advance` 的 `from` 须经 `GetTileIndex(player)` 从 `PlayerState` **实时读取**(非测试码硬传 5),且两次**同一测试帧内串行调用**(无 Tick 间隔)——否则无法证伪"双点第二程读旧起点"。注:单线程 BP + 禁 Latent 下此竞态**理论不可达**(对齐 player-turn R2),AC-2 主防 BP 图人为缓存 bug。
- **AC-3** [Advisory·code-review] 受控写软约束:只经 `SetTileIndex` 写 `PlayerState.CurrentTileIndex`(对齐 player-turn AC-35 / economy AC-4 / dice AC-20)。**范围限定(unreal-specialist)**:本项目 Blueprint 为主,BP 层无引擎级"对外只读对内可写"的精细访问控制,故此约束在 **BP 层仅社会性约束(code-review)、仅 C++ 层可静态验证**;`SetTileIndex` 本身须为 C++ `UFUNCTION` 才能让 C++ `private` 生效。OQ-Move-3 选 C++ 强封装(位置 private + 受控访问器)→ AC-3 升 [Logic] 静态扫描禁直写,**前置条件:player-turn OQ-1 生命周期 ADR 已 Accepted**(共享同一宿主形态决策)。
- **AC-4** [Logic·关键 guard] `SetTileIndex` 入口守界:`SetTileIndex(-1)` / `(N)` → `ensure` 拒绝、不写、`CurrentTileIndex` 不变(`ensure`+早返语义:报告后继续、保留旧值,可断言"不变";非 `check` 硬停)。

### B. Advance 编排（CR-2/CR-3,P-1）
- **AC-5** [Logic] 基础位移:from=5, steps=7 → `newIndex==12`, `passedGo==0`。
- **AC-6** [Logic] 过 GO:from=38, steps=5, N=40 → `newIndex==3`, `passedGo==1`。
- **AC-7** [Logic·关键] 发薪 push 门 P-1 四值边界(验 `>` 而非 `≥`):`passedGo=0` → **不 push**(关键边界);`passedGo=1` → push `(1, SalaryAmount)` 恰 1 次;`passedGo=2`(多圈)→ push `(2, SalaryAmount)`;`passedGo=−1`(后退穿 GO)→ 不 push。
- **AC-7b** [Logic] CR-3.1 `Total` 仅作位移消费、不投递:`Advance(steps=Total, context=DiceMove)` → 移动**不**对经济发起任何 `Total` push/cache 调用(spy 经济接口:`Total` 投递调用次数==0);`Total` 仅被当作 `steps` 用于 `AdvanceIndex`。证伪 R1 的 store-and-forward 模型(Finding E:Utility `dice_total` 由经济解决期 PULL,非移动推送)。**注**:经济解决期 PULL 的正确性 + 双点/事件额外骰单源去歧义归经济(5)/回合(2) AC(OQ-Move-5 跨 doc),本 AC 仅断言 movement 侧不投递。
- **AC-8** [Logic] 后退位移:from=2, steps=−5, N=40 → `newIndex==37`, `passedGo==−1` → 不发薪。
- **AC-9a** [Logic] `steps=0` 早返:`Advance(0)` → 原地、`passedGo=0`、不 push、`OnPawnLanded` 发火 **0 次**(不重复触发落地结算)。
- **AC-9b** [Advisory·code-review] 掷骰路径 `ensure(steps≥2)` 存在(Dev 构建暴露异常牌;Shipping 编译为空,非 [Logic])。
- **AC-9c** [Logic·关键] GO 格落地不双发:from=35, steps=5, N=40 → `newIndex==0` ∧ `passedGo==1` ∧ 发薪 push **恰 1 次**(GO 格 `SpecialAction=CollectSalary` 仅 UI 标记,不触发第二次 push,经济 CR-2 防双发)。

### C. TeleportTo（CR-2,P-2/P-3）
- **AC-10** [Logic] `paysGo=true` 归约:from=5, target=15 → `steps=steps_between(5,15)=10` → `newIndex==15`。
- **AC-11a** [Logic] `target==from`、`advanceOnZero=true` 整圈分支(M-1):from=10, target=10, paysGo=true, advanceOnZero=true → `effective_steps=N=40`(P-2)→ `newIndex==10`, `passedGo==1`, 发薪 1 次。**确定性可测**——移动只执行注入的 `advanceOnZero`,与 OQ-Move-1 无关。
- **AC-11b** [Logic] `target==from`、`advanceOnZero=false` 原地分支:from=10, target=10, paysGo=true, advanceOnZero=false → `effective_steps=0` → `newIndex==10`, `passedGo==0`, 不发薪。**确定性可测**。
  > **R2 Finding D 解冻说明:** 把 `target==from` 裁决参数化(`advanceOnZero`)后,movement 两分支**均确定性可测、不再依赖未核实事实**——故 AC-11a/11b **均为 [Logic] gate**(原 AC-11 的 PROVISIONAL 冻结解除)。OQ-Move-1 的不确定性(经典"前进到最近X"牌应注入 true 还是 false)已**整体移交事件(7)**:那是事件(7)的 test 义务(OQ-T-Move-2),不再是 movement 的 gate 冻结项。movement 不再背"默认值未核实"的债。
- **AC-12** [Logic] `paysGo=false` 直达(去坐牢):`TeleportTo(JailIndex, paysGo=false)` → `newIndex==JailIndex`、`passedGo` 强制 0、不 push 发薪;**不调 `AdvanceIndex`**,入口 `ensure` 守界(M-2)。**须含跨 GO 向量**:`from=38, JailIndex=10, N=40`(几何路径 38→…→0→…→10 **跨 GO**)→ 仍 `passedGo==0`、不 push(证伪"跨 GO 仍不发薪"经典忠实;仅 `from<JailIndex` 的非跨 GO 向量不充分)。
- **AC-12b** [Logic] 去坐牢 context 强制(M-2,R2 Finding B movement 侧):`TeleportTo(JailIndex, paysGo=false)` → `OnPawnLanded.arrivalContext == SentToJail`(移动**强制**置 `SentToJail`,非调用方注入)。这是 movement **可运行时断言**的透传正确性,补此前 AC-12/AC-14 未覆盖的"`paysGo=false` 路径 context 取值"。**注**:落地结算的**实际抑制**(`SentToJail` 时不进买地/收租分支)归 player-turn(2) ResolvePhase,见 AC-15 + 跨 doc followup。
- **AC-13** [Logic·关键 guard] `target` 越界:`TeleportTo(-1 / N, ...)` → `ensure` 拒绝、不写、dev ensure。

### D. 落地移交（CR-5）
- **AC-14** [Logic] `arrivalContext` 透传:`Advance(steps=5, context=AdvanceCard)` → `OnPawnLanded.arrivalContext==AdvanceCard`。移动**透传调用方注入的 context,不自决分类**(DiceMove/AdvanceCard 由回合2/事件7 区分传入)。
- **AC-15** [Advisory·code-review] `SentToJail` 抑制买地/收租结算:移动侧的 context 强制取值已由 **AC-12b [Logic]** 覆盖(`paysGo=false`→`arrivalContext==SentToJail`);本 AC 仅余 **实际抑制 ResolvePhase 买地/收租分支归 player-turn(2)** 部分——移动不执行 ResolvePhase、不可运行时断言抑制本身。⚠ **跨 doc followup(R2 Finding B,须 `/propagate-design-change`):** qa-lead 核验 player-turn.md 全文**无** `SentToJail`/`arrivalContext`/`OnPawnLanded` 命中、**无对应 AC**,且 player-turn 已 Approved 不会回读 OQ-T 表 → 此抑制("去坐牢却仍能买落地格地"的核心规则正确性)当前**无任何 BLOCKING 覆盖**。**修复(producer 协调,跨 Approved 系统)**:player-turn 须补一条 [Logic] AC(`arrivalContext==SentToJail`→ResolvePhase 不进买地/收租分支),并在本 AC-15 + systems-index OQ-T-Move-1 行回填该实 AC 号(从"待确认"推到实链)。本档**不单方改 player-turn**(用户 R2 决策:cross-doc 留 followup)。**✅ propagate RESOLVED(2026-06-02):player-turn 已补 [Logic] AC-46(`arrivalContext==SentToJail`→ResolvePhase 买地决策/收租结算入口 hook 调用次数==0,对照组 DiceMove==1)+ CR-3.1/Edge Cases 抑制规则;实 AC 号 player-turn AC-46 已回填本行(从"待确认"推到实链)。**

### E. 事件广播 payload（CR-5）
- **AC-16** [Logic] `OnPawnMoveStarted(Player,From,To,Steps,PassedGo)` 字段含正/反向:(a) from=38,steps=5 → `From==38, To==3, Steps==5, PassedGo==1`;(b) from=2,steps=−5 → `To==37, PassedGo==−1`(后退 payload 正确)。
- **AC-17** [Logic] 事件次数精确:一次 `Advance` → 恰 1 次 `OnPawnMoveStarted` + 1 次 `OnPawnLanded`(steps=0 见 AC-9a:0 次 Landed)。

### F. 逻辑先于表现（CR-4)
- **AC-18** [Logic] `Advance()` 返回后三状态**同步可断言且有序**:位置已写定 ∧ 发薪已 push(若 P-1 成立)∧ 落地已广播——fixture 在 call 返回后直接断言,不依赖动画/帧时序(纯整数索引,无 float→无跨编译配置分歧)。**须断言执行顺序**(CR-4"依次"语义):用 spy/mock 记录三操作实际序,断言 `SetTileIndex_order < push_order < OnPawnLanded_order`(仅测终态会放过"先广播后写位置"的错误实现)。**push-skipped 子例(P-1 不成立的常态):** 当 `push_salary==false`,断言 `SetTileIndex_order < OnPawnLanded_order` **且无 push 发生**(否则现行措辞的"三状态"隐含 push 已发,放过"未过 GO 却发薪"的错误)。
- **AC-19** [Advisory·playtest] 移动顺滑无卡顿、逐格 hop 节奏(手感)。非 [Logic]:表现质量。

### G. 接口稳定 & 职责边界
- **AC-20** [Logic·CI 构建] `EArrivalContext` 初始**恰含** `{DiceMove, AdvanceCard, SentToJail}` 三项(之后只扩不改)——枚举项数量是可测逻辑断言。枚举须 `UENUM(BlueprintType)` + `uint8` 底层类型(BlueprintType 枚举限 256 项/uint8;勿声明 `: int32` 否则破坏 BlueprintType)。**注**:两事件经 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*Params` 声明 + `BlueprintAssignable` 的"编译通过"部分由 **CI 构建验证**(headless `-nullrhi`),**无对应 Spec 测试文件**(避免空壳测试);delegate 宏 token 与签名须在实现期对 **UE5.7 核实**(VERSION.md 知识缺口警告:LLM 知识止于 ~5.3)。
- **AC-21** [Logic·CI 构建验证] **公共编排接口** `Advance`/`TeleportTo`/`GetTileIndex` 均标 `UFUNCTION(BlueprintCallable)`、BP 可调、编译通过——由 CI 构建保证,非 Spec 运行时断言。**`SetTileIndex` 的 BP 暴露强度不在本 AC 断言**——它是受控写入口,暴露形态由 OQ-Move-3 ADR 裁定(C++ 强封装→C++-only 或 `meta=(BlueprintProtected/BlueprintPrivate)`;BP 约定→`BlueprintCallable`)。**理由(R3 unreal-specialist+CD):** 原无条件 `SetTileIndex` 标 `BlueprintCallable` 与 OQ-Move-3(a) C++ 强封装**直接冲突**(任意 BP 可调=重开受控写漏洞),实现者无法执行。CD/unreal 倾向 **Mode D**(`SetTileIndex` C++-only,写入只走 `Advance`/`TeleportTo`),但此为 OQ-Move-3 ADR 裁决,本 AC 仅去除写死的矛盾。
- **AC-22** [Advisory·code-review] 职责边界(CR-6):移动实现内**无** `mod(from+steps,N)`/`floor((from+steps)/N)`/`mod(target−from,N)` 公式直实现,只调 board `AdvanceIndex`/`steps_between`/`GetTileCount`(防移动复制 board 拥有的位置数学)。**前置条件注(unreal-specialist)**:此防护以 board `AdvanceIndex` 本身为 **C++ `UFUNCTION` 实现**为前提(board GDD F-2 line 211 建议 `UBoardMathLibrary`);若 board OQ-6 ADR 选纯 BP 实现,则 BP `%` 负数坑/`floor` 截断坑内聚于 board 接口内,届时 board F-2 的 BP 实现约束才是真正防线,AC-22 只保证移动不**额外**复制公式。注:`effective_steps` 的 `0→N` 分支(P-2)经 `GetTileCount()` 取 N 后仍走 `AdvanceIndex`,不触公式陷阱。

### H. 鲁棒性 & 跨系统
- **AC-23** [Logic] `from` 越界鲁棒:`Advance(from=−1, steps=5, N=40)` → board 公式数学鲁棒(`((A%N)+N)%N`,board AC-34)不崩、落点按 board 定义 + dev ensure(`AddExpectedError` 捕获)。移动信任 board 兜底、不抑制 ensure。
- **AC-24** [Advisory·code-review] steps 超界冒泡(M-4):board 的 `|steps|>2N` 告警经移动**不抑制地**向上传(回合2/结构化日志);board 侧发火责任归 board AC,移动侧责任=不静默吞告警。

> **[Logic] gate（BLOCKING PR)= 23 条:** AC-1/2/4/5/6/7/7b/8/9a/9c/10/11a/11b/12/12b/13/14/16/17/18/20/21/23。**[Logic·PROVISIONAL,冻结]= 0 条**(R2 Finding D:AC-11 参数化后拆为 AC-11a/11b 两条确定性 [Logic],冻结解除;默认值不确定性整体移交事件(7) OQ-T-Move-2)。**[Advisory]= 6 条:** AC-3/9b/15/19/22/24。**总计 29 条。**(AC-20/21 标 [Logic·CI 构建]:"编译通过"部分归 CI headless `-nullrhi` 构建验证、无 Spec 文件,见各条注;实际可跑 Spec ≈ 21 条 + 2 条 CI 构建。)
> **CR/谓词覆盖:** CR-1→1/2/3/4;CR-2/3→5/6/7/8/9a/9c/10/11a/11b/12/12b/13;CR-3.1→7b;CR-4→18;CR-5→12b/14/15/16/17;CR-6→22;P-1→7;P-2→10/11a/11b;P-3→12。**全覆盖,含 M-1(AC-11a/11b)/M-2(AC-4/12/12b)/M-4(AC-24);R2 补 CR-3.1(AC-7b)、去坐牢 context 强制(AC-12b)此前覆盖缺口。**

### 跨系统测试义务（OQ-T-Move-*,回链下游 GDD,Phase 5 登记 systems-index 继承义务表)
- **OQ-T-Move-1 → 玩家回合(2)🔴 R2 Finding B:** ResolvePhase 须消费 `OnPawnLanded.arrivalContext`,`SentToJail` 时**不触发**买地/收租结算分支(AC-15 抑制侧归 2)。**player-turn 当前无此 AC、无 `SentToJail`/`arrivalContext`/`OnPawnLanded` 命中且已 Approved** → player-turn **须补一条 [Logic] AC**(`arrivalContext==SentToJail`→ResolvePhase 不进买地/收租),并回填实 AC 号至 movement AC-15 + systems-index OQ-T-Move-1 行。须 producer 协调 `/propagate-design-change`(跨 Approved 系统)。**✅ RESOLVED 2026-06-02:player-turn 已补 AC-46(SentToJail 抑制)+ AC-47(程间非重入,承接 CR-4),grep 核实落地;实 AC 号已回填。**
- **OQ-T-Move-2 → 事件格(7):** 7 须经移动 `TeleportTo(target, paysGo=true, advanceOnZero=?)` 实现"前进到最近X"、`TeleportTo(JailIndex, paysGo=false)` 实现去坐牢,并保证 `target ∈ [0,N−1]`。**(R2 Finding D)** 7 须为每张"前进到X"牌**注入 `advanceOnZero`**(`target==from` 时整圈 vs 原地);经典牌默认值待 OQ-Move-1 核查后由 7 填入,并须有集成测试覆盖所注入分支。
- **OQ-T-Move-3 → VFX(19):** 19 承接 `OnPawnMoveStarted` 逐格 hop 回放(端到端动画 owner)+ **逐格期待/屏息的 experiential 体验契约**(R2 Finding A:核心循环第一拍的"看着棋子逼近命运"felt 体验归 VFX19,非仅 hop-path 数据义务——art bible/VFX GDD 须背此 experiential AC,否则 concept 30 秒循环爽感无 owner);hop path 由 VFX 自建 vs 移动提供 `HopPath` 归 VFX 设计裁定(OQ-Move-2,`HopPath` 若为 `TArray<int32>` 须包进 USTRUCT)。
- **OQ-T-Move-4 → 经济(5)🔴 R2 Finding E:** 移动 push `(passedGo, SalaryAmount)` 触发经济 F-1 发薪(经济侧消费正确性归经济 AC-6/AC-7,本档 AC-7 验 push 时机/次数)。**Utility 租 `dice_total` 改为经济在 ResolvePhase 从回合掷骰上下文 PULL**(非移动 push,Finding E 对齐 economy AC-18 不缓存),经济须保证"当前正在结算的这一程" `Total` 单源、双点链/事件额外骰下不取错值(OQ-Move-5);movement 侧只断言不投递(AC-7b)。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-Move-1** | 经典"前进到最近X"牌在 `target==from` 时事件(7)应注入 `advanceOnZero=true`(整圈,本档 AC-11a 取此默认)还是 `false`(原地)——待 Rento Fortune 实际行为事实核查。**R2 Finding D 后此问题归属事件(7) 默认值选择**(裁决权已参数化交调用方,movement 两分支均确定性可测、不再冻结 gate);movement 仅在 AC-11a 注默认期望。 | 事件(7) 设计 / MVP 冻结前核查 |
| **OQ-Move-5 ✅ RESOLVED(propagate 2026-06-02)** | (R2 Finding E)Utility 租 `dice_total` 解决期 PULL 的 **holder 机制 + owner**。**✅ 闭合:economy line 57/102/330 旧 push 措辞已清理为 PULL(grep 核实);holder owner=回合(2) 已落 player-turn CR-3.1(ResolvePhase 暴露当前程 `Total` 单源)+ economy/player-turn Interactions 玩家回合行;单源去歧义(双点链/事件额外骰)在 player-turn CR-3.1 钉死、事件额外骰 dice_total 由事件(7)注入(经济 OQ-T-Econ-3)。**〔历史问题,已修复〕**economy.md line 57/102/330 **曾**写「移动(4)…收本次移动骰 `Total` 供 Utility 租金」(R2 前旧 push 语义),与本档 CR-3.1「移动不投递、经济 PULL」方向相反——R2 single-doc 修订制造的跨档真矛盾(照旧措辞实现会造出被 AC-7b「投递次数==0」立即打死的通道),**已于本次 propagate 清理为 PULL(见上 ✅)**。**holder owner 判定=回合(2)**(持本回合 `RollDice()` 结果,唯一能在 ResolvePhase 暴露「当前正在结算这一程的 `Total`」者);economy F-4「显式参数传入/不读缓存」与 PULL **兼容(非矛盾)**,真缺口是「正常落地路径谁递参」无人承诺。+ **单源去歧义**(双点链/事件额外骰下 `dice_total` 唯一来源,与 economy OQ-T-Econ-3 协同)。涉及回合(2)/经济(5)/事件(7),须 `/propagate-design-change`。 | 下游 5/7 开工前 propagate |
| **OQ-Move-6 ✅ RESOLVED(propagate 2026-06-02)** | (R3 CD/economy-designer 拆单)Utility 侧 `DiceMultiplierTable[i]` 上界校验(防 `12×mult` int32 溢出,OQ-Econ-10 确认不覆盖)。**owner 不在 movement**(movement 既不拥有该表也不拥有 Utility 租公式),独立派发棋盘(1)/编辑器(26),建议并入 OQ-Econ-10 scope——非塞进 OQ-Move-5(架构 holder OQ)。**✅ 闭合:board-data 补 AC-23j(`DiceMultiplierTable[i] ∈ [1, DICE_MULT_MAX]`,`12×DICE_MULT_MAX ≤ INT32_MAX`)+ Validation 拒绝规则 + Tuning 上界;economy OQ-Econ-10 并轨注(grep 核实)。** | ✅ 已 propagate(board-data AC-23j) |
| **OQ-Move-2** | `OnPawnMoveStarted` 的逐格 hop path:VFX(19) 自 `(From,Steps,N)` 重建(引入对 board N 的隐依赖)vs 移动 payload 增 `TArray<int32> HopPath`(移动展开,非新公式)。 | VFX(19) 设计时 |
| **OQ-Move-3 ⚠ ADR** | 受控写宿主形态:`CurrentTileIndex` 容器 + `SetTileIndex` 封装强度。与 dice OQ-1 / player-turn OQ-1 / economy OQ-Econ-1 **协调同一生命周期层 ADR**。选 C++ 强封装(位置 private + 受控访问器)→ AC-3 升 [Logic] 静态扫描禁直写;BP 约定 → 软约束 code-review。**R2 待 ADR 决议项:**(a) 若走 C++ 强封装,`SetTileIndex` 标 `BlueprintCallable` 会**重开**受控写漏洞(任意 BP 可调)——须改 C++-only 或 `meta=(BlueprintProtected)`,与 AC-21 的 `BlueprintCallable` 标注张力须裁;(b) 访问器命名 movement 称 `SetTileIndex`、player-turn 称 `SetPosition`——同一受控写接缝两侧异名,须收敛或显式标别名;(c) 联网阶段 `OnRep_*` vs `NetMulticast` 非自由互换(见 CR-4 注),据 server 是否需自驱广播裁定。 | 下游 5/6/7 开工前 `/architecture-decision` |
| **OQ-Move-4** | steps 超界告警(board `|steps|>2N`)经移动向上冒泡的具体形态(`OnWarning` 事件 vs 结构化日志),与 board AC-29 协同。 | 实现期 |
