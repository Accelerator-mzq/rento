# AI 对手 (AI Opponent)

> **Status**: In Design
> **Author**: msc + design-system agents
> **Last Updated**: 2026-06-04
> **Implements Pillar**: ③ 运气×策略交织(AI 提供策略对抗面)+ ④ 易上手(难度分级让休闲玩家也能赢)

## Overview

AI 对手系统是《骰子大亨》MVP 单机核心的对手大脑——它为所有非人类玩家(`bIsAI==true`)**计算出一个人类玩家本会通过 UI 做出的那些选择**:买不买这块地、要不要建房、何时抵押筹钱、被关进监狱是付保释还是赌掷骰。它有两个面:对内是一个**决策策略层**——玩家回合(2)的 CR-8 在每个决策点**同步**调用 AI 接口(`DecidePostRollActions` 等 hook)、立即取回一组确定性动作并推进回合;本系统填充这些 hook 背后的**决策逻辑**,消费经济(5)/地产(6)/建房(8)/棋盘(1)已定的估值事实(租金、垄断、净可变现值)来给每个选择打分,用骰子(3)的可种子化 RNG 注入受控随机。对玩家则是**一个可信、可被棋力战胜的对手**——它"像个人在玩、不犯蠢",会买地、集组、卡点建房、被逼到墙角时变卖家当挣扎,让你在没有真人对手的牌桌上仍能体验"算计并碾压一个对手"的社交博弈内核。

它通过 `AIDifficulty`(休闲 Casual / 普通 Normal / 精明 Sharp 三档,字段归玩家回合2)调节棋力:三档**全部用加权启发式打分**(无博弈树前瞻),靠估值权重与随机度的不同产生玩家**可感知**的行为差异——休闲档会漏买好地、买得保守;精明档会卡现金缓冲、优先集垄断组、果断建房。

它**不拥有**:回合流程与"何时轮到谁/何时调 AI"(回合2 CR-8 同步调用协议)、AI 决策的随机源(骰子3 可种子化 RNG)、AI 行动的可观察呈现("看 AI 买地建房"的动画归 HUD16 `OnAIActionExecuted`,框架不为回放门控——player-turn R6 RETREAT)、以及一切被它消费的经济/地产数值(它只读不重定义)。它只拥有**"给定一份 GameStateSnapshot,这个难度的 AI 选择做什么"这组决策事实**。它做得好时,单机一局有一个让你想赢、赢了有成就感的对手;没有它,MVP 的 vs-AI 核心(concept 唯一的单机验证路径)根本跑不起来。

## Player Fantasy

**核心幻想:"它在跟我斗"——一个像人一样精明、会跟你抢地、会卡你建房、又能被你的棋力战胜的对手,让一个人的牌局也有"又坑又赢"的爽快。**

你在做一笔交易的算计:橙色组就差最后一块,你绕到那格前盘算着现金够不够。结果上一圈 AI 抢先把它买走了——还在你停租最贵的红组前,悄悄建起了两栋房。你愣了一下:**它不是在乱走,它在跟我斗。** 这一下"对手活过来了"的瞬间,是单机局有灵魂的关键。然后你绕开它的杀手区、用一笔抵押凑齐了蓝组反将一军,看着它现金见底、变卖家当、最终破产——**你赢了,而且是赢了一个值得赢的对手。** 这份成就感,是 concept「在朋友之间又坑又赢」在没有真人时的替身兑现——**〔MVP 范围诚实(game-designer R-review GD-R2):MVP vs-AI 兑现「赢」这一半;「坑」(对 AI 发起交易/拍卖/使坏)依赖响应方 hook(`DecideTradeResponse`/`DecideAuctionBid`)全在 Alpha,MVP 不含。故 MVP 单机验证的是"赢一个值得赢的对手",非完整社交博弈。〕**

它服务支柱③(运气×策略:AI 是策略对抗面——没有一个会算计的对手,棋力就无处施展,游戏退化成跟运气掷骰)与支柱④(易上手:三档难度让休闲玩家也能赢,精明档给硬核玩家挑战)。情感弧按难度分三种关系:
- **休闲档(陪练):** 一个宽容的对手——会漏买好地、建房保守。新玩家在前几局里学会"掷骰—买地—收租"循环并赢下来,建立信心(服务 concept Onboarding「首局可开提示」)。
- **普通档(对手):** 一场真正的较量——有来有回,你大意了会输。
- **精明档(劲敌):** 一个会惩罚失误的劲敌——卡现金缓冲、优先集垄断、果断建房。战胜它,赢得有分量(服务 concept「战胜更高难度 AI」的精通需求)。

**反幻想(必须避免):** AI "犯蠢"——做出任何人类一眼看出是错的蠢事(放着能集的垄断组不买、现金充裕却不建房、为一块烂地抵押掉黄金地段),会瞬间戳破"对手活着"的幻觉,这是 concept 点名的头号技术风险("AI 做得太蠢会破坏单机体验")。同样,精明档**不该靠作弊**(偷看牌、改骰子)制造难度——那违反支柱①忠实,且玩家能察觉。

**enable-not-own:** "对手活过来了"那一刻的**戏剧呈现**——AI 思考的停顿节奏、买地建房的动画、得意/挣扎的表情音效——本系统**使能但不拥有**(归 HUD16 `OnAIActionExecuted` / VFX19 / 音频22);本系统供的是让那些瞬间**有据可演的决策质量**:AI 真的做出了精明的选择,呈现层才有"它在跟我斗"可演。

> **〔MVP 范围诚实标注〕** MVP 的 AI 是**启发式、单机、主动方**:三档全靠加权打分(无前瞻),面向 Pass'N Play + 本地对战(2–4 人)。**"像人不犯蠢"是一个可证伪的体验目标**,非已证事实——原型期 playtest 须量化记录"玩家是否察觉到 AI 蠢招"(如每局明显失误数)与"三档是否产生可感知差异"(player-turn OQ-6 契约),不达标则调权重/补规则。AI 作为**被交易/拍卖的响应方**(他人对 AI 发起提案)、博弈机制(命运之轮/俄罗斯轮盘/策略卡)的使用决策,全在 Alpha(对齐 concept 分层),MVP 不含。

## Detailed Design

### Core Rules

**CR-1 决策边界(被动、return-only、无环).** AI **不自驱**——它只在玩家回合(2)CR-8 在决策点**同步**调用其 hook 时,读入 `GameStateSnapshot`、返回确定性决策,然后立即返回控制权给回合2。AI **从不写游戏状态**(买地/建房/抵押的执行归经济5/地产6/建房8,经回合2 应用);它只回答"做什么",不"做"。**无环纪律:** 10 被 2 调用、只读 1/3/5/6/8 的估值事实,**绝不被 5/6/8 反调、绝不回调 2**。

**CR-2 MVP 决策点(三 hook,签名归 player-turn CR-8).** AI 填充 player-turn CR-8 声明的以下 hook 的**内部逻辑**(签名是 player-turn 拥有的契约,本系统不改):
1. **`bool DecideBuyProperty(snapshot, PropertyData)`**(ResolvePhase 落在无主可购地):买 / 不买。
2. **`TArray<FTurnAction> DecidePostRollActions(snapshot)`**(PostRollAction 窗口):一组有序的**自愿**动作(建房 / 抵押筹现 / 赎回),`[]` = 本回合不做任何额外动作。
3. **`EJailAction DecideJailAction(snapshot)`**(JailTurn):`PayBail` / `UseCard`(持出狱卡时)/ `RollDouble`。
- **不属于 AI 决策的:** **强制清算**(Cash<欠款时的抵押/卖房)是**系统确定性驱动**(回合2 CR-3.3 + 建房8 `ForcedSellNextBuilding` 自动选最高档最小 TileIndex + 经济5 CR-7.3 抵押顺序),**人类与 AI 一致、无选择 hook**——AI 不"挣扎选择"卖什么,系统按固定规则代偿。**Alpha 开口(MVP 不触发,仅标契约避免返工):** `DecideAuctionBid`(拍卖12)/ `DecideTradeResponse`(被动交易11)/ 命运之轮13·俄罗斯轮盘14·策略卡15 的使用决策。

**CR-3 启发式决策模型(加权打分、无前瞻).** 每个 hook 内部走统一管线:**① 枚举候选**(买/不买;可建的格;可抵押的地;出狱三选项)→ **② 对每个候选算效用分**(`Score`,见 Formulas F-1..F-4,消费 rent/垄断/净值等估值事实)→ **③ 按难度策略调整**(权重 + 现金缓冲约束 + 随机扰动,CR-4)→ **④ 选择**(买地/出狱:阈值判定;PostRollAction:贪心选所有正收益且不破现金缓冲的动作,排序后返回)。**全部无博弈树前瞻**(MVP 上限,user 裁定)——"精明"来自更好的权重与更严的现金纪律,不来自模拟对手。

**CR-4 三档难度(`AIDifficulty`,字段归 player-turn2;本系统定义其行为语义).** 三档**共用同一套打分管线**,差异**仅在参数**,产生玩家**可感知**差异(player-turn OQ-6 契约,继承义务⑤):

| 维度 | 休闲 Casual | 普通 Normal | 精明 Sharp |
|---|---|---|---|
| 估值精度(权重) | 粗略(低估垄断价值) | 标准 | 精确(高权重给垄断补全/卡位) |
| 现金缓冲阈值 | 低/松(易现金见底) | 中 | 高/严(留缓冲防停租破产) |
| 随机扰动幅度 | 大(常做次优/漏买="人性化失误") | 中 | 小(接近确定性最优启发) |
| 建房积极性 | 保守(晚建/少建) | 标准 | 果断(集组即尽早均衡建房) |

三档**均不作弊**(CR-5):差异是估值与纪律,不是信息优势。具体数值见 Formulas / Tuning。

