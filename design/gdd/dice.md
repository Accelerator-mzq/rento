# 骰子 (Dice)

> **Status**: APPROVED — design-review R3 fresh re-review (2026-06-02, 4 specialists + CD senior synthesis; CONVERGED, 0 doc-blocking). IG-1(OQ-T-Dice-5 propagate)✅ + IG-2(systems-index 继承义务登记)✅ 已闭合;仅剩 IG-3(OQ-1 ADR,归 architecture,移动(4)/AI(10) 开工前关闭)——见 review-log R3。
> **Author**: msc + agents
> **Last Updated**: 2026-06-02
> **Implements Pillar**: ③运气×策略交织（运气源头）、①桌游忠实（标准 2d6）
> **System**: #3 in systems-index design order | Foundation | Core | MVP
> **Review Mode**: full (design-review R2, 2026-06-02 — 4 specialists + CD senior synthesis)
> **闭合契约**: player-turn(2, APPROVED) 锁定——回合内掷标准双骰返回 `(点数, bIsDouble)`；定序掷骰用双骰但只取点数和、忽略 bIsDouble（不进双点链）；Full Vision 须可种子化（OQ-3 重放），AI 随机性也走此 RNG。

<!-- 骨架由 /design-system 创建。逐节设计。
     关键约束（供续做参考）：
     - 零依赖 Foundation 纯 RNG 服务；被依赖：玩家回合(2)/移动(4)/事件格(7,前进到最近X)/AI(10,随机性)/VFX(19,juice)
     - 契约已由 player-turn 锁定：标准双骰 (d6+d6)，返回 (点数和, bIsDouble)；定序只取和、忽略 bIsDouble
     - 种子化关键点：BP 全局 Random 不可种子化 → 须走 FRandomStream 以兑现 OQ-3 重放 + AI RNG 软约束
     - 编码标准：数值数据驱动、不硬编码；引擎 UE5.7 Blueprint 为主
-->

## Overview

骰子系统是《骰子大亨》的**随机数源头**:一个零依赖的纯 RNG 服务,负责"掷一次骰、产出一个结果"。它对外提供唯一的核心能力——**掷标准双骰(2d6)**,返回 `(点数和, bIsDouble)`:点数和 ∈ [2,12] 供移动(4)推进棋子,`bIsDouble`(两骰同点)供玩家回合(2)驱动双点额外回合链与连续双点入狱判定。本系统**只产出随机结果,不解释结果的含义**——"走几步""是否额外回合""定序谁先手"全归调用方裁决;骰子只回答"掷出了什么"。整套随机走**可种子化 RNG**(`FRandomStream`),使开局定序、回合内掷骰、(Full Vision)AI 决策的随机性全部可确定性重放(OQ-3 联网权威/重放预留),且 AI 的内部随机性**也必须经此服务**而非引擎全局随机,以保证一局可被同种子完整复现。对玩家而言,这个系统隐形:他们看到的"掷骰期待感、骰子翻滚、点数定格"的 juice 由 VFX(19)/HUD(16) 呈现,本系统只为其提供权威的随机结果与"掷骰完成"事件。它做得好时玩家只觉得"骰子是公平的、随机的",而不会意识到背后有一个 RNG 服务。

## Player Fantasy

> *本系统为纯基础设施,无独立 player fantasy(`/design-system` 框架问诊裁定)。玩家不会想"这是一个 RNG 服务"。*

骰子系统服务的情感目标完全是**间接的**:它兑现的是支柱③"运气×策略交织"中**运气那一极的公平性前提**。玩家真正感受到的——掷骰前的期待、骰子翻滚的紧张、点数定格的瞬间爽快或懊恼——这些 juice 由 VFX(19)/HUD(16) 呈现,**不属于本系统**。本系统对幻想的唯一贡献是一条隐形的信任底线:**"骰子是公平的、不可预测的、不会针对我"**。当随机分布均匀、没有可被玩家察觉的偏向或可预测模式时,玩家把每一次掷骰的结果归因于"运气",而非"系统在坑我"——这正是支柱③运气刺激能成立的前提(若骰子被怀疑不公,运气就不再是刺激而是挫败)。它的成功标准与棋盘数据、玩家回合一样是**隐形的正确**:玩家全部情绪投向"这把运气好不好"的博弈本身,而非"这骰子是不是有问题"的猜疑。

> **(design-review R2,承认 MDA 风险)** 真随机存在一个砸穿此信任底线的内在风险:玩家对"公平"的感知遵循**可得性启发(availability heuristic)**——连续掷出低点(如连三次 ≤4)即使统计完全正常,玩家主观会显著偏向"骰子在针对我"。本系统对此风险的 **MVP 立场:接受**(支柱①真随机忠实优先),缓解手段(anti-tilt 方差压缩)登记 OQ-5 待 playtest 数据决定;呈现层可用 VFX(19) juice 部分掩盖体感(归 VFX19,非本系统)。此处显式承认而非沉默,使 OQ-5 与 Player Fantasy 有连接、而非各自孤立。

## Detailed Design

### Core Rules

**CR-1 标准双骰掷骰(核心 API):** `RollDice()` → `FDiceRollResult { int32 Die1; int32 Die2; int32 Total; bool bIsDouble; }`。两颗骰子 `Die1`/`Die2` 各**独立**均匀 ∈ [1,6];`Total = Die1+Die2` ∈ [2,12];`bIsDouble = (Die1==Die2)`。**是"两颗独立 d6"而非"先掷 Total 再拆解"**——分布形态(2/12 各 1/36、7 为 6/36 的三角分布)由两独立骰自然产生,经典大富翁忠实(支柱①)。

**CR-2 中央可种子化 RNG 服务:** 本系统持**唯一权威 `FRandomStream`**,所有随机抽取走此流:
- **主 API:** `RollDice()`(上)。
- **通用原语:** `RandomRange(Min, Max)` → int ∈ [Min,Max] 闭区间均匀;`RandomFloat01()` → float ∈ [0,1)。供 AI(10) 决策随机、Alpha 命运之轮/俄罗斯轮盘抽取。
- **禁止旁路:** 任何需可重放随机的系统(AI/Alpha 博弈)**必须**经本服务,**禁**直接调引擎全局 `FMath::Rand`/BP `Random Integer in Range`(走全局不可种子化流、破坏重放)。**强度坦诚标注:** BP-only 下为**软约束**(code-review 守);若 OQ-1 选 C++ 强封装可加静态扫描禁用引擎 RNG 符号作硬门——与 player-turn CR-8 确定性 bullet / OQ-T-3② 同构。

**CR-3 种子与确定性:** 流在开局初始化设 `InitialSeed`。MVP:种子来自系统时间(每局不同)或固定值(测试 fixture);Full Vision:服务器下发种子,使一局由 (种子 + 确定性抽取序列) 完整重放(OQ-3)。**确定性前提:** 重放成立要求抽取调用序列确定——单线程同步回合流(player-turn CR-8 已把 AI 决策钉为同步)保证序列确定 → **单一共享流可完整重放**。

**CR-4 掷骰与"含义"解耦:** 本系统只产出 `FDiceRollResult`,不解释:移动(4) 消费 `Total`;玩家回合(2) 消费 `bIsDouble`(双点链/入狱 F-3);开局定序(player-turn F-4) 复用 `RollDice()` 只取 `Total`、**忽略 `bIsDouble`**(定序双点不进双点链,player-turn CR-2 已钉);**本系统无需为定序提供单骰模式**;VFX(19)/HUD(16) 消费 `Die1`/`Die2` 画两颗物理骰子。

