# 棋盘数据 (Board/Tile Data)

> **Status**: Revised R3 (design-review 第三轮 fresh re-review:5 项 BLOCKING 收口 + 低成本 RECOMMENDED 应用 2026-06-01)
> **Author**: msc + agents
> **Last Updated**: 2026-06-01
> **Implements Pillar**: ①桌游忠实、④易上手轻松局
> **System**: #1 in systems-index design order | Foundation | Core | MVP
> **Review Mode**: full（systems-designer / game-designer / economy-designer / qa-lead / unreal-specialist + creative-director 综合裁定）
> **Creative Director Review (CD-GDD-ALIGN)**: 第二轮 NEEDS REVISION → 已修订（9 项 GDD 本体 BLOCKING + 2 项归 OQ-6 ADR + 1 项概念 carry 已应用；布局/公式拓扑两轮验证扎实，未重做。本轮性质=修内部契约自相矛盾，非精修接口）
> **R2 用户决策**：发薪由移动(4)读 SalaryAmount 传经济(5)；F-2 恒用通用式（对 from 越界鲁棒自修正）；AdvanceIndex 原子单调用返回 (newIndex, passedGo) 元组

<!-- 骨架由 /design-system 创建。逐节设计：新会话运行 `/design-system 棋盘数据` 自动从下一个未完成节续做。 -->
<!-- 关键约束（Phase 2 上下文，供续做参考）：
     - 被依赖：移动(4)/所有权(6)/事件格(7)/存档(21)/地图编辑器(26)/HUD(16) —— 接口要稳
     - art bible §8.2：棋盘配置用 Data Asset（DA_ 前缀）驱动
     - 经典大富翁布局：8 色组、四角大格（起点/监狱/停车场/前往监狱）
     - 编码标准：数值数据驱动、不硬编码
     - 引擎：UE5.7，DataTable/Data Asset 驱动，Blueprint 可读
     - 这是 data/infrastructure 系统（玩家不直接交互）
-->

## Overview

棋盘数据系统是《骰子大亨》的空间地基:它以纯数据形式定义一张棋盘"是什么"——格子的数量、环形排列顺序、每格的类型(地产/车站/公用/机会/命运/税/角格)及其静态属性(名称、所属色组、地价、租金表、抵押价值等)。它不包含任何运行时行为或玩家交互逻辑;它只提供一张被其他系统读取的**只读权威数据表**。移动系统按它的格子顺序推进棋子,地产所有权系统按它的地价/色组结算买卖,事件格系统按它的格子类型触发机会命运,存档系统引用它的格子索引。整张棋盘通过 Unreal 的 `DA_` Data Asset 驱动,数值全部外置、零硬编码,使得"经典 40 格标准盘"只是该 schema 的第一份实例数据——同一套结构未来可直接承载地图编辑器产出的自定义棋盘。本系统的设计重心是**一份稳定、可序列化、易读的格子数据契约**,因为有 7 个上层系统将依赖它且尚未成形,接口的稳定性优先于一切。

## Player Fantasy

棋盘数据本身是玩家**永远不会直接察觉**的系统——没有人会想"这是一份格子数据表"。它服务的是一种**间接的、奠基性的体验**:当玩家第一眼看到棋盘,立刻认出"这就是大富翁"——熟悉的方形环路、四角的出发/监狱、沿边排布的彩色地产组——从而**零学习成本地坐下就玩**(支柱④易上手)。每一格的位置、颜色、地价都恰好落在桌游老玩家的预期之中,带来一种"对,就该是这样"的踏实感(支柱①桌游忠实)。

玩家真正"感受"到这个系统的时刻,是它**悄无声息地正确**的时刻:棋子总是停在该停的格子、红色组永远是那三块、抵押价值永远算得对。它的成功标准是隐形——玩家从不为棋盘的布局或数据困惑,所有注意力都能投向真正有趣的博弈(买地、收租、互坑)。换言之,这个系统的"幻想"是它把舞台搭得如此自然,以至于没人会注意到舞台的存在。

> **支柱①落地路径备注(R2 CONCERN-carry,game-designer 提,CD 裁定回流 concept):** 支柱①"立刻认出这就是大富翁"的**主要认知触发器是地块名称与视觉外观**(由 OQ-3 命名策略 + art bible §6 承载),而非棋盘数据的布局序号本身——尤其 OQ-3 将采用 IP 安全自创名,辨识感更依赖视觉色组与名称。**故本系统的 40 格布局是支柱①的"必要非充分条件":它提供正确的空间骨架,但"认出感"由命名(OQ-3)与视觉(art bible)共同完成。** 此风险显式回流 concept/art-bible,不由棋盘数据 schema 独立承担。**可检验接管前提(R3,game-designer):此"回流"的认领前提是 ① OQ-3 命名策略已确认;② art bible §6 色组色键已填入 `DA_` 色键表(CR-5.1)。任一前提未满足时,支柱①认出感风险处于无归属状态——"已回流"才不是无法核验的空声明。** 注:本系统能独立保障的那部分(空间骨架正确性、色组归属契约、四角拓扑)由 CR-5.1/CR-6/AC-6/AC-7 守住,这是 art bible 再漂亮的颜色也无法补救的底层正确性。

## Detailed Design

### Core Rules

**CR-1 棋盘是闭合环路。** 一张棋盘由 `N` 个格子组成,索引 `0 … N-1`,首尾相接成环。经典标准盘 `N = 40`,索引 0 为出发格(GO),顺时针递增。从索引 `N-1` 前进一格回到索引 `0`,即"经过出发格"。

**CR-2 格子类型(`ETileType`)封闭枚举,共 9 类功能(10 个枚举项):**

| 枚举值 | 含义 | 可购买 | 触发事件 |
|---|---|---|---|
| `Property` | 地产格(归属某色组) | 是 | 落地结算租金/购买 |
| `Railroad` | 车站 | 是 | 租金随持有车站数缩放 |
| `Utility` | 公用事业 | 是 | 租金 = 骰点 × 倍率 |
| `Chance` | 机会格 | 否 | 抽机会牌堆 |
| `CommunityChest` | 命运/社区宝箱格 | 否 | 抽命运牌堆 |
| `Tax` | 税格 | 否 | 扣固定税额 |
| `Go` | 出发角格(索引 0) | 否 | 经过/停留发薪(金额存 `SalaryAmount` 字段,见 CR-3) |
| `Jail` | 监狱/探视角格 | 否 | 探视(无害)或服刑 |
| `FreeParking` | 免费停车角格 | 否 | 无操作(可挂 House Rule) |
| `GoToJail` | 前往监狱角格 | 否 | 强制移动到 Jail |

*(四角中 Jail 与 GoToJail 行为不同,故各占一个枚举项;功能类别为 9 类。)*

> **发薪权威契约(防双重发薪,CD 裁定):** 经过/停留 GO 的薪水**唯一**由 F-2 的 `passed_go` 计数授权发放。Go 格上的 `SpecialAction=CollectSalary` 仅是**语义/UI 标记**(用于呈现"这格会发薪"),**不是第二个发薪触发器**。停在 GO 时 `passed_go=1`(见 F-2、AC-33),发薪一次;经济系统(5)/事件格系统(7)**不得**因落地在 Go 格而再次发薪。此契约消除停在 GO 的双重发薪 bug。
>
> **发薪读取路径契约(R2 用户决策,消除三处信号矛盾):** **移动系统(4)** 调 `AdvanceIndex` 得到 `passed_go`,同时从 `GetTileData(0).SalaryAmount` 读出薪额,将 `(passed_go, SalaryAmount)` 一并传给**经济系统(5)**结算发薪。**事件格系统(7)不读 `SalaryAmount`、不参与发薪**——发薪是"经过 GO"触发(移动的语义),不是"落地某格"触发(事件格的语义)。Interactions 表据此:移动(4)读 `SalaryAmount`,经济(5)消费 `(passed_go, SalaryAmount)`。**消费契约:`passed_go ≤ 0` 一律不发薪(无论绝对值,逆向穿越/原地皆不发);仅 `passed_go > 0` 发 `passed_go × SalaryAmount`。**

**CR-3 每格用统一的扁平结构 `FBoardTileData` 描述**(决策一:扁平 struct,DataTable 友好)。非本类型适用的字段取默认值(`0` / `None` / 空数组)。**棋盘级元数据(非每格)另存于 DA 资产顶层:`BoardIdentifier: FName`(稳定存档标识,见 Interactions)、`EColorGroup→hex` 色键表(见 CR-5.1)、`bIsPlaceholderData: bool`(temp-fill 硬门控标记,见 CR-6)。** 每格字段:

| 字段 | 类型 | 适用类型 | 说明 |
|---|---|---|---|
| `TileIndex` | int32 | 全部 | 环上位置,0..N-1,必须连续唯一 |
| `TileType` | ETileType | 全部 | 见 CR-2 |
| `DisplayName` | FText | 全部 | 本地化、IP 安全的自定义名 |
| `ColorGroup` | EColorGroup | Property | 8 色组之一;非地产为 `None` |
| `PurchasePrice` | int32 | Property/Railroad/Utility | 购买价;不可购买为 0 |
| `RentTable` | TArray\<int32\> | Property/Railroad | **现金租金**数组,见 CR-4。Utility 不用此字段 |
| `DiceMultiplierTable` | TArray\<int32\> | Utility | **骰点倍率**数组(长度 2,如 `[4,10]`),见 CR-4。与 `RentTable` 单位不同,故独立字段,杜绝静默单位混用 |
| `BuildingCost` | int32 | Property | 每栋房屋造价;酒店见 CR-5 |
| `MortgageValue` | int32 | 可购买格 | 抵押可得现金 |
| `TaxAmount` | int32 | Tax | 固定税额 |
| `SalaryAmount` | int32 | Go | 经过/停留 GO 发薪额;与 `TaxAmount` 对称,棋盘拥有金额(支持自定义棋盘不同薪水)。**由移动(4)读 `GetTileData(0).SalaryAmount` 连同 `passed_go` 传经济(5)发薪(见 CR-2 读取路径契约);事件格(7)不读**。非 Go 格此字段必须为 0(反向校验,见 Edge Cases) |
| `EventDeck` | EEventDeck | Chance/CommunityChest | 指向哪个牌堆;否则 `None` |
| `SpecialAction` | ETileAction | 角格 | 见下方 `ETileAction` 封闭枚举 |

**`ETileAction` 封闭枚举(穷举,无"等"):**

