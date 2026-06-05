# 建房升级 (Building & Rent Scaling)

> **Status**: Approved-with-followups (R4 design-review 2026-06-04)
> **Author**: msc + design-system agents
> **Last Updated**: 2026-06-04（R4 design-review:**APPROVED-with-followups**;5 项 fast close-out 已应用[L131/L202 单一 owner 适用域 + AC-29 spy 约定 + AC-11a 调用序更正 + CR-8 重入注→OQ-Build-5 + Fantasy 均衡建房措辞];OQ-Build-1/2/6 producer propagate 债 + RECOMMENDED 下一 pass 见 review-log。R3:修 3 in-doc blocker[CR-9 旁路/AC-20 tautology/AC-3-8 GIVEN]）
> **Implements Pillar**: ③ 运气×策略(策略那一极的主战场)/ ① 桌游忠实(经典均衡建房)/ ② 社交博弈(抬租压制)

## Overview

建房升级系统是《骰子大亨》的**租金成长引擎**:玩家在集齐整组(垄断)的地产上逐栋建房、最终升为酒店,把"踩上来要付的租"一档档抬高,是 MVP 阶段把垄断从"翻倍 ×2"变成"碾压级现金流"的唯一手段。它有两个面:对内是一个**以 `TileIndex` 为键的运行时状态层**——持有每格 `house_count`(0..5,0=无房、5=酒店),并裁决"能不能在这格建/拆一栋"的全部规则(须垄断整组、整组未抵押、组内房数均衡差 ≤1、满 4 房才升酒店);对玩家则是**支柱③「策略那一极」的主战场**——"这把现金是留着保命,还是砸下去给橙组建满房、让对手下一脚就破产?"的投资抉择,以及看着自己地产从光秃秃变成插满小房子、租金数字翻着跟头往上跳的获得感(支柱②互坑、①桌游忠实的经典建房手感)。

它**不拥有**:现金(归经济5——建房扣钱、卖房返钱都由经济 `Debit`/`Credit` 执行)、地产归属与垄断/抵押判定(归所有权6——本系统只**读** 6 的 `is_monopoly`/`is_mortgaged` 作建房门控)、地价/造价/租金表等静态数值(归棋盘1)、租金公式本身(归经济5 F-2——本系统只**供** `house_count` 作其输入)、"买/建/拆"的决策点触发(归回合2 `ResolvePhase` / AI10)。它只拥有**"每格几栋房、能不能动这栋房"这组运行时事实与建造规则**。它做得好时,垄断不再是一个静态状态而是一条可投资的成长曲线——玩家全部注意力在"建房时机与现金流管理"的博弈上(支柱③);没有它,游戏就只剩"集齐整组租金 ×2"的天花板,垄断后再无成长、中后期迅速失去策略张力。

## Player Fantasy

**核心幻想:"把这条街变成杀手区"——精算一次投资,然后坐等对手送钱上门。**

你刚集齐橙色组三块地。现在面前是那个让大富翁玩家上瘾的决定:手里 600 块,是留着以防踩到对手的地,还是**把现金一栋栋砸下去、给这三块地轮着建到各两栋房**(均衡建房:每次只在最矮那块加一栋,六次点下来三块地各满两栋)?你选择了建。租金表从 `[14]` 跳到 `[200]`,你盯着棋盘上那几栋小绿房子,心里开始算对手的骰子——他离你的橙组还有 6 格,只要掷出 6、8、9 里任意一个……。下一回合,对手的棋子稳稳停在你建满房的黄金地段,屏幕弹出那个数字,他脸都绿了,现金哗啦一下流进你的账户。**这一刻的爽,不在于运气,而在于这是你三回合前就布好的局。**

这是支柱③「策略那一极」最纯粹的体现:骰子给的是运气,而**建房是你对抗运气、把垄断从"略微占优"变成"碾压"的主动杠杆**。它同时是支柱②社交互坑的引信——建满房的地段是牌桌上所有人都在绕着走、私下骂你的"地雷区"。情感内核是**"精算 + 收割"的双重满足**:先是现金流管理的算计紧张("建了房这把就没钱保命了,值不值?"),后是看对手付不起租、滑向破产的坏笑。

> **〔MVP 范围诚实标注〕** 对**领先者**,建房是滚雪球碾压的加速器,MVP 内完整可感。对**落后者**,建房是"再绕一圈集齐组就能翻身"的念想驱动(支柱③策略那一极由建房时机承载,对齐 economy MVP 范围声明)——**⚠ 此念想在 MVP 有一硬限(R3 game-designer 补诚实标注):若关键垄断组的某格已被对手持有,而 MVP 不含交易(11)/拍卖(12),落后者无机制取回该格,"集齐组"路径在此场景下字面无法兑现(等同经典桌游"棋盘瓜分后难翻身"的已知后期缺陷,支柱①有意保留)。原型期 playtest 须观察落后者是否因此提前认输/挂机;若观察到,加速 Alpha 翻盘注入器排期、不引入 rent cap(对齐 OQ-Build-6 裁定)。** 真正的"逆风绝处逢生"翻盘注入器(交易11/拍卖12/命运之轮13/策略卡15)全在 Alpha;本系统不假装独力提供翻盘戏剧性,只提供"投资成长"这一极。

## Detailed Design

### Core Rules

**CR-1 `house_count` 状态所有权(以 `TileIndex` 为键)。** 本系统拥有唯一一份运行时可变状态:每个 **Property 格**的 `house_count ∈ {0,1,2,3,4,5}`(0=无房,1–4=房,5=酒店)。**仅 Property 格可建房**——Railroad(用 `station_count`)/Utility(用骰点倍率)永不建房,本系统对其 `house_count` 恒为 0 且拒绝任何建造。开局全部为 0。经受控写接口改此状态(强度随 OQ-Prop-1 生命周期 ADR,缺省取最严)。**本系统不拥有**:现金(经济5)、归属/垄断/抵押判定(所有权6)、造价/租金表静态值(棋盘1)、决策点触发(回合2/AI10)。

**CR-2 「一栋 = 一个 `BuildingCost` 单位」统一计价模型。** `house_count` 的每一档(0→1→…→5)都是**一个建筑单位**,造价均为该格 `BuildingCost`(棋盘1供值,同组应一致)。酒店(第 5 档)经济上等同"第 5 栋":到达酒店玩家累计付了 `5 × BuildingCost`。此模型让 economy F-8/F-9 的"建筑清单"枚举 = 各格 `house_count` 之和,每单位卖回 `floor(BuildingCost/2)`,无需区分房/酒店。**(对齐 economy F-8 `building_sellback` 与 registry `building_sellback`。)**

**CR-3 建房前置(`CanBuildHouse(tile, player)` 全满足才合法)。**
1. `tile` 是 Property 且 `player` 拥有它(读 6 `GetOwner`);
2. `player` **垄断整组**(读 6 `is_monopoly`);
3. **整组无任何一格被抵押**(读 6 `is_mortgaged` 遍历 `GetTilesInGroup`;经典规则:组内有抵押则全组禁建);
4. **均衡建房**:`house_count[tile] == min(组内各格 house_count)` 且 `< 5`(只能在最矮格上加,保组内差 ≤1);
5. 酒店门自然涌现:`4→5` 要求 `house_count[tile]==min==4`,即**全组满 4** 才能升酒店(无需单独规则)。

**CR-4 建房事务 `BuildHouse(tile, player)`(原子,现金腿 8→5)。** 顺序:校验 CR-3 → 调经济 `Debit(player, BuildingCost)` → **Debit 成功**则 `house_count[tile] += 1` 并广播 `OnBuildingChanged(tile, newCount)`;**Debit 失败(现金不足)**则不改状态、返回 `BuildFailed_Insufficient`。**建房是自愿行为,买不起只是建不了,绝不触发 Raising Funds/破产。**(镜像 property-ownership `Buy` 的"校验→Debit→提交"原子序。)

**CR-5 卖房事务 `SellHouse(tile, player)`(自愿,现金腿 8→5)。** 前置:`player` 拥有 `tile` ∧ `house_count[tile] > 0` ∧ **均衡卖房** `house_count[tile] == max(组内各格 house_count)`(只能从最高格往下拆)。执行:`house_count[tile] -= 1` → 调经济 `Credit(player, floor(BuildingCost/2))` → 广播。卖房前置满足即必成(Credit 无失败路径)。

**CR-6 均衡不变式(本系统全程维持)。** 对任意色组,恒满足 `max(组内 house_count) − min(组内 house_count) ≤ 1`。"只在 min 格建、只从 max 格拆"两条规则保证此不变式永不被破坏,也使"全组满 4 才升酒店""卖房先从高档拆"自然成立。

**CR-7 抵押接缝(本系统只供读,不拥有门控)。** "带房不可抵押"的 `house_count[tile]==0` 前置由**决策点(回合2 `ResolvePhase`/AI10)在调 6 `Mortgage()` 前**读本系统 `GetHouseCount(tile)` 校验——本系统**只暴露 `GetHouseCount` 供读**,不拥有抵押门控(对齐 property CR-3 / economy F-4 已确立的"前置归决策点"裁定,6 读不到 house_count 防 6↔8 环)。

