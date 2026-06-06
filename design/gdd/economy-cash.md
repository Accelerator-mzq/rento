# 经济与现金 (Economy & Cash)

> **Status**: APPROVED-WITH-FOLLOWUPS (re-review full 2026-06-02：5 前轮 blocker 全闭合;R2 落 B-1/B-2 诚实硬线 + P-2/faucet 软化 + OQ-Econ-10;留 P-1 board-data propagate + Recommended AC)
> **Author**: msc + agents
> **Last Updated**: 2026-06-02
> **Implements Pillar**: ③运气×策略交织（策略骨干）、②社交博弈（收租/互坑）；约束于 ④易上手
> **System**: #5 in systems-index design order | Core | Economy | MVP | ⚠ bottleneck high-risk
> **Review Mode**: lean（Section D 公式 / Section H 验收派发 systems-designer + economy-designer / qa-lead）

<!-- 骨架由 /design-system 创建。逐节设计。
     关键约束（Phase 2 上下文，供续做参考）：
     - 本 GDD 是 registry 占位常量的平衡推导 owner：go_salary_amount(200)/tax_amount_income_classic(200)/
       tax_amount_luxury_classic(100)/monopoly_rent_multiplier(2,source 待迁入)/mortgage_warning_threshold(0.6)
     - board-data 契约：RentTable 只存未翻倍 base；Property[0..5]/Railroad[0..3]/Utility DiceMultiplierTable[0..1]；
       翻倍/档位查找/数持有数归经济或所有权(6) 运行时；发薪权威 passed_go>0 时发 passed_go×SalaryAmount
     - dice：Utility 租金 = 骰点(Total∈[2,12]) × 倍率
     - player-turn：PlayerState.Cash 受控写（AC-35 软约束/C++ 强封装待 OQ-1 ADR）
     - 编码标准：数值数据驱动、不硬编码；引擎 UE5.7 Blueprint 为主
-->

## Overview

经济与现金系统是《骰子大亨》的**金钱中枢**:它拥有每位玩家的现金(`PlayerState.Cash`,经受控写),并裁决游戏里**一切金钱流动的规则**——过/停起点发薪、落地收租付租、购买地产、抵押与赎回、缴税。它有两个面:对内是一个**数据/公式层**——持有现金状态、并把棋盘(1)提供的"未翻倍租金 base、地价、抵押额"通过运行时公式(垄断翻倍、车站档位、公用骰点倍率、赎回溢价)换算成实际收付金额;对玩家则是**直接可感的体验源头**——收租滚雪球的碾压快感、付不起租的肉痛、现金吃紧时"买还是留着保命"的抉择,以及现金归零滑向破产的紧张。它是系统分解中标注的 **bottleneck 高风险系统**:几乎所有玩法交互(移动落地、事件格、建房、交易、AI 决策)的金钱后果最终都在这里结算,因此它的平衡数值(薪水额、税额、垄断翻倍系数 ×2、抵押率与赎回费率)是全盘节奏与"运气×策略"手感(支柱③)的总开关,数值错误会让整局玩法崩坏。它**不拥有**地产归属(归所有权6)、建房规则(归建房8)、破产判定流程(归破产9——本系统只提供"现金能否支付/是否为负"的判据)、随机(归骰子3)与棋盘静态数据(归棋盘1);它只拥有**钱的真值与钱怎么动**。它做得好时,玩家全部注意力都在"这把收租够不够翻盘、要不要抵押搏一把"的博弈上(支柱②社交互坑),而非怀疑算账有没有错。

## Player Fantasy

> *(lean 模式未派发 `creative-director`——本节由主会话起草,production 前建议人工复核 framing。)*

经济系统兑现的是 concept 核心幻想里最硬的那一块:**"精明的地产大亨"**——你布下地产、看着对手一个个付不起租金相继破产,最后只剩你笑到最后。它服务两极对立又互补的情感:

- **直接交互层(掌控与博弈)**:每一次落地、每一回合结束,玩家都在和现金做有意义的算计——"这块地买不买?现金留着够不够付下一脚的租?要不要抵押两块地搏一把集齐整组、把租金翻倍?"这是支柱③"运气×策略"里**策略那一极**的主战场:骰子给运气,经济给玩家**对抗运气的掌控手段**(*支柱③:建房提供对抗运气的掌控;交易〔Alpha〕亦是掌控手段,但 MVP 不含——见下方 MVP 范围声明*)。看着自己的现金数字因一次垄断收租而跳涨,是这个系统最直接的爽感。

- **基础体验层(钱的重量)**:即便玩家没有主动操作,经济也在背后持续制造情绪——过起点发薪的小确幸、踩中对手满级地产时现金哗啦减少的**肉痛**、现金逼近零时的窒息感。钱必须有"重量",收租和付租才成为支柱②社交互坑的燃料(*支柱②:乐趣源于玩家之间的收租、交易与使坏*)。

这两极合起来制造 concept 点名的两种戏剧性:**滚雪球碾压的成长快感**(领先者垄断收租、雪球越滚越大)与**逆风翻盘的绝处逢生**(落后者抵押搏命、靠一轮收租咬住不掉队)。它的成功标准是:玩家把全部情绪投向"这把钱够不够、敢不敢搏"的博弈本身,而**绝不怀疑算账是否准确**——经济的算术正确性必须隐形可信,就像骰子的公平性一样,是体验成立的底线。

> **MDA 锚点**:本系统主要服务 **Challenge/精通**(现金流管理的策略深度)与 **Fellowship/社交**(收租互坑),并驱动 concept 的雪球/翻盘 Dynamics。`Sensation`(金币飞溅 juice)由 VFX(19)/HUD(16) 承载,非本系统。

> **风险备注**:concept 已点名 Design Risk"运气占比过高让偏策略玩家挫败"与"后期翻盘无望拖沓"——这两条的缓解(可调局长/破产加速/翻盘机制)与经济平衡强相关,登记于 Tuning Knobs / Open Questions,不在本节关死。

> **〔MVP 范围声明 — 翻盘为"待兑现"承诺〕**:上文"逆风翻盘绝处逢生"是**全愿景目标**,MVP 内为待兑现承诺。MVP 内落后玩家的**主动**翻盘工具仅抵押延命 + 收租咬住;真正的翻盘注入器(交易11/拍卖12/命运之轮13/策略卡15)全在 Alpha。"绝处逢生"戏剧性依赖 Alpha 上线——本 GDD 诚实标注此为设计未完成项,非默示 MVP 已交付。同理支柱③"策略那一极"MVP 内主要由建房时机的现金流管理承载。

## Detailed Design

### Core Rules