| 枚举值 | 用于 | 含义 |
|---|---|---|
| `None` | 默认 / FreeParking / Jail | 棋盘数据不附带特殊行为(`FreeParking`、`Jail` 默认即此值——探视/服刑是玩家运行时状态,不是棋盘 action) |
| `CollectSalary` | Go | 语义/UI 标记:此格发薪(金额读 `SalaryAmount`)。**注:发薪权威是 F-2 的 `passed_go`,此标记不触发第二次发薪,见 CR-2 契约** |
| `GoToJail` | GoToJail | 强制将玩家移动到本盘的 `Jail` 格 |
| `TriggerHouseRuleCheck` | FreeParking(预留) | 预留枚举项:House Rule(如奖池)开启时,事件格系统据此查询设置&House Rules(23)。MVP 不实现,但枚举位预留以稳定接口 |

**CR-4 租金/倍率数组的语义随类型而变**(决策二:棋盘数据拥有金额,但不拥有公式)。**关键:Property/Railroad 用 `RentTable`(承载货币);Utility 用 `DiceMultiplierTable`(承载倍率系数)。两者是不同字段,单位永不混用——任何读取方无需类型分支也不会把倍率当成货币。**
- **Property(`RentTable`)**:长度 6 数组 = `[无房, 1房, 2房, 3房, 4房, 酒店]` 的**未翻倍基础租金**。索引 0(无房)即"集齐同色组前的基础租金";DA 中**只存未翻倍值,绝不预存翻倍后的值**。
- **Railroad(`RentTable`)**:长度 4 数组 = 持有 `[1,2,3,4]` 个车站时的租金(货币)。**档位查找责任(R2;R-prop 拆分两腿):本系统只供 4 级 base 数组;"数玩家持有几个车站(`station_count`)"的运行时逻辑归地产所有权(6)(它拥有归属 map,能数持有数);"选 `RentTable` 哪一档(`RentTable[station_count−1]`)+ 结算金额"归经济(5) F-3(6 无 `RentTable` 访问权,只供 count,见所有权 CR-5/AC-39)。本系统不查档、不数持有数。** 此责任已在所有权(6)/经济(5) GDD 显式认领(所有权 CR-5 供 count、经济 F-3 查表),避免系统间协议空白。
- **Utility(`DiceMultiplierTable`)**:长度 2 数组 = 持有 `[1,2]` 个公用时的"骰点倍率"(如 `[4,10]`),实际租金 = 骰点 × 倍率,由经济公式在运行时计算。**此数组不是货币,是倍率系数。** **骰点 runtime input 来源(R2):正常落地用本次移动骰;机会牌"前进到最近公用"触发时按规则需额外掷骰——骰点由事件格(7)/移动(4)提供并传经济(5),来源歧义由事件格(7) GDD 裁定,本系统只供倍率系数。**

> **垄断翻倍归属契约(CD 裁定):** "集齐同色组后租金翻倍"的系数是**经济系统(5)拥有的全局常量**(经典固定 ×2,对所有色组一致),**已注册于 entities registry `monopoly_rent_multiplier`(R3:status=placeholder,source 临时归棋盘,经济(5)成形后迁移 source 并置 active——此前 CR-4 称"注册于 registry(source=经济#5)"是未来意图非当前事实,R3 已补建该条目)**。本系统的 `RentTable[0]` 只存**未翻倍 base**;翻倍由经济系统在运行时乘算。本系统既不存倍率、也不裁决翻倍,确保棋盘数据保持通用且不与经济公式耦合。

**CR-5 色组与建房约束是数据,不是行为。** 棋盘数据声明每格的 `ColorGroup` 和经典盘的色组构成(棕2/浅蓝3/粉3/橙3/红3/黄3/绿3/深蓝2)。"集齐整组才能建房""房屋数差不超过 1(均衡建房)""4 房换 1 酒店"这些规则由建房系统执行,但其判定所需的**组成员清单**由本系统提供查询接口(见 Interactions)。

**CR-5.1 色组色值契约(锁定,避免"粉/紫"歧义破坏桌游忠实)。** `EColorGroup` 的 8 个枚举值各携带一个**锁定的十六进制色键**,作为棋盘数据层的视觉契约(UI 不得自行决定色组颜色,只能读此键),色值来源为 art bible §6《环境设计语言》。枚举值与色键一一对应:

| EColorGroup | 经典对应 | 色键来源 |
|---|---|---|
| `Brown` | 棕 | art bible §6 |
| `LightBlue` | 浅蓝 | art bible §6 |
| `Magenta` | 粉/品红(锁定为 Magenta,消除 Pink/Purple 跨版本歧义) | art bible §6 |
| `Orange` | 橙 | art bible §6 |
| `Red` | 红 | art bible §6 |
| `Yellow` | 黄 | art bible §6 |
| `Green` | 绿 | art bible §6 |
| `DarkBlue` | 深蓝 | art bible §6 |

> 具体 hex 值由 art bible §6 拥有并填入 `DA_` 色键表;本 GDD 锁定的是**枚举命名与"色组颜色归属于棋盘数据契约层"这一事实**,而非允许 UI/HUD(16/17) 各自臆测颜色。

**CR-6 经典 40 格标准盘布局(首份实例 `DA_Board_Classic40`)** —— 索引→类型映射,与标准大富翁盘一致:

| Idx | 类型 | Idx | 类型 | Idx | 类型 | Idx | 类型 |
|---|---|---|---|---|---|---|---|
| 0 | Go(角) | 10 | Jail(角) | 20 | FreeParking(角) | 30 | GoToJail(角) |
| 1 | Property 棕1 | 11 | Property 粉1 | 21 | Property 红1 | 31 | Property 绿1 |
| 2 | CommunityChest | 12 | Utility 1 | 22 | Chance | 32 | Property 绿2 |
| 3 | Property 棕2 | 13 | Property 粉2 | 23 | Property 红2 | 33 | CommunityChest |
| 4 | Tax(所得税) | 14 | Property 粉3 | 24 | Property 红3 | 34 | Property 绿3 |
| 5 | Railroad 1 | 15 | Railroad 2 | 25 | Railroad 3 | 35 | Railroad 4 |
| 6 | Property 浅蓝1 | 16 | Property 橙1 | 26 | Property 黄1 | 36 | Chance |
| 7 | Chance | 17 | CommunityChest | 27 | Property 黄2 | 37 | Property 深蓝1 |
| 8 | Property 浅蓝2 | 18 | Property 橙2 | 28 | Utility 2 | 38 | Tax(奢侈税) |
| 9 | Property 浅蓝3 | 19 | Property 橙3 | 29 | Property 黄3 | 39 | Property 深蓝2 |

校验:22 地产 + 4 车站 + 2 公用 + 4 角 + 2 税 + 3 机会 + 3 命运 = **40**。本 GDD 只锁定结构与布局。

> **四边方位映射(R3 新增,game-designer;art bible 与视觉实现的可靠锚点):** 经典盘四角为索引 0/10/20/30,四条边以 GO(索引 0)为起点**顺时针**定义:索引 1–9 = 第一边、11–19 = 第二边、21–29 = 第三边、31–39 = 第四边。Railroad 均匀分布于四边中点(5/15/25/35,间隔 10);Utility 在 12/28(**刻意的非均匀**,符合标准大富翁盘,非可均匀化的自由度)。色组沿边顺时针由低价向高价递增(棕在 GO 侧起步、深蓝在末边收尾)。本说明只锁定**索引区间→棋盘边**的对应,供 art bible §6 棋盘绘制与地图编辑器(26)UI 旋转方向引用;不新增数据字段。

> **占位参考值 temp-fill(CD 裁定,解除 AC-4/AC-7 阻塞):** `DA_Board_Classic40` 的货币字段(`PurchasePrice`/`RentTable`/`DiceMultiplierTable`/`BuildingCost`/`MortgageValue`/`TaxAmount`/`SalaryAmount`)**现在即用公开参考数值 temp-fill**(纯数值不受版权保护)。**IP 边界口径(R2 对齐):地块名称/视觉外观(含整体 trade dress)的 IP 安全边界以 `game-concept.md` 与 art bible 为准,本系统不在此扩大或缩小该范围**——本系统只声明"数值用公开参考值占位、名称走 OQ-3 IP 安全自创"。这些值**显式标注为占位、归属经济(5)、待重新平衡**——它们是测试 fixture,非平衡决策。如此:① 棋盘数据系统在经济(5)成形前即可独立跑通一局、独立通过 AC-4/AC-7(否则 `PurchasePrice=0` 会被 AC-23 拒绝,GDD 自相矛盾);② 经济(5)产出真实曲线后覆盖,并回链平衡依据(编码标准)。
>
> **值源与 playtest 有效性(R3 新增,game-designer):** ① temp-fill 的"公开参考数值"取自**经典大富翁标准盘的公开数值体系**(地价/租金/税额/薪水),用作结构占位;本作目标是复刻 **Rento Fortune**,其经济数值与经典大富翁可能存在差异,真值以经济(5)的现金流模型为准。② **硬门控 `bIsPlaceholderData` 防的是"占位变真值",防不住"用占位值 playtest 得出误导性结论"——故显式声明:任何基于 `bIsPlaceholderData==true` 数据跑出的 playtest,其经济平衡/局长节奏/支柱③(运气×策略)相关结论不得作为设计有效性证据**(占位值非目标平衡值)。此限制传递给 producer:Alpha 门控前置条件须含"经济(5)真实数值已覆盖(`bIsPlaceholderData==false`)",而非仅"棋盘能跑通"。早期 playtest 只验证**结构/流程跑通**,不验证**平衡手感**。
>
> **temp-fill 硬门控(R2 新增,economy-designer + CD;防"占位变真值"):** 仅靠文字"标注为占位"不构成阻止路径。强制机制:① **DA 顶层元数据加 `bIsPlaceholderData: bool` 字段**,`DA_Board_Classic40` 的 temp-fill 版本置 `true`;经济(5)产出真实曲线、回链平衡依据后才置 `false`。② **OQ-1 的关闭条件(见 Open Questions)显式绑定经济(5) GDD**:经济(5) 必须有对应 AC 验证 `DA_Board_Classic40` 数值来自经济公式推导而非手填,且将 `bIsPlaceholderData` 置 `false`。③ **`bIsPlaceholderData==true` 的棋盘不得进入 Alpha 里程碑构建**(传递给 producer / gate-check 作里程碑门控)。这样占位状态机器可验证,经济(5)(bottleneck 高风险系统)不会因"反正能跑通"惯性而永不正式推导。

### States and Transitions

**本系统在运行时是不可变的(read-only)。** 它没有逐回合状态机。棋盘数据的生命周期是一条**资产生命周期**,而非游戏状态机:

| 状态 | 触发 | 说明 |
|---|---|---|
| `Authored` | 设计期 | 设计师/地图编辑器在 `DA_` 资产中填写格子数据 |
| `Loaded` | 关卡/对局初始化 | 引擎加载选定的棋盘 Data Asset 到内存 |
| `Validated` | 加载后一次性校验 | 跑完整性校验(见 Edge Cases),失败则拒绝进入对局 |
| `Active (read-only)` | 对局期间 | 所有系统只读访问;本系统不写任何字段 |
| `LoadFailed` | `Validated` 失败 | 校验失败(见 Edge Cases 拒绝类),DA 卸载、返回结构化错误码(`BoardTooSmall`/`Index0NotGo`/… 见 AC-18~23)给调用方(对局初始化流程/地图编辑器 26 UI),**不进入 Active**。错误展示与重试由调用方负责;本系统不进入半加载态。 |