**CR-8 时机:仅自己回合。** 自愿建/拆(CR-4/CR-5)只在玩家自己回合的管理窗口(回合2 `ResolvePhase` 决策点)发生;强制卖房(筹资,见 CR-9)是自己回合 `ResolvePhase` 的阻塞子相。**不支持别人回合建/拆**(MVP 简化),杜绝跨回合并发写。**⚠ 重入注(R4 unreal-specialist):**「单线程串行」杜绝的是跨帧/跨回合并发,但**不**自动等于重入安全——`OnBuildingChanged` 是同步多播,监听者(VFX19/HUD16/地产卡17)回调内若同步调回本系统写接口(`BuildHouse`/`SellHouse`/`ForcedSellNextBuilding`)即同步嵌套重入,Blueprint 无语言级 critical section 强制 CR-8 约定。重入防御口径(监听者只读 observational / C++ `bIsExecuting` 重入 guard / coding-standards Forbidden Pattern,三选一)随 **OQ-Build-5** 受控写强度 ADR 裁定;AC-28 现仅 Advisory 覆盖,ADR 落地后升运行时 guard 断言。

**CR-9 强制清算接缝(承接 economy CR-7.3,⚠ 见接缝标注 1)。** 本系统供**确定性建筑清算迭代器** `ForcedSellNextBuilding(player) → (tile, amount) | INDEX_NONE`:每调一次卖掉**一个**建筑——选 `house_count` 最高的格(并列取最小 `TileIndex`),decrement 后调经济 `Credit`,返回 `(tile, amount)`;玩家无任何建筑时返回 `INDEX_NONE`。**⚠ 强制路径直接按 F-4 argmax 选格并 decrement,绝不经 `canSellHouse` 谓词门控(R3 systems-designer 闭合实现歧义):** `canSellHouse`(F-3)以**整组** `max_group` 判定,在 in-kind 拆分组中会令低档子集持有者 `==false`——若强制路径误经 `canSellHouse`,会对**仍持建筑**的玩家提前返回 `INDEX_NONE`、破坏有界终止保证。F-4 argmax 选的全盘最高档格**必为其组内 max**(见 F-4 归纳注),decrement 即等价"组内 max 格降档",故**无需且不调 `canSellHouse` 即维持 CR-6 均衡**。本系统**只拥有"卖哪一栋"的确定性 + 有界终止**(建筑有限),**不拥有清算总编排**(谁先卖房后抵押、抵押地排序归 economy 顺序 spec + 回合2 执行)。

> **⚠ 接缝标注 1(propagate 候选 → OQ-Build-1,触及 economy(5)/player-turn(2),均 Approved):** economy CR-7.3 现写"**本系统(经济)提供确定性自动清算:按 MortgageValue 升序遍历玩家资产(带房地先按 F-8 卖房…)**"。但 economy(5)是 6/8 的**上游**,若由 economy 直接调 8 `SellHouse`/6 `Mortgage` 执行清算 = **5→8 / 5→6 反向边 = 环**。正解:清算**执行驱动归回合2 `ResolvePhase`**(它合法 depends-on 5/6/8),economy 只拥有**顺序 spec + 现金判据**(`Cash≥应付?` / `is_insolvent`)。回合2 驱动环(**R4 OQ-Build-1 裁定=止损优先 mortgage-empty-first**):`while 现金不足 { 若有可直接抵押地(未抵押 ∧ (非Property ∨ house_count==0))→调 6.Mortgage(MV 最小可抵押地);否则若有建筑→调 8.ForcedSellNextBuilding(F-4 全盘最高档,卖空某组后该组 Property 转可抵押);否则→is_insolvent }`——**先抵押空地止损(抵押可赎回、零半价损失),仅当无空地可抵押时才卖房(F-4)**,现金够即早停;顺序不影响 is_insolvent(NLV 是 order-independent 和),仅影响早停后剩余资产与玩家损失额。**须 propagate:economy CR-7.3 措辞"本系统提供/遍历"澄清为"economy 拥有顺序 spec,执行归回合2 驱动",消 5→6/5→8 环歧义。**

> **⚠ 接缝标注 2(propagate 候选 → OQ-Build-2,触及 player-turn CR-3.2 + economy F-9):** economy F-9 净可变现价值 `Σ building_sellback(b)` 需**逐格 house_count × BuildingCost**。economy 不能调 8(5→8 环),故建筑枚举须**经回合2 聚合传入**——与租金 F-2 的 house_count 聚合**同一接缝**(player-turn CR-3.2 已确权)。须把 CR-3.2 聚合契约**扩展覆盖 F-9/破产判据路径**:回合2 经 8 `GetPlayerBuildings(player)→[(tile,house_count)]` 聚合后传 economy 算 NLV。

### States and Transitions

每个 Property 格的 `house_count` 是一台 6 态(0..5)有限机:

| 当前态 | 转移 | 触发 | 守卫 |
|---|---|---|---|
| h (0..4) | **Build** → h+1 | `BuildHouse`(自愿) | CR-3 全前置 + Debit 成功 |
| h (1..5) | **Sell** → h−1 | `SellHouse`(自愿)/ `ForcedSellNextBuilding`(强制) | CR-5 均衡卖 +(强制路径无现金守卫) |
| h (0..5) | **OwnerChange(in-kind)** → h 不变 | 破产9 / 交易11(Alpha)归属移交 | house_count 以 TileIndex 为键、随格走(见下) |

- **归属移交(破产 in-kind,⚠ 接缝标注 3 → OQ-Build-3):** `house_count` 以 `TileIndex` 为键、与 owner 解耦,故归属转移时**建筑自动随格转移(本系统无需动作)**——对齐 economy 现有"建筑 in-kind 转移"措辞。**容许的冻结异常**:接收方若因此持"非垄断却带房"的格,**禁止再建/再拆该组**(CR-3 前置②失败),直至其补齐整组+解抵押;租金仍按 economy F-2 `else` 分支 `RentTable[house_count]` 计(公式对非垄断带房自洽)。**⚠ 此 in-kind 选择 vs 经典"破产前须卖房还银行"存在桌游忠实张力**,最终口径待破产(9)设计 + Rento 行为核查(镜像 OQ-Prop-5 模式);MVP 取 in-kind provisional,provisional 备选接口 `LiquidateAllBuildings(debtor)` 留破产(9)裁。**该接口语义已承诺(随 OQ-Build-3 仍 provisional):** `LiquidateAllBuildings(debtor)` 是**银行破产专用**接口——遍历 debtor 全部 `house_count>0` 格逐格清零(`house_count=0`),**不调用经济5 `Credit`**(无偿清算,守 economy F-11 银行破产建筑拆除无现金返还)——与 `ForcedSellNextBuilding`(自愿/抵押腿清算,调 `Credit` 返半价 `floor(BuildingCost/2)`)**语义不同**;由破产9 在**银行破产分支**调用(收租破产建筑 in-kind 随格、不调本接口)。**⚠ provisional 边界(OQ-Build-3):** 本承诺只钉死「无 Credit / 无偿清算」语义(为 bankruptcy AC-19「Credit==0」提供 building 侧对端支撑),**不预判** OQ-Build-3 终局口径;若 Rento 行为核查裁定为经典「卖房还银行」(建筑拆除有现金 sink),须改走带 Credit-to-bank-sink 的**另一接口**、届时 re-propagate。

### Interactions with Other Systems

| 系统 | 数据流 / 接口 | 方向 | 谁拥有 |
|---|---|---|---|
| 棋盘(1) | 读 `BuildingCost`/`ColorGroup`/`GetTilesInGroup(group)→升序成员` 作造价与均衡判定 | 8→1 | 1 供静态数据;8 拥有建造规则 |
| 经济(5) | 建房 `Debit(player, BuildingCost)`、卖房 `Credit(player, floor(BuildingCost/2))`(**8→5**);**8 不被 economy 反调**(无环) | 8→5 | 5 执行现金;8 拥有建造/卖房规则与 house_count |
| 所有权(6) | **读** `GetOwner`/`is_monopoly`/`is_mortgaged`(建房门控,**8→6**);**8 不被 6 反调,6 不读 8 的 house_count**(防 6↔8 环) | 8→6 | 6 拥有归属;8 读作门控 |
| 回合(2) | `ResolvePhase` 决策点调 `BuildHouse`/`SellHouse`;聚合 `GetHouseCount` 入算租(CR-3.2)+ 聚合 `GetPlayerBuildings` 入 F-9(OQ-Build-2);驱动强制清算调 `ForcedSellNextBuilding`(OQ-Build-1) | 2→8 | 2 拥有流程/决策点/清算驱动;8 供操作与读 |
| 破产(9, Not Started) | 收租破产建筑 in-kind 随格(无需动作);银行破产调 provisional `LiquidateAllBuildings(debtor)` 逐格清零、**不调 `Credit`**(无偿清算,守 economy F-11) | 9→8 | 9 拥有破产裁决;8 供建筑态+清算接口(OQ-Build-3) |
| AI(10, Not Started) | 读 `GetHouseCount`/`CanBuildHouse` 估值建房决策 | 10→8 | 10 拥有决策;8 供读 |
| 事件格(7) | RepairFee 牌读 `GetTotalHouseCount`/`GetTotalHotelCount`(全盘房/酒店分开计,F-7) | 7→8 | 7 拥有牌结算;8 供聚合 |
| VFX(19)/HUD(16)/地产卡(17) | `OnBuildingChanged(tile, newCount)` 多播驱动建房 juice / 房屋图标 | 8→下游 | 8 供事件;下游拥有呈现 |

