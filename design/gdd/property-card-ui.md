# 地产卡与卡牌 UI（Property Card & Card UI）

> **Status**: ✅ APPROVED(2026-06-05 R-2 fresh design-review,finishing-class,用户 accept)。本档内 finishing 修订完成(F-1 G-0 guard、vacuous-spy AC-1/16/24、EC-6 无主×垄断、AC-25 措辞、AC-42 升 OQ-PC-8 门控、tnum 诚实化、信息架构消歧)。**3 跨档 must-land 已 propagate 落地 + fresh-grep 核验**:OQ-PC-8 ✅(economy-cash 加纯函数 `GetUnmortgageCostForDisplay`)/ OQ-PC-11 ✅(economy-cash 登记 depended-on-by #17)/ OQ-PC-6 🔶(hud.md OQ-HUD-10 回标 provisional-RESOLVED ✅;art-bible 垄断视觉 propagate 待 art-director)。OQ-PC-9/10 ✅、systems-index ✅。**剩余非阻 followup**:art-bible §6.2/§7.4(b) 垄断视觉(OQ-PC-6 art 部分)、§7.2 等宽字体 + Utility 色组(asset-spec 前)、架构 ADR OQ-PC-4(门控 AC headless 前提)。
> **Author**: msc + design-system agents
> **Last Updated**: 2026-06-05(R-2 revision)
> **Implements Pillar**: ④ 易上手（地产/租金信息清晰可读）+ ① 桌游忠实（地产卡=大富翁产权契据）+ ② 社交博弈（抵押/垄断可见性=对手是否可乘之机）
> **MVP 范围**: 地产/车站/公用事业**产权契据详情卡** + 建房/卖房/抵押/赎回动作转发。机会/命运翻牌 UI 与监狱动作卡 deferred Alpha（依赖未定 #7 卡牌数据）。

## Overview

地产卡与卡牌 UI(#17)是《骰子大亨》呈现层的**产权信息中枢**——它把棋盘(1)、地产所有权(6)、建房升级(8)持续维护的只读数据,翻译成玩家一眼可读的**产权契据卡**:地块名、色组、购买价、整条租金阶梯(无房→1~4 栋→酒店)、抵押价、当前归属、建房数与抵押/垄断状态。作为**呈现层纯叶子**,#17 只**读取/订阅**数据、只负责**显示与转发玩家意图**;建房/卖房/抵押/赎回按钮仅向 owner 系统(经回合2 ResolvePhase → #8/#6 执行)**转发意图**,#17 从不写游戏状态、不裁决结算——所有数值由 owner 先算定。对玩家而言,这张卡就是大富翁桌面上那张**产权契据**:翻开它、读懂"这块地现在收多少租、再建一栋要多少钱、抵押能套现多少、对手的垄断组是否已凑齐",是核心决策循环里反复发生的桌游时刻。它与 HUD(16)边界清晰:HUD 拥玩家面板/现金/回合/操作栏与一个"地产卡快览入口",#17 拥**产权卡详情面板**本身。没有它,玩家无法读懂任一地块的租金结构与建房代价,买地/建房/抵押的策略决策将退化为盲猜。

## Player Fantasy

**核心感受:"翻开底牌——这块地值多少、能榨多少、押多少,我一眼看穿。"**

#17 服务的是一种**产权在握的掌控感**,直接落点支柱①桌游忠实与④易上手。当玩家点开一张地产卡,他摸到的就是大富翁桌上那张**产权契据**:整条租金阶梯(无房→1/2/3/4 栋→酒店)、再建一栋的代价、抵押能套现多少——清楚、分层、不需心算。这种"底牌透明"是策略决策的地基:买不买、建哪块、押哪张,代价摆在眼前,决策不再盲猜。

在掌控感之上,#17 点亮支柱②社交博弈的**侦察时刻**:

1. **"我的产权"** —— 翻开自己的地,绿色归属、垄断组凑齐时整组高亮,"这组我成了,该建房了"的成就感即时可见(①+④)。
2. **"对手的破绽"** —— **分两层渐进披露**:① **概览扫描**——棋盘 tile 常驻叠加(CR-10)让"谁垄断了、谁抵押缺钱"**不开卡即可一眼扫**(支柱②"判断对手是否可逼"的即时读牌,无需为看一眼对手状态被迫开卡);② **决策深读**——打开对手详情卡看精确租金阶梯/建房代价/赎回成本(策略深度信息)。两层服务不同决策粒度,叠加层承担即时侦察、详情卡承担深算,**非冗余而是分层**。抵押/垄断状态的可见性归 #17(承接 HUD OQ-HUD-10)。

*Design test(支柱④)*:在"信息密度/深度"与"易懂不吓退休闲玩家"之间,卡片永远优先后者——核心(价/租/归属)永远直读,次要(精确抵押费率算式)可折叠/次级字号。

而建房弹出、酒店合并、卡牌翻面的"获得感"juice,#17 **使能而不拥有**(enable-not-own):卡片提供准确的数值与事件锚点,弹性动画/星星粒子的视觉资产归 VFX(19)/art-bible §6.3;#17 拥**卡片信息布局、UMG widget 结构与数据绑定**。

> 注:`creative-director` 未咨询(lean 模式跳过)。production 前须人工复核本节情感措辞与支柱对齐。

## Detailed Design

> 约定:本节 #17 一律为**消费侧/呈现侧**——读数据、订阅事件、显示、转发玩家输入意图;**绝不**写游戏状态或裁决结算。owner 接口签名见 Interactions 表。

### Core Rules

**CR-1 卡片触发与上下文(handoff 协议)**:#17 暴露 `OpenCard(TileIndex)` 入口。触发源:① HUD 底部操作栏"地产卡快览入口"(HUD CR-1)转发当前/选定地块;② 玩家点击棋盘地块。#17 据 `TileIndex` 向 #1 `GetTileData(index)` 拉取数据并渲染。**HUD 仅转发"打开卡片"意图 + TileIndex,不拥卡片内容**(HUD CR-14 边界)。关闭由 #17 自持(CR-11)。

**CR-2 卡片类型路由**:据 `FBoardTileData.TileType` 路由——`Property`/`Railroad`/`Utility` 三类渲染**产权契据卡**(CR-3/4/5)。**MVP 非购买地块**(`Tax`/`Jail`/`Go`/`Chance`/`CommunityChest`/四角)**不开详情卡**;机会/命运**翻牌卡**与监狱**动作卡** deferred Alpha(依赖未定 #7 `FCardData`,见 Open Questions)。MVP 落这些地块仅由 HUD/事件通告呈现,不经 #17。

**CR-3 地产契据卡(Property)内容**:读 #1 `{DisplayName, ColorGroup, PurchasePrice, RentTable[0..5], BuildingCost, MortgageValue}` + #6 `{GetOwner, IsMortgaged, IsMonopoly}` + #8 `{GetHouseCount, CanBuildHouse(+禁用原因), GetSellback}` + owner 提供的 `unmortgage_cost`(赎回成本,经济5 F-6 口径,见下)。呈现:① 顶部色组色条(art-bible §4.4,高 22%,饱和 ≥70%);② 地块名 + 购买价;③ **租金阶梯 6 行**(无房/1/2/3/4 栋/酒店 = `RentTable[0..5]`),**当前生效行高亮**(据 house_count 与 monopoly:`is_monopoly ∧ house_count==0` 时 `RentTable[0]` 行标注"×2 垄断",经济5 口径,见 Formulas 边界);④ 建房区块——每栋建房价 `BuildingCost` + **卖回额**(读 #8 `GetSellback`=`floor(BuildingCost/2)`,经济5 F-8 口径,#17 不自算)+ **建后租金预览**(下一栋后生效租金 = `RentTable[clamp(house_count+1,0,5)]`,兑现 building-upgrade L266「精算」义务;house_count==5 无预览);⑤ 抵押区块——**抵押套现额** `MortgageValue`(抵押放款,读 #1)+ **赎回价** `unmortgage_cost`(=`MortgageValue + ceil(MortgageValue×1/10)`,经济5 F-6;**读 owner 提供值,#17 绝不自算 ceil**——见 Dependencies propagate 债:owner/经济5 须经回合2 暴露 `GetUnmortgageCost(tile)`);⑥ 当前 owner 色徽章(玩家色+编号,§5.2)或"无主";⑦ `house_count` [0..5] 图标(1~4 栋 / 酒店);⑧ `is_mortgaged` 灰化+斜纹;⑨ `is_monopoly` 组高亮。

> **义务来源(building-upgrade L265-266,已 APPROVED 上游强制):** 地产卡须显示 ④卖回额 + 建后租金预览 + ⑦门控原因(CR-7/8 tooltip)。本 CR-3 + CR-7/8 兑现之;producer 须回标 building-upgrade L265-266 义务已被 #17 接收(propagate 闭合)。

**CR-4 车站契据卡(Railroad)内容**:读 #1 `{DisplayName, PurchasePrice, RentTable[0..3], MortgageValue}` + #6 `{GetOwner, IsMortgaged, GetStationCount}`。呈现 **4 行租金表**(拥 1/2/3/4 站),据 owner 的 `GetStationCount` **标注当前生效行**;owner 徽章、抵押视觉。**车站无建房**(无 BuildingCost/house 行)。

**CR-5 公用事业契据卡(Utility)内容**:读 #1 `{DisplayName, PurchasePrice, DiceMultiplierTable[0..1], MortgageValue}` + #6 `{GetOwner, IsMortgaged, GetUtilityCount}`。呈现 **倍率**(拥 1 个=×4 / 2 个=×10),**严禁显示为固定现金**——须文案明示"**实租 = 骰点 × 倍率**"(经济5 运行时算);据 `GetUtilityCount` 标注当前生效倍率;owner 徽章、抵押视觉。**公用无建房**。

**CR-6 多源数据聚合(纯读)**:#17 据 `TileIndex` 聚合三源——#1 tile data + #6 ownership 快照 + #8 `house_count`。**`house_count` 一律取自 #8 `GetHouseCount(tile)`,绝不取自 #6**(#6 快照不含 house_count;防 6↔8 环)。三源任一读取失败 → 见 Edge Cases EC-2 降级。

**CR-7 动作按钮 affordance(只读反映 + 意图转发 + 禁用原因)**:契据卡含动作按钮组——买地/建房/卖房/抵押/赎回(适用者)。按钮**启用/禁用是对只读 affordance 的呈现反映**(读 #8 `CanBuildHouse`/`CanSellHouse` + #6 归属/抵押 + 现金),点击**仅向回合2 ResolvePhase 公开接口转发玩家意图**(单次),#17 **不裁决结果、不写状态**(同 HUD CR-14)。**建房按钮禁用时须显门控原因 tooltip**——读 #8 `CanBuildHouse` 返回的禁用原因枚举(非垄断 / 组内有抵押 / 非最低档需先补齐 / 现金不足,building-upgrade L265 强制),#17 只**呈现** owner 给出的原因码、不自行判定为何禁用。owner 执行:买地/抵押/赎回→#6,建房/卖房→#8,均经回合2 ResolvePhase 路由、结算走经济5。

**CR-8 抵押按钮门(affordance 侧)**:抵押按钮仅当 `owner==当前玩家 ∧ ¬is_mortgaged ∧ GetHouseCount(tile)==0` 时启用(读 #8 灰化);**但权威门由 caller(回合2 ResolvePhase)在调 #6 `Mortgage` 前校验 `house_count==0`**——#17 灰化仅呈现 affordance,不构成权威闸(#6 读不到 #8 house_count,防环;真值由 ResolvePhase 聚合校验)。赎回按钮:`owner==当前玩家 ∧ is_mortgaged ∧ 现金≥赎回价`(赎回价=`unmortgage_cost`,经济5 F-6=`MortgageValue+ceil(MortgageValue×1/10)`;**读 owner 经回合2 暴露的 `GetUnmortgageCost(tile)`,#17 不自算**——见 Dependencies propagate 债)。

**CR-9 订阅实时刷新**:卡片打开期间,#17 订阅 #6 `OnOwnershipChanged(TileIndex,Old,New)`/`OnMortgageChanged(TileIndex,bMortgaged)` + #8 `OnBuildingChanged(TileIndex,NewCount)`;命中**当前卡片 TileIndex** 时实时刷新对应字段(归属徽章/抵押视觉/建房图标/按钮 affordance),**不关闭卡片**。

**CR-10 棋盘 tile 状态常驻叠加(承接 OQ-HUD-10,#17 拥)**:**即使卡片未打开**,#17 在棋盘各 tile 上常驻呈现 **抵押**(灰化+斜纹,§6.2)与 **垄断组**(组高亮)状态叠加。这是 HUD F-4 依赖的"对手抵押/建房状态持久可读"通道(HUD F-4 据此把 AI `Mortgage`/`Unmortgage` 判为非 critical 可跳过瞬时通告——前提是其结果状态在此常驻可见)。#17 订阅同 CR-9 事件维护该叠加。

> **⚠ 叠加 widget 所有权(架构盲区,须 ADR 落定):** 该常驻叠加**独立于按需开关的详情卡面板**,其 widget 归属/生命周期/实现路径(候选:① 每 tile 一个 world-space `UWidgetComponent` 跟随 tile;② 单一 screen-space overlay 层 `WorldToScreen` 定位)**未定,归架构期 ADR(并入 OQ-PC-4)**。MVP 设计意图:叠加由一个**常驻 owner**(候选 = HUD 根 widget 子树 或 一个 UI subsystem)持有,生命周期覆盖整局、不随详情卡开关创建/销毁。**AC-25/26 的 headless 可测前提依赖此 owner 在 ADR 落定**(同 OQ-HUD-2 门控)。垄断组环圈需枚举同组 tile → 读 #1 `GetTilesInGroup(group)`(Interactions 表已列)。
>
> **⚠ OQ-HUD-10 回标前提订正:** 本 CR-10 + V-Own-07 设计了垄断/抵押状态归属与视觉,但「OQ-HUD-10 RESOLVED」回标的**前提是叠加 owner 在架构 ADR 落定**——在 ADR 产出前,OQ-HUD-10 标为 **provisional-RESOLVED**(归属已定、实现通道待 ADR 兑现),不可让 HUD 据一个尚无 owner 的通道把 AI 抵押判为可跳过(避免虚假闭合)。

**CR-11 输入与关闭**:鼠标点击为主(技术偏好,禁 hover-only,利手柄);卡片可经 关闭按钮 / 点击面板外空白 / Esc 关闭。打开新卡(`OpenCard` 另一 TileIndex)直接切换内容(重读),不堆叠多卡(MVP 单卡)。

**CR-12 juice RNG 隔离**:#17 本地任何 juice 随机(卡片粒子节奏等)**必须走独立非权威 `FRandomStream`**,**严禁复用骰子3 权威流**(同 HUD CR-16 / ai CR-5④,防重放污染)。

### States and Transitions

**卡片面板状态机(MVP 单卡)**

| 当前态 | 触发 | 新态 | 备注 |
|--------|------|------|------|
| Closed | `OpenCard(TileIndex)` 且 TileType∈{Property,Railroad,Utility} | Open | 读三源渲染卡(CR-3/4/5/6) |
| Closed | `OpenCard(TileIndex)` 且 TileType∉ 三类(MVP 非购买地块) | Closed | 不开卡(CR-2),静默忽略(EC-4) |
| Closed | `OpenCard(越界/无效 TileIndex)` | Closed | 不开卡,记告警(EC-3),不调 `GetTileData` |
| Closed | `OnOwnershipChanged`/`OnMortgageChanged`/`OnBuildingChanged`(任意 TileIndex) | Closed | 仅更新棋盘叠加(CR-10),不开卡(叠加独立于卡片开关) |
| Open | `CloseCard` / 点面板外 / Esc | Closed | 释放卡片内容订阅引用(棋盘叠加订阅保留,AC-26/30) |
| Open | `OnOwnershipChanged`/`OnMortgageChanged`/`OnBuildingChanged`(==当前 TileIndex) | Open | 刷新对应字段(CR-9),不关闭 |
| Open | `OnOwnershipChanged` 等(≠当前 TileIndex) | Open | 仅更新棋盘叠加(CR-10),卡片内容不变 |
| Open | `OpenCard(其他三类 TileIndex)` | Open | 切换内容、重读三源(不堆叠) |
| Open | `OpenCard(非购买/越界 TileIndex)` | Open | **保持当前卡片不变**(不切换、不关闭、不臆造),越界记告警(SM-1/2 补:Open 态非法 OpenCard 不破坏当前卡) |

> **状态机不变式**:① **单卡**——任意时刻至多一张详情卡 Open(MVP);② 棋盘 tile 状态叠加(CR-10)**独立于卡片开关**,始终活跃(订阅常驻,卡片开关不影响);③ **可证伪形式(非"订阅刷新保证"自证)**:Open(tile=T)态下,对任一字段 f,`显示值(f) == owner.Get(f, T)` 的最新返回值——若 `OnXChanged(T)` 已触发但 `显示值(f)` 仍为旧值(spy 可捕),即 violation(反例可证伪)。AC-21/22/23 即此不变式的具体可测实例(不接受"因订阅刷新保证故成立"的循环论证)。

### Interactions with Other Systems

> 方向约定:#17 是**呈现层叶子消费者**——`←` 接收/读取/订阅;唯一 `→` 是经回合2 公开接口**转发玩家输入意图**(非写状态)。

| 系统 | 数据流入 #17(#17 消费) | #17 流出 | owner |
|------|------------------------|----------|-------|
| **棋盘(1)** | `GetTileData(index)→FBoardTileData{DisplayName,ColorGroup,PurchasePrice,RentTable,DiceMultiplierTable,BuildingCost,MortgageValue,TileType}`;`GetTilesInGroup(group)`(垄断环圈枚举同组 tile,CR-10) | 无 | #1 拥地图/字段契约 |
| **地产所有权(6)** | `GetOwner`/`IsMortgaged`/`IsMonopoly`/`GetStationCount`/`GetUtilityCount`;订阅 `OnOwnershipChanged`/`OnMortgageChanged` | 无 | #6 拥归属/抵押运行时事实 |
| **建房升级(8)** | `GetHouseCount(tile)→[0..5]`/`CanBuildHouse`/`CanSellHouse`;订阅 `OnBuildingChanged` | 无 | #8 拥 house_count + 建房门 |
| **玩家回合(2)** | 无(读 affordance 经 #6/#8) | **转发意图**:买地/建房/卖房/抵押/赎回意图 → ResolvePhase 公开接口(单向,非写状态) | 回合2 拥流程编排 + caller 侧权威门校验(CR-8) |
| **经济(5)** | 不直连;**按经济租金公式口径显示**(×2 仅 RentTable[0]、utility=骰点×倍率)——只引用口径,不调 #5 | 无 | #5 拥结算 + 租金公式 |
| **事件格(7)** | **MVP 不消费**;机会/命运翻牌卡 deferred Alpha(订阅 `OnCardDrawn`/`OnTileEventResolved` 待 Alpha) | 无 | #7 拥卡牌效果;`FCardData` 未定(OQ) |
| **HUD(16)** | **边界**:HUD 转发"打开卡片"意图 + TileIndex(快览入口);HUD 拥玩家面板/现金/回合/操作栏,#17 拥产权卡详情 + 棋盘状态叠加 | 无(不向 HUD 推送) | 各拥各域,无重叠 |
| **游戏反馈 VFX(19)** | **enable-not-own**:#17 拥卡片信息布局+UMG+数据绑定;建房弹出/酒店合并/卡翻面 juice 视觉资产/Niagara 归 VFX19(art-bible §6.3),VFX 直订 #8/#6 事件,不经 #17 转发 | 无 | 各拥各域 |

> **⚠ RentTable 基数契约(消费侧期望,propagate 回 #1 确认):** `FBoardTileData.RentTable` 在 Property 为 6 元素(`[0..5]`=无房/1~4 栋/酒店)、Railroad 为 4 元素(`[0..3]`=拥 1~4 站)、Utility **不用** RentTable(用 `DiceMultiplierTable[0..1]`)。#17 **按 `TileType` 路由后**只访问对应基数的下标(Property 读 `[0..5]`、Railroad 读 `[0..3]`),**绝不跨类型越界访问**(如对 Railroad tile 读 `RentTable[4..5]`)。**#1 须明确 `RentTable` 是按 TileType 变长 `TArray<int32>`(推荐)还是定长**——此契约 owner 是 #1,producer 须 propagate 回 #1 确认并回标双向一致;在 #1 确认前 #17 按"变长、按 TileType 决定有效下标"实现。AC-7/AC-11 的下标语义依赖此契约。
>
> 注:`ux-designer`/`ue-umg-specialist` 未咨询(lean 模式)。widget 层级、数据绑定形式、卡片开关动画驱动 → 架构期 ADR(随 HUD OQ-HUD-2)+ Pre-Production `/ux-design`。

## Formulas

> **边界声明**:#17 是呈现层纯叶子,本节公式**仅限呈现层显示推导**。所有租金/价格数值(`RentTable`/`DiceMultiplierTable`/`MortgageValue`)由 owner(棋盘1/经济5)算定,#17 **读显不推导**——本节不含任何游戏经济/概率公式。动画时长常量(pop-in 200–300ms / flip 300ms / 按钮 80ms,art-bible §7.5)列 Tuning Knobs,不在此重复为伪公式。

### F-1 生效租金行索引(Active Rent-Row Derivation)

#17 显示**整条**租金表(全部行恒显),仅**高亮当前生效行**。生效行索引是 `{是否抵押, 持有计数}` 的确定性显示推导,**必须与经济5 租金口径严格同构**(否则卡片显示的高亮行与引擎实收租金发散——正确性硬风险)。三路按 TileType 分:

**G-0 前置 guard(第 0 分支,优先于下列 (a)/(b)/(c) 全部正常推导;实现须写在 clamp 之前):** 先做两类拦截,任一命中即短路返回 `highlight_row=-1`(Utility 为 `highlight_multiplier=-1`)+ 「数据异常」占位,**绝不进入下方 clamp**:
- **G-0a 越界拦截**:`house_count∉[0,5]` / `station_count∉[0,4]` / `utility_count∉[0,2]` 属 owner 数据缺陷 → 走 **EC-2 降级**(占位 + 告警),**不交给 clamp 静默压回**。clamp 仅是合法域 [0,N] 内的下标安全网,**不得充当越界值的隐形修正器**(只读公式符号而漏 G-0a = 实现缺陷,AC-11 越界例钉死)。
- **G-0b 矛盾态拦截(EC-6)**:`owner==NONE ∧ (house_count>0 ∨ is_monopoly)` 等逻辑矛盾态(无主却有房 / 无主却垄断)→ 强制 `-1` 且 **`show_monopoly_badge=false`**,**绝不渲染逻辑上不存在的收租档或 ×2 徽章**(AC-32 两变体钉死)。

**(a) Property:**
`highlight_row = -1` if `is_mortgaged`;  `= 0`(并显 ×2 垄断徽章) elif `is_monopoly ∧ house_count==0`;  `= clamp(house_count, 0, 5)` else

| 变量 | 符号 | 类型 | 范围 | 描述 |
|------|------|------|------|------|
| 是否抵押 | is_mortgaged | bool | {f,t} | #6 `IsMortgaged`;最高优先短路 |
| 是否垄断 | is_monopoly | bool | {f,t} | #6 `IsMonopoly`;仅 ¬is_mortgaged 后评估 |
| 建筑档位 | house_count | int | [0,5] | #8 `GetHouseCount`;5=酒店 |
| 高亮行索引 | highlight_row | int | {-1,0..5} | -1=无高亮(抵押整卡灰化);0..5=`RentTable` 下标 |

**×2 徽章派生(独立 bool,须显式落实现):** `show_monopoly_badge = ¬is_mortgaged ∧ is_monopoly ∧ house_count==0`。**严防发散**:徽章条件**必含 `house_count==0`**——经济5 垄断翻倍仅在无房时生效(`is_monopoly ∧ house_count==0` 才 `RentTable[0]×2`);若实现漏 `house_count==0`,则"垄断+1栋"误显 ×2 而引擎实收 `RentTable[1]` 不翻倍,卡片骗人。

**(b) Railroad:** `highlight_row = -1` if `is_mortgaged ∨ station_count==0`;  `= clamp(station_count−1, 0, 3)` else
(`station_count`∈[0,4] 来自 #6 `GetStationCount`;输出 {-1,0..3}=`RentTable[0..3]`=拥 1~4 站)

**(c) Utility:** `highlight_multiplier = -1` if `is_mortgaged ∨ utility_count==0`;  `= clamp(utility_count−1, 0, 1)` else
(`utility_count`∈[0,2] 来自 #6 `GetUtilityCount`;输出 {-1,0,1}=`DiceMultiplierTable[0/1]`=×4/×10。公用**不显固定现金**,只高亮生效倍率 + 文案"实租=骰点×倍率")

**Output Range:** 三路均含哨兵 **-1**(抵押/无主=无行高亮,合法输出不可被 clamp 掉)。
**越界 / 矛盾语义见 G-0(第 0 分支,优先于本节 (a)/(b)/(c)):** 越界值走 EC-2 降级(不 clamp 静默压回)、矛盾态(含 `owner==NONE ∧ house_count>0` 与 `owner==NONE ∧ is_monopoly`)强制 `-1` + 抑制 ×2 徽章,均在下方 clamp **之前**拦截。clamp 仅是合法域内的下标安全网,不掩盖越界 / 矛盾。
**不变式(R-mirror):** F-1(a)/(badge) 的分支结构必须逐分支同构于经济5 `property_rent` piecewise;(b)/(c) 同构于 `railroad_rent`/`utility_rent`。回归守护见 AC(AC-11 表驱动 8 边界,**同构性为 [Advisory·cross-system] + propagate 债**——见下)。
**Example:** Property `{¬mortgaged, monopoly, house_count=2}` → else 分支 → `clamp(2,0,5)=2`,高亮 `RentTable[2]`,**无 ×2 徽章**(有房);`{¬mortgaged, monopoly, house_count=0}` → `0` + ×2 徽章;`{mortgaged,...}` → `-1` 整卡灰化。Railroad `station_count=3` → `clamp(2,0,3)=2`。Utility `utility_count=2` → `1`(×10)。

## Edge Cases

> 注:#17 为呈现层纯叶子,以下均为**呈现正确性**问题,绝不涉及游戏状态写入。`systems-designer` 未咨询(lean 模式);Pre-Production `/ux-design` 复核。

- **EC-1 卡片打开期间归属/抵押/建房变化**:据 CR-9 订阅刷新对应字段(归属徽章/抵押视觉/建房图标/F-1 高亮行/按钮 affordance),**不关闭卡片**;显示值始终等于最新权威。
- **EC-2 三源(#1/#6/#8)某源读取失败或缺数据**:降级——缺失字段显占位("—"),其余正常渲染,**不崩溃**、记录告警;呈现层不阻断游戏(数据修复归 owner 系统)。
- **EC-3 `OpenCard(TileIndex)` 传入越界/无效 TileIndex**:不开卡,记录告警,不崩溃,不臆造数据。
- **EC-4 `OpenCard` 传入非购买地块**(Tax/Jail/Go/Chance/Community/四角):据 CR-2 **不开详情卡**(MVP 三类卡之外无卡),静默忽略(可选简短提示),不崩溃。机会/命运翻牌 deferred Alpha。
- **EC-5 抵押地产卡显示**:F-1 `highlight_row==-1`,整卡灰化+斜纹(§6.2),但**租金表行仍恒显(灰显)**——玩家仍读得到该地价值结构,只是当前实收租=0(经济5 口径)。`is_mortgaged` 视觉 + "已抵押"标签必现。
- **EC-6 数据源矛盾**(如 `house_count>0` 但 `owner==INDEX_NONE`,理论不应发生):**fail-safe 不高亮**——字段层各 owner 返回值原样呈现(owner 显"无主"、house_count 按 #8 图标),但 **`highlight_row` 强制置 -1(无任何租金行高亮)+ 显式「数据异常」占位标签**,**绝不高亮一个逻辑上不存在的收租档**(如无主地却高亮 `RentTable[3]` 会主动误导玩家以为有人收此租)。记录告警,**#17 不自行修正字段值/不臆断**(矛盾归 owner 系统排查),但**呈现层有义务不渲染自相矛盾的收租承诺**。AC-32 钉死此场景下 `highlight_row==-1`。
- **EC-7 同帧多次/快速 `OpenCard` 切换**:以**最后一次**为准(单卡,CR-11),取消上一次渲染,不堆叠、不闪烁残留。
- **EC-8 垄断组内某地抵押(monopoly × mortgage,OQ-Prop-5 provisional)**:#17 **据 #6 `IsMonopoly` 返回值呈现,不自行判定**——当前 #6 设计"抵押不破垄断"(组高亮保留 + 该地灰化);**若 OQ-Prop-5 翻转为"抵押破垄断",`IsMonopoly` 即返 false、组高亮自动消失**,#17 无需改逻辑(口径单源归 #6,见 Dependencies provisional 标注)。
- **EC-9 读档后(存档21)卡片重建**:卡片是瞬态 UI **不序列化**;读档后若卡片处打开态,**重绑全部订阅(#6/#8 delegate,BP `Bind Event` 须先解绑再绑,同 HUD AC-48 footgun)+ 从快照重读三源**直接呈现最终态,不回放历史;棋盘叠加(CR-10)从快照重建。
- **EC-10 公用事业倍率被误读为现金**:CR-5 强制文案"实租=骰点×倍率"防误解;缺该文案 = 呈现缺陷(AC 守)。
- **EC-11 按钮 affordance 与权威门不一致**:#17 灰化是**尽力呈现**(读 #8 `CanBuildHouse` 等);若 #17 误启用按钮、玩家点击 → 回合2 ResolvePhase 权威拒绝(意图被拒、**不写状态、无害**),但应尽量对齐避免误导(CR-7/8)。权威门**始终以 owner 为准**,#17 affordance 非权威闸。
- **EC-12 juice RNG 误接骰子3 权威流**:设计/实现禁止(CR-12);code-review 发现复用即缺陷(ai CR-5④ 重放污染)。
- **EC-13 赎回价/抵押价混淆**:抵押区块**必须同时显**抵押套现额(`MortgageValue`,放款)与赎回价(`unmortgage_cost`=`MV+ceil(MV×1/10)`,经济5 F-6)。**只显 `MortgageValue` = 呈现缺陷**——玩家会低估赎回成本 10%(MV=100 实际赎回 110),违背 Player Fantasy「押多少…一眼看穿、无需心算」。`unmortgage_cost` **读 owner 提供值,#17 不自算 ceil**(避免与经济5 取整口径分歧)。AC-42 守。
- **EC-14 卖回额/建后租金预览缺失**:Property 建房区块缺卖回额或建后租金预览 = 未兑现 building-upgrade L265-266 上游强制义务(呈现缺陷)。卖回额读 #8 `GetSellback`、建后租金预览取 `RentTable[clamp(house_count+1,0,5)]`(house_count==5 无预览,不报错)。AC-43 守。

## Dependencies

> #17 是**呈现层叶子**:除"经回合2 转发玩家意图"(单向输出)外,所有依赖均为**单向读取/订阅**(#17 ←)。**⚠ 索引订正(propagate):systems-index #17 的 depends-on 列为 `1, 6, 7`——应订正为 `1, 6, 8`(+ 7 仅 Alpha)**:#8 是 MVP 硬读依赖(house_count/建房门),#7 在 MVP 不被 #17 消费(机会/命运翻牌 deferred Alpha)。

### 硬依赖(MVP 系统无法运作)

| 系统 | 依赖性质 | 接口/数据 |
|------|---------|-----------|
| **棋盘(1)** | 硬 | `GetTileData(index)→FBoardTileData`(DisplayName/ColorGroup/PurchasePrice/RentTable/DiceMultiplierTable/BuildingCost/MortgageValue/TileType);`GetTilesInGroup(group)`。#1 拥字段契约 |
| **地产所有权(6)** | 硬 | `GetOwner`/`IsMortgaged`/`IsMonopoly`/`GetStationCount`/`GetUtilityCount`;订阅 `OnOwnershipChanged`/`OnMortgageChanged`。#6 拥归属/抵押运行时事实 |
| **建房升级(8)** | 硬(**index 漏列,须补**) | `GetHouseCount(tile)→[0..5]`/`CanBuildHouse`/`CanSellHouse`;订阅 `OnBuildingChanged`。#8 拥 house_count + 建房门;**house_count 一律取自 #8 不取 #6(防 6↔8 环,CR-6)** |

### 软/间接依赖

| 系统 | 依赖性质 | 说明 |
|------|---------|------|
| **玩家回合(2)** | 软(单向输出) | 经 ResolvePhase 公开接口**转发**买地/建房/卖房/抵押/赎回意图;#17 不写状态、权威门校验归回合2(CR-7/8) |
| **经济(5)** | 软(口径引用) | 不直连;按经济租金公式口径**显示**(F-1 同构 `property_rent`/`railroad_rent`/`utility_rent`)。赎回价/sellback 等数值显示读 owner 提供值,不重算 |
| **事件格(7)** | **Alpha-deferred** | MVP #17 **不消费**;机会/命运翻牌卡 Alpha 订阅 `OnCardDrawn`/`OnTileEventResolved`(依赖未定 `FCardData`,OQ-PC-1) |

### 呈现层边界(平级,非依赖)

| 系统 | 边界 |
|------|------|
| **HUD(16)** | HUD 转发"打开卡片"意图 + TileIndex(快览入口);HUD 拥面板/现金/回合/操作栏,#17 拥**产权卡详情 + 棋盘状态叠加(CR-10)**。互不依赖 |
| **游戏反馈 VFX(19)** | enable-not-own:#17 拥卡片布局+UMG+数据绑定;建房/酒店/翻牌 juice 资产归 VFX19,VFX 直订 #6/#8 事件 |

### 横切消费方(依赖 #17 行为,非 #17 依赖它)

| 系统 | 关系 |
|------|------|
| **存档/读档(21)** | 卡片瞬态 UI **不序列化**;读档后重绑订阅 + 从快照重读三源重建(EC-9) |
| **HUD(16) F-4** | HUD F-4 把 AI `Mortgage`/`Unmortgage` 判为非 critical(可跳过瞬时通告),**前提是其结果状态由 #17 CR-10 棋盘叠加常驻可读**——#17 须兑现此可见性通道(闭 OQ-HUD-10) |

> **双向一致性待同步(propagate,producer 协调):** ① systems-index #17 depends-on `1,6,7`→`1,6,8`(+7 Alpha);② #1/#6/#8 的"depended-on-by"应含 #17;③ HUD OQ-HUD-10(垄断/抵押状态归属)由本 GDD CR-10 兑现,回标 **provisional-RESOLVED**(归属已定,实现通道待架构 ADR 定叠加 owner,见 CR-10 订正)。**R-1 新增 propagate 债**:④ economy5/回合2 须暴露 `GetUnmortgageCost(tile)`(OQ-PC-8,赎回价无源则 AC-42 不可兑现);⑤ building-upgrade L265-266 卖回额/建后租金预览/门控原因义务回标 #17 已接收(OQ-PC-9);⑥ #1 `RentTable` 基数契约确认(OQ-PC-10);⑦ economy5 租金 piecewise 变更触发 #17 F-1/AC-11 重核债(OQ-PC-11,登记 economy-cash depended-on-by #17)。

## Tuning Knobs

> 全部为**呈现层数据驱动旋钮**。固定值(card flip 300ms / 按钮 80ms,art-bible §7.5)是硬约束非旋钮。视觉量(色条高 22%、饱和 ≥70%、抵押灰化 60%+斜纹、垄断高亮)归 **art-bible §4.4/§6.2/§7.4b** 拥有,#17 不复制为旋钮,引用源真值。

| 旋钮 | 来源 | 默认 | 安全范围 | 调太高 | 调太低 |
|------|------|------|---------|--------|--------|
| `T_card_open` | art-bible §7.5 | 250ms | 200~300ms | 卡片弹出拖沓 | 弹出突兀缺手感 |
| `T_card_flip`(Alpha 翻牌) | art-bible §7.5 | 300ms | **固定**(§7.5 硬线) | — | — |
| `T_button_press` | art-bible §7.5 | 80ms | **固定**(§7.5) | — | — |
| `reduce_motion` | 全局 a11y(承接 HUD) | Off | {Off,On} 布尔 | —(On=卡片 pop-in/flip 去动效改瞬现、按钮去 scale) | —(Off=保留全部动画) |

### 旋钮交互
- **`reduce_motion` ↔ 卡片动画**:On 时 `T_card_open`/`T_card_flip` 旁路为瞬现(同 HUD AC-57 reduce-motion 模式),与 HUD 全局 a11y 开关**共享同一布尔**(不独立),避免两套 a11y 开关割裂。具体降幅归 OQ-HUD-9 `/ux-design`(与 HUD 统一)。

## Visual/Audio Requirements

> **边界声明(enable-not-own)**:#17 拥有卡片信息布局、UMG widget 结构、数据绑定及所有卡片/棋盘叠加的状态视觉(色组色条、所有权徽章、高亮行、抵押灰化、垄断组完成标记);juice 视觉资产(建房弹出粒子、酒店合并闪光、卡牌翻面粒子)归 **VFX(19)**,音效归 **音频(22)**。区分 #17 拥有(V-Own)与使能不拥有(V-Enable)。

### 1. #17 拥有的视觉需求(V-Own)

#### V-Own-01 地产契据卡(Property)布局
**依据**: art-bible §7.4(b)、§4.4、§3.4、CR-3
| 区域 | 规格 |
|------|------|
| **色组色条** | 卡高 22%(§7.4b),色组主色(§4.4),饱和 ≥70% HSV;背景 Off White `#FAFAF5` |
| **地块名称** | Level 3(14–18px SemiBold,Dark Ink `#1A1A2E`) |
| **购买价** | Level 2(20–28px SemiBold)Fortune Gold `#F4C430`,"价格"前缀(Level 4) |
| **租金阶梯(6 行)** | 每行:行标签(Level 4,Dark Ink 60%)+ 金额(Level 3,Tabular Figures 等宽);无房/1/2/3/4 栋/酒店=`RentTable[0..5]`;**当前生效行高亮(V-Own-04)** |
| **建房区块** | Level 4;每栋造价 `BuildingCost` + **卖回额**(读 #8 `GetSellback`=`floor(BuildingCost/2)`,经济5 F-8)+ **建后租金预览**「建后→ `RentTable[house_count+1]`」(house_count<5 显;==5 隐)。**三者并列**,兑现 building-upgrade L265-266 精算义务 |
| **抵押区块** | Level 4;**抵押套现额** `MortgageValue`(放款)+ **赎回价** `unmortgage_cost`(读 owner `GetUnmortgageCost`,经济5 F-6=`MV+ceil(MV×1/10)`);两值并列须文案区分(「抵押可得」vs「赎回需付」),**严禁只显其一**(只显 MortgageValue=误导赎回成本,EC-13 AC 守) |
| **所有权徽章** | 右下角圆形 20–24px;玩家色填充 + P1–P8 白数字(§5.2);无主显灰占位 |
| **建筑图标行** | house_count∈[1,4] 绿色小房图标;5=红色酒店图标(§6.3);0=空 |
| **门控原因 tooltip** | 建房按钮禁用时悬浮/可达呈现禁用原因(读 #8 `CanBuildHouse` 原因码:非垄断/组内有抵押/非最低档需补齐/现金不足,building-upgrade L265);手柄可达非 hover-only(CA/AC-40) |

面板:圆角 8–12px,Off White 背景,软阴影(blur ≥8px/偏移 ≤4px,§3.4),宽度固定不撑动。

#### V-Own-02 车站契据卡(Railroad)
**依据**: CR-4、§4.4。同 V-Own-01 框架,差异:色条用深墨 `#1A1A2E`;租金 **4 行**(拥 1/2/3/4 站=`RentTable[0..3]`);无建房单价行;建筑图标行改显车站图标(1–4 枚)。

#### V-Own-03 公用事业契据卡(Utility)
**依据**: CR-5、EC-10。同框架,差异:色条用 Utility 色(待 art-bible §4.4 正式分组,fallback 天空蓝 `#3AACDB`,OQ);**不显固定金额租金表**,改倍率区块——行1 ×4(`DiceMultiplierTable[0]`)、行2 ×10(`[1]`),倍率用 Level 2;下方固定文案(Level 4)「**实租 = 骰点合计 × 倍率**」——**不可省略(EC-10 AC 守)**;无建房单价行。

#### V-Own-04 活跃租金行高亮
**依据**: F-1、CR-3/4/5
**正常高亮(`highlight_row ≥ 0`)**:生效行背景=色组主色 30% 不透明覆盖;**行标签升 SemiBold(前提:非生效行用 Regular/Medium,art-bible §7.2 须明确非生效行字重否则"升 SemiBold"是零信号,a11y CA-04)**;金额亮度 +20%;左 4px 竖线(色组主色饱和版 ≥70%);若 `show_monopoly_badge==true`(F-1(a))在行标签右显「×2」角标(Fortune Gold 底/Dark Ink 字,Level 4)——**仅 `¬is_mortgaged ∧ is_monopoly ∧ house_count==0` 时显,有房绝不显**。
**无高亮(`highlight_row==-1`,抵押/矛盾态)**:全部租金行灰显(Dark Ink 40%)**仍恒显**(EC-5),无高亮/竖线/徽章。
**Tabular Figures 等宽(同 HUD CR-3 传播债):** 金额数字须用**字形本身等宽的字体资产**。**保守结论(R-2 诚实化:负面命题不靠"文档没写"反证)**:据引擎参考(`docs/engine-reference/unreal/`,截止核验日)+ 截止训练知识,UE5.7 `FSlateFontInfo` 公开层无 per-run OpenType feature(`tnum`)控制 API、无 `FTypographicFeatures`(HarfBuzz feature 仅存引擎内部 shaping);**Pre-Production 前须一次引擎源码正向核验(grep `SlateCore`/`FreeType` 模块 `tnum`/`OpenTypeFeature`/`FTypographicFeature`)再终判**。当前**保守按等宽字形字体资产走**(若核验翻转则 §7.2 改用 feature 激活,HUD CR-3 与本档双档同改)。**⚠ art-bible §7.2 须指定具体等宽数字字体资产名(与 HUD CR-3 共享债)**——此为真 propagate 债(Pre-Production 前 §7.2 仍缺资产名则 WBP TextBlock 无可绑字体,见 Asset Spec Flag)。

#### V-Own-05 所有权徽章
**依据**: §5.2、§6.2、CA-03。右下角圆形;有主=玩家色填充 + 白 P 编号(≥10px);无主=Off White + "无"(Level 4);直径 20–24px;**色盲冗余=颜色 + P 编号双重(CA-01)**。

#### V-Own-06 抵押视觉
**依据**: §6.2、§7.4(a)、EC-5
**卡片**:整卡去饱和 HSV ×0.4(≈60%,与 §7.4a 破产面板一致语义);对角斜纹遮罩(Dark Ink 20% 半透,右上→左下,条宽 ≈8px/间距 ≈12px);**「已抵押」文字标签(Level 3 SemiBold)不可省略(EC-5 AC 守)**;徽章保留同步去饱和;色条保留位置降饱和;租金行全灰无高亮。
**棋盘 tile 叠加(CR-10 常驻)**:整格去饱和 + 斜纹,独立于卡片开关;tile 上「已抵押」文字省略(空间有限),可加小「M」图标(/asset-spec 细化)。
**一致性**:破产面板始终可见 vs 抵押卡按需打开,不同层级但同视觉语义("不活跃/不可用")。

#### V-Own-07 垄断组"完成"视觉(关键决策,**闭合 OQ-HUD-10**)
> **背景**:OQ-HUD-10 明确"垄断组完成/抵押状态归 #17",无锁定 art-bible spec。本条首次做具体视觉决策,须传播回闭 OQ-HUD-10 + art-director propagate 进 art-bible。

**决策:双层"色组环圈 + 完成角标"(Monopoly Ring Badge)**——在可读/色盲安全/与其他状态区分三项最优,契合卡通 2.5D,小 tile 仍可辨。
**A. 卡片详情层**:① 色组环圈=卡片外轮廓 3px 描边,色组主色饱和 100%(无主/抵押不显);② 完成角标=左上角(与右下所有权徽章对角不冲突),圆角矩形 Fortune Gold `#F4C430` 底 + Deep Ink「全」(Level 4)+ 小星图标,约 20×16px;③ 冗余=环圈(形状)+「全」(文字)+ 金色(颜色)三重。
**B. 棋盘 tile 叠加(CR-10)**:① 环圈=tile 外轮廓 2px 描边色组主色饱和 100%;② 完成角标缩小版=右下 Fortune Gold 小圆点(直径 6–8px)+ 内嵌白「★」,尺寸不够降级为纯金点;③ 与所有权 tile 徽章约 4px 偏移避重叠。
**区分矩阵**:抵押(去饱和+斜纹/暗)vs 垄断完成(金色+环圈/亮)视觉极性相反;所有权徽章(右下)vs 完成角标(左上)位置不同;垄断+抵押共存(EC-8)依 #6 `IsMonopoly` 返回值,环圈与斜纹各自独立共存。
**备选淘汰**:发光 glow(依赖 Emissive,§8.5 禁主驱 Lumen + 明亮背景噪声大);纯 badge 无环圈(tile 小尺寸太小);色组色填整格(与持有难区分)。
**⚠ art-bible propagate 债(A)**:art-director 须传播进 art-bible §6.2(地块状态视觉表加"垄断完成"行)+ §7.4(b)(地产卡加"垄断完成"条)+ 新增垄断/抵押共存规则。
**⚠ OQ-HUD-10 闭合**:本 GDD CR-10 拥棋盘叠加 + 本条设计垄断完成视觉 → **OQ-HUD-10 归属问题 RESOLVED**;producer 协调回标 HUD OQ-HUD-10 RESOLVED,回链本 GDD CR-10 + V-Own-07。

#### V-Own-08 棋盘 tile 状态叠加(常驻,CR-10)
**依据**: CR-10、§6.2
| 状态 | 视觉 | 优先级 |
|------|------|--------|
| **抵押** | 整格去饱和 60% + 对角斜纹 | 高(覆盖垄断环圈) |
| **垄断组完成** | 2px 色组环圈 + Fortune Gold 完成圆点 | 低(被抵押覆盖) |
共存(EC-8):斜纹灰化 + 环圈(环圈随去饱和降饱和度,保轮廓位置)。

### 2. #17 使能但不拥有的 Juice(VFX19 / 音频22 事件锚点)

> VFX/音频 **直接订阅 #6 `OnOwnershipChanged`/`OnMortgageChanged` + #8 `OnBuildingChanged`**,不经 #17 转发;#17 不持 VFX/音频引用(enable-not-own)。

| 编号 | 触发事件 | #17 提供锚点 | Juice 归属 |
|------|---------|------------|-----------|
| V-Enable-01 | #8 `OnBuildingChanged(tile,NewCount∈[1,4])` | tile 屏幕/世界坐标、NewCount | 建房弹出弹性 + 星星粒子(§6.3)— VFX19 |
| V-Enable-02 | #8 `OnBuildingChanged(tile,5)` 升酒店 | tile 坐标、酒店图标位 | 4栋→酒店合并闪光+旋转升起(§6.3)— VFX19 |
| V-Enable-03 | `OpenCard` 面板 Closed→Open | 面板中心坐标、TileType | 卡牌翻转入场粒子 — VFX19(翻转动画本身属 V-Own) |
| V-Enable-04 | 买地意图成功(#6 `OnOwnershipChanged`) | 徽章位置、玩家色 | 买地金色脉冲 + 地产卡飞入 — VFX19 |
| V-Enable-05 | `OpenCard` | TileType | 卡牌翻面音(纸张质感)— 音频22 |
| V-Enable-06 | #8 `OnBuildingChanged([1,4])` | NewCount | 建房落地咔哒音 — 音频22 |
| V-Enable-07 | #8 `OnBuildingChanged(5)` | 酒店信号 | 酒店合并铿锵音(§6.3)— 音频22 |
| V-Enable-08 | 抵押意图成功(#6 `OnMortgageChanged`) | — | 抵押提交低沉音 — 音频22 |

### 3. 色盲安全 / 可读性(CA)
**依据**: §4.6、§5.2、§7.2
- **CA-01 所有权**:玩家色 + 白 P 编号(≥10px)双冗余;禁仅色区分(P1 红 vs P3 绿高风险对,编号是唯一可靠信号)。
- **CA-02 垄断完成**:形状(色组环圈)+ 文字(「全」/★)+ 颜色(Fortune Gold)三重,不仅靠色。
- **CA-03 抵押**:饱和度(60% 去饱和)+ 纹理(斜纹)+ 文字(「已抵押」)三重。
- **CA-04 高亮行**:背景色块 + 左竖线(位置)+ 字重 SemiBold + ×2 文字徽章,非纯色。
- **CA-05 UI QA 色盲清单**(Coblis Deuteranopia/Protanopia):① 垄断完成 vs 普通持有(环圈+角标文字);② 抵押 vs 正常(去饱和+斜纹);③ P1 红 vs P3 绿(P 编号);④ ×2 徽章(「×2」文字非纯金色)。

### 4. 音频锚点(归音频22)
> #17 仅持事件时刻 + 上下文;Sound Cue/混响归音频22,直订 #6/#8 事件。

| 编号 | 触发时刻 | #17 上下文 | 音效需求 |
|------|---------|-----------|---------|
| A-01 | `OpenCard` Closed→Open | TileType | 卡牌翻开音(纸张,≤0.3s,三类一致 MVP) |
| A-02 | `CloseCard`/Esc | — | 关闭收叠音(可选 ≤0.2s) |
| A-03 | "买地"意图 | 名/价 | 买地确认正向 chime |
| A-04 | "建房"意图(转 #8) | NewCount | 由 VFX19/音频22 直订 #8 事件(同 V-Enable-06/07,#17 不重复触发) |
| A-05 | "抵押"意图 | MortgageValue | 抵押提交低沉音(配卡片灰化) |
| A-06 | "赎回"意图 | 赎回价 | 赎回轻正向 chime |

> 音量/混响分组归音频22;每锚点对应可独立关闭 `SC_` Sound Cue(§8.2)。

> **📌 Asset Spec Flag(#17)**:Visual/Audio 已定义。以下 propagate 债须先闭合,再 `/asset-spec system:property-card-ui`:① art-bible §7.2 **等宽数字字体资产名**(与 HUD CR-3 共享;tnum API 路线已排除〔V-Own-04〕,必须落具体等宽字形字体资产,缺则 WBP 无可绑字体——升为真 BLOCKING propagate 债);② art-bible §6.2/§7.4(b) 垄断组完成视觉规格(V-Own-07);③ art-bible §4.4 Utility 色组色(V-Own-03 fallback 天空蓝);④ OQ-HUD-10 **provisional-RESOLVED** 回标(producer,回链 CR-10 + V-Own-07;实现通道待架构 ADR 定叠加 owner);⑤ art-bible §7.4(b) 地产卡加「建房区块=造价+卖回额+建后租金预览」「抵押区块=套现额+赎回价」「门控原因 tooltip」布局条(R-1 新增,兑现 building-upgrade L265-266);⑥ a11y:art-bible §7.2 非生效租金行字重(CA-04 高亮信号前提)+ 10px→text-scale CONCERN(OQ-PC-12)。

## UI Requirements

> #17 是 UI 系统。本节定义**屏上元素清单 + 信息架构 + 输入要求**;详细交互规格(布局尺寸/focus 顺序/手柄导航)归 Pre-Production `/ux-design`(见 UX Flag)。

### #17 贡献的屏上元素(WBP 候选)
| 元素 | 内容 | 始终可见? | 来源 CR |
|------|------|-----------|---------|
| **产权卡详情面板** | Property/RR/Utility 契据卡(名/价/租金阶梯/抵押价/归属/建房数/抵押·垄断态/动作按钮) | 上下文(OpenCard 触发) | CR-1/3/4/5/7 |
| **棋盘 tile 状态叠加** | 抵押灰化+斜纹、垄断组环圈+完成角标 | **常驻**(独立卡片开关) | CR-10 V-Own-07/08 |
| **动作按钮组** | 买地/建房/卖房/抵押/赎回(affordance 随只读状态) | 上下文(卡片内) | CR-7/8 |
| **关闭控件** | 关闭按钮 / 点面板外 / Esc | 上下文(卡片内) | CR-11 |

### 信息架构(可读性优先,支柱④)
- **永远直读**:地块名、购买价、当前生效租金行、归属——核心决策信息不藏 hover/二级菜单。
- **可折叠/次级**:精确抵押**费率算式**(×1.1 推导过程)、完整租金阶梯历史档位可次级字号呈现(支柱④"易懂不吓退")。**注(R-2 消歧)**:可次级的是费率**算式/推导**;**赎回价数字本身属核心必显**(EC-13/AC-42 强制,不可折叠)——与 design test 不冲突。
- **不遮挡棋盘操作**:卡片面板不压盖棋盘中央操作区(art-bible §7.1 ≥65% 留棋盘);棋盘 tile 叠加(CR-10)是 tile 上轻量标记,不增交互阻挡。

### 输入要求
- **主交互鼠标点击**:打开卡片、动作按钮均点击(CR-1/7 转发意图);**禁 hover-only**(技术偏好,利手柄,AC-40)。
- **按钮 affordance**:启用/禁用反映只读状态(CR-7/8);禁用态须色 + 不透明度 + 图标三重冗余(CA)。
- **手柄适配预留**:可聚焦导航结构(CommonUI 候选),导航顺序留 `/ux-design` + 架构期 ADR。

### 边界
- HUD(16)拥玩家面板/现金/回合/操作栏 + "地产卡快览入口";#17 拥产权卡详情 + 棋盘状态叠加。主菜单/大厅归 #20。
- **MVP 范围**:仅 Property/RR/Utility 契据卡;机会/命运翻牌 UI + 监狱动作卡 deferred Alpha(OQ-PC-1/5)。

> **📌 UX Flag — 地产卡 UI(#17)**:本系统有 UI 需求。Pre-Production(Phase 4)须运行 `/ux-design` 为 **产权卡详情面板、棋盘 tile 状态叠加、动作按钮组** 各写 UX spec(与 HUD `/ux-design` 协调,共享 reduce-motion/handoff/手柄导航决策)。引用 UI 的 story 须引 `design/ux/[screen].md`,非直引本 GDD。

## Acceptance Criteria

> **类型说明**:[Logic]=确定性单测 headless `-nullrhi` 可测 BLOCKING;[Integration]=多系统事件驱动 BLOCKING;[Advisory]=纯视觉/feel 或 code-review 软约束(截图/签核)非阻塞。**门控标注** `[Logic·门控 OQ-HUD-2]`=headless 前提为 OQ-HUD-2 ADR 产出状态机/事件 handler 抽独立 UObject + IGameClock DI(同 HUD AC-12/24a/30/57),标在条目自身。**负向断言**:每条"#17 不做 X"须命名可 spy、可真违规的注入边界 + 接口黑名单(参考 HUD AC-29/43/46)。**渲染层诚实**:卡片动画时长/像素布局/高亮外观=`-nullrhi` 无探针 → [Advisory];仅状态机转换/F-1 推导/payload 校验/spy 结果=[Logic]。

### A. 卡片触发与类型路由(CR-1, CR-2)
- **AC-1** [Logic](CR-1 OpenCard handoff)GIVEN HUD 调 `OpenCard(TileIndex=5)`;WHEN 处理;THEN #17 向 #1 调 `GetTileData(5)` 返回 `TileIndex==5`,调用恰一次,且 `GetTileData(其他 index)` 调用==0(spy 有真 fail 条件:误读越界则≠0)。**写状态负向断言集中于 AC-16,不在此重复**(原"写接口调用==0"在 #17 架构无写引用时 vacuous-true,删,同 R-1 AC-5/39 处理)。
- **AC-2** [Logic](CR-2 三类路由打开)GIVEN `TileType=Property/Railroad/Utility` 三变体;WHEN `OpenCard`;THEN 状态机 Closed→Open,对应渲染路径 spy 调用==1,均不写 #6/#8 状态。
- **AC-3** [Logic](CR-2 非购买地块不开卡,负向)GIVEN `TileType∈{Tax,Jail,Go,Chance,CommunityChest,Corner}` 六变体;WHEN `OpenCard`;THEN 状态机留 Closed(**断言 `GetCardState()==Closed`**——以状态机权威状态为准,非渲染方法名 spy;`Render*` 方法名在 BP 路径不可靠 spy,见 OQ-PC-4),不崩溃,不写状态。**注**:渲染层"是否真挂载了卡片 widget"的负向验证 `-nullrhi` 不可靠 → 归 AC-3 之外的 code-review/playtest;本 AC 只验状态机停在 Closed(可 headless)。
- **AC-4** [Logic](EC-3 越界 TileIndex)GIVEN ① `-1` ② `40`;WHEN `OpenCard`;THEN 留 Closed,记告警,不崩溃,不调 `GetTileData`(spy==0,防读越界)。

### B. 卡片内容绑定(CR-3/4/5/6)
- **AC-5** [Logic](CR-3 Property 三源全读)GIVEN mock 三源确定值(#1 字段全、#6 `{P1,¬mortgaged,¬monopoly}`、#8 `GetHouseCount=2`);WHEN Open;THEN 数据绑定全取 mock 值无臆造。**「不直连 #5」改由 [Advisory·code-review] 守**——若架构上 #17 根本不持 #5 引用,则 spy==0 永真(vacuous);故降级为 reviewer 验 #17 源码/BP 无经济5(#5)直连引用(无 `#include EconomySystem` / 无 #5 节点)。证据 `propcard-no-econ-coupling-review.md`。
- **AC-6** [Logic](CR-6 house_count 读 #8 不读 #6)GIVEN #8 `GetHouseCount=3`、#6 mock 若访问 house_count 则抛断言;WHEN Open;THEN 展示 3,**spy 断言 #6 house_count 访问==0**(防 6↔8 环)。
- **AC-7** [Logic](CR-4 Railroad)GIVEN #6 `GetStationCount=3`、#1 `RentTable[0..3]`;WHEN Open;THEN 绑 4 行,`highlight_row==2`(F-1(b)),无建房渲染(spy==0)。
- **AC-8** [Logic](CR-5 Utility 倍率 + 禁固定现金)GIVEN #6 `GetUtilityCount=2`、#1 `DiceMultiplierTable=[4,10]`;WHEN Open;THEN `highlight_multiplier==1`(×10);spy 断言无固定现金金额绑定 + "实租=骰点×倍率"文案非空(EC-10)。
- **AC-9** [Advisory](CR-3 色组色条视觉)截图验色条色值=§4.4、高 ≈22%、饱和 ≥70%。证据 `propcard-color-band.png`。
- **AC-10** [Advisory](CR-5 Utility 文案)截图验"实租=骰点×倍率"≥10px、无固定现金行。证据 `propcard-utility-wording.png`。

### C. F-1 生效租金行索引(关键同构守卫)
- **AC-11** [Logic](F-1(a) Property 表驱动 8 边界)GIVEN 纯函数 `F1_Property(is_mortgaged,is_monopoly,house_count)→highlight_row`;WHEN 8 变体:

  | # | mortgaged | monopoly | house | →row | →badge |
  |---|---|---|---|---|---|
  | 1 | t | any | any | -1 | false |
  | 2 | f | f | 0 | 0 | false |
  | 3 | f | f | 1 | 1 | false |
  | 4 | f | f | 5 | 5 | false |
  | 5 | f | t | 0 | 0 | **true** |
  | 6 | f | t | 1 | 1 | **false**(关键防发散) |
  | 7 | f | t | 4 | 4 | false |
  | 8 | f | t | 5 | 5 | false |

  THEN 8 组全通;**#6(垄断+1栋=无 badge)是防发散核心**(漏 `house_count==0` 检查→垄断+1栋误显 ×2 而引擎实收 RentTable[1])。此表为 **[Logic]**——验 `F1_Property` 纯函数**自身逻辑一致性**(确定性、8 边界全覆盖)。
  - **AC-11g G-0 越界例(R-2 新增,守 clamp 不静默吸收)** [Logic] 追加两越界变体:① `house_count=6` ② `house_count=-1`(均 `¬mortgaged,¬monopoly`);THEN **均不返回 clamp 结果(5 / 0)**,而是走 G-0a → EC-2 降级路径(`highlight_row=-1` + 占位),**断言 `highlight_row≠clamp(6,0,5)=5` 且 `≠clamp(-1,0,5)=0`**——验越界拦截先于 clamp(防只读公式符号漏 G-0 的实现把越界静默压回)。
  - **⚠ 同构性降级为 [Advisory·cross-system] + propagate 债**:本 8 组期望值是否真与经济5 `property_rent` piecewise 同构,**不能靠测试注释引用行号**保证(经济5 改 piecewise 时本表照绿=drift,伪机器约束)。**机器侧只验纯函数自洽**;**同构性靠 propagate 债**:经济5 任何 `property_rent`/`railroad_rent`/`utility_rent` piecewise 变更,**必须触发重核本节 F-1 + AC-11/12/13 期望表**(登记于 Open Questions + economy-cash 的 depended-on-by)。可选强化:架构期若经济5 暴露可调用租金纯函数,升级为 [Integration] 运行时同入参交叉比对(F-1 输出==经济5 输出),届时同构性回归 BLOCKING。
- **AC-12** [Logic](F-1(b) Railroad)五变体:`{f,0}→-1`/`{t,3}→-1`/`{f,1}→0`/`{f,3}→2`/`{f,4}→3`;-1 哨兵不被 clamp 为 0。
- **AC-13** [Logic](F-1(c) Utility)四变体:`{f,0}→-1`/`{t,1}→-1`/`{f,1}→0`/`{f,2}→1`;output 为索引非现金。
- **AC-14** [Logic](F-1 -1 哨兵防越界,三路全覆盖)三变体:① Property `is_mortgaged=true`;② Railroad `station_count==0`(`is_mortgaged=false`);③ Utility `utility_count==0`(`is_mortgaged=false`);WHEN highlight 选择;THEN 三路 `-1` 输出均**不访问任何 `RentTable[x]`/`DiceMultiplierTable[x]`**(spy 下标访问==0,防 -1 当索引越界);**Railroad/Utility 的哨兵路径与 Property 同等守护**(防 `clamp(-1,...)` 被误算或 `-1` 被当合法下标)。
- **AC-15** [Advisory](F-1 ×2 徽章视觉)截图:monopoly∧house=0 显「×2」徽章,house=1 无。证据 `propcard-monopoly-badge.png`。

### D. 动作按钮 Affordance + 负向 Spy(CR-7, CR-8)
- **AC-16** [Logic](CR-7 意图转发单次)GIVEN 当前玩家==owner、合法建房、DI 注入 #6/#8 + ResolvePhase mock;WHEN 点"建房";THEN 向 ResolvePhase 转发"建房意图"恰一次、不崩溃。
- **AC-16b** [Advisory·code-review](CR-7 负向:#17 不持 #6/#8 写引用)**R-2 由 AC-16 黑名单 spy 拆出降级**:原"spy 黑名单 `#6.SetOwner`/`#8.AddHouse`/任何 `Set/Write/Modify/Apply` 前缀==0"在 #17 架构上根本不持 #6/#8 写接口引用时 **vacuous-true**(空 mock 天生返 0,同 R-1 AC-5/39 逻辑)。改 reviewer 验 #17 源码/BP **无任何 #6/#8 写方法引用/节点**(无 `SetOwner`/`AddHouse`/`SetHouseCount` 等)。证据 `propcard-no-write-coupling-review.md`。C++ 强封装(只持 const 引用/只读接口)可升 [Logic]。
- **AC-17** [Logic](CR-8 抵押按钮门 GetHouseCount==0)GIVEN ① `owner==我∧¬mortgaged∧GetHouseCount==0` ② `GetHouseCount==1`;WHEN 渲染 affordance;THEN ① 启用 ② 灰化;read 源=#8 `GetHouseCount`(spy #8 调用≥1)。
- **AC-18** [Logic](CR-8 赎回按钮门)GIVEN ① `owner==我∧mortgaged∧现金≥赎回价` ② `现金<赎回价`;THEN ① 启用 ② 禁用;现金读经济 mock。
- **AC-19** [Logic](CR-7 非 owner 禁用)GIVEN owner==P1、当前==P2;WHEN Open;THEN 动作按钮全禁用(spy 断言 P2 点击对 ResolvePhase 意图调用==0)。
- **AC-20** [Advisory](按钮视觉差异)截图启用(金底)vs 禁用(灰+半透)+ 图标/不透明度冗余(色盲)。证据 `propcard-button-affordance.png`。

### E. 订阅实时刷新(CR-9)
- **AC-21** [Logic·门控 OQ-HUD-2](CR-9 归属变更刷新不关卡)GIVEN 卡片 Open(TileIndex=5);WHEN `OnOwnershipChanged(5,P1,P2)`;THEN 保持 Open、owner 逻辑值→P2、handler ≤1 帧更新绑定值;TileIndex≠5 不触发卡片刷新(spy 分支)。
- **AC-22** [Logic·门控 OQ-HUD-2](CR-9 抵押变更刷新)GIVEN Open(7)、`¬mortgaged`;WHEN `OnMortgageChanged(7,true)`;THEN `is_mortgaged→true`、F-1 重算 `highlight_row==-1`(spy F-1 调用≥1)、保持 Open、抵押按钮变禁用。
- **AC-23** [Logic·门控 OQ-HUD-2](CR-9 建房变更刷新)GIVEN Open(3)、`house_count=1`;WHEN `OnBuildingChanged(3,2)`;THEN `house_count→2`、`highlight_row` 重算==2、保持 Open、图标更新 2 栋。
- **AC-24** [Logic](CR-9 TileIndex 不匹配不刷新卡片)GIVEN Open(5);WHEN `OnOwnershipChanged(9,...)`;THEN **断言卡片绑定值输出不变**(`GetDisplayedOwner(tile=5)` 等绑定 getter 返回值与事件前一致——验绑定值输出,**非 spy 一个不存在的"更新"方法调用计数==0**,避免 vacuous-true)、棋盘叠加 tile=9 更新(spy≥1,CR-10)、保持 Open。**注**:`GetDisplayedOwner` 等可 spy getter 的暴露归 OQ-PC-4(i)spy 输出接口(同门控前提)。

### F. 棋盘 tile 状态常驻叠加(CR-10 / HUD F-4 依赖通道)
- **AC-25** [Logic·门控 OQ-HUD-2](CR-10 叠加常驻独立于卡片开关)GIVEN 叠加订阅已建、卡片 Closed;WHEN `OnMortgageChanged(12,true)`;THEN tile=12 **叠加 cached 逻辑状态变量更新**(如 `bTileIsMortgaged=true`,**非 Slate 渲染输出**——`-nullrhi` 无渲染探针;视觉灰化+斜纹归 AC-27 Advisory 截图)、卡片仍 Closed(叠加更新不开卡)。
- **AC-26** [Logic·门控 OQ-HUD-2](CR-10 卡片关闭后叠加仍活跃)GIVEN 卡片 Open→Closed;WHEN 随后 `OnMortgageChanged(8,true)`/`OnOwnershipChanged(15,...)`;THEN 叠加仍更新(spy≥1)、不崩溃;**叠加 delegate 未被 CloseCard 解绑**(CloseCard 只释放卡片内容订阅)。
- **AC-27** [Advisory](CR-10 棋盘叠加视觉)截图抵押灰化+斜纹、垄断组高亮;色盲下纹理+高亮非仅色。证据 `propcard-board-overlay.png`。

### G. 状态机单卡不变式
- **AC-28** [Logic](单卡不变式)GIVEN Open(5);WHEN `OpenCard(12)`;THEN 切新内容、旧卡逻辑关闭、**任何帧不存在两卡同 Open**(spy Open 计数最大==1)。
- **AC-29** [Logic](EC-7 同帧多 OpenCard 以最后为准)GIVEN Closed;WHEN 同帧 `OpenCard(3)`+`OpenCard(7)`;THEN 最终 TileIndex=7,3 不残留,不双卡闪烁。
- **AC-30** [Logic·门控 OQ-HUD-2](CloseCard 正常关闭 + 解绑)GIVEN Open;WHEN CloseCard(按钮/点外/Esc);THEN 转 Closed、卡片内容订阅解绑(spy 后续 `OnOwnershipChanged` 不再刷新卡片,仅棋盘叠加继续)、不崩溃。**门控 OQ-HUD-2**:解绑行为的 headless spy 依赖 handler 抽独立 UObject(否则 UMG handler 生命周期在 `-nullrhi` 不可观测)。

### H. 边界与降级(EC-2, EC-6, EC-8)
- **AC-31** [Logic](EC-2 某源失败降级)GIVEN #1 正常、#6 失败/null、#8 失败;WHEN Open;THEN 缺失字段占位"—"、其余正常、不崩溃、记告警(spy≥1)、状态机进 Open。
- **AC-32** [Logic](EC-6/G-0b 数据矛盾 fail-safe 不高亮)**两变体**:① GIVEN #8 `GetHouseCount=3` 但 #6 `owner==NONE`;THEN 字段原样呈现(显"无主"+3 栋图标)、**`highlight_row==-1`(断言不等于 clamp(3,0,5)=3)**;② GIVEN #6 `owner==NONE ∧ IsMonopoly=true ∧ GetHouseCount=0`(无主却垄断);THEN **`highlight_row==-1` 且 `show_monopoly_badge==false`(无主绝不显 ×2 徽章)**。两变体共同 THEN:显「数据异常」占位、不自修正(spy 无 #6/#8 写)、记告警(spy≥1)、**`RentTable[x]`/`DiceMultiplierTable[x]` 下标访问==0**(同 AC-14,防 -1 当下标 + 防渲染不存在收租档)。**关键断言:矛盾态下 F-1 输出 -1**(防误导玩家)。
- **AC-33** [Integration·provisional](EC-8 OQ-Prop-5 #17 单源依赖 #6.IsMonopoly)GIVEN 垄断组内一地抵押、#6 `IsMonopoly=true`(当前设计抵押不破垄断);WHEN Open;THEN 显垄断高亮 + 该地灰化、**#17 只读 #6.IsMonopoly 无自身垄断计算**(spy);注:OQ-Prop-5 翻转时 #6 返 false、#17 无需改逻辑(单源)。**provisional 标注,待 OQ-Prop-5 定论复核。**

### I. 存档读档重建(EC-9)
- **AC-34a** [Integration·门控 OQ-HUD-2·C++路径](EC-9 重绑 + 三源重读)GIVEN 存档:TileIndex=5 卡片 Open、`house_count=2`、`¬mortgaged`;WHEN 读档重建;THEN C++ 路径 `RemoveDynamic`→`AddDynamic` 重绑 + 从快照重读直接呈现最终态、不回放、棋盘叠加重建、重建后新事件正常响应;**spy 断言重建后单次 `OnBuildingChanged` 触发 handler 调用计数==1(非 2)**——经 delegate 触发计数验证去重,非检查绑定状态(`IsBound` 只返 bool 测不出双绑)。门控 OQ-HUD-2:handler 须抽独立 UObject 才可 headless 实例化驱动。
- **AC-34b** [Advisory·code-review·BP路径](EC-9 BP `Bind Event` 双绑 footgun)WHEN reviewer 审 BP 读档重建 graph;THEN 验 `Bind Event` 前**显式调用 `Unbind Event from`**(无 Unbind 即 S2 footgun,同 HUD AC-48);**诚实标注:BP `Bind Event` 是 graph 层操作,`-nullrhi` 下 BP 图不跑、C++ spy 看不见 BP 绑定层 → headless 自动化框架内结构性不可覆盖**,须 code-review + playtest 两重。证据 `propcard-bp-rebind-review.md`(截图 BP graph + 签字)。**注**:若 MVP 采用 CommonActivatableWidget 销毁重建(非 widget 复用),读档走「新建 widget→NativeConstruct 绑定」路径、天然不双绑,此时 AC-34b 验证点改为「NativeConstruct/NativeDestruct 绑定生命周期对称」——架构 ADR(OQ-PC-4)定 widget 生命周期模型后据此精化。

### J. CR-12 juice RNG 隔离
- **AC-35** [Advisory·code-review](CR-12 juice RNG 独立流,同 HUD AC-47)GIVEN 实现提交;WHEN reviewer 静态扫描 #17 juice 随机路径;THEN 所有随机源自 #17 自建独立 `FRandomStream`,无对骰子3 权威流引用/别名(防重放污染,ai CR-5④);复用即 S2。证据 `propcard-rng-isolation-review.md`(签字+commit)。BP 无语言级拦截=code-review 软约束;C++ 强封装可升 [Logic]。

### K. 动画与视觉(Advisory)
- **AC-36** [Advisory](卡片 pop-in 时长)录帧验 `T_card_open∈[200,300ms]`(§7.5);`reduce_motion=On` 瞬现(≤50ms)。证据 `propcard-open-animation.png`。
- **AC-37** [Advisory](EC-5 抵押灰化 + 租金表灰显仍可读)截图整卡灰化+斜纹+「已抵押」标签+ 6 行灰显仍可读。证据 `propcard-mortgaged.png`。
- **AC-38** [Advisory](owner 徽章 + 垄断高亮)截图 P2 蓝徽章+编号可辨(色盲冗余)、monopoly 组高亮明显。证据 `propcard-owner-badge.png`。

### L. 额外完整性
- **AC-39** [Advisory·code-review](CR-2 deferred 路径:机会/命运不经 #17)GIVEN `TileType=Chance/CommunityChest`;WHEN `OpenCard`;THEN 不开卡([Logic] 同 AC-3 验 `GetCardState()==Closed`)。**「不订阅 #7」改 code-review 守**——MVP 阶段 #7 `OnCardDrawn`/`OnTileEventResolved`/`FCardData` 可能尚不存在,spy 订阅==0 永真(vacuous);故 reviewer 验 #17 源码/BP 无 #7 订阅引用(防 Alpha 接口提前误接)。证据 `propcard-no-event7-coupling-review.md`。
- **AC-40** [Advisory](CR-11 禁 hover-only)GIVEN 鼠标不悬停/手柄导航;WHEN Open;THEN 关闭/动作按钮组/启用禁用态非 hover 即可读可达(利手柄)。
- **AC-41** [Advisory](EC-9 视觉重建一致性)读档后截图对比存档前:house 图标/抵押灰化/owner 徽章正确重现、无残影。证据 `propcard-save-reload.png`。

### M. 赎回价 / 卖回额 / 门控原因(building-upgrade L265-266 + EC-13/14 兑现)
- **AC-42** [Logic·门控 OQ-PC-8](EC-13 赎回价显示=owner 提供值,非抵押价)GIVEN #6/owner `MortgageValue=100`、owner `GetUnmortgageCost(tile)=110`、is_mortgaged=true;WHEN Open;THEN 抵押区块**同时**绑定抵押套现额==100 **与** 赎回价==110(两值不同源、不相等);**spy 断言赎回价绑定值取自 owner `GetUnmortgageCost` 返回值,#17 未自行执行 `ceil` 运算**(防口径分歧 + 防只显 MortgageValue 误导)。**关键:赎回价显示值 ≠ MortgageValue**(MV=100 时显 110)。**⚠ 门控 OQ-PC-8(R-2 主审 fresh-grep 实证)**:`GetUnmortgageCost(tile)` 读接口在 economy-cash(仅 Credit/Debit/GetCash)/property-ownership(接口清单无)/player-turn(零命中)**当前均不存在**——本 fixture 的 `owner GetUnmortgageCost=110` 是尚不存在的接口。**OQ-PC-8 闭合(owner 暴露纯函数 `GetUnmortgageCostForDisplay(MV)→int32` 或等价读接口)前,本条按 BLOCKED 处理,禁入 CI BLOCKING gate**(否则 mock 自洽通过=false-green;闭合后升真实 [Integration] 交叉验证)。
- **AC-43** [Logic](EC-14 卖回额 + 建后租金预览)GIVEN #8 `GetSellback=50`(BuildingCost=100)、`house_count=2`、#1 `RentTable=[2,10,30,90,160,250]`;WHEN Open;THEN 建房区块绑卖回额==50(取自 #8,#17 不自算 floor)+ 建后租金预览==`RentTable[3]`==90(=`RentTable[clamp(2+1,0,5)]`);house_count==5 变体:建后租金预览隐藏不报错。
- **AC-44** [Logic](CR-7 门控原因 tooltip)GIVEN #8 `CanBuildHouse=false` 且原因码=`组内有抵押`;WHEN Open 且建房按钮禁用;THEN tooltip 绑定值==该原因码(取自 #8 返回,#17 不自判);四原因码(非垄断/组内有抵押/非最低档/现金不足)各一变体均正确映射。负向:`CanBuildHouse=true` 时无禁用 tooltip。

---

### BLOCKING / Advisory 汇总(R-2 revision:AC-11g 新增、AC-16 拆 16/16b、AC-42 升 OQ-PC-8 门控)
- **[Logic] BLOCKING**:AC-1,2,3,4,6,7,8,11,11g,12,13,14,16,17,18,19,24,28,29,31,32,43,44 +(门控 OQ-HUD-2)21,22,23,25,26,30 +(门控 OQ-PC-8)42。
  - **其中 ADR 门控(OQ-HUD-2)(6)**:AC-21/22/23/25/26/**30** 标 `[Logic·门控 OQ-HUD-2]`——headless 前提为状态机/事件 handler 抽独立 UObject + IGameClock DI **+ ADR 须额外定 (i) spy 输出接口 (ii) 同步/异步刷新机制 (iii) 测试实例化协议**(仅"抽 UObject"不足以 headless,UMG `NativeTick` 在 `-nullrhi` 不触发→false-green;见 OQ-PC-4)。**⚠ ADR 产出前这 6 条不进 CI BLOCKING gate**(防门控误判解除后裸 headless 跑出 false-green)。
  - **其中接口门控(OQ-PC-8)(1)**:AC-42 标 `[Logic·门控 OQ-PC-8]`——owner 暴露 `GetUnmortgageCost`/`GetUnmortgageCostForDisplay` 前按 BLOCKED,不进 CI gate(R-2 fresh-grep 实证对端真空,详见 AC-42)。
  - **R-2 vacuous-spy 修订**:AC-1 删末段"写接口==0"(vacuous);AC-16 拆 **16[Logic 正向意图转发]/16b[Advisory·code-review 负向无写引用]**(同 R-1 AC-5/39);AC-24 改"断言绑定值输出不变"(非 spy 不存在的更新方法)。
- **[Integration] BLOCKING(2)**:AC-33(OQ-Prop-5 单源,provisional)/ AC-34a(读档重建 C++ 路径,门控 OQ-HUD-2)。
- **[Advisory]**:AC-5(code-review)、9,10,15,**16b(code-review·负向无写引用,R-2 拆出)**、20,27,34b(code-review·BP路径)、35(code-review)、36,37,38,39(code-review)、40,41。
  - **R-1 类型订正**:AC-5/AC-39 由 [Logic] 降 [Advisory·code-review](spy #5/#7==0 在架构无引用时 vacuous-true,改 reviewer 验源码无耦合);AC-11 同构性由 [Logic] 降 [Advisory·cross-system]+propagate(纯函数自洽仍 [Logic]);AC-34 拆 34a[Integration·门控]/34b[Advisory·BP code-review]。
- **诚实说明**:渲染时刻/像素/外观(AC-9/10/15/20/27/36/37/38/41)全 [Advisory];BP graph 层(AC-34b)+ 源码耦合(AC-5/35/39)= code-review;[Logic] 项均为状态机/F-1/payload/spy 可 headless(门控 6 项前提见上)。

### 覆盖矩阵
| 需求来源 | AC 号 | 类型 |
|---|---|---|
| CR-1 OpenCard handoff | AC-1 | Logic |
| CR-2 类型路由 | AC-2,3 / AC-39(Adv·cr) | Logic/Adv |
| CR-3 Property 内容/视觉 | AC-5(Adv·cr),6,42,43 / AC-9(Adv) | Logic/Adv |
| CR-4 Railroad | AC-7 | Logic |
| CR-5 Utility + 禁固定现金 | AC-8 / AC-10(Adv) | Logic/Adv |
| CR-6 house_count 读 #8 不读 #6 | AC-6 | Logic(spy 关键) |
| CR-7 意图转发 + 门控原因 tooltip | AC-16,19,44 | Logic |
| CR-8 抵押/赎回按钮门 + 赎回价 | AC-17,18,42 | Logic |
| CR-9 订阅刷新 | AC-21,22,23,24 | Logic·门控 |
| CR-10 棋盘叠加(HUD F-4 通道) | AC-25,26 / AC-27(Adv) | Logic·门控/Adv |
| CR-11 输入/关闭/禁 hover | AC-28,29,30,40 | Logic/Adv |
| CR-12 juice RNG 隔离 | AC-35 | Adv·code-review |
| F-1(a) Property 8 边界 + 防发散 | AC-11(纯函数 Logic;同构性 Adv·cross-system) | Logic/Adv |
| F-1(b)/(c) | AC-12,13 | Logic |
| F-1 -1 哨兵防越界(三路) | AC-14 | Logic |
| F-1 ×2 徽章视觉 | AC-15 | Adv |
| EC-2 降级 | AC-31 | Logic |
| EC-3 越界 | AC-4 | Logic |
| EC-5 抵押灰化 | AC-37 | Adv |
| EC-6 数据矛盾 fail-safe | AC-32 | Logic |
| EC-7 快速 re-open 单卡 | AC-29 | Logic |
| EC-8 OQ-Prop-5 单源 | AC-33 | Integration·provisional |
| EC-9 读档重建(C++/BP 拆) | AC-34a,41 / AC-34b(Adv·cr) | Integration/Adv |
| EC-10 Utility 误读 | AC-8 | Logic |
| EC-13 赎回价≠抵押价 | AC-42 | Logic |
| EC-14 卖回额+建后租金预览 | AC-43 | Logic |
| building-upgrade L265-266 义务 | AC-42,43,44 | Logic |
| 状态机单卡/叠加独立 | AC-25,26,28,30 | Logic |

### 覆盖缺口(诚实标注)
1. **CR-3/4/5 视觉外观**(色条高 22%/饱和/斜纹不透明度):像素级须录帧,`-nullrhi` 无输出 → [Advisory] 截图 + art-director 签核。
2. **CR-9/10 刷新渲染帧时延**:渲染侧时序同 HUD AC-12b,仅测逻辑状态机(AC-21~26),渲染时延 [Advisory]。
3. **EC-8 OQ-Prop-5**:#6.IsMonopoly 实现待定;AC-33 按"抵押不破垄断"provisional 写,翻转时 AC 无需改(单源)。
4. **CR-2 deferred 机会/命运翻牌**:`FCardData` 未定(OQ-PC-1),无法写 AC;AC-39 仅验不误订阅。Alpha 补。
5. **CR-8 affordance vs 权威门端到端一致性**:#17 affordance 是尽力呈现,权威门在 ResolvePhase;AC-17/18 验 affordance 逻辑,端到端拒绝场景须 Integration playtest,非 #17 单元可测(已知缺口)。
6. **BP graph 层绑定(AC-34b)结构性不可自动覆盖**:BP `Bind Event` 是 graph 层操作,`-nullrhi` 下 BP 图不跑、C++ spy 看不见 BP 绑定层 → BP 双绑 footgun **headless 自动化框架内无法覆盖**,仅 code-review + playtest 两重(C++ 路径 AC-34a 可 headless)。
7. **门控 6 AC 的 headless 真前提待 ADR**(AC-21/22/23/25/26/30):仅"抽 UObject"不足,须 OQ-PC-4 ADR 定 spy 输出接口/刷新同步性/实例化协议/叠加 owner;ADR 前勿按裸 headless 写(false-green)。
8. **F-1 同构性非机器守**:AC-11 纯函数自洽 [Logic],但与 economy5 piecewise 同构靠 propagate 债(OQ-PC-11)非运行时断言;economy5 改租金公式时须人工重核。

## Open Questions

| ID | 问题 | Owner | 目标解决期 |
|----|------|-------|-----------|
| **OQ-PC-1** | 机会/命运**翻牌卡 UI** + `FCardData`(16 效果/deck 内容)未在任何 approved GDD 定义;MVP deferred,Alpha 落 | #1/#7 owner / ux-designer | Alpha(`/design-system` 翻牌卡时) |
| **OQ-PC-2** | HUD↔#17 **handoff 接口精确机制**(快览入口按钮如何触发 `OpenCard` + 传 TileIndex:event/delegate/直调) | technical-director / ue-umg | 架构期 ADR(随 OQ-HUD-2) |
| **OQ-PC-3** | art-bible §4.4 **Utility 色组色**未正式分组(V-Own-03 fallback 天空蓝 `#3AACDB`) | art-director | art-bible 更新 |
| **OQ-PC-4** | widget 数据绑定形式(`BindWidget` vs MVVM)+ 卡片/叠加 handler 抽独立 UObject——**与 HUD OQ-HUD-2 共享**。**R-1 强化**:ADR 须额外输出 (i) **spy 输出接口**(AC-21 等"绑定值"须暴露可 spy 的 getter/虚函数,否则断言不到渲染层输出);(ii) **刷新同步 vs 异步**(delegate 触发即同步更新 cached 值 / 还是下一 tick);(iii) **测试实例化协议**(裸 `NewObject` vs 需 GameInstance/mock World);(iv) **CR-10 棋盘叠加 widget owner + 生命周期**(world-space `UWidgetComponent`/tile vs screen-space overlay 层——比"抽 UObject"更底层,AC-25/26 前提);(v) **MVVM 选型时的 spy 注入协议**(MVVM 经 `UMVVMSubsystem` 需 GameInstance,spy 注入点须随之定)。仅"抽 UObject"不足以 headless | technical-director / ue-umg | 架构期 ADR(随 OQ-HUD-2) |
| **OQ-PC-5** | **监狱状态卡/动作 UI 归属**(HUD 面板 vs #17 卡):待 player-turn(2)/事件7 暴露 `bIsInJail`/`JailTurnsServed` 字段;承接 HUD OQ-HUD-10 jail 部分 | game-designer / #2/#7 owner | Pre-Production |
| **OQ-PC-6 🔶(HUD 回标已落 / art-bible 部分待落)** | **垄断完成视觉(V-Own-07)propagate art-bible** §6.2/§7.4(b)〔art-director 待落〕+ **OQ-HUD-10 回标**〔✅ 已落〕。**R-2 fresh-grep 核实+回标**:hud.md F-4 L190 确实依赖 #17 CR-10 常驻通道把 AI `Mortgage`/`Unmortgage` 判非 critical(claim 属真);hud.md L694 OQ-HUD-10 已回标 **provisional-RESOLVED**(垄断/抵押归属由 #17 CR-10+V-Own-07/08 兑现,实现通道随同一架构 ADR OQ-HUD-2/OQ-PC-4;ADR 前 F-4 非-critical 裁定不视作硬闭合)。剩 art-bible §6.2/§7.4(b) 垄断完成视觉规格待 art-director propagate | art-director / producer | HUD 回标 ✅;art-bible 部分 art-bible 更新时 |
| **OQ-PC-7** | reduce-motion 与 HUD **共享同一布尔** + 卡片动画降幅参数 | ux-designer | Pre-Production `/ux-design`(随 OQ-HUD-9) |
| **OQ-Prop-5(依赖)** | 垄断×抵押交互(抵押是否破垄断)provisional——#17 据 #6 `IsMonopoly` 单源呈现,翻转无需改逻辑(EC-8/AC-33);**待 #6 OQ-Prop-5 定论** | property-ownership(6) owner | 待 #6 解出(Rento 行为核验) |
| **OQ-PC-8 ✅(R-2 已闭合)** | **赎回价读接口**:R-2 主审 fresh-grep 曾实证 `GetUnmortgageCost` 三档真空。**已 propagate 落地(2026-06-05 同 PR)+ fresh-grep 核验**:economy-cash 新增纯函数 `GetUnmortgageCostForDisplay(MV)→int32`(L65 CR-5 + L122 接口稳定承诺 + L380 UI 节,无副作用/F-6 单源/不破无环)。AC-42 `GetUnmortgageCost` 改读此接口。CR-3⑤/CR-8/V-Own-01/EC-13 有源 | economy(5) owner / producer | ✅ 已闭合(R-2 propagate + fresh-grep) |
| **OQ-PC-9 ✅(R-2 已核闭合)** | building-upgrade L265-266 义务回标:**R-2 fresh-grep 确认对端已闭合**——building-upgrade L265 含「卖回额 floor(BuildingCost/2)+门控四原因码」、L78/222/269 已列 #17 为消费方。AC-43/44 真兑现 | producer | ✅ 已闭合(R-2 核实) |
| **OQ-PC-10 ✅(R-2 已核闭合)** | FBoardTileData.RentTable 基数契约:**R-2 fresh-grep 确认对端已闭合**——board-data L70/89/370 已定变长 `TArray<int32>`(Property6/Railroad4/Utility 不用)、L165/305/316 已列 #17。systems-index depends-on 已订正 1,6,8 | board-data(1) owner / producer | ✅ 已闭合(R-2 核实) |
| **OQ-PC-11 ✅(R-2 已闭合)** | **F-1 同构性维护债**:economy5 改 `property_rent`/`railroad_rent`/`utility_rent` piecewise 须触发重核 #17 F-1 + AC-11/12/13(AC-11 同构性已降 Advisory,靠此债守)。**已 propagate 落地 + fresh-grep**:economy-cash Interactions(L112)+ 下游表(L330)已登记 depended-on-by #17 + 同构维护债,economy 改租金 piecewise 须触发 #17 重核 | economy(5) owner / producer | ✅ 已闭合(R-2 propagate + fresh-grep) |
| **OQ-PC-12(R-1 → 集中 a11y CONCERN)** | **10px 字号下限 + 无 text-scaling**:art-bible §7.2 继承的 10px 低于游戏 a11y 通行 14–16px;#16/#17/所有 §7.2 继承者共担。**CD 裁:跨档 art-bible/a11y CONCERN,非 #17 单档 blocker**(与已 APPROVED 的 #16 保持一致裁决);Pre-Production `/ux-design` 统一裁 text-scale 选项 | art-director / ux-designer | Pre-Production `/ux-design` |