> **热切换边界(R2,systems-designer + unreal-specialist + CD,与"运行时只读"对齐):** **棋盘热切换(DA 替换)不允许在 `Active` 对局中发生。** 地图编辑器(26)的预览/换盘在**独立预览模式**(非正式对局)中运行,经完整 `Loaded → Validated → Active` 重新初始化,不在对局进行中替换 DA——这避免了切换窗口期的悬空引用/读旧数据竞态,也保持本系统"对局期间只读不变"的核心假设。**运行时 DA 持有者(推荐 `UGameInstanceSubsystem`)的具体加载/释放时机、双缓冲与防 GC 协议属实现层,归 OQ-6 BLOCKING-ADR 定义**(见 Open Questions OQ-6)。

**关键边界:运行时可变状态不属于本系统。** 哪块地产被谁拥有、建了几栋房、是否抵押——这些都是**以 `TileIndex` 为键**的运行时状态,由地产所有权(6)/建房(8)系统各自持有。棋盘数据只提供"这一格静态上是什么",从不记录"现在它处于什么所有权状态"。这条分离是存档系统(21)能干净序列化的前提:存档只需存"用了哪张棋盘 DA + 各 TileIndex 的可变状态",无需复制静态布局。

### Interactions with Other Systems

本系统对所有上层暴露一组**只读查询接口**(契约稳定优先)。下表列出每个依赖方读取什么、谁拥有接口:

| 依赖方 | 读取的数据 | 查询接口(建议) | 谁拥有 |
|---|---|---|---|
| 移动(4) | `N`(格数)、环序、`SalaryAmount`(Go 格) | `GetTileCount()`、`AdvanceIndex(from, steps) → (newIndex, passedGo)`、`GetTileData(0).SalaryAmount` | 本系统拥有环算法接口 |
| 经济(5) | `(passed_go, SalaryAmount)`(由移动 4 传入,非直接查本系统)、`PurchasePrice`/`RentTable` 等金额 base | 经移动(4)转交 + `GetTileData(index)` | 本系统供金额 base,经济拥有发薪/租金公式与倍率 |
| 地产所有权(6) | `PurchasePrice`、`ColorGroup`、`MortgageValue`、`TileType` | `GetTileData(index)`、`GetTilesInGroup(group)` | 本系统供数据,所有权系统拥有运行时归属 map(6 读 `TileType` 计车站/公用持有数 F-2/F-3、读 `GetTilesInGroup` 判垄断 F-1) |
| 事件格(7) | `TileType`、`EventDeck`、`TaxAmount`、`SpecialAction` | `GetTileData(index)` | 本系统供数据,事件系统拥有牌堆与触发逻辑 |
| 建房(8) | `BuildingCost`、`RentTable`、`ColorGroup` 及**同组成员清单** | `GetTilesInGroup(group) → [indices]` | 本系统供数据与组查询,建房系统拥有建造规则 |
| 存档(21) | 棋盘 DA 的引用 ID(非全量布局) | `GetBoardId()` | 本系统供标识,存档系统拥有序列化 |
| HUD/地产卡 UI(16/17) | `DisplayName`、`ColorGroup` 颜色、`PurchasePrice`、`RentTable` | `GetTileData(index)` | 本系统供数据,UI 拥有呈现 |
| 地图编辑器(26) | 完整 schema(读写) | 产出符合 `FBoardTileData` 的新 `DA_` 实例 | 编辑器写,本系统定义 schema 与校验 |

**接口语义补充(经 qa-lead / unreal-specialist / systems-designer 校验补全):**
- `GetTilesInGroup(group)` **按 `TileIndex` 升序**返回成员索引;调用方(建房均衡建造、UI 列表)可依赖此顺序。**实现必须对收集结果显式 `Sort`,不得依赖 DataTable/数组录入顺序**(否则升序不成立,AC-30 失败)。
- `GetBoardId()` 返回 **DA 内显式 `BoardIdentifier: FName` 字段**(如 `"Classic40"`),由内容作者手写、**与资产路径/文件名/PrimaryAssetId 解耦**。**禁止**从资产路径或 `PrimaryAssetId` 派生——资产改名/移动会静默损坏所有旧存档。该字段跨会话/跨版本恒定,供存档系统(21)可靠引用棋盘而无需复制布局。见 AC-28。
- **`AdvanceIndex` 的原子性(R2 用户决策,绑定契约,非建议):** 本系统**只暴露单一接口** `AdvanceIndex(from, steps) → (new_index, passed_go)` 元组(内部一次性算 F-1+F-2)。**不存在独立的公开 `PassedGoCount` 接口**——F-2 是 `AdvanceIndex` 返回元组的 `passed_go` 分量的数学定义,仅为文档/单测命名,不是第二个可被分离调用的接口。此设计强制两值对同一 `(from, steps)` 互相一致,杜绝调用方分离查询导致的联网回放不一致。最终 C++/Blueprint 函数签名(struct 返回 vs out-param 维度)由 OQ-6 ADR 定型。见 AC-37a/AC-37b。**命名约定(R3):本 GDD 正文用 snake_case `new_index`/`passed_go` 作公式/单测的数学命名;最终 C++/Blueprint 字段名按项目 conventions 用 PascalCase(`NewIndex`/`PassedGo`),由 OQ-6 ADR 落定——两者指同一分量,正文混用不构成两个接口。**
- **Utility 读取方注意:** Utility 格的租金倍率读 `DiceMultiplierTable`(非 `RentTable`)。地产卡 UI(17) 呈现 Utility 时须按"骰点 × 倍率"展示,不可把倍率当货币。
- **运行时 DA 持有者:** 本 DA 的运行时强引用持有者(以 `UPROPERTY` 防 GC 回收)为 **[待 OQ-6 ADR 确定:推荐 `UGameInstanceSubsystem`]**;上层系统(4/6/7/8)通过该持有者获取 DA 引用,**不得**各自硬引用 DA 资产(避免 GC 不当回收或加载时机错误)。地图编辑器(26)的换盘**经完整 `Loaded→Validated→Active` 重新初始化、不在 Active 对局中热替换**(见 States and Transitions 热切换边界);其双缓冲/释放时机/防 GC 协议属实现层,归 OQ-6 ADR。

**接口稳定承诺**:`FBoardTileData` 字段一旦发布,只增不改语义(向后兼容);新增格子类型通过扩展 `ETileType` 枚举,不破坏既有字段。这是给 7 个尚未成形的上层系统的稳定保证。

## Formulas

棋盘数据系统拥有的运行时计算**仅限环路拓扑**——它们只需要 `N`(本系统唯一拥有的静态事实)。租金、抵押、过路费金额等公式均不属于本系统(归经济系统)。

> **设计契约(经 systems-designer 校验):** 本系统的公式一律**如实输出有符号原始值**,不做发薪/截断等语义裁决。"逆向经过 GO 是否发薪""多圈是否多次发薪"由**经济系统**消费 `passed_go` 时决定。此约定让棋盘数据保持通用,可承载后退牌、传送等变体而无需改动。

### F-1 环路位移 AdvanceIndex

`new_index = mod(from + steps, N)`

其中 `mod` 为**数学取余**(结果恒 ≥ 0,即 Python `%` 语义,非 C++ 对负数的 `%`)。

> **Blueprint 实现约束(强制,非建议;unreal-specialist):** UE Blueprint 的 `%`(Modulo)节点对负数是 **C 风格**(结果与被除数同号),会让后退移动落点为负而出错。**必须**用组合式 `((A % N) + N) % N`,**禁止**直接用单个 `%` 节点。否则 AC-10/AC-14 必失败,且易被误诊为逻辑错误。

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 起点 | `from` | int32 | 0 .. N-1(越界由公式兜底,开发期 `ensure` 报警,见 AC-34) | 移动前格索引 |
| 步数 | `steps` | int32 | 任意 int32(正常单次 -(N-1)..+12;`\|steps\|>2N` 触超界告警但照常计算,绝不 clamp,见 AC-29) | 净位移;正=顺时针,负=逆时针,可超 N(多圈/后退牌) |
| 格数 | `N` | int32 | 4 .. ∞(经典=40;校验保证 N≥4,见 AC-18) | 读自 `GetTileCount()` |
| 落点 | `new_index` | int32 | 0 .. N-1 | 移动后格索引 |

**Output Range:** 恒在 `[0, N-1]`,由 mod 保证,无需额外 clamp。

**Example(N=40):** `from=38, steps=5 → mod(43,40) = 3`(越过 GO);`from=5, steps=-7 → mod(-2,40) = 38`(后退越过 GO)。

### F-2 经过出发格计数 PassedGoCount(`AdvanceIndex` 返回元组的 `passed_go` 分量)

`passed_go = floor((from + steps) / N) - floor(from / N)`  ← **通用式(R2 用户决策:恒用此式)**

`floor` 为**向负无穷取整**的地板除。**R2 变更:本系统恒用上方通用式,不再用简化式。** 理由:简化式 `floor((from+steps)/N)` 仅在 `from ∈ [0,N-1]`(此时 `floor(from/N)=0`)时成立;但 `ensure()` 在 Shipping 构建被编译为空操作,若 `from` 越界则简化式会**静默输出错值且无检测**。通用式对任意 `from` 数学正确(自修正),消除该 Shipping 隐患。在正常游戏 `from ∈ [0,N-1]` 时通用式 `== floor((from+steps)/N)`,与旧用例结果完全一致(AC-12~15 不变)。

> **越界与防护的关系(R2):** 通用式保证**计算结果**对越界 `from` 仍正确(鲁棒自修正)。这与 AC-34 的 `ensure()` **不矛盾、各司其职**:通用式负责"算对",`ensure()` 负责"开发期暴露上游状态损坏的 bug"。即:Shipping 下越界 `from` 不再产生错值(通用式兜底),开发期仍 `ensure` 报警以追查上游。