**公开稳定接口承诺(给下游 2/5/7/9/10/16/17/19):** `GetHouseCount(tile)`/`CanBuildHouse(tile,player)`/`BuildHouse(tile,player)`/`CanSellHouse(tile,player)`/`SellHouse(tile,player)`/`ForcedSellNextBuilding(player)`/`GetPlayerBuildings(player)`/`GetTotalHouseCount(player)`/`GetTotalHotelCount(player)`/`OnBuildingChanged` 签名稳定;事件 payload 只增不改语义。**返回类型契约(R3 补):** `BuildHouse`/`SellHouse` 返回成功/具名错误码(`Build*Rejected_*`/`Sell*Rejected_*`/`BuildFailed_Insufficient`,见 Edge Cases);`ForcedSellNextBuilding` 返回 `(tile, amount)` 或 `INDEX_NONE`(**非错误码**——强制路径无"拒绝"语义,只有"有/无下一栋"),两套返回契约不互换、不共享底层错误码枚举。

## Formulas

> 建房本地编号 F-1..F-6。经济侧公式(租金 F-2、卖回 F-8、净可变现 F-9、垄断×2)归经济(5),本系统**只引用不重定义**,引用处标注为 `economy F-x`。

### F-1 组内 min/max 档位

`min_group(tile) = min { house_count[t] | t ∈ G(tile) }`;`max_group(tile) = max { house_count[t] | t ∈ G(tile) }`
其中 `G(tile) = GetTilesInGroup(GetColorGroup(tile))`(棋盘1供,升序成员)。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `G(tile)` | Set\<int32\> | \|G\|≥1 | 同色组全部 TileIndex(棋盘1供;加载期保非空) |
| `house_count[t]` | int32 | 0..5 | 各格当前档位(本系统拥有) |
| `min_group`/`max_group` | int32 | 0..5 | 组内最低/最高档 |

**Output Range:** [0,5]。**Example:** 红组 `[2,2,3]` → min=2,max=3。

**运行时契约(`|G|` 退化守卫,R1 新增):** F-1 仅以**已验非空**的 `G(tile)` 调用——`|G|≥1` 由棋盘1加载期保证(见上表)。若 `|G|==0`(棋盘配置错误漏过加载期校验),`min{}`/`max{}` 对空集未定义,本系统**dev `ensure(|G|>0)` + log 暴露上游 bug,并对该格 `canBuildHouse→false` 兜底拒绝**(同 `BuildingCost≤0` 的"按棋盘1校验为准 + 不自崩"处置,见 Edge Cases),**不静默 UB**。**求值序契约(R2 新增,补 systems-designer B3 — 此前只说 `canBuildHouse→false` 兜底,未定义 min/max 在空集上的返回值,留 UB 缺口):`min_group`/`max_group` 实现须先 `ensure(|G|>0)`,失败即返回哨兵 `INDEX_NONE(-1)`,绝不对空集执行 `min{}`/`max{}`(防 C++ `min_element` 空 range 解引用 UB);F-2/F-3 谓词须在消费 min/max 之前先短路——`|G|==0 → canBuildHouse/canSellHouse == false`,使 `house_count[tile]==min_group(tile)` 这类比较永不读到哨兵或空集求值结果。** `|G|==1`(单格色组,自定义棋盘容许):min==max==该格档位,均衡约束退化为恒真、酒店门退化为"该格满4即升",语义仍自洽。

### F-2 建房合法谓词 `canBuildHouse(tile, player)`

`canBuildHouse = (TileType(tile)==Property) ∧ (owner[tile]==player) ∧ is_monopoly(player, GetColorGroup(tile)) ∧ (∀ t∈G(tile): is_mortgaged[t]==false) ∧ (house_count[tile]==min_group(tile)) ∧ (house_count[tile]<5)`

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `owner[tile]` | int32 | `INDEX_NONE`/0..P-1 | 归属(所有权6拥有,8 读) |
| `is_monopoly`/`is_mortgaged[t]` | bool | — | 垄断/抵押(所有权6拥有,8 读) |
| `house_count[tile]` | int32 | 0..5 | 目标格档位(8拥有) |

**Output Range:** bool。**Example:** 全组未抵押、`[2,2,3]`:建 2-格(==min)→true;建 3-格(≠min)→false。

### F-3 卖房合法谓词 `canSellHouse(tile, player)`

`canSellHouse = (TileType(tile)==Property) ∧ (owner[tile]==player) ∧ (house_count[tile]>0) ∧ (house_count[tile]==max_group(tile))`

**Output Range:** bool。**Example:** `[2,2,3]`:卖 3-格(==max)→true;卖 2-格(≠max)→false。

### F-4 强制清算选择函数(CR-9,非玩家主动路径)

`target = argmin_TileIndex { t ∈ Tiles(player) | house_count[t]==M }`,其中
`M = max { house_count[t] | t ∈ Tiles(player) }`,`Tiles(player) = { t | owner[t]==player ∧ house_count[t]>0 }`。
`Tiles(player)` 为空 → 返回 `INDEX_NONE`(无建筑可卖,终止)。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `Tiles(player)` | Set\<int32\> | 子集 | 该玩家持有且带房的格 |
| `M` | int32 | 1..5 | 该玩家全盘建筑最高档 |
| `target` | int32 | TileIndex / `INDEX_NONE` | 本次卖房目标(最高档中最小 TileIndex) |

**有界终止:** 每步总建筑数严格 −1,有限步后空集终止。**强制路径选全盘最高档(可跨组)**,非某组 max;全盘 max 必为其组内 max(**归纳依据,R3 补:** 由 CR-6 不变式,decrement 前每组 `max_group−min_group≤1`,故含全盘最高档 M 的组其 `min_group≥M−1`;decrement 该 M 格→M−1 后,新组差 `≤ M−(M−1)=1`,仍 ≤1),不破坏 CR-6 均衡。**⚠ 适用域(R4 systems-designer):此归纳仅在「单一 owner 持有整组」配置成立**——其前提「decrement 前每组 `max_group−min_group≤1`」即 CR-6 不变式,而 CR-6 仅在单一 owner 整组域有良定义(见 L181-182)。破产 in-kind 拆分组(OQ-Build-3,MVP provisional)中,持低档子集的 owner 反复强制卖房可使其格降到远低于另一 owner 的高档格(如 A 持格 5→4→3 而 C 持同组格恒 5 → 组 `[3,5]` 差 2),此时全盘 max(A 侧)**非**组内 max(C 侧),组差可 >1。拆分组的强制清算均衡为 **provisional,随 OQ-Build-3 破产路径裁定**,不在本归纳的良定义域内。**Example:** 持格 `[3,8,22]`、count `[3,3,2]` → M=3,选 TileIndex=3。

### F-5 造价/返还引用(非重定义)

- 建房扣款:`build_cost(tile) = BuildingCost[tile]`(棋盘1供值,8 只读;每档均一个 `BuildingCost` 单位,CR-2)。
- 卖房返还:`sellback(tile) = economy F-8 building_sellback(BuildingCost[tile]) = floor(BuildingCost[tile] × 1/2)`——**比率 1/2 归经济5(`building_sellback_ratio`),8 只传入正确 `BuildingCost[tile]`,经济执行 `Credit`**。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `BuildingCost[tile]` | int32 | >0 | 每档造价(棋盘1供) |
| `sellback(tile)` | int32 | ≥0(floor) | 每拆一档返还(经济5 F-8 计) |

**Output Range:** `sellback ≥0`。**Example:** `BuildingCost=75` → `sellback=floor(37.5)=37`。**无套利:** 建−卖净亏 `BuildingCost − floor(BuildingCost/2) ≥ BuildingCost/2`,恒为正,任意轮次单向亏损。

### F-6 每玩家建筑枚举(供 economy F-9 NLV,经回合2聚合传入)

本系统经 `GetPlayerBuildings(player) → [(tile, house_count)]` 暴露建筑枚举;其总可变现 `building_total_sellback(player) = Σ_{t ∈ PlayerPropertyTiles(player)} house_count[t] × floor(BuildingCost[t]/2)`,其中 `PlayerPropertyTiles(player) = { t | owner[t]==player ∧ TileType(t)==Property }`。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `house_count[t]` | int32 | 0..5 | 各格档位(8供) |
| `BuildingCost[t]` | int32 | >0 | 各格造价(棋盘1供) |
| `building_total_sellback` | int32 | ≥0(经典全盘上界 ~10⁴ ≪ 2³¹) | 该玩家全部建筑总返还 |

**职责边界:** **8 供建筑枚举**(`house_count` 真值 + Property 过滤);**economy 拥有 F-9 公式**(消费枚举、自执行 floor 乘加)。**economy 不反调 8**——枚举与 F-2 的 `house_count` 同经回合2 `ResolvePhase` 聚合传入(CR-3.2 扩展,防 5→8 环;见 OQ-Build-2)。**Example:** 格6(h=3,BC=100)→150 + 格9(h=1,BC=120)→60 = **210**。

