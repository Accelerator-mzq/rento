# 破产与胜负 (Bankruptcy & Win)

> **Status**: Approved(2026-06-04 R2 re-review;6 in-doc + 2 cross-doc blocker 全闭合 fresh-grep 核实,user 裁定就地批准)
> **Author**: msc + design-system agents
> **Last Updated**: 2026-06-04
> **Implements Pillar**: ① 桌游忠实(经典淘汰制破产)+ ② 社交博弈(「出局的坏笑」是牌桌互坑的终局兑现)— enables 整局戏剧弧(concept core loop:对手陆续破产,决出唯一赢家)

## Overview

破产与胜负系统是《骰子大亨》的**终局裁决层**:它判定一名玩家何时已无力偿付(破产)、编排其全部资产的清算移交、宣告其出局,并在棋盘上仅剩一名未破产玩家时宣告这一局的胜者——是把"掷骰—买地—收租—建房"的循环真正**收口成一局有赢家的游戏**的那一环。它有两个面:对内是一个**裁决/编排层**——消费经济(5)的 `is_insolvent` 判据触发破产,按"欠谁的钱"选择 creditor 分支(收租破产→资产 in-kind 转债权人 / 税费破产→回退银行),依次编排地产所有权(6)的归属移交、建房(8)的建筑随格转移、经济(5)的现金侧清算,维护玩家的 `bIsBankrupt` 出局态,并以 `ActivePlayerCount==1` 触发胜负信号;对玩家则是**整局戏剧弧的终点**——看着对手一个个付不起租金、变卖家当、滑向破产相继出局,最后只剩你笑到最后(concept Core Fantasy「看着对手相继破产,最后只剩你笑到最后」)。

它**不拥有**:破产判据公式(经济5 F-10 `is_insolvent` 严格 `<` + F-9 净可变现价值)、强制清算驱动环(回合2 CR-3.3,玩家挣扎筹资在它那里发生)、现金侧转移(经济5 F-11)、归属移交的执行(所有权6 `TransferOwnership`/`ReturnTileToBank`)、建筑随格(建房8 in-kind)、回合顺序与"跳过已破产玩家"(回合2 turn order/`NextActivePlayerIndex`)。它只拥有**"何时宣告破产、欠谁的钱(creditor 分支)、建筑 in-kind 还是卖房还银行(OQ-Build-3 裁定)、出局后如何排名(net worth 口径)、何时宣告胜利"这组终局裁决事实**。它做得好时,一局有清晰、戏剧性、规则忠实(支柱①)的终点;没有它,破产循环永远收不了口、无人能赢、游戏没有终局。破产清算与胜利时刻的视觉戏剧(资产易主 juice、出局动画、胜利庆祝、音效)是 **enable-not-own**——归 VFX(19)/HUD(16)/音频(22) 拥有,本系统只供裁决事实与事件供其挂接,不拥有那份"清算与封王"的呈现体验。

## Player Fantasy

**核心幻想:"封王时刻"——看着最后一个对手翻不出钱,棋盘归你,游戏在你的笑声里落幕。**

牌桌上只剩你和老对手两人。他上回合刚踩进你建满酒店的橙色街区掏空了现金;这一圈他又停在你的红组,租金 1200,他翻遍家底——抵押了最后两块地、拆光了房子,凑出 900,还差 300。屏幕弹出那行字:**[对手] 破产出局**。他的地产哗啦一下全染成你的颜色,棋盘上只剩一种颜色——你的。**你赢了。** 这一刻的爽,是整整四十分钟掷骰、算计、互坑的总账一次结清。

它服务支柱①(桌游忠实:经典大富翁就是"逼到所有人破产、最后一人封王"的淘汰制)与支柱②(社交博弈:看对手出局的坏笑是牌桌互坑的最终兑现)。情感弧线三段:
- **绝处的挣扎**(轮到你欠租付不起):抵押、卖房、最后一搏——"还差 300,我还有什么能变现?"(这份挣扎的机制在回合2 清算环 CR-3.3;本系统在它穷尽时落下破产判决)。
- **出局的坏笑**(对手破产):你建的杀手区收割猎物,他的家当流进你的账户(收租破产 in-kind)。
- **封王的凯旋**(`ActivePlayerCount==1`):最后存活者,游戏为你落幕。

**enable-not-own:** 这三段情感的视觉/听觉戏剧——资产易主的金币与变色 juice、出局的黯然动画、胜利的庆祝画面与音乐——本系统**使能但不拥有**(归 VFX19/HUD16/音频22);本系统供"谁破产了、谁赢了"这组裁决事实与事件,让那些时刻有据可演。