> **Blueprint 实现约束(强制;unreal-specialist):** Blueprint 的整数 `/` 节点是 **C 风格截断**(朝零取整,如 `-2 / 40 = 0`),**不是**地板除(应为 `-1`)。**必须**对通用式的**两个** `floor` 项各自先转 float 再 Floor,且**显式选 `Floor`(→int32) 节点**(2026-06-06 spike 更正:5.7 `Floor`/`Floor to Integer64` 入参均 double、差在节点名+返回类型,非 float/double 两条入参推导;误选 int32 `Floor` 处理需 int64 值会在 `static_cast<int32>` 截断,见 OQ-6/ADR-0002)。在 `|from+steps| < 2^23` 的正常游戏范围内 float 精度足够;远超此范围由 AC-29 超界告警捕获。**两个易错点(R3 补充):** ① **忘记转 float 时,Blueprint 整数 `/` 节点会直接截断(朝零)**——这比 double 重载误用更早发生且更隐蔽,务必确认 `/` 的输入是 float 而非 int;② **`from + steps` 的整数加法本身可能溢出 int32**(若 `steps` 接近 int32 上限),发生在 float 转换之前,与 float 精度无关——正常游戏 `steps` 远不及此,但地图编辑器(26)/未来联网协议若允许任意 `steps` 输入,须由 AC-34 的 `ensure` 前置截断非法输入(AC-29 用 `steps=1000` 测试,不覆盖 int32 溢出边界)。**推荐**:将 F-1/F-2 封装为 C++ `UFUNCTION(BlueprintPure)` 工具函数(如 `UBoardMathLibrary::AdvanceIndex`),Blueprint 调单一节点,约束一次性内化,单测直测 C++ 函数无需 Blueprint 运行环境(实现归属待 OQ-6 ADR)。

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 起点 | `from` | int32 | 0 .. N-1(越界由通用式兜底+`ensure` 报警) | 同 F-1 |
| 步数 | `steps` | int32 | 任意 int32(正常 -(N-1)..+12;超界照算+告警) | 同 F-1 |
| 格数 | `N` | int32 | 4 .. ∞(校验保证 N≥4) | 同 F-1 |
| 净经过次数 | `passed_go` | int32 | 理论无界;正常 -1 .. +2 | 净经过 GO 次数;`>0`=顺时针经过(发薪);`≤0`=未过/逆向穿越(经济不发薪,见 CR-2 消费契约) |

**Output Range:** 理论无界,正常单次掷骰/事件为 `-1 .. +2`。语义:`0`=未过 GO;`>0`=顺时针经过该次数;`<0`=逆时针穿越(经济系统据契约通常忽略,不发薪)。

**Example(N=40,通用式):** `from=38, steps=5 → floor(43/40)-floor(38/40)=1-0=1`(过 GO 一次,经济发薪一次);`from=5, steps=-7 → floor(-2/40)-floor(5/40)=-1-0=-1`(逆向穿越,`passed_go≤0` 经济不发薪)。越界示例:`from=-1, steps=5 → floor(4/40)-floor(-1/40)=0-(-1)=1`(通用式自修正,开发期 `ensure` 仍报警)。

### F-3 环上前向距离 StepsBetween(辅助查询)

`steps_forward = mod(target - from, N)`

给定起点与目标格,返回**顺时针**到达所需步数。供"前进到最近车站/最近公用"类机会牌与 AI 距离估值使用(纯拓扑,故归本系统)。

> **Blueprint 实现约束(强制,R3 新增;unreal-specialist):** F-3 的 `mod(target - from, N)` 与 F-1 是**完全相同的数学取余**,且 `target - from` 同样可为负(如 `StepsBetween(36,5)`:`5-36=-31`)。**必须**用组合式 `((A % N) + N) % N`,**禁止**直接用单个 Blueprint `%` 节点(C 风格对负数返回负值,会让 `StepsBetween(36,5)` 返回 `-31` 而非 `9`,AC-16 必失败且易被误诊为"逻辑写反")。等同 F-1 约束,见 F-1 Blueprint 实现约束。

> **`from == target` 语义契约(填补接口空白):** F-3 在 `from == target` 时返回 `0`(语义="已在目标格")。**调用方(机会牌/事件系统)负责**决定将 `0` 解释为"原地不动"还是"前进整圈(`N` 步)";本系统只如实返回拓扑距离,不做此裁决。
>
> **传送类事件的 `passed_go` 契约(消除发薪歧义;economy-designer):** "传送到 GO""前进到最近车站"等事件**必须先用 F-3 `StepsBetween(from, target)` 求出顺时针步数,再以该步数调用 F-1/F-2**。**禁止**以 `steps=0` 调用 F-2 来表达"传送"——那会让本应发薪的传送永不发薪。绕过公式直接改位置索引而跳过 `PassedGoCount` 同样禁止。

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 起点 | `from` | int32 | 0 .. N-1 | 当前格 |
| 目标 | `target` | int32 | 0 .. N-1 | 目标格 |
| 格数 | `N` | int32 | 4 .. ∞(校验保证 N≥4) | 同上 |
| 前向步数 | `steps_forward` | int32 | 0 .. N-1 | 顺时针步数;同格=0 |

**Output Range:** `[0, N-1]`。

**Example(N=40):** `from=36, target=5(车站) → mod(5-36,40) = mod(-31,40) = 9` 步。

## Edge Cases

### 加载期完整性校验(Validated 状态执行,失败则拒绝进入对局)

- **若 `N < 4`**:拒绝该棋盘。环路至少需容纳四角格(Go/Jail/FreeParking/GoToJail),少于 4 格无意义。返回校验错误并阻止开局。
- **若 `TileIndex` 不连续或不唯一(存在缺号/重号)**:拒绝该棋盘。索引必须恰好覆盖 `0 … N-1` 各一次,否则环序断裂。
- **若索引 0 不是 `Go` 类型**:拒绝。出发格固定为环路起点,移动/发薪逻辑依赖此约定。
- **若 `Property` 格的 `ColorGroup == None`**:拒绝。地产必须归属某色组,否则集组/建房无法判定。
- **若非 `Property` 格的 `ColorGroup != None`**:拒绝(反向校验)。仅地产格可有色组;车站/公用/角格/事件格等带色组属数据污染,会让 `GetTilesInGroup` 返回非法成员。
- **若 `RentTable` 长度与类型不符**(Property≠6、Railroad≠4):拒绝。长度即语义(见 CR-4),错长会让下游越界读取。
- **若 Utility 格的 `DiceMultiplierTable` 长度 ≠ 2**:拒绝。Utility 倍率数组固定长度 2(持 1/2 个公用)。
- **若 Utility 格 `DiceMultiplierTable` 任一元素 `<= 0` 或 `> DICE_MULT_MAX`**(R3 propagate,移动 OQ-Move-6 对端):拒绝。`<= 0` 非法(倍率须为正,见 Tuning「单调不减、非倍率值即数据错误」);上界 `DICE_MULT_MAX` 防经济(5) Utility 租公式 `骰点(≤12) × mult`(经济 F-4)的 int32 溢出——须保证 `12 × DICE_MULT_MAX ≤ INT32_MAX`。错误码 `DiceMultiplierOutOfRange{TileIndex}`。见 AC-23j。**owner 说明**:本表与其上界归棋盘数据(本系统),移动(4)既不拥有该表也不拥有 Utility 租公式(移动 OQ-Move-6 派发至此);最终 `DICE_MULT_MAX` 数值与 `SalaryAmount` 上界(经济 OQ-Econ-10)并轨裁定。
- **若 Property/Railroad 格带非空 `DiceMultiplierTable`,或 Utility 格带非空 `RentTable`**:拒绝(反向校验,防字段误填)。
- **若非可购买格(Chance/CommunityChest/Tax/Go/Jail/FreeParking/GoToJail)带非空 `RentTable` 或非空 `DiceMultiplierTable`**:拒绝(反向校验,完全不可购买格不应携带任何租金/倍率数组)。错误码 `RentArrayOnNonPurchasable{TileIndex}`。见 AC-23g-c。
- **若非 `Go` 格的 `SalaryAmount != 0`**:拒绝(反向校验,R2 新增,与 `DiceMultiplierTable`/`ColorGroup` 反向校验同模式)。`SalaryAmount` 仅 Go 格适用;非 Go 格携带薪额是数据污染,移动(4)只读 `GetTileData(0).SalaryAmount`,误填值虽不被读取但应在加载期拒绝以暴露作者错误。错误码 `SalaryOnNonGoTile{TileIndex}`。见 AC-23h。
- **若非 `Tax` 格的 `TaxAmount != 0`**:拒绝(反向校验,R2 新增,与 SalaryAmount 对称)。`TaxAmount` 仅 Tax 格适用。错误码 `TaxOnNonTaxTile{TileIndex}`。见 AC-23i。
- **若 Property 格的 `RentTable` 非单调不减**(存在 `RentTable[i] > RentTable[i+1]`):**发出警告但不拒绝**(CD 裁定:单调性是平衡属性,归经济(5)/playtest;自定义棋盘/House Rule 允许)。严格递增 vs 非递减由经济(5)裁决。警告码 `RentTableNotMonotonic{TileIndex}`。
- **若 `EventDeck == None` 而格类型为 `Chance`/`CommunityChest`**:拒绝。事件格必须指向一个牌堆,否则事件系统(7)收到空引用。
- **若存在 `GoToJail` 格但全盘无 `Jail` 格**:拒绝。`GoToJail` 的目标未定义。
- **若 `Go` 类型格数 ≠ 1**(缺失或多于一个):拒绝。F-2 的 GO 计数假设 Go 唯一且固定在索引 0。
- **若 `Jail` / `FreeParking` / `GoToJail` 任一类型格数 ≠ 1**(R3 新增,补角格唯一性缺口):**发出警告但不拒绝**(自定义棋盘可能有 0 个或多个某类角格,如无监狱的简化盘;但经典四角盘应各 1)。警告码 `CornerTileCountUnusual{TileType, Count}`。**注:`GoToJail` 存在但无 `Jail` 仍是硬拒绝(见上一条,目标未定义);此条覆盖的是"数量异常但不致命"的情形——如 2 个 FreeParking。** 经典盘 `DA_Board_Classic40` 四角各恰 1,由 AC-3 精确计数守住。此规则堵住 R3 发现的 gap:此前仅 Go 数有校验,Jail/FreeParking/GoToJail 唯一性无任何规则。
- **若同色组内各格的 `BuildingCost` 不完全相同**:发出警告(不拒绝)。经典盘同组造价应一致(由资产单元测试守住);自定义棋盘若差异化建房成本,需确认与均衡建房规则的交互由建房系统(8)显式处理。警告码 `BuildingCostMismatchInGroup{ColorGroup}`。
- **若某色组的成员格数与声明不符**(经典盘:棕2/浅蓝3/粉3/橙3/红3/黄3/绿3/深蓝2):发出**警告但不拒绝**。自定义棋盘(地图编辑器)允许非经典组构成;经典盘 `DA_Board_Classic40` 必须精确匹配,由资产单元测试守住。
- **若 `MortgageValue > PurchasePrice`**:**拒绝**(数据错误:抵押所得不应超过买价,`MortgageValue ≤ PurchasePrice` 是硬数据约束,非合法 House Rule)。
- **若 `MortgageValue > PurchasePrice × 0.6`**(超过 60% 抵押率,但 ≤ 100%):**发出警告**(抵押率越出经典规则区,经典约 50%)。警告码 `MortgageRateHigh{TileIndex}`。**阈值来源/归属(R2,R-econ 2026-06-02 更正):60% 是【board 数据质量】保守警戒线——警告的是抵押率数据异常,非套利防范。〔旧措辞"套利风险区间 / 抵押-赎回循环回报趋近转正 / 理论套利路径"系错误,已删:经济 AC-42 数学证伪——一抵一赎净现金 = `−fee_rate×MortgageValue ≤ 0`,对任意抵押率恒非正,套利不存在。〕此阈值是经济(5)的配置旋钮,经济(5)若据数据质量策略调整(如 55%/65%)应在经济 GDD 记录并更新 registry `mortgage_warning_threshold`(source 临时归棋盘待经济5接管),而非改本系统硬编码。**
- **若可购买格(Property/Railroad/Utility)的 `PurchasePrice <= 0`**:拒绝。可购买格必须有正价格。
- **若 `DisplayName` 为空**:发出警告;回退显示为 `"Tile {index}"`,不阻止开局(便于编辑器草稿态)。