### F-7 全盘建筑聚合(供事件格7 RepairFee,与 per-tile 区分)

事件格(7) RepairFee 修缮费牌需**房/酒店分开计数**的全盘聚合(经典 `per_house_fee`/`per_hotel_fee`),与 per-tile `house_count[0,5]` 口径不同(本系统把酒店折为档 5)。本系统经两个具名接口暴露:

- `GetTotalHouseCount(player) = Σ_{t∈PlayerPropertyTiles(player)} (house_count[t]==5 ? 0 : house_count[t])` —— 全盘**普通房**数(仅档 1–4,酒店不计)。
- `GetTotalHotelCount(player) = count_{t∈PlayerPropertyTiles(player)}(house_count[t]==5)` —— 全盘**酒店**数(仅档 5)。

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `GetTotalHouseCount` | int32 | [0,~32] 经典 | 全盘普通房(档 1–4;酒店折档 5,不计入此) |
| `GetTotalHotelCount` | int32 | [0,~8] 经典 | 全盘酒店(档 5) |

**消歧(关闭 tile-events OQ-Event-3):** per-tile `GetHouseCount(tile)→[0,5]`(酒店折档 5)与本聚合**口径不同**——RepairFee 须调本聚合接口(房/酒店**分开**计),**不可误用 per-tile 值、不可把档 5 当 4 房**。**Example:** 玩家持 3房地(h=3)+ 酒店地(h=5)+ 4房地(h=4):`GetTotalHouseCount = 3+0+4 = 7`、`GetTotalHotelCount = 1` → RepairFee(25/100)`7×25+1×100 = 275`。

### ROI 引用注(本系统不拥有数值真值)

建房回报结构 `房间ROI = (RentTable[h+1] − RentTable[h]) / BuildingCost`——**RentTable/BuildingCost 真值归棋盘1/经济5,本系统只文档化公式结构、不填数值**(对齐 board-data Tuning「RentTable 与 BuildingCost 联动,整组评估」)。满酒店(h=5)用 `economy F-2 RentTable[5]`,**酒店不享垄断×2**(×2 只作用 h=0 无房 base);满酒店总造价 5×BuildingCost,回报合理性归经济5 真值域。数据包络 `RentTable[1] ≥ RentTable[0] × monopoly_rent_multiplier`(防建首房降租;**R3 改:引用参数名而非硬值 2,虽 MVP 锁 2**)归经济定义、棋盘加载期校验(economy OQ-Econ-9)。**酒店末档 ROI 防退化(R3 economy-designer 补):`RentTable[5] > RentTable[4]`(严格 `>`,非 `≥`)须并入 economy OQ-Econ-9 同批 propagate 作棋盘加载期校验——board 现仅保证单调不减(允许相等),`RentTable[5]==RentTable[4]` 会令 5×BuildingCost 的酒店末栋边际 ROI=0(死投资);经典盘严格递增免疫,自定义棋盘(编辑器26)须校验。**

### CR-6 均衡不变式归纳证明(systems-designer)

**命题:** 任意色组**在单一 owner 持有期间**全程 `max_group − min_group ≤ 1`(不变式按 **(组 ∩ owner)** 评估;in-kind 拆分见下〔归属移交步〕)。
**基础:** 开局全 0,差=0 ≤1。
**建房步:** F-2 要求只在 `house_count==min` 格 +1 → 该格变 min+1;新差 ∈{0,1} ≤1。
**卖房步:** F-3 要求只从 `house_count==max` 格 −1 → 该格变 max−1;新差 ∈{0,1} ≤1。
**归属移交步(in-kind,第三类转移,States 表行3):** `house_count` 以 TileIndex 为键、随格走,owner 改而档位不变。破产 in-kind 可使一色组跨两 owner(如组 [3,3,2] 中 [3,3] 转 B、[2] 转 C),此时该组**不再是单一 owner 的完整组**,按 CR-3 前置②(缺垄断)进入**冻结域**——`canBuildHouse` 天然拒绝;`canSellHouse` 仅容该 owner 持有子集内 max 格降档(L196),本系统**不对跨 owner 的"整组 max−min"作断言**(该量在拆分组上无良定义)。owner 补齐整组后该组回到单一 owner 域,基础/建房/卖房步重新适用。**故 in-kind 转移不破坏本不变式**——它不改 house_count,只把组移出单一 owner 评估域。
> **R2 精确化(补 systems-designer B1/B2,消"虚假完备"与未声明副作用):** 本不变式**仅在"单一 owner 持有整组"配置上有良定义**;拆分组不是"不变式在子集上恒真"(单格子集 `max−min` 恒 0 是平凡真、非有意义保证),而是**显式置于不变式定义域之外**——证明不靠重界定子集制造完备感,而是声明拆分组不在范围内。**未声明副作用显式登记:** `canSellHouse` 以整组 `max_group`(F-3,全组评估)判定,故拆分组中持**低档**子集的 owner 可能因另一 owner 的**高档**格而 `canSellHouse==false`、无法降档自己的格(如组 [3,3,2]、B 持 [3,3]、C 持 [2]:`max_group=3` 由 B 持有 → C 卖 [2] 被拒)。此为 **provisional 冻结语义**,最终随 OQ-Build-3 破产路径裁定;MVP 内此场景仅由破产 in-kind 产生(houses 仅在垄断整组下存在,破产时整组转单一债权人 → 通常不拆分),Alpha 交易(11)带房移交才常态化拆分。
**∴** "只在 min 建 / 只从 max 卖" 充分且完备保证不变式在**单一 owner 域内**永不被破坏,in-kind 拆分组以冻结域处置而非违反不变式;**酒店门(全组满4才升5)由 `house_count==min_group ∧ <5` 自然涌现,无需额外规则**。

## Edge Cases

- **若在 Railroad/Utility 格建房**:拒绝,返回 `BuildRejected_NotProperty`。这两类格 `house_count` 恒 0,租金走 station_count/骰点倍率,永不接受建造。
- **若建房目标格非本人拥有 / 无主**:拒绝 `BuildRejected_NotOwner`(F-2 `owner[tile]==player` 失败)。
- **若未集齐整组(非垄断)建房**:拒绝 `BuildRejected_NoMonopoly`(读 6 `is_monopoly==false`)。
- **若组内任一格被抵押时建房**:拒绝 `BuildRejected_GroupMortgaged`(经典规则:全组任一抵押则禁建整组)。
- **若在非最低档格建房(会破坏均衡)**:拒绝 `BuildRejected_Uneven`(`house_count[tile] ≠ min_group`)。
- **若已是酒店(house_count==5)再建**:拒绝 `BuildRejected_AtMax`(F-2 `<5` 失败)。
- **若建房时现金 < BuildingCost**:`Debit` 失败,`house_count` 不变,返回 `BuildFailed_Insufficient`。**建房自愿,绝不进 Raising Funds/破产**(与付租/缴税强制扣款的破产路径不同)。
- **若卖房时 house_count==0**:拒绝 `SellRejected_Empty`(无房可卖;Railroad/Utility 恒落此分支)。
- **若在非最高档格卖房(会破坏均衡)**:拒绝 `SellRejected_Uneven`(`house_count[tile] ≠ max_group`)。
- **若 `ForcedSellNextBuilding` 时玩家无任何建筑**:返回 `INDEX_NONE`,不报错——清算驱动方(回合2)据此转入抵押腿(CR-9 / OQ-Build-1)。
- **若卖酒店(5→4)**:MVP 无限供给下纯 decrement(无需"拆酒店换回 4 房"的库存操作)。*(对照:经典有限供给下破酒店需银行有 4 房可换,否则须降级卖出——此复杂度随短缺机制归 Alpha。)*
- **若 `BuildingCost ≤ 0`(棋盘配置错误)**:本系统不自校,按棋盘1加载期校验为准(board Tuning `BuildingCost > 0`);若 0 漏过则建房免费,属棋盘1 bug,非本系统职责。
- **若决策点未先校验 `house_count==0` 就对带房地调 6 `Mortgage()`**:本系统不拦(抵押门控归决策点,CR-7);6 照置抵押标记并 dev `ensure`+log 暴露上游 bug(property AC-14)。本系统只保证 `GetHouseCount` 返回真值供决策点读。
- **若破产 in-kind 移交后接收方持"非垄断却带房"的格**:`house_count` 随格不变;该组**冻结**——`canBuildHouse`(缺垄断)与 `canSellHouse`(若该格非组内 max)按谓词自然拒绝,直至接收方补齐整组+解抵押。租金仍按 `economy F-2 else` 分支 `RentTable[house_count]` 计(公式对非垄断带房自洽)。最终口径待破产9 + Rento 核查(OQ-Build-3)。
- **若同一帧两次建/拆同格**:不可能——CR-8 限自己回合管理窗口、单线程串行,无并发写。
- **强制清算的均衡安全(单一 owner 域)**:`ForcedSellNextBuilding` 每次减全盘最高档格,**在单一 owner 持有整组时**该格在其组内必为 max,decrement 后组差仍 ≤1(CR-6 归纳证明涵盖),反复调用不破坏该域内任何组的均衡。**⚠ 拆分组例外(R4,见 F-4 适用域注 / L181-182):** 破产 in-kind(OQ-Build-3 provisional)拆分组中,持低档子集的 owner 强制清算可使组差 >1;此场景均衡为 provisional、随 OQ-Build-3 裁定,不在本「不破坏均衡」断言的良定义域内。