> **〔MVP 范围诚实标注〕** 破产在 MVP 是**单向、终局**的:出局玩家不回来(逆风翻盘的 injector——命运之轮13/俄罗斯轮盘14/交易11——全在 Alpha,对齐 building/economy 已声明的落后者托底范围)。Pass'N Play 同屏下,早破产玩家须**旁观余下牌局**直到分出胜负——concept 以"单局短(30–60 分钟可调)+ 再来一局成本低"缓解,但这是淘汰制的已知代价(经典大富翁通病);原型期 playtest 须观察早出局者等待体验是否可接受,不可接受则归 Alpha 加速破产/回合上限(House Rules23)+ injector 排期。**本系统不在 MVP 引入"复活"或"软出局"**(违支柱①桌游忠实)。
>
> **〔滚雪球 falsifiable 假设(game-designer F-2 / economy in-kind 雪崩,CD R-review carry-to-Alpha〕** MVP 砍翻盘 injector(命运之轮13/俄罗斯轮盘14/交易11)+ 拍卖(12,Alpha)→ 收租 in-kind 全资产直送单一债权人,可能在多人局放大滚雪球,使「封王」退化为「熬完」(concept Core Fantasy 含「逆风翻盘」)。此为**可证伪假设**:原型期 playtest 须量化记录 **post-bankruptcy 资产集中度(gini)** 与「领先者优势建立后无逆转案例占比」;若实测过高,**Alpha 优先级提前 injector 排期**。本系统**不在 MVP 自造翻盘刹车**(违 return-only 裁决层定位;翻盘 owner 归 13/14/11,时长上限旋钮归 House Rules23)。早出局者旁观体验(支柱① 淘汰制已知代价)的 playtest 验收阈值(如等待 P90)归 UX/HUD16,本系统不拥有。

## Detailed Design

### Core Rules

**CR-1 破产触发(被动判决,消费经济判据,不自算)。** 破产**唯一触发点** = 强制扣款(租金/税/费/修缮)穷尽清算后仍不足:回合2 清算环(CR-3.3)对债务人抵押+卖房穷尽后,经济5 `is_insolvent(player, amount_due, preaggregated_nlv)==true`。本系统**不自行计算** is_insolvent / NLV(归经济5 F-9/F-10),只消费 `true` 判决执行破产编排。玩家**无主动"宣告破产"动作**(自愿卖房/抵押买不起只是建不了/赎不回,不破产——见经济 CR-7;只有**强制**扣款付不起才破产)。

**CR-2 Creditor 分支(欠谁的钱,由触发扣款上下文携带,本系统不重判收款方)。**
- **收租破产**(债主=另一玩家):`amount_due` 是租金 → 债务人全部资产去向该债权人(对手)。
- **银行破产**(债主=银行):`amount_due` 是税/费/修缮(流向银行 sink)→ 债务人全部地回退银行、现金蒸发。
creditor 身份由回合2/事件7 的结算上下文传入 `ResolveBankruptcy(debtor, creditor, …)`,`creditor==INDEX_NONE` 即银行破产。

**CR-3 破产清算编排(关键架构裁定:9 为 return-only 编排子程序,防 2↔9 环)。** 回合2 在 is_insolvent 时调 `9.ResolveBankruptcy(debtor, creditor, activePlayersSnapshot)`;本系统**只向下调** 5/6/8 执行、**绝不回调 2**,执行完**返回裁决** `{debtorEliminated, winnerId|INDEX_NONE, rankingEntry}` 供回合2 应用。编排序(全或全无,委 property 批量原子+重入锁 AC-33b/33c):
1. **现金侧**(经济5 F-11):债务人剩余 `Cash` → 收租破产转债权人 / 银行破产蒸发。
2. **建筑侧**(建房8,CR-4):收租破产 in-kind 随格(无显式动作);银行破产调 `LiquidateAllBuildings(debtor)` 清建筑(守"无主地无房")。
3. **归属侧**(所有权6):收租破产 `TransferOwnership(tile, debtor, creditor)` 逐格(保留 bIsMortgaged);银行破产 `ReturnTileToBank(tile)` 逐格(清 owner+bIsMortgaged)。
4. **返回裁决**:标记 debtor 待出局(`debtorEliminated=true`)+ 出局序号 + 重算 APC 后的 `winnerId|NONE`(见 CR-6),回合2 据此置 `bIsBankrupt`(player-turn 拥字段)并跳过其回合。**⚠ 回合2 应用返回值的时序契约(systems-designer R-review):** 回合2 须 **① 先置 debtor `bIsBankrupt=true` → ② 再(若 `winnerId!=INDEX_NONE`)触发 `OnGameWon`**,确保下游(VFX19/HUD16)收到 `OnGameWon` 时读 PlayerState 已是破产终态。**权威规格归 player-turn AC-12**(本系统返回值契约在此声明该时序归属回合2,实现勿先广播后写字段)。

**CR-4 建筑处理裁定(OQ-Build-3,building 委托 #9,本系统裁定)。** MVP 采:**收租破产→建筑 in-kind 随格**(house_count 随 TileIndex 走、随 `TransferOwnership` 自动归债权人,本系统无单独建筑动作;接收方持"非垄断带房"地→冻结,归 building/property 已定 provisional);**银行破产→`LiquidateAllBuildings(debtor)` 随地清零回退**(无主地不可有房,守 building CR-1)。**最终口径待 Rento 行为核查**(OQ-Build-3 保留开放,设计冻结前;若核查为经典"卖房还银行"则 propagate 改本 CR + property AC-33/building)。

**CR-5 出局态(bIsBankrupt 终态,字段归回合2)。** 裁决返回后由回合2 置债务人 `bIsBankrupt=true`(player-turn 拥有的 PlayerState 字段)。出局玩家:不再轮到(回合2 turn order 跳过,`NextActivePlayerIndex` 守卫)、不参与任何收/付租/交互、PlayerState 保留供排名。**MVP 终态不可逆**(无复活/软出局)。

**CR-6 胜负判定(APC==1,裁决权归本系统)。** 每次破产出局后重算 `ActivePlayerCount = count(p ∈ activePlayersSnapshot where p != debtor ∧ bIsBankrupt==false)`(**debtor 显式排除**,见 F-2;本系统 return-only 不直写 bIsBankrupt,故不依赖外部写入时序):`APC==1` → 该唯一存活者=胜者,返回 `winnerId` 供回合2 触发 `OnGameWon`,游戏进入 Resolved 终态。`APC==0` 为异常态(不可达:末次破产必从 APC=2→1 触发胜利;若同帧并发致 0,dev assert+log,回退取末存活快照)。

**CR-7 排名口径(net worth,OQ-Econ-5 裁定)。** 最终名次:第1=胜者(唯一存活);其余按**出局逆序**(越晚破产名次越高,出局序号由本系统在 CR-3 记录)。**net worth 口径**(供 Alpha round-cap/平局):`net_worth(player) = Cash + Σ MortgageValue(未抵押地) + Σ building_sellback(每栋)`(**= Cash + 经济 F-9 net_liquidation_value;⚠ F-9 不含 Cash,本量在其上加 Cash,实现不可直接调 F-9 当 net_worth**;已抵押地贡献 0,不含未抵押 face value 溢价——口径理由见 F-1)。**MVP 不计算 net_worth 排名**(round-cap/回合上限默认禁用,归 Alpha House Rules23);是否改用含 face value 口径留 OQ(Alpha 实装时定)。

### States and Transitions

| 实体 | 当前态 | 转移 | 触发 | 守卫 |
|---|---|---|---|---|
| Player | **Active** | → Bankrupt | `ResolveBankruptcy` 完成 | 回合2 清算环穷尽 ∧ `is_insolvent==true`(强制扣款) |
| Player | **Bankrupt(终态)** | —(MVP 不可逆) | — | 无复活/软出局(支柱①) |
| Game | **InProgress** | → Resolved(winner) | `APC==1` | 每次出局后 CR-6 检查 |
| Game | **Resolved(终态)** | — | — | 胜者已定 |

### Interactions with Other Systems

| 系统 | 数据流 / 接口 | 方向 | 谁拥有 |
|---|---|---|---|
| 回合(2) | 调 `9.ResolveBankruptcy(debtor,creditor,activePlayersSnapshot)→{eliminated,winnerId\|NONE,rankingEntry}`(**9 return-only,不回调2**);回合2 据返回置 `bIsBankrupt`(其字段)+ turn order 跳过 + 触发 `OnGameWon` | 2→9 调用 / 9 返回(**无 9→2 回调,无环**) | 2 拥流程/PlayerState/顺序;9 拥破产裁决+creditor 分支+胜负判定 |
| 经济(5) | 读 `is_insolvent`(F-10)判决(经回合2 传入);调 `F-11` 现金侧转移(债务人 Cash→债权人/蒸发) | 9→5 | 5 拥判据公式+现金转移;9 编排触发 |
| 所有权(6) | 调 `TransferOwnership`(收租,逐格保留抵押)/ `ReturnTileToBank`(银行,逐格清标记);批量原子+重入锁 | 9→6 | 6 拥归属执行;9 编排+定 creditor |
| 建房(8) | 收租破产建筑 in-kind 随格(无动作);银行破产调 `LiquidateAllBuildings(debtor)` | 9→8 | 8 拥建筑态+清算接口;9 编排 |
| 事件格(7) | 税/费/修缮触发银行破产时携 `creditor=Bank(INDEX_NONE)` 上下文(经回合2 结算路径传入) | 7→2→9 | 7 拥事件;9 消费 creditor 语境 |
| 俄罗斯轮盘(14, Alpha) | 14 depends-on 9(轮盘致死=破产/出局变体)调本系统出局接口 | 14→9 | 14 拥轮盘;9 供破产/出局接口 |
| VFX(19)/HUD(16)/音频(22) | `OnPlayerBankrupt(debtor,creditor,reason)`(9 清算编排末广播,AC-39)/ `OnPlayerRanked(player,rank)`(9 供 rankingEntry)/ `OnGameWon(winnerId)`(**9 仅返回 winnerId,由回合2 触发广播**,AC-40——9 不直接广播 OnGameWon)多播驱动清算/出局/胜利 juice(enable-not-own) | OnPlayerBankrupt 9→下游;OnGameWon 9 供 winnerId→回合2 触发→下游 | 9 供裁决事实/事件;OnGameWon 由回合2 触发;下游拥呈现 |

**无环纪律:** 9 对 2/5/6/8 **只向下调/读 + return-only**(`ResolveBankruptcy` 返回裁决,绝不回调 2 的方法),被 14(Alpha)依赖;不被 2/5/6/8 反调。`bIsBankrupt`/APC 字段归回合2,本系统经返回值驱动其更新,不直写(防 2↔9 环)。**systems-index 注:** 回合2 ResolvePhase 调 9 是 orchestration 边(类比 movement(4)→经济 push,非硬循环依赖)——须在 index 补 orchestration 注。

## Formulas

> 破产(9)是裁决/编排层,**几乎不拥有新数学**——破产判据(`is_insolvent` 经济5 F-10)、净可变现值(F-9)、现金转移(F-11)归经济5,本系统**只引用不重定义**。本系统只拥有**排名口径、存活计数、出局排序**三个本地量(F-1 是经济量的派生)。本地编号 F-1..F-3。

### F-1 net_worth 排名口径(= Cash + 经济 F-9)

`net_worth(player) = Cash + net_liquidation_value(player) = Cash + Σ MortgageValue(未抵押地) + Σ building_sellback(每栋)`

**⚠ 本量 = Cash + 经济 F-9,非"= F-9 同口径"**(F-9 不含 Cash;实现不可直接调 F-9 当 net_worth,须显式加 Cash——R4 systems/economy 双核)。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `Cash` | int32 | ≥0(economy CR-1) | 现金 |
| `MortgageValue(未抵押地)` | int32 | ≥1(未抵押)/ 0(已抵押贡献0) | 每地抵押额(棋盘1) |
| `building_sellback` | int32 | ≥0(经济 F-8 floor) | 每栋半价卖回 |
| `net_worth` | int32 | ≥0(三项非负,不可为负),上界 ~10⁵+Cash ≪ 2³¹ | 排名口径总值 |

**Output Range:** ≥0。**Example:** Cash=300 + 未抵押地(MV 100/60)+ 3栋(sellback 50 each)= 300+160+150 = **610**。
**口径选择理由(economy R-1):** 选 nlv口径(变现值,已抵押地=0)而非经典 face-value(PurchasePrice+BuildingCost)——①与破产判据 F-9/F-10 同一估值体系,防两口径并存;②round-cap 为 Alpha,MVP 不计算,Rento 核查后若应改 face value 则改此处(**OQ-Bankrupt-2** 开放)。

### F-2 ActivePlayerCount(存活计数,触发胜负)

`APC = count(p ∈ activePlayersSnapshot where p != debtor ∧ p.bIsBankrupt == false)`

**⚠ debtor 显式排除**(本系统 return-only 不直写 bIsBankrupt,APC 须在快照上显式减当前 debtor,不依赖外部写入时序;见 CR-6)。**⚠ 链式破产扩展(systems-designer R-review「返回破产者」洞):** 链式破产(A→B,B 接收资产后立即 insolvent)下,B 那层算 APC 时旧快照里 A 的 `bIsBankrupt` 仍为 false(9 从不写该字段),若只排除「当前层单一 debtor」会把刚出局的 A 当存活者、甚至作为 winner 返回——**纯同步单线程可达**(非仅异步)。故 APC 须显式排除**本次 `ResolveBankruptcy` 调用链内所有已判出局的 debtor 集合**(链根 + 各层继承 insolvent 者),而非仅当前层 debtor。**此扩展仅在 OQ-Bankrupt-1 裁定为「B 立即 insolvent」分支时生效**(条件未满足则 dormant,与 F-3 tiebreak 同前提);具体洞与排除集口径登记 OQ-Bankrupt-1。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `activePlayersSnapshot` | Player[] | 长度 [2,8] | 回合2 传入快照 |
| `debtor` | int32 | 玩家 index | 当前破产者(排除项) |
| `APC` | int32 | [0, P-1](排除 debtor 后) | 存活数 |

`APC==1` → winner;`APC==0` → 异常态 assert(纯同步单线程不可达;异步/并发路径有窗口,fallback=取 snapshot 中除 debtor 外末名 `bIsBankrupt==false` 者,无则 fatal log)。**Example:** 4人 snapshot、debtor=A、B/C/D 未破产 → APC=3;末局 snapshot=(A,B)、debtor=A → APC=1 → B 胜。

### F-3 ranking-order(出局逆序 + 同 tick tiebreak)

rank1 = 胜者(唯一存活);rank k∈[2,P] = 按 `elimination_sequence` 降序(越晚出局名次越高)。

**同 tick 双重出局 tiebreak(systems-designer,防链式破产排名不确定):** 同一 `ResolveBankruptcy` 链内多人出局(A 破产转资产给 B、B 因此 insolvent,乃至 N 层链 A→B→C…),决胜序按**链触发深度**形式化:① **链根(直接债务人)序号最小**,因继承资产被判 insolvent 者依触发先后顺次续接(深度 d 的序号 < 深度 d+1);② 同一深度并列(同 tick 多个继承 insolvent)按 `TurnOrderIndex` 升序。序号由本系统在该次调用内的**内部链计数器**顺序分配(return-only 不写状态,序号随 `rankingEntry[]` 数组返回、回合2 应用)。保排名确定性。**⚠ 前提:本 tiebreak 仅当 OQ-Bankrupt-1 裁定为「B 接收资产后立即 insolvent」分支时才触发;若 Rento 核查为「不立即 insolvent」,本规则为 dormant(不删,标条件未满足)。**

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `elimination_sequence` | int32 | [1, P-1](同 tick 多人出局时为该次调用内顺序分配的唯一序号集,链根最小) | 出局序号(本系统 CR-3 记录,内部链计数器分配) |
| `rank` | int32 | [1, P] | 名次(1=胜者) |

**Output Range:** rank ∈ [1,P]。**Example:** 4人局出局序 [A=1, B=2],存活 C/D 决出 C 胜 → rank: C=1, D=2(末存活), B=3(后出局), A=4(先出局)。
**MVP:** last-survivor + 出局逆序生效;net_worth 排名(平局)归 Alpha。链式破产(A→B)的 Rento 行为(B 继承资产后是否立即 insolvent、APC 是否一次降 2)→ **OQ-Bankrupt-1**。

## Edge Cases

- **链式破产**(A 破产 in-kind 转资产给 B,B 接收后自身 insolvent):同一 `ResolveBankruptcy` 链内多人出局——`elimination_sequence` tiebreak = **直接债务人优先 → `TurnOrderIndex` 升序**(F-3);B 接收资产后是否立即判 insolvent、APC 是否一次降 2 的 Rento 行为 → **OQ-Bankrupt-1**(MVP provisional)。
- **APC==0**(并发窗口):纯同步单线程**不可达**(末次破产必 2→1 触发胜利);异步/并发路径 dev `assert`+log + fallback 取 `activePlayersSnapshot` 中除 debtor 外末名 `bIsBankrupt==false` 者,无则 fatal log。
- **银行破产带房地**:先调建房8 `LiquidateAllBuildings(debtor)` 清建筑,再 `ReturnTileToBank` 逐格(守"无主地无房",building CR-1);建筑无现金返还(经济 F-11)。
- **收租破产 in-kind 债权人持"非垄断带房"地**:该组冻结(不能建/拆,租金按 `RentTable[house_count]`)——归 building/property 已定 provisional,本系统不额外处理。
- **债务人现金付到恰好 0**:**不破产**(economy F-10 严格 `<`;`Cash + nlv == amount_due` 算能付)。
- **仅 2 人局一人破产**:APC 2→1 即时胜利,无排名歧义(rank1=存活、rank2=破产者)。
- **已抵押地 in-kind**:保留 `bIsMortgaged`,债权人继承赎回义务(property AC-33;无继承利息,economy F-11)。
- **pay_to_each 多债主逐笔扣款致破产**:第一笔触发破产的 payee = 单一 creditor(tile-events OQ-Event-4 逐笔单债权人),其余欠款因债务人无资产而豁免——本系统消费此语义,不重判 creditor。
- **出局玩家持出狱卡/未结算牌**:出局即作废/归还牌堆——牌处置归事件格7(7 拥牌堆),本系统只触发出局态、不处置牌。
- **移交中监听器回写**:破产编排期间 property 重入锁(`bIsMidBankruptcyTransfer`)拦截监听器受控写(AC-33c),owner map 不被级联篡改。
- **同帧并发双破产(非链式)**:MVP 单线程串行,逐笔结算,不可达;Alpha 异步路径见 APC==0 fallback + OQ-Bankrupt-1。

## Dependencies

**上游(本系统 depends-on,均 Approved):**

| 系统 | 硬/软 | 接口 / 数据 | 方向 |
|---|---|---|---|
| 玩家回合(2) | 硬 | `ResolveBankruptcy` 被回合2 调用(2→9 orchestration invoke);读 `activePlayersSnapshot`;经返回值驱动 `bIsBankrupt` 写 + turn order 跳过出局者(`NextActivePlayerIndex`) | 9↔2(9 return-only,无回调) |
| 经济(5) | 硬 | `is_insolvent`(F-10)判据消费;调 `F-11` 现金侧转移;`net_liquidation_value`(F-9)口径(net_worth 派生) | 9→5 |
| 地产所有权(6) | 硬 | `TransferOwnership`(收租,逐格保留抵押)/ `ReturnTileToBank`(银行,逐格清标记);批量原子+重入锁 | 9→6 |
| 建房(8) | 硬 | `LiquidateAllBuildings(debtor)`(银行破产清建筑);收租破产 in-kind 建筑随格行为依赖 | 9→8 |

**下游(depend-on 本系统):**

| 系统 | 硬/软 | 对本系统的依赖 | 方向 |
|---|---|---|---|
| 俄罗斯轮盘(14, Alpha, Not Started) | 硬 | 轮盘致死=破产/出局变体,调本系统出局接口 | 14→9 |
| VFX(19)/HUD(16)/音频(22, Not Started) | 软 | `OnPlayerBankrupt`/`OnGameWon`/`OnPlayerRanked` 事件驱动清算/出局/胜利 juice(enable-not-own) | 下游→9 |

**双向一致性核查(本节揪出 1 gap,本轮一并改):** economy 列 9→5(is_insolvent 消费 + F-11)✓;property 列 9→6(破产裁决调改 owner)✓;building 列 9→8(in-kind + `LiquidateAllBuildings`)✓——**但 systems-index #9 行 `depends-on 2,5,6` 缺 8**(本系统 CR-3/CR-4 确调 8)→ **本轮补 systems-index #9 → 2,5,6,8**。另:`2→9 orchestration invoke`(回合2 调 `ResolveBankruptcy`,return-only 非硬循环依赖,类比 movement4→经济 push)须在 index 补 orchestration 注;player-turn CR-3.3 已引 9。 **⚠ R-review(2026-06-04 `/design-review`)修正本节失真:首版仅核 5/6/8 三侧 + index 缺 8,却遗漏最严重接缝——2↔9 接口(谁写 `bIsBankrupt`／APC 公式／胜负信号 `OnLastPlayerStanding` vs `OnGameWon`)与 player-turn(Approved)系统性矛盾(systems-designer + qa-lead 双独立确认)。** **✅ 已闭合回填(2026-06-04 `/propagate-design-change`):** propagate 工单 `docs/architecture/change-impact-2026-06-04-bankruptcy-playerturn-2to9.md` 已执行——player-turn 改对齐本档 return-only:`bIsBankrupt` 写者=回合2(据 `ResolveBankruptcy` 返回 `debtorEliminated`,**9 不直写**)／APC 胜负口径**单源归本档 F-2**(快照显式排除 debtor,player-turn F-2 降为写后计数辅助、不独立判胜)／胜负信号统一 **`OnGameWon(winnerId)`**(删 player-turn 旧 `OnLastPlayerStanding` 2→9 反向信号 + `bDegenerateTie` payload,退化局 fallback 归本档 AC-29)。fresh-grep 双侧核实:`SetBankrupt`/`bIsBankrupt` 写者、APC 公式、胜负信号方向三处**单侧唯一、双侧一致**;本档 AC-2(对回合2 公开接口 spy==0)在 return-only 下可真实断言。player-turn 因此从 APPROVED 降 NEEDS RE-REVIEW◊,三锁面(F-2/受控写面/信号 schema)待 `/design-review design/gdd/player-turn.md` 重过后回 APPROVED,#9 随之回 Approved 候选。**

**无环纪律:** 9 对 2/5/6/8 **只向下调/读 + return-only**(`ResolveBankruptcy` 返回裁决,绝不回调 2 的方法);被 14(Alpha)依赖;不被 2/5/6/8 反调。`bIsBankrupt`/APC 字段归回合2,经返回值驱动更新(防 2↔9 环)。

## Tuning Knobs

> 破产(9)是裁决逻辑层,**几乎无自有数值旋钮**——破产/胜负是结构规则,非可调数值。本系统拥有的是结构开关 + 对上游真值的引用。

**本系统拥有的旋钮:**

| 旋钮 | 值/范围 | 影响 | 过高/过低 |
|---|---|---|---|
| `WIN_CONDITION` | **LastSurvivor(MVP)** / +RoundCap(Alpha) | 胜负触发条件 | MVP 锁淘汰制(支柱①);RoundCap 须 net_worth 排名(归 House Rules23 `max_rounds`) |
| `ELIMINATION_TERMINAL` | **true(MVP)** | 出局是否可逆 | MVP 锁 true(支柱① 经典淘汰);Alpha 俄罗斯轮盘14 若引"软出局/复活"须改此处 |
| `INHERIT_MORTGAGE_FEE` | **0(MVP,免费期权)** / Alpha 参考经典 ~10%×MV | 收租破产 in-kind 债权人继承已抵押地是否计赎回继承费(AC-15) | MVP=0 简化(无继承利息,债权人继承赎回义务);economy-designer 2026-06-04 指 0=对债权人免费等待期权、滚雪球摩擦缺失,Alpha 经济平衡时定(随 OQ-Bankrupt-2 口径裁定) |

**引用(真值不归本系统,指向 source of truth):**

| 旋钮 | Source | 本系统关系 |
|---|---|---|
| `is_insolvent` 判据 | **经济5 F-10** | 9 消费判决,不定义 |
| 清算顺序 spec(mortgage-empty-first) | **经济5 CR-7.3** | 9 经回合2 编排执行,不定义顺序 |
| `net_worth` 排名口径 | **本系统 F-1 定义**(= Cash + 经济5 F-9) | MVP 不计算;Alpha round-cap 用 |
| `MortgageValue` / `building_sellback` | 棋盘1 / 经济5 F-8 | net_worth 取值,不重定义 |
| `max_rounds` / round-cap | **House Rules23(Alpha)** | 触发 net_worth 排名;MVP 默认禁用 |

**旋钮交互:** `WIN_CONDITION=RoundCap` 与 `max_rounds`(House Rules23)联动——仅当回合上限启用时 net_worth 排名(F-1)才被计算;MVP 两者皆禁用,只有 LastSurvivor + 出局逆序生效。

## Visual/Audio Requirements

> **职责边界:enable-not-own。** 本系统供 `OnPlayerBankrupt(debtor, creditor, reason)` / `OnPlayerRanked(player, rank)` 事件 + 裁决事实;`OnGameWon(winnerId)` 由本系统**返回 winnerId、回合2 触发广播**(9 不直接广播 OnGameWon——return-only,见 CR-6/AC-40);实际 VFX/动画/音效由 VFX(19)/HUD(16)/音频(22) 拥有。本节钉最低呈现契约。

- **破产清算(`OnPlayerBankrupt`)**:资产逐格易主变色——**复用所有权6 `OnOwnershipChanged` 逐格广播**(避免重复事件源;in-kind/银行回退逐格各一次),VFX19 据 old/new owner 区分"易主清算"vs"回退无主";+ 债务人出局黯然反馈 + 清算音效(concept Visual Identity §2 juice;破产清算 priority-4 Sensation)。
- **胜利(`OnGameWon`)**:封王庆祝画面 + 胜利音乐——整局高潮、concept Core Fantasy「最后只剩你笑到最后」的兑现时刻。
- **排名(`OnPlayerRanked`)**:终局排名表呈现(胜者 + 出局逆序名次)。
- **本系统职责边界**:供事件 + 裁决数据;清算逐格 juice 复用 property `OnOwnershipChanged`,出局 juice 由本系统 `OnPlayerBankrupt` 使能;胜利 juice 由本系统**返回 winnerId、回合2 触发 `OnGameWon`** 使能(9 不直接广播 OnGameWon),呈现归下游。

> **📌 Asset Spec**:Visual/Audio 需求已定义。art bible 批准后,运行 `/asset-spec system:bankruptcy` 产出出局/胜利画面、清算 VFX 的逐资产规格与生成提示。

## UI Requirements

- **破产通知**:债务人破产时全员可见通知(谁破产、欠谁的钱、出局)——归 HUD(16),本系统供 `OnPlayerBankrupt(debtor, creditor, reason)`。
- **胜利画面**:封王 + 终局排名表(胜者 + 出局逆序名次)——归 HUD(16)/主菜单大厅(20),本系统供 `OnGameWon(winnerId)` / `OnPlayerRanked(player, rank)` + rank 数据。
- **清算可视化(凑钱挣扎)**:Raising Funds 阻塞子相的抵押/卖房 UI 与触发时机(AI 即时 / 人类超时)归回合2/经济/HUD(economy OQ-Econ-8),**非本系统**。
- **本系统职责边界**:供事件 + 裁决数据(破产者/债权人/出局序/胜者/名次);呈现/布局/输入路由归 HUD(16)/主菜单(20)/UMG。

> **📌 UX Flag — 破产与胜负(9)**:本系统有终局 UI(破产通知、胜利画面、终局排名表)。Pre-Production 阶段须对终局/胜利相关屏运行 `/ux-design` 产出 UX spec,再写 epics;引用 UI 的 story 应引 `design/ux/[screen].md` 而非本 GDD。

## Acceptance Criteria

> **测试分层**:`[Logic]`=自动化单测 headless `-nullrhi`,BLOCKING PR gate,`tests/unit/bankruptcy/`;`[Integration]`=集成/文档化 playtest,BLOCKING,`tests/integration/bankruptcy/`;`[Advisory]`=code-review,`production/qa/evidence/bankruptcy-manual/`。
> **Fixture 纪律(所有 in-kind AC 强制)**:in-kind 移交断言用 **identity-check**——逐对象枚举:移交前 debtor 持有的每一 TileIndex,移交后 `owner==creditor` ∧ `house_count` 完整保留;**严禁用「移交总额==经济 F-9」作断言**(in-kind 非现金等价,已抵押地 F-9 记 0 但资产对象仍转出——economy R4 OQ-Bankrupt-3 教训)。下游 mock(经济5/地产6/建房8)以接口 spy 注入。

### A. 接口契约与无环纪律(CR-3 架构)

- **AC-1** `[Logic]`(CR-3)**GIVEN** `ResolveBankruptcy(debtor,creditor,snapshot)` 被调用,**WHEN** `debtor==creditor`,**THEN** dev ensure + 返回 `{debtorEliminated=false, winnerId=INDEX_NONE}`,不调任何下游 5/6/8、不移交。
- **AC-2** `[Logic]`(无环纪律)**GIVEN** `ResolveBankruptcy` 全路径执行完,**WHEN** 对回合2 公开接口加调用 spy,**THEN** spy 计数==0(系统9 绝不回调回合2,防 2↔9 环)。
- **AC-3** `[Logic]`(CR-5/无环)**GIVEN** `ResolveBankruptcy` 全路径执行,**WHEN** 在 `PlayerState.bIsBankrupt` 写路径(`SetBankrupt` 及任何写该字段的 player-turn 公开接口)注入 dev-build `ensure`+log 桩,**THEN** headless `-nullrhi` 下 ensure **未触发**(9 全程不直写 bIsBankrupt;字段经返回值 `debtorEliminated=true` 由回合2 写)。测试文件 `tests/unit/bankruptcy/bankruptcy_no_direct_write_test`,**BLOCKING PR gate**(无环纪律是核心架构安全阀)。 **⚠ 标签订正(qa-lead/CD R-review):** 原 `[Advisory→Logic]` 非合法 CI 桶——MVP Blueprint 期以 dev-build ensure-based 运行期断言落地为合法 `[Logic]` BLOCKING(headless 可跑);C++ 强封装落地后**同一文件**改写为编译期类型隔离测试(移除 ensure 改链接/编译错误),不降级。
- **AC-4** `[Logic]`(CR-3)**GIVEN** 执行期,**WHEN** `activePlayersSnapshot` 内各 `bIsBankrupt` 字段加写 spy,**THEN** 写入==0(快照只读,APC 经显式排除算,不写快照)。

### B. 破产触发守卫(CR-1)

- **AC-5** `[Logic]`(CR-1)**GIVEN** 入参携 `is_insolvent=true`(经济5 算、经回合2 传入),**WHEN** 对经济5 `is_insolvent`/`GetNLV`/`GetCash` 加 spy,**THEN** 系统9 内部不再独立计算破产判据,仅消费传入 `true`(职责边界防腐层)。
- **AC-6** `[Logic]`(CR-1)**GIVEN** 系统9 公开接口列表,**THEN** 不存在 `DeclareBankruptcy`/`SurrenderBankruptcy` 等主动宣告方法;破产唯一入口 `ResolveBankruptcy`。

### C. Creditor 分支路由(CR-2)

- **AC-7** `[Logic]`(CR-2 收租)**GIVEN** debtor 持 {T1(未抵押),T2(已抵押)},creditor=玩家P(`!=INDEX_NONE`),**WHEN** Resolve,**THEN** 调 `TransferOwnership(*,debtor,P)`(非 `ReturnTileToBank`),且不调 `LiquidateAllBuildings`。
- **AC-8** `[Logic]`(CR-2 银行)**GIVEN** creditor=`INDEX_NONE`,**WHEN** Resolve,**THEN** 调 `ReturnTileToBank` + `LiquidateAllBuildings(debtor)`;不调 F-11 `Credit(creditor,…)`(现金蒸发)。
- **AC-9** `[Logic]`(CR-2)**GIVEN** 同场景 creditor 分别传 玩家P / INDEX_NONE,**WHEN** Resolve,**THEN** 两次产出完全不同的下游编排(in-kind vs 回退),证明系统9 不在内部重判收款方。

### D. 清算编排序与原子性(CR-3)

- **AC-10** `[Logic]`(CR-3 ①→③)**GIVEN** 收租破产 Cash=100,tiles={T1,T2},creditor=玩家P,**WHEN** Resolve,**THEN** spy 序:`economy.F-11 Credit(P,100)` 先于所有 `property.TransferOwnership`(现金侧在归属侧前)。
- **AC-11** `[Logic]`(CR-3 ②→③)**GIVEN** 银行破产 tiles={T1(hc=2),T2},creditor=INDEX_NONE,**WHEN** Resolve,**THEN** `building.LiquidateAllBuildings(debtor)` 先于所有 `property.ReturnTileToBank`(守"无主地无房")。
- **AC-12** `[Logic]`(CR-3 批量原子)**GIVEN** {T1,T2,T3},mock `TransferOwnership(T2)` 触发 ensure(模拟中途异常),**WHEN** Resolve,**THEN** 3 格要么全转要么全不转(batch 原子,非逐格独立写);重入锁 `bIsMidBankruptcyTransfer` 无论成功/异常必复位(不挂起)。
- **AC-13** `[Logic]`(CR-3/property AC-33c)**GIVEN** 移交期 `OnOwnershipChanged` 监听器尝试调所有权6 写接口(`Buy`/`Mortgage`/`TransferOwnership`),**WHEN** 锁 `bIsMidBankruptcyTransfer==true` 期间,**THEN** 写接口前置拒绝(ensure+log),owner map 不被级联篡改;移交完成后锁复位、写接口恢复。

### E. In-Kind 移交正确性(CR-2 收租分支,identity-check)

- **AC-14** ⭐ `[Logic]`(CR-2/OQ-Bankrupt-3)**GIVEN** debtor 赛前持 tiles={T1(未抵押,hc=2), T2(已抵押,hc=0), T3(未抵押,hc=0)},creditor=玩家P,**WHEN** Resolve 完成读 owner map 与 house_count,**THEN** 逐格 identity-check:`owner[T1]==P ∧ house_count[T1]==2`、`owner[T2]==P ∧ bIsMortgaged[T2]==true`、`owner[T3]==P`、debtor 无残留 owned tile。**严禁用「economy.F-9(debtor) == Σ到达creditor」作断言**(in-kind 非现金等价)。
- **AC-15** `[Logic]`(edge/economy F-11)**GIVEN** T1 已抵押 in-kind 转 玩家P,**WHEN** Resolve,**THEN** `bIsMortgaged[T1]==true` 保留 ∧ 系统9 不调经济5 任何"收取继承利息"接口(MVP 无继承利息,债权人继承赎回义务)。
- **AC-16** `[Logic]`(edge/property)**GIVEN** T1 已抵押,creditor=INDEX_NONE,**WHEN** Resolve 后读所有权6,**THEN** `owner[T1]==INDEX_NONE` ∧ `bIsMortgaged[T1]==false`(owner 与抵押标记同格原子清零,守 Unowned 不变式:无主不可抵押)。

### F. 建筑处理(CR-4)

- **AC-17** `[Logic]`(CR-4)**GIVEN** 收租破产 T1(hc=3),creditor=玩家P,**WHEN** Resolve,**THEN** `LiquidateAllBuildings` 调用==0;`TransferOwnership` 后 `house_count[T1]==3`(建筑随格,house_count 以 TileIndex 为键与 owner 解耦)。
- **AC-18** `[Logic]`(CR-4/edge)**GIVEN** 银行破产 {T1(hc=2),T2(hc=0)},creditor=INDEX_NONE,**WHEN** Resolve,**THEN** `LiquidateAllBuildings(debtor)` 恰 1 次且先于所有 `ReturnTileToBank`;终态 `house_count[T1]==0`(无主地无房守住)。
- **AC-19** `[Logic]`(CR-4/edge)**GIVEN** 银行破产、debtor 有建筑,**WHEN** Resolve,**THEN** 经济5 `Credit` 在 `LiquidateAllBuildings` 路径中调用==0(建筑清算无现金返还)。

### G. 现金侧(CR-3 ①/经济 F-11)

- **AC-20** `[Logic]`(CR-3①)**GIVEN** 收租破产 Cash=300,creditor=玩家P,**WHEN** Resolve,**THEN** `Debit(debtor,300)`+`Credit(P,300)` 各恰 1 次且金额相等(现金侧原子,无创造/丢失货币)。
- **AC-21** `[Logic]`(CR-3①/CR-2 银行)**GIVEN** 银行破产 Cash=200,creditor=INDEX_NONE,**WHEN** Resolve,**THEN** `Debit(debtor,200)` 恰 1 次;`Credit` 调用==0(现金流向银行 sink,蒸发)。

### H. 出局态与返回值(CR-5)

- **AC-22** `[Logic]`(CR-5)**GIVEN** 正常破产路径完成,**WHEN** 读返回值,**THEN** `debtorEliminated==true`,`rankingEntry` 含有效 `elimination_sequence≥1`。
- **AC-23** `[Logic]`(CR-5/Tuning ELIMINATION_TERMINAL)**GIVEN** 玩家已 `debtorEliminated=true` 标记出局、回合2 已置 `bIsBankrupt=true`,**WHEN** 再次 `ResolveBankruptcy(sameDebtor,…)`,**THEN** dev ensure + 返回 `debtorEliminated=false`(幂等守卫,终态不可逆)。
- **AC-24** `[Logic]`(CR-5)**GIVEN** 玩家出局后,**THEN** 所有权6 `GetOwner`/经济5 `GetCash` 等历史快照接口仍可被排名逻辑读取(PlayerState 不销毁)。

### I. 胜负判定 APC(CR-6/F-2)

- **AC-25** `[Logic]`(CR-6/F-2 ⚠)**GIVEN** snapshot=[A,B,C,D],debtor=A 且 A.bIsBankrupt 仍 false(回合2 未写),B/C/D 未破产,**WHEN** 内部算 APC,**THEN** APC==3(debtor A 显式排除,不依赖外部写入时序)。
- **AC-26** `[Logic]`(CR-6/F-2)**GIVEN** snapshot=[A(debtor),B],B 未破产,**WHEN** Resolve,**THEN** APC==1 → 返回 `winnerId==B.PlayerIndex` ∧ `debtorEliminated=true`。
- **AC-27** `[Logic]`(edge)**GIVEN** 2人局 snapshot=[A,B],debtor=A,**WHEN** Resolve,**THEN** `winnerId==B`(rank1=B,rank2=A 无歧义)。
- **AC-28** `[Logic]`(CR-6)**GIVEN** snapshot=[A(debtor),B,C](B/C 存活),**WHEN** Resolve,**THEN** `winnerId==INDEX_NONE`(游戏继续,不触发 OnGameWon)。
- **AC-29** `[Logic]`(CR-6/F-2/edge fallback——空快照)**GIVEN** snapshot=[A]、debtor=A(模拟异常并发态),**WHEN** Resolve,**THEN** dev assert+log;fallback 取「除 debtor 外末名 `bIsBankrupt==false` 者」无候选 → fatal log,不崩溃、不产 `winnerId=-1` 无效胜者。(纯同步单线程不可达,测空快照健壮性。)(锚定编号——player-turn L322/L330 引「bankruptcy AC-29」指本退化局 fallback 簇,AC-29b 为其补充边界,引用不破。)
- **AC-29b** `[Logic]`(CR-6/F-2/edge fallback——非 debtor 全破产)**GIVEN** snapshot=[A,B]、debtor=A、B.`bIsBankrupt==true`(模拟外部脏写/异步态),**WHEN** Resolve 内部 APC==0,**THEN** dev assert+log;fallback 在快照中找不到 `bIsBankrupt==false` 候选 → fatal log,不崩溃、不产无效 `winnerId`(测 fallback 真实边界:所有非 debtor 成员均 `bIsBankrupt==true` 时不误返破产者为胜者)。

### J. 排名口径(CR-7/F-1/F-3)

- **AC-30** `[Logic·CI-stub]`(CR-7/F-3)**GIVEN** 4人局出局序 A=1(先),B=2,C=3,存活 D=胜者,**WHEN** 读所有 `rankingEntry`,**THEN** rank1=D,rank2=C(最晚出局),rank3=B,rank4=A(出局逆序)。**⚠ CI-stub:** 断言读 `rankingEntry` 字段,依赖 `FRankingEntry` struct schema(OQ-Bankrupt-6②未定义)——stub 文件 `bankruptcy_ranking_order_stub_test`,CI 报「待 FRankingEntry schema 定义」而非静默跳过(同 AC-31/32 诚实标注)。
- **AC-31** `[Logic·CI-stub]`(F-1 ⚠)**GIVEN** player Cash=300、经济5 F-9(net_liquidation_value)=200(不含 Cash),**WHEN** 系统9 算 net_worth,**THEN** ==500 且 spy 验证实现调了 `GetCash(player)` 并加在 F-9 之上(不直接以 F-9 当 net_worth)。**MVP 不计算 net_worth(round-cap 禁用)**;stub 文件 `bankruptcy_networth_ranking_stub_test`,CI 报"待 Alpha round-cap ADR 落地后解 skip"而非静默跳过。
- **AC-32** `[Logic·CI-stub]`(F-1)**GIVEN** player 仅持 1 格已抵押地(MortgageValue=100)+Cash=200,**WHEN** 算 net_worth,**THEN** ==200(已抵押地贡献 0)。
- **AC-33** `[Logic·CI-stub]`(F-3 同 tick tiebreak)**GIVEN** 链式破产同 tick:A=直接债务人(原始 debtor),B=因继承资产 insolvent,**WHEN** 读两者 `elimination_sequence`,**THEN** A < B(直接债务人优先);仍并列则按 `TurnOrderIndex` 升序。**⚠ CI-stub + dormant 前提:** ① 断言依赖 `FRankingEntry.elimination_sequence` 字段(OQ-Bankrupt-6②未定义)——stub 文件 `bankruptcy_tiebreak_stub_test`;② 本 AC 仅当 OQ-Bankrupt-1 裁定为「B 接收资产后立即 insolvent」分支时**active**;若裁定「不立即 insolvent」则本 AC dormant(测试套件 skip 并标条件未满足,非失败)。

### K. 关键 Edge Case

- **AC-34** `[Logic]`(edge)**GIVEN** 经济5 `is_insolvent(player, amount_due=500, preaggregated_nlv=500)`==false(Cash+nlv==due,严格 `<` 不成立),**WHEN** 判据 false,**THEN** 回合2 不调 `ResolveBankruptcy`(调用==0;入口前置守卫,Cash 付到恰好 0 不破产)。
- **AC-35** `[Logic]`(edge/事件7 OQ-Event-4)**GIVEN** pay_to_each 第 1 笔支付后 insolvent,creditor=玩家P1(第一笔 payee),**WHEN** `ResolveBankruptcy(debtor,P1,snapshot)`,**THEN** 全资产 in-kind 转 P1,对 P2/P3 欠款豁免;spy 验证系统9 未向 P2/P3 移交(消费传入 creditor,不重判多债主)。
- **AC-36** `[Logic]`(edge/building CR-1)**GIVEN** 银行破产 {T1(hc=3),T2(hc=0)},creditor=INDEX_NONE,**WHEN** Resolve,**THEN** spy 序:`LiquidateAllBuildings` 调用时 T1/T2 仍属 debtor(owner 未清零),`ReturnTileToBank` 在其后;终态 `house_count[T1]==0 ∧ owner[T1]==INDEX_NONE`。
- **AC-37** `[Logic]`(edge/building-upgrade OQ-Build-3 冻结)**GIVEN** in-kind 后 creditor 持 T1(hc=2,该组其他格属他人,非垄断),**WHEN** 后续回合尝试 `BuildHouse(T1)`/`SellHouse(T1)`,**THEN** `CanBuildHouse`==false(垄断前置失败);租金仍按 economy F-2 `else` 分支 `RentTable[house_count]` 收(hc=2 正常);系统9 移交时不触发额外建筑处理(归 building/property provisional)。
- **AC-38** `[Integration]`(edge/事件7)**GIVEN** debtor 持出狱卡,`ResolveBankruptcy` 返回 `debtorEliminated=true`,**WHEN** 回合2 据返回值处理出局,**THEN** 事件7 `ReturnCardToDeck` 被调用(牌归还牌堆),出局后不再持卡。(事件7 拥牌堆,系统9 只触发出局态。)

### L. 事件广播(enable-not-own 契约)

- **AC-39** `[Logic]`(Visual/Audio·UI)**GIVEN** 清算编排序 4 步全部完成后,**WHEN** 读 `OnPlayerBankrupt` 广播,**THEN** 恰广播 1 次,payload `{debtor,creditor,reason}` 字段非空,且广播发生在资产移交**之后**(VFX19/HUD16 见已是终态 owner map);`reason` 枚举:creditor=玩家→`Rent`,creditor=INDEX_NONE→`{Tax,Fee,RepairFee}`(具体枚举值待实现期定,见 OQ-Bankrupt-6)。
- **AC-40** `[Logic]`(CR-6/UI)**GIVEN** APC==1 场景,**WHEN** 回合2 据 `winnerId` 触发 `OnGameWon`,**THEN** `OnGameWon(winnerId)` payload==返回值 winnerId;APC>1 场景 `OnGameWon` 调用==0。
- **AC-41** `[Logic·CI-stub]`(CR-7/UI)**GIVEN** 4人局出局序已确定,系统9 逐次产 `rankingEntry`,**WHEN** 累计所有 `OnPlayerRanked` 广播,**THEN** P 位玩家中产出 P-1 个(胜者 rank=1 经 OnGameWon 处理),`rank` 字段 2..P 严格递增、无重复、无空缺。**⚠ CI-stub:** 断言读 `OnPlayerRanked` 的 `rank` 字段(随 `FRankingEntry` schema,OQ-Bankrupt-6②未定义)——stub 文件 `bankruptcy_ranked_broadcast_stub_test`,CI 报「待 FRankingEntry schema 定义」。

### M. 性能与帧预算

- **AC-42** `[Integration]`(return-only 编排/原子性)**GIVEN** 最大规模:8人局 debtor 持全部 22 块可购买地(各 hc=5),下游为轻量 mock,**WHEN** `ResolveBankruptcy` 完整执行,**THEN** 总执行时间 <16.6ms(60 FPS 帧预算),主线程纯同步不 yield/defer(无协程/异步等待);实机须目标最低 spec 硬件复测,mock 版 CI headless 验。
- **AC-43** `[Integration]`(批量原子/生命周期)**GIVEN** debtor 持 22 格、银行破产最重路径(LiquidateAllBuildings + 22× ReturnTileToBank),**WHEN** Resolve 完成后运行内存检查(UE Automation Memory Profiler headless),**THEN** 临时 TileIndex 枚举列表无残留引用,`bIsMidBankruptcyTransfer` 复位(锁不挂起)。

**合计 44 条**:41 BLOCKING Logic(含 AC-30/31/32/33/41 共 5 条 CI-stub)+ 3 BLOCKING Integration(AC-38/42/43)+ 0 Advisory。覆盖 CR-1..7 全、F-1..3 全、Edge Cases 全。**R-review 订正(2026-06-04 `/design-review`):** AC-3 原 `[Advisory→Logic]` 非合法 CI 桶 → 订正合法 `[Logic]`(dev-build ensure,BLOCKING);AC-29 拆 AC-29a(空快照)+ AC-29b(非 debtor 全破产 fallback 边界,+1 条);AC-30/33/41 原 `[Logic]` 隐性依赖未定义 `FRankingEntry` → 诚实改标 `[Logic·CI-stub]`(同 AC-31/32);AC-33 加 dormant 前提(OQ-Bankrupt-1)。

## Open Questions

| OQ | 问题 | Owner | 目标解决时机 |
|---|---|---|---|
| **OQ-Bankrupt-1** | 链式破产(A 破产 in-kind 转资产给 B,B 接收后自身 insolvent)的 Rento 行为:B 接收资产后立即判 insolvent 还是变 solvent?APC 是否一次降 2?同 tick 出局序 tiebreak 已定 provisional(直接债务人优先 → `TurnOrderIndex` 升序,F-3),Rento 核查确认。**⚠ 附属:「返回破产者」洞(systems-designer R-review)** —— 若裁定为「B 立即 insolvent」,F-2 的 APC 计算**必须排除本次 `ResolveBankruptcy` 调用链内所有已判出局的 debtor 集合**(非仅当前层单一 debtor),否则旧快照下会把链内刚出局者(其 `bIsBankrupt` 尚未由回合2 写入)当存活者甚至 winner 返回(纯同步单线程可达,见 F-2 ⚠ 链式扩展);并须一并裁定链式调用模型(嵌套递归 vs 回合2 迭代——决定 `elimination_sequence` 全局唯一性,见 F-3)。 | game-design / Rento 核查 | 设计冻结前 |
| **OQ-Bankrupt-2** | net_worth 排名口径:本系统采 **nlv 变现值口径**(F-1,= Cash + 经济 F-9,已抵押地=0)而非经典 **face-value**(PurchasePrice + BuildingCost);MVP 不计算(round-cap 禁用),Alpha round-cap 实装前须 Rento 核查定口径(**口径方向**:nlv 相对 face-value 低估「持大量**未抵押**高价值地产、现金少」的玩家;另一独立方向——已抵押地继承赎回费负债未计入 net_worth、会略微高估重度抵押者;两点均待 Alpha 口径裁定,参 economy-designer 2026-06-04 审)。 | Rento 核查 / House Rules23 | Alpha round-cap 实装前 |
| **OQ-Bankrupt-3 ✅ RESOLVED 2026-06-04(`/propagate-design-change`,fresh-grep 双侧核实)** | 闭合 economy 三处。fresh-grep 暴露实际范围**比登记更窄**——economy R1 已自修 AC-27②/AC-30/F-11 头:**①** economy `OQ-T-Econ-2`(line 479)「9 移交总额 == F-9」与本档 `AC-27②` identity-check 自相矛盾 → **已改 identity-check + 标 RESOLVED**;**③** economy F-11(line 247)+ AC-31(line 446)「建筑拆除」执行主体悬空 → **已补注归 9 调建房8 `LiquidateAllBuildings`、地产回退归 6 `ReturnTileToBank`**;**②(已闭合不改)** economy F-11 头(line 245)+ AC-30 早已声明「本系统只执行现金侧、不写 owner map、归属转移归 9 经 6/8」——登记时假设 economy 全是旧措辞,实为已自修,无真矛盾。`docs/architecture/change-impact-2026-06-04-bankruptcy.md`。 | producer ✅ | — |
| **OQ-Bankrupt-4** | 非破产路径 APC==1(玩家主动退出 / 断线 / AI 强制移除)是否触发 `OnGameWon`?归会话管理/主菜单20 还是本系统?MVP Pass'N Play 单机暂不涉(无退出路径);Full Vision 联网须定。 | 会话管理 / 联网25 | Full Vision |
| **OQ-Bankrupt-5(共享 ADR)** | 出局态/胜负态容器与受控写强度,随所有权6 **OQ-Prop-1 owner map 生命周期 ADR**;`bIsBankrupt` 字段归回合2(player-turn PlayerState),本系统经返回值驱动其写(不直写,防 2↔9 环)。 | 架构期(OQ-Prop-1 ADR) | 下游实现前 |
| **OQ-Bankrupt-6(实现期,本地无传播)** | 系统9 返回值与执行期边界三项待定(qa-lead AC 审揪出的正交盲区):**①** `ResolveBankruptcy` 执行期是否禁止存档(存档21 接缝——Raising Funds/清算瞬态不应中途序列化)、由谁阻止;**②** 返回值 `FRankingEntry` struct schema(字段:`playerIndex` / `elimination_sequence` / `net_worth_snapshot`?)——AC-30/33/41 的"读 rankingEntry"断言基准依赖此 struct 定义;**③** `OnPlayerBankrupt.reason` 枚举具体值(`Rent` / `Tax` / `Fee` / `RepairFee`)——HUD16/VFX19 "欠谁的钱"文案依赖,AC-39 验枚举区分。三项均本地(无跨档传播),实现期定义。 | lead-programmer / 架构期 | 破产实现期前 |
| **OQ-Bankrupt-7(CONCERN→carry,game-designer F-5)** | Raising Funds 挣扎弧(差额显示／倒计时压力／卖房动画戏剧性／AI 陷困表现)的设计 owner 未明:本系统只拥裁决终点,「绝处的挣扎」「出局坏笑」两段情感弧的呈现节奏委托回合2 CR-3.3／HUD16,但无单一 GDD 明确拥有该体验设计责任。建议在 player-turn／HUD(16)／UX-spec 明确(/consistency-check 或 /review-all-gdds 层追踪)。 | UX / HUD16 / player-turn | Pre-Production UX |

**已在设计中关闭(记录):** 胜负条件(MVP last-survivor 淘汰制)/ 建筑处理(OQ-Build-3 裁定 = 收租 in-kind 随格 + 银行 `LiquidateAllBuildings`,provisional 待 Rento 核查)/ creditor 分支(收租→对手 in-kind、银行→回退+蒸发)/ 排名(出局逆序 + net_worth provisional)/ 无环架构(9 return-only 编排,不回调 2)均已裁定。systems-index #9 depends-on 补 8 已闭合。