> **校验范围声明(R3 新增,game-designer;给地图编辑器(26)留体验层设计债):** 本系统的加载期校验只保证**结构正确性**(数据类型、字段约束、枚举合法性、长度匹配),**不保证体验可玩性**(色组节奏、经济平衡、棋盘可理解性)。一张自定义棋盘可以"结构合法但反体验"——如单色组含 8 格、各格 `BuildingCost` 全不同(仅触发警告)、全盘无 Chance/CommunityChest(合法)。**地图编辑器(26) GDD 有责任在结构校验之上增加体验层指引**(推荐色组大小范围、推荐 Railroad 数量、N 与 `SalaryAmount` 联动说明),以防用户产出技术合法但无法理解的棋盘、破坏支柱④易上手。MVP 不实现编辑器,此处仅留设计债记录,使编辑器(26)作者有文档依据承担该责任。

### 运行时位移边界(F-1/F-2/F-3 的输入异常)

- **若 `steps` 为负(后退)**:正常处理。F-1 的数学取余、F-2 的地板除均已对负值定义正确;落点恒在 `[0,N-1]`。
- **若 `steps` 后退穿越 GO**:F-2 返回 `passed_go < 0`。按设计契约,棋盘数据如实返回负数;经济系统据约定**不发薪**。棋盘数据本身不裁决。
- **若 `|steps| ≥ N`(多圈)**:正常处理。F-1 落点正确,F-2 返回净经过次数(可 ≥ 2)。防护:若 `|steps| > 2*N`(远超任何合法掷骰/事件组合),仍**照常计算、绝不 clamp**(clamp 会破坏落点),但记一条「步数超界」告警日志供排查上游 bug。位移与计数结果始终如实可信——这与 Formulas 的"如实输出有符号值"契约一致,保证联网回放与单元测试的确定性。
- **若 `from` 越界(< 0 或 ≥ N)**:**双重处理(R2)**——① **计算层**:F-1/F-2 恒用对越界鲁棒的形式(F-1 的 `((A%N)+N)%N`、F-2 的通用式),即使 `from` 越界也输出数学正确值,Shipping 下不产生错值;② **检测层**:开发期 `ensure()`(报错+继续,而非 `check()` 硬崩溃——联网场景数据损坏不应让客户端崩)+ 记 error 日志,暴露上游状态损坏。`from` 来自合法棋子位置,越界即上游 bug;计算兜底保证结果可信,`ensure` 保证 bug 不被掩盖。两者各司其职、不矛盾。见 AC-34。
- **若 `GetTilesInGroup(None)` 被调用**:返回空数组(非地产格无组),不报错。
- **若 `GetTileData(index)` 的 index 越界**:返回校验失败/断言;调用方应先用 `GetTileCount()` 约束范围。

## Dependencies

### 上游(本系统依赖的系统)

**无。** 棋盘数据是零依赖的 Foundation 系统,这正是它被排在设计顺序第 1 位的原因。它不读取任何其他系统的输出。

### 下游(依赖本系统的系统)

全部为**硬依赖**(没有棋盘数据这些系统无法运行)。接口细节见 Detailed Design → Interactions:

| 系统 | 关系 | 读取内容 |
|---|---|---|
| 移动(4) | 硬 | `GetTileCount()`、`AdvanceIndex → (newIndex, passedGo)`(单一原子接口,无独立 PassedGoCount)、`GetTileData(0).SalaryAmount` |
| 地产所有权(6) | 硬 | `PurchasePrice`、`ColorGroup`、`MortgageValue`、`TileType`、`GetTilesInGroup` |
| 事件格(7) | 硬 | `TileType`、`EventDeck`、`TaxAmount`、`SpecialAction` |
| 建房(8) | 硬 | `BuildingCost`、`RentTable`、`ColorGroup`、`GetTilesInGroup` |
| HUD(16) | 硬 | `DisplayName`、`ColorGroup` 颜色、`PurchasePrice` |
| 地产卡 UI(17) | 硬 | `DisplayName`、`RentTable`、`ColorGroup`、`MortgageValue` |
| 游戏反馈 VFX(19) | 软(呈现层叶子) | 只读查询 `GetTileWorldTransform(TileIndex)`(格事件/juice 锚点 tile-center)+ 环序总格数 N(自建 hop path 插值);juice 视觉腿,不写本系统(vfx-feedback RB-4/OQ-VFX-7,R-1 揪出) |
| 存档(21) | 硬 | `GetBoardId()`(棋盘标识,非全量布局) |
| 设置 & House Rules(23) | 软 | 可选覆盖棋盘相关 House Rule(如 FreeParking 奖池) |
| 地图编辑器(26) | 硬(读写) | 完整 `FBoardTileData` schema;产出新 `DA_` 实例 |

### 双向一致性修正(待同步系统索引)

系统索引当前仅在 移动(4)/所有权(6)/事件格(7)/存档(21)/地图编辑器(26) 的 depends-on 标了"1"。经本 GDD 的接口分析,以下系统也**直接读取**棋盘数据字段,应在索引补标对棋盘数据(1)的依赖(决策:显式加依赖):

- **建房(8)** — 直接读 `BuildingCost / RentTable / ColorGroup`
- **HUD(16)** — 直接读 `DisplayName / ColorGroup / PurchasePrice`
- **地产卡 UI(17)** — 直接读 `DisplayName / RentTable / ColorGroup / MortgageValue`

> 此修正将在 Phase 5 更新 `design/gdd/systems-index.md` 的依赖列。

## Tuning Knobs

棋盘数据的旋钮就是 `DA_` Data Asset 中的字段——**全部数据驱动、零硬编码**。具体数值在经济系统调参期填入 `DA_Board_Classic40`(经济系统是这些数值的平衡责任方);本节定义结构性旋钮及其安全范围与约束。

| 旋钮 | 作用 | 安全范围 | 过低 / 过高的后果 |
|---|---|---|---|
| `N`(TileCount) | 一张棋盘的格数 | 经典=40;编辑器 12 .. 60 | 过低(<4)无法容纳四角,校验拒绝;过高俯视棋盘可读性下降、单圈过长拖沓 |
| `PurchasePrice`(每格) | 地产/车站/公用购买价 | > 0;最便宜地产建议 ≤ 起始现金的 ~15%(确保开局有实质购买决策而非无脑全买)。具体比值由经济(5)现金流模型确定,需回链此处参数名 | 过低地产瞬间被买光无博弈;过高无人买得起,经济停滞 |
| `RentTable`(每格,Property/Railroad) | 各等级现金租金 | **应单调不减**(房越多租越高);违反触发警告 `RentTableNotMonotonic`(见 Edge Cases),不拒绝 | 非递增则建房无意义(支柱③策略坍塌);过高一次收租即破产,过低收租无感 |
| `DiceMultiplierTable`(Utility) | 公用事业骰点倍率 | 长度 2,单调不减(如 `[4,10]`);每元素 `∈ [1, DICE_MULT_MAX]`(`DICE_MULT_MAX` 防 `12×mult` int32 溢出,初值建议 `1_000_000`,留 ~178× 裕度;最终值并入经济 OQ-Econ-10 裁定,见 AC-23j);非倍率值即数据错误 | 过低公用无收益;过高一次过路即重伤;越界 = 数据错误/溢出风险 |
| `SalaryAmount`(Go) | 经过/停留 GO 发薪额 | > 0;现金注入主阀,与 `N`、起始现金联动 | 过低绕圈无激励、经济通缩;过高现金泛滥、地产相对贬值 |
| `BuildingCost`(每格) | 每栋房屋造价 | > 0;通常与该色组租金涨幅挂钩 | 过低建房无成本风险;过高无人建房,中后期无成长 |
| `MortgageValue`(每格) | 抵押可得现金 | > 0 且 ≤ `PurchasePrice`(经典约 50%) | 过高抵押率异常(触发数据质量警告);过低抵押无救急价值 |
| `TaxAmount`(税格) | 固定税额 | > 0 | 过高惩罚过重制造挫败;过低税格形同虚设 |
| 布局(index→类型映射) | 棋盘格局 | 经典盘锁定;编辑器自由 | 错位会破坏色组节奏与四角对称(经典盘由单元测试守住) |

**旋钮间相互作用(调参注意):**
- `RentTable` 与 `BuildingCost` 联动:建房回报率 = 租金涨幅 / 造价,经济调参时需整组评估,不可单格调。
- `PurchasePrice` 与 `MortgageValue` 绑定:抵押率 = `MortgageValue / PurchasePrice`,建议全盘统一(经典 50%),否则出现抵押率异常地块(触发数据质量警告)。
- `N` 改变会同时影响单圈时长与"经过 GO 发薪"频率(经济现金注入节奏),属跨系统旋钮,调整需与经济、玩家回合系统协同。
- **变更发起方锁定(R2,economy-designer):** 当 playtest 驱动的局长/现金流调参涉及 `SalaryAmount`/`TaxAmount`/`PurchasePrice`/`MortgageValue` 时,**变更提案由经济系统(5)发起**(这是经济平衡判断),棋盘(1)仅执行写入 DA。任何对这些字段的修改须在经济(5) GDD 有平衡依据记录,反向引用到本 GDD 变更历史。此条锁定发起方,防两系统审批路径互相推诿。这些字段同时注册于 entities registry(source=棋盘数据,平衡推导责任归经济 5),使跨系统冲突在 `/consistency-check` 被捕获。

> **数值来源说明:** 本表只定义范围与约束;`DA_Board_Classic40` 的实际地价/租金/造价由经济系统 GDD 的公式与调参产出,变更须回链到经济 GDD 的平衡依据(编码标准:平衡值须链接来源公式或理由)。

## Visual/Audio Requirements