**CR-5 无偏均匀:** 每面 1/6 等概率、两骰独立。**MVP 不做**"幸运骰/保底/伪随机平滑"等偏置(支柱③公平性前提,见 Player Fantasy);MVP 真随机是支柱①忠实的正当起点。**但 anti-tilt 方差压缩(保期望、降方差)与加权骰(改期望)是不同性质,登记为 OQ-5 待 playtest 决定**(design-review R1,不在 CR-5 关死)。Alpha/House Rules 加权骰另见 OQ-4。

### States and Transitions

骰子系统是**服务,非状态机**——它不持有"上一次掷骰结果"作为权威状态(结果产出即交调用方,玩家回合2 持有并序列化**完整 `FDiceRollResult{Die1,Die2,Total,bIsDouble}`**,见 player-turn AC-34 与下方 B1 回链)。唯一内部状态是 RNG 流的**当前 Seed**(`FRandomStream` 是 LCG、无显式游标计数器,"进度"完全编码在当前 `int32 Seed` 值里——见 OQ-2 用词):

| 状态 | 含义 | 转换 |
|---|---|---|
| 流未初始化 | 开局前,种子未设 | → 就绪(开局 `SetSeed`) |
| 就绪 | 流持当前 `Seed`,可抽取 | 每次 `RollDice`/`RandomRange` → `Seed` 滚动推进(仍就绪) |

**无"掷骰中"瞬态:** `RollDice()` 是同步函数、单帧返回。骰子翻滚动画是 VFX(19) 的**呈现态**,非本系统状态(本系统在动画开始前已产出权威结果)。

**当前 Seed 序列化(读档确定性,OQ):** MVP——玩家回合(2)已序列化当前回合**完整 `FDiceRollResult`(含 `Die1`/`Die2`)**、读档不重掷当前骰(player-turn Edge Case),故 MVP **即使读档后重设种子(`Seed` 不续)也不触发重复掷骰 bug**。Full Vision 重放(OQ-3)才需序列化当前 `Seed`(`GetCurrentSeed()`)以续算后续抽取——列为 Open Question(OQ-2)。