**CR-5 决策纯度纪律(确定性 + RNG 源 + 反作弊).** ① **RNG 源单一:** AI 内部一切随机(扰动、平分时的 tiebreak)**只走骰子(3)的可种子化 `RandomRange`/`RandomFloat01`**,**禁引擎全局随机**(`FMath::Rand` 等)——确定性重放,Full Vision 联网服务器权威(dice OQ-T-Dice-2,继承义务②)。② **决策纯函数式:** 同一 `snapshot` + 同一 RNG 流状态 → 同一决策,AI 不持跨调用隐藏状态(无记忆、无学习)。**单次 hook 调用内的局部累积变量(F-5 `MortgagedThisTurn`、F-4 贪心循环的已建集合)是调用栈内数据流、非跨调用状态——纯函数性指 `(snapshot, RNG 流状态) → 决策` 映射确定,不排斥调用内局部可变量;实现上这些变量必须是 hook 内局部、严禁提升为成员 `UPROPERTY`(否则跨调用泄漏,systems R-review B-2 / unreal R-2)。** ③ **反作弊忠实(支柱①):** AI 只用 snapshot 中"该信息态下一个人类玩家能看到的"信息——不预读骰子结果、不偷看牌堆顺序、不读对手隐藏信息。④ **权威 RNG 流隔离(重放确定性,ai-programmer R-review B-3):** 骰子3 的权威可种子化 `FRandomStream` **只被 gameplay-determining 系统消费**(骰子掷骰 / AI 扰动 / Alpha 博弈机制);**呈现层随机(VFX19/HUD16 juice、装饰性抖动)严禁消费此权威流**,必须走独立非权威流——否则呈现层会"偷偷推进游标",使后续 AI 扰动错位、破坏重放。〔propagate 债,同 PR:回链 dice OQ-T-Dice-2 + 登记 VFX(19) GDD 接收义务"juice 随机走独立流"。〕

**CR-6 GameStateSnapshot 消费契约(只读,继承义务③).** AI 各 hook 只读 `GameStateSnapshot`(回合2 在该决策阶段生成的只读跨系统聚合视图;UE 类型/字段构成由架构期 OQ-1 ADR 定义,AI 不拥有)。本系统据 player-turn per-hook 字段充分性判据,声明各 hook **实际所需字段**(供 OQ-1 ADR 注入方对照清单 + 继承义务③ 字段齐备性验证):
> **全盘暴露视图(三 hook 共需,systems/economy/unreal R-review B-1 修复):** F-1 `Buffer` 现金门被**全部三个** hook 消费(买地 F-3 现金门、建房/赎回 F-4/F-5b 现金门、出狱 F-6 步骤2),而 `Buffer` 的输入 `MaxRentExposure` 需要**全盘对手未抵押地产的单次租金前两高 `Rent_top1`/`Rent_top2`**。故下列每个 hook 字段清单均含此项;此前漏列导致 CI-stub AC-5/6/7 构造的 snapshot 算不出 Buffer("标记事实却无载体"失效模式)。注入方须按 piecewise `property_rent` 口径预派生或提供全盘 per-tile `{owner, house_count, is_mortgaged, ColorGroup}` 供 AI 自求 top-2。

- `DecideBuyProperty`:自身 Cash、地产 PurchasePrice、地产 ColorGroup、**AI 已持该组地数 / 该组总格数**(垄断进度,派生)、该组各格归属、**`Rent_top1`/`Rent_top2`(全盘暴露视图,算 F-1 Buffer)**。
- `DecidePostRollActions`:自身 Cash、各持有地的 `house_count`/`is_mortgaged`/`is_monopoly`、BuildingCost、MortgageValue、`unmortgage_cost`、均衡建造约束所需的组内 house_count、**`preaggregated_nlv`(净可变现值,回合2 生成 snapshot 时已聚合 6/8——AI 不可自调经济 F-9,防 5→8 反向环,economy-designer R-review Risk-3)**、**`Rent_top1`/`Rent_top2`(全盘暴露视图,算 F-1 Buffer)**。
- `DecideJailAction`:自身 Cash、`JAIL_BAIL_AMOUNT`、**是否持出狱卡(归事件格7)**、`JailTurnsServed`、**全盘 owner map + `board_tile_count_classic`(算 F-6 `BoardDensity`,systems R-review R-2 修复)**、**`Rent_top1`/`Rent_top2`(算 F-1 Buffer,F-6 步骤2 现金门所需)**、(可选)棋盘上对自己有利/不利的停租态势。
- **字段缺失=AI 开工硬阻塞**(player-turn 已声明 snapshot 是 AI(10) 开工硬前提);缺字段则该 hook 降保守默认(不买 / `[]` / 留狱)+ dev 诊断。
- **〔propagate 债,同 PR〕** 本清单(尤其新增的全盘暴露视图 + owner map)须向架构期 OQ-1 GameStateSnapshot ADR 注入方对照同步(OQ-AI-3 已列 CR-6 为 ADR 注入基准)。

**CR-7 执行期失败安全网(依赖 player-turn 四 hook 降级契约).** AI 力求只返回**决策时刻可行**的动作,但因 `DecidePostRollActions` 是 T0 快照批处理、动作依序执行会改状态,后续动作可能执行期不可行。**可行性校验与降级归对应规则系统 + player-turn 框架**(经济5/地产6/建房8 执行时重校,不可行则静默跳过/视同不买/降留狱 + dev 日志,不崩不中断——player-turn CR-8 四 hook 失败语义簇);**本系统的责任是 minimize 不可行返回**(如 PostRollAction 内多笔动作时,AI 须按"先抵押后建房"等自洽顺序排列、并在算现金缓冲时预扣前序动作的现金影响),不依赖安全网兜常规情形。**MVP 单线程漂移边界(systems R-review R-1):** AI 正确预扣前序动作现金后,单线程 AI 回合内**不应有合法漂移**(无并发、无他人插入动作),故"静默跳过"在 MVP 应为**零常规发生**——日常出现非零跳过 = AI 预扣逻辑 bug 或 F-4 均衡副本 stale(AC-63 守护),非正常路径;安全网主要为 Alpha 并发/被动交易插入预留。

### States and Transitions

AI **无跨调用持久状态机**——每次 hook 调用是对当前 snapshot 的**无状态求值**(CR-5②)。唯一的"配置态"是 `AIDifficulty`(归 player-turn PlayerState),唯一的"流"是骰子(3)的 RNG 流(归骰子3)。单次 hook 调用内的**决策管线**(非持久状态,纯数据流):

| 阶段 | 输入 | 处理 | 输出 |
|---|---|---|---|
| 枚举 | snapshot | 列出合法候选(CR-3①) | 候选集 |
| 打分 | 候选集 + snapshot | 每候选算 `Score`(F-1..F-4) | 候选→分 |
| 难度调整 | 候选→分 + AIDifficulty | 应用权重/缓冲/扰动(CR-4) | 调整后分 |
| 选择 | 调整后分 | 阈值/贪心选择(CR-3④) | 决策(bool / TArray / EJailAction) |

管线**无副作用**(不写状态),可重入安全(同步单帧,player-turn 保证不并发调用同一 AI)。

### Interactions with Other Systems

| 系统 | 数据流 / 接口 | 方向 | 谁拥有 |
|---|---|---|---|
| 玩家回合(2) | 被 CR-8 同步调 `DecideBuyProperty`/`DecidePostRollActions`/`DecideJailAction`;返回决策;回合2 执行并据返回推进(**10 return-only,不回调2**) | 2→10 调用 / 10 返回 | 2 拥 hook 签名/调用协议/流程;10 拥决策逻辑 |
| 骰子(3) | 读可种子化 RNG `RandomRange`/`RandomFloat01`(AI 扰动/tiebreak 唯一随机源) | 10→3(读) | 3 拥 RNG;10 消费 |
| 经济(5) | 读估值事实:`property_rent`/`railroad_rent`/`utility_rent`、`net_liquidation_value`、`unmortgage_cost`、`building_sellback`、`starting_cash`(经 snapshot) | 10→5(读) | 5 拥公式/数值;10 只读估值 |
| 地产(6) | 读 `is_monopoly`/`station_count`/`utility_count`/owner map/`is_mortgaged`(经 snapshot,垄断进度估值) | 10→6(读) | 6 拥归属事实;10 只读 |
| 建房(8) | 读 `house_count`/BuildingCost/均衡建造约束(经 snapshot,建房决策) | 10→8(读) | 8 拥建筑态;10 只读 |
| 棋盘(1) | 读 PurchasePrice/MortgageValue/ColorGroup/TileType(经 snapshot,地产估值) | 10→1(读) | 1 拥棋盘数据;10 只读 |
| 事件格(7) | 读出狱卡持有态(经 snapshot,`DecideJailAction` 判 `UseCard` 可行) | 10→7(读) | 7 拥牌/监狱规则;10 只读 |
| HUD(16)/VFX(19)/音频(22) | **间接**:AI 返回的动作由回合2 执行时广播 `OnAIActionExecuted(ActionIndex)`,呈现层据此非阻塞展示(enable-not-own;**AI 不直接 emit 事件**) | (2→下游,非 10) | 16/19/22 拥呈现;10 只供决策质量 |
| 俄罗斯轮盘14·命运之轮13·拍卖12·交易11·策略卡15(Alpha) | Alpha hook(`DecideAuctionBid`/`DecideTradeResponse`/机制使用决策),MVP 不触发 | (Alpha) | 各机制拥规则;10 将拥其决策逻辑 |

**无环纪律:** 10 对 2 **被调用 + return-only**(绝不回调 2);对 1/3/5/6/7/8 **只读**(经 snapshot,绝不反向驱动);不被任何 MVP 系统反调。AI 是依赖图的**纯叶子消费者**。

## Formulas

> AI 只读注册表已批准的估值事实(`property_rent`/`is_monopoly`/`net_liquidation_value`/`unmortgage_cost`/`building_sellback`/`JAIL_BAIL_AMOUNT`/`starting_cash` 等),**不重定义**。一切随机走骰子(3)可种子化 `RandomRange`/`RandomFloat01`(CR-5)。浮点参数(B_frac/U_frac/DensityThreshold)建议存定点 int32(×100)、计算即时 floor,保跨平台确定性(Blueprint)。本地编号 F-1..F-7。