**n/a —— 本系统是纯数据层,不渲染任何视觉、不触发任何音频。** 棋盘的视觉呈现(棋盘板面、地块、色组色条、建筑、桌面氛围)由 art bible §6《环境设计语言》定义并由 `SM_env_board_*` / `T_env_board_tile_*` 资产实现;格子上的信息展示(名称、地价、租金表、所有权角标)由 HUD(16) 与 地产卡 UI(17) 的 GDD 拥有。本系统只为这些呈现层**提供数据**(`DisplayName`、`ColorGroup` 颜色键、`PurchasePrice`、`RentTable` 等),自身无视觉/音频职责。

## UI Requirements

**n/a —— 本系统无任何 UI。** 它不绘制屏幕元素,只向 UI 层暴露只读数据查询(见 Detailed Design → Interactions)。所有面向玩家的棋盘/地产信息显示由 HUD(16)、地产卡 UI(17) 的 GDD 与对应 `WBP_` Widget 实现;地图编辑器(26)的编辑界面属该系统自身的 UI 范畴。本系统的"接口"是数据契约,不是界面。

## Acceptance Criteria

> **测试分层(qa-lead / unreal-specialist):**
> - **`[Logic]`** = 纯逻辑(确定性输入→输出,用 fixture struct 构造),放 `tests/unit/board-data/`,在 CI `-nullrhi` headless 跑,无文件 I/O。**可作 PR merge gate(BLOCKING)。**
> - **`[Config]`** = `.uasset` 资产布局校验,放 `tests/asset-validation/board-data/`,**需编辑器/commandlet 环境**(不能在 `-nullrhi` headless 跑)。**因此 `[Config]` 测试只能作 nightly/staging gate,不能作 PR merge gate** —— 此约束须传递给 devops-engineer 配置 CI pipeline,否则 `[Config]` 测试会悄悄失去 CI 保护。
> - **`[Advisory]`** = 非自动化(契约/全局会话类),靠 code review + sign-off 守门。
> - 警告类校验须返回**结构化警告**(枚举/struct,如 `RentTableNotMonotonic{TileIndex}`),非自由文本 log,以便测试断言 pass/fail。
> - **跨系统契约的测试责任归属(R2,R3 加追踪机制):** 部分契约的 BLOCKING 测试**不归本系统**,因棋盘数据无法在自身单测中验证调用方行为:逆向/原地不发薪(AC-38)归经济(5);传送到 GO 发薪、传送先用 F-3 求步数(AC-39)归事件格(7) Integration;GetBoardId 跨会话稳定(AC-28b)归存档(21) Integration。本系统提供公式/契约,下游 GDD 承接其 BLOCKING 测试。**⚠ R3 修订:仅文字"承接"不构成追踪,否则责任真空只是推迟到下游 GDD 成形时才暴露(与 `bIsPlaceholderData` 软门控同构)。强制追踪机制见 OQ-8:这些外推测试在对应下游 GDD 的 §Acceptance Criteria 必须有显式 AC 实现并回链本 GDD 的 AC 编号;`/design-review` 审下游 GDD 时核对此回链,未实现则该下游 GDD 不得 APPROVED。** 此分配 + OQ-8 追踪共同避免责任真空。

### A. 核心规则(CR-1 ~ CR-6)

- **AC-1(CR-1)** [Logic] GIVEN 已校验经典盘(N=40),WHEN `GetTileCount()`,THEN 返回 40 且索引 0 的 `TileType==Go`。
- **AC-2(CR-1 环闭合)** [Logic] GIVEN N=40,WHEN `AdvanceIndex(39, 1)`,THEN 返回 0(首尾相接)。
- **AC-3(CR-2 类型完整,R2 改精确计数)** [Config] GIVEN 经典盘 DA,WHEN 遍历 40 格 `TileType`,THEN 每格均属 `ETileType` 合法枚举成员(无非法整数),且各枚举值出现次数精确为:`Go=1`、`Jail=1`、`FreeParking=1`、`GoToJail=1`、`Tax=2`、`Chance=3`、`CommunityChest=3`、`Railroad=4`、`Utility=2`、`Property=22`(精确计数优于"至少出现一次")。
- **AC-4(CR-3 Property schema 结构,R2 拆分:可作 PR gate)** [Logic] GIVEN 用代码构造的 Property `FBoardTileData` fixture(如 `ColorGroup=Red, PurchasePrice=300, RentTable=[20,60,180,500,700,900], BuildingCost=200, MortgageValue=150`),WHEN 校验 schema 合法性,THEN `ColorGroup!=None` 且 `PurchasePrice>0` 且 `RentTable.Num()==6` 且 `BuildingCost>0` 且 `DiceMultiplierTable.Num()==0`。**纯 fixture、headless 可跑、作 BLOCKING PR gate(不依赖资产 temp-fill 状态)。**
- **AC-4-asset(CR-6 资产 temp-fill 已填)** [Config] GIVEN `DA_Board_Classic40`,WHEN 遍历所有 Property 格,THEN 每格 `PurchasePrice>0` 且 `BuildingCost>0`(验证 temp-fill 已实际写入资产,而非默认 0)。**依赖资产、nightly 跑;与 AC-4 区分使失败原因可辨(逻辑 bug vs 资产未填)。**
- **AC-4b(CR-3 Utility schema)** [Config] GIVEN 任一 Utility 格,WHEN 读 `FBoardTileData`,THEN `PurchasePrice>0` 且 `DiceMultiplierTable.Num()==2` 且 `RentTable.Num()==0`(倍率不混入 RentTable)。
- **AC-4c(CR-6 temp-fill 硬门控,R3 新增——使 `bIsPlaceholderData` 机器可验证)** [Config] GIVEN `DA_Board_Classic40`,WHEN 读 DA 顶层 `bIsPlaceholderData` 字段,THEN 在 **Alpha 里程碑构建**中必须为 `false`(经济(5)已覆盖 temp-fill 并回链平衡依据);若为 `true` 则测试失败、Alpha gate 不通过。**此测试标 `gate_level: alpha_blocking`(传递给 devops-engineer 在 CI pipeline 配置):MVP/nightly 阶段允许 `true`,Alpha 门控阶段强制 `false`。** 此 AC 把 CR-6/OQ-1 的"占位不得进 Alpha"从文字承诺转为机器断言,堵住"AC-4-asset 因 temp-fill 值 >0 而绿灯、`bIsPlaceholderData` 却无人验"的绕过路径(见 OQ-1 关闭条件)。
- **AC-5(CR-4 数组语义长度)** [Config] GIVEN 经典盘,WHEN 按类型查数组长度,THEN Property `RentTable`==6、Railroad `RentTable`==4、Utility `DiceMultiplierTable`==2 且 `RentTable`==0、非购买格 `RentTable`==0。
- **AC-6(CR-5 色组成员)** [Config] GIVEN 经典盘,WHEN 对 8 色组调 `GetTilesInGroup`,THEN 数量为 棕2/浅蓝3/粉3/橙3/红3/黄3/绿3/深蓝2,合计 22。
- **AC-7(CR-6 经典布局精确匹配)** [Config] GIVEN `DA_Board_Classic40`(已 temp-fill,见 CR-6),WHEN 逐格比对 `TileType`(及 Property 的 ColorGroup)与 CR-6 规格表,THEN 全 40 格匹配;统计=22地产+4车站+2公用+4角+2税+3机会+3命运。**测试 oracle(R2):Property 的 ColorGroup 期望值由 CR-6 的中文色标 + CR-5.1 枚举映射唯一确定(棕→Brown、浅蓝→LightBlue、粉→Magenta、橙→Orange、红→Red、黄→Yellow、绿→Green、深蓝→DarkBlue);实现者据此构造精确 `index→(TileType,ColorGroup)` oracle,不得各自臆测。**
- **AC-7b(Go SalaryAmount schema)** [Config] GIVEN `DA_Board_Classic40` 索引 0 的 Go 格,WHEN 读 `FBoardTileData`,THEN `SalaryAmount > 0` 且 `SpecialAction==CollectSalary`(发薪金额由棋盘拥有,见 CR-3)。

### B. 公式(F-1 ~ F-3,含边界)

> **R2 接口说明:** F-1(落点)与 F-2(`passed_go`)是 `AdvanceIndex` 单一原子调用返回元组的两个分量(见 Interactions 原子性契约 / AC-37a/AC-37b)。下方 AC-8~AC-15 分别断言元组的 `new_index` 分量与 `passed_go` 分量,**不意味着存在两个可分离调用的接口**;它们测的是同一次 `AdvanceIndex(from,steps)` 返回元组的两个字段。F-3 `StepsBetween` 是独立的辅助查询接口。

- **AC-8(F-1)** [Logic] N=40,`AdvanceIndex(28,7)` → 35。
- **AC-9(F-1 越 GO)** [Logic] `AdvanceIndex(38,5)` → 3。
- **AC-10(F-1 负步数)** [Logic] `AdvanceIndex(5,-7)` → 38(数学取余,非负)。
- **AC-11(F-1 多圈)** [Logic] `AdvanceIndex(10,40)` → 10;`AdvanceIndex(10,83)` → 13。
- **AC-12(F-2 过 GO 一次)** [Logic] `PassedGoCount(38,5)` → 1。
- **AC-13(F-2 未过 GO)** [Logic] `PassedGoCount(28,7)` → 0。
- **AC-14(F-2 逆向穿越 GO)** [Logic] `PassedGoCount(5,-7)` → -1(如实输出有符号值,本系统不裁决发薪)。
- **AC-15(F-2 多圈计数)** [Logic] `PassedGoCount(38,45)` → 2。
- **AC-16(F-3 前向距离)** [Logic] `StepsBetween(36,5)` → 9。
- **AC-17(F-3 同格)** [Logic] `StepsBetween(20,20)` → 0。

### C. 加载期校验(拒绝类 + 警告类)