## Dependencies

**上游(本系统 depends-on,均 Approved):**

| 系统 | 硬/软 | 接口 / 数据 | 方向 |
|---|---|---|---|
| 棋盘数据(1) | 硬 | `BuildingCost`/`ColorGroup`/`GetTilesInGroup(group)→升序成员`(造价 + 均衡判定) | 8→1 |
| 经济与现金(5) | 硬 | 建房 `Debit(player,BuildingCost)`、卖房 `Credit(player,floor(BuildingCost/2))`;F-8 比率、F-9 公式归 5 | 8→5 |
| 地产所有权(6) | 硬 | 读 `GetOwner`/`is_monopoly`/`is_mortgaged`(建房门控) | 8→6 |

**下游(depend-on 本系统):**

| 系统 | 硬/软 | 对本系统的依赖 | 方向 |
|---|---|---|---|
| 玩家回合(2) | 硬 | `ResolvePhase` 调 `BuildHouse`/`SellHouse`;聚合 `GetHouseCount`(算租 CR-3.2)+ `GetPlayerBuildings`(F-9);驱动 `ForcedSellNextBuilding`(清算) | 2→8 |
| 破产胜负(9, Not Started) | 硬 | 收租破产建筑 in-kind 随格;银行破产调 provisional `LiquidateAllBuildings(debtor)`(逐格清零、**不调 `Credit`**=无偿清算,守 economy F-11) | 9→8 |
| 事件格(7, Approved§) | 软(RepairFee 牌) | RepairFee 读 `GetTotalHouseCount`/`GetTotalHotelCount`(全盘房/酒店分开,F-7;关闭 tile-events OQ-Event-3) | 7→8 |
| AI 对手(10, Not Started) | 硬 | 读 `GetHouseCount`/`CanBuildHouse` 估值建房决策 | 10→8 |
| HUD(16)/地产卡 UI(17) | 软 | `GetHouseCount` + `OnBuildingChanged` 呈现房屋图标/档位 | 16,17→8 |
| 游戏反馈 VFX(19) | 软 | `OnBuildingChanged` 驱动建房 juice | 19→8 |

**双向一致性核查(已核实):** systems-index `8 depends-on 1,5,6` ✓;board:163 / economy:105 / property:108 均已反向列出建房(8)为 dependent ✓;下游 2(CR-3.2 引 house_count)/ economy F-2:152(house_count 归8供)/ index(AI10、VFX19 depends-on 含 8)✓。**新增 7→8(RepairFee 读 F-7 聚合,本轮 consistency-check 揪出 tile-events:155 重载)** → tile-events 侧 OQ-Event-3 本轮已 RESOLVED 并回链;**systems-index 系统 7 depends-on 须补 8(soft,本轮一并改)**。

**无环纪律(核心):** 本系统对 5/6 **只单向调/读**(8→5 现金腿、8→6 门控读),**绝不被 5 或 6 反调**;6 不读本系统 `house_count`(防 6↔8 环);economy 算租/算 NLV 所需 `house_count`/建筑枚举经**回合2 聚合传入**(防 5→8 环)。

**跨系统义务(Phase 5 登记 / propagate 候选):**
- **OQ-Build-1**(→ economy(5) + player-turn(2)):economy CR-7.3 清算执行措辞澄清为"economy 拥有顺序 spec + 现金判据,执行驱动归回合2"。**R3 加宽范围(economy-designer fresh-grep):同批须改 economy line 105("建筑清单供 F-8/F-9"→"经回合2聚合传入")+ AC-43 变体B("先 F-8 卖 2 房"→"经回合2驱动 `ForcedSellNextBuilding`"),三处皆暗示 economy 直接知 house_count = 5→8 反向边;并消歧 CR-7.3"按 MortgageValue 升序遍历资产"(economy 拥"哪块地先处理"顺序 spec)与 building F-4"全盘最高档贪心"(每块地内"卖哪栋"执行)两层策略优先级。**
- **OQ-Build-2**(→ player-turn CR-3.2 + economy F-9/F-10):CR-3.2 聚合契约扩展覆盖 F-9 建筑枚举(`GetPlayerBuildings` 经回合2 传入)。**R3 补(economy-designer):economy F-9 `Σ building_sellback(b)`("b=每栋")与 building F-6/`GetPlayerBuildings` 返回 `(tile, house_count)` 口径不对齐——F-9 须补等价展开注 `Σ_t house_count[t]×floor(BuildingCost[t]/2)`(现仅 AC-25 数值 3×50=150 隐含),防 propagate 实现把 house_count 误当数组长度;并兑现 F-10 `is_insolvent` 签名加 `preaggregated_nlv` 形参(否则 F-10 内部算 NLV 必调 8 = 5→8 环)。**
- **OQ-Build-3**(→ 破产(9) + Rento 核查):破产建筑 in-kind vs 经典"卖房还银行",最终口径待 9 设计裁定。

## Tuning Knobs

**本系统拥有的旋钮:**

| 旋钮 | 值 / 范围 | 影响 | 过高/过低 |
|---|---|---|---|
| `MAX_BUILDING_LEVEL` | **5**(0–4房+酒店),**结构锁定 = RentTable 长度 6** | house_count 上限、酒店档位 | 改它须同步改棋盘 RentTable 长度(否则 F-2 越界);非自由旋钮,锁 5 |
| `HOUSE_SUPPLY_MODE` | **Unlimited(MVP)** / Limited(Alpha) | 是否有 32房/12酒店有限供给 | MVP=Unlimited;Limited 须新增库存状态 + 屯房竞争逻辑(Alpha) |
| `EVEN_BUILD_RULE` | **ON**(均衡建/卖,差 ≤1) | CR-6 不变式 | MVP 锁 ON(桌游忠实);Alpha House Rules(23)可作可选关闭项 |

**引用(真值不归本系统,指向 source of truth):**

| 旋钮 | Source | 本系统关系 |
|---|---|---|
| `BuildingCost`(每格) | **棋盘1**(值)/ 经济5(平衡) | 8 读以扣款;ROI 联动见下 |
| `building_sellback_ratio` = 1/2 | **经济5**(F-8,registry `building_sellback_ratio`) | 8 卖房时传 BuildingCost,经济计返还 |
| `RentTable[0..5]` | **棋盘1**(存储)/ 经济5(平衡真值) | 8 供 house_count 作 F-2 输入,不填 RentTable 值 |
| `monopoly_rent_multiplier` = 2 | **经济5** | 8 不涉翻倍逻辑 |

**旋钮交互:** `BuildingCost ↔ RentTable` 联动决定建房 ROI(`(RentTable[h+1]−RentTable[h]) / BuildingCost`),**经济5 调参须整组评估、不可单格调**(对齐 board-data Tuning + economy ROI 真值域);数据包络 `RentTable[1] ≥ RentTable[0]×2` 必守(经济定义、棋盘加载期校验)。`HOUSE_SUPPLY_MODE` 与策略深度交互:Unlimited 移除"屯房卡对手"互坑杠杆(支柱②)——见 Open Questions 诚实标注。

## Visual/Audio Requirements

- **建房事件**(`OnBuildingChanged` 升档驱动 VFX19):房屋"啵"地弹上地块 + 金币花出反馈 + 建造音效(锤击/施工 + 扣款声),体现 concept Visual Identity §2「获得感视觉化 juice」。**酒店升级**(4房→酒店)用更夸张的变换 juice(4 小房合成 1 酒店)。
- **卖房事件**(降档):房屋移除 + 半额返现反馈 + 较轻音效(区别于建房的"获得"基调)。
- **棋盘呈现**:地块上按 `house_count` 渲染 1–4 栋小房 + 酒店模型(资产归 art bible §6 `SM_env_board_*` / 建筑资产);**斜俯视一眼可读**(Visual Identity §1)——房屋数在棋盘视角须清晰可数。
- **本系统职责边界**:只供 `OnBuildingChanged(tile, newCount)` 事件 + `GetHouseCount` 读;实际 VFX/模型/音效由 VFX(19)/art bible/audio 拥有。建房 juice 的体验契约是 **enable-not-own**(本系统使能,不拥有寄生 VFX 体验)。

## UI Requirements