### F-1 Cash Buffer(现金储备门)

任何主动花费动作(买/建/赎回)只在"花完后 Cash ≥ Buffer"时执行。

`Buffer = clamp(floor(B_frac × MaxRentExposure), Buffer_min, Buffer_max)`
`MaxRentExposure = Rent_top1 + Rent_top2`(全盘对手**未抵押**地产当前单次租金的**前两高之和**,piecewise 用注册表 `property_rent` 口径——垄断×2 仅作用无房 base;不足两块时取现有之和,无对手地产=0)

> **为何 top-2 而非单一最高(economy R-review B-4 修复):** 单次最高租金只防"踩一家酒店"的最坏单点;但 AI 一圈可能连踩多家。top-2 之和近似"一圈踩到两家高租"的**累计停租风险**,防"砸光现金建房后连踩两家破产"。完整整圈期望暴露模型留 Alpha(OQ-AI-8)。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `B_frac` | 定点 | 0.3–2.0 | 难度系数(F-7) |
| `Rent_top1`/`Rent_top2` | int32 | [0,2000] | 全盘对手未抵押地产单次租金前两高(piecewise 注册表口径) |
| `MaxRentExposure` | int32 | [0,4000] | top-2 之和;开局/全抵押=0 |
| `Buffer_min` | int32 | [50,400] | 绝对下限(≥保释金50) |
| `Buffer_max` | int32 | [600,2200] | 防过度保守上限(抬高以免中后期 clamp 钉死、丧失 B_frac 档位语义) |
| `Buffer` | int32 | [Buffer_min,Buffer_max] | AI 目标最低持有现金 |

**Output Range:** [Buffer_min, Buffer_max],永不负。**退化守卫:** 开局/全抵押 MaxRentExposure=0 → Buffer=Buffer_min;仅一块对手地产 → Rent_top2=0、MaxRentExposure=Rent_top1。**随局推进自然放大**(中后期酒店租高→Buffer 抬高)。**Buffer_max 上界抬至 [600,2200] 后,Sharp(B_frac=1.5)遇中后期酒店租不再被 clamp 钉死,三档 B_frac 语义在全程保持可分(economy R-review B-3 修复)。**
**Example(Normal,B_frac=1.0,Buffer_max=1400):** 对手两高未抵押租金=橙组酒店租 1050 + 红组带房租 550 → MaxRentExposure=1600 → `Buffer = clamp(floor(1.0×1600),150,1400) = 1400`。

### F-2 Buy Score(买地效用,4 项)

`DecideBuyProperty` 枚举阶段对候选地打分。

`BuyScore = W_rent×RentPotential + W_mono×MonopolyProgress + W_block×BlockingValue + W_nlv×LiquidationDiscount`

- `RentPotential = clamp(floor(base_rent(¬抵押,¬垄断,house=0,RentTable) × 100 / PurchasePrice), 0, 10)` — **原始(非垄断)租金回报率**(≈百分比收益,归一到 [0,10]);**用非垄断 base 租金(去 ×2)**——垄断价值唯一由 MonopolyProgress 表达,**消除 RentPotential 与 MonopolyProgress 对垄断价值的双重计入**(economy R-review B-1/B-2 修复)
- `MonopolyProgress = floor((HeldInGroup+1)² × 10 / GroupSize²)` — 二次曲线,补全垄断那格满分(0/1/2 格买第1/2/3 格得分比≈1:4:9),范围 [1,10]
- `BlockingValue`(Normal+Sharp;Casual `W_block=0`):令 `OppMaxInGroup = max_{对手o} |o 在该组持有格数|` —— 若 `OppMaxInGroup==GroupSize−1 ∧ owner(tile)==INDEX_NONE` → **10**(抢走对手补全垄断的最后一块);`OppMaxInGroup≥1` → `OppMaxInGroup×3`(clamp ≤10);否则 0
- `LiquidationDiscount = clamp(floor((Cash−PurchasePrice−Buffer)×10/starting_cash), −10, 10)` — 买后现金充裕度修正

> **四项量纲已归一(economy R-review B-1 修复):** RentPotential/MonopolyProgress/BlockingValue 均 [0,10]、LiquidationDiscount [−10,10] —— 四项**同尺度**,故 F-7 的 `W_*` 权重才真正控制各项占比,不再出现旧式 MonopolyProgress 占 ~86% 而 RentPotential 名存实亡。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `RentPotential` | int32 | [0,10] | 非垄断 base 租金回报率(归一) |
| `HeldInGroup`/`GroupSize` | int32 | [0,3]/[2,3] | AI 持该组格数/该组总格数(棋盘1) |
| `MonopolyProgress` | int32 | [1,10] | 垄断推进分(唯一垄断价值表达) |
| `OppMaxInGroup` | int32 | [0,3] | 单一对手在该组最大持有数 |
| `BlockingValue` | int32 | [0,10] | 阻断对手垄断分 |
| `LiquidationDiscount` | int32 | [−10,10] | 现金充裕修正 |
| `W_rent`/`W_mono`/`W_block`/`W_nlv` | int32 | 见 F-7 | 四项权重(难度相关) |
| `BuyScore` | int32 | 约[−40,+130] | 综合效用 |

**Output Range:** 不 clamp,实际约 [−40,+130]。**退化守卫:** `PurchasePrice<1→RentPotential=0`;`GroupSize<1→MonopolyProgress=1`(空集守门,对称 building F-1)。
**Example(Normal:W_rent=2/W_mono=3/W_block=2/W_nlv=2;橙组中间格 PurchasePrice=180、base 租14、HeldInGroup=1、GroupSize=3、无对手持该组、Cash=900、Buffer=600):** RentPotential=clamp(floor(14×100/180),0,10)=7;MonopolyProgress=floor(40/9)=4;BlockingValue=0(无对手持组);LiquidationDiscount=clamp(floor((900−180−600)×10/1500),−10,10)=0。`BuyScore = 2×7+3×4+2×0+2×0 = 26`(RentPotential 占 54% / MonopolyProgress 占 46%——量纲可比,对比旧式 MonopolyProgress 占 ~86%)。

### F-3 Buy Decision(买地决策)

`FinalScore = BuyScore + RNG.RandRange(−Epsilon,+Epsilon)`(seedable,走骰子3)
`DecideBuy = (FinalScore ≥ BuyThreshold) ∧ (Cash − PurchasePrice ≥ Buffer)`

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `Epsilon` | int32 | [0,20] | 扰动幅度(F-7);Sharp=0 最优、Casual=15 人性化漏买 |
| `BuyThreshold` | int32 | [3,25] | 决策门槛(F-7) |
| `DecideBuy` | bool | {T,F} | 最终决定 |

**双门:** ① 效用门(受扰动)② 硬现金门(不受扰动)。两条同时满足才买。**Output Range:** bool。**退化守卫:** `Cash<PurchasePrice` → 现金门直接 false,不进评分。
**Example(Normal:Epsilon=7/BuyThreshold=10;承 F-2 BuyScore=26,Buffer=600 早期态):** Perturbation=−3(示例种子)→ FinalScore=23 ≥ 10 ✓;Cash−Price=720 ≥ Buffer=600 ✓ → `DecideBuy=true`。

### F-4 Build Priority(`DecidePostRollActions` 建房)

对每个 `CanBuildHouse(tile,AI)==true`(谓词归建房8、含均衡约束)的格打分:

`BuildScore = W_build_rent×RentGain + W_build_mono×CompletionBonus`
`RentGain = property_rent(¬抵押,is_monopoly,house+1,…) − property_rent(¬抵押,is_monopoly,house,…)`(两次查注册表 `property_rent` 正确 piecewise——垄断×2 只在 house=0)
`CompletionBonus = (建完此栋后全组同档) ? 50 : 0`

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `house_count[tile]` | int32 | [0,4] | 当前档(CanBuild 保证<5) |
| `RentGain` | int32 | [0,800] | 单栋租金增量 |
| `CompletionBonus` | int32 | {0,50} | 全组齐档奖励 |
| `W_build_rent`/`W_build_mono` | int32 | 见 F-7 | 权重 |
| `BuildScore` | int32 | [0,~2500] | 建房优先级 |

**贪心循环:** `while ∃ CanBuildHouse 候选: best=argmax BuildScore; if Cash−BuildingCost(best)<Buffer: break; else BuildHouse(best)`。
**解锁建房的前置赎回(ai-programmer R-review R-3,防整组瘫痪蠢招):** 若某垄断组有格被抵押致 `CanBuildHouse==false`、而赎回它即可解锁整组建房,贪心候选集**纳入"赎回该格"作为前置动作**(经 F-5b 现金门评估),排序保证"赎回→建房"同窗级联——防"垄断组单格抵押致整组瘫痪、现金充裕却不建房"(与 AC-58 协同;此为单 hook 内可达的跨动作协调,非 OQ-AI-9 的跨 hook 融资盲区)。
**⚠ AI 内部模拟均衡约束(max−min≤1)生成合法建房序列**——不把均衡校验完全甩给执行层兜底(economy R-review Risk-2;呼应 CR-7,minimize 不可行返回;一致性回归守护见 AC-63)。**Output Range:** [0,~2500]。
**Example(Normal:W_build_rent=2/W_build_mono=1;橙组 house=[1,1,1]、目标格 RentTable=[14,10,30,90,160,250]):** RentGain=RentTable[2]−RentTable[1]=30−10=20;建后[2,1,1] 非齐档→CompletionBonus=0。`BuildScore = 2×20+1×0 = 40`。

### F-5 Mortgage / Unmortgage(反 churn)