**CR-1 现金所有权与受控写。** 本系统拥有每位玩家的现金值 `PlayerState.Cash`(int32,单位:游戏货币)。所有现金读写经本系统的受控接口(`Credit(player, amount)` 入账 / `Debit(player, amount)` 出账),沿用 player-turn 的受控写契约(AC-35,BP 软约束 / C++ 强封装待 OQ-1 ADR)。**不变式:`Cash ≥ 0` 恒成立**——任何会使 `Cash < 0` 的扣款不得直接落地,必须先进入凑钱/破产流程(CR-7)。**amount 非负契约(R1,blocker #2):`Credit`/`Debit` 的 `amount` 必须 ≥ 0;`amount < 0` 是非法调用(如上游 F-1 溢出为负),运行时 assert 拒绝(丢弃 + dev log)、不修改 `Cash`、不广播事件——防负 amount 绕过 `Debit` 路径静默击穿 `Cash≥0`。** 开局初始现金 = `STARTING_CASH`(Tuning Knob)。

**CR-2 发薪(过/停起点)。** 当玩家本次移动的 `passed_go > 0`(由移动(4)经 `AdvanceIndex` 算出并连同 `SalaryAmount` 传入,见棋盘 CR-2 契约),本系统发薪 `passed_go × SalaryAmount`(`Credit`)。`passed_go ≤ 0`(未过/逆向穿越)**一律不发**。停在起点格 `passed_go = 1`,发一次;`SpecialAction=CollectSalary` 仅是 UI 标记,**不构成第二次发薪触发器**(防双重发薪)。

**CR-3 收租结算(落地自动收租)。** 玩家落在一格地产/车站/公用(`Property`/`Railroad`/`Utility`)时,本系统按归属状态**自动**结算,无需地主手动索取:
- **无主** → 不收租;暴露买地决策点(决策点归回合 `ResolvePhase`,执行扣款归本系统 CR-4)。
- **自己拥有 / 已抵押 / 同格落地为拥有者本人** → **不收租**。
- **他人拥有且未抵押** → 自动从落地者 `Debit`、向拥有者 `Credit`,金额由 F 节租金公式算出:
  - **Property**:`RentTable[房屋数]` base;若拥有者**集齐整组(垄断)且该地无房**,租金 = `RentTable[0] × monopoly_rent_multiplier`(×2,本系统拥有此系数);已建房则按 `RentTable[房屋数]`(垄断翻倍只作用于无房 base,经典规则)。
  - **Railroad**:`RentTable[持有车站数−1]`(持 1..4 站 → 档位 0..3)。
  - **Utility**:`骰点 × DiceMultiplierTable[持有公用数−1]`(持 1/2 → 档位 0/1)。骰点来源(**PULL,本系统在 ResolvePhase 算租时取,非上游 push**):正常落地由**回合(2)** 暴露的「当前程掷骰上下文」`Total` PULL(见移动 CR-3.1:移动不缓存/不推送 `Total`);"前进到最近公用"等事件需额外掷骰时,由**事件格(7)** 把额外骰点作 `dice_total` 注入 F-4。本系统经 F-4 显式参数消费(line 176「不读缓存」),不反向读移动状态。
  - **归属事实来源**:垄断与否 / 同类持有数 / 已抵押标记由地产所有权(6)提供;**房屋数(`house_count`)归建房(8)提供**(6 不持,防 6↔8 环;6 快照 + 8 house_count 经回合2 `ResolvePhase` 聚合传入);base 金额读棋盘(1);本系统取这些算最终租金并执行现金转移。

**CR-4 购买地产的现金腿(事务归所有权6,方向6→5)。** 买地事务 `Buy(tileIndex, playerId)` 由**所有权(6)拥有**(决策点归回合2 `ResolvePhase`);6 在其事务内调本系统 `Debit(玩家, PurchasePrice)` 执行扣款腿(**6→5**)。本系统**只执行扣款**:校验 `Cash ≥ PurchasePrice`,成立则 `Debit` 并返回成功,现金不足则购买不可用(不进入凑钱流程——买地是可选行为,非强制债务)。**本系统不登记归属、不通知 6**——归属 map 由 6 在 `Debit` 成功后自行登记(归属 map 归 6 拥有,本系统不持有"谁拥有什么";economy 不反调 6,保 5↔6 无环,见所有权 CR-2 / OQ-Prop-2)。

**CR-5 抵押与赎回。** 拥有者可抵押其未建房地产:
- **抵押**:抵押事务 `Mortgage(tileIndex, playerId)` 由**所有权(6)拥有**;6 调本系统 `Credit(拥有者, MortgageValue)` 执行放款腿(**6→5**)。本系统只执行 `Credit`,**不标记/不通知 6**——`bIsMortgaged` 标记由 6 在 `Credit` 成功后自置(economy 不反调 6,保无环)。已抵押地产**不收租**(CR-3)。**带房不可抵押的 `house_count==0` 前置由决策点(回合2/AI10)在调 `Mortgage()` 前校验**(决策点能读建房8 的 `house_count`),**非本系统在 `Credit` 内校验**(`Credit` 是通用入账接口,不知该笔是抵押还是收租;卖房归建房(8))。
- **赎回**:赎回事务 `Unmortgage(tileIndex, playerId)` 由**所有权(6)拥有**;6 调本系统 `Debit(拥有者, MortgageValue × (1 + 赎回费率))`(`赎回费率` 本系统拥有,真值见 D 节/Tuning)执行扣款腿。本系统只执行 `Debit`,成立后**由 6 自行解除抵押标记**(本系统不通知 6)。现金不足无法赎回。
- **赎回价显示侧读接口(R-2 propagate ← 地产卡 UI #17 OQ-PC-8)**:本系统额外暴露**纯函数只读** `GetUnmortgageCostForDisplay(MortgageValue: int32) → int32`(返回 F-6 `unmortgage_cost = MortgageValue + ceil(MortgageValue×fee_num/fee_den)`,**无副作用、不改任何状态、不触事件**)。供地产卡 UI(17)显示赎回价(CR-3⑤/AC-42):#17 读棋盘(1)的 `MortgageValue` 传入本函数,**#17 不自算 ceil**——赎回费率/取整口径**单源归本系统**(防与 #17 取整分歧)。本函数不破 5↔6 无环(纯计算,不读归属/不反调 6)。

**CR-6 缴税(现金回收 sink)。** 玩家落在 `Tax` 格,本系统 `Debit(玩家, TaxAmount)`(税额读棋盘(1),真值由本系统平衡推导)。税款流向银行(蒸发,不进任何玩家),是经济的现金 sink。不足额则进入 CR-7。

**CR-7 付款不足 → 凑钱或破产(全额或破产)。** 当一笔**强制**扣款(租金/税)超过玩家现金:
1. 本系统不部分扣款、不允许负现金;先广播"需筹资"信号,玩家须通过抵押(CR-5)/卖房(建房8)/交易(11,Alpha)把 `Cash` 提到 ≥ 应付额。
2. 若变卖**全部可变现资产后仍不足** → 本系统向破产胜负(9)提供"无力支付"判据(`Cash + 全部可变现价值 < 应付额`),破产流程归 9:债务人**剩余全部资产转移给债权人**(收租场景=对手;税场景=银行),债务人出局。本系统只负责**现金侧**结算(现金转移 + F-9 变现口径);**地产/建筑的归属 in-kind 转移由破产9 经所有权6(`TransferOwnership`)/建房8 执行,本系统不写 owner map**;本系统亦不拥有"宣告破产"这一裁决。
3. **确定性自动清算兜底(R1 blocker #4,防无界阻塞 — OQ-Build-1 RESOLVED)**:Raising Funds 是回合(2)`ResolvePhase` 的**阻塞子相**,若玩家 solvent-by-assets(nlv 够付)却不行动(挂机/断线/AI 决策死循环),框架须有界终止。**本系统拥有且仅拥有:清算顺序 spec + 现金判据(`Cash ≥ 应付额?` / `is_insolvent`)。执行驱动归回合(2):`ResolvePhase` 调用 6.`Mortgage` / 8.`ForcedSellNextBuilding`，economy 不直接调 6 或 8(防 5→6/5→8 反向环)。** 清算顺序 spec(止损优先,mortgage-empty-first):`while Cash < amount_due { 若存在可直接抵押地(owner==player ∧ 未抵押 ∧ (非Property ∨ house_count==0)):→ 回合2调 6.Mortgage(MV 最小的可抵押地);否则若存在建筑(house_count>0):→ 回合2调 8.ForcedSellNextBuilding(F-4 全盘最高档);否则:→ break → is_insolvent → 破产(9) }`。止损优先理由:抵押可赎回(零半价损失),卖建筑永久半价亏损;经典规则禁止抵押组内有房的地产,故带房地须先由 8 卖房腾空才可抵押。现金够即早停;顺序不影响 `is_insolvent`(NLV 是 order-independent 和),只影响早停后剩余资产与玩家损失额。**触发时机(AI 即时 / 人类超时时长)与 UI 提示归回合(2)/AI(10)/UX 裁定(OQ-Econ-8),本系统只拥有"清算顺序确定性 + 有界终止"契约。**

**CR-8 现金转移原子性与银行模型。** 玩家间转移(租金)是原子操作:落地者扣减额 == 拥有者入账额,无中间态、无创造/丢失货币。银行(发薪源、税收 sink、抵押放款)视为**无限资金池**(经典大富翁银行不会破产),发薪/抵押放款不受银行余额限制。

**CR-9 职责边界(本系统不拥有什么)。** 本系统**只产出钱的结算**:不拥有地产归属(所有权6)、建房/卖房规则(建房8)、破产宣告流程(破产9,本系统仅供判据)、随机(骰子3)、棋盘静态金额数据(棋盘1,本系统消费其 base 并拥有平衡真值)。决策点(买/不买、抵押与否)由回合(2)/AI(10) 触发,本系统执行其金钱后果。

### States and Transitions

**本系统主要是服务,而非逐回合状态机**(同棋盘/骰子)。它拥有的唯一持久状态是**每位玩家的现金值 `Cash`**——一个数值(经 `Credit`/`Debit` 受控写、随存档序列化),不是 FSM。地产的"已抵押"标记是运行时状态,**归地产所有权(6)拥有**(以 `TileIndex` 为键),本系统只读它来决定收租与否(CR-3/CR-5)。

但 CR-7 引入一个**结算瞬态:筹资中(Raising Funds)**——当一笔强制扣款超过现金时,玩家进入该瞬态直至解决。本系统**发起并裁决**该瞬态的进出判据,瞬态本身**寄宿于回合(2)的 `ResolvePhase`**(作为其阻塞子相,回合不推进到下一玩家直到解决):

| 状态 | 含义 | 转换 |
|---|---|---|
| **Solvent(偿付正常)** | 现金足以应付当前结算 | 强制扣款 ≤ Cash:直接结算,留在 Solvent |
| **Raising Funds(筹资中)** | 强制扣款 > Cash,等待变现(阻塞 ResolvePhase) | ① 玩家(或不行动时的**确定性自动清算兜底**,CR-7.3)经抵押(CR-5)/卖房(8)/交易(11)使 `Cash ≥ 应付额` → 执行扣款 → **回 Solvent**;② 变现穷尽(F-9 nlv 全清)仍 < 应付额 → `is_insolvent` 移交破产(9) → **Bankrupt**。**有界终止保证**:资产有限 + 自动清算兜底,两出口必达其一,无第三(无界阻塞)出口 |
| **Bankrupt(破产,终态)** | 无力支付且无可变现资产 | **现金侧**转移由本系统结算;地产/建筑归属 in-kind 转移归破产(9)经所有权6/建房8;出局裁决归破产(9);该玩家退出经济交互 |

**无"交易中/结算中"独立瞬态:** 发薪、收租、缴税、买地、赎回均为**同步单步**结算(单帧内完成 `Credit`/`Debit`),无中间态。唯一例外是 Raising Funds(需玩家多步变现),它有界终止——要么凑够付款回 Solvent,要么变现穷尽进 Bankrupt,不存在第三出口。

**现金状态序列化:** `Cash` 随存档(21)序列化(每玩家一值)。Raising Funds 瞬态**不应在其中途存档**(与 player-turn"回合中途特定阶段可存档"对齐由回合(2)裁定;本系统建议筹资未决时不落存档点,避免序列化"欠多少钱"的瞬态债务)——登记为 Open Question 待存档(21)/回合(2)协同。

> 边界:本系统不持有"谁拥有哪块地""建了几栋房""哪块被抵押"——这些是 6/8 的运行时状态,以 `TileIndex` 为键。本系统只持有 `Cash`(以 player 为键)。

### Interactions with Other Systems

本系统是**金钱结算中枢**:它从其他系统取"事实"(归属、base 金额、骰点),算出金额,执行现金转移,并广播结果供呈现。下表列出数据流与归属:

| 系统 | 数据流 | 谁拥有 |
|---|---|---|
| 玩家回合(2) | `ResolvePhase` 触发落地结算(收租/买地决策点);**ResolvePhase 暴露「当前程掷骰上下文」`Total` 供本系统 Utility 租 PULL(holder=回合2)**;`Cash` 经受控写;Raising Funds 寄宿于 `ResolvePhase` 阻塞子相 | 回合拥有流程/决策点与当前程 `Total` 单源;本系统执行金钱后果 |
| 移动(4) | 收移动 push 的 `(passed_go, SalaryAmount)` 发薪(CR-2);**Utility 租 `dice_total` 不由移动投递**——本系统在 ResolvePhase 从回合(2)掷骰上下文 PULL(移动 CR-3.1) | 移动拥有位移与发薪 push 时机;本系统拥有发薪/租金公式并自取 Utility 骰点 |
| 棋盘(1) | 经 `GetTileData(index)` 读 `PurchasePrice`/`RentTable`/`DiceMultiplierTable`/`MortgageValue`/`TaxAmount`/`SalaryAmount` 的 **base** | 棋盘供 base;本系统拥有平衡真值与运行时换算 |
| 地产所有权(6) | **收(push,经 ResolvePhase 聚合)**:owner / 是否垄断 / 同类持有数 / 已抵押标记(供 CR-3 算租;**房屋数 `house_count` 归建房8、不在6**);**现金腿**:买地/抵押/赎回事务由 6 拥有并调本系统 `Debit`/`Credit`(**6→5**),**本系统不写归属、不通知 6**(保 5↔6 无环) | 6 拥有归属 map 与抵押标记;本系统拥有现金转移 |
| 建房(8) | 建房/卖房的现金扣减/返还由本系统执行(`Debit`/`Credit`) | 8 拥有建造规则与房屋数;本系统执行金钱 |
| 破产胜负(9) | **供**"无力支付"判据(`Cash + 全部可变现价值 < 应付额`)+ 执行债务人→债权人**现金侧**转移(地产/建筑归属转移归 9 经所有权6/建房8) | 9 拥有破产宣告/出局裁决;本系统供判据 + 执行现金侧转移 |
| 事件格(7) | 缴税(CR-6);机会/命运卡的金钱效果(收/付现金、"前进到最近X"额外掷骰的骰点经 7/4 传入)由本系统执行 | 7 拥有牌堆/触发;本系统执行金钱效果 |
| 交易(11)/拍卖(12)(Alpha) | 玩家间现金+资产转移 / 中标扣款由本系统执行 | 11/12 拥有谈判/出价流程;本系统执行金钱 |
| 命运之轮(13)/策略卡(15)(Alpha) | 金钱类效果(奖惩现金)经本系统结算 | 各机制拥有规则;本系统执行金钱 |
| HUD(16) | 监听 `OnCashChanged` 显示现金;读应付额提示 | 呈现层拥有显示 |
| 地产卡 UI(17) | 调纯函数 `GetUnmortgageCostForDisplay(MV)` 显示赎回价(CR-5,R-2 propagate);**F-1 同构维护债(OQ-PC-11)**:本系统改 `property_rent`/`railroad_rent`/`utility_rent` piecewise 须触发 #17 重核 F-1/AC-11 期望表 | 呈现层拥有显示;本系统拥租金公式口径单源 |
| VFX(19) | 监听经济事件呈现金币飞溅/收租/破产 juice | 呈现层拥有动画 |
| 存档(21) | 序列化每玩家 `Cash` | 存档拥有序列化 |

**事件(本系统广播,`DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`,payload `USTRUCT(BlueprintType)`):**
- `OnCashChanged(Player, OldCash, NewCash, EChangeReason)` — 任何现金变动(reason ∈ {Salary, Rent, Purchase, Mortgage, Unmortgage, Tax, Build, Trade, Bankruptcy})。HUD(16)/VFX(19)/音频(22) 挂接。
- `OnRentPaid(Payer, Payee, Amount, TileIndex)` — 收租完成(供 VFX(19) 金币飞溅 + 音频(22) 收租音效 + 互坑反馈)。
- `OnInsufficientFunds(Player, AmountDue, AmountShort)` — 进入 Raising Funds 瞬态(供 UI 弹"需筹资"、回合阻塞)。
- `OnBankruptcyDeclared(Debtor, Creditor)` — 破产结算转移完成时广播(裁决归 9,本系统在执行完**现金侧**转移后广播金钱侧完成;地产归属转移由 9 经 6 执行)。

**接口稳定承诺:** `Credit`/`Debit`/`GetCash`/`GetUnmortgageCostForDisplay`(纯函数只读,R-2 新增)签名稳定;事件 payload 字段只增不改语义;`EChangeReason` 枚举只扩不改既有项(给下游 6/7/8/9/16/17/19/21 的稳定保证)。
>
> **方向 reconcile 裁定(#22 propagate,VFX19 CR-5 / audio CR-4 三档同步)**:本系统**不为收/付方向扩枚举**——`EChangeReason` 维持九值无方向位(`Rent` 收/付共用一值)。下游收/付方向**一律从 payload 派生**:`OnCashChanged` 的 `NewCash−OldCash` delta 符号定数字正负向(AC-33)+ `OnRentPaid` 的 `Payer/Payee` 视角定金币飞溅方向(AC-34)。下游 VFX19 CR-5 / audio22 CR-4 的 reason 分类表须以本枚举**真实九值**为键(`Salary`/`Rent`/`Purchase`/`Mortgage`/`Unmortgage`/`Tax`/`Build`/`Trade`/`Bankruptcy`),不得用 `RentReceived`/`RentPaid`/`PassGo`/`JailFine`/`BuildCost` 等本枚举未定义的方向子值/别名;方向另从上述 delta/视角派生。详见 Visual/Audio Requirements「好/坏语境」节。

**禁双重发薪/双重收租(防御契约):** 发薪唯一由 `passed_go>0` 授权(CR-2);收租唯一由"他人拥有且未抵押的落地"授权(CR-3)。任何系统不得绕过本系统接口直接改 `Cash`(受控写,沿用 player-turn AC-35 软/硬约束)。

> 跨系统义务(回链下游 GDD,Phase 5 登记 systems-index 继承义务表):所有权(6) 须实现"数持有数/判垄断/抵押标记 set-clear"接口供本系统取租;破产(9) 须消费本系统"无力支付"判据;事件格(7) 须把"前进到最近X"的额外骰点传入本系统。

## Formulas

> 由 `economy-designer`(平衡值)+ `systems-designer`(数学严谨)派发提案,主会话整合 + 用户决策(全经典值 / 所得税 flat 200 / 起始 1500+快速档 750 / 破产移交经典忠实简化)。**determinism 原则:所有非整数结果用整数 num/den + 显式取整方向(联网重放 OQ-3 防 desync);所有数组查找守 `count≥1`/`clamp`;`is_mortgaged` 短路置 0 先于表查找。**

### F-1 发薪 Salary
`salary = clamp(max(passed_go, 0), 0, PASSED_GO_SAFE_MAX) × SalaryAmount`,**仅当 `passed_go > 0`**(gate 在 passed_go 非 salary)

> **R1 溢出硬防护(blocker #2)**:`ensure` 在 Shipping 编译为空,不能作唯一防线。乘法前对 `passed_go` 做**运行时 clamp** 到 `[0, PASSED_GO_SAFE_MAX]`(非仅 dev ensure):`PASSED_GO_SAFE_MAX` 取保守上限 **1000**(远超任何合法多圈)。**SalaryAmount 上界依赖(R2 honesty)**:经典值 200 时 1000×200=2×10⁵ 极安全;但 clamp 只护 passed_go 不护 SalaryAmount——若地图编辑器(26)设 SalaryAmount > ⌊(2³¹−1)/1000⌋≈2.147×10⁶ 仍溢出(故旧"≪2³¹"措辞在该假设下仅约 7% 裕量,已更正)。派发棋盘(1)/编辑器(26)加载期校验 `SalaryAmount ≤ 2,000,000`(OQ-Econ-10)。下界 0 与 gate `passed_go>0` 双重确保 `salary≥0`、永不产生负 `Credit`。`passed_go > PASSED_GO_SAFE_MAX` 时 clamp 兜底 + dev `ensure(passed_go ≤ 12)` 暴露上游(移动4/传送链)bug。

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `passed_go` | int32 | 正常 -2..+2;**运行时 clamp 到 [0,1000]**(dev `ensure ≤12` 暴露异常) | 移动(4)传入 |
| `PASSED_GO_SAFE_MAX` | int32 | =1000(锁定) | 溢出硬防护上限,本系统拥有(单系统内部,不入 registry) |
| `SalaryAmount` | int32 | =200 | 棋盘 Go 格读 |
| `salary` | int32 | ≥0(clamp 下界 0 保证) | `Credit` 额 |

**Output Range:** ≥0。例:passed_go=1, Salary=200 → 200。passed_go≤0 → 不发(不触发 `Credit`,amount==0 早返不广播事件)。passed_go=10⁷(异常溢出输入)→ clamp 到 1000 → 不溢出、不负值、dev ensure 报警。

### F-2 地产租金 Property Rent(piecewise,×2 只作用无房 base)
```
if is_mortgaged:                        rent = 0          // 短路先于表查找
elif is_monopoly and house_count == 0:  rent = RentTable[0] × monopoly_rent_multiplier
else:                                   rent = RentTable[clamp(house_count, 0, 5)]
```
| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `house_count` | int32 | 0..5(0=无房,5=酒店;clamp 防 6 越界) | **建房(8)供**(6 不持,防 6↔8 环;经 ResolvePhase 聚合) |
| `is_monopoly` | bool | — | 所有权(6)判集齐整组 |
| `monopoly_rent_multiplier` | int32 | =2(锁定) | 本系统拥有 |
| `RentTable[6]` | int32[] | base(未翻倍) | 棋盘供 |

**Output Range:** ≥0。例:无房垄断 base=2 → 2×2=4;3房 → RentTable[3]。**酒店租金绝不翻倍**(只 index 0)。

> **数据包络约束(R1 blocker #3,防"建首房降租"反激励)**:必须恒满足 `RentTable[1] ≥ RentTable[0] × monopoly_rent_multiplier`,否则"垄断无房租金(base×2)> 建 1 房租金"——玩家花钱建第一栋房反而**降租**,摧毁建房投资激励。经典值满足(棕:RentTable[0]=2→×2=4 < RentTable[1]=10),但 board-data 现仅校验 RentTable 自身单调(`[i]≤[i+1]`),**不守此跨公式约束**。本约束由本系统拥有定义、**校验责任派发给棋盘(1)加载期**(见 Dependencies 跨系统义务 + OQ-Econ-9),违反 warning;自定义棋盘(地图编辑器26)亦受此约束。

### F-3 车站租金 Railroad Rent
`rent = RentTable[clamp(station_count−1, 0, 3)]`,**守 `station_count≥1` 否则 rent=0**(防 `RentTable[-1]` 崩;count 来自所有权(6),不假设其正确)

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `station_count` | int32 | 1..4(0 守门置 0 租) | 所有权(6)数持有 |
| `RentTable[4]` | int32[] | 持 1..4 站档位 | 棋盘供 |

**Output Range:** ≥0。例:持 2 站 → RentTable[1]。

### F-4 公用租金 Utility Rent
`rent = dice_total × DiceMultiplierTable[clamp(utility_count−1, 0, 1)]`,守 `utility_count≥1` 否则 0

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `dice_total` | int32 | [2,12] | **显式参数传入**(正常=本次移动骰;"前进到最近公用"=事件额外骰);不读缓存 |
| `utility_count` | int32 | 1..2(0 守门置 0) | 所有权(6)数持有 |
| `DiceMultiplierTable[2]` | int32[] | 持 1/2 倍率 | 棋盘供 |

**Output Range:** ≥0。例:持 1, dice=7, mult=4 → 28。

### F-5 抵押放款 Mortgage Payout
`payout = MortgageValue`(`Credit`)。**前置(未抵押 / 无房)由调用方保证,非 economy 校验**——所有权(6) `Mortgage()` 前置校验 `bIsMortgaged==false`(防重复抵押双放款,property-ownership AC-10),决策点(回合2/AI10)校验 `house_count==0`(带房须先卖,见抵押边界 + OQ-T-Econ-1)。**economy `Credit` 是通用入账接口、读不到抵押标记/房屋数,不在 `Credit` 内校验**(OQ-Prop-7 propagate 修正:此前 F-5 误写"economy 读 is_mortgaged/无房前置",与本档抵押边界自相矛盾)。

### F-6 赎回 Unmortgage Cost(整数 ceil,杀 float)
```
fee = (MortgageValue × fee_num + fee_den − 1) / fee_den    // 整数向上取整
unmortgage_cost = MortgageValue + fee
```
| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `MortgageValue` | int32 | 1..PurchasePrice | 棋盘供 |
| `fee_num`/`fee_den` | int32 | =1/10(经典 10%) | 本系统拥有,整数表示防跨平台 desync |
| `unmortgage_cost` | int32 | >MortgageValue | `Debit` 额 |

**Output Range:** >MortgageValue。例:MV=100 → fee=10 → 110;MV=75 → fee=ceil(7.5)=8 → 83。fee 恒 ≥1(堵"1×1.1→1 免费赎回"退化)。

### F-7 缴税 Tax
`debit = TaxAmount`(flat;所得税 200 / 奢侈税 100,银行 sink 蒸发)。**固定额,无百分比变体**(避免引入 float + 净资产依赖)。

### F-8 建筑卖回 Building Sellback
`sellback = (BuildingCost × sell_num) / sell_den`(整数 floor),`sell_num/sell_den = 1/2`(经典半价)

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `BuildingCost` | int32 | >0 | 棋盘供 |
| `sell_num`/`sell_den` | int32 | =1/2 | **比率归本系统,"有哪些建筑"归建房(8)** |
| `sellback` | int32 | ≥0 | 卖回返还额 |

**Output Range:** ≥0。例:BuildingCost=100 → 50。

### F-9 净可变现价值 Net Liquidation Value
🔴 **CR-7 判据与破产(9)转移共用同一式**(防"被判破产但其实付得起")
```
net_liquidation_value(player) = Σ MortgageValue(t)      [t = 玩家拥有的未抵押可购买地]
                              + Σ building_sellback(b)   [b = 玩家拥有的每栋建筑,按 F-8]
```
已抵押地贡献 0(已借过)。

> **展开注(OQ-Build-2 RESOLVED):** `Σ building_sellback(b) = Σ_t house_count[t] × floor(BuildingCost[t] / 2)`(每栋单独 floor,**非** floor 先合后算)。例:BuildingCost=75,house_count=3 → per-building floor(75/2)=37,合计 3×37=**111**;若先算 floor(75×3/2)=floor(112.5)=**112**,差 1 — 经典半价须逐栋取整,不得合并再 floor。**建筑枚举 `house_count` 不由本系统直接读建房(8)——直读 = 5→8 反向环。枚举结果由回合(2)调 8.`GetPlayerBuildings(player)→[(tile,house_count)]` 聚合后作 `preaggregated_nlv` 传入本系统**,见 F-10 签名。交叉参考:建房 F-6 `sellback = floor(BuildingCost/2)` 单栋定义与此一致。

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `MortgageValue(t)` | int32 | 1..PurchasePrice | 每地抵押额(棋盘) |
| `building_sellback(b)` | int32 | ≥0 | 每栋卖回(F-8) |
| `net_liquidation_value` | int32 | ≥0(单玩家独占全盘约 10⁵ ≪ 2³¹,int32 安全) | 全变现现金等价 |

**口径一致性(R1,旧"不变式"措辞已修正):** F-9 是破产(9)移交的**资产枚举口径**——破产(9)须用同一枚举(同一段实现)清点移交资产(AC-27③)。**注意非财务等价**:F-11 把已抵押地 in-kind 转给债权人,债权人继承其赎回义务(须付 MV×(1+fee) 方变现),故已抵押地对债权人即时变现价值非正;F-9 一致地记其为 0。AC-27 验证的是**枚举完整性 + 单一入口**,非"转移现金等价"(旧措辞是循环同义反复——用 F-9 口径估 F-11 转入永真、不证伪任何东西,已废)。

### F-10 无力支付判据 Insolvency Predicate
`is_insolvent(player, amount_due, preaggregated_nlv) = (Cash + preaggregated_nlv < amount_due)`,**严格 `<`**(付到恰好 0 算能付、不破产)

> **签名变更说明(OQ-Build-2 RESOLVED):** F-10 不再内部计算 NLV——内部计算需直读建房(8)的 `house_count` 枚举 = 5→8 反向环。`preaggregated_nlv` 由**回合(2)在调 F-10 前聚合**:回合2 调 8.`GetPlayerBuildings(player)` 取建筑清单,结合 6 的抵押/MV 数据算出 nlv,作为参数传入。本函数只做 `Cash + preaggregated_nlv < amount_due` 的纯谓词判断,不持有资产枚举逻辑。

| 变量 | 类型 | 范围 | 说明 |
|---|---|---|---|
| `Cash` | int32 | ≥0(CR-1 不变式) | 当前现金 |
| `preaggregated_nlv` | int32 | ≥0 | 由回合(2)聚合 6(MV) + 8(`GetPlayerBuildings` sellback)后传入;等价于 F-9 `net_liquidation_value` 的外部计算结果 |
| `amount_due` | int32 | >0 | 强制应付(租/税) |
| `is_insolvent` | bool | — | true → 移交破产(9) |

例:Cash=50,preaggregated_nlv=120,due=170 → 170<170=false(能付到 0);due=171 → true。

### F-11 破产资产移交 Bankruptcy Transfer
M-3,经典忠实简化;裁决/出局归破产(9),本系统执行**现金侧**结算(地产/建筑归属 in-kind 转移由破产9 经所有权6/建房8 执行,本系统不写 owner map):
- **收租破产(债权人=玩家)**:`creditor.Cash += debtor.Cash`;债务人全部地产/建筑 **in-kind 转给债权人(保留抵押状态)**;**MVP 不收继承利息**。债务人出局。
- **银行破产(税/银行=债权人)**:债务人全部地产**回退为无主(unowned)**,建筑拆除无返还(**建筑拆除归破产9 调建房8 `LiquidateAllBuildings` 执行、地产回退归所有权6 `ReturnTileToBank`,本系统只执行现金侧、不拆建筑、不写 owner map**),现金蒸发。**MVP 不走拍卖**(拍卖=Alpha 12,届时改走拍卖)。

> **已抵押地 in-kind 的价值口径(R1,配合 AC-27)**:收租破产时债权人继承已抵押地及其赎回义务(变现须先付 MV×(1+fee)),故其即时变现价值非正——这是经典忠实的刻意简化、**非财务等价**。F-9 与破产(9) 一致记已抵押地为 0(AC-27③ 单一枚举口径),不声称"转移现金等价"。

---

### 平衡值与现金流模型(本 GDD 拥有真值)
| 常量 | MVP 真值 | 性质 | 依据 |
|---|---|---|---|
| `STARTING_CASH` | **1500**(+ 可选 `STARTING_CASH_FAST=750`) | 工程/平衡 | 经典验证起始;快速档压缩拖沓 |
| `go_salary_amount` | **200** | 平衡(faucet) | 经典;主现金注入 |
| `tax_amount_income` | **200** flat | 平衡(sink) | flat 易上手④ |
| `tax_amount_luxury` | **100** flat | 平衡(sink) | 经典 |
| `monopoly_rent_multiplier` | **2** | 锁定 | board-data CR-4,source 迁入本 GDD |
| `unmortgage_fee` | **1/10**(num/den) | 平衡(sink) | 经典 10%,整数表示 |
| `building_sellback_ratio` | **1/2**(num/den) | 平衡 | 经典半价 |
| `mortgage_warning_threshold` | **0.6** | 数据质量警戒线 | 抵押率>60%(越出经典约0.5规则区)触发 board 加载警告;**非套利防范**——见下方"套利轴(已删除)" |

**faucet/sink:** 注入(faucet)=过 GO 薪水、抵押放款;回收(sink)=税、买地付银行、赎回溢价、(银行破产)资产蒸发。**预期**30 分钟局趋于平衡偏通缩(地产逐步私有化使银行注入减少),服务雪球而不通胀——此为设计预期、**非已量化结论**,须原型期 playtest 验证(AC-40,Advisory),且数值前提依赖 OQ-Econ-6 闭合。

> **拖沓杠杆(→ Tuning Knobs / Open Questions)**:优先级 `STARTING_CASH`(默认 1500 / 快速 750)> `go_salary_amount` > 回合/圈数上限(按 net worth 排名裁定胜者)> 税额 > 赎回费。默认全经典,压缩在原型期调参。

> **跨系统义务(Phase 5 登记)**:net_liquidation_value 消费方=破产(9)须用同式;building_sellback 比率归本系统、建筑清单归建房(8);所有权(6)须供 station/utility count + is_monopoly + is_mortgaged(**house_count 归建房8,经 ResolvePhase 聚合,6 不供**)。**(R1 新增)棋盘(1)加载期须校验 `RentTable[1] ≥ RentTable[0]×monopoly_rent_multiplier`(F-2 数据包络,防建首房降租),违反 warning——此约束跨 board↔economy,须经 propagate-design-change 派发给已 Approved 的 board-data(OQ-Econ-9)。**

## Edge Cases

**现金与支付**
- **若一笔扣款后现金恰为 0**:允许,玩家**不破产**(`is_insolvent` 用严格 `<`,付到 0 算能付,F-10)。
- **若强制扣款(租/税)> 现金**:进入 Raising Funds 瞬态(CR-7),玩家须抵押/卖房/交易凑够;不部分扣款、不允许负现金。
- **若变卖全部可变现资产后仍 < 应付额**(`is_insolvent==true`):移交破产(9),按 F-11 移交资产,债务人出局。
- **若 `amount == 0` 的转移**(发薪 passed_go=0、自有地租金=0 等):早返,不执行 `Credit`/`Debit`、不广播 `OnCashChanged`(避免噪声事件)。
- **若买地时现金不足**:购买不可用(灰显),**不进入** Raising Funds——买地是可选行为,非强制债务。
- **若赎回时现金不足**:赎回不可用,该地保持抵押。

**收租归属边界**
- **若落地为无主可购买格**:不收租,暴露买地决策点(归回合 ResolvePhase)。
- **若落地为自己拥有的地产**:不收租(CR-3)。
- **若落地地产已抵押**:不收租(`is_mortgaged` 短路置 0,先于任何 RentTable 查找,F-2)。
- **若 Railroad/Utility 的 `station_count`/`utility_count` 运行时返回 0**(所有权(6)竞态/计数 bug):**守门置 rent=0,不崩**(防 `RentTable[-1]`),dev `ensure(count≥1)` 暴露上游 bug。不假设所有权(6)恒正确。
- **若 `house_count` 越界**(>5 或 <0,建房(8) bug):`clamp(0,5)` 兜底 + dev `ensure`。
- **若 `RentTable[1] < RentTable[0] × monopoly_rent_multiplier`**(数据包络违反,自定义棋盘/手填错误):垄断建首房**降租**的反激励(F-2 数据包络约束)。本系统运行时不崩(按式照算),但棋盘(1)加载期应 warning(`MonopolyBaseExceedsOneHouse{TileIndex}`,归 OQ-Econ-9 派发);经典 DA 满足、不触发。

**发薪边界**
- **若 `passed_go ≤ 0`**(未过 GO / 逆向穿越):不发薪(gate 在 passed_go)。
- **若 `passed_go` 异常大**(后退牌/传送循环 abuse 致多圈):仍按式计算,但 dev `ensure(passed_go ≤ 12)` 暴露异常;防 `passed_go × SalaryAmount` int32 溢出。
- **若停在 GO 格**:`passed_go=1` 发一次;`SpecialAction=CollectSalary` 不构成第二次发薪(防双重发薪,CR-2)。

**抵押边界**
- **若抵押带房地产**:拒绝(须先经建房(8)卖房);**`house_count==0` 前置由决策点(回合2/AI10)在调所有权(6) `Mortgage()` 前校验**(决策点能读建房8),**非本系统在 `Credit` 内校验**(`Credit` 通用入账,不知该笔为抵押;本系统读不到 `house_count`)。
- **若重复抵押已抵押地产**:拒绝——**由所有权(6) `Mortgage()` 前置校验 `bIsMortgaged==false`**(防双放款,property-ownership AC-10);economy `Credit` 读不到抵押标记、不在 `Credit` 内校验(同 house_count,OQ-Prop-7)。
- **若抵押率数据异常**(`MortgageValue > PurchasePrice`):由棋盘(1)加载期校验已拒绝(board-data Edge Cases),本系统运行时不再遇到。

**破产移交边界**
- **若收租破产**(债权人=玩家):债务人现金 + 全部地产/建筑 in-kind 转债权人(保留抵押状态,MVP 不收继承利息,F-11)。
- **若银行破产**(税/银行=债权人):债务人地产回退无主、建筑拆除无返还、现金蒸发(MVP 不拍卖,F-11)。
- **若债务人破产后仅剩 1 名未破产玩家**:破产(9)在 `ResolveBankruptcy` 返回 `winnerId`,回合(2)据此触发 `OnGameWon(winnerId)`(信号名 + 广播者统一,见 bankruptcy CR-6/AC-40、player-turn CR-6),本系统停止经济交互。
- **若同一笔破产的债权人本身已出局**:不可达(债权人=当前收租地主,必为存活玩家;税场景债权人=银行)。dev `ensure`。

**确定性/精度**
- **所有非整数运算**(赎回费 F-6、卖回 F-8)用整数 num/den + 显式取整(ceil/floor),**无 float**,跨平台/跨编译配置位级一致(联网重放 OQ 前提)。
- **现金转移原子性**:租金额算一次存局部变量,同值 `Debit(payer)` + `Credit(payee)`,绝不为两次调用各自重算(防造币/丢币,CR-8)。

> 局末平局(所有玩家 net worth 相等、或回合上限触发需排名裁定)归破产胜负(9)/Open Questions,本系统供 net worth 计算口径(= net_liquidation_value 的 face-value 变体,待 9 裁定是否含未抵押溢价),不在本节关死。

## Dependencies

### 上游(本系统依赖)
| 系统 | 强度 | 接口 |
|---|---|---|
| 玩家回合(2) | 硬 | 提供当前玩家、`PlayerState.Cash` 受控写容器、`ResolvePhase` 触发结算/买地决策点/Raising Funds 阻塞子相 |
| 棋盘(1) | 硬(间接) | `GetTileData(index)` 读 `PurchasePrice`/`RentTable`/`DiceMultiplierTable`/`MortgageValue`/`TaxAmount`/`SalaryAmount` 的 base |

### 下游(依赖本系统)
| 系统 | 强度 | 读/写本系统 |
|---|---|---|
| 地产所有权(6) | 硬 | 买地/抵押/赎回事务由 6 拥有、调本系统 `Debit`/`Credit` 执行现金腿(6→5,economy 不反调 6);**并经 push 向本系统供 owner/is_monopoly/count/is_mortgaged 供算租**(`house_count` 归建房8、不在6 快照;见下方双向说明) |
| 事件格(7) | 硬 | 缴税、机会/命运卡金钱效果经本系统;传入"前进到最近X"额外骰点 |
| 建房(8) | 硬 | 建/卖房现金扣减/返还经本系统;F-9 建筑枚举经回合(2)`ResolvePhase` 聚合 8.`GetPlayerBuildings` 传入(economy 不直读8,防5→8环) |
| 破产胜负(9) | 硬 | 消费 `is_insolvent` 判据(F-10)+ 按 F-11 移交;**net_liquidation_value 须与本系统 F-9 同式** |
| AI(10) | 硬 | 读现金/资产估值做买地/抵押/建房决策 |
| HUD(16) | 硬 | 监听 `OnCashChanged` 显示现金、应付额提示 |
| 地产卡 UI(17) | 软(R-2 新增) | 调纯函数 `GetUnmortgageCostForDisplay(MV)` 显示赎回价(OQ-PC-8);**F-1 同构维护债(OQ-PC-11)**:本系统改租金 piecewise 须触发 #17 重核 F-1/AC-11 |
| 游戏反馈 VFX(19) | 硬 | 监听 `OnRentPaid`/`OnCashChanged`/`OnBankruptcyDeclared` 呈现金币 juice |
| 音频(22) | 软 | 监听 `OnRentPaid`(金币哗啦)/`OnCashChanged`(据 `EChangeReason` 正负音色)/`OnBankruptcyDeclared`(破产音+BGM duck)播音(呈现侧纯叶子,各订各播;audio L217 已对齐) |
| 存档(21) | 硬 | 序列化每玩家 `Cash` |
| 交易(11)/拍卖(12)/命运之轮(13)/策略卡(15) | 软(Alpha) | 玩家间转移/中标扣款/奖惩现金经本系统结算 |
| 教程(24) | 软(Alpha) | 引导买地/收租示例经本系统 |

### 移动(4)关系(orchestrated,非严格上下游)
移动(4)在回合(2)编排下把 `(passed_go, SalaryAmount)` **推送**给本系统发薪;**Utility 租所需 `dice_total` 不由移动推送**——本系统在 `ResolvePhase` 从回合(2)暴露的当前程掷骰上下文 `Total` **PULL**(移动 CR-3.1:移动不缓存/不投递 `Total`)。本系统不反向读移动状态。

> **⚠ 双向一致性发现(Phase 5 须复核 systems-index,重要):**
> 1. **经济(5) ↔ 所有权(6) 互为数据流**:本系统算租需读 6 的归属事实(owner/is_monopoly/count/is_mortgaged;**`house_count` 归建房8、不在6**),而 6 买地/抵押需调本系统现金执行。systems-index 当前仅标 **6 depends-on 5**,未标反向。**为避免破坏 index"无环"声明,本 GDD 采用 push 模型:租金结算时由所有权(6)/ResolvePhase 把归属快照作为参数传入本系统的算租调用,本系统不直接硬引用 6 的接口**——如此本系统不新增对 6 的硬依赖,环被打破。Phase 5 须在 index 注明此 push 契约。
> 2. **经济(5) → 棋盘(1) 读依赖未在 index 标注**:本系统经 `GetTileData` 读金额 base,但 systems-index 当前 `5 depends-on 2`(仅 2)。Phase 5 应补标 `5 depends-on 1`(经移动转交 + 直接读 base)。
> 3. **net_liquidation_value 跨系统共用**:破产(9)的资产移交口径须 == 本系统 F-9(否则算账穿帮)。Phase 5 登记 systems-index 继承义务表(破产9 行)。

## Tuning Knobs

| 旋钮 | 作用 | 安全范围 | 过低/过高后果 |
|---|---|---|---|
| `STARTING_CASH` | 每玩家起始现金(**拖沓主杠杆**) | 750–2000(默认 **1500**) | 过低=早期买不起地、空转;过高=无人缺钱、无紧张、局变长 |
| `STARTING_CASH_FAST` | 快速局起始现金档 | 默认 **750** | 压缩局长用;过低则早期破产偶然性过大 |
| `go_salary_amount` | 过/停 GO 薪水(主 faucet) | 100–300(默认 **200**) | 过低=现金枯竭拖沓;过高=通胀、租金压力失效、局变长 |
| `tax_amount_income` | 所得税(sink) | 100–300(默认 **200** flat) | 过低=回收不足通胀;过高=早期惩罚过重 |
| `tax_amount_luxury` | 奢侈税(sink) | 50–200(默认 **100** flat) | 同上,量级较小 |
| `monopoly_rent_multiplier` | 垄断无房地租翻倍 | **MVP 锁定 2**(经典) | **跨系统旋钮**(牵动收租节奏与雪球速度);改动须协同所有权(6)/建房(8),故 MVP 锁定 |
| `unmortgage_fee` (num/den) | 赎回手续费率(sink) | 0/1–2/10(默认 **1/10**=10%) | 过低=抵押套利(见警戒线);过高=抵押不可用、翻盘手段失效 |
| `building_sellback_ratio` (num/den) | 卖房返还率 | 1/2–1/1(默认 **1/2**=半价) | 过低=卖房惩罚过重难翻身;1/1=建房无成本风险 |
| `mortgage_warning_threshold` | 抵押率**数据质量**警戒线(仅警告,棋盘加载校验用) | 0.5–0.7(默认 **0.6**) | 过高=放过越出经典规则的异常抵押率数据;过低=误报合法数据。**非套利旋钮**(见套利轴澄清) |
| `max_laps` / `max_rounds`(Open) | 圈数/回合上限(**拖沓硬杠杆**,Alpha) | 默认禁用(0=无限) | 启用后到限按 net worth 排名裁定胜者;归破产胜负(9)实现,本系统供 net worth 口径 |

**旋钮间相互作用:**
- **通胀轴**:`go_salary_amount`↑ 与 `tax_*`↓ 同向推高现金存量 → 租金压力失效、局变长(拖沓)。调参须成对看 faucet/sink 净值。
- **局长轴**:压缩拖沓优先级 `STARTING_CASH`(最干净)> `go_salary_amount` > `max_laps/rounds` 上限 > 税额 > 赎回费。
- **~~套利轴~~(R1 删除——套利不存在)**:一抵一赎净现金 = `MortgageValue − MortgageValue×(1+fee_rate) = −fee_rate×MortgageValue`,**对任意抵押率恒为负**(亏 fee_rate×MV)。抵押的真实代价是"已抵押地不收租"的机会成本,非套利。`mortgage_warning_threshold` 与 `unmortgage_fee` **无套利联动**:前者是 board 数据质量警戒线(抵押率越出经典规则区即警告),后者是赎回 sink 费率,两者独立(经济 AC-42 数学证伪旧"套利回报趋正"声称)。
- **跨系统锁定**:`monopoly_rent_multiplier` 改动牵动收租/雪球/建房 ROI,MVP 锁定;Alpha 若开放须三系统(5/6/8)协同。

> **数值来源**:所有默认值取经典大富翁公开数值体系(faithful-clone,见 Formulas 平衡值表);`STARTING_CASH_FAST`/`max_laps` 是本作为压缩"后期拖沓"通病(concept Design Risk)新增的工程旋钮,非经典。真实手感以原型期 playtest 调参为准(登记 Open Questions)。

## Visual/Audio Requirements

**n/a —— 本系统是数据/结算层,不渲染视觉、不触发音频。** 但金钱流动是本作**签名感官时刻**(concept Sensation priority-4:"收租金币飞溅的 juice"),由 VFX(19)/HUD(16)/音频(22) 拥有。本系统为其提供数据与事件,并钉最低呈现契约:

- **端到端 owner = VFX(19)/HUD(16)**:收租金币飞溅、发薪入账、抵押放款、破产清算的 juice 与音效。具体动画/数值/时长归 VFX19/audio22。
- **本系统义务边界**:提供权威结算 + 事件(`OnCashChanged`/`OnRentPaid`/`OnInsufficientFunds`/`OnBankruptcyDeclared`)+ 结果在动画开始前已产出(同步结算)。呈现层只回放既定结果,不影响金额逻辑。
- **好/坏语境(方向派生契约,#22/#19 reconcile,源 VFX19 CR-5 / audio CR-4)**:`OnCashChanged.EChangeReason` 携带**类型**语境但**不携带收/付方向位**——`Rent` 是单一无方向枚举值(一次收租广播 2 次 `OnCashChanged`:Payer 与 Payee 各一,AC-37,reason 均 `Rent`)。下游(VFX19/audio22)**一律靠 payload 自行派生方向,不依赖 reason 区分正负向**:① 现金数字色/音色正负 = **`OnCashChanged` 的 `NewCash−OldCash` delta 符号**(>0=正向金色/上扬,<0=负向红色/下沉,AC-33 已保证 delta 与实际变化等值);② 收租金币飞溅方向 = **`OnRentPaid` 的 `Payer→Payee` 视角**(Payer=肉痛、Payee=入账,AC-34 四字段权威)。`EChangeReason` 仅供下游做**类型分类**(Salary/Rent/Purchase/Mortgage/Unmortgage/Tax/Build/Trade/Bankruptcy 九值,映射 art-bible 状态),方向不入枚举——故无需 `RentReceived`/`RentPaid` 之类方向子值,亦不需额外接口。
- **回链**:登记 systems-index 继承义务表(VFX19/HUD16 行),类比 dice OQ-T-Dice-4 模式——"收租金币飞溅"priority-4 Sensation 须有 GDD 端到端负责(VFX19),避免"标记事实却无载体=不可测真空"。

> 📌 **Asset Spec**:art bible 批准后,可运行 `/asset-spec system:economy-cash` 产出金币/现金 UI 的视觉规格(若需要)。

## UI Requirements

**n/a —— 本系统无任何 UI。** 它不绘制屏幕元素,只暴露 `Credit`/`Debit`/`GetCash` 接口、纯函数只读 `GetUnmortgageCostForDisplay(MV)`(R-2 新增,供 #17 显示赎回价,非 UI)与 4 个事件。现金数值显示、应付额提示、买地/抵押/赎回按钮、筹资(Raising Funds)界面均由 **HUD(16)** 实现;赎回价显示由 **地产卡 UI(17)** 调上述纯函数,本系统供 `OnCashChanged` 事件 + `GetCash` 查询 + 赎回价口径单源。

> 📌 **UX Flag — 经济与现金**:HUD 现金面板、Raising Funds 筹资 UI(玩家抵押/卖房凑钱的交互)、买地/抵押决策 UI 须在 Pre-Production 由 `/ux-design` 为相关 HUD 屏出规格,再交 `/team-ui`。Stories 引用 `design/ux/[screen].md`,非本 GDD。

## Acceptance Criteria

> *由 `qa-lead` 校验(lean 模式 H 节派发)。**测试分层(对齐 dice F-5 / player-turn):** `[Logic]` 纯 fixture、headless、确定性、整数、PR merge gate;`[Advisory]` 集成/code-review/统计/playtest,非门控。*
> **核心原则(本项目反复失效模式,player-turn 8 轮代价 + dice R3 重申):平衡数值是否"对"永不作 [Logic] gate(salary=200、tax、ratio 是否平衡 = tuning,playtest 验)→ [Logic] 只断言"给定输入,公式算得对"(确定性)。经济无 RNG,公式是纯函数 `f(inputs)→output`,fixture 直接喂定值断言定值。**

### A. 核心规则不变式(CR-1/CR-8)
- **AC-1** [Logic] `Cash ≥ 0` 不变式:GIVEN Cash=50, WHEN Debit(80) THEN Cash 仍==50 ∧ `OnInsufficientFunds` 触发 ∧ 无 `OnCashChanged`(不直接落地为负、转入 Raising Funds)。
- **AC-2** [Logic] 原子转移守恒(CR-8):GIVEN payer.Cash=C_p / payee.Cash=C_e / rent=R(R≤C_p),WHEN 收租,THEN `payer.Cash==C_p−R` ∧ `payee.Cash==C_e+R` ∧ 总和不变(同一 R 局部变量用于 Debit 与 Credit,非两次重算)。
- **AC-3** [Logic] `Credit`/`Debit` 单调:`Credit(p,a)` 使 `Cash+=a`、`Debit(p,a)`(a≤Cash)使 `Cash−=a`,精确等值、不副作用他人 Cash。
- **AC-4** [Advisory·code-review] 受控写软约束(对齐 player-turn AC-35 / dice AC-20):全代码库无外部系统直写 `PlayerState.Cash`,只经 `Credit`/`Debit`。**强度坦诚:BP-primary 不可静态扫描→code-review;OQ-Econ-1 选 C++ 强封装可加禁用直写扫描升 [Logic]。**
- **AC-5** [Logic] `amount==0` 早返静默:GIVEN amount=0(passed_go=0 / 自有地租金=0 / MV=0),THEN 不调 Credit/Debit、不广播 `OnCashChanged`。

### B. 公式正确性 [Logic](每条 F-1..F-11 ≥1,含全部 guard/boundary)
> 每条断言"给定输入算得对",喂定值得定值。NOT 断言 200/2/10% 是否平衡。

**F-1 发薪**
- **AC-6** [Logic] `salary = passed_go × SalaryAmount`:passed_go=1,Salary=200 → 200;passed_go=2 → 400。
- **AC-7** [Logic] gate 在 passed_go(防双重发薪):passed_go≤0(0/−1)→ 不发(无 Credit、无事件);`SpecialAction=CollectSalary` 标记不触发第二次 Credit(停 GO passed_go=1 只发一次)。
- **AC-8** [Logic] 溢出 guard:passed_go=12 → 不 int32 溢出、结果正确;passed_go=13 → dev ensure(`AddExpectedError` 捕获)。

**F-2 地产租金(piecewise,×2 只作用无房 base)**
- **AC-9** [Logic] 短路抵押先于表查找:is_mortgaged=true(任意 house/monopoly)→ rent==0(不查 RentTable)。
- **AC-10** [Logic] 翻倍仅 house_count==0:monopoly ∧ house==0 ∧ RentTable[0]=2 → rent==4;monopoly ∧ house==1 → `RentTable[1]`(不翻倍)。
- **AC-11** [Logic·关键] 酒店绝不翻倍:monopoly ∧ house==5 → `RentTable[5]`(无 ×2)。
- **AC-12** [Logic] 非垄断按房数:is_monopoly=false ∧ house=3 → `RentTable[3]`。
- **AC-13** [Logic] house_count clamp:house=6 → `clamp(0,5)`→`RentTable[5]` 不崩 + ensure;house=−1 → `RentTable[0]` + ensure。

**F-3 车站租金**
- **AC-14** [Logic] `rent=RentTable[station_count−1]`:station=2 → `RentTable[1]`;station=4 → `RentTable[3]`。
- **AC-15** [Logic·关键 guard] station=0(所有权6 竞态)→ rent==0、不查 `[−1]`、不崩 + dev ensure(`AddExpectedError`)。

**F-4 公用租金**
- **AC-16** [Logic] `rent=dice_total × DiceMultiplierTable[utility_count−1]`:count=1,dice=7,mult=4 → 28。
- **AC-17** [Logic·关键 guard] utility_count=0 → rent==0、不查 `[−1]`、不崩 + ensure。
- **AC-18** [Logic] dice_total 是显式参数非缓存:两次传不同 dice_total(7/11,同 count)→ rent 各按传入算(28/44),证明不读缓存骰点("前进到最近公用"额外骰正确)。

**F-5 抵押放款**
- **AC-19** [Logic] `payout==MortgageValue`:MV=100 → Credit 额==100。
- **AC-20** [Advisory·code-review] 抵押前置不归 economy:`Credit` 实现内**无** `is_mortgaged`/`house_count` 读取或拒绝分支——未抵押前置由所有权(6) `Mortgage()` 校验、无房前置由决策点校验(economy 读不到二者)。(原 [Logic] 守门断言为残留:economy 无该数据无法门控,OQ-Prop-7 propagate 修正;mirror of property-ownership AC-14。)

**F-6 赎回(整数 ceil,杀 float)**
- **AC-21** [Logic·整数 ceil] `unmortgage_cost = MV + ceil(MV×fee_num/fee_den)`:MV=100,fee=1/10 → 110;MV=75 → fee=ceil(7.5)=8 → **83**(非 82.5/82);MV=1 → fee=1 → **2**(非免费赎回,fee 恒≥1)。
- **AC-22** [Logic] 赎回现金不足:Cash < unmortgage_cost → 赎回不可用、不 Debit、地保持抵押。

**F-7 缴税**
- **AC-23** [Logic] `debit==TaxAmount`(flat):TaxAmount=200 → Debit==200;Cash<200 → 进 CR-7 Raising Funds(同 AC-1 路径)。断言 flat 额被正确 Debit,NOT 断言 200 是否平衡。

**F-8 建筑卖回(整数 floor)**
- **AC-24** [Logic·整数 floor] `sellback=floor(BuildingCost×sell_num/sell_den)`:BuildingCost=100,ratio=1/2 → 50;BuildingCost=75 → floor(37.5)==37(非 38/37.5)。

**F-9 净可变现价值 🔴 最高风险契约**
- **AC-25** [Logic·核心不变式] `nlv = Σ MortgageValue(未抵押地) + Σ building_sellback(每栋,F-8)`:持 2 未抵押地(MV 100/60)+ 3 栋建筑(各 sellback 50)→ nlv==100+60+150==310。
- **AC-26** [Logic] 已抵押地贡献 0:持 1 已抵押(MV 100)+ 1 未抵押(MV 60)→ nlv 只计 60。
- **AC-27** [Logic·🔴 全档最高风险门 — R1 重写] 旧版"`nlv == F-11 转移现金等价`"是**循环同义反复**(用 F-9 口径估 F-11 in-kind 转入→永真、即便 F-11 漏转建筑也通过),已废。重写为**可证伪的三断言**:
  - **①(F-9 数值正确)** fixture player 持 PropA(MV=100,未抵押)/PropB(MV=60,已抵押,house=0)/2 栋建筑(BuildingCost=100 各)、Cash=30 → `nlv == 100 + 0(PropB已抵押) + 50 + 50 == 200`(已抵押地贡献 0、建筑各 floor(100×1/2)=50)。
  - **②(F-11 资产移交完整性,identity check 非估值)** 同 fixture 走收租破产 → creditor 新增**恰好** {PropA, PropB, 建筑×2} 逐对象身份核对(不遗漏/不重复/不凭空增)∧ `PropB.is_mortgaged 仍==true`(保留原抵押状态、未被清除)∧ `debtor.Cash` 转入 creditor 后归 0。**此条捕获"F-11 漏转/错改资产",旧 AC 无法捕获。**
  - **③(单一枚举入口,code-review/code-search 非运行时)** F-9 的资产枚举只有一段实现;F-10 的 is_insolvent 调用它、不另写第二段。
  > **设计澄清**:F-11 已抵押地 in-kind 转债权人是**经典忠实的刻意简化**(债权人继承赎回义务),**非财务等价**;本条验证枚举完整性 + 单一入口,不再声称"现金等价"那种不存在的算账安全保证。①② 为 [Logic],③ 为嵌入式 code-review 子项。**若只能保一条,是 ②(移交完整性)。**

**F-10 无力支付判据(严格 `<`)**
- **AC-28** [Logic·关键边界] `is_insolvent=(Cash + nlv < amount_due)` 严格 `<`(注:CR-7 散文"全部可变现价值" == F-9 `net_liquidation_value`,同一量):Cash=50,nlv=120,due=170 → `170<170==false`(付到 0 算能付、NOT 破产);due=171 → true。断言边界两侧。
- **AC-29** [Logic·code-review] is_insolvent 消费 F-9 同源(并入 AC-27③ 单一枚举入口核查):AC-28 的 nlv 由 F-9 计算、非独立重算;F-10 不另写第二段资产枚举。

**F-11 破产移交**
- **AC-30** [Logic] 收租破产 in-kind **现金侧**:debtor.Cash=30 + 2 地 + 1 房,收租破产 → `creditor.Cash+=30`(现金腿)∧ MVP 不收继承利息。**地产/建筑 owner in-kind 转移 + 保留抵押状态由破产9 经所有权6 执行并断言(所有权 AC-33),本系统不写 owner map、不在本 AC 断言归属转移**(范围分离,OQ-Prop-2)。
- **AC-31** [Logic] 银行破产**现金侧**:税致破产(债权人=银行)→ 建筑拆除**无现金返还** ∧ 现金蒸发 ∧ MVP 不拍卖。**建筑拆除由破产9 调建房8 `LiquidateAllBuildings` 执行、地产回退无主由所有权6 `ReturnTileToBank` 执行并断言(所有权 AC-34),本系统只断言现金侧、不写 owner map、不拆建筑**(范围分离,OQ-Prop-2)。

### C. 确定性 / 整数纯净(F-6/F-8/F-9 — 杀 float)
- **AC-32** [Logic·确定性] 全金钱运算整数纯净:F-6/F-8/F-9 取整路径两次冷启动 / 跨编译配置(Debug vs Shipping)位级相同(无 float→无 `/fp:fast` 截断分歧,对齐 dice F-4)。fixture:MV=75 赎回恒 83、BuildingCost=75 卖回恒 37,不因优化等级变。

### D. 事件广播(payload 正确性)
- **AC-33** [Logic] `OnCashChanged(Player,OldCash,NewCash,EChangeReason)`:Credit(p,200) reason=Salary → OldCash==旧 ∧ NewCash==旧+200 ∧ reason==Salary ∧ `NewCash−OldCash` 与实际变化等值。
- **AC-34** [Logic] `OnRentPaid(Payer,Payee,Amount,TileIndex)`:四字段 == 实际转移(Amount==rent、Payer/Payee/Tile 正确)。
- **AC-35** [Logic] `OnInsufficientFunds(Player,AmountDue,AmountShort)`:Cash=50,due=170 → AmountDue==170 ∧ AmountShort==120。
- **AC-36** [Logic] `OnBankruptcyDeclared(Debtor,Creditor)`:破产移交完成后广播一次(不多不少),时机在资产转移之后。
- **AC-37** [Logic] 事件次数精确:一次收租 → 恰 1 次 OnRentPaid + 2 次 OnCashChanged(payer/payee 各一,reason=Rent);amount==0 → 0 次(对齐 AC-5)。

### E. 接口稳定性(编译/反射)
- **AC-38** [Logic] 四事件均标 `UPROPERTY(BlueprintAssignable)`、`DYNAMIC_MULTICAST_DELEGATE`、payload `USTRUCT(BlueprintType)`、编译通过(对齐 dice AC-22)。
- **AC-39** [Logic] `Credit`/`Debit`/`GetCash` 均标 `UFUNCTION(BlueprintCallable)`、BP 可调、编译通过;`EChangeReason` 含全部既有项(Salary/Rent/Purchase/Mortgage/Unmortgage/Tax/Build/Trade/Bankruptcy)。

### F. 平衡值(Advisory — 永不 [Logic])
- **AC-40** [Advisory·playtest] 30 分钟局趋于"平衡偏通缩"、不通胀(faucet/sink 净值观测)。非 [Logic]:平衡手感。
- **AC-41** [Advisory·smoke] Config/Data smoke:STARTING_CASH=1500 / salary=200 / tax 200·100 / fee 1/10 / sellback 1/2 / multiplier 2 从数据驱动源正确加载、非硬编码。非 [Logic]:断言"值被加载",值本身对错是 tuning。
- **AC-42** [Logic·可数学验证,R1 由 Advisory 升级] 抵押无套利:对任意 `MortgageValue>0` 与 `fee_rate≥0`,一抵一赎净现金 == `−fee_rate×MortgageValue ≤ 0`(恒非正)。fixture:MV=100,fee=1/10 → 抵押 +100、赎回 −110、净 −10;MV=60 → 净 −6。**证伪旧"套利回报趋正"声称**。`mortgage_warning_threshold=0.6` 是 board 数据质量警戒线、非套利防范(与本断言无耦合)。

### G. 凑钱状态机 / 自动清算兜底(CR-7,R1 blocker #4)
- **AC-43** [Logic·状态机] 确定性自动清算兜底终止性 — 回合(2)驱动、economy 拥有顺序 spec + 现金判据:
  - **主 fixture(mortgage-empty-first,无建筑)**:GIVEN Cash=50、应付=200、玩家持 3 空地(MV 升序 60/80/100,house_count 全为0)不行动 → 回合2按 economy spec 依次调 6.Mortgage(MV最小可抵押地):MV=60→Cash=110<200;MV=80→Cash=190<200;MV=100→Cash=290≥200 → 执行 Debit(200) → Cash==90 → 回 Solvent。**断言:清算顺序确定(MV升序、两次冷启动位级一致) ∧ 够即停(第3地后停,不超额清算) ∧ economy 仅提供"Cash<amount_due?"判据,回合2驱动调 6/8。**
  - **变体A(走向破产)**:nlv 总和 < 应付 → 清算穷尽 → `is_insolvent==true` → 移交破产(9)(证伪兜底死锁)。
  - **变体B(mortgage-empty-first,混合资产)**:GIVEN Cash=50、应付=300、玩家持 PropX(空地,MV=80,house_count=0)+ PropY(带 2 房,MV=100,house_count=2,BuildingCost=100)。回合2 按 economy spec:PropX 可直接抵押(house_count==0)→ 先调 6.Mortgage(PropX,MV=80)→ Cash=130<300;PropX 已抵押、PropY 带房不可抵押 → 调 8.ForcedSellNextBuilding(PropY)→ sellback=50 → Cash=180<300;再调 8.ForcedSellNextBuilding(PropY)→ sellback=50 → Cash=230<300;PropY house_count=0 转可抵押 → 调 6.Mortgage(PropY,MV=100)→ Cash=330≥300 → 执行 Debit(300) → Cash==30 → 回 Solvent。**断言:空地优先抵押(止损,零半价损失);建筑在无空地可抵押后才由 8.ForcedSellNextBuilding 卖出;顺序由回合2驱动,economy 不直调 6/8。**
  - **变体C(奇数 BuildingCost per-building floor 验证)**:GIVEN PropZ,BuildingCost=75,house_count=3 → F-9 中 `Σ building_sellback = 3 × floor(75/2) = 3 × 37 = 111`(per-building floor) — **断言 ≠ 112**(`floor(75×3/2)=floor(112.5)=112`,合并再floor差1);F-8 单栋 sellback=37 亦同此结果。**验证逐栋取整与合并取整在奇数 BuildingCost 场景下必须用前者。**

> **[Logic] gate(BLOCKING PR)= 41 条:** AC-1/2/3/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/**27🔴**/28/29/30/31/32/33/34/35/36/37/38/39/**42**/**43**。**[Advisory]= 3 条:** AC-4(受控写,可升 [Logic] 待 OQ-Econ-1)/40/41。**总计 44 条。**(R1:AC-42 由 Advisory 升 Logic;新增 AC-43。)
> **F-1..F-11 [Logic] 覆盖:** F-1→6/7/8;F-2→9/10/11/12/13;F-3→14/15;F-4→16/17/18;F-5→19/20;F-6→21/22/**42(无套利)**;F-7→23;F-8→24;F-9→25/26/**27🔴**;F-10→28/29;F-11→30/31。**11/11 全覆盖,含全部 flagged guard。CR-7 凑钱状态机→43(R1 新增)。**

### 跨系统测试义务(OQ-T-Econ-*,回链下游 GDD,Phase 5 登记 systems-index 继承义务表)
- **OQ-T-Econ-1 → 所有权(6):** 6 须实现 `owner/is_monopoly/station_count/utility_count/is_mortgaged` 接口供算租(**`house_count` 归建房8,经回合2 `ResolvePhase` 聚合 6 快照 + 8 house_count 后传入,不在6 接口**),**且经 push 模型把归属快照作参数传入本系统算租调用**(Dep 双向一致性①,保 index 无环)。字段供给正确性 + push 快照时机归 6 的 AC(本档 AC-9~18 假设其正确并 guard count==0/house 越界)。
- **OQ-T-Econ-2 → 破产(9)🔴 ✅ RESOLVED 2026-06-04**(propagate,见 bankruptcy OQ-Bankrupt-3):破产判据(F-10 `is_insolvent`)与 net_worth 排名(= Cash + F-9)须 verbatim 使用本系统 F-9 `net_liquidation_value` **估值口径**(单一枚举入口,AC-27③);但**资产移交本身是 in-kind(资产对象逐项转移、非现金等价),9 的 AC 须用 identity-check(逐对象枚举完整性,对齐本档 AC-27②)断言,~~不得断言"移交总额 == F-9 值"~~**——旧措辞与本档 AC-27②(已废"总额==F-9"循环同义反复)自相矛盾:F-9 是估值口径、已抵押地记 0 但对象仍转出,bankruptcy AC-14 已据此落 identity-check。**另:局末排名用的 net worth 口径(可能含未抵押地 face value,≠ nlv 变现值)归 9 裁定**——已由 bankruptcy F-1(= Cash + F-9,nlv 变体)裁定,face-value 变体待 OQ-Bankrupt-2/OQ-Econ-5。
- **OQ-T-Econ-3 → 事件格(7):** 7 须把"前进到最近公用/车站"额外骰点作 dice_total 参数传入 F-4(AC-18 上游侧);卡牌金钱效果经本系统 Credit/Debit。
- **OQ-T-Econ-4 → 存档(21):** 21 序列化每玩家 `Cash`;Raising Funds 瞬态不应中途存档(States 节 OQ,与回合2协同)。
- **OQ-T-Econ-5 → 建房(8):** F-8 sellback 比率归本系统、建筑清单归 8 供 F-8/F-9 枚举;建/卖房现金经本系统执行。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-Econ-1 ⚠ ADR** | 受控写宿主形态:`Cash` 容器 + `Credit`/`Debit` 的封装强度。与 dice OQ-1 / player-turn OQ-1 **协调同一生命周期层 ADR**。选 C++ 强封装(Cash private + 访问器)→ AC-4 升 [Logic] 静态扫描禁直写;BP 约定 → 软约束 code-review。 | 下游 6/7/8/9 开工前 `/architecture-decision` |
| **OQ-Econ-2** | registry source 迁移与转 active:`monopoly_rent_multiplier`/`mortgage_warning_threshold` source 从 board-data 迁入本 GDD 并置 active;`go_salary_amount`/`tax_amount_income`/`tax_amount_luxury` 占位转 active(本 GDD 已推导真值)。 | Phase 5 执行(本会话) |
| **OQ-Econ-3** | 拖沓压缩 playtest:`STARTING_CASH`(1500/750)/`go_salary_amount`/`max_laps` 哪个组合最有效压缩 concept Design Risk"后期翻盘无望拖沓",原型期 playtest 定。 | playtest 后;MVP 默认经典值 |
| **OQ-Econ-4** | Raising Funds 瞬态存档:是否允许筹资未决时落存档点(本 GDD 建议不允许,避免序列化瞬态债务),与存档(21)/回合(2)协同。 | 存档(21) 设计时 |
| **OQ-Econ-5** | net worth 排名口径:局末/回合上限排名用的 net worth(是否含未抵押地 face value,≠ nlv 变现值)归破产(9)裁定,本 GDD 供 nlv(F-9),不拥有排名口径。 | 破产(9) 设计时 |
| **OQ-Econ-6** | 原作数值核查:board-data 已 flag 本作目标是 Rento Fortune(非经典大富翁),经济数值可能有差异。真值偏离须在**设计冻结前**事实查证(类比 dice OQ-5 调查项前置),否则"全经典值"前提存疑。 | MVP 设计冻结前 |
| **OQ-Econ-7(Alpha)** | 拍卖破产移交 / 所得税百分比变体:Alpha 拍卖(12)就位后,银行破产改走拍卖(F-11);所得税是否引入经典"200 或净资产 10%"二选一(引入 float+净资产依赖,MVP 不做)。 | Alpha(12)/House Rules(23) |
| **OQ-Econ-8** | Raising Funds 自动清算兜底的**触发时机**(AI 即时清算 vs 人类玩家超时时长)与 UI 提示(凑钱界面/自动清算可视化)。本系统已钉"清算顺序确定性 + 有界终止"契约(CR-7.3),只差触发策略与呈现。 | 回合(2)/AI(10) 设计时 + UX(/ux-design) |
| **OQ-Econ-9** | F-2 数据包络约束须经 `/propagate-design-change` 派发给**已 Approved 的棋盘(1)**,在其**加载期校验批次**新增以下三条 warning(本系统拥有约束定义、棋盘拥有校验执行):**① (既有)** `RentTable[1] ≥ RentTable[0]×monopoly_rent_multiplier`(防建首房降租,`MonopolyBaseExceedsOneHouse{TileIndex}`)。**② (新增)** `RentTable[5] > RentTable[4]`(strict,防酒店末档边际 ROI=0 的死投资——玩家花 BuildingCost 把4房升酒店却租金不涨,摧毁建酒店激励)。**③ (新增)** `RentTable[5] > RentTable[0]×monopoly_rent_multiplier`(防酒店租金低于垄断空地翻倍租金——逆激励:建到满级反不如清空,自定义棋盘需防此退化;经典棋盘满足)。②③ 仅 warning、不 fatal;经典 DA 均满足,自定义棋盘(地图编辑器26)须通过校验。 | 棋盘(1) re-touch(propagate)/ 经济实现前 |
| **OQ-Econ-10** | F-1 溢出安全的 SalaryAmount 上界:passed_go clamp 不护 SalaryAmount,须棋盘(1)/编辑器(26)加载期校验 `SalaryAmount ≤ 2,000,000`(经典 200 安全;防自定义棋盘溢出 int32)。**并轨(R3 propagate,移动 OQ-Move-6):F-4 Utility 租 `dice_total × DiceMultiplierTable[i]` 的 `DiceMultiplierTable[i]` 上界亦须棋盘(1)/编辑器(26)加载期校验防 `12×mult` 溢出——已落 board-data AC-23j(`[1, DICE_MULT_MAX]`),最终 `DICE_MULT_MAX` 数值与本 SalaryAmount 上界并轨裁定。** | 棋盘(1)/编辑器(26) propagate;经济实现前 |