- **建/卖控件**:在自己回合管理窗口(回合2 `ResolvePhase`)的地产管理 UI / 地产卡 UI(17)提供「建房」「卖房」按钮(鼠标点击为主,**禁 hover-only**,对齐 technical-preferences)。
- **地产卡显示**:当前 `house_count`(档位)、下一栋造价(`BuildingCost`)、卖回额(`floor(BuildingCost/2)`)、以及**门控原因**——`canBuildHouse` 为 false 时按钮禁用 + tooltip 给因(非垄断 / 组内有抵押 / 非最低档需先补齐 / 现金不足)。
- **建后租金预览(兑现"精算"幻想,R1 新增)**:建房按钮旁须显示该格**建后租金**(`economy F-2 RentTable[house_count+1]`,垄断场景取对应值)——即"建完→租金 X→Y"的可读反馈。Player Fantasy 的"精算"半("心里开始算对手的骰子……建完租 200、对手现金 300、踩一脚就破产")**依赖此信息可见**,缺则玩家只能脑算 RentTable,对休闲受众(支柱④)是隐性摩擦。**本系统供 `house_count`/`CanBuildHouse`;RentTable 真值归经济5 F-2,由 UI 经回合2 聚合读取**(不在本系统重算)。
- **均衡引导**:UI 应提示哪些格当前可建(只有 `house_count==min_group` 的格),避免玩家困惑为何某格不能建。**并列 min(全组同档)时(R3 补):** 组内全部格均 `==min_group`、均可建,UI 须高亮**全部**可建格(非仅一格)、tooltip 示"可建任意格",防休闲玩家(支柱④)误认只能建某一格。
- **房屋数呈现**:地块上 + 地产卡上一致显示 house_count。
- **本系统职责边界**:供数据(`GetHouseCount`/造价/卖回额/`CanBuildHouse`+原因)与事件;呈现/布局/输入路由归 HUD(16)/地产卡 UI(17)/UMG。

> **📌 UX Flag — 建房升级(8)**:本系统有 UI 需求(建/卖控件、门控原因、房屋数显示)。Pre-Production 阶段须对地产管理/地产卡相关屏运行 `/ux-design` 产出 UX spec,再写 epics;引用 UI 的 story 应引 `design/ux/[screen].md` 而非本 GDD。

> **📌 Asset Spec**:Visual/Audio 需求已定义。art bible 批准后,运行 `/asset-spec system:building-upgrade` 产出房屋/酒店模型、建房 VFX 的逐资产规格与生成提示。

## Acceptance Criteria

> **fixture 约定:** 注入 `MockEconomy`(`Debit(player,amount)→bool` / `Credit(player,amount)`)、`MockOwnership`(`GetOwner`/`IsMonopoly`/`IsMortgaged`/`GetTilesInGroup`)、`MockBoard`(`BuildingCost`/`GetTilesInGroup`/`TileType`)。**spy 约定(R4 qa-lead,使 AC-29/AC-30 出边集断言可操作):** `MockEconomy`/`MockOwnership` 除上述接口外须各维护 `OutboundCallLog: [(MethodName, Args...)]`,每次被调入栈一条;测试以**精确序列/集合**断言真实 8 的出站调用(如 AC-29:`MockEconomy.OutboundCallLog == [("Credit", player0, amount)]` ∧ `MockOwnership.OutboundCallLog` 无 `Mortgage`/写调用),区别于仅"调用次数"计数——出边"集合性"(不回调 6、不被 economy 重入)由此可证伪;无 spy log 则 Integration·BLOCKING 标签名不副实。集成 AC 归 `tests/integration/building-upgrade/`,单测归 `tests/unit/building-upgrade/`。共 **35 条**(R1:AC-11 拆 11a/11b;R3:新增 AC-33 空组守卫):**30 [Logic](gating)+ 2 [Advisory·code-review](AC-11b/28)+ 2 [Integration·BLOCKING](AC-29 即时可独证子断言 / AC-30 deferred-gate,见各条 R2/R3 订正)+ 1 [Logic·provisional·quarantined](AC-31,待 OQ-Build-3 激活、不计 gating)**;AC-27 回链义务另作〔Obligation·OQ-Build-1〕登记,不另占编号。**(R3:AC-20 由纯数学 tautology 重写为可证伪实现行为断言、维持 [Logic] gating;+AC-33 空组守卫 → gating 29→30。)**