- **AC-18(拒绝 N<4)** [Logic] GIVEN N=3 棋盘,WHEN 校验,THEN 失败返回 `BoardTooSmall`,对局不得启动。
- **AC-19a(拒绝 索引缺号,R2 拆分)** [Logic] GIVEN 索引序列 `[0,1,3]`(缺 2),WHEN 校验,THEN 失败 `MissingTileIndex{2}`。
- **AC-19b(拒绝 索引重号,R2 拆分)** [Logic] GIVEN 索引序列 `[0,1,1]`(重号),WHEN 校验,THEN 失败 `DuplicateTileIndex{1}`。
- **AC-20(拒绝 idx0 非 Go)** [Logic] GIVEN 索引 0 `TileType!=Go`,WHEN 校验,THEN 失败 `Index0NotGo`。
- **AC-21(拒绝 Property 无色组)** [Logic] GIVEN Property 格 `ColorGroup==None`,WHEN 校验,THEN 失败并指明 TileIndex。
- **AC-22(拒绝 Property RentTable 长度不符)** [Logic] GIVEN Property 格 `RentTable.Num()==4`,WHEN 校验,THEN 失败并指明期望长度 6、实际 4 及 `TileIndex`。
- **AC-22b(拒绝 Railroad RentTable 长度不符,R2 新增)** [Logic] GIVEN Railroad 格 `RentTable.Num()==6`,WHEN 校验,THEN 失败并指明期望长度 4、实际 6 及 `TileIndex`(Railroad 是 4 个核心可购买格,长度校验不可漏测)。
- **AC-23(拒绝 可购买格价格≤0)** [Logic] GIVEN 任一可购买格类型(Property/Railroad/Utility)`PurchasePrice<=0`(三种类型各一 fixture),WHEN 校验,THEN 失败并指明 `TileIndex`(R2:三种可购买格均须覆盖,不止 Railroad)。
- **AC-23b(拒绝 EventDeck 缺失)** [Logic] GIVEN Chance/CommunityChest 格 `EventDeck==None`,WHEN 校验,THEN 失败 `EventDeckMissing{TileIndex}`。
- **AC-23c(拒绝 GoToJail 无 Jail)** [Logic] GIVEN 含 `GoToJail` 格但全盘无 `Jail` 格,WHEN 校验,THEN 失败 `NoJailTile`。
- **AC-23d(拒绝 Go 数 ≠ 1)** [Logic] GIVEN 0 个或 ≥2 个 `Go` 格,WHEN 校验,THEN 失败 `GoTileCountInvalid`。
- **AC-23e(拒绝 MortgageValue>PurchasePrice)** [Logic] GIVEN 可购买格 `MortgageValue > PurchasePrice`,WHEN 校验,THEN 失败(数据错误:`MortgageValue` 不应超过 `PurchasePrice`,非 House Rule)。
- **AC-23f(拒绝 Utility 倍率长度)** [Logic] GIVEN Utility 格 `DiceMultiplierTable.Num()!=2`,WHEN 校验,THEN 失败并指明期望长度 2。
- **AC-23g-a(拒绝 Property/Railroad 带倍率表,R2 拆分)** [Logic] GIVEN Property 或 Railroad 格 `DiceMultiplierTable.Num()>0`,WHEN 校验,THEN 失败 `DiceMultiplierTableNotAllowed{TileIndex}`。
- **AC-23g-b(拒绝 Utility 带租金表,R2 拆分)** [Logic] GIVEN Utility 格 `RentTable.Num()>0`,WHEN 校验,THEN 失败 `RentTableNotAllowed{TileIndex}`。
- **AC-23g-c(拒绝 非可购买格带数组,R2 新增)** [Logic] GIVEN 非可购买格(如 Tax/Chance/Go)`RentTable.Num()>0` 或 `DiceMultiplierTable.Num()>0`,WHEN 校验,THEN 失败 `RentArrayOnNonPurchasable{TileIndex}`。
- **AC-23h(拒绝 非 Go 格带薪额,R2 新增)** [Logic] GIVEN 非 `Go` 格 `SalaryAmount != 0`(如 Property 格 SalaryAmount=200),WHEN 校验,THEN 失败 `SalaryOnNonGoTile{TileIndex}`(反向校验,防数据污染)。
- **AC-23i(拒绝 非 Tax 格带税额,R2 新增)** [Logic] GIVEN 非 `Tax` 格 `TaxAmount != 0`,WHEN 校验,THEN 失败 `TaxOnNonTaxTile{TileIndex}`(与 SalaryAmount 反向校验对称)。
- **AC-23j(拒绝 Utility 倍率越界,R3 propagate,移动 OQ-Move-6 对端)** [Logic] GIVEN Utility 格 `DiceMultiplierTable` 含元素 `<= 0`(如 `[0,10]`)或 `> DICE_MULT_MAX`(如 `[4, 200000000]`),WHEN 校验,THEN 失败并指明越界元素与合法区间 `[1, DICE_MULT_MAX]`,错误码 `DiceMultiplierOutOfRange{TileIndex}`。边界 `12 × DICE_MULT_MAX ≤ INT32_MAX` 保证经济(5) Utility 租 `骰点 × mult`(F-4)不溢出 int32。**纯 fixture、headless 可跑、作 BLOCKING PR gate。**(填补缺口:此前 AC-23f 仅校验 `DiceMultiplierTable.Num()==2` 长度,不校验元素值上界——移动 OQ-Move-6/economy OQ-Econ-10 并轨。)
- **AC-24a(警告 空名回退)** [Logic] GIVEN 某格 `DisplayName` 为空,WHEN 校验,THEN **通过**并产生结构化警告 `EmptyDisplayName{TileIndex}`,且 `GetTileData` 回退显示 `"Tile {index}"`。
- **AC-24b(警告 色组成员数异常)** [Logic] GIVEN 某色组成员数与声明不符,WHEN 校验,THEN **通过**并产生结构化警告 `ColorGroupMemberCountMismatch{ColorGroup}`(自定义棋盘允许;经典盘由 AC-7 资产测试守住)。
- **AC-24c(警告 抵押率偏高,R2 明确边界 fixture)** [Logic] GIVEN `PurchasePrice=100, MortgageValue=61`(即 `61 > 100×0.6=60`),WHEN 校验,THEN **通过**并产生结构化警告 `MortgageRateHigh{TileIndex}`;边界对照:`MortgageValue=60`(=60% 下界)**不触发**警告(安全区);`MortgageValue=100`(=PurchasePrice)由 AC-23e 拒绝、不适用本 AC。**阈值用 `MortgageValue > floor(PurchasePrice×0.6)`(`>` 非 `≥`),int 截断边界以此 fixture 锁定。**
- **AC-24d(警告 角格数量异常,R3 新增,守 CornerTileCountUnusual)** [Logic] GIVEN fixture 棋盘含 2 个 `FreeParking` 格(数量异常但非致命),WHEN 校验,THEN **通过**并产生结构化警告 `CornerTileCountUnusual{FreeParking, 2}`(自定义棋盘允许;经典盘四角各 1 由 AC-3 精确计数守住)。注:`GoToJail` 存在但无 `Jail` 仍由 AC-23c 硬拒绝。

### D. 运行时只读契约 + 防护