> **(design-review R1, B1 关键修)** 序列化必须携带 `Die1`/`Die2`,**不可只存 `(Total,bIsDouble)`**:VFX(19) 契约(见 Visual/Audio)强制用 `Die1`/`Die2` 分别画两颗骰面、禁从 `Total` 拆解;若读档只存 `(Total,bIsDouble)`,Die1/Die2 丢失→VFX 无法忠实重现骰面(Total=4 无法判定是 1+3 还是 2+2)。回链 player-turn AC-34(其序列化字段须从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`)——见 OQ-T-Dice-5。

### Interactions with Other Systems

| 系统 | 数据流 | 谁拥有 |
|---|---|---|
| 玩家回合(2) | `RollPhase` 调 `RollDice()` → 收**完整 `FDiceRollResult`(Die1/Die2/Total/bIsDouble)**:消费 `Total`+`bIsDouble` 做逻辑、**序列化全字段含 Die1/Die2(B1)**;定序调 `RollDice()` 只取 `Total` | 骰子拥有随机;回合消费做逻辑 |
| 移动(4) | 间接:回合把 `Total` 传给移动推进步数 | 移动拥有位置改写 |
| 事件格(7) | 掷双点出狱:调 `RollDice()` 收**完整结果**、取 `bIsDouble` 字段(出狱双点不进双点链,归7落实;非"只返回 bIsDouble 的简化接口") | 7 拥有监狱规则 |
| AI(10) | AI 决策随机经 `RandomRange`/`RandomFloat01`(禁引擎全局 RNG;软约束) | AI 拥有决策;骰子拥有随机源 |
| 命运之轮/俄罗斯轮盘(Alpha) | 经通用原语抽取(可种子化、可重放) | 各机制拥有规则;骰子拥有随机源 |
| VFX(19)/HUD(16) | 监听 `OnDiceRolled(FDiceRollResult)` 读 `Die1`/`Die2`/`Total` 呈现两骰 + juice | 呈现层拥有动画 |
| 存档(21) | MVP 不序列化当前 Seed(当前骰结果由回合2序列化完整 `FDiceRollResult` 含 Die1/Die2);Full Vision 重放序列化当前 Seed(OQ-2) | 存档拥有序列化 |

**事件:** 本系统广播 `OnDiceRolled(FDiceRollResult)` 供 VFX19/HUD16 挂接呈现(`DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`,payload `FDiceRollResult` 须 `USTRUCT(BlueprintType)`——与 player-turn 事件形态一致)。

**广播/返回顺序(design-review R1 规格化,守 F-4 前提②):** `RollDice()` 的执行顺序**必须**是:① 从流抽取 Die1/Die2 → ② 在本地固化 `FDiceRollResult` → ③ 广播 `OnDiceRolled(固化值)` → ④ 返回**同一固化本地值**给调用方。返回的是广播前已固化的本地变量,**非广播后重读流**——保证"同步返回值 == 事件 payload"恒成立(AC-14 断言依赖此顺序)。
**禁单线程重入:** **禁止**在 `OnDiceRolled` 回调内同步调任何抽取 API(`RollDice`/`RandomRange`/`RandomFloat01`)——回调内二次抽取会把额外抽取插入调用序列中间、且其顺序依赖订阅者注册顺序(dynamic multicast 调用顺序不保证稳定),破坏 F-4 前提②"调用序列确定"。这与"多线程并发禁止"(Edge Cases)是不同的失序来源(一个单线程重入、一个跨线程)。订阅者**必须是纯呈现/只读**,不得产生抽取副作用。

**接口稳定承诺:** `FDiceRollResult` 字段只增不改语义;`RollDice()`/`RandomRange()`/`RandomFloat01()` 签名稳定(给下游的稳定保证)。

## Formulas

> *由 `systems-designer` 提议(lean 模式 D 节派发,Section D 为高风险节);主会话采纳测试分层(确定性 [Logic] gate / 统计 [Advisory] 非门控)。*

本系统**不拥有**任何 gameplay 公式(走几步/租金归移动4/经济5),只规格化随机抽取自身。

### F-1 RollDice — 标准双骰

`Die1 = FRandomStream.RandRange(1,6)`;`Die2 = FRandomStream.RandRange(1,6)`(两次独立抽取、游标各推进一次);`Total = Die1+Die2`;`bIsDouble = (Die1==Die2)`

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 第一颗骰 | `Die1` | int32 | [1,6] | 独立均匀 d6 |
| 第二颗骰 | `Die2` | int32 | [1,6] | 独立均匀 d6 |
| 点数和 | `Total` | int32 | [2,12] | 供移动(4) |
| 双点 | `bIsDouble` | bool | {F,T} | `Die1==Die2`,供回合(2) |

**Output Range:** `Total ∈ [2,12]` 有界、无需 clamp。`RandRange(1,6)` 是**闭区间**整数均匀,每面精确 1/6(UE pre-5.3 稳定 API)。**是"两颗独立 d6"而非"先掷 Total 再拆解"**——三角分布由两独立骰自然产生(经典大富翁忠实①)。

**Total 完整 PMF(三角分布,统计验收锚点):**
| Total | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 组合 | 1 | 2 | 3 | 4 | 5 | 6 | 5 | 4 | 3 | 2 | 1 |
| P | 1/36 | 2/36 | 3/36 | 4/36 | 5/36 | 6/36 | 5/36 | 4/36 | 3/36 | 2/36 | 1/36 |

`P(bIsDouble) = 6/36 = 1/6`(六种同点组合各一:(1,1)…(6,6))。

**Example:** seed=42 → Die1=3,Die2=3 → Total=6,bIsDouble=true。(具体值随种子→序列映射;确定性 fixture 须先运行取真实序列再固化为 expected,见 F-5。)

### F-2 RandomRange(Min,Max) — 闭区间整数均匀

`RandomRange(Min,Max) = FRandomStream.RandRange(Min,Max)`,result ∈ [Min,Max],`P = 1/(Max−Min+1)`

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 下界(含) | `Min` | int32 | 任意,须 ≤Max | 闭区间下界 |
| 上界(含) | `Max` | int32 | 任意,须 ≥Min | 闭区间上界 |
| 结果数 | `OutcomeCount` | int32 | [1,∞) | `Max−Min+1` |
| 抽取值 | `result` | int32 | [Min,Max] | 均匀整数 |

**Output Range:** `[Min,Max]` 闭区间。供 AI(10) 决策随机、Alpha 博弈抽取。

**退化与边界契约:**
- `Min==Max` → **封装层 early-return Min,不调 FRandomStream(`Seed` 不推进)** —— 防退化调用消耗 `Seed` 破坏已固化的确定性 fixture 序列。
- `Min>Max` → **调用方错误;不自动交换**,`ensure(Min<=Max)` + `UE_LOG(Warning)` 暴露(自动交换会静默掩盖参数反向传的 bug;且 `FRandomStream.RandRange` 在 Min>Max 时引擎层行为未明文保证,封装层加硬门拦截)。
- **跨度溢出 + 浮点精度(design-review R1 补 S-5;R2 修正约束上界)** → `OutcomeCount=Max−Min+1` 在 `Max=INT32_MAX, Min=INT32_MIN` 时整数溢出(=2³²)。但真实约束**比"不溢出"更紧**:因 `RandRange` 内部走 `FRand()*Range` 浮点中介(见 UE5.7 标注 L172 / F-4),当 `Range ≥ 2²⁴` 时浮点乘积精度崩溃(float 尾数 23 位)、均匀性破坏——**与 F-3 `floor(f*N)` 的 `N<2²⁴` 上界同因同源**(整数路径实走浮点,与 F-3 对称,非"整数比浮点安全")。故 `RandRange` 须约束 **跨度 `Range = Max−Min+1 < 2²⁴`**(R2 修正:非仅 `< INT32_MAX`);超 2²⁴ 须改用纯整数 LCG 路径(同 OQ-3)。封装层加 `ensure((int64)Max-(int64)Min+1 < (1<<24))`。MVP 实际跨度极小(AI 三选一、骰面 6),此为接口完整性边界。

**Example:** `RandomRange(0,2)` → P(0)=P(1)=P(2)=1/3(AI 三选一);`RandomRange(5,5)` → 恒 5、不抽取。

### F-3 RandomFloat01 — 半开区间浮点均匀

`RandomFloat01() = FRandomStream.FRand()`,f ∈ [0.0, 1.0)

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 浮点抽取 | `f` | float | [0.0, 1.0) | 1.0 严格排除 |

**Output Range:** `[0.0, 1.0)`,**1.0 严格排除**。理由:标准消费模式 `floor(RandomFloat01()*N)` 给均匀 [0,N-1];若 1.0 可达则 `floor(1.0*N)=N` 越界。**N 上界(design-review R1 补 rec-9):** `floor(f*N)` 不越界要求 `N < 2²⁴`(float 尾数精度上限);`f_max≈0.99999994`,当 `N≥2²⁴` 时 `f_max*N` 可能因浮点舍入恰等于 N、`floor` 返回 N。MVP 命运之轮 N=12 等小 N 绝对安全;若 Alpha 出现大 N(≥16M 量级)须改用 `RandomRange` 整数路径(与下方"f*MAX_INT 溢出→改 RandomRange"同因)。

**陷阱:** `round(f*(N-1))` 两端概率减半(**不均匀**,禁用);`f==1.0f` guard 恒 false(多余且误导);`f*MAX_INT` 截断有溢出风险(改用 RandomRange)。

**Example:** 命运之轮 12 扇区 `floor(0.7917*12)=9`;边界 f=0→扇区0,f→0.9999→扇区11,f=1.0 不可达。

### F-4 确定性契约(性质 D)

> **性质 D:** 给定相同 `InitialSeed` + 相同**有序抽取调用序列** `[call_1…call_n]`(每个 call 为 `RollDice`/`RandomRange`/`RandomFloat01` 之一),输出序列在**同一平台/同一编译配置**的任何执行时完全相同。

**成立前提(全须满足):**
① 单一权威 `FRandomStream`、禁旁路引擎全局 RNG(CR-2);
② 调用序列确定——由 player-turn CR-8 单线程同步回合流保证(AI 同步、无后台线程插随机)、且**无单线程重入抽取**(见事件节广播/返回顺序);
③ 种子相同(MVP 固定测试种子 / Full Vision 服务器下发);
④ `Min==Max` 退化不推进 `Seed`(F-2 封装保险);**且进入确定性 fixture 的 `RandomRange` Min/Max 须为跨运行稳定值(编译期常量 / 测试期已固化值)**——真实约束是"值在两次运行间确定不变"(R2 修正归因:数据驱动只是不稳定的一个常见来源,数据驱动但在 fixture 构建期已固化的值仍安全;反之未固化的任何来源都禁入)。否则运行时 `Min==Max` 与否会改变是否推进 `Seed`,使固化序列错位(design-review R1 补 rec-8,R2 措辞修正)。

> **⚠ 跨平台位级一致性不被性质 D 保证(design-review R1 重大修正,CD 裁定)。** 原表述"任何平台(同字节序)完全相同"**过强且约束词错误**("同字节序"无关,关键是浮点舍入行为)。经 UE 源码核实:`FRandomStream::RandRange(int,int)` **内部走浮点中介**(`RandHelper: TruncToInt(FRand()*Range)`),因此**整数 `RandRange` 的跨平台确定性 = `FRand()` 的跨平台确定性**——"整数比浮点安全"**不成立**。`Seed` 的 LCG 整数滚动跨平台一致(流推进确定),但每次抽取的**输出映射**在浮点截断边界(如 `FRand()*6` 落在 4.9999997 vs 5.0000001)上跨编译器/优化/平台(FMA 合并、x87 vs SSE/NEON)**可能产生极罕见的 off-by-one**。
> **后果:** Full Vision 联网重放(OQ-3)若服务器与客户端各自跑流,即使同种子同序列,单骰输出仍可能分歧→desync。**正解(归 OQ-3):** 服务器单一权威 RNG、客户端只收 RPC 下发的 `FDiceRollResult`(绕开客户端浮点);或自实现纯整数 LCG→整数取模(绕开 `FRand()` 浮点)。MVP 单机不触发,降为 Full Vision OQ。**实现阶段须查 UE5.7 源码确认 `RandHelper` 是否仍走 `FRand()` 浮点。**

### F-5 统计可测性(测试分层 — 防 flaky gate)

**层一 [Logic] 确定性种子序列测试(PR Gate,BLOCKING):** 固定种子下 `FRandomStream` 输出序列完全确定;fixture 固化一段已知序列(≥20 次抽取),逐字段精确相等断言。**不用频率断言**——单局掷骰次数有限、频率置信区间宽、CI 会 flaky(这正是 [Logic] gate 不可接受的)。失败当且仅当 `FRandomStream` API 破坏性变更(可检测、有意义)。fixture 构建:固定种子运行一次→打印序列→硬编码为 expected 常量。

**层二 [Advisory] 大样本 chi-square 均匀性(非门控):** n=36000 掷,`expected[Total]=n×PMF`(如 expected[7]=6000、expected[2]=1000),**Total PMF chi-square(df=10) < 29.59**(α=0.001 极保守;Total 有 11 类别 [2,12],拟合优度 df=11−1=10);同验 `bIsDouble`(二分类,df=1,临界≈10.83)、单面均匀(两骰面值合并 N=72000、expected 每面 12000,df=5)、双骰独立性(联合频率矩阵 vs 边缘乘积,df=25)。失败→记录日志、**不阻塞 PR**。

> **(design-review R1 修)** 自由度勘误:Total 11 类别拟合优度 df=10(非 11)、临界 29.59(非 31.26);与 AC-4 对齐。`bIsDouble` 二分类 df=1、临界≈10.83(非 31.26)。

> **核心原则:PR gate 只放确定性断言;频率/统计断言作定期 Advisory,不阻塞合并。**

### UE5.7 标注
- `FRandomStream.RandRange` 闭区间 / `FRand()` ∈[0,1) / `FRandomStream` 是值类型(可 `UPROPERTY` 持有、无需 GC)——均 pre-5.3 稳定。
- `FRandomStream` 原生**非 BP callable**,须 C++ 封装为 `UFUNCTION(BlueprintCallable)` 函数库/Component,BP 调封装函数。
- **(design-review R1)** `FRandomStream::RandRange(int,int)` **内部实现走浮点中介**(`RandHelper: Min + TruncToInt(FRand()*Range)`),**非纯整数取模**——故整数 `RandRange` 与 `FRand()` 同源、跨平台位级风险同源(见 F-4 跨平台警告)。
- **(design-review R1)** `FRandomStream()` **默认构造种子恒 `0`**(确定常量)——lazy-init **禁**落此默认值(见 Edge Cases B4)。
- **(design-review R1)** `FRandomStream` 无显式"游标计数器",内部状态仅 `InitialSeed` + 当前滚动 `Seed`(两 int32);序列化当前进度 = 序列化当前 `Seed`。**实现须用公有 accessor `GetCurrentSeed()` 显式存取,勿依赖 `UPROPERTY(SaveGame)` struct 反射**(反射可能只持久化 `InitialSeed` 而丢当前 `Seed`、静默破坏 OQ-2 续算)。
- ⚠ **知识盲区:** UE5.4–5.7 的 RNG 子系统是否有变更超出 LLM 知识截止——实现阶段须对照 UE5.7 Release Notes/源码确认:① `RandHelper`/`RandRange` 是否仍走 `FRand()` 浮点;② `FRandomStream` 经 `UPROPERTY` 序列化是否持久化当前 `Seed`;③ 默认种子是否仍 `0`;④ 是否新增官方确定性/网络 RNG 子系统(若有,Full Vision 联网重放可能优于手搓 `FRandomStream`)。
- BP 全局 `Random Integer in Range` **禁用于可重放路径**(走 `FMath::Rand`、不受种子控制)。

## Edge Cases

- **若流未初始化即调 `RollDice`/`RandomRange`/`RandomFloat01`**:开局必先 `SetSeed`(CR-3)。未初始化调用 → `ensure` dev 诊断 + lazy-init 兜底(shipping 不崩、不返回未定义值)。**⚠ lazy-init 种子来源必须是非确定值(如 `FPlatformTime::Cycles()`),禁用 `FRandomStream` 默认构造种子 `0`**(design-review R1):UE `FRandomStream()` 默构种子恒 `0`,若 lazy-init 落到固定 `0` → 每个"忘记 SetSeed"的对局掷出**完全相同的确定序列**,玩家会察觉"怎么每次都是这几把"、静默砸穿 Player Fantasy 的"骰子不可预测"信任底线,且 AC-1 范围断言查不出(返回值范围照样合法)。**Full Vision 重放模式下,未 SetSeed 是致命错误→应 fail-fast,不走 lazy-init 兜底**(重放路径要求种子由服务器显式下发)。
- **种子 = 0 或负数**:`FRandomStream` 接受任意 int32 种子,**`0` 是合法种子**(非"无种子"哨兵)。不对种子值做拒绝。
- **`RandomRange(Min,Max)` 其中 `Min>Max`**:调用方错误;`ensure(Min<=Max)` + `UE_LOG(Warning)`,返回 `Min`,**不自动交换**(交换静默掩盖参数反向传 bug,见 F-2)。
- **`RandomRange(Min,Max)` 其中 `Min==Max`**:封装层 early-return `Min`,**不调 FRandomStream、游标不推进**(防退化调用破坏确定性 fixture 序列,见 F-2/F-4④)。
- **`floor(RandomFloat01()*N)` 当 `N==0`**:调用方错误(无扇区可选);本系统不防御(N 由调用方语境保证 ≥1),归调用方责任。
- **多线程并发抽取**:**禁止**——性质 D 前提②要求单线程调用序列确定。若未来后台线程需随机,**须用独立 `FRandomStream`,不得共享此权威流**(共享会插入不确定顺序的抽取、破坏重放)。此为架构约束,登记 Open Questions。
- **单线程重入抽取(design-review R1 新增)**:**禁止**在 `OnDiceRolled` 回调内同步调任何抽取 API——回调内二次抽取会按订阅者注册顺序(不保证稳定)插入调用序列、破坏 F-4 前提②。与多线程并发是不同失序来源。订阅者须纯呈现/只读。见事件节"广播/返回顺序"。
- **payload 不变式**:`FDiceRollResult` 中 `Total==Die1+Die2` 且 `bIsDouble==(Die1==Die2)` 是构造时不变式。下游收到违反此不变式的 payload = 上游 bug(序列化/重建须保持)。本系统产出时恒满足。**附:** `bIsDouble==true` 蕴含 `Total` 必为偶数(2/4/6/8/10/12)、`Total==7` 蕴含 `bIsDouble==false`——几何相关性,可作下游重建校验的强化不变式(非本系统强制)。
- **读档后当前 Seed**:MVP 读档**重设为新的非确定种子**(`Seed` 不续算)——注意**不可重设回开局种子**,否则读档后整条后续掷骰序列与开局可复现地重复(可被玩家/测试观测的统计异常)。当前回合骰结果由玩家回合(2)序列化完整 `FDiceRollResult`(含 Die1/Die2)保护、读档不重掷(见 States and Transitions + player-turn Edge Case)。Full Vision 重放须序列化当前 `Seed`——Open Question(OQ-2)。
- **RNG 流周期耗尽**:`FRandomStream`(LCG)周期极大,正常对局(数百次抽取)远不可达,无需处理。

## Dependencies

### 上游(本系统依赖的系统)
**无 —— 零依赖 Foundation 纯 RNG 服务。** 不读取任何其他系统的数据;先于或并行于所有消费方设计。

### 下游(依赖本系统的系统)

| 系统 | 关系 | 读/写内容 |
|---|---|---|
| 玩家回合(2) | 硬 | `RollPhase` 调 `RollDice()` 收**完整 `FDiceRollResult`(含 Die1/Die2)**:消费 Total+bIsDouble、**序列化全字段(B1)**;开局定序调 `RollDice()` 只取 `Total`(忽略 bIsDouble) |
| 移动(4) | 硬(间接) | 经回合(2)取 `Total` 推进步数(不直接调骰子) |
| 事件格(7) | 硬 | 掷双点出狱调 `RollDice()` 取 `bIsDouble`(出狱双点不进双点链,归7落实) |
| AI(10) | 硬 | 决策随机经 `RandomRange`/`RandomFloat01`;**禁旁路引擎全局 RNG**(软约束,见 CR-2/OQ);回链 player-turn OQ-T-3② |
| 拍卖(12)/命运之轮/俄罗斯轮盘(14) | 软(Alpha) | 经通用原语抽取(可种子化、可重放) |
| VFX(19) | 硬 | 监听 `OnDiceRolled(FDiceRollResult)` 读 `Die1`/`Die2`/`Total` 呈现两骰翻滚 + juice |
| HUD(16) | 软 | 可显示骰子结果数值(主要 juice 归 VFX19) |
| 存档(21) | 软 | MVP 不序列化当前 Seed(当前骰结果由回合2序列化完整 `FDiceRollResult` 含 Die1/Die2,见 B1);Full Vision 重放序列化当前 Seed `GetCurrentSeed()`(OQ-2) |

> **双向一致性待同步(Phase 5):** systems-index 当前骰子(#3)被依赖方向标注不全——player-turn(2,已确认 depends-on 3)与移动(4,depends 1,2,3)已列;但事件格(7)/AI(10) 经"掷双点出狱"与"决策随机走此 RNG"也实质依赖本系统,索引未标。Phase 5 复核 systems-index 下游依赖标注(7/10 应补对 3 的依赖)。

## Tuning Knobs

| 旋钮 | 作用 | 安全范围 | 过低/过高后果 |
|---|---|---|---|
| `InitialSeed` | 开局 RNG 种子(F-4 性质 D 的输入)。MVP:系统时间(每局不同)或固定值(测试 fixture);Full Vision:服务器下发以重放 | 任意 int32(`0` 合法) | 任意种子随机质量等价;固定值用于可复现测试,正常对局应每局不同 |
| `bUseFixedSeed` | 是否用固定种子(调试/测试)而非每局随机 | bool(默认 `false`=每局随机) | `true`=每局同序列(仅调试/录像用,正常对局禁用) |
| `DieFaces` | 单颗骰子面数 | 接口预留,**MVP 锁定 6**(经典忠实①) | MVP 不暴露;Alpha House Rules 若做异形骰再开,改值影响 F-1 PMF 与 [2,12] 范围 |
| `DiceCount` | 单次掷骰的骰子数 | 接口预留,**MVP 锁定 2**(标准双骰) | MVP 不暴露;改值破坏 `bIsDouble`/双点链语义与移动范围,须协同回合(2)/移动(4) |

> **数值来源说明:** `DieFaces=6`/`DiceCount=2` 来自经典大富翁标准双骰(支柱①桌游忠实),**MVP 不作可调旋钮**,仅以接口预留形态登记防硬编码。`InitialSeed`/`bUseFixedSeed` 是工程/测试旋钮,非平衡决策。本系统无任何平衡数值(随机分布由 F-1 数学固定)。

**旋钮间相互作用:** `DieFaces`/`DiceCount` 一旦在 Alpha 开放,会同时改变 F-1 PMF、`Total` 输出范围、`bIsDouble` 概率,**牵动移动(4) 步数范围与回合(2) 双点逻辑**——属跨系统旋钮,改动须三系统协同(故 MVP 锁定)。

## Visual/Audio Requirements

**n/a —— 本系统是数据/RNG 层,不渲染视觉、不触发音频。** 但掷骰是本作的**签名感官时刻**(支柱③运气刺激 + MDA Sensation),其呈现由 VFX(19)/音频(22) 拥有。本系统为其提供数据与事件,并钉一组**最低呈现契约**(具体数值/动画归 VFX19/audio22 GDD):

> **🎯 掷骰爽感的端到端 owner = VFX(19) GDD(design-review R1, B5 指派)。** game-concept 把 Sensation 列 priority-4、"掷骰的期待感"是 30 秒核心循环动作的**起点**——这是本作签名感官时刻。本系统(数据/RNG 层)**明确不拥有**掷骰 juice,但"掷骰爽感"这个 concept 钦定的 priority-4 aesthetic **必须有一份 GDD 端到端负责**,否则会重演本项目"标记事实却无载体=不可测真空"失效模式(掉落的是 aesthetic 而非数据字段)。**指派:VFX(19) GDD 须显式继承"掷骰爽感(期待→翻滚→定格节奏 + 双点强化反馈)"为目标,并为其背 experiential acceptance criteria。** 本系统的义务边界:提供权威结果 + `OnDiceRolled` 事件 + 结果在动画开始前已产出(本 GDD 已保证)。回链登记于 systems-index 继承义务节(VFX19 行)。

- **两骰面值精确呈现**:VFX(19) 须用 `Die1`/`Die2` **分别**呈现两颗物理骰子的面值,**不得只用 `Total` 随意拆解**——否则玩家看到的骰子面与逻辑结果不符(如 Total=4 画成 1+3 但逻辑是 2+2,穿帮)。这是 payload 携带单骰点数(而非仅 Total)的根本理由(见 Detailed Design 决策),也是 B1 序列化须含 Die1/Die2 的根本理由(读档后仍须忠实重现骰面)。
- **双点强化反馈 + 好/坏语境(design-review R1, rec-11 接缝声明)**:`bIsDouble==true` 是关键戏剧时刻,VFX/audio 应有区别于普通掷骰的强化反馈。**但 `bIsDouble` 是无语境裸布尔**——`OnDiceRolled` 不携带"这次双点是好(第1-2次=再走一回合)还是坏(第3次连续=直接入狱)"。**VFX 的强化反馈差异化(庆祝 vs 入狱警告)须从 player-turn 取语境(`ConsecutiveDoubles`),不可仅凭 `OnDiceRolled.bIsDouble` 决定反馈情绪**——否则第3次入狱双点会被误播成庆祝、期待管理错位。掷骰前的"已连续2次、再双点即入狱"风险提示亦归 player-turn→HUD 链,非本系统。本系统只提供 `bIsDouble` 原值。
- **定序双点须抑制强化反馈(design-review R1, B6)**:开局定序复用 `RollDice()` 但只取 `Total`、忽略 `bIsDouble`(CR-4)。**定序阶段掷出双骰相同(如 (4,4))时,VFX 须抑制双点强化反馈**——否则玩家在开局看到"双点庆祝"却无额外掷/无特殊效果,与正式回合的双点语义相反、制造新手困惑(撞支柱④)。**"当前是定序阶段"这一语境由 player-turn 的回合/阶段状态告知 VFX(定序阶段标志 / 定序专用呈现路径)**,本系统的 `OnDiceRolled` 不区分语境(定序与回合内是同一 payload,均带真实 `bIsDouble`)。语境裁决归 player-turn,VFX 据此抑制——本系统不改接口(CR-4"无需单骰模式"维持)。回链 OQ-T-Dice-1。
- **掷骰节奏**:期待(掷骰前)→翻滚(过程)→定格(结果)的节奏制造运气刺激;本系统在动画**开始前**即已产出权威结果(`RollDice()` 同步返回),呈现层动画只是回放既定结果,**不影响逻辑**(动画时长/跳过不改变骰点)。
- **事件钩子**:本系统广播 `OnDiceRolled(FDiceRollResult)` 供 VFX19/HUD16/audio22 挂接。以上要求登记于 Dependencies VFX(19) 行。

## UI Requirements

**n/a —— 本系统无任何 UI。** 它不绘制屏幕元素,只暴露 `RollDice()`/通用原语接口与 `OnDiceRolled` 事件。"掷骰"按钮/输入由 HUD(16)/输入层处理后调用本系统接口;骰子结果的数值显示由 HUD(16) 实现。本系统的"接口"是数据契约与事件,不是界面。

## Acceptance Criteria

> *由 `qa-lead` 校验(lean 模式 H 节派发,Section H 为高风险节)。**测试分层(对齐 F-5):** `[Logic]` 纯 fixture、headless、确定性、PR merge gate;`[Advisory]` 集成/code-review/统计检查,非门控。*
> **核心原则(本项目反复失效模式,player-turn 8 轮代价):统计/频率断言永不作 [Logic] gate(即便 n=36000+α=0.001,日构建仍偶发误触发)→ 一律 [Advisory]。确定性种子序列断言才是 [Logic]。**

### A. 核心规则(CR-1~5)
- **AC-1** [Logic] 固定种子调 `RollDice()` 一次:断言 `Die1∈[1,6]` ∧ `Die2∈[1,6]` ∧ `Total==Die1+Die2` ∧ `bIsDouble==(Die1==Die2)`(单次字段完整性 + payload 不变式)。
- **AC-2** [Logic] 固定种子连续 `RollDice()` N≥20 次:断言每次 `Total==Die1+Die2` ∧ `bIsDouble==(Die1==Die2)` 无一例外(不变式覆盖整序列,防单次碰巧)。
- **AC-3** [Advisory] n=36000 次:`bIsDouble` 频率 ∈ [0.155,0.178](约 ±4σ 保守带,宽于 α=0.001 的 ±3.29σ≈[0.160,0.173] 以防 flaky)、**chi-square(df=1)<10.83**(二分类:双点/非双点,df=2−1=1,α=0.001 临界≈10.828)。**非 [Logic] 因:频率断言、CI 偶发误触发(同 player-turn 历史问题)。(design-review R1 修:原 31.26 是 df=11 的临界值、误配二分类,应 10.83。)**
- **AC-4** [Advisory] n=36000 次:Total PMF chi-square(df=10)<29.59(α=0.001),expected[7]=6000/expected[2]=expected[12]=1000。**非 [Logic]:频率统计。**
- **AC-5** [Advisory] n=36000 次:单面均匀 chi-square(df=5)<20.52;两骰独立性(联合频率 vs 边缘乘积)chi-square(df=25)<52.62。**非 [Logic]:统计推断。**
- **AC-6** [Logic] 固定种子 `S` **新建流**直接展开两次 `RandRange(1,6)` 得 `expected_d1`/`expected_d2`;相同 `S` **重建同一新流**(同 `S`、同 0 起点、抽 Die1 前不得有任何额外抽取消耗 `Seed`)调 `RollDice()`:断言 `Die1==expected_d1` ∧ `Die2==expected_d2`(证明实现是两次独立顺序抽取,非"先取 Total 再拆解")。**(design-review R1 补 D-2 前提)** 此断言成立的前提是**两条路径从同一 `S` 同一起点出发、`RollDice` 在抽 Die1 前零额外抽取**——否则游标错位使比对失效;前提须在 fixture 构建时显式保证(新建流、不复用已用过的流)。
- **AC-7** [Logic] F-5 层一 fixture 序列中每次 `Die1`/`Die2` 均 ∈[1,6](CR-5 范围边界确定性覆盖;"均匀"属统计层 AC-3~5)。

### B. 确定性(F-4 性质 D)
- **AC-8** [Logic·核心 PR gate] 固定种子(如 12345)+ 固定有序调用序列 `[RollDice×5, RandomRange(0,5)×3, RandomFloat01×2, …]` 总抽取≥20;fixture 先运行打印真实序列、硬编码为 `expected[]`;两次冷启动运行均逐字段精确等于 `expected[]`。失败当且仅当 `FRandomStream` API 破坏性变更(可检测、有意义)。**这是所有 [Logic] gate 的骨干 fixture。**
- **AC-9** [Logic] seed=12345 与 seed=99999 各跑 `[RollDice×5]`,**两组序列各自固化为 `expected_12345[]`/`expected_99999[]` 常量**(同 AC-8 范式:先运行打印真实序列、硬编码),断言运行结果逐字段精确等于各自固化常量。**(design-review R1 修 D-1)** 不再用"两组不完全相同"作运行期断言——那是概率命题(理论存在极小撞种子概率,严格说非确定性)。"两组确不相同"的判断移到**测试编写期离线**完成(固化时人工确认两常量不同),运行期只做确定性的"等于固化常量"断言。本条同时覆盖"忽略种子退化"(两固化常量不同→实现若忽略种子则至少一组对不上)与确定性。

### C. RandomRange 边界契约(F-2)
- **AC-10** [Logic] 固定种子 `S`:路径 A=`[RollDice()]`;路径 B=`[RandomRange(5,5), RollDice()]`;断言 `seqA[0] == seqB[1]` 逐字段相等(证明 `Min==Max` 退化调用**未推进游标**)。
- **AC-11** [Logic] 调 `RandomRange(10,3)`(Min>Max):断言返回 `10`(=Min,**非 3、非随机**)+ ensure 触发(dev,UE Automation `AddExpectedError` 捕获)/shipping UE_LOG Warning(防实现者"好心"加 swap 掩盖 bug)。
- **AC-12** [Advisory] `RandomRange(1,6)` 闭区间边界 1 与 6 可达性。**降 [Advisory] 因:种子依赖性风险**(某种子前 N 次或未现边界);若固化 fixture 确认边界出现位置可升 [Logic]。
- **AC-12b** [Logic·范围不变式] **(design-review R1 新增,补 D-4 gate 缺口)** 固定种子,对多组 `(Min,Max)`(如 `(0,5)`/`(1,6)`/`(-3,3)`/`(10,10)`)各调 `RandomRange(Min,Max)` N≥20 次:断言**每次** `Min <= result <= Max` 无一例外。这是**确定性范围不变式**(非统计可达性、非均匀性),守"结果恒落闭区间内"——此前 AC 仅覆盖边界可达(Advisory AC-12)与 Min>Max/Min==Max 退化(AC-10/11),缺这条范围硬门。失败=封装/截断 bug。
- **AC-12c** [Logic] **(design-review R1 新增,补 D-7/F-3 回归守门;R2 声明收窄)** 固定种子,对**MVP 实际 N 范围**多组(如 2/6/12/100),计算 `floor(RandomFloat01()*N)` ≥100 次:断言结果恒 `∈ [0, N-1]`(永不返回 N)——守 F-3 半开区间设计理由(1.0 严格排除→小 N 不越界)。**R2 注:本条只覆盖 MVP N 范围(命运之轮 N=12 等,远小于 2²⁴);`N ≥ 2²⁴` 的浮点舍入越界(见 F-3 N 上界)在 MVP 不可达、无可测对象,守门登记 OQ-4(Alpha 大 N 路径开放时补 AC),不在本条声称已覆盖**(避免假安全感)。
- **AC-13** [Logic] 固定种子 `RandomFloat01()` ≥100 次:所有值 `0.0 <= f < 1.0`(1.0 严格排除)。

### D. 事件广播(OnDiceRolled)
- **AC-14** [Logic] 订阅 `OnDiceRolled`,固定种子调 `RollDice()`:断言事件 payload 四字段与同步返回值逐一相等。
- **AC-15** [Logic] 连续 `RollDice()` N=5 次:断言 `OnDiceRolled` 触发次数 ==5(精确,不多不少)。
- **AC-16** [Advisory] 下游订阅持引用后 `RollDice()`:DiceService 当前 `Seed` 恰滚动两步(经 `GetCurrentSeed()` 前后比对验证)、无其他字段被修改(需集成层验证)。
- **AC-16b** [Advisory·code-review] **(design-review R1 新增,守单线程重入约束)** 审查所有 `OnDiceRolled` 订阅者:无任一在回调内同步调 `RollDice`/`RandomRange`/`RandomFloat01`(订阅者纯呈现/只读)。BP 动态绑定不可静态扫描→code-review;若 OQ-1 选 C++ 强封装可加调用图静态检查升 [Logic]。
- **AC-16c** [Logic] **(design-review R1 新增,守广播/返回顺序)** 订阅 `OnDiceRolled`、回调内捕获 payload;`RollDice()` 返回后断言**返回值四字段 == 回调捕获的 payload 四字段**(证明返回的是广播前固化值、广播与返回同源,非广播后重读流)。

### E. 边界与错误处理
- **AC-17** [Logic] 不调 `SetSeed` 直接 `RollDice()`:shipping 不崩、返回值满足 AC-1 约束(lazy-init 兜底);dev ensure 触发。
- **AC-17b** [Logic] **(design-review R1 新增,守 B4 非确定兜底种子;R2 改确定性断言)** 不调 `SetSeed` 直接 `RollDice()` 触发 lazy-init 后:经公有 accessor(`GetInitialSeed()`/`GetCurrentSeed()`)断言 **lazy-init 实际写入的 `InitialSeed != 0`**(证明兜底种子取自非确定源 `FPlatformTime::Cycles()`、未退化为 `FRandomStream` 默认构造的固定 `0`)。**R2 修正(qa+CD):** 原断言"两次冷启动 `Total` 序列不完全相同"是**概率命题**(非确定种子理论撞种子概率≈0 但非 0、且依赖测试机时序分辨率),与 R1 刚修过的 AC-9 同模式缺陷、违反本项目核心原则"统计/频率断言永不作 [Logic] gate";改为**确定性单点断言** `InitialSeed != 0`(`Cycles()` 实际恒不返回 0 → 断言结果确定为真、零 flaky),直接覆盖 B4 意图。注:`0` 作**显式 SetSeed 传入**仍是合法种子(见 Edge Cases),本条只拦截 lazy-init 退化到默认构造 `0` 的路径。
- **AC-18** [Logic] `seed=0` 初始化调 `RollDice()`:Die1/Die2∈[1,6]、不变式成立(0 是合法种子、无特殊路径)。
- **AC-19** [Advisory] 代码审查 DiceService 无并发访问 `FRandomStream` 路径;持有方有单线程所有权标注(并发禁止是架构约束,无自动化 fixture)。

### F. CR-2 禁旁路(强度坦诚)
- **AC-20** [Advisory](BP-primary)/ **可升 [Logic] 若 OQ-1 选 C++ 强封装**:所有需可重放随机的系统(AI/Alpha)无直接调引擎全局 `FMath::Rand`/BP `Random Integer in Range`。**强度坦诚:** BP 节点不可静态扫描→只能 code-review([Advisory]);选 C++ 则可加禁用符号静态扫描升 [Logic] PR gate(与 player-turn AC-35a 同构,**待 OQ-1 决议**)。

### G. 接口稳定性(编译/反射检查)
- **AC-21** [Logic] `FDiceRollResult` 标 `USTRUCT(BlueprintType)`、字段 `UPROPERTY(BlueprintReadOnly)`、编译通过。
- **AC-22** [Logic] `OnDiceRolled` 标 `UPROPERTY(BlueprintAssignable)`、类型 `DYNAMIC_MULTICAST_DELEGATE`、编译通过。
- **AC-23** [Logic] `RollDice`/`RandomRange`/`RandomFloat01` 均标 `UFUNCTION(BlueprintCallable)`、BP 可调、编译通过。

> **[Logic] gate(BLOCKING PR):** AC-1/2/6/7/8/9/10/11/12b/12c/13/14/15/16c/17/17b/18/21/22/23(20 条)。**[Advisory]:** AC-3/4/5/12/16/16b/19/20(8 条)。
> **(design-review R1)** 新增 [Logic]:12b(RandomRange 范围不变式,补 gate 缺口)/12c(floor 不越界)/16c(广播=返回同源)/17b(lazy-init 非固定种子);新增 [Advisory]:16b(重入 code-review)。

### 跨系统测试义务(OQ-T 回链,归下游 GDD)
- **OQ-T-Dice-1 → player-turn(2,已确认):** "开局定序调 `RollDice()` 只取 `Total`、忽略 `bIsDouble`、不进双点链"是 player-turn 消费决策,归其 AC(本系统不含此逻辑)。**(R1 新增 B6)** 并须告知 VFX"当前=定序阶段"语境(定序阶段标志/专用呈现路径),使 VFX 抑制定序双点强化反馈——语境裁决归 player-turn。
- **OQ-T-Dice-2 → AI(10):** "AI 决策随机经此服务(RandomRange/RandomFloat01)、禁引擎全局 RNG"的集成测试/code-review,回链 CR-2/AC-20。**(R1)** 注:可重放路径慎用 `RandomFloat01`(浮点跨平台风险,见 F-4/OQ-3),AI 决策随机优先整数 `RandomRange`——但二者跨平台同源,Full Vision 联网须服务器权威。
- **OQ-T-Dice-3 → 事件格(7):** "掷双点出狱取 `bIsDouble`、出狱双点不进双点链"的语义归 7(本系统只提供 `bIsDouble` 原值、不解释出狱语境)。
- **OQ-T-Dice-4 → VFX(19):** "VFX 用 `Die1`/`Die2` 分别呈现两骰面值、不得只用 Total 拆解"的呈现侧测试,回链 Visual/Audio 契约(本系统只保证 payload 携带 Die1/Die2,AC-1/2 已覆盖)。**(R1 新增 B5+rec-11)** VFX(19) GDD 须**继承"掷骰爽感"端到端 owner** 并背 experiential AC;双点强化反馈差异化(庆祝 vs 入狱警告)须从 player-turn 取 `ConsecutiveDoubles` 语境,不可仅凭裸 `bIsDouble`。**(R2 强化回链约束力,CD 裁定:owner 指派非纸面承诺)** 本指派为**MUST-FULFILL 移交**:VFX(19) GDD 撰写时,"掷骰爽感端到端 owner + experiential AC"转为该文档的**强制承接义务**,qa-lead 在 VFX(19) design-review 时须核对其 AC 已含此承接(若 VFX(19) 未承接 = 该 GDD 的 blocking 缺口,非本骰子 GDD 的)。**本骰子 GDD(S 级纯 RNG 服务)义务边界止于:提供权威结果 + `OnDiceRolled` 事件 + 结果在动画开始前已产出——三者已兑现,不背呈现层 AC**(R2 专家分歧:game-designer 主张本档加呈现层硬钩子,CD+qa+unreal 裁定越界、owner 已正确指派 VFX19,用户采纳 CD 裁定)。
- **OQ-T-Dice-5 → player-turn(2)/存档(21):** **(R1 新增 B1)** player-turn AC-34 序列化字段须从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`(含 `Die1`/`Die2`),否则读档后 VFX 无法重现骰面。player-turn 已 APPROVED → 此为 design-change,须 `/propagate-design-change` 评估对其 AC-34 的影响。**✅ propagate 状态:RESOLVED(2026-06-02,IG-1 已执行)** —— player-turn 7 处(L110/180/195 RECEIVE 契约 + L338/369/468/471 SERIALIZE)已从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`(含 Die1/Die2);AC-33/AC-34 已含 Die1/Die2、门控分级不变;B1 整条论证链已跨文档闭合。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-1(dice)⚠ ADR** | `DiceService` 的 UE 宿主形态。**(design-review R1, unreal-specialist)** **`UBlueprintFunctionLibrary` 已从候选删除**——它是无状态静态函数集,**不能持实例 `FRandomStream`**(CR-2 要求唯一权威流);强做要全局 `static` → PIE 多实例共享污染、种子注入无主、对局边界错位。**剩余真候选:`UWorldSubsystem`(倾向:引擎保证单例 + World=一局边界天然对齐 + PIE 隔离 + `OnWorldBeginPlay`/显式 `SetSeed` 注入点)/ `UGameInstanceSubsystem`(跨局存活、对局边界须手动 reset、次选)/ Component(唯一性靠纪律、不推荐)。** **ADR 硬约束:DiceService 与 player-turn 状态容器必须落同一生命周期层**(否则 Full Vision 序列化 `Seed` 存活域与消费方存活域错位=重放 bug 温床)。**与 CR-2 强度联动**:选 C++ 强封装 → AC-20 可升 `[Logic]` 静态扫描;BP 约定 → 软约束 code-review。**联网注:** `UWorldSubsystem` 在专用服务器/客户端下服务器与客户端各有 World/流,ADR 须标注"服务器权威 RNG / 客户端只收 RPC 结果"语义(见 F-4 跨平台 + OQ-3)。**(R2 unreal-specialist 补注,ADR 须纳入)** ① **Listen Server 陷阱**:Listen Server 模式下服务器与客户端**共享同一 WorldSubsystem 实例**,"各有独立 World/流"的隔离假设失效→权威性须靠 `HasAuthority()`/NetMode check 判定,非靠实例隔离;② **初始化顺序**:`USubsystemCollection` 按类名而非依赖图初始化,不保证 DiceService 先于 player-turn subsystem `Initialize()`→`SetSeed` 调用时机须锚 `OnWorldBeginPlay` 或更晚,**禁在 `Initialize()` 阶段跨 subsystem 依赖**;③ **热重载陷阱(开发期)**:C++ 热重载后 `UPROPERTY FRandomStream` 成员可能重置为 CDO 默认(Seed=0),测试序列静默失效易误判 RNG bug→开发期热重载后须重新 `SetSeed`(不影响 shipping)——此条进一步支持 UWorldSubsystem 首选(随 World 重建、PIE Stop→Play 自动重置)。 | **下游 #4/AI #10 开工前** `/architecture-decision`(与 player-turn OQ-1 协调) |
| **OQ-2(dice)** | 当前 `Seed` 序列化(读档确定性续算):MVP 读档**重设为新非确定种子**(不续 `Seed`、不可重设回开局种子,见 Edge Cases),当前回合骰结果由玩家回合(2)序列化完整 `FDiceRollResult`(含 Die1/Die2)保护、不重掷。Full Vision 重放(OQ-3)须序列化当前 `Seed`(经 `GetCurrentSeed()` 显式存,勿靠 struct 反射,见 UE5.7 标注)以续算后续抽取。 | 联网(25)/存档(21) Full Vision;MVP 记开口不展开 |
| **OQ-3(dice)⚠ 升级** | **重放确定性(含跨平台浮点 + 多线程)。** ① 多线程:性质 D 前提②要求单线程,若 Full Vision 引入后台线程需随机(如异步 AI 预演),须用独立 `FRandomStream`、不共享此权威流。② **(design-review R1 新增)跨平台浮点位级一致**:`RandRange` 内部走 `FRand()` 浮点,服务器/客户端各自跑流时输出映射在浮点截断边界可能 off-by-one→desync(见 F-4)。**正解:服务器单一权威 RNG、客户端只收 RPC 下发 `FDiceRollResult`(绕开客户端浮点);或自实现纯整数 LCG。** **(R2 unreal-specialist 补注)** ③ **跨编译配置**:off-by-one 风险不限跨平台——**同平台 Debug vs Shipping**(`/fp:precise` vs `/fp:fast`)亦可在 `FRand()*Range` 截断边界分歧,Full Vision 重放验证须在 **Shipping 配置**下做;④ **自实现 LCG 选参**:若走"纯整数 LCG"正解,须用**已知良好参数**(如 Knuth 乘法 LCG / Xoshiro256)并经 F-5 层二 chi-square 验证通过后方可替换 `FRandomStream`,**禁随手搓未验证参数**(周期/统计质量不达标)。 | 架构/联网阶段;MVP 单机不触发 |
| **OQ-4(dice)** | Alpha 异形骰/加权骰(House Rules):`DieFaces`/`DiceCount` 接口预留但 MVP 锁定 6/2(支柱①)。Alpha 若开放须协同移动(4) 步数范围、回合(2) 双点逻辑、F-1 PMF。**(design-review R1)`DiceCount>2` 时 `bIsDouble=(Die1==Die2)` 二元定义失效,须重定义(三骰全同?任两同?)——属接口预留旋钮的语义债,Alpha 开放前须协同回合(2)裁定。** 加权骰(非均匀)会触碰支柱③公平性前提(Player Fantasy),须 CD 复核。 | Alpha House Rules(23);MVP 不做 |
| **OQ-5(dice)** | **(design-review R1 新增,game-designer;R2 补调查项+锚点)** anti-tilt 方差压缩(**保期望、降方差**,如连续远离 7 后微调下一掷方差)作为缓解 concept Design Risk("运气占比过高让偏策略玩家挫败"+"后期翻盘无望拖沓")的候选——**与 OQ-4 加权骰(改期望)是不同性质**(平滑保持长期 PMF 三角分布、不破坏支柱①统计忠实门面)。CR-5 当前真随机是 MVP 可接受起点(支柱①正当),但此设计空间不应在 CR-5 一句关死;登记待 playtest 数据决定,须 CD 复核与支柱①忠实权衡。**(R2)决策前置两项:① 调查原作 Rento Fortune 是否含伪随机平滑——若有,则"真随机=支柱①忠实"论证须修正(原版体验本身已平滑);② playtest 须采集的具体锚点指标(如"连续 N 次掷出 ≤4 的频率 vs 玩家自评挫败率"),否则数据收集方向盲目。支柱①(统计真实)vs 支柱③(减挫败)权衡:MVP 优先①,若 playtest 建议方差压缩须 CD 复核并记为支柱①的有限例外。** | playtest 后;MVP 真随机不做 |
| **OQ-T-Dice-1~5** 跨系统追踪 | 见 Acceptance Criteria 末"跨系统测试义务"——回链 player-turn(2)/AI(10)/事件格(7)/VFX(19),下游 GDD 审查时 qa-lead 核对。**(R1 新增 OQ-T-Dice-5):player-turn(2) AC-34 序列化字段须从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`(含 Die1/Die2),否则读档后 VFX 无法重现骰面(B1)——player-turn 已 APPROVED,此为 design-change 须 propagate。✅ propagate 状态:RESOLVED(2026-06-02,IG-1 已执行——player-turn 7 处 RECEIVE+SERIALIZE 契约已扩为完整 FDiceRollResult)。** | 各下游 GDD;审查时核对 |