- **AC-1** [Logic] CR-1 初始状态:GIVEN 系统初始化,WHEN 查询全部格 `GetHouseCount`,THEN 每格 == 0。
- **AC-2** [Logic] CR-1 Railroad 拒建:GIVEN tile=Railroad、owner=player0、IsMonopoly=true,WHEN `BuildHouse(tile,player0)`,THEN 返回 `BuildRejected_NotProperty`、house_count 仍 0、`Debit` 未调用。
- **AC-3** [Logic] CR-1 Utility 拒建:GIVEN tile=Utility、owner=player0、house_count=0、IsMonopoly=true、IsMortgaged=false(mock 前置完整,值对结果无影响——TileType 门最先短路,补全防别序实现命中错误分支),WHEN `BuildHouse(tile,player0)`,THEN `BuildRejected_NotProperty`、house_count 仍 0、`Debit` 未调用。
- **AC-4** [Logic] CR-3 非 owner 拒建:GIVEN Property、house_count=0、`GetOwner==player1`、IsMonopoly=false、IsMortgaged=false,WHEN `BuildHouse(tile,player0)`,THEN `BuildRejected_NotOwner`、不变、`Debit` 未调用。
- **AC-5** [Logic] CR-3 非垄断拒建:GIVEN tile=Property、owner=player0、house_count=0、`IsMonopoly==false`、IsMortgaged=false(house_count=0 确保不先命中 Uneven 分支),WHEN `BuildHouse(tile,player0)`,THEN `BuildRejected_NoMonopoly`、不变、`Debit` 未调用。
- **AC-6** [Logic] CR-3 组内抵押禁建:GIVEN 组 [A,B,C]、owner=player0、IsMonopoly=true、`IsMortgaged(B)==true`、`house_count(A)==0`(最低),WHEN `BuildHouse(A,player0)`,THEN `BuildRejected_GroupMortgaged`、全组不变、`Debit` 未调用。
- **AC-7** [Logic] F-2 非最低档拒建(均衡):GIVEN 组 [A,B]、house_count=[2,3]、owner(A/B)=player0、IsMonopoly=true、IsMortgaged(A/B)=false(补全防先命中 NotOwner/NoMonopoly/GroupMortgaged 错误分支),WHEN `BuildHouse(B,player0)`(B≠min),THEN `BuildRejected_Uneven`、house_count(B) 仍 3、`Debit` 未调用。
- **AC-8** [Logic] F-2 满酒店拒建:GIVEN house_count(A)=5、组全 5、owner=player0、IsMonopoly=true、IsMortgaged=false(补全防先命中 NoMonopoly/GroupMortgaged 分支,确保命中 AtMax),WHEN `BuildHouse(A,player0)`,THEN `BuildRejected_AtMax`、仍 5、`Debit` 未调用。
- **AC-9** [Logic] CR-4 正常建房:GIVEN house_count=[1,2](A==min)、合法归属、`Debit→true`,WHEN `BuildHouse(A,player0)`,THEN 成功、house_count(A)==2、`Debit(player0,BuildingCost[A])` 恰 1 次、`OnBuildingChanged(A,2)` 恰 1 次广播。
- **AC-10** [Logic] CR-4 原子回滚:GIVEN house_count(A)=0、合法归属、`Debit→false`(现金不足),WHEN `BuildHouse(A,player0)`,THEN `BuildFailed_Insufficient`、house_count(A) 仍 0、`OnBuildingChanged` 未广播。
- **AC-11a** [Logic] CR-4 调用序可观察代理(Blueprint 可测,**R2 强化:补一维区别于 AC-10**):GIVEN house_count(A)=0、合法前置、`Debit→false`,WHEN `BuildHouse`,THEN `Debit(player0,BuildingCost[A])` **被调用恰 1 次**(校验通过→写前已尝试扣款)∧ 返回 `BuildFailed_Insufficient` ∧ house_count(A) 仍 0 ∧ `OnBuildingChanged` 未广播。**(R4 更正 — 删除虚假蕴含:此可观察代理只证明"Debit 被尝试恰 1 次 ∧ 最终 state 干净",**不**蕴含调用序——"先写 `house_count+=1` → Debit 失败 → 回滚"亦满足本断言,故本 AC **不**能证明"Debit 在 `+=1` 之前"。本 AC 相对 AC-10 的增量价值是多断言"确实尝试过扣款"这一维度,非复制;真实内部调用序断言**唯一**归 AC-11b code-review。)**
- **AC-11b** [Advisory·code-review] CR-4 调用序内部顺序:代码审查确认 `BuildHouse` 实现中 `Debit()` 调用位于 `house_count += 1` **之前**(校验→Debit→提交原子序);C++ 侧若可经 `OnDebitCalled` 回调注入则升为自动断言。证据存 `production/qa/evidence/`。**(项目以 Blueprint 为主,调用序无法自动断言时由本 code-review 兜底——故拆出可观察态 AC-11a 作 gating Logic,不让整条降级。)**
- **AC-12** [Logic] CR-6 建后均衡:GIVEN 组 [A,B,C]=[2,2,2]、合法、`Debit→true`,WHEN 各选 min 格 `BuildHouse` 三次,THEN 每次后 `max−min ≤ 1`。
- **AC-13** [Logic] CR-6 拦非 min 不破不变式:GIVEN 组 [A,B]=[1,2],WHEN `BuildHouse(B)`(高档),THEN `BuildRejected_Uneven` ∧ `max−min` 仍 1。
- **AC-14** [Logic] CR-6 卖后均衡:GIVEN 组 [A,B,C]=[3,3,2]、**`GetOwner(A/B/C)==player0`(MockOwnership 注入,补 owner 前置防命中 `SellRejected_NotOwner` 错误分支)**,WHEN player0 从 max(3) 格 `SellHouse`,THEN 卖后 `max−min ≤ 1`。
- **AC-15** [Logic] 酒店门:GIVEN 组 [4,4,3]、合法,WHEN `BuildHouse(4-格)`(≠min=3),THEN `BuildRejected_Uneven`(非 AtMax)、仍 4。
- **AC-16** [Logic] 酒店门:GIVEN 组 [4,4,4]、合法、`Debit→true`,WHEN `BuildHouse(C)`(==min==4<5),THEN 成功、house_count(C)==5、`OnBuildingChanged(C,5)` 广播。
- **AC-17** [Logic] CR-5 空格拒卖:GIVEN house_count(A)=0、owner=player0,WHEN `SellHouse(A,player0)`,THEN `SellRejected_Empty`、仍 0、`Credit` 未调用。
- **AC-18** [Logic] F-3 非 max 拒卖(均衡):GIVEN 组 [A,B]=[2,3]、owner=player0,WHEN `SellHouse(A)`(2≠max=3),THEN `SellRejected_Uneven`、仍 2、`Credit` 未调用。
- **AC-19** [Logic] F-3 正常卖房:GIVEN 组 [A,B]=[2,3]、owner=player0,WHEN `SellHouse(B)`,THEN 成功、house_count(B)==2、`Credit(player0,floor(BuildingCost[B]/2))` 恰 1 次、`OnBuildingChanged(B,2)` 恰 1 次。
- **AC-20** [Logic] F-5 卖回额 floor 截断 + 无套利(**R3 改:由纯数学 tautology 重写为可证伪的实现行为断言**):GIVEN house_count(A)=1、owner=player0、组内 A 为 max、`BuildingCost[A]=75`、`Credit→ok`,WHEN `SellHouse(A,player0)`,THEN house_count(A)==0 ∧ `Credit(player0, 37)` 恰 1 次(`floor(75/2)=37`——**验证实现执行整数截断、传入 floor 值而非 75 原值、亦非四舍五入 38**)∧ 单向亏损可校 `build_cost 75 > sellback 37`(无套利)。**(原 AC-20 验算 `C−floor(C/2)≥1` 为永真数学定理、不调系统接口、不可证伪;同 R1 杀 AC-11 自降级——gating 须可失败。qa-lead R3。)**
- **AC-21** [Logic] F-5 卖酒店:GIVEN house_count(A)=5、组全 5(A 为 max)、owner=player0,WHEN `SellHouse(A)`,THEN house_count(A)==4、`Credit(player0,floor(BuildingCost[A]/2))` 恰 1 次、无库存检查。
- **AC-22** [Logic] F-4 强制清算选择:GIVEN player0 持 [t3,t8,t22]、house_count=[3,3,2]、`Credit→ok`,WHEN `ForcedSellNextBuilding(player0)`,THEN 返回 t3(max=3 中最小 TileIndex)、house_count(t3)==2、`Credit(player0,floor(BuildingCost[t3]/2))` 被调用。
- **AC-23** [Logic] F-4 空集:GIVEN player0 全格 house_count=0,WHEN `ForcedSellNextBuilding(player0)`,THEN 返回 `INDEX_NONE`、`Credit` 未调用、状态不变。
- **AC-24** [Logic] F-4 有界终止:GIVEN player0 持 [A,B]=[2,1](总 3 单位)、`Credit→ok`,WHEN 循环 `ForcedSellNextBuilding` 至 `INDEX_NONE`,THEN 成功 3 次、第 4 次 `INDEX_NONE`、循环终止。
- **AC-25** [Logic] F-4 强制不破均衡(单组):GIVEN 组 [A,B,C]=[3,3,2]、player0 全持、全盘仅此组有建筑,WHEN `ForcedSellNextBuilding`(选 A),THEN house_count(A)==2、`max−min ≤ 1`。
- **AC-25b** [Logic] F-4 强制不破均衡(跨组):GIVEN player0 持组 X 全 3 档 + 组 Y 全 2 档,WHEN `ForcedSellNextBuilding`,THEN 首选组 X 内某格(全盘 max=3、TileIndex 最小)、卖后组 X `max−min ≤ 1` 且组 Y 不动。
- **AC-26** [Logic] F-6 枚举正确性:GIVEN tileA(Property,player0,3)、tileB(Property,player0,0)、tileC(Property,player1,2)、tileD(Railroad,player0,0),WHEN `GetPlayerBuildings(player0)`,THEN 返回 `[(tileA,3)]`(B house=0、C 他人、D 非 Property 均不在列)。
- **AC-27** [Logic] CR-7 只读接缝(本系统侧可测断言):GIVEN 带房格 house_count(A)=k>0,WHEN 决策点调本系统 `GetHouseCount(A)`,THEN 返回真值 k。本系统**不实现** `Mortgage` 的 `house_count==0` 前置(归决策点),只供真值供读。
- **〔Obligation·OQ-Build-1,不占 AC 计数〕** 回链义务:player-turn 须新增 AC「调 6 `Mortgage` 前先读 8 `GetHouseCount==0`」回链本接缝。**✅ RESOLVED 2026-06-04(`/propagate-design-change`,fresh-grep 核实 player-turn L143/L549):player-turn 已补 AC-50**(spy 断言对 `house_count>0` 的 tile 恒不调 `6.Mortgage`,house_count==0 才调)+ CR-3.3 驱动环兑现本接缝。〔原:已 grep 核实下游 player-turn 现无此 AC、义务真空,须 producer 兑现——委托下游+登记≠闭合纪律〕。**硬门转移至实现侧(qa-lead):player-turn AC-50 对应实现落地前,建房(8)涉抵押接缝的 story 不得标 Done——本系统作为 Integration 依赖方,义务未兑现即视 #8 抵押接缝未闭合,防 producer 不跟进时义务静默丢失。**
- **AC-28** [Advisory·code-review] CR-8 无并发:`BuildHouse`/`SellHouse` 无跨帧异步路径;限自己回合 `ResolvePhase`,无跨回合并发写入口。
- **AC-29** [Integration·BLOCKING] CR-9/OQ-Build-1(**R2 重设计:认领本系统可独证子断言**):GIVEN player0 现金不足付租且持建筑,WHEN 集成 fixture(**真实 BuildingSystem** 实例 + MockEconomy/MockOwnership)执行回合2 清算环 `while 现金不足 { 有建筑→8.ForcedSellNextBuilding;否则→6.Mortgage }`,THEN ①(**语境核查,非可证伪 gating 核心 — R3 qa-lead 降级:** 单线程 fixture 中测试驱动码本就是唯一调用路径,"回合2 是唯一调用方"在任何 fixture 结构下永真,故仅作 caller 记录、不作 gating 失败判据)回合2 是 `ForcedSellNextBuilding` 唯一调用方;② **(可证伪 gating 核心)真实 8 在清算中对经济只发 `Credit`(8→5)一种出边**——spy 记录真实 8 的出站调用集 `== {economy.Credit}`,不回调 6、不被 economy 重入。**⚠ 范围诚实(R2):"真实 economy 不直调 8"(5→8 反向边证伪)非本 Mock fixture 可证——MockEconomy 系手写、本就不会自发调 8,该断言在 Mock 上永真无意义;此侧归 economy 真实实现的集成套件 + OQ-Build-1 propagate 闭合后补"真实-5 × 真实-8"集成断言。** 集成测试归 `tests/integration/building-upgrade/`。*(grep 与 Mock 均非反向边终证;本 AC 只认领"真实 8 出边受限 + 回合2 唯一驱动"这一可独证子断言。)*
- **AC-30** [Integration·BLOCKING·deferred-gate] OQ-Build-2(**R2 订正:消"code-review 占位"漏洞**):GIVEN economy F-9 NLV 计算场景,WHEN 集成 fixture 执行破产判据路径,THEN 建筑枚举由回合2 `ResolvePhase` 先聚合 `GetPlayerBuildings` 再传入经济——**真实 8 侧 spy 断言 `GetPlayerBuildings` 的调用方==回合2、零次来自 economy**。**门控语义(R2,消占位漏洞):本 AC 是集成门,gate 的是 F-9/破产判据集成 story、不 gate 本系统单测 PR;在真实 economy + 回合2 聚合(OQ-Build-2 / economy F-10 签名)落地前本 AC 标 `deferred`——不计入本系统单测 sprint Done 的 BLOCKING gate,亦不得以 code-review 充作其"已过";待 propagate 落地后激活为运行时 spy 断言。激活责任人(R3 补,消执行层泄漏):producer 在 OQ-Build-2 propagate 闭合后须通知 qa-lead 将本 AC 由 `deferred`→`active` 并运行集成测试,`/qa-plan` 须把本 AC 写入对应 sprint 的 explicit checklist 并命名检查人。门控边界明确化:本 AC gate 的"F-9/抵押接缝集成 story"定义为——任何调用 `is_insolvent`/NLV 聚合/破产判据路径的 story;纯建房/卖房单测 story 不在此门内(防被笼统标"不涉接缝"绕过)。** 集成测试归 `tests/integration/building-upgrade/`。**(不接受 code-review 占位充当 BLOCKING 通过——同 R1 对 AC-11 自降级的处置。)**
- **AC-31** [Logic·provisional·quarantined(待 OQ-Build-3)] in-kind 冻结:GIVEN tileA 由 player1 in-kind 转 player0、house_count(A)=2、转后 `IsMonopoly(player0,组)==false`,WHEN `BuildHouse(A,player0)`,THEN `BuildRejected_NoMonopoly`(垄断前置天然封闭建造方向)。**⚠ 隔离、不计入 gating Logic:** 本场景能否出现取决于破产9 路径——若 9 采经典"卖房还银行",破产前 house_count 已清零、场景永不出现 → 断言永真不证伪(同义反复)。**故隔离,不作 sprint Done 证据,直至破产9 批准 in-kind 路径后激活。**
- **AC-32** [Logic] F-7 全盘聚合(供 tile-events RepairFee):GIVEN player0 持 tileA(Property,h=3)+tileB(Property,h=5 酒店)+tileC(Property,h=4)(不同色组),WHEN `GetTotalHouseCount(player0)` / `GetTotalHotelCount(player0)`,THEN `GetTotalHouseCount==3+0+4==7`(酒店地 h=5 计 0 房)∧ `GetTotalHotelCount==1`(仅 tileB);验证全盘房/酒店分开计、不混用 per-tile [0,5] 口径(关闭 tile-events OQ-Event-3)。
- **AC-33** [Logic](R3 新增,补 F-1 `|G|==0` 空组守卫覆盖)空组兜底:GIVEN `GetTilesInGroup(GetColorGroup(tile))` 返回空集(棋盘配置错误漏过加载期校验)、tile=Property、owner=player0,WHEN `CanBuildHouse(tile,player0)` / `CanSellHouse(tile,player0)`,THEN 均返回 `false`(F-2/F-3 在消费 `min_group`/`max_group` 前短路 `|G|==0`)∧ **不 crash**(`min_group`/`max_group` 返回哨兵 `INDEX_NONE`,绝不对空集执行 `min{}`/`max{}`)∧ `Debit`/`Credit` 未调用。

