# 事件格 (Tile Events)

> **Status**: ✅ **APPROVED-with-followups**(R2 re-review full,2026-06-03,`CD-GDD-ALIGN: APPROVE`)。首评 MAJOR REVISION NEEDED → R1 改 8 blocker + AC-craft → **R2(systems/qa + CD)12 首评 BLOCKING 全 CLOSED + L110 单 cell must-fix 已修**。followup(不阻下游):player-turn 命名监狱态受控写接口(REC propagate)、Player Fantasy 结构重构/保释金诚实标注(deferred REC)、OQ-Event-3/6/7。详见 review-log。
> **Author**: msc + agents
> **Last Updated**: 2026-06-03
> **Implements Pillar**: ①桌游忠实(机会/命运/税/监狱经典还原)、③运气×策略(抽牌=运气尖峰)、②社交博弈(收每位玩家钱类卡)
> **System**: #7 in systems-index design order | Core | Gameplay | MVP
> **Review Mode**: lean

> **R1 修订摘要(2026-06-03,首评后)**:① **OQ-Event-1 监狱态双写**(确证真矛盾,以 player-turn 已 Approved 模型为准)——字段存 player-turn PlayerState、player-turn 计数、7 拥规则+受控写;字段名 `bInJail`→`bIsInJail`;CR-3/5/7/9/States/Interactions/AC-37/40/45/52 全对齐;残留 player-turn 命名受控写接口(低成本 propagate)。② **B-12** 补 AC-62/63(DecideJailAction 非法降级,承接 player-turn CR-8#4/AC-39b)。③ **虚假引用**"economy-designer 校验 2~2.5:1"→"待平衡 pass"。④ **OQ-Event-4** 自述矛盾收口(逐笔足够+重表零和,降说明性 note)。⑤ **单 bool 出狱卡**→按堆 `bHoldsChanceOutCard`/`bHoldsChestOutCard`。⑥ CR-9 掷骰时序明确。⑦ 枚举 `EJailReason`/`EJailReleaseMethod` 进正文(uint8)。⑧ 牌堆 model B(无指针)+ Fisher-Yates 种子序列化。+ AC-craft 簇(AC-29 不可能 fixture 重写/AC-37 反同义反复/AC-51 永真删/AC-58 UHT 零警告)。OQ-Event-2 CLOSED、OQ-Event-5 RESOLVED、OQ-Event-7(engine ADR)新增。**deferred**:Player Fantasy 结构重构 + 保释金 50 退化/jail-as-safe-haven 诚实标注(REC,本轮未做,留下一 pass)。

## Overview

事件格系统是《骰子大亨》的**非地产格结算层与监狱状态机**:棋盘上不可购买的特殊格——机会(Chance)、命运(CommunityChest)、税格(Tax)、去坐牢(GoToJail)、监狱(Jail)——落地后该发生什么,由本系统拥有并执行。它有两个面。对内是一个**落地事件路由器 + 监狱裁决层**:回合(2)的 `ResolvePhase` 把一次非地产落地交给本系统,本系统据棋盘(1)`GetTileData(index)` 的 `TileType`/`EventDeck`/`TaxAmount`/`SpecialAction` 决定走哪条结算——抽牌(从机会/命运牌堆抽一张并执行其效果)、缴税(读 `TaxAmount` 调经济 `Debit`)、强制入狱(调移动 `TeleportTo(JailIndex, paysGo=false)`),以及玩家身处监狱时每回合的出狱裁决(掷双点脱身 / 付保释金 / 用出狱卡 / 第三回合强制缴费出狱)。对玩家则是**运气尖峰与戏剧反转的载体**:抽一张牌的赌博心跳、被关进监狱眼看对手扩张的焦灼、一张"向每位玩家收钱"的卡瞬间反转同桌局势(支柱①桌游忠实 / ③运气×策略 / ②社交博弈)。它**不拥有**:现金数值与转移(归经济5,本系统只调 `Credit`/`Debit`)、棋子移动(归移动4,本系统只调 `TeleportTo`/触发位移)、骰子随机(归骰子3)、回合流程与"3 次连续双点入狱"触发(归回合2 CR-4)、格子静态数据与牌堆定义(归棋盘1);⚠ 也**不拥有起点 GO 发薪**——GO 过路费由 `passed_go` 经移动→经济 F-1 结算,本系统对落在 GO 不做二次发薪(board-data CR-2 `CollectSalary` 仅语义标记)。它只拥有**"落在这格 / 在监狱里,该触发哪个事件、按什么规则裁决"这组事件结算事实**。它做得好时,机会牌的惊喜、监狱的紧张、税单的肉痛都准确而及时地发生;没有它,棋盘就只剩买地收租,运气的刺激、桌游的经典记忆点(抽到"直接入狱"、"银行付你股息")与翻盘的戏剧性全部消失。

## Player Fantasy

事件格兑现的是 concept 核心幻想里**"运气的手伸进牌桌"**那一下——棋盘不再只是冷静的买地算计,命运突然介入,制造同桌一起惊呼或哀嚎的派对时刻(concept 点名的 Mario Party 式戏剧性 + 经典大富翁的记忆点)。它服务两层体验:

- **被动层(命运的重量)**:大多数事件是运气强加于你的——落在机会格抽到什么、踩中税格要交多少,你说了不算。这正是支柱③"运气×策略"里**纯运气的那一极**:抽牌前一瞬的心跳(会是"银行付你股息 200"还是"每栋房子缴修缮费"?)、翻到"直接进监狱"的集体哀嚎、抽到"向每位玩家收 50"时对手们的同步肉痛(支柱②社交博弈的燃料)。这些情绪的**重量**来自事件结果真实改变了局势——钱真的转走了、人真的被关了。
- **直接层(监狱里的那一个选择)**:事件格里唯一交还给玩家的能动时刻是**监狱**。被关进去后,要不要赌一把掷双点免费脱身(运气)、还是稳妥付保释金离场(算计)、还是亮出一直攒着的"出狱卡"(策略)——这是落后者搏命、领先者止损的小型决策,是支柱③里"对抗运气的掌控手段"在 MVP 的一个落点。

> **〔enable-not-own 体验契约〕**:本系统是**事件结算层**,产出正确的事件结果与时机,但不渲染任何呈现。翻牌动画、金币飞溅、"叮"的抽卡音、入狱铁门声由 VFX(19)/音频(22)/卡牌 UI(17) **拥有(own)**;本系统**使能(enable)**它们——发出"抽到了哪张牌、扣了多少税、谁进了监狱"的事实与事件,呈现层据此回放。

> **〔MVP 范围诚实标注〕**:concept 把命运之轮、俄罗斯轮盘、策略卡列为更强的运气尖峰,**全在 Alpha**。MVP 事件格交付的是经典大富翁的**基础运气层**——机会/命运牌、税格、监狱。"赌一把高风险机制翻盘"的完整戏剧性待 Alpha 的命运之轮(13)/俄罗斯轮盘(14)补全;MVP 验证的是基础事件循环本身就够制造同桌惊呼。

> **MDA 锚点**:本系统主要服务 **Fellowship/社交**(收每位玩家钱、集体哀嚎的互坑燃料)与 **Sensation/惊喜**(抽牌的运气尖峰),并驱动 concept 的"派对戏剧性"Dynamics。`Sensation` 的 juice(翻牌/金币/入狱演出)由 VFX(19)/音频(22)/UI(17) 承载,非本系统。

## Detailed Design

### Core Rules

**CR-1 事件结算入口(7 拥有非地产落地结算)。** 回合(2)`ResolvePhase` 在落地后,若 `TileType ∈ {Chance, CommunityChest, Tax, GoToJail, Go, Jail, FreeParking}`(非可购买格),委派单一入口 `ResolveTileEvent(playerId, tileIndex, arrivalContext)`。本系统据棋盘(1)`GetTileData(tileIndex)` 路由。可购买格(Property/Railroad/Utility)不归本系统(收租归所有权6/经济5)。**`arrivalContext==SentToJail` 时 ResolvePhase 不调本系统**(movement L147-149 / player-turn AC-46:去坐牢落地抑制所有结算);事件结算只在 `arrivalContext ∈ {DiceMove, AdvanceCard}` 触发。

**CR-2 事件路由表(据 TileType / SpecialAction)。**
- `Chance` / `CommunityChest` → 抽牌(CR-3),牌堆由 `GetTileData.EventDeck` 指定。
- `Tax` → 缴税(CR-6):读 `GetTileData.TaxAmount`,调经济 `Debit`。
- `GoToJail`(`SpecialAction==GoToJail`)→ 入狱(CR-7)。
- `Go` → **no-op**:发薪由 `passed_go` 经移动→经济 F-1 结算,本系统**不二次发薪**(board-data CR-2 `CollectSalary` 仅语义标记)。
- `Jail`(落地=探视)→ no-op(仅探视,不入狱)。
- `FreeParking` → no-op(MVP;House Rule 奖池 Alpha,经 `SpecialAction==TriggerHouseRuleCheck` 预留位,本系统 MVP 不实现)。

**CR-3 抽牌(7 拥有牌堆与抽牌)。** 本系统拥有两个牌堆 Chance/CommunityChest,各 16 张(`CHANCE_DECK_SIZE`/`CHEST_DECK_SIZE`,tuning)。抽牌 = 抽对应牌堆顶张 → 执行其效果(CR-4)→ **除出狱卡外用后回到牌堆底**(循环洗牌制,经典忠实:有限牌池循环、走完一轮才再现同张)。
> **容器模型(unreal Issue-2,采 model B 移动末尾制)**:每堆为有序 `TArray<FCardData>`;抽牌 = 取首元素(index 0)→ 执行 → 非出狱卡 append 回尾、出狱卡移出不 append(CR-5)。**无独立"抽牌指针"**——牌序即数组顺序,存档(21)只序列化数组顺序即可精确重现(`FCardData` 须标 `UPROPERTY(SaveGame)`);出狱卡离堆期 `Num()` 减1、回堆后恢复(AC-11/12/13)。
> **洗牌(unreal Issue-5)**:开局用骰子(3)`FRandomStream` + **Fisher-Yates**;种子在定序后首回合开始时确定、随存档序列化(可重放;两堆各洗一次);**禁引擎全局随机** `FMath::Rand`。

**CR-4 牌效果类型(完整枚举 `ECardEffectType`)。** 每张牌 = 一个 `ECardEffectType` + 参数:

| 效果类型 | 语义 | 经谁执行 | 代表牌例 |
|---|---|---|---|
| `BankCredit` | 银行付玩家固定额 | 经济 `Credit(playerId, amount)` | "银行付你股息 50" |
| `BankDebit` | 玩家付银行固定额 | 经济 `Debit(playerId, amount)` | "缴学费 150" |
| `CollectFromEach` | 向其他每位在局玩家各收 amount | `Debit(each other)` + `Credit(self, Σ)` | "生日,每人付你 10" |
| `PayToEach` | 向其他每位在局玩家各付 amount | `Debit(self, Σ)` + `Credit(each)` | "付每位玩家 50" |
| `MoveToTile` | 移动到指定 TileIndex | 移动 `TeleportTo(target, paysGo, AdvanceCard)` | "前进到起点 GO"(paysGo=true) |
| `MoveRelative` | 前进/后退 N 格 | 移动 `Advance`(过 GO 计 passedGo)/ 后退直达 | "后退 3 格" |
| `MoveToNearest` | 前进到最近 Railroad/Utility | `TeleportTo(paysGo=true)` + Utility 额外掷骰(CR-8) | "前进到最近公用" |
| `GoToJailCard` | 直接入狱 | 同 CR-7 | "直接进监狱,不过 GO" |
| `GetOutOfJailFree` | 获得出狱卡(持有) | 本系统记 `bHoldsChanceOutCard`/`bHoldsChestOutCard`(按堆,CR-5) | "出狱卡,可保留" |
| `RepairFee` | 每房屋/酒店缴固定额 | 读建房(8) `GetTotalHouseCount`/`GetTotalHotelCount` × 单价 → 经济 `Debit` | "每栋房 25、每酒店 100" |

> ✅ **`RepairFee` 依赖(OQ-Event-3 RESOLVED 2026-06-04)**:依赖建房(8)的全盘聚合 `GetTotalHouseCount`(全盘普通房,档1-4)/`GetTotalHotelCount`(全盘酒店,档5)。建房(8) GDD F-7 已暴露此两具名接口,**与 per-tile `house_count[0,5]` 口径区分**(酒店折档5,聚合处房/酒店分开计;防静默混用)。其余 9 类无此依赖。
> **金钱效果一律经经济**:所有 `Bank*`/`*Each`/`RepairFee` 现金转移由本系统调经济 `Credit`/`Debit` 执行(economy L108"机会/命运卡金钱效果经本系统";本系统不持现金)。**破产触发(付不起)归经济/破产(9)**,本系统只发起 Debit、不裁决破产。

**CR-5 出狱卡持有(7 拥有持有状态,按牌堆区分)。** 每堆各 1 张出狱卡(经典:Chance 1 + Chest 1),玩家**可能同时持两张**——故按堆分两个标记 `bHoldsChanceOutCard`/`bHoldsChestOutCard`(单 bool 无法区分出狱时还哪堆,systems F-6c)。抽到 Chance 堆的 `GetOutOfJailFree` → 置 `bHoldsChanceOutCard=true`、该牌离开 Chance 堆(不回底直到被用);Chest 同理。玩家在监狱消费出狱(CR-9)→ 清对应标记 + 牌回**该堆**底。**派生谓词** `HoldsAnyOutCard := bHoldsChanceOutCard ∨ bHoldsChestOutCard`(供 JC-2"可用出狱卡"判定;若同持两张,用卡时择一)。两标记均为本系统(7)**自有状态**(不在 player-turn PlayerState),随存档(21)序列化。MVP 不支持出狱卡交易(交易归11,Alpha)。

**CR-6 缴税。** 落 `Tax` 格:读 `GetTileData.TaxAmount`(经典:所得税格=200、奢侈税格=100,值归棋盘1/经济F-7),调经济 `Debit(playerId, TaxAmount)`。MVP 所得税取 flat(economy F-7 已定 flat;"按净资产% vs 固定额二选一"若 Rento 支持归 OQ-Event-2)。税款去向(银行 sink vs 奖池)归经济(economy F-7 = 银行 sink)。

**CR-7 强制入狱(三来源;7 拥有入狱规则,监狱态字段存于 player-turn)。** 三个入狱来源:① 落 `GoToJail` 格;② 抽到 `GoToJailCard`;③ 回合(2)CR-4 检测第 3 次连续双点。①② 由本系统执行入狱转换:调移动 `TeleportTo(playerId, JailIndex, paysGo=false, context=SentToJail)`(直达不过 GO 不发薪)+ 经 player-turn **受控写接口**(类比移动经 `SetPosition` 受控写,player-turn 边界契约 L89)置 `bIsInJail=true`、`JailTurnsServed=0`。**③ 的双点计数与入狱触发归回合2**(player-turn CR-4 拥有 `ConsecutiveDoubles` 与阈值);入狱转换经本系统统一入口 `SendToJail(playerId)` 驱动。
> **⚠ 所有权裁定(OQ-Event-1 RESOLVED 2026-06-03,以 player-turn 已 Approved 模型为准)**:监狱态字段 `bIsInJail`/`JailTurnsServed` **物理存于 player-turn 中心 PlayerState**(本系统**不自存**,消除双源);本系统(7)拥有**入/出狱规则与转换权威**,经 player-turn 受控写接口改写 `bIsInJail`;`JailTurnsServed` 的**机械计数由 player-turn 据本系统裁决结果执行**(player-turn L85"本系统计数、7 定规则" / L200"bIsInJail 改写归7、本系统持字段+服刑计数")。**残留 propagate(producer)**:player-turn 须为监狱态显式命名受控写接口(现 L89 只举 `SetPosition` 例);本档不再声称"7 拥有/直写监狱态"。出狱卡持有态 `bHolds*OutCard`(CR-5)**不在** player-turn PlayerState 字段表,由本系统(7)自有并序列化。

**CR-8 "前进到最近X"额外掷骰(MoveToNearest,economy OQ-T-Econ-3)。** 到最近 Utility:① 调骰子(3)`RollDice()` 取新 `Total`;② 调移动 `TeleportTo(nearestUtilityIndex, paysGo=true, AdvanceCard)`;③ 把新 `Total` 作 `dice_total` **注入**经济 F-4 算租(economy CR-3/F-4 显式参数;player-turn CR-3.1:事件额外掷骰的 dice_total 由事件格7注入)。到最近 Railroad 按经典可能收 2× 租(归经济 Railroad 租,本系统只负责移动 + 触发结算)。`paysGo=true`:越过 GO 照常发薪(移动 AdvanceIndex 计 passedGo)。

**CR-9 监狱出狱裁决(7 拥有规则;经典忠实 + Rento 待核查 OQ-Event-6)。** 玩家 `bIsInJail==true` 时,回合(2)在其 RollPhase 前把出狱决策点交本系统(player-turn CR-3 JailTurn 分支"规则归7";AI 经 player-turn CR-8 `DecideJailAction` hook)。**掷骰时序(消 B-6 歧义)**:玩家先在三选项中择一——选项 1/2 不需掷骰即决出狱,出狱后由回合2 正常 RollPhase 掷骰移动;选项 3 的掷骰**即**出狱判定本身,其 `Total` 兼作(若出狱)本回合移动步数。三选项:
1. **用出狱卡**(若持有任一 `bHoldsChanceOutCard`/`bHoldsChestOutCard`,CR-5)→ 7 裁定出狱、该卡回**对应**堆底、经 player-turn 受控写置 `bIsInJail=false`;回合2 正常掷骰移动。
2. **付保释金** `JAIL_BAIL_AMOUNT`(经典 50)→ 经济 `Debit(playerId, 50)` 成功后经受控写置 `bIsInJail=false`,回合2 正常掷骰移动。
3. **掷骰碰运气**:掷骰(此即本回合骰),`bIsDouble==true` → 7 裁定免费出狱、用该骰 `Total` 移动(**此双点不进双点链、不给额外回合**);非双点 → 7 裁定留狱,**player-turn 计 `JailTurnsServed+1`**(本系统不自增,CR-7 所有权裁定)、回合结束。
4. **第 3 在狱回合(`JailTurnsServed==2`)选项3 掷出非双点 → 强制付保释 50**(经济 `Debit`;付不起 → 破产9 流程)后用该骰 `Total` 移动出狱。`MAX_JAIL_TURNS=3`。

> **非法决策降级(承接 player-turn CR-8 #4 / AC-39b,B-12)**:`DecideJailAction` 返回 `PayBail` 但 `Cash<50` / `UseCard` 但无任何出狱卡 → 本系统(7)**校验合法性**,不可行则**降级留狱**(player-turn 计 `JailTurnsServed+1`)+ dev `ensure`+log,不扣成负现金、不凭空消卡(见 AC-62/63)。`RollDouble` 后续链见下方双点链契约。
> **出狱双点不进双点链(关键,player-turn OQ-T-1 / dice OQ-T-Dice-3)**:本系统消费 `bIsDouble` 仅判"是否免费出狱",**不向回合2 发任何双点链增量(`ConsecutiveDoubles`)或额外回合信号**;骰子(3)只提供 `bIsDouble` 原值,出狱语境由本系统解释。

### States and Transitions

**监狱状态机(每玩家)**:

| 状态 | 含义 | 转换 |
|---|---|---|
| **NotInJail** | `bIsInJail==false` | 落 GoToJail / 抽 GoToJailCard / 回合2 第3次双点 → `SendToJail()` → **InJail**(`JailTurnsServed=0`) |
| **InJail** | `bIsInJail==true`,`JailTurnsServed∈{0,1,2}` | ① 出狱卡/付保释/掷双点成功 → **NotInJail**(出狱当回合行动);② 非双点且 `JailTurnsServed<2` → 留 **InJail**(7 裁定留狱,**player-turn 计 `JailTurnsServed+1`**,CR-7);③ `JailTurnsServed==2` 非双点 → 强制付保释 → **NotInJail** |

> **关键不变式**:① `JailTurnsServed ∈ [0, MAX_JAIL_TURNS-1]`(==2 时下次非双点必强制出狱,无 served==3 留狱态);**计数权威单一=player-turn**(本系统裁定留狱/出狱结果,player-turn 据此机械 `+1`,杜绝双增,CR-7/CR-9);② 出狱(任何途径)后 `bIsInJail==false`(经 player-turn 受控写);③ 出狱卡持有(`bHoldsChanceOutCard`/`bHoldsChestOutCard`)与 `bIsInJail` 独立(可狱外持卡)。
> **程间非重入(承接 player-turn CR-3.1 / AC-47;UE 实现 unreal Issue-1)**:`MoveToNearest`/`MoveToTile`/`MoveRelative` 触发的新位移严格串行——前程 `OnPawnLanded` 同步返回前不开下一程。**UE 同步 `Broadcast()` 下二次 `TeleportTo` 在落地回调内同步调用=同栈重入**,故本系统**不在 `OnPawnLanded` 回调内直接发起二次位移**,而是**入 deferred 队列**待前程调用栈返回后处理(或置 `bIsResolvingTileEvent` 守门拒绝重入;具体机制归 OQ-Event-7 ADR)。卡触发的二次移动落地后,其落地格事件由该次落地的 ResolvePhase 正常处理。
> **确定性 + 序列化**:洗牌见 CR-3(`FRandomStream` Fisher-Yates + 种子序列化);存档(21)序列化**牌堆数组顺序(model B,无指针)+ 7 自有 `bHoldsChanceOutCard`/`bHoldsChestOutCard`**;`bIsInJail`/`JailTurnsServed` 存于 player-turn PlayerState、由 player-turn 序列化(CR-7,本系统不重复存)。

### Interactions with Other Systems

| 系统 | 数据流 | 谁拥有 |
|---|---|---|
| 棋盘(1) | 读 `GetTileData(index)` 的 `TileType`/`EventDeck`/`TaxAmount`/`SpecialAction`/`JailIndex` 路由事件 | 棋盘供静态格数据/牌堆指向;本系统拥有结算 |
| 玩家回合(2) | `ResolvePhase` 调 `ResolveTileEvent`(非地产落地);JailTurn 分支调本系统出狱裁决(`DecideJailAction` 合法性校验+非法降级归7,CR-9);回合2 第3次双点经 `SendToJail()` 驱动入狱转换;**监狱态字段 `bIsInJail`/`JailTurnsServed` 存于 player-turn PlayerState、player-turn 据7裁决机械计数;本系统经受控写改 `bIsInJail`、拥有留狱/出狱裁决**(OQ-Event-1 以 player-turn 为准) | 回合拥有流程/双点计数 + 监狱态字段存储;本系统拥有事件结算 + 入/出狱规则 |
| 骰子(3) | 调 `RollDice()` 取 `Total`(MoveToNearest 额外掷骰)/ `bIsDouble`(出狱判定) | 骰子拥有 RNG;本系统解释出狱语境 |
| 移动(4) | 调 `TeleportTo(target, paysGo, context)`(入狱/前进到X)/ 触发 `Advance`(相对移动) | 移动拥有位移;本系统发起 |
| 经济(5) | 调 `Credit`/`Debit`(牌金钱效果/缴税/保释金);MoveToNearest Utility 注入 `dice_total` 给 F-4 | 经济拥有现金与公式;本系统发起转移 |
| 建房(8) | `RepairFee` 牌读 `GetTotalHouseCount`/`GetTotalHotelCount` 算额(建房8 F-7,OQ-Event-3 ✅ RESOLVED) | 8 拥有建筑态与聚合接口;本系统消费 |
| 破产(9) | 缴税/保释/付牌款 Debit 致现金不足 → 9 裁决(本系统只发起 Debit) | 9 拥有破产裁决 |
| AI(10) | 读监狱态 `bIsInJail`(player-turn)/`HoldsAnyOutCard`(7)做出狱决策估值 | AI 拥有决策;本系统供态 |
| 命运之轮(13)/策略卡(15)(Alpha) | 经事件触发 / 扩展牌效果类型 | 13/15 拥有;本系统 MVP 不实现 |
| 卡牌UI(17)/VFX(19)/音频(22) | 订阅 `OnCardDrawn`/`OnTileEventResolved`/`OnPlayerJailed`/`OnPlayerReleased` 呈现 | 呈现层拥有 |
| 存档(21) | 序列化**牌堆数组顺序(model B,无指针)+ 7 自有 `bHoldsChanceOutCard`/`bHoldsChestOutCard`**;`bIsInJail`/`JailTurnsServed` 由 player-turn 序列化(存其 PlayerState,CR-7,本系统不重复存) | 存档拥有序列化 |

**事件(本系统广播,`DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`,payload `USTRUCT(BlueprintType)`;所有 enum 字段以 `UENUM(BlueprintType) : uint8` 为基底、struct 成员均标 `UPROPERTY()`,unreal Issue-3)**:`OnCardDrawn(playerId, EEventDeck deck, cardId)`、`OnTileEventResolved(playerId, tileIndex, ECardEffectType effectType)`、`OnPlayerJailed(playerId, EJailReason reason)`、`OnPlayerReleased(playerId, EJailReleaseMethod releaseMethod)`。供 UI/VFX/音频/AI 挂接。
> **枚举定义(本系统拥有,正式定义于此供 AC-56/57 引用,非 AC 首现,B-8)**:`EJailReason : uint8 { TileEffect, CardEffect, ConsecutiveDoubles }`(对应 CR-7 三入狱来源:落 GoToJail 格 / 抽 GoToJailCard / 回合2 第3次双点);`EJailReleaseMethod : uint8 { Card, BailPaid, DoubleDice, ForcedBail }`(对应 CR-9 四出狱路径)。

## Formulas

> 本系统**无平衡数值公式**——牌面 `amount`、税额、保释金是内容/数据(归棋盘1 CardData / 经济5 / Tuning),不是公式。本节只收录**确定性聚合/搜索/判定逻辑**。`Bank*` 的经济性质锁定:`BankCredit`=faucet(凭空注入)、`BankDebit`/`Tax`=sink(蒸发);`CollectFromEach`/`PayToEach`=**零和玩家间转移**(不经银行、不改货币总量)。faucet:sink 比例是数据/平衡 pass 事(经典约 2~2.5:1 为**起草期 economy-designer 建议值、待正式经济平衡 pass 校验**——economy GDD(L261)现仅记"预期偏通缩、非已量化结论、须 playtest",**未量化此比例**),不在本 GDD 焊死。

### F-1 `collect_from_each_total` / `pay_to_each_total` — 向其他每位在局玩家收/付总额 [Logic]

`total = amount × other_count`,其中 `other_count = |ActivePlayers| − 1`(排除行动玩家)。

| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 每位金额 | `amount` | int32 | ≥1(牌面数据) | 卡 Data Asset 字段,非公式推导 |
| 在局玩家集 | `ActivePlayers` | set\<PlayerId\> | \|·\|∈[1,8] | 与回合2 `active_player_count` **同一时刻**快照(防两次查询间破产致 Debit/Credit 不平衡) |
| 行动玩家 | `actingPlayer` | PlayerId | — | 卡持有人,从集合排除 |
| 其他在局数 | `other_count` | int32 | [0,7] | =\|ActivePlayers\|−1(派生) |

**Output Range**: [0, 7×amount]。`other_count==0`(仅剩1人在局)→ total=0,无转移(正常末期退化,不报错)。int32 安全(7×合理 amount 远未近溢出)。
**执行(零和**在每笔成对转移层**、逐笔)**: `CollectFromEach` = 对每位 other `Debit(p, amount)`(某人付不起→破产9流程,实际转移可 <amount)后 `Credit(self, 逐笔实收 Σ)`(`total` 为**名义上界**、实际入账=实收);`PayToEach` = **逐笔** `Debit(self, amount)` + `Credit(p_i, amount)` 按回合顺序(**非汇总经银行中转**,防凭空造币/蒸发)。**零和澄清(economy finding-1/systems F-3,OQ-Event-4 收口)**:零和指每笔 `Debit(x)↔Credit(x)` 成对、不经银行造币;**逐笔已使每笔触发破产时债权人单一(economy F-11 单债权人路径清晰)**;致行动玩家破产时的资产清算(债务人剩余资产归触发该笔破产的单一债权人 `p_i`)归破产9、是经典规则,**非本卡零和的违反**。故 **OQ-Event-4 降为说明性 note、非 economy/破产9 propagate 债**(不再"自述已解决又呼吁补规则")。
**Example**: 4人局 P1 收,amount=50,P2/P3/P4 在局 → other_count=3 → total=150;2人局 → 50;仅 P1 → 0。

### F-2 `nearest_tile_of_type(from, type)` — 环绕搜最近指定类型格 [Logic]

`= argmin_{t∈TypeIndices(type)} StepsBetween(from, t)`,平局取较小 TileIndex(确定性裁决,联网重放一致)。**消费棋盘 `StepsBetween(from,t)=mod(t−from,N)`**(registry `steps_between`,不重造环绕数学)。`TypeIndices` 运行时遍历 `GetTileData` 取(**不硬编码索引**,自定义盘兼容)。

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `from` | int32 | [0,N-1] | 当前格 |
| `type` | ETileType | {Railroad,Utility} | 目标类型 |
| `TypeIndices(type)` | set\<int32\> | ⊆[0,N-1];经典 RR={5,15,25,35}/Util={12,28} | 遍历棋盘取 |
| `N` | int32 | ≥4,经典40 | 棋盘格数 |

**Output Range**: [0,N-1];`TypeIndices` 空(自定义盘无该类型)→ `INDEX_NONE`,调用方 no-op(弃该牌效果)。`from` 本身即该类型 → steps=0 → 交移动 `TeleportTo(advanceOnZero=true)` 前进整圈(经典"前进到同类型则走一圈"语义)。
> **与 board-data L235 协调(systems F-2)**:board 禁"以 `steps=0` 调 F-2 表达传送"——本系统不绕过:`MoveToNearest` 经移动 `TeleportTo`(移动内部按 board F-3 `StepsBetween` 求顺时针步数再走 F-1/F-2,`passed_go` 正确),`advanceOnZero` 是**移动 `TeleportTo` 的参数**(非 board F-2 入参);`from==target` 时由移动据 `advanceOnZero=true` 走整圈。整圈落回**自有**该类型格不收租(自有免租);落**他人**格按经济正常收租=经典行为(本系统只移动+触发该次落地结算)。
**Example**(N=40): from=39,RR → StepsBetween(39,5)=6 最小 → 返回 5(环绕正确);from=26,RR → 返回 35(9 步)。

### F-3 `repair_fee_total`(⚠provisional)— 修缮费总额 [Logic,依赖8 mock]

`= house_count × per_house_fee + hotel_count × per_hotel_fee`。

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `house_count` | int32 | [0,32] 经典 | **全盘累计普通房数**=建房8 `GetTotalHouseCount`(档1-4,酒店折档5不计;≠economy F-2 单地产 [0,5] 口径,接口已区分,OQ-Event-3 ✅ RESOLVED → registry `total_house_count`) |
| `hotel_count` | int32 | [0,8] 经典 | 全盘酒店数=建房8 `GetTotalHotelCount`(档5;registry `total_hotel_count`) |
| `per_house_fee`/`per_hotel_fee` | int32 | ≥0(牌面数据) | 典型 25/100 |

**Output Range**: [0, 经典上界 32×25+8×100=1600],int32 安全。无房无酒店→0(合法退化)。⚠ **上界 1600 > 起始现金 1500(快速档 750)**——RepairFee 作 sink 的量级/触发频率(全盘建房密度高时对建房激进者惩罚重、形成隐性翻盘)须经经济平衡 pass 评估翻盘效果是否符合意图(economy finding-5,并入 OQ-Event-3)。
**Example**: 3房1酒店,25/100 → 3×25+1×100=175 → `Debit(self,175)`。

### F-4 监狱出狱判定条件式(JC-1~JC-5)[Logic]

纯常量比较(命名供单测/引用):
- **JC-1 强制出狱**: `bForceRelease = bIsInJail ∧ (JailTurnsServed == MAX_JAIL_TURNS−1)`(==2)。**本系统内部推导值**(不单设接口,qa B-4);UI 据 player-turn 的 `JailTurnsServed` 字段自行预告。
- **JC-2 可用出狱卡**: `bIsInJail ∧ HoldsAnyOutCard`(=`bHoldsChanceOutCard ∨ bHoldsChestOutCard`)。
- **JC-3 可付保释**(仅 UI 灰显;实付由经济 Debit 权威,非守门): `bIsInJail ∧ Cash≥JAIL_BAIL_AMOUNT`。
- **JC-4 双点出狱**: `bIsInJail ∧ bIsDouble` → 免费出狱、用该骰点移动、**不进双点链**。
- **JC-5 留狱递增**: `bIsInJail ∧ ¬bIsDouble ∧ JailTurnsServed<2` → 7 裁定留狱,**player-turn 计 `JailTurnsServed+1`**(本系统不自增,CR-7 所有权)、回合结束。
- **JC-1 ∧ JC-4 同时**(第3回合掷到双点): 双点优先,免费出狱、**不付保释金**。

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `bIsInJail` | bool | {f,t} | **存于 player-turn PlayerState**;7 经受控写改、拥有规则(CR-7) |
| `bHoldsChanceOutCard`/`bHoldsChestOutCard` | bool | {f,t} | 本系统7自有(按堆,CR-5);与 `bIsInJail` 独立 |
| `JailTurnsServed` | int32 | [0, MAX_JAIL_TURNS-1]=[0,2] | **存于 player-turn、player-turn 计数**(据7裁决);**不变式:永不到3**(==2 非双点必强制出狱) |
| `MAX_JAIL_TURNS` | int32 | 3 | tuning,经典 |
| `JAIL_BAIL_AMOUNT` | int32 | 50 | tuning,经典 |
| `bIsDouble` | bool | {f,t} | 来自骰子3 `FDiceRollResult.bIsDouble` |

**不变式**: `JailTurnsServed∈[0,2]`(用 `≥` 守门防越级到3);任何出狱路径(卡/保释/双点/强制)后 `bIsInJail=false`。

> **新常量(Phase 5 注册 registry)**: `JAIL_BAIL_AMOUNT=50`(**source=事件格7**,非经济5——监狱规则参数;经济只执行 Debit,平衡合理性是经济 Advisory)、`MAX_JAIL_TURNS=3`、`CHANCE_DECK_SIZE`/`CHEST_DECK_SIZE=16`、`per_house_fee`/`per_hotel_fee`(provisional,待8)。

## Edge Cases

**金钱效果边界**
- **若玩家付不起 Tax / 保释金 / PayToEach / RepairFee**:本系统照发起 `Debit`,现金不足由经济/破产(9)触发 Raising Funds / 破产裁决(economy F-10/F-11);本系统**不裁决破产、不预先拦截**(职责边界)。
- **若 `PayToEach` 致行动玩家破产(多债权人)**:逐笔 `Debit(self,amount)`+`Credit(p_i)` 按回合顺序;某笔 Debit 触发破产时该笔债权人**单一** `p_i`(economy F-11 路径清晰)。已收款的 `p_1..p_{i-1}` 各自完整收到其 amount(已成对转移);破产清算(债务人剩余资产归触发破产的 `p_i`)归破产9、**是经典规则,非零和违反、非 F-11 缺口**。**OQ-Event-4 为说明性 note(非 propagate 债)**:此"先收款者只得 amount、触发破产者额外继承债务人剩余资产"的不对称是经典忠实,AI(10) 估值须知此性质(见 F-1 零和澄清)。
- **若 `CollectFromEach` 多位被收方同时付不起**:按回合顺序依次处理,每位各自进 Raising Funds/破产,债权人=收款方(单一),F-11 路径清晰。
- **若 amount=0 或 other_count=0**(仅剩1人在局):total=0,无转移、不报错(牌面 amount≤0 由数据层校验拒绝,非本系统)。

**"前进"类牌双重发薪防御(economy-designer [BLOCKING])**
- **若牌效果"前进到/经过 GO"**:发薪**只**经移动 `TeleportTo(paysGo=true)` 的 `passed_go`→经济 F-1,牌本身**不**再附加 `BankCredit`(防双重 faucet,economy CR-2 禁双重发薪)。"前进到 GO 领 200"= passed_go 那 200,非额外 200。
- **若 `MoveRelative` 后退**:后退直达(`paysGo=false`),不经过 GO 不发薪(经典:后退不领薪)。
- **若 `MoveToNearest` 越过 GO**:`paysGo=true`,照常发薪。

**MoveToNearest 边界**
- **若 `TypeIndices(type)` 为空**(自定义盘无该类型):`INDEX_NONE` → 牌效果 no-op(不移动、不报错)。
- **若 from 本身即该类型格**:steps=0 → 移动 `advanceOnZero=true` 前进整圈回到该格(经典"前进到同类型走一圈"语义),越 GO 发薪。
- **到最近 Utility**:重掷骰注入 dice_total 算 F-4(CR-8);**到最近 Railroad**:经典可能 2× 租(归经济 Railroad 租,本系统只移动+触发结算)。

**监狱边界**
- **若 `arrivalContext==SentToJail`**(去坐牢落地):ResolvePhase 不调本系统事件结算(不抽牌/不收租/不缴税,movement L147-149 / player-turn AC-46);本系统只在 `SendToJail` 时置监狱态。
- **若在狱掷双点出狱**:免费出狱、用该骰点移动,**不进双点链、不给额外回合**(player-turn OQ-T-1 / dice OQ-T-Dice-3 关键);本系统消费 `bIsDouble` 仅判免费出狱。
- **若第3在狱回合非双点且付不起保释 50**:触发破产9(本系统发起强制 `Debit`,裁决归9)。
- **若被强制出狱回合玩家持出狱卡**:可选用卡免付保释(JC-2 优先于 JC-1 强制付费)。
- **若3次连续双点入狱**:由回合2 CR-4 触发经 `SendToJail()`,该次不移动不结算、双点链终止(回合2);本系统只置态。
- **若玩家已在狱**:在狱回合只走出狱裁决(CR-9)、不掷骰落地、不抽牌 → "在狱中又抽到 GoToJailCard"不可能发生。

**抽牌边界**
- **若出狱卡被持有致牌堆少1张**:堆剩 15 张正常循环抽;出狱卡用后回底补回。牌堆**不会抽空**(用后回底循环制)。
- **开局洗牌**用骰子(3)RNG 服务(确定性可重放,非引擎全局随机)。

**程间重入**
- **若卡触发多程移动**(如"前进到最近X"落地后该格又触发事件):严格串行,前程 `OnPawnLanded` 同步返回、前程 ResolvePhase 结算完才开下一程(player-turn AC-47 非重入);本系统不在前程落地回调内同步发起下一程。

## Dependencies

### 上游(本系统依赖)
| 系统 | 强度 | 接口 |
|---|---|---|
| 棋盘数据(1) | 硬 | `GetTileData(index)`(`TileType`/`EventDeck`/`TaxAmount`/`SpecialAction`/`JailIndex`)、`GetTileCount()`、`StepsBetween(from,t)`(F-2 环绕);遍历取 `TypeIndices` |
| 玩家回合(2) | 硬 | `ResolvePhase` 调 `ResolveTileEvent`;JailTurn 分支调出狱决策;回合2 第3次双点调 `SendToJail()`;回合2 查 `IsInJail()` 决定 JailTurn vs RollPhase |
| 骰子(3) | 硬 | `RollDice()`→`FDiceRollResult`(MoveToNearest 额外掷骰取 `Total`;出狱判定取 `bIsDouble`) |
| 移动(4) | 硬 | `TeleportTo(player,target,paysGo,context,advanceOnZero)`(入狱/前进到X);`Advance`(相对移动) |
| 经济(5) | 硬 | `Credit`/`Debit`(牌金钱/税/保释);MoveToNearest Utility 注入 `dice_total`→F-4 |

### 下游(依赖本系统)
| 系统 | 强度 | 读/写本系统 |
|---|---|---|
| AI(10) | 硬 | 读 `IsInJail`/`HoldsAnyOutCard` 做出狱决策;读事件概率估值 |
| 地产卡与卡牌 UI(17) | 硬 | 订阅 `OnCardDrawn`/`OnTileEventResolved` 呈现抽牌/事件 |
| 音频(22) | 硬 | 订阅事件播放抽卡/入狱/税单音效(index 22 depends-on 7) |
| 存档(21) | 硬 | 序列化牌堆数组顺序(model B,无指针)+ 7 自有 `bHoldsChanceOutCard`/`bHoldsChestOutCard`;`bIsInJail`/`JailTurnsServed` 由 player-turn 序列化(存其 PlayerState,CR-7) |
| 游戏反馈 VFX(19) | 软 | 翻牌/金币/入狱 juice(订阅事件) |
| 命运之轮(13)/策略卡(15) | 软(Alpha) | 经事件触发 / 扩展牌效果类型 |
| 建房(8) | 软 | `RepairFee` 牌读 `GetTotalHouseCount`/`GetTotalHotelCount`(全盘房/酒店分开计,建房8 F-7;OQ-Event-3 ✅ RESOLVED) |

> **双向一致性(已 grep 核实上游均列 7)**:board-data L162 / player-turn CR-3 / dice OQ-T-Dice-3 / movement OQ-T-Move-2 / economy L108 均已把事件格(7)列为依赖方 ✓。
>
> **⚠ 须 producer propagate 核实的接缝(本 GDD 新引入,登记 Open Questions)**:
> - **监狱态所有权 vs player-turn(OQ-Event-1 ✅ RESOLVED 2026-06-03)**:已核实 player-turn(已 Approved L84/L85/L200)**持监狱态字段 `bIsInJail`/`JailTurnsServed` + 据7裁决机械计数**;本档已对齐为"字段存 player-turn、7 拥规则 + 经受控写改 `bIsInJail`"(CR-7)。**残留低成本 propagate(producer)**:player-turn 须为监狱态显式命名受控写接口(现 L89 仅举 `SetPosition` 例)。
> - **多债权人破产(OQ-Event-4 ✅ 收口,非 propagate 债)**:逐笔执行使每笔触发破产时债权人单一(economy F-11 路径清晰);不对称清算是经典忠实、非零和违反(见 F-1 / Edge Cases)。无须 economy/破产9 propagate。
> - **新 registry 常量(OQ-Event-5 ✅ 已注册)**:`JAIL_BAIL_AMOUNT=50`(source=7)/`MAX_JAIL_TURNS=3`/牌堆 size/收付公式 已于 authoring Phase 5b 注册;`per_house_fee`/`per_hotel_fee` 待建房8(prov)。
> - **残留 propagate(唯一)**:OQ-Event-1 的 player-turn 受控写接口命名(低成本,producer)。

## Tuning Knobs

| 旋钮 | 作用 | 安全范围 | 过高/过低后果 | 归属 |
|---|---|---|---|---|
| `JAIL_BAIL_AMOUNT` | 保释金 | 默认 **50**;[0, ~200] | 过高→难赎身、监狱过度惩罚伤支柱④;过低/0→出狱无代价、监狱失去张力 | **事件格(7)** |
| `MAX_JAIL_TURNS` | 最长服刑回合 | 默认 **3**;[1,5] | 过高→困狱拖沓(伤支柱④);=1→监狱几无威慑 | **事件格(7)** |
| `CHANCE_DECK_SIZE` / `CHEST_DECK_SIZE` | 各牌堆张数 | 默认 **16**;[8,24] | 过小→同张牌频繁复现、惊喜衰减;过大→牌效果稀释、记忆点弱 | **事件格(7)**(牌内容归棋盘1 CardData) |
| 牌组 faucet:sink 构成 | `BankCredit` vs `BankDebit` 张数/金额比 | 经典约 **2~2.5:1**(起草期建议,**待经济平衡 pass 校验**) | faucet 过多→通胀冲刺、绕圈碰牌优于投资;过少→现金枯竭加速淘汰(伤支柱②/④) | **数据/平衡 pass**(CardData,非本 GDD 焊死) |
| `per_house_fee` / `per_hotel_fee` ⚠prov | RepairFee 牌单价 | 典型 **25 / 100** | 过高→建房惩罚劝退建设;过低→无意义 | **数据/平衡 pass**(待建房8) |

> **不在本系统的旋钮(指向真值源,不重复定义)**:
> - 税额 `TaxAmount`(所得税 200 / 奢侈税 100)→ 棋盘(1)字段 / 经济(5) F-7 平衡。
> - `doubles_jail_threshold`(3 次连续双点入狱)→ **玩家回合(2)** CR-4(registry 已注册,owner=player-turn)。
> - GO 发薪额 `SalaryAmount`(200)→ 棋盘(1)字段 / 经济(5) F-1。
> - 牌面具体金额 / 移动目标 → 棋盘(1) CardData 数据表。
>
> **旋钮交互**:`JAIL_BAIL_AMOUNT` × `MAX_JAIL_TURNS` 共同决定监狱惩罚烈度(保释贵且服刑久=双重惩罚,调参须合看);faucet:sink 构成 × 税额/起始现金 共同决定经济通胀曲线(归经济平衡)。

## Visual/Audio Requirements

**n/a own —— 本系统是事件结算层,不渲染视觉、不播音频。** 但事件的可见/可听时刻——翻牌动画、入狱铁门、税单、金币飞溅——是核心反馈,由 VFX(19)/卡牌UI(17)/音频(22) 拥有。本系统钉最低呈现契约:

- **端到端 owner**:卡牌UI(17)=翻牌/牌面呈现;VFX(19)=金币/入狱清算演出;音频(22)=抽卡"叮"/入狱铁门/税单声。具体动画/时长归呈现层。
- **本系统义务边界**:广播 `OnCardDrawn(playerId, deck, cardId)`/`OnTileEventResolved(playerId, tileIndex, effectType)`/`OnPlayerJailed(playerId, reason)`/`OnPlayerReleased(playerId, releaseMethod)`;事件结果在动画开始前已产出(确定性事实),呈现层只回放既定结果,不影响结算逻辑。
- ⚠ **金钱 juice 的事件源不是本系统**:牌/税/保释的现金转移经经济 `Credit`/`Debit` → 金币飞溅应订阅**经济 `OnRentPaid`/`OnCashChanged`**(同 economy/property 模式),非本系统事件。本系统的事件只标"抽到哪张牌/谁入狱/谁出狱",钱的视觉归经济事件。此澄清防 VFX(19) 误订阅本系统事件来触发金币动画。
- **好/坏语境**:`OnCardDrawn` 携 `cardId` 供呈现层据牌效果区分庆祝(收钱)vs 警告(付钱/入狱);`OnPlayerReleased` 携 `releaseMethod` 区分免费出狱(双点)vs 付费赎身。

> 📌 **Asset Spec**:art bible 批准后,可运行 `/asset-spec system:tile-events` 产出机会/命运牌面、监狱、税格的视觉规格。

## UI Requirements

**n/a own —— 本系统无任何 UI。** 只暴露事件 + 查询(`IsInJail`/`HoldsAnyOutCard`)+ 出狱决策入口。抽牌弹窗/牌面呈现、监狱出狱决策面板(用出狱卡 / 付保释 / 掷骰)、出狱卡持有角标由 **地产卡与卡牌 UI(17)/HUD(16)** 实现;出狱决策的输入路由由 UI 处理后回调本系统接口。本系统的"接口"是数据契约与事件,不是界面。

> 📌 **UX Flag —— 事件格**:抽牌弹窗 + 监狱出狱决策面板须在 Pre-Production 由 `/ux-design` 为相关卡牌屏/HUD 出规格,再交 `/team-ui`。Stories 引用 `design/ux/[screen].md`,非本 GDD。

## Acceptance Criteria

> *由 `qa-lead`(lean 模式 H 节高风险派发)产出。* **测试分层(对齐 economy/movement/player-turn):** `[Logic]` 纯 fixture struct/确定性整数/headless `-nullrhi` 可跑/BLOCKING PR merge gate(`tests/unit/tile-events/`);`[Advisory]` 集成/code-review/跨进程,非门控。
> **Mock 原则**:所有 [Logic] 中经济 `Credit`/`Debit`、移动 `TeleportTo`/`Advance`、骰子 `RollDice`、建房 `house_count`/`hotel_count` 全 mock 注入;只验本系统的路由/聚合/状态转换输出。dev `ensure` AC 默认 Development build + 捕获 ensure 回调。

### A. 事件入口与路由(CR-1/CR-2)
- **AC-1** [Logic] 入口签名:`ResolveTileEvent(playerId, tileIndex, arrivalContext)` 标 `UFUNCTION(BlueprintCallable)`,编译通过。
- **AC-2** [Logic] `arrivalContext==SentToJail` 抑制全结算:GIVEN arrivalContext=SentToJail,WHEN ResolveTileEvent,THEN mock Debit/Credit 调用==0 ∧ TeleportTo==0 ∧ 无抽牌 ∧ bIsInJail 不因本次调用改变(入狱由 SendToJail 置)。(本系统自防御;上游不调归 player-turn AC-46。)
- **AC-3** [Logic] Chance 路由:GIVEN TileType=Chance ∧ arrivalContext=DiceMove,THEN 从 Chance 堆抽顶张一次 ∧ 路由层本身不直接操作金钱(金钱由牌效果执行,见 AC-15)。
- **AC-4** [Logic] CommunityChest 路由:同 AC-3,堆为 CommunityChest。
- **AC-5** [Logic] Tax 路由:GIVEN TileType=Tax ∧ mock TaxAmount=200,THEN mock Debit(playerId,200) 恰 1 次 ∧ 无 TeleportTo ∧ 无抽牌。
- **AC-6** [Logic] GoToJail 路由:GIVEN SpecialAction=GoToJail,THEN mock TeleportTo(JailIndex,paysGo=false,SentToJail) 恰 1 次 ∧ bIsInJail==true ∧ JailTurnsServed==0 ∧ 无 Debit/Credit ∧ 无抽牌。
- **AC-7** [Logic] Go no-op:GIVEN TileType=Go,THEN 无 Debit/Credit/TeleportTo/抽牌(发薪在移动层,不二次发薪)。
- **AC-8** [Logic] Jail 探视 no-op:GIVEN TileType=Jail ∧ bIsInJail==false,THEN 无副作用。
- **AC-9** [Logic] FreeParking no-op(MVP):GIVEN TileType=FreeParking ∧ SpecialAction!=TriggerHouseRuleCheck,THEN 无副作用。

### B. 抽牌机制(CR-3/CR-5)
- **AC-10** [Logic] 抽牌回底循环:GIVEN Chance 堆 [A,B,C],抽两次,THEN 第一次抽 A(堆→[B,C,A])∧ 第二次抽 B(堆→[C,A,B])∧ 堆长恒==3(非出狱卡场景)。
- **AC-11** [Logic] 出狱卡离堆:GIVEN Chance 堆 GetOutOfJailFree 在堆顶,抽到,THEN bHoldsChanceOutCard==true ∧ Chance 堆 `Num()`==CHANCE_DECK_SIZE-1(离堆不回底)∧ 后续抽牌跳过它。
- **AC-12** [Logic] 出狱卡用后回底:GIVEN bHoldsChanceOutCard==true 且玩家消费该卡出狱,THEN bHoldsChanceOutCard==false ∧ Chance 堆 `Num()`==CHANCE_DECK_SIZE(回底)∧ 数组末元素是该出狱卡。
- **AC-13** [Logic] 牌堆不抽空(出狱卡被持有时):GIVEN 出狱卡已持有(堆剩15),连抽15次,THEN 每次取得非 null ∧ 第16次轮回堆首 ∧ 不崩溃。
- **AC-14** [Logic] 每堆各 1 张出狱卡:GIVEN 初始化完成两堆,THEN 各堆 GetOutOfJailFree count==1。

### C. 牌效果执行(CR-4)
- **AC-15** [Logic] BankCredit:抽到 BankCredit(50) → mock Credit(playerId,50) 恰 1 次 ∧ 无其他金钱调用。
- **AC-16** [Logic] BankDebit:抽到 BankDebit(150) → mock Debit(playerId,150) 恰 1 次。
- **AC-17** [Logic] MoveToTile + 双重发薪防御:抽到 MoveToTile(target=0,paysGo=true) → mock TeleportTo(playerId,0,paysGo=true,AdvanceCard) 恰 1 次 ∧ **无额外 Credit**(发薪只走 paysGo→F-1,牌不附加)。
- **AC-18** [Logic] MoveRelative 后退不过 GO:抽到 MoveRelative(-3) → mock Advance(playerId,-3) ∧ paysGo==false ∧ 无 Credit(后退不发薪)。
- **AC-19** [Logic] GoToJailCard:抽到 → mock TeleportTo(JailIndex,paysGo=false,SentToJail) 恰 1 次 ∧ bIsInJail==true ∧ JailTurnsServed==0(与 CR-7 对称)。
- **AC-20** [Logic] GetOutOfJailFree:GIVEN 从 Chance 堆抽到 GetOutOfJailFree(抽前 bHoldsChanceOutCard==false)→ bHoldsChanceOutCard==true ∧ 无 Credit/Debit/TeleportTo(只置持有)。
- **AC-21** [Logic] RepairFee:GIVEN 抽到 RepairFee(25,100) ∧ mock `GetTotalHouseCount`=3 ∧ `GetTotalHotelCount`=1 → mock Debit(playerId, 175) 恰 1 次。(OQ-Event-3 ✅ RESOLVED 2026-06-04,建房8 F-7 接口已定 → [Logic]。)

### D. F-1 CollectFromEach / PayToEach 零和聚合
- **AC-22** [Logic] CollectFromEach 总额:GIVEN Active={P1,P2,P3,P4},acting=P1,amount=50 → other_count==3 ∧ Debit 对 P2/P3/P4 各 50 ∧ Credit(P1,实收Σ)。
- **AC-23** [Logic] PayToEach 零和逐笔:GIVEN acting=P1,other={P2,P3},amount=30 → Debit(P1,30) 调**两次**(逐笔非汇总60)∧ Credit(P2,30)+Credit(P3,30) 各一次 ∧ 无银行中转 ∧ 总流出60==总流入60。
- **AC-24** [Logic] other_count==0 退化:GIVEN Active={P1},amount=50 → total==0 ∧ 无 Debit/Credit ∧ 不报错。
- **AC-25** [Logic] 2人局 CollectFromEach:Active={P1,P2},acting=P1,amount=50 → Debit(P2,50)×1 ∧ Credit(P1,50)×1。
- **AC-26** [Logic] ActivePlayers 单次快照:GIVEN F-1 途中 mock 模拟 P3 破产移除,THEN 仍按**快照时刻** other_count 执行(不途中重算),防两次查询间不平衡。

### E. F-2 nearest_tile_of_type 环绕搜索
- **AC-27** [Logic] 环绕最近 Railroad:N=40,from=39,RR={5,15,25,35} → 返回 5(StepsBetween=6 最小,环绕正确)。
- **AC-28** [Logic] 多候选取最小步(GDD 例):from=39→返回 5(steps=6);from=26→返回 35(steps=9)。
- **AC-29** [Logic] F-2 确定性 + 单元素集合:GIVEN N=40,from=3,TypeIndices={5,15,25,35},连调两次 `nearest_tile_of_type` → 两次同值==5 ∧ 结果与遍历内部排列无关(确定性,联网重放一致);GIVEN 单元素 TypeIndices={15} → 返回 15(单候选退化)。(注:真平局[两不同格距 from 等步]在单棋盘**数学不可能**——`mod(t−from,N)` 对不同 t 单射;原"虚拟等步格"fixture 不可构造已删,systems F-5/qa B-3。重复 TileIndex 数据 bug 取最小归数据层校验。)
- **AC-30** [Logic] advanceOnZero(from 即目标类型):N=40,from=5(本身 RR) → StepsBetween(5,5)==0 → 返回 5 ∧ TeleportTo 的 advanceOnZero==true(前进整圈)∧ paysGo==true(越圈发薪)。
- **AC-31** [Logic] 空集 no-op:TypeIndices(type)=={} → 返回 INDEX_NONE ∧ 无 TeleportTo ∧ 无 RollDice ∧ 不崩溃。

### F. F-3 repair_fee_total(provisional)
- **AC-32** [Logic] 基本计算:house=3,hotel=1,25/100 → total==175 ∧ Debit(playerId,175)×1。
- **AC-33** [Logic] 退化(无建筑):house=0,hotel=0 → total==0 ∧ Debit(playerId,0) 调用或不调用(合法,零额不报错)。
- **AC-34** [Logic] 全盘口径区分:mock 来自建房8 `GetTotalHouseCount`/`GetTotalHotelCount`(全盘房/酒店分开计,**非 economy F-2 单地产 [0,5] 口径**),测试注入聚合 mock 防静默混用(建房8 F-7)。
- *AC-32~34:OQ-Event-3 ✅ RESOLVED(2026-06-04,建房8 F-7)→ [Logic]。*

### G. F-4 监狱出狱判定(JC-1~JC-5)
- **AC-35** [Logic] JC-1 强制出狱:GIVEN bIsInJail=true ∧ JailTurnsServed==2 ∧ bIsDouble=false → bForceRelease==true ∧ Debit(playerId,50) ∧ bIsInJail==false ∧ JailTurnsServed 不增(无 served=3)。
- **AC-36** [Logic] JC-2 出狱卡消费:GIVEN bIsInJail=true ∧ bHoldsChanceOutCard=true,选"用 Chance 出狱卡" → bHoldsChanceOutCard==false ∧ 该卡回 Chance 堆底 ∧ 经受控写 bIsInJail==false ∧ 无 Debit。(出狱后掷骰移动由回合2 RollPhase 负责,不在本系统 THEN 边界,B-10。)
- **AC-37** [Logic] JC-4 双点出狱:本系统不发双点链信号(player-turn OQ-T-1 / dice OQ-T-Dice-3 单元侧):GIVEN bIsInJail=true ∧ JailTurnsServed∈{0,1},WHEN mock RollDice bIsDouble=true → 经受控写 bIsInJail==false ∧ TeleportTo 以骰 `Total` 移动 ∧ **本系统向 mock player-turn 发出的"双点链增量/额外回合"接口调用次数==0**(本系统不调任何 `ConsecutiveDoubles++`/`grantExtraTurn`;`ConsecutiveDoubles` 值本身归 player-turn,跨系统由 AC-50 覆盖——消除"断言本系统不拥有的字段==0"同义反复,qa B-1)。
- **AC-38** [Logic] JC-5 留狱递增:GIVEN bIsInJail=true ∧ JailTurnsServed==1 ∧ bIsDouble=false → JailTurnsServed==2 ∧ bIsInJail==true ∧ 无 Debit/TeleportTo。
- **AC-39** [Logic] JC-3 付保释出狱:GIVEN bIsInJail=true ∧ JailTurnsServed∈{0,1},选"付保释" → Debit(playerId,50)×1 ∧ 经受控写 bIsInJail==false。(规则跨 JailTurnsServed 一致;出狱后掷骰移动归回合2 RollPhase,不在本系统 THEN 边界,B-10。)
- **AC-40** [Logic] JC-1∧JC-4 同时(第3回合掷双点):GIVEN JailTurnsServed==2 ∧ bIsDouble=true → 双点**免费**出狱(无 Debit)∧ 经受控写 bIsInJail==false ∧ **本系统不发双点链增量信号**(双点优先,JC-1 强制付费被覆盖)。
- **AC-41** [Logic] JC-2 优先 JC-1:GIVEN JailTurnsServed==2 ∧ bHoldsChanceOutCard=true,选"用卡" → bHoldsChanceOutCard==false ∧ 经受控写 bIsInJail==false ∧ **无 Debit**(卡免保释)。

### H. 监狱状态机不变式
- **AC-42** [Logic] `JailTurnsServed∈[0,2]` 永不到3:连续在狱3回合非双点 → 序列 0→1→2→第3回合强制出狱(不出现 served==3)。
- **AC-43** [Logic] 出狱后 `bIsInJail==false`(四路径):卡/保释/双点/强制各路径出狱后 bIsInJail 精确==false。
- **AC-44** [Logic] 出狱卡持有与 `bIsInJail` 独立:GIVEN bIsInJail=false ∧ bHoldsChanceOutCard=true(狱外持卡),WHEN 落 Tax 格 ResolveTileEvent → 正常缴税路由(Debit(TaxAmount))∧ bHoldsChanceOutCard 不被清除(持卡不受非监狱结算影响)。
- **AC-45** [Logic] SendToJail 入口:回合2 第3次双点调 SendToJail(playerId) → 经受控写 bIsInJail==true ∧ JailTurnsServed==0 ∧ TeleportTo(JailIndex,paysGo=false,SentToJail)×1。**写路径唯一**:入狱转换经 SendToJail() 驱动,字段 `bIsInJail`/`JailTurnsServed` 存于 player-turn PlayerState、本系统经受控写改 `bIsInJail`、player-turn 据裁决置 JailTurnsServed=0(CR-7,OQ-Event-1 以 player-turn 为准;无双写)。

### I. 关键 Edge Cases
- **AC-46** [Advisory·code-review] 在狱回合不走 ResolveTileEvent:code-review 核查 player-turn JailTurn 分支不调 ResolveTileEvent(与 player-turn AC-6 协同)。
- **AC-47** [Logic] Go 格双重发薪防御:抽到 MoveToTile(GO_INDEX,paysGo=true) → TeleportTo(GO_INDEX,paysGo=true) ∧ **无额外 Credit(playerId,SalaryAmount)**(发薪只走 paysGo→F-1)。
- **AC-48** [Logic] 多债权人 PayToEach 逐笔(OQ-Event-4 防御):acting=P1,other={P2,P3},amount=30 → Debit(P1,30) 按回合顺序先 P2 债再 P3 债 ∧ 每笔独立(不合并 60)∧ Credit 各独立(保 economy F-11 单债权人前提)。
- **AC-49** [Logic] MoveToNearest 空集不崩溃:TypeIndices(Utility)=={} → 返回 INDEX_NONE ∧ 无 TeleportTo ∧ 无 RollDice ∧ no-op。

### J. 继承测试义务(systems-index 4 条逐条对应)
- **AC-50** [Advisory·integration] 双点出狱不进双点链(集成,player-turn OQ-T-1 + dice OQ-T-Dice-3):集成环境(7+3+2)∧ bIsInJail=true,mock RollDice bIsDouble=true → bIsInJail==false ∧ ConsecutiveDoubles==0 ∧ 无 bGrantsExtraTurn ∧ TeleportTo 以骰点移动。(与 AC-37 单元层互补:AC-37 BLOCKING,AC-50 ADVISORY。)
- **AC-51** [Logic] 出狱掷骰纯消费(dice OQ-T-Dice-3 单元层):出狱掷骰路径 → mock RollDice 调 1 次(签名==无参 `RollDice()`)∧ 本系统仅读 bIsDouble(判免费出狱)+ Total(供 TeleportTo 距离)∧ 无第二次 RollDice 调用。(删原"不改 RollDice 返回值"——mock 注入下永真,qa B-11。)
- **AC-52** [Logic] MoveToNearest Utility 额外掷骰 dice_total 注入(economy OQ-T-Econ-3):抽到 MoveToNearest(Utility) ∧ mock nearest=28 → mock RollDice 调 1 次(额外骰 Total=X)∧ **本系统经 player-turn 受控写接口 `SetCurrentRollContext(FDiceRollResult)` 更新当前程 dice 上下文 holder 为 X**(mock player-turn `SetCurrentRollContext` 调 1 次、参数 Total==X;使经济在 ResolvePhase 从 holder PULL 到 X、不串程,player-turn CR-3.1/受控写接口面)∧ TeleportTo(28,paysGo=true,AdvanceCard) ∧ 经济 F-4 结算 dice_total==X(非回合初始骰)。(qa B-9:若架构裁定事件格直接传参经济而不经 holder,则删 `SetCurrentRollContext` 断言并在 CR-8 散文注明;接口名已与 player-turn 受控写接口面对齐,不再凭空命名。)
- **AC-53** [Logic] 牌金钱效果经经济(economy OQ-T-Econ-3 金钱侧):BankCredit(50)/BankDebit(150)/CollectFromEach(30) 各执行 → 分别 Credit(playerId,50)/Debit(playerId,150)/Debit(each,30)+Credit(self,Σ);本系统无绕过 economy 的内部现金操作。
- **AC-54** [Logic] TeleportTo 接口正确(movement OQ-T-Move-2):三路断言 — ① 前进到最近X:TeleportTo(nearestIndex,**paysGo=true**,AdvanceCard);② 去坐牢:TeleportTo(JailIndex,**paysGo=false**,SentToJail);③ MoveToTile:TeleportTo(target,paysGo=牌面,AdvanceCard);三路 target∈[0,N-1] 不越界。

### K. 事件广播 payload
- **AC-55** [Logic] OnCardDrawn:抽到 Chance BankCredit cardId=C3 → OnCardDrawn(playerId,Chance,C3) 广播 1 次,字段正确,标 `BlueprintAssignable`+`DYNAMIC_MULTICAST_DELEGATE`。
- **AC-56** [Logic] OnPlayerJailed:落 GoToJail→reason=TileEffect;抽 GoToJailCard→reason=CardEffect;回合2双点链 SendToJail→reason=ConsecutiveDoubles。三 reason 各 1 次。
- **AC-57** [Logic] OnPlayerReleased:releaseMethod∈{Card,BailPaid,DoubleDice,ForcedBail},各出狱路径值正确。
- **AC-58** [Logic·CI 构建] 四事件均标 `BlueprintAssignable`、payload `USTRUCT(BlueprintType)`、enum 字段 `uint8` 基底;UHT 生成 + headless CI `-nullrhi` 编译**零 UHT 警告**通过(unreal Issue-3:仅"编译通过"不够,int32-backed enum/缺 UPROPERTY 会编译过但 BP 绑定失效)。

### L. 确定性 / 接口稳定性
- **AC-59** [Logic] `IsInJail(playerId)` 标 `UFUNCTION(BlueprintCallable)` ∧ bIsInJail==true→true / false→false ∧ 编译通过。
- **AC-60** [Logic] F-3 整数纯净:house=3,hotel=1,25/100 → 两次冷启动/跨 Debug-Shipping 位级一致 ==175(无 float,对齐 economy AC-32)。
- **AC-61** [Advisory·smoke] Config/Data smoke:`JAIL_BAIL_AMOUNT=50`/`MAX_JAIL_TURNS=3`/`CHANCE_DECK_SIZE=16`/`CHEST_DECK_SIZE=16` 从数据源正确加载、非硬编码;`per_house_fee`/`per_hotel_fee` 从 CardData 加载(prov)。

### M. 非法决策降级 + 序列化(承接 player-turn CR-8#4/AC-39b;B-12 / R-9)
- **AC-62** [Logic] DecideJailAction 非法 PayBail 降级:GIVEN bIsInJail=true ∧ mock Cash<JAIL_BAIL_AMOUNT,WHEN DecideJailAction 返回 PayBail,THEN 不执行 Debit ∧ 7 裁定留狱(player-turn 计 JailTurnsServed+1)∧ 经受控写 bIsInJail 仍 true ∧ dev `ensure`+log(Development build 捕获)。(承接 player-turn CR-8#4/AC-39b,B-12。)
- **AC-63** [Logic] DecideJailAction 非法 UseCard 降级:GIVEN bIsInJail=true ∧ HoldsAnyOutCard==false,WHEN DecideJailAction 返回 UseCard,THEN 两出狱卡标记不变==false ∧ 7 裁定留狱(JailTurnsServed+1)∧ bIsInJail 仍 true ∧ dev `ensure`+log(不凭空消卡)。
- **AC-64** [Advisory·integration] 出狱卡持有序列化还原:GIVEN bHoldsChanceOutCard=true ∧ 执行存档,WHEN 读档还原,THEN bHoldsChanceOutCard==true ∧ Chance 堆数组顺序与存档前一致 ∧ 还原后该卡可经 AC-36 路径消费(R-9;存于7自有状态,与 player-turn 的 bIsInJail 序列化分属各自系统)。

> **[Logic] gate(BLOCKING PR)≈ 63 条(R2 新增 AC-62/63;RepairFee 4 条 AC-21/32/33/34 经 OQ-Event-3 ✅ RESOLVED 2026-06-04 升回 [Logic]);[Advisory]= 4 条(AC-46 code-review / AC-50 integration / AC-61 smoke / AC-64 序列化)。**
> **4 条继承义务覆盖**:player-turn OQ-T-1 → AC-37(Logic 不发信号)+AC-50(Advisory 集成);dice OQ-T-Dice-3 → AC-37/AC-51;economy OQ-T-Econ-3 → AC-52/AC-53;movement OQ-T-Move-2 → AC-54;**player-turn CR-8#4/AC-39b(出狱非法降级)→ AC-62/63**(R2 补,B-12)。
> **CR/F 覆盖**:CR-1→1/2;CR-2→3~9/47;CR-3→10~14;CR-4→15~21/22~26/27~31/47~49;CR-5→11~14/36/41;CR-6→5;CR-7→6/19/45;CR-8→52/53;CR-9→35~41/**62/63**;F-1→22~26;F-2→27~31;F-3→32~34;F-4→35~41;监狱不变式→42~44;广播→55~58;序列化→64。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-Event-1 ✅ RESOLVED 2026-06-03(re-review,以 player-turn 为准)** | 经核实 player-turn(已 Approved)L84/L85/L200:监狱态字段 `bIsInJail`/`JailTurnsServed` **存于 player-turn 中心 PlayerState、player-turn 据7裁决机械计数**,7 拥有入/出狱规则 + 经受控写改 `bIsInJail`。本档 CR-3/5/7/9/States/Interactions/AC-37/40/45/52 已全部改写对齐(不再声称"7 拥有/直写/自增"监狱态);字段名统一 `bIsInJail`。**残留 propagate(producer,低成本、不阻塞)**:player-turn 须为监狱态显式命名受控写接口(现 L89 仅举 `SetPosition` 例)。 | ✅ 已对齐(残留:player-turn 命名受控写接口) |
| **OQ-Event-2 ✅ CLOSED** | 所得税 MVP flat 200 已由 **economy F-7 锁定**(已 Approved);"净资产% vs 固定二选一"变体归 **economy OQ-Econ-7(Alpha)**,非本档开放项(消假性不确定,economy finding-6) | ✅ 已闭合(并入 economy F-7/OQ-Econ-7) |
| **OQ-Event-3 ✅ RESOLVED**(2026-06-04) | `RepairFee` 的全盘 `house_count`/`hotel_count` 接口已由建房(8) F-7 提供具名 `GetTotalHouseCount`/`GetTotalHotelCount`,与 economy F-2 单地产 [0,5] 口径明确区分(registry total_house_count/total_hotel_count);本轮 `/consistency-check` 揪出名称重载并关闭。RepairFee 条件门 AC(21/32/33/34)据其自述升回 [Logic] | ✅ 建房(8) F-7 已落 |
| **OQ-Event-4 ✅ 降为说明性 note(非 propagate 债)** | `PayToEach` 逐笔执行使每笔触发破产时债权人单一(economy F-11 路径清晰);"先收款者得 amount、触发破产者继承债务人剩余资产"的不对称是经典忠实、**非零和违反、非 F-11 缺口**(见 F-1 零和澄清 / Edge Cases)。**无须 economy/破产9 propagate**;AI(10) 估值须知此性质 | ✅ 收口(说明性) |
| **OQ-Event-5 ✅ RESOLVED** | 新常量已注册 registry(authoring Phase 5b):`JAIL_BAIL_AMOUNT=50`(source=7)、`MAX_JAIL_TURNS=3`、`CHANCE_DECK_SIZE`/`CHEST_DECK_SIZE=16` + 公式 `collect/pay_to_each_total`;`per_house_fee`/`per_hotel_fee` 待建房8(prov) | ✅ 已注册 |
| **OQ-Event-7 ⚠ ADR(R2 新增,engine)** | UE 实现层,待生命周期/引擎 ADR:① 程间非重入机制(deferred 队列 vs `bIsResolvingTileEvent` 守门,unreal Issue-1);② 牌堆宿主 UObject 型(`UGameInstanceSubsystem` vs `UWorldSubsystem`,与 player-turn PlayerState 的读写接缝,unreal Issue-4);③ Fisher-Yates 洗牌 + `FRandomStream` 种子序列化细节(unreal Issue-5);④ `ECardEffectType` 10 类 dispatch 归 C++(BP 大 switch 保守性,unreal Issue-6)。容器 model B / enum uint8 已在正文焊死,此 OQ 仅留实现细节 | 下游实现/`/architecture-decision` |
| **OQ-Event-6 ⚠Rento核查** | 监狱出狱规则(保释 50 / 3 回合 / 双点免费出狱)是经典大富翁;本作 Rento 复刻,须对照 Rento Fortune 实测(类 OQ-Prop-5) | MVP 设计冻结前核查 |