**F-5a 主动抵押** `ShouldMortgage(tile)`:`¬is_monopoly(组)` ∧ `¬is_mortgaged` ∧ `house_count==0` ∧ `Cash<Buffer`(现金不足才抵押,不为囤现金而押)∧ `MortgageValue>0`。优先押 MortgageValue 最高者;入 `MortgagedThisTurn` 集合。
> **〔R-review 修复〕** 原条件含悬空未定义符号 `expected_roi`(全档无定义)→ 已删除该子句(user 裁定"删悬空子句")。抵押动机已由 `Cash<Buffer` 充分表达(现金不足才押),不需 ROI 预测;过度抵押由 churn 守卫(`MortgagedThisTurn` + 仅垄断组赎回 F-5b)与"≥10% 往返费恒亏"经济纪律拦截。原 `PAYBACK_THRESHOLD` 旋钮仅服务于已删子句,一并移除。
**F-5b 主动赎回** `ShouldUnmortgage(tile)`:`is_mortgaged` ∧ `tile∉MortgagedThisTurn`(防 churn)∧ `is_monopoly(组)`(只赎回垄断组——立得建房权+×2)∧ `Cash − unmortgage_cost ≥ Buffer×U_frac`。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `unmortgage_cost` | int32 | 注册表 F-6 | =MortgageValue+ceil(MV/10) |
| `U_frac` | 定点 | [1.0,2.0] | 赎回安全系数(F-7) |
| `MortgagedThisTurn` | Set | — | 本回合已抵押格(防 churn) |

**Output Range:** 两谓词均 bool。**经济纪律(economy R-review):** 抵押→赎回往返恒亏 ≥10% fee(AC-42 证伪),故 churn 守卫硬性;不为"囤现金"抵押。**退化守卫:** Cash=0 → `Cash<Buffer` 但强制清算归 CR-3.3 接管,AI 主动路径退出;全抵押→赎回无候选→空动作。
**Example(Normal,U_frac=1.1):** 橙组抵押格 MV=90 → unmortgage_cost=90+9=99;is_monopoly ✓、∉MortgagedThisTurn ✓;Cash=800,Buffer=600 → 800−99=701 ≥ 660 ✓ → 赎回。

### F-6 Jail Decision(`DecideJailAction`,确定性无随机)

`BoardDensity = count{t | owner(t)≠INDEX_NONE} / board_tile_count_classic`;`EarlyGame = (BoardDensity < DensityThreshold)`
短路优先级:
1. `bHasJailCard` → `UseCard`(零现金损失,卡优先)
2. `EarlyGame ∧ Cash ≥ JAIL_BAIL_AMOUNT + Buffer` → `PayBail`(早期买地机会成本高)
3. `¬EarlyGame` → `RollDouble`(晚期坐牢避高租是庇护,先赌免费出狱)
4. (早期但没钱付保释)→ `RollDouble`

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `bHasJailCard` | bool | — | 持出狱卡(事件格7供,snapshot 读) |
| `BoardDensity` | 定点 | [0,1] | 已购格数/总格数 |
| `DensityThreshold` | 定点 | [0.15,0.6] | 早/晚期分界(F-7) |
| `EJailAction` | enum | {PayBail,UseCard,RollDouble} | 返回值 |