## Open Questions

| OQ | 问题 | Owner | 目标解决时机 |
|---|---|---|---|
| **OQ-Build-1** | ✅ **RESOLVED 2026-06-04**(`/propagate-design-change`,fresh-grep 双侧核实):economy CR-7.3 改「economy 拥顺序 spec+现金判据、执行归回合2、不直调6/8」+ F-10 签名 + AC-43 变体B/C + Dep 行;player-turn 补 **CR-3.3 清算驱动环 + AC-50(Mortgage-pre-read)+ AC-51(顺序+终止)**;两层顺序裁定=**止损优先 mortgage-empty-first**(用户裁定)。〔原债〕economy CR-7.3 清算执行措辞("本系统提供/遍历玩家资产")须 propagate 澄清为"economy 拥有顺序 spec + 现金判据,执行驱动归回合2 `ResolvePhase`",消 5→6/5→8 反向边歧义。**含**:player-turn 新增 AC「调 6 `Mortgage` 前先读 8 `GetHouseCount==0`」回链本档 AC-27。触及 economy(5)+player-turn(2),均 Approved。**(R3 加宽:并改 economy line 105 + AC-43 变体B + CR-7.3 两层策略消歧,详见 Dependencies §跨系统义务 OQ-Build-1。)** | producer(`/propagate-design-change`) | 建房实现期前 / 破产(9)设计时 |
| **OQ-Build-2** | ✅ **RESOLVED 2026-06-04**(`/propagate-design-change`,fresh-grep 核实):economy F-10 加 `preaggregated_nlv` 形参(`is_insolvent(player,amount_due,preaggregated_nlv)`,不内部算 NLV 防 5→8)+ F-9 展开式 `Σ_t house_count[t]×floor(BuildingCost[t]/2)`(含奇数 BC fixture AC-43 变体C);player-turn 补 **CR-3.4 全组合 NLV 聚合 + AC-52**(zero economy→8)。〔原债〕economy F-9 NLV 的建筑枚举须经回合2 聚合传入(`GetPlayerBuildings`),与租金 F-2 的 house_count 聚合同一接缝——player-turn CR-3.2 聚合契约须**扩展覆盖 F-9/破产判据路径**。**⚠ R2 sharpen(economy-designer):economy F-10 `is_insolvent(player, amount_due)`(economy-cash.md line 229)当前签名无建筑枚举/预聚合 NLV 形参——"经回合2聚合传入"要求一次具名签名变更(如 `is_insolvent(player, amount_due, preaggregated_nlv)`,NLV 由回合2 聚合后算妥传入),否则 F-10 内部算 NLV 必调 8 = 5→8 环。propagate 须点名此 F-10 签名缺口,非仅 CR-3.2 扩展。** 触及 player-turn CR-3.2 + economy F-9/F-10,均 Approved。 | producer(`/propagate-design-change`) | 建房实现期前 / 破产(9)设计时 |
| **OQ-Build-3** | 破产时建筑 **in-kind 随格转移**(MVP provisional,本档采)vs 经典 **"破产前卖房还银行"** 存在桌游忠实张力。最终口径 + Rento 行为核查(接收方持"非垄断带房"冻结态是否符 Rento)。镜像 OQ-Prop-5 模式。 | 破产(9)设计 + Rento 核查 | 破产(9)GDD 设计时 / 设计冻结前 |
| **OQ-Build-4** | MVP **无限房屋供给**移除了经典 32房/12酒店短缺机制——连带移除"屯房卡对手"互坑杠杆(支柱②)与短缺反通胀阀。**诚实标注**:建房速度/租金爆发节奏须原型期 playtest 校验;Alpha 若加回短缺须新增库存状态 + 竞争逻辑。**可证伪 playtest 判据(R3 补):失败 = 4人局中所有玩家"想建就建、零摩擦感",现金流风险不足以制造决策张力(→ "主战场"宣言空洞,议加回有限供给或别的时间压力);成功 = 现金流风险(建房=无保命钱)本身产生可观察犹豫(暂不建/等一圈)。观察指标:领先者从集齐首组到满房的平均回合数 + 是否有玩家"等着建"而非"被迫选择"。** | game-design / playtest | MVP 原型期 playtest |
| **OQ-Build-5(共享 ADR)** | `house_count` 容器形态(`TArray<int32>` per-tile)与受控写强度,与所有权6 owner map、player-turn 等同属 **OQ-Prop-1 owner map 生命周期 ADR**;本档缺省取最严受控写,实现期随该 ADR 收敛。 | 架构期(OQ-Prop-1 ADR) | 下游实现(8/9)开工前 |
| **OQ-Build-6(支柱裁定登记)** | 无限滚雪球的领先者碾压是桌游忠实(支柱①)的**有意结果**,**非本系统加 rent cap 修正**(rent cap 违背①)。落后者体验托底归 Alpha 翻盘注入器(交易11/拍卖12/命运之轮13/策略卡15)。原型期 playtest 须验证 MVP-without-injectors 的落后者体验是否可接受;不可接受则**加速 injector 排期、不引入 rent cap**。同批 playtest 观察项:CR-8"仅自己回合建房"对支柱②"建房引信"社交感的削弱(MVP 被动兑现 vs 主动宣告)。**通告 beat 具名义务(R3 game-designer 补;✅ player-turn 侧 RESOLVED 2026-06-04 `/propagate-design-change`:player-turn CR-3.5 `OnBuildingAnnounced` 广播 + AC-53;HUD(16) 消费侧已登记 systems-index 继承义务表,待 HUD GDD 兑现):** 预承诺补救"ResolvePhase 全员可见建房通告 beat"的规格委托 **player-turn(2) ResolvePhase + HUD(16)**,须定义:触发时机(建房事务完成即时广播 vs ResolvePhase 结束汇总)/ 信息内容(至少含格名、新 house_count、建房玩家)/ Pass'N Play(同屏其他玩家当场可见,beat 可简化,低削弱风险)vs 未来多屏/在线(高削弱风险,beat 必需)。本系统侧 `OnBuildingChanged` 事件已供数据源,通告呈现归下游。(creative-director 裁定,R1 登记;R3 补具名义务。) | game-design / playtest + creative-director | MVP 原型期 playtest |

**已在设计中关闭(记录):** 房屋供给/酒店机制/均衡卖房/建房时机 4 项裁定已定(MVP无限/经典酒店/须均衡/仅自己回合);酒店"不翻倍"与巨额投资激励一致性已由 economy-designer 核验;建/卖 churn 无套利已证(F-5/AC-20)。**tile-events OQ-Event-3(RepairFee 全盘 house_count/hotel_count 口径)本轮 `/consistency-check` 揪出并关闭**:F-7 暴露 `GetTotalHouseCount`/`GetTotalHotelCount`(与 per-tile [0,5] 区分),tile-events 侧 + registry + systems-index 已同步(见 Dependencies 双向一致性注)。