- **AC-25(只读不变)** [Advisory] "运行时不暴露写接口、静态字段不可变"是**接口契约**,非自动化断言("一整局后"属 CLAUDE.md 排除的完整会话)。改为 **code review 守门** + sign-off:review 确认 `GetTileData` 返回 const 引用/值拷贝、无任何 public 写接口。记录于 `production/qa/evidence/`。
- **AC-26(GetTilesInGroup(None))** [Logic] WHEN `GetTilesInGroup(None)`,THEN 返回空数组,不报错。
- **AC-27(越界访问防护,R2 统一防护语言)** [Logic] N=40,WHEN `GetTileData(40)` 或 `GetTileData(-1)`,THEN 触发 `ensure(false)`(开发期报错+继续,非 `check()` 硬崩溃,与 AC-34 `AdvanceIndex` 越界防护语言一致)并记 error 日志,返回默认空 `FBoardTileData`,不静默返回脏数据;调用方应先用 `GetTileCount()` 约束范围。
- **AC-28(GetBoardId 类型与同进程稳定性,R2 拆分)** [Logic] GIVEN `DA_Board_Classic40`,WHEN 同进程内多次调 `GetBoardId()`,THEN 返回**非空 `FName`,等于 DA 内显式 `BoardIdentifier` 字段(如 `"Classic40"`),与资产路径解耦**,且每次结果完全一致。**`FName` 大小写不敏感陷阱(R3,unreal-specialist):`FName` 比较是 case-insensitive 的(`"Classic40"`==`"classic40"`),存档系统(21)通过 `FName` 比较时安全;但若任何地方降级为 `FString` 比较,须先 `ToLower()` 归一化,否则作者大小写不一致会引发隐蔽读档失配 bug。**
- **AC-28b(GetBoardId 跨会话/跨存档稳定性,R2 拆分)** [Advisory/Integration] GIVEN 一局存档后重启会话读档,WHEN 再调 `GetBoardId()`,THEN 返回值与存档时一致(供存档系统 21 可靠引用)。**跨会话/序列化往返非纯 [Logic] 可测,归存档系统(21)的 Integration 测试 + code review 守门**(单元测试无法跨进程)。
- **AC-29(步数超界照算+告警)** [Logic] N=40,WHEN `AdvanceIndex(0,1000)`,THEN 落点仍数学正确(mod 1000,40=0),`PassedGoCount` 如实返回(=25),并记一条「步数超界」告警日志;**不 clamp、不中断**(与 Edge Cases 多圈防护一致)。
- **AC-30(GetTilesInGroup 升序契约,R2 明确乱序 fixture)** [Logic] GIVEN fixture 棋盘:某色组(如 LightBlue)成员索引以非升序录入 `[9,6,8]`(故意非顺非逆,含中间值),WHEN 对该色组调 `GetTilesInGroup(LightBlue)`,THEN 返回 `[6,8,9]`(严格 `TileIndex` 升序;验证实现显式 `Sort` 而非依赖录入顺序或数组反转)。
- **AC-32(非地产格无色组反向校验)** [Logic] GIVEN 构造一组含"非 `Property` 格带 `ColorGroup!=None`"的 fixture,WHEN 校验,THEN 失败(此为拒绝规则,与 AC-18~23 同属 BLOCKING,故 `[Logic]` 非 `[Config]`)。
- **AC-33(停在 GO 即经过)** [Logic] N=40,WHEN `PassedGoCount(38,2)`(恰好停在索引 0),THEN 返回 1(`floor(40/40)=1`,停在 GO 等价经过一次)。**据 CR-2 发薪权威契约,经济仅据 `passed_go` 发薪一次,Go 格 `CollectSalary` 不再触发第二次。**
- **AC-34(from 越界 ensure 防护 + 通用式仍正确返回,R3 补返回断言)** [Logic] N=40,WHEN `AdvanceIndex(from=-1, steps=5)` 或 `AdvanceIndex(from=40, steps=1)`,THEN ① 触发 `ensure()`(开发期报错+继续,非崩溃)并记 error 日志,不静默修正(暴露上游状态损坏);**② 同时返回通用式数学正确的结果(`AdvanceIndex(-1,5)→(new_index=4, passed_go=1)`;`AdvanceIndex(40,1)→(new_index=1, passed_go=0)`),验证 F-1 `((A%N)+N)%N` 与 F-2 通用式对越界 `from` 的鲁棒自修正(计算层"算对"与检测层"`ensure` 报警"各司其职,见 F-2 越界与防护契约)。** 断言双重处理:既报警又返回可信值。
- **AC-35(RentTable 单调性警告)** [Logic] GIVEN Property 格 `RentTable` 存在 `RentTable[i] > RentTable[i+1]`,WHEN 校验,THEN **加载成功**并产生结构化警告 `RentTableNotMonotonic{TileIndex}`(警告非拒绝,见 Edge Cases / CD 裁定)。
- **AC-36(BuildingCost 同组不一致警告,R2 新增)** [Logic] GIVEN fixture 棋盘:同一色组(如 Red)三格 `BuildingCost` 为 `[150,150,200]`(不一致),WHEN 校验,THEN **加载成功**(不拒绝)并产生结构化警告 `BuildingCostMismatchInGroup{Red}`(见 Edge Cases;警告码此前已承诺但无 AC)。
- **AC-37a(AdvanceIndex 原子返回,R3 拆分自 AC-37)** [Logic] WHEN 对同一 `(from=38, steps=5)` 调用 `AdvanceIndex`,THEN 返回**单个元组/struct** `(new_index=3, passed_go=1)`(两值由同一次调用返回)。**此为可运行时断言的 [Logic] 测试:验证元组两分量值正确且一次返回。**
- **AC-37b(无独立 PassedGoCount 公开接口,R3 拆分自 AC-37)** [Advisory] **API 设计期 code-review + 编译期守门(非运行时 [Logic]):** review 确认代码库**不存在**独立的公开 `PassedGoCount(from, steps)` 接口可被分离调用(F-2 仅为 `AdvanceIndex` 返回元组 `passed_go` 分量的文档/单测命名,不暴露为独立函数)。**理由(R3 修订):运行时单测无法断言"某函数不存在"——引用不存在的函数会编译失败而非测试失败,引用存在的函数则证明它存在;故此项只能由 code-review/编译期静态检查守门,不可标 [Logic]。** 防两函数分离实现导致联网回放不一致(见 Interactions 原子性契约)。记录于 `production/qa/evidence/`。
- **AC-38(逆向/原地不发薪消费契约,R2 新增)** [Logic] GIVEN `passed_go ∈ {-2,-1,0}`,WHEN 经济消费契约判定发薪,THEN 不发薪(仅 `passed_go>0` 发 `passed_go × SalaryAmount`)。**此 AC 的 BLOCKING 测试归属经济系统(5)**,本系统仅声明契约(`passed_go` 如实输出有符号值,见 F-2 / CR-2 消费契约)。
- **AC-39(传送到 GO 发薪,R2 新增,测试责任归系统7)** [Advisory] *(本系统视角为 Advisory——契约声明;实测为事件格(7)的 Integration 测试。R3 修订:原 `[Advisory→Integration]` 非本 GDD 分层法定标签,本系统三档只有 [Logic]/[Config]/[Advisory],故标 [Advisory]。)* GIVEN 机会牌"传送到 GO"从索引 35 触发,WHEN 事件格(7)按 F-3 传送契约先求 `StepsBetween(35,0)=5` 再调 `AdvanceIndex(35,5)`,THEN `passed_go=1`、发薪一次。**此契约的 BLOCKING 测试归属事件格系统(7)的 Integration 测试**(棋盘数据无法在自身单测中验证调用方是否正确串联 F-3);本系统仅提供 F-3 与契约。追踪见 OQ-8。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| OQ-1 | `DA_Board_Classic40` 各格的**具体地价/租金表/造价/抵押值**最终填多少? | 经济现金(5) GDD 的公式 + 调参期产出;**现已 temp-fill 经典公开参考值(见 CR-6)解除阻塞**。**关闭条件(R2 硬门控):经济(5) GDD 须有对应 AC 验证 `DA_Board_Classic40` 数值来自经济公式推导而非手填,并将 DA 顶层 `bIsPlaceholderData` 置 `false`;`bIsPlaceholderData==true` 的棋盘不得进入 Alpha 构建(producer/gate-check 守门)。** 仅经济(5)覆盖并回链平衡依据后本 OQ 方可关闭 |
| ~~OQ-2~~ **(已解决)** | ~~过路费(经过 GO 发薪额)的数值存哪?~~ | **已解决:Go 格新增 `SalaryAmount` 字段(与 `TaxAmount` 对称,棋盘拥有金额)。`SalaryAmount` 由**移动(4)**读 `GetTileData(0).SalaryAmount`,连同 `passed_go` 传**经济(5)**结算发薪;**事件格(7)不读、不参与发薪**(见 CR-2 读取路径契约——发薪是"经过 GO"触发,属移动语义,非"落地某格"触发)。发薪权威是 F-2 `passed_go`(见 CR-2 契约)。具体数值由经济(5)调参填入。** *(R3 修订:旧措辞误写"事件格(7)读",与 R2 决策 CR-2/CR-3 矛盾,已改为移动(4)读传经济(5)。)* |
| OQ-3 | **IP 安全的自定义地块命名** —— `DisplayName` 用什么名字才不侵权? | 进入内容阶段前确认法律边界(concept Open Question 延续) |
| OQ-4 | `DisplayName` 是否需**本地化**(FText 本地化键)? MVP 是否做多语言? | localization-lead / `/localize`;MVP 暂可单语言,留 FText 接口 |
| OQ-5 | **FreeParking 角格**是否挂奖池等 House Rule? | 设置 & House Rules(23) GDD |
| **OQ-6 ⚠ BLOCKING-ADR** | 实现层选 **DataTable 还是 Primary Data Asset** 承载 `FBoardTileData`? | **必须在下游 #4/6/7/8 实现开始前**由 `/architecture-decision` 解决。两方案在 `GetTileData`/`GetBoardId` 机制、异步加载、CSV 导入(地图编辑器 26)、`[Config]` 测试 harness 形态上差异显著,下游硬依赖无法在其前安全开工。ADR 须满足/裁定:① `GetBoardId` 用 `BoardIdentifier` 字段与路径解耦;② 运行时 DA 持有者(推荐 `UGameInstanceSubsystem`)防 GC,**并定义热切换双缓冲/释放时机协议**(换盘走完整 Loaded→Validated→Active、不在 Active 对局中热替换,见 States)。**R3 补充(unreal):Subsystem `Initialize()` 内的异步加载 handle 必须在 `Deinitialize()` 显式 `CancelHandle`/`ReleaseHandle`,不依赖析构顺序——否则 PIE Stop 时 pending handle 与下次 Initialize 竞争,表现为偶发"第二次 PIE 棋盘数据为空",排查极难;**③ **AdvanceIndex 原子接口的最终 C++/Blueprint 函数签名,并裁定 F-1/F-2 是否封装为 `UBoardMathLibrary` 的 `BlueprintPure`**。**R3 补充(unreal):须明确 struct 返回(`FBoardAdvanceResult`,Blueprint Break Struct 拆包)vs out-param(`int32 AdvanceIndex(..., int32& OutPassedGo)`,Blueprint 双输出 pin,Epic 自身惯用法)这一维度——两者 Blueprint 作图体验不同,ADR 须选定而非含糊"元组 struct 形态";**④ 经济调参角度 DataTable(CSV 驱动)曾被认为更优,**但见下方 ⚠ TArray 知识缺口——此优势前提存疑**。⑤ **R3 补充:棋盘级元数据(`bIsPlaceholderData`/`BoardIdentifier`/`EColorGroup→hex` 色键表)的落地位置——DataTable 方案无"顶层"概念(资产是行集合,无头部字段),须另建包装 `UDataAsset` 持有;Primary DA 方案天然有顶层。且 `bIsPlaceholderData` 是 CI/构建门控语义,建议用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离,gate-check 在 cook 前检测而非运行时读取。** ⑥ **R3 补充:若 (a) 确认 CSV 不支持 `TArray<int32>` 列,地图编辑器(26)设计师如何输入 `RentTable` 的 6 个值(改 JSON 行导入?自定义编辑器 UI?)须一并裁定;FText `DisplayName` 的 localization key 控制(OQ-7)在 DataTable CSV 下无法用 NSLOCTEXT,与 OQ-7 建议存在张力,ADR 须处理。****⚠ R2 知识缺口(UE5.4-5.7,项目 engine-reference 无记录,ADR 前须人工查 UE5.7 文档核验):** (a) **DataTable CSV 导入是否支持 `TArray<int32>` 列**(`RentTable`/`DiceMultiplierTable` 是 TArray;经典 UE CSV 导入不支持 TArray 列,若 5.7 仍不支持则"CSV 驱动更优"论据失效、需改 JSON 行导入并重评地图编辑器导入方案);(b) DataTable 行内 `FText` 导入时 localization key 的自动生成行为(关联 OQ-4/OQ-7);(c) `FStreamableManager::RequestAsyncLoad`/`UAssetManager` 在 5.4+ 的 API 变化;(d) Blueprint `Floor` 节点 float/double 重载在 5.7 的类型推导规则。 |
| OQ-7 | `DisplayName` 用 inline FText 还是 String Table 引用?(影响未来本地化迁移成本) | localization-lead;MVP 单语言可 inline,但须知未来转 String Table 是破坏性数据迁移(unreal-specialist I-05)。**R2 缓解建议:即便 MVP 单语言,temp-fill 占位名应使用带命名空间的 `NSLOCTEXT("BoardData", "TileName_X", "...")` 而非裸字符串字面量;这样后续转 String Table 只需提取 key、不需修改 `.uasset` 二进制行(避免不可 diff 的 Git 冲突)。**R3 澄清(unreal-specialist):`NSLOCTEXT` 宏仅适用于 **C++ 侧硬编码默认值**(如 `FBoardTileData` 构造/CDO);若 `DisplayName` 由设计师在编辑器 Details 面板填写(inline FText)或 DataTable CSV 导入,则无法写 `NSLOCTEXT` 宏——此时 MVP 单语言用 inline FText,后续迁移须手动提 key 到 String Table(`.uasset` 会变动但可接受)。真正"迁移零成本"路径是从一开始即用 String Table 引用,但前期投入更大。具体取舍归 OQ-6 ADR(与 DataTable vs PrimaryDataAsset 方案耦合)。 |
| **OQ-8 ⚠ 跨系统测试追踪(R3 BLOCKING-收口,用户裁定升级)** | 外推给下游的 BLOCKING 测试(AC-38 归经济5、AC-39 归事件7、AC-28b 归存档21)如何保证下游真的实现,而非软承诺留真空? | **追踪机制(本 GDD 已写入 §AC 注脚):** 每条外推测试须在对应下游 GDD 的 §Acceptance Criteria 出现显式 AC 实现,并**回链本 GDD 的 AC 编号**(如经济5 的某 AC 注明"实现 board-data AC-38")。`/design-review` 审下游 GDD(经济5/事件7/存档21)时**核对此回链**,缺失则该下游 GDD **不得 APPROVED**。此 OQ 在三个下游 GDD 全部 APPROVED 且回链齐备后关闭。负责方:下游 GDD 作者 + design-review 守门。 |