**Output Range:** EJailAction 之一。第3回合(`JailTurnsServed==MAX_JAIL_TURNS−1`)强制付保释由事件格7规则驱动,非 AI 决策。**退化守卫:** 持卡但快照实际无卡(数据错)→ 执行期校验归事件格7、降 `RollDouble`(player-turn CR-8 #4 降级)。
**Example(Normal,BoardDensity=0.2,DensityThreshold=0.3,Cash=400,Buffer=300):** 无卡;EarlyGame=(0.2<0.3)=true;400 ≥ 50+300=350 ✓ → `PayBail`。

### F-7 难度参数表(source of truth;Tuning Knobs 引用本表)

三档共用决策管线,**仅参数不同**(起始 playtest 基线,标安全范围):

| 参数 | Casual | Normal | Sharp | 安全范围 |
|---|---|---|---|---|
| `B_frac`(F-1) | 0.6 | 1.0 | 1.5 | 0.3–2.0 |
| `Buffer_min`(F-1) | 50 | 150 | 250 | 50–400 |
| `Buffer_max`(F-1) | 700 | 1400 | 1800 | 600–2200 |
| `BuyThreshold`(F-3) | 5 | 10 | 15 | 3–25 |
| `Epsilon`(F-3) | 15 | 7 | 0 | 0–20 |
| `W_rent`(F-2) | 1 | 2 | 3 | 1–5 |
| `W_mono`(F-2) | 2 | 3 | 4 | 1–6 |
| `W_block`(F-2) | **0** | 2 | 3 | 0–4 |
| `W_nlv`(F-2) | 1 | 2 | 3 | 1–4 |
| `W_build_rent`(F-4) | 1 | 2 | 3 | 1–4 |
| `W_build_mono`(F-4) | 1 | 1 | 2 | 1–3 |
| `U_frac`(F-5) | 1.5 | 1.1 | 1.0 | 1.0–2.0 |
| `DensityThreshold`(F-6) | 0.4 | 0.3 | 0.2 | 0.15–0.6 |

**玩家可感知差异(OQ-6 契约,继承义务⑤):** Casual=大扰动漏买 + `W_block=0` 不卡位 + 松缓冲(常莫名破产)+ 地光秃秃;Sharp=零扰动最优 + 抢垄断最后一块+卡位 + 严缓冲 + 集组即建到3房甜点。**三档均不作弊**——差异是估值精度与纪律,非信息优势(支柱①忠实)。
*(utility 估值用 `dice_total` 期望=7:持1公用期望租≈28、持2≈70,AI 不低估车站/公用——economy R-review Risk-4。)*

## Edge Cases

- **开局空棋盘(无任何对手地产):** `MaxRentExposure=0` → `Buffer=Buffer_min`(F-1);`BlockingValue=0`(F-2,无对手持组)。AI 据 BuyScore + 宽松现金门**积极买地**(经典"早期见地就买"land-grab,economy #1 启发式)。
- **现金近零:** `Cash<PurchasePrice` → 买地现金门直接 false(F-3),不评分;`Cash−BuildingCost<Buffer` → 建房贪心循环 break(F-4)。**若 Cash<欠款(强制扣款)→ 非 AI 决策**:强制清算由回合2 CR-3.3 + 建房8/经济5 确定性驱动接管(CR-2)。
- **车站/公用地产买地估值(F-2 口径切换):** F-2 `RentPotential` 默认用 `property_rent`(色地)。**落在 Railroad 格** → `RentPotential` 改用 `railroad_rent(station_count+1)`、`MonopolyProgress/BlockingValue` 按"已持车站数/4"计(车站是跨色独立组);**落在 Utility 格** → 用 `utility_rent(dice_total=7 期望, utility_count+1)`、按"已持公用数/2"计。AI 不低估车站(稳定档位收益,economy Risk-4)。
- **无合法建房动作(无垄断组 / 均衡已达上限 / 全组已酒店):** `CanBuildHouse` 候选集空 → `DecidePostRollActions` 该类动作产出空,连同抵押/赎回一并评估;全空 → 返回 `[]`(AI 本回合不做额外动作,框架直接 EndTurn,player-turn AC-37d)。
- **打分平分(两候选 BuyScore/BuildScore 相等):** 确定性 tiebreak = **TileIndex 升序**取最小(不消耗 RNG,保重放确定性);仅当设计需要"看起来更随机"时才用骰子3 RNG 扰动,默认走 TileIndex(CR-5② 纯函数式)。
- **snapshot 字段缺失(注入方未提供 AI 所需字段):** 该 hook 降**保守默认**(`DecideBuyProperty→false` / `DecidePostRollActions→[]` / `DecideJailAction→RollDouble 或留狱`)+ `UE_LOG` dev 诊断。字段齐备是 AI 开工硬前提(CR-6 / 继承义务③),缺字段是架构期 OQ-1 ADR 注入缺陷,非 AI 逻辑错。
- **DecideBuyProperty 返回 true 但执行期不可行**(并发/状态漂移致现金不足或地已被买,MVP 单线程罕见):可行性校验归经济5/地产6,**视同 false(不买)**+ dev 日志,不扣成负现金(player-turn CR-8 #1 降级,CR-7)。
- **DecidePostRollActions 动作执行期不可行**(前序动作改了状态,如先抵押改现金后建房不够):回合2 框架**逐动作重校、不可行静默跳过、继续后续、不中断**(player-turn CR-8 批处理失败策略)。AI 已尽量按自洽顺序排列并预扣现金(CR-7),把跳过当兜底而非常规。
- **DecideJailAction 返回 UseCard 但实际无出狱卡**(快照与真实态不一致):可行性校验归事件格7 → **降级 RollDouble** + dev 诊断(player-turn CR-8 #4);不凭空消卡。
- **多 AI 局(2 个以上 AI 玩家):** 每个 AI 独立走同一管线评估自己的 snapshot,**无跨 AI 协调/串谋**(纯叶子消费者,各自 return-only);"两个 AI 联手压制人类"不是设计目标也不实现(MVP 无结盟,对齐 concept 结盟是 Alpha 涌现行为)。
- **抵押→赎回同回合空转风险:** `MortgagedThisTurn` 集合在 `DecidePostRollActions` 内拦截——本回合抵押的格不进赎回候选(F-5,反 churn,净亏 10% fee 的退化被硬挡)。
- **AI 在自己回合内破产**(建房/抵押路径走到 insolvent,或停租付不起):破产编排归破产9(`ResolveBankruptcy`),**AI 不参与自己的破产清算决策**(强制清算系统驱动);AI 出局后不再被调用任何 hook(回合2 turn order 跳过 `bIsBankrupt`)。
- **决策耗时超单帧预算(慢但非挂死):** AI 同步调用返回后,回合2 做事后帧预算诊断(`ai_turn_watchdog`,dev-only `UE_LOG`),不强制 EndTurn(已正常返回);"永不返回"死循环靠测试框架进程级超时暴露(player-turn dev 兜底,非帧内机制)。

## Dependencies

**上游(本系统 depends-on):**

| 系统 | 硬/软 | 接口 / 数据 | 方向 |
|---|---|---|---|
| 玩家回合(2) | 硬(编排) | 被 CR-8 同步调三 hook(`DecideBuyProperty`/`DecidePostRollActions`/`DecideJailAction`)+ 提供 `GameStateSnapshot`;AI 返回决策、**return-only 不回调2**(类比 9↔2 编排边,非硬循环) | 2→10 调用 / 10 返回 |
| 骰子(3) | 硬 | AI 一切随机的唯一源:可种子化 `RandomRange`/`RandomFloat01`(F-3 扰动等;确定性重放) | 10→3(读) |
| 经济(5) | 硬 | 估值事实(经 snapshot):`property_rent`/`railroad_rent`/`utility_rent`/`unmortgage_cost`/`building_sellback`/`preaggregated_nlv`/`starting_cash` | 10→5(经 snapshot 读) |
| 地产所有权(6) | 硬 | `is_monopoly`/`station_count`/`utility_count`/owner map/`is_mortgaged`(经 snapshot,垄断进度+卡位估值) | 10→6(经 snapshot 读) |
| 建房(8) | 硬 | `house_count`/`CanBuildHouse` 谓词/BuildingCost/均衡约束(经 snapshot,F-4 建房决策) | 10→8(经 snapshot 读) |
| 棋盘(1) | 硬(数据) | PurchasePrice/MortgageValue/ColorGroup/TileType/`board_tile_count_classic`(经 snapshot,地产估值底数) | 10→1(经 snapshot 读) |
| 事件格(7) | 硬 | 出狱卡持有态 + `JAIL_BAIL_AMOUNT`/`MAX_JAIL_TURNS`(经 snapshot,`DecideJailAction`) | 10→7(经 snapshot 读) |

**下游(depend-on 本系统):** **无(MVP)。** AI 是依赖图的**纯叶子消费者**——没有任何 MVP 系统依赖 AI 的输出。HUD16/VFX19/音频22 消费的 `OnAIActionExecuted` 由**回合2 执行 AI 动作时广播**(非 AI 发),故它们依赖 2 不依赖 10。**Alpha:** 拍卖12/交易11/命运之轮13/俄罗斯轮盘14/策略卡15 将各获一个 AI 决策 hook(`DecideAuctionBid`/`DecideTradeResponse`/机制使用)——届时 AI depends-on 它们的状态,仍非它们 depend-on AI。

**⚠ 双向一致性核查(揪出 systems-index #10 依赖列失真,Phase 5 订正):**
- systems-index #10 当前列 `depends-on 3,4,5,6,7,8`。本设计实际依赖 = **1,2,3,5,6,7,8**。差异:
  - **缺 1(棋盘)** — AI 估值底数(price/group/type)源自棋盘,经 snapshot 读 → **应补 1**。
  - **缺 2(玩家回合)** — AI 被 CR-8 调用 + 取 snapshot,是 AI 存在的前提 → **应补 2**(标 orchestration 边,类比 9↔2/movement4→经济)。
  - **多列 4(移动)** — MVP **无前瞻**启发式 AI **不使用**移动/距离估值(`steps_between` 在 registry 标"预期消费方 AI"是为 Alpha"前进到最近X"类决策预留;MVP 不触发)→ **MVP 应移除 4**(或标 Alpha-only)。
- 处置:Phase 5 订正 systems-index #10 `depends-on` 为 **1,2,3,5,6,7,8**(4 降 Alpha 注);registry `steps_between` 的 AI 消费标注维持(Alpha 预留,不删)。
- 反向核查:player-turn(2)CR-8 + PlayerState(`bIsAI`/`AIDifficulty`)已引 AI(10)✓;registry `property_rent`/`is_monopoly`/`station_count`/`utility_count`/`unmortgage_cost`/`building_sellback`/`net_liquidation_value`/`net_worth`/`JAIL_BAIL_AMOUNT`/`starting_cash` 均已列 AI(10) 为 referenced_by/预期消费方 ✓(双向一致)。

**无环纪律:** 10 是纯叶子——被 2 调用(return-only)、只读 1/3/5/6/7/8(经 snapshot),不被任何系统反调,不回调 2。无环。

## Tuning Knobs

> AI 的可调数值 = F-7 难度参数表(**source of truth 在 Formulas F-7**,本节给极端行为与交互,不重复数值)。AI 几乎不拥有 F-7 之外的旋钮——决策结构(管线、hook、无环)是规则不是数值。

**本系统拥有的旋钮(全部 per-tier,定义见 F-7):**

| 旋钮(F-7) | 影响 | 过高 | 过低 |
|---|---|---|---|
| `B_frac` / `Buffer_min` / `Buffer_max` | 现金纪律(F-1) | 过度保守——钱闲置不买不建(现金贬值,退化C/B) | 现金见底——停租即破产(退化F) |
| `BuyThreshold` | 买地门槛(F-3) | 几乎不买地——land-grab 失败、沦为孤岛旁观者(退化G) | 见地就买不顾后果——现金门虽兜底但买废地 |
| `Epsilon` | 买地扰动(F-3) | AI 像在乱走——失"不犯蠢"(戳破对手活着幻觉) | 三档趋同——失玩家可感知差异(违 OQ-6) |
| `W_mono` / `W_block` | 垄断/卡位权重(F-2) | `W_block` 过高→为卡位倾家荡产;`W_mono` 过高→只追垄断忽略车站现金流 | `W_mono` 过低→不集垄断,纯靠运气(头号退化) |
| `W_rent` / `W_nlv` | 租金率/流动性权重(F-2) | 偏好高租率小地、忽略集组 | 忽略回报率,买贵而无收益的地 |
| `W_build_rent` / `W_build_mono` | 建房积极性(F-4) | 倾尽现金建房后破产(若不配合 Buffer) | 集组后不建房——浪费垄断(退化B) |
| `U_frac` | 赎回保守度(F-5) | 几乎不赎回——垄断组长期抵押收不到×2 | 赎回过激——赎后现金不足 |
| `DensityThreshold` | 早/晚期分界(F-6) | 长期判"晚期"——总坐牢躲租、错过买地 | 长期判"早期"——总付保释、晚期仍乱跑踩酒店 |

**旋钮交互:**
- **`Buffer` 联动棋盘态:** `B_frac × MaxRentExposure`(F-1)——Buffer 随对手建房自动放大,故 `B_frac` 不是绝对值而是"对最坏单次租金的倍数",调它=调风险容忍度。
- **`Epsilon` 与三档差异:** `Epsilon=0`(Sharp)时三档差异全靠权重/缓冲;`Epsilon` 拉大主要制造 Casual 的"人性化失误"。**若把三档 Epsilon 调到相同,权重表必须仍保持差异**否则 OQ-6 可感知差异崩塌。
- **`BuyThreshold` 与 `Buffer` 双门(F-3):** 效用门与现金门独立——调 BuyThreshold 改"想不想买",调 Buffer 改"敢不敢买",二者须协同(高 Threshold + 低 Buffer = 挑剔但敢冒险)。

**引用(真值不归本系统,指向 source of truth):**

| 旋钮/真值 | Source | 本系统关系 |
|---|---|---|
| `AIDifficulty` 字段(None/Casual/Normal/Sharp) | **玩家回合(2) PlayerState** | 本系统定义其行为语义(F-7),不拥有字段 |
| RNG 源(可种子化) | **骰子(3)** | AI 一切随机的唯一源(CR-5) |
| 经济数值(rent/cost/价/费率) | **经济(5)/棋盘(1)** | AI 只读估值,不调这些旋钮 |
| `ai_turn_watchdog`(dev 帧诊断) | **玩家回合(2)** | dev-only 诊断阈值,非 gameplay 旋钮 |
| `ai_action_display_mode`(Animated/Instant) | **HUD(16)** | 呈现旋钮,框架/AI 不读(R6 RETREAT) |

## Visual/Audio Requirements

> **职责边界:enable-not-own。** AI **无自有视觉/音频资产**。AI 行动的呈现(买地/建房/抵押动画、AI"思考"停顿节奏、得意/挣扎表情与音效)归 HUD16/VFX19/音频22,经 player-turn `OnAIActionExecuted(ActionIndex)` 驱动(**AI 不 emit 事件**——回合2 执行 AI 返回的动作时广播)。本系统只供**决策质量**(AI 真做了精明选择,呈现层才有"它在跟我斗"可演)。`ai_action_display_mode`(Animated/Instant)是 HUD 呈现旋钮,框架/AI 不读(R6 RETREAT)。**无 asset-spec 需求**(本系统不产视听资产)。

## UI Requirements

- **难度选择(Casual/Normal/Sharp)UI**:归主菜单/房间大厅(20)——写 `PlayerState.AIDifficulty`(字段归玩家回合2);本系统供三档**行为语义**(F-7),不拥有选择 UI/布局。
- **"AI 思考中"指示 / AI 行动摘要**:归 HUD(16),经 `OnAIActionExecuted` 呈现(本系统供决策、不供呈现)。
- **本系统职责边界**:无自有 UI、无本系统拥有的屏 → **不触发 UX-spec flag**。难度选择屏的 UX 归大厅(20)/HUD(16) 的 UX-spec(届时引用其 GDD,非本档)。

## Acceptance Criteria

> **测试分层:** `[Logic]` = 纯 fixture、headless `-nullrhi`、PR merge gate,`tests/unit/ai-opponent/`;`[Integration]` = 集成测试 OR 文档化 playtest,BLOCKING,`tests/integration/ai-opponent/`;`[Logic·CI-stub]` = 依赖未定义 struct/ADR(架构期 OQ-1 GameStateSnapshot),CI 报 "pending X" 非静默跳过;`[Advisory]` = code-review 证据,`production/qa/evidence/`。每条 AC 独立 pass/fail 可裁定。

### A. 无环纪律 / 纯叶消费者(CR-1,CR-5②③)

- **AC-1** `[Logic]`(CR-1)**GIVEN** 框架对 player-turn/经济5/地产6/建房8 任意**写**方法装 spy,**WHEN** 三 hook 各调用一次返回,**THEN** 写方法调用次数==0(AI 不写任何游戏状态)。
- **AC-2** `[Logic]`(CR-1)**GIVEN** 对 player-turn 装反向调入 spy,**WHEN** 三 hook 各完成一次,**THEN** player-turn 方法调用次数==0(return-only,不回调2)。
- **AC-3** `[Logic]`(CR-1)**GIVEN** 对经济5/地产6/建房8 装"是否主动调 AI"spy,**WHEN** 执行 100 个 AI 决策轮次,**THEN** 次数==0(AI 不被反调,纯叶子)。
- **AC-4** `[Advisory]`(CR-1)代码审查:AI 模块无对 player-turn 任何 public 写方法(`SetCash`/`SetTileIndex`/`SetBankrupt`)的引用或 BP 连线。证据 `production/qa/evidence/ai-no-write-review.md`。

### B. 三 Hook 契约(CR-2,CR-6)

- **AC-5** `[Logic·CI-stub]`(CR-2/CR-6,依赖 OQ-1 ADR)**GIVEN** 含 `DecideBuyProperty` 所需全字段(Cash/PurchasePrice/ColorGroup/HeldInGroup/GroupSize/各格归属)的完整 snapshot,**WHEN** 调用,**THEN** 返回 true/false(不 crash/不 null)、不写外部状态。
- **AC-6** `[Logic·CI-stub]`(CR-2/CR-6,依赖 OQ-1 ADR)**GIVEN** 含 `DecidePostRollActions` 全字段(Cash/house_count/is_mortgaged/is_monopoly/BuildingCost/MortgageValue/unmortgage_cost/组内 house_count/preaggregated_nlv)的 snapshot,**WHEN** 调用,**THEN** 返回 `TArray<FTurnAction>`(动作类型∈{BuildHouse,Mortgage,Unmortgage})、不 crash、不写外部状态。
- **AC-7** `[Logic·CI-stub]`(CR-2/CR-6,依赖 OQ-1 ADR)**GIVEN** 含 `DecideJailAction` 全字段(Cash/JAIL_BAIL_AMOUNT/bHasJailCard/JailTurnsServed/停租态势)的 snapshot,**WHEN** 调用,**THEN** 返回 `EJailAction`∈{PayBail,UseCard,RollDouble}、不 crash、不写外部状态。
- **AC-8** `[Logic]`(CR-2,空数组 noop)**GIVEN** 无垄断组、Cash<Buffer 的 snapshot,**WHEN** 调 `DecidePostRollActions`,**THEN** 返回 `[]`,框架后续 EndTurn 被调、不挂起(验证 player-turn AC-37d 契约)。

### C. 公式边界值(F-1..F-6)

- **AC-9** `[Logic]`(F-1 退化:空棋盘)**GIVEN** MaxRentExposure=0、B_frac=1.0、Buffer_min=150、Buffer_max=1400,**WHEN** 算 Buffer,**THEN** Buffer==150(下界守卫,永不负)。
- **AC-10** `[Logic]`(F-1 正常,top-2)**GIVEN** Rent_top1=1050、Rent_top2=550(MaxRentExposure=1600)、B_frac=1.0、Buffer_max=1400,**WHEN** 算 Buffer,**THEN** Buffer==1400(GDD Example)。
- **AC-10b** `[Logic]`(F-1 top-2 退化:仅一块对手地产)**GIVEN** Rent_top1=400、Rent_top2=0(仅一块对手未抵押地)、B_frac=1.0、Buffer_min=150,**WHEN** 算 Buffer,**THEN** Buffer==400(MaxRentExposure=Rent_top1)。
- **AC-11** `[Logic]`(F-1 上界 clamp)**GIVEN** MaxRentExposure=800、B_frac=3.0、Buffer_max=1800,**WHEN** 算 Buffer,**THEN** Buffer==1800(floor(2400) 夹至上界)。
- **AC-12** `[Logic]`(F-1 永不负)**GIVEN** B_frac=0.3、MaxRentExposure=0、Buffer_min=50,**WHEN** 算 Buffer,**THEN** Buffer>=0(floor(0)→clamp 50)。
- **AC-13** `[Logic]`(F-2 RentPotential 退化)**GIVEN** PurchasePrice=0,**WHEN** 算 RentPotential,**THEN** ==0(不除零 crash)。
- **AC-14** `[Logic]`(F-2 GroupSize<1 退化)**GIVEN** GroupSize=0,**WHEN** 算 MonopolyProgress,**THEN** ==1(空集守门)。
- **AC-15** `[Logic]`(F-2 BlockingValue Normal/Sharp)**GIVEN** OppMaxInGroup=GroupSize−1=2、该格无主,**WHEN** Normal 算 BlockingValue,**THEN** ==10(抢最后一格满分)。
- **AC-16** `[Logic]`(F-2 Casual W_block=0)**GIVEN** 同上 fixture,**WHEN** Casual(W_block=0),**THEN** BlockingValue 贡献==0(不卡位)。
- **AC-17** `[Logic]`(F-2 LiquidationDiscount 上界)**GIVEN** Cash=9999、PurchasePrice=60、Buffer=150、starting_cash=1500,**WHEN** 算,**THEN** ==10(clamp 上界)。
- **AC-18** `[Logic]`(F-2 下界)**GIVEN** Cash=200、PurchasePrice=300、Buffer=150,**WHEN** 算 LiquidationDiscount,**THEN** ==−10(clamp 下界)。
- **AC-19** `[Logic]`(F-2 GDD Example,归一后)**GIVEN** Normal(2/3/2/2)、橙组中间格(180,base14,Held=1,GroupSize=3,无对手,Cash=900,Buffer=600),**WHEN** 算 BuyScore,**THEN** ==26(RentPotential=7/Mono=4/Block=0/Liq=0;RentPotential 占 54%、Mono 占 46%,量纲可比)。
- **AC-20** `[Logic]`(F-3 现金门硬排除)**GIVEN** Cash<PurchasePrice,**WHEN** 调 DecideBuyProperty,**THEN** false(不进评分,现金门短路)。
- **AC-21** `[Logic]`(F-3 双门满足)**GIVEN** Sharp(Epsilon=0,Threshold=15)、BuyScore=20、现金门过,**WHEN** 决策,**THEN** true。
- **AC-22** `[Logic]`(F-3 效用门不足)**GIVEN** Sharp、BuyScore=10、现金门过,**WHEN** 决策,**THEN** false(10<15)。
- **AC-23** `[Logic]`(F-3 Epsilon 范围+确定性)**GIVEN** Casual(Epsilon=15)、固定 seed、BuyScore=8,**WHEN** 同 seed 调 50 次,**THEN** FinalScore∈[−7,23] 且同 seed 结果相同。
- **AC-24** `[Logic]`(F-4 贪心 break)**GIVEN** 橙组 [1,1,1]、BuildingCost=100、Cash=Buffer+50,**WHEN** 调建房路径,**THEN** 返回长度==1(第2次 Cash−Cost<Buffer break)。
- **AC-25** `[Logic]`(F-4 CompletionBonus)**GIVEN** 组[A,B]=[1,1]、Normal,**WHEN** 算 A(建后[2,1] 非齐),**THEN** Bonus=0;算 B(内部模拟已建 A→[2,2] 齐),**THEN** Bonus=50。
- **AC-26** `[Logic]`(F-4 空候选)**GIVEN** 无垄断组或全酒店,**WHEN** 调建房路径,**THEN** 贡献 0 条动作(不 crash)。
- **AC-27** `[Logic]`(F-4 内部均衡模拟,economy Risk-2)**GIVEN** 组[A,B,C]=[1,1,0]、正常 Cash,**WHEN** 产建房序列,**THEN** 序列任意前缀执行后组内 max−min≤1(AI 内部已过均衡,不靠执行层兜底)。
- **AC-28** `[Logic]`(F-4 GDD Example)**GIVEN** Normal、橙组[1,1,1]、RentTable=[14,10,30,90,160,250],**WHEN** 算 BuildScore,**THEN** ==40(RentGain=20/Bonus=0)。
- **AC-29** `[Logic]`(F-5 同回合 churn 拦截)**GIVEN** 格 A 本回合已入 MortgagedThisTurn,**WHEN** 同次评估赎回候选,**THEN** A 不在候选集(无"抵押 A 后赎回 A")。
- **AC-30** `[Logic]`(F-5b 仅垄断组赎回)**GIVEN** is_mortgaged(A)、A 组非垄断、Cash 足,**WHEN** 评估 ShouldUnmortgage(A),**THEN** false。
- **AC-31** `[Logic]`(F-5b U_frac 门)**GIVEN** Normal(U_frac=1.1)、MV=90(cost=99)、Buffer=600,Cash=750 → false;Cash=800 → true(GDD Example)。
- **AC-32** `[Logic]`(F-5 全抵押空集)**GIVEN** 全持有格已抵押,**WHEN** 评估赎回,**THEN** 候选空、贡献 0 条(不 crash)。
- **AC-33** `[Logic]`(F-6 持卡优先)**GIVEN** bHasJailCard=true(任意态),**WHEN** 调 DecideJailAction,**THEN** UseCard(短路,无随机)。
- **AC-34** `[Logic]`(F-6 早期付保释)**GIVEN** 无卡、BoardDensity=0.2<Threshold=0.3、Cash=400、bail=50、Buffer=300,**WHEN** 调,**THEN** PayBail(GDD Example)。
- **AC-35** `[Logic]`(F-6 晚期留狱)**GIVEN** 无卡、BoardDensity=0.5>Threshold=0.3,**WHEN** 调,**THEN** RollDouble(不论 Cash)。
- **AC-36** `[Logic]`(F-6 早期没钱)**GIVEN** 无卡、EarlyGame、Cash=100<bail+Buffer=250,**WHEN** 调,**THEN** RollDouble。
- **AC-37** `[Logic]`(F-6 纯确定性)**GIVEN** 同 snapshot(无卡),**WHEN** 不同 seed 调 20 次,**THEN** 结果全同(F-6 不消耗 RNG)。

### D. 难度差异可感知性(CR-4,F-7,OQ-6 继承义务⑤)

- **AC-38** `[Integration]`(CR-4/F-7/OQ-6,继承义务⑤——三档**行为差异**可证伪门)**GIVEN** 一组**版本锁定的 N=20 个固定 seed**(写入 fixture)、每 seed 模拟 **K=200 局** Casual vs Sharp(对称起始),**WHEN** 在该锁定 seed 集上统计,**THEN** 三项**行为差异**指标全部达标(任一不达即 FAIL):② Sharp 平均建筑数≥Casual×1.3;③ Casual 平均垄断完成数≤Sharp×0.7;④ Casual 买地率≤Sharp×0.8。**阈值在 OQ-AI-1 原型期一次性冻结后写死进本 AC;冻结后不得为通过而调阈值或调 K**——删除原"提高 K 非降阈值"逃逸条款(固定 seed 下无抽样方差,该条款使 AC 逻辑上**无法 FAIL**=可证伪性陷阱,qa R-review B-2)。这三项是 heuristic-only 管线**可达且可证伪**的行为差异,**承重继承义务⑤**(绝对棋力不在此门,见 AC-38b)。`tests/integration/ai-opponent/difficulty_simulation_test`,**nightly CI(非 PR-merge gate——200 局×20 seed 运行成本高,unreal R-review R-3)**。
- **AC-38b** `[非 gate;移交 OQ-AI-1]`(绝对棋力,CR-4/OQ-AI-7)**Sharp 胜率**(对称起始 Sharp vs Casual)是**棋力上限**指标——依赖 heuristic 强度,**可能超 MVP 无前瞻管线上限**(ai-programmer R-review B-2:≥60% 在无前瞻下可能不可达,而唯一增强出口 1-ply 在 Alpha → BLOCKING gate 与 Alpha-only 出口**死锁**)。故绝对棋力**降为 OQ-AI-1 的 playtest 校准目标、非 BLOCKING gate**:原型期观测 Sharp 胜率;若 heuristic 证明目标不可达,触发 OQ-AI-7 评估 1-ply 浅前瞻(Alpha 增强),**而非无限调参或卡死 MVP gate**。
- **AC-39** `[Advisory]`(F-7 参数落地)代码审查:F-7 全 13 参数三档均数据驱动注入(config/DataAsset)、非硬编码;Casual `W_block==0`、Sharp `Epsilon==0`(不调 RNG 扰动)。证据 `production/qa/evidence/ai-f7-params-review.md`。
- **AC-40** `[Logic]`(F-7 Epsilon=0 不消耗 RNG)**GIVEN** Sharp(Epsilon=0),**WHEN** 调 DecideBuyProperty 10 次,**THEN** RNG 调用次数==0(游标不推进)。
- **AC-41** `[Logic]`(CR-4 共用管线)**GIVEN** 三档 F-7 参数手动设为完全相同值,**WHEN** 同 snapshot+同 seed 调三档,**THEN** 返回完全相同决策(差异唯一来源=参数)。

### E. 确定性 / RNG 纯函数(CR-5,OQ-T-Dice-2 继承义务②)

- **AC-42** `[Logic]`(CR-5② 重放)**GIVEN** 固定 snapshot S + 固定 seed K,**WHEN** 每次重置 seed 后连调三 hook 各 3 次,**THEN** 每次返回与首次逐元素全同。
- **AC-43** `[Logic]`(CR-5① tiebreak 不消耗 RNG)**GIVEN** 两格 BuildScore 相等,**WHEN** 调,**THEN** tiebreak 取 TileIndex 升序最小、RNG 调用不增。
- **AC-44** `[Advisory·code-review]`(CR-5①/OQ-T-Dice-2 继承义务②)代码审查:AI 模块**无** `FMath::Rand`/`RandRange`/`UKismetMathLibrary::RandomInteger`/BP `Random Integer/Float` 等引擎全局随机;一切随机经骰子3 `RandomRange`/`RandomFloat01`(`FRandomStream`)。BP-only 软约束,C++ 强封装可加静态扫描升 `[Logic]`。证据 `production/qa/evidence/ai-rng-review.md`,回链 dice OQ-T-Dice-2。
- **AC-45** `[Logic]`(CR-5③ 反作弊)**GIVEN** snapshot 注入"不该可见"字段(DeckOrder/NextDiceResult),**WHEN** 调三 hook,**THEN** 这些字段不影响任何决策(只读 CR-6 字段,额外字段被忽略,结果与无额外字段时同)。

### F. 继承义务(systems-index AI(10) 行)

- **AC-46** `[Integration]`(继承义务①+④,OQ-T-3)**GIVEN** 真实 AI 回合(bIsAI=true,字段完整),**WHEN** player-turn 在 PostRollAction 同步调 `DecidePostRollActions` 并执行返回序列,**THEN** ① 调用在单帧预算(≤16ms)内同步返回(不挂起/不 Latent);② 执行完毕后 `OnTurnEnded` 广播、行动权在 **≤ N 帧**移交下一未破产玩家。**回填 player-turn AC-37b 的 N = 1**(AI 决策+批执行+EndTurn 均单帧完成;无 async/Latent;单帧完整性由 player-turn CR-8 同步协议保障;将来若引入 Latent 须重裁 N 并同步 player-turn AC-37b)。
- **AC-47** `[Integration]`(继承义务② RNG 验证腿)**GIVEN** AI 真实决策期间(需跑决策路径并观测引擎全局游标,非纯 fixture),**WHEN** 追踪 RNG 游标,**THEN** 仅骰子3 RNG 游标推进、引擎全局游标不推进(与 AC-44 code-review 腿共同覆盖义务②)。**〔R-review 订正(qa B-1):本条 tier 此前在条目/合计/计数三处不一致;现钉死 `[Integration]` BLOCKING 并计入合计。〕**
- **AC-48** `[Logic·CI-stub]`(继承义务③ 字段齐备,依赖 OQ-1 ADR)**GIVEN** 三 hook 各收"刻意缺一个 CR-6 必需字段"的 snapshot,**WHEN** 调用,**THEN** ① 返回保守默认(false/[]/RollDouble);② 恰一条 `UE_LOG` dev 诊断(含缺失字段名);③ 不写状态。OQ-1 关闭前 CI 报 `pending OQ-1 struct`。
- **AC-49**(交叉引用锚点,不独立计数)继承义务⑤ = AC-38;确认绑定。**⑤ 的可裁定性现由 AC-38 的 seed-lock + 冻结阈值形态承重**(原 AC-38 含"提高 K 非降阈值"逃逸条款=可证伪性陷阱,已删,qa N-1);绝对棋力(原 ①胜率)已移交 AC-38b/OQ-AI-1,不在义务⑤ 的 gate 内。

### G. Edge Cases / 特殊情形

- **AC-50** `[Logic]`(Edge 空棋盘)**GIVEN** 全对手组持数=0、MaxRentExposure=0,**WHEN** Normal DecideBuyProperty,**THEN** BlockingValue=0、Buffer=Buffer_min、不 crash。
- **AC-51** `[Logic]`(Edge 车站口径切换)**GIVEN** 落 Railroad、station_count=1,**WHEN** 算 RentPotential,**THEN** 用 `railroad_rent(2)` 非 property_rent;MonopolyProgress 按 Held=1/GroupSize=4。
- **AC-52** `[Logic]`(Edge 公用估值)**GIVEN** 落 Utility、持 0 公用,**WHEN** 算 RentPotential,**THEN** 用 `utility_rent(dice=7,count=1)` 期望口径(不低估);MonopolyProgress 按 0/2。
- **AC-53** `[Logic]`(Edge 买地执行期不可行降级)**GIVEN** DecideBuyProperty 返 true 但执行期 5/6 校验不可行(mock),**WHEN** 框架降级,**THEN** AI 层不崩、决策已完、后续 hook 不受影响。
- **AC-54** `[Logic]`(Edge UseCard 但无卡)**GIVEN** 返 UseCard 但 bHasJailCard=false,**WHEN** 事件格7 校验无卡,**THEN** 降级 RollDouble(player-turn CR-8 #4);AI hook 本身不负责执行校验。
- **AC-55** `[Logic]`(Edge 多 AI 无串谋)**GIVEN** 3 AI 同局,**WHEN** 各自回合调 hook,**THEN** 各只消费自己 snapshot、无跨 AI 共享/调用(spy 验证 outbound log 无调另一 AI 实例)。
- **AC-56** `[Advisory·Performance]`(Edge 决策耗时;**R-review 降级 qa R-2/unreal B-2**)**GIVEN** 最复杂 snapshot(全盘满建筑、4 玩家、字段满载),**WHEN** 调三 hook,**THEN** 测试框架计时 P50≤16ms、P99≤50ms(headless `-nullrhi` 进程级,**nightly 趋势记录、非 PR-merge gate**)。**降级理由:** `-nullrhi` 墙钟 P99 在共享 CI 上方差大、不满足 coding-standards determinism 规则,作 `[Logic]` BLOCKING 会 flaky 误红;真正帧预算验证归目标硬件 + player-turn dev watchdog,非本 AC。**可数硬底线另立 AC-63b**(决策管线对最复杂 snapshot 的算分调用次数 ≤ 上界,确定性、CI 稳定)。
- **AC-57** `[Logic]`(Edge 破产后不被调用)**GIVEN** bIsBankrupt=true 的 AI,**WHEN** 回合2 turn order 跳过,**THEN** 其三 hook 调用次数==0(hook 入口计数断言)。

### H. 反幻想硬底线 / 纯度·重放·均衡守护(R-review 2026-06-05 补)

- **AC-58** `[Logic]`(反幻想①:现金充裕集组必建房,qa R-review B-3——concept 头号风险可枚举硬底线)**GIVEN** Cash≥BuildingCost+Buffer ∧ 持至少一个垄断组(组内全未抵押)∧ 该组未达均衡上限,**WHEN** 调 `DecidePostRollActions`,**THEN** 返回序列含 ≥1 条 `BuildHouse`(不得"现金充裕集组却不建房")。
- **AC-59** `[Logic]`(反幻想②:可补全垄断的最后一格必买,qa R-review B-3)**GIVEN** 候选无主地是某垄断组的最后一格(买下即垄断)∧ `Cash−PurchasePrice≥Buffer` ∧ **非 Casual 档**,**WHEN** 调 `DecideBuyProperty`,**THEN** true(不得"放着能集的垄断组不买")。
- **AC-60** `[Logic]`(反幻想③:不为烂地抵押黄金地段,F-5a 守卫的反幻想独立回归断言,qa R-review B-3)**GIVEN** 同时存在可抵押的非垄断空房地与垄断组黄金地、AI 需筹现,**WHEN** 评估 `ShouldMortgage`,**THEN** 选非垄断地、**绝不选垄断组**。
- **AC-60b** `[Logic]`(Casual 失误守卫:人性化≠犯蠢,game-designer R-review GD-B2)**GIVEN** Casual 档、候选无主地是某垄断组**补全格**(MonopolyProgress 满分)且现金门满足,**WHEN** ε 扰动求值,**THEN** AI **不得**因纯 ε 扰动漏买该补全格(扰动仅作用于非补全格的边际决策)——Casual"人性化失误"限定在边际,不触 concept 点名反幻想。
- **AC-61** `[Integration]`(权威 RNG 流隔离 + 全回合重放,CR-5④,ai-programmer R-review B-3)**GIVEN** 不重置 seed 的完整多回合序列(掷骰→AI 决策→掷骰→…),**WHEN** 连跑两次,**THEN** ① 两次逐步全同(端到端重放,非仅孤立 hook);② spy 断言权威流消费方 ∈ {dice,AI},呈现层(VFX/HUD juice)未消费权威流。补 AC-42(重置 seed 的孤立 hook 重放)未覆盖的"流共享端到端"盲区。
- **AC-62** `[Logic]`(纯函数无跨调用泄漏,CR-5②,systems R-review B-2/unreal R-2)**GIVEN** 同一 AI 实例**连续两次** `DecidePostRollActions`(第一次有抵押动作写入 `MortgagedThisTurn`),**WHEN** 第二次调用同一 snapshot+seed,**THEN** 第二次结果不受第一次历史影响(`MortgagedThisTurn` 为 hook 内局部、非成员;成员实现会使本 AC FAIL,守护跨调用泄漏)。
- **AC-63** `[Logic]`(F-4 内部均衡模拟 ⇌ 建房8 谓词一致性回归守护,systems R-review B-3)**GIVEN** 同一 fixture,**WHEN** AI 内部均衡判定(F-4 序列 max−min≤1 校验)与建房8 `CanBuildHouse` 逐格比对,**THEN** 二者逐格一致(AI 内部副本未 stale;建房8 改均衡规则时本 AC FAIL,暴露副本漂移——补 AC-27 只验序列自洽、不验"是否仍等于建房8 真实规则"的盲区)。
- **AC-63b** `[Logic]`(决策耗时可数硬底线,替代 AC-56 墙钟硬门,unreal R-review B-2)**GIVEN** 最复杂 snapshot,**WHEN** 调三 hook,**THEN** 决策管线算分/查表调用次数 ≤ 确定性上界(F-4 贪心 O(候选²),如 ≤2500 次);不依赖墙钟、CI 稳定、可作 PR gate。

**合计 67 条(65 实测 + AC-49/AC-38b 2 个非计数锚点·移交)**:53 `[Logic]`(含 0 永久 stub)+ 4 `[Logic·CI-stub]`(AC-5/6/7/48,依赖 OQ-1 GameStateSnapshot ADR)+ 4 `[Integration]`(AC-38 难度差异 / AC-46 同步移交 / AC-47 RNG 验证腿 / AC-61 重放隔离)+ 4 `[Advisory]`(AC-4/39/44/56)= **61 BLOCKING + 4 Advisory**。覆盖 CR-1..7 全、F-1..7 全、Edge Cases 全、**反幻想头号风险可枚举硬底线(AC-58/59/60/60b)**、5 继承义务全(①AC-46 ②AC-44+47 ③AC-48 ④AC-46 N=1 ⑤AC-38 seed-lock 形态;绝对棋力移交 AC-38b/OQ-AI-1)。

## Open Questions

| OQ | 问题 | Owner | 目标时机 |
|---|---|---|---|
| **OQ-AI-1** | F-7 全 **13** 参数为 playtest 起始基线;**AC-38 行为差异阈值**(建筑≥Casual×1.3、垄断≤Sharp×0.7、买地率≤Sharp×0.8)须原型期 playtest **一次性校准后冻结写死进 AC**(冻结后不得为通过而调阈值/调 K)。**④买地率方向风险(ai-programmer R-review B-2):** F-7 给 Sharp 更高 BuyThreshold=更挑剔,可能使 Sharp 买地率反低于 Casual;校准须验证 Casual 的大 ε+松缓冲(漏买/现金门挡)确实压低其**有效**买地率,若基线反向则在 F-7 安全范围内重调、非翻转 AC。**绝对棋力(原 Sharp 胜率,AC-38b)= 校准目标非 gate**:不可达触发 OQ-AI-7、不无限调参。 | game-design / playtest | 原型期 |
| **OQ-AI-2** | "像人不犯蠢"剩余**主观整体**目标(concept #1 技术风险):**可枚举蠢招已落 BLOCKING 硬底线 AC-58/59/60/60b**(现金充裕集组必建 / 可补垄断最后格必买 / 不抵押垄断黄金地 / Casual ε 不漏补全格);本 OQ 现仅覆盖**不可枚举的整体观感**(每局明显失误数 playtest 量化),超阈值则补新守卫规则。头号风险不再纯靠主观 playtest。 | playtest / UX | 原型期 |
| **OQ-AI-3** | `GameStateSnapshot` UE struct/字段构成(架构期 OQ-1 ADR,**AI 开工硬前提**);CR-6 per-hook 字段清单(含 R-review 新增的**全盘暴露视图 Rent_top1/top2 + owner map**)为 ADR 注入方对照基准。AC-5/6/7/48 依赖此 struct(CI-stub 至 ADR 关闭)。**CI-stub 关闭条件(qa R-review R-1):** OQ-1 ADR 关闭后,AC-5/6/7/48 必须在**同一 sprint 内**从 CI-stub 转真实 `[Logic]`,pending 横幅不得跨 sprint 存活。**snapshot UE 成本裁定(unreal R-review B-1,ADR 须钉死):** ① hook 签名传 `const FGameStateSnapshot&`(避免按值深拷贝聚合 struct);② 派生量预聚合 vs hook 内现算统一口径;③ snapshot 构造+传入端到端耗时须有一条 AC(当前 AC-56 只测 hook 内、AC-37b 是 Advisory,构造成本两侧无机器证明)。 | 架构期(OQ-1 ADR) | AI 实现前 |
| **OQ-AI-3b** | **AI 决策核心 BP vs C++ 裁定(unreal R-review B-3)= 确定性义务② 可执行性前置**:BP 为主项目无法机器强制禁 BP 随机节点(uasset 二进制 diff 不可审),AC-44 软约束不足;OQ-1 ADR 须裁定 AI 决策核心是否强制落 C++ 以加静态符号扫描(真硬门)。 | 架构期(OQ-1 ADR) | AI 实现前 |
| **OQ-AI-4(跨档 backfill,本轮 propagate)** | 本档 AC-46 定 **N=1**(AI 决策+批执行+EndTurn 单帧移交);须回填 player-turn **AC-37b** 的"N 由 AI(10) 定义"空白(继承义务④)。 | producer / player-turn | Phase 5 propagate |
| **OQ-AI-5(跨档,本轮)** | systems-index #10 `depends-on` 订正:补 **1,2**(棋盘/回合编排),**4 降 Alpha**(MVP 无前瞻不用移动估距)。 | Phase 5 | 本轮 |
| **OQ-AI-6** | Alpha hook 决策逻辑:`DecideAuctionBid`(拍卖12)/`DecideTradeResponse`(交易11)/命运之轮13·俄罗斯轮盘14·策略卡15 使用决策。MVP 不触发,契约开口已在 CR-2 标。 | 各 Alpha 机制 GDD | Alpha |
| **OQ-AI-7** | lookahead 上限:MVP **heuristic-only**(user 裁定)。若 playtest 显示 Sharp 棋力不足以"值得赢",Alpha 评估是否加 1-ply 浅前瞻(非 MVP 返工,纯增强)。 | game-design | Alpha |
| **OQ-AI-8(R-review 新增)** | F-1 MaxRentExposure 现用 **top-2 之和**近似一圈累计停租风险(MVP)。完整"整圈期望暴露"模型(按棋盘密度/步距分布加权所有可能停租)留 Alpha 评估——若 playtest 显示 top-2 仍不足以防多次停租破产则升级。 | game-design / playtest | Alpha |
| **OQ-AI-9(R-review 新增,跨 hook 融资盲区)** | MVP 孤立 hook 无法"为补全垄断而在 buy hook 内融资"(抵押 A 买 B)——ai-programmer B-1 结构盲区,user 既定 heuristic-only 边界内**接受为 MVP 已知局限**(蠢招②整组瘫痪已由 AC-58 + F-4/F-5b 同窗赎回-解锁建房缓解,见 RECOMMENDED)。Alpha 随响应方 hook 评估是否补跨 hook 现金调度。 | game-design | Alpha |
