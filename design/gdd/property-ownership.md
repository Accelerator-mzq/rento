# 地产所有权 (Property Ownership)

> **Status**: In Design
> **Author**: msc + agents
> **Last Updated**: 2026-06-03
> **Implements Pillar**: ③运气×策略交织（垄断是策略骨干）、②社交博弈（地产归属=互坑/交易标的）
> **System**: #6 in systems-index design order | Core | Economy | MVP
> **Review Mode**: lean

## Overview

地产所有权系统是《骰子大亨》的**归属真值层**:它拥有"棋盘上每一格可购买地产现在归谁、是否已抵押、谁集齐了整组(垄断)"这一运行时事实。它有两个面:对内是一个**以 `TileIndex` 为键的运行时状态层**——持有 `TileIndex → ownerPlayerId` 归属 map 与每格 `is_mortgaged` 标记,并据棋盘(1)的色组/同类成员关系派生"是否垄断、持有几个车站/公用"等查询;对玩家则是**核心幻想的载体**——"这条街是我的、我集齐了整组、对手踩上来就得付钱给我"的地产帝国扩张感(支柱③策略骨干 / 支柱②互坑与交易的标的)。它**不拥有**现金(归经济5,买地/抵押的钱由经济 `Credit`/`Debit` 执行)、不拥有房屋建造规则(归建房8)、不拥有"买/不买"决策点(归回合2 的 `ResolvePhase`)、不拥有地价/租金/抵押额等静态数值(归棋盘1)、不拥有租金公式(归经济5);它只拥有**"谁拥有什么、是否抵押、是否垄断"这组归属事实**,并把它作为只读快照 **push 给经济(5)算租**(保依赖图无环)。它做得好时是隐形的正确——归属永远算得对、垄断判定永远准、已抵押的地永不被误收租——让玩家全部注意力投向"要不要买下这块凑齐整组、要不要抵押搏一把"的博弈本身。没有它,游戏就只剩掷骰移动而没有"地产"——无人能拥有任何东西,收租、垄断、交易、破产全部无从谈起。

## Player Fantasy

> *lean 模式:`creative-director` 未咨询(full 才派)。本节由主会话起草,生产前建议人工复核 framing。*

地产所有权兑现 concept 核心幻想最实在的那块——**"地产大亨"**:把一格格中性的棋盘格变成**你的**领土,集齐整组,看着对手一个个踩进你的收租陷阱。它服务两层体验:

- **直接层(掌控与扩张)**:每次落在无主地产,玩家面对"买不买"的抉择;买下后那格**变成你的颜色**,而真正的张力在**集齐整组**——"还差最后一块红组,值不值得抵押两块地凑钱拿下?"垄断是支柱③"运气×策略"里玩家**对抗运气的核心杠杆**:骰子决定你走到哪,但**买什么、何时集组、要不要抵押搏一把**是纯粹的算计(SDT Autonomy 主战场)。看着地产从零散连成垄断片区,是本系统最直接的成长快感(concept 雪球 Dynamics)。
- **基础层(归属的重量)**:玩家永不会想"这是一张 `TileIndex→owner` 的 map"。他们感受到的是**对手停在你满组地产上、现金哗啦转进你账户**的爽快,以及**自己踩进别人垄断区**的肉痛与"赶紧逃离这条街"的紧张(支柱②互坑的燃料)。归属必须有**重量**——"这条街归谁、有没有垄断、抵押没"要时刻清晰可感,收租与被收租才成为一桌人互坑的戏剧核心。

两层合起来制造 concept 点名的社交博弈:领先者**垄断收租雪球碾压**,落后者**抵押地产延命搏翻盘**。

> **〔MVP 范围诚实标注〕**:地产作为**交易/拍卖标的**(把地当筹码讨价还价)依赖交易(11)/拍卖(12),**全在 Alpha**;MVP 内本系统兑现的是**垄断收租雪球**与**抵押延命**两条主动线,"围绕同色组讨价还价"的 Dynamics 待 Alpha 上线。

> **MDA 锚点**:本系统主要服务 **Challenge/精通**(垄断布局的策略深度)与 **Fellowship/社交**(收租互坑、地产作交易标的),并驱动 concept 的雪球/翻盘 Dynamics。`Sensation`(地产变色/收租金币 juice)由 VFX(19)/HUD(16)/地产卡 UI(17) 承载,非本系统。

成功标准是**隐形的正确**:归属永远算得对、垄断判定永远准、抵押的地永不误收租——玩家从不怀疑"这块归谁、算没算翻倍",全部情绪投向"要不要拿下这块凑齐整组"的博弈本身。

## Detailed Design

### Core Rules

**CR-1 归属状态所有权(以 `TileIndex` 为键)。** 本系统拥有两份运行时可变状态,均以 `TileIndex` 为键、仅对可购买格(`Property`/`Railroad`/`Utility`)有效:① 归属 map `TileIndex → OwnerPlayerId`(无主 = `INDEX_NONE`);② 每格 `bIsMortgaged` 标记。开局全部无主、未抵押。经受控写接口改这两份状态,不直接暴露写字段(沿 player-turn AC-35 受控写契约,强度待生命周期 ADR)。本系统**不拥有**:现金(经济5)、房屋数 `house_count`(建房8)、买/不买决策点(回合2 `ResolvePhase`)、地价/抵押额静态值(棋盘1)。

**CR-2 买地登记(归属写,现金归经济)。** 玩家在无主可购买格选择购买时(决策点归回合2 `ResolvePhase`,现金扣减归经济5 CR-4),本系统经受控写 `RegisterPurchase(tileIndex, playerId)` 把该格 `owner` 置为 `playerId`。**前置**:该格可购买 且 当前无主(`owner==INDEX_NONE`);违反则拒绝登记(dev `ensure`)。**现金与归属的原子契约(6 拥有事务,调经济扣款)**:购买事务接口 `Buy(tileIndex, playerId)` 由回合2 `ResolvePhase` 决策点调用;本系统调经济5 `Debit(playerId, PurchasePrice)` 执行扣款(**6→5,economy 只执行不回调本系统**)→ 扣款成功后本系统登记 `owner`。顺序 `经济扣款成功 → 6 登记 owner`,二者要么都发生要么都不发生(防"扣了钱没拿到地"或"拿到地没扣钱")。本系统不持有价格、不执行扣款。**economy 不反向调本系统**(保 5↔6 无环;economy CR-4 现写「本系统…通知所有权(6)登记归属」是 5→6 反向,与 6→5 并存成环,须 propagate 修正为「现金扣减被 6 调用执行、归属登记由 6 完成」,见 Open Questions)。

**CR-3 抵押标记(标记归 6,事务调经济执行现金)。** 本系统拥有 `bIsMortgaged` 标记。抵押/赎回事务由回合2/AI 决策点调用本系统接口 `Mortgage(tileIndex)`/`Unmortgage(tileIndex)`;本系统调经济5 `Credit(拥有者, MortgageValue)`(抵押)/`Debit(拥有者, 赎回费)`(赎回,赎回费 = 经济 F-6)执行现金(**6→5**)→ 成功后本系统自置/清 `bIsMortgaged`。**economy 不反向调本系统**(同 CR-2,保无环;economy CR-5「通知所有权(6)标记」措辞须 propagate 修正)。**已抵押地产本系统报 `is_mortgaged=true`,经济 F-2 据此短路置租金 0**(经济拥有该短路)。**本系统不校验"带房不可抵押"**——`house_count==0` 前置由建房8/决策点在调用本系统之前校验(本系统读不到 `house_count`,见 CR-6 无环约束);本系统只对自己拥有的状态做 `owned 且 未抵押`(抵押)/`owned 且 已抵押`(赎回)入口 `ensure` 防御。

**CR-4 垄断判定(6 拥有,经济消费)。** 本系统计算 `is_monopoly(playerId, colorGroup)`:经棋盘1 `GetTilesInGroup(colorGroup) → [indices]` 取该组全部格,若 `playerId` 拥有**该组全部格**则 `is_monopoly=true`。**抵押不破坏垄断归属**:垄断 = 拥有整组(与各格是否抵押无关);某格抵押的收租后果由该格 `is_mortgaged` 短路(经济 F-2),其余未抵押无房格仍享 ×2(经典忠实:完整色组的未抵押无房地享双倍租,即便组内有地被抵押)。⚠ 此忠实细节待 OQ Rento 行为核查。本系统 depends-on 棋盘1 ✓,可调 `GetTilesInGroup`。

**CR-5 同类持有计数(6 拥有,经济消费)。** 本系统据 owner map + 棋盘1 `GetTileData(index).TileType` 计算:`station_count(playerId)` = 该玩家拥有的 `Railroad` 格数(全盘,非按组);`utility_count(playerId)` = 拥有的 `Utility` 格数。供经济 F-3/F-4 算租(经济守 `count≥1` 否则 0,不假设本系统恒正确)。

**CR-6 归属快照 push(保依赖图无环,economy OQ-T-Econ-1 对端)。** 本系统把自己拥有的归属事实组装成**只读值快照**(`owner` / `is_mortgaged` / `is_monopoly` / `station_count` / `utility_count`)push 给经济5 算租调用(本系统 depends-on 5 ✓,单向)。**`house_count` 不在本系统的快照内**——它归建房8,由 8 单独供;**本系统绝不读取 8 的 `house_count`**(否则 6→8 与既有 8→6 成环)。完整 rent 参数 = 6 的归属快照 + 8 的 `house_count`,二者在落地结算路径(回合2 `ResolvePhase` 编排)聚合后传经济。快照是值拷贝(非活引用),防结算期归属漂移。⚠ **propagate followup**:economy registry `property_rent` 现写"`house_count` 由所有权6供"措辞不精确,须修正为"6 供 owner/monopoly/mortgage/count,8 供 `house_count`"(见 Open Questions)。

**CR-7 职责边界(本系统不拥有什么)。** 本系统只产出"归属真值 + 抵押标记 + 派生垄断/计数"。不拥有:现金(经济5)、房屋数与建造规则(建房8)、买/不买/抵押/赎回的**决策点**(回合2 `ResolvePhase` / AI10)、地价/抵押额/色组成员静态数据(棋盘1)、租金公式(经济5)。决策由回合2/AI10 触发,现金后果由经济5 执行,本系统只登记归属结果。

### States and Transitions

**本系统主要是服务/状态层,非逐回合状态机**(同棋盘/经济)。它拥有的持久状态是**每格的归属与抵押**(以 `TileIndex` 为键),随存档(21)序列化。有意义的状态机是**每个可购买格的归属生命周期**:

| 状态 | 含义 | 转换 |
|---|---|---|
| **Unowned(无主)** | `owner==INDEX_NONE` | 玩家购买 → `RegisterPurchase`(CR-2)→ **Owned** |
| **Owned(已购·未抵押)** | `owner` 已置、`bIsMortgaged==false` | ① 抵押 → `SetMortgaged(true)` → **Mortgaged**;② 收租破产:此地 in-kind 转债权人(经济 F-11)→ owner 改、**留 Owned**;③ 银行破产:回退 → **Unowned**(经济 F-11) |
| **Mortgaged(已抵押)** | `owner` 已置、`bIsMortgaged==true` | ① 赎回 → `SetMortgaged(false)` → **Owned**;② 收租破产 in-kind 转债权人 → owner 改、**`bIsMortgaged` 保持 true**(经济 F-11 保留抵押状态)→ 仍 **Mortgaged**(新主);③ 银行破产 → 回退 **Unowned**(标记清除) |

> **关键不变式**:Owned/Mortgaged 状态恒有合法 owner(`!=INDEX_NONE`);Unowned 恒 `bIsMortgaged==false`(无主不可能抵押)。破产移交(改 owner)与抵押标记(改 `bIsMortgaged`)都经受控写,由经济5/破产9 通知触发,本系统不自决破产/抵押,只执行归属/标记的状态转换。

> **垄断/计数是派生态、非存储态**:`is_monopoly`/`station_count`/`utility_count` 不作为字段存储,而是每次查询时从 owner map + 棋盘1 实时派生(CR-4/CR-5),避免"买地/破产改 owner 后忘同步垄断标记"的脏状态。存档只存 owner map + `bIsMortgaged`,派生量读档后重算。

> **序列化**:owner map(每可购买格一 `PlayerId`)+ `bIsMortgaged`(每可购买格一 `bool`)随存档(21)序列化(以 `TileIndex` 为键)。本系统无回合内瞬态、无中途存档点问题。

### Interactions with Other Systems

| 系统 | 数据流 | 谁拥有 |
|---|---|---|
| 棋盘数据(1) | 读 `GetTileData(index)` 的 `TileType`/`ColorGroup`/`PurchasePrice`/`MortgageValue` + `GetTilesInGroup(group)→[indices]` 算垄断/计数 | 棋盘供静态数据;本系统拥有运行时归属 |
| 经济(5) | **本系统调** `Debit`/`Credit` 执行买地/抵押/赎回现金(**6→5**);**本系统 push 归属快照**(`owner`/`is_mortgaged`/`is_monopoly`/`station_count`/`utility_count`)供经济算租(F-2/3/4)。**经济不反向调本系统** | 经济拥有现金与公式;本系统拥有归属事实 |
| 玩家回合(2) | `ResolvePhase` 决策点调本系统 `Buy`(无主地产买决策);落地结算编排聚合 6 快照 + 8 `house_count` 传经济 | 回合拥有流程/决策点;本系统执行归属登记 |
| 建房(8) | **8 读本系统** `owner`/`is_monopoly`/`is_mortgaged`(判可否建房:须垄断、未抵押)(**8→6**);8 拥有 `house_count` 与建造规则,**本系统不读 8**(防 6↔8 环) | 8 拥有房屋数/建造规则;本系统供归属/垄断/抵押 |
| 破产胜负(9) | 9 裁决破产后**调本系统**改 owner(**9→6**):收租破产 → 债务人全部地 in-kind 转债权人(保留 `bIsMortgaged`);银行破产 → 回退无主(清标记)。economy F-11 只执行**现金侧**转移 | 9 拥有破产裁决;本系统执行归属转移 |
| AI(10) | 读归属/垄断/计数(经快照/查询)做买/建/抵押决策估值 | AI 拥有决策;本系统供归属事实 |
| 地产卡 UI(17) | 读 `owner`/`is_mortgaged`/`is_monopoly` 呈现地产卡归属角标/抵押态/垄断高亮 | UI 拥有呈现 |
| HUD(16) | 读各玩家持有地产/垄断概览(资产面板) | HUD 拥有呈现 |
| 交易(11)/拍卖(12)(Alpha) | 经本系统受控写转移 owner(交易换手/拍卖中标登记) | 11/12 拥有谈判/出价;本系统执行归属写 |
| 存档(21) | 序列化 owner map + `bIsMortgaged`(以 `TileIndex` 为键) | 存档拥有序列化 |

**事件(本系统广播,`DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`,payload `USTRUCT(BlueprintType)`)**:
- `OnOwnershipChanged(TileIndex, OldOwner, NewOwner)` — 归属变更(买地/破产移交/交易/拍卖)供 UI/HUD/VFX/AI 挂接。
- `OnMortgageChanged(TileIndex, bIsMortgaged)` — 抵押/赎回标记变更。

**接口稳定承诺**:`Buy`/`Mortgage`/`Unmortgage`/`GetOwner`/`IsMortgaged`/`IsMonopoly`/`GetStationCount`/`GetUtilityCount`/快照接口签名稳定;事件 payload 只增不改语义。给 8 个下游(8/9/10/11/12/16/17/21)的稳定保证。

**禁绕过受控写**:任何系统不得直接改 owner map / `bIsMortgaged`,只经本系统受控接口(沿 player-turn AC-35 软/硬约束)。

> **⚠ 双向一致性 / propagate 待办(均登记 Open Questions):**
> - **economy CR-4/CR-5/F-11 措辞**:现写"经济通知6登记/标记""经济执行资产转移"= 5→6 反向,与 6→5(快照 push)并存成 5↔6 环 → 须 propagate 改为"现金腿被 6 调用执行 / 资产 owner 写由破产9 经 6 执行,economy 只管现金值侧"。
> - **systems-index 依赖列**:`9 depends-on` 须补 **6**(9→6 破产归属移交);`16 depends-on` 若 HUD 直接读归属须补 **6**(现仅 1,2,5)。`8→6`/`17→6`/`21→6` 已在 index,无需补。

## Formulas

> *由 `systems-designer`(lean 模式 D 节高风险派发)校验严谨性。* **本系统无平衡公式**——地价/租金/抵押额归棋盘(1)/经济(5);本系统只持 3 条从 owner map 派生的谓词/计数(布尔/整数,无平衡数值)。

### F-1 `is_monopoly(playerId, colorGroup)` — 是否集齐整色组

```
if playerId == INDEX_NONE: return false
if colorGroup == None:     return false
tiles = GetTilesInGroup(colorGroup)          // board 按 TileIndex 升序返回
if |tiles| == 0:           return false       // 空集悖论守门:不得 vacuous-true
for t in tiles: if owner_map[t] != playerId: return false
return true
```

| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 查询玩家 | `playerId` | int32 | [0,P-1] ∪ {INDEX_NONE=-1} | -1 哨兵早出 |
| 色组 | `colorGroup` | EColorGroup | 8 色组 ∪ {None} | Property 专属 |
| 组内格集 | `tiles` | [TileIndex] | 长度经典 2~3 | `GetTilesInGroup` 返回 |

**Output Range:** `bool`。**抵押状态不影响结果**(垄断 = 拥有整组;某格抵押的收租后果由该格 `is_mortgaged` 短路,经济 F-2)。
**Example**(经典红组 = TileIndex 21/23/24):三格全归 player2 → `true`;有一格 `INDEX_NONE`(未买)或他人持有 → `false`;`colorGroup==None` → `false`。
**⚠ 空集守门(systems-designer B-5)**:C++ `std::all_of` 对空 range 返回 `true`,**必须**先加 `|tiles|==0 → false` 守门,否则"集齐零格的空组"被误判垄断、令经济 F-2 对该格误翻倍。

### F-2 `station_count(playerId)` — 拥有的 Railroad 格数(全盘)

```
if playerId == INDEX_NONE: return 0
return count{ t in all_tiles | GetTileData(t).TileType == Railroad AND owner_map[t] == playerId }
```

| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 查询玩家 | `playerId` | int32 | [0,P-1] ∪ {-1} | -1 早出返 0 |
| 结果 | `station_count` | int32 | 经典 [0,4];自定义 [0,N-1] | |

**Output Range:** [0,4](经典 Railroad 固定 4 格)。无主格 `owner_map[t]==INDEX_NONE ≠ 合法 playerId` 由严格等值比较自然排除;无 int32 溢出(`count ≤ N-1`)。
**Example**(Railroad = TileIndex 5/15/25/35):player1 持 5/15 → `2`;全持 → `4`;`INDEX_NONE` → `0`。

### F-3 `utility_count(playerId)` — 拥有的 Utility 格数(全盘)

结构同 F-2,`TileType == Utility`:
```
if playerId == INDEX_NONE: return 0
return count{ t in all_tiles | GetTileData(t).TileType == Utility AND owner_map[t] == playerId }
```

**Output Range:** 经典 [0,2](Utility = TileIndex 12/28)。
**Example**:player0 持 12/28 → `2`;持 12 → `1`;全无主 → `0`。

> **守门职责分层(systems-designer B-7)**:经济 F-3/F-4 守 `count≥1 否则 0` 只防"返回 0 不崩";本系统须保证**不低报**(玩家实持 2 站绝不返回 1,否则租金算低)——两层守门职责不同,不可互替。
> **确定性(B-6)**:3 谓词均纯整数/集合,无浮点、无随机;owner map 快照输入 → 输出确定(联网回放安全),不持内部缓存。

## Edge Cases

**购买边界**
- **若买已有主格**:拒绝(CR-2 前置 `owner==INDEX_NONE`),dev `ensure`。
- **若买不可购买格**(Chance/Tax/角格等):拒绝(仅 Property/Railroad/Utility 可登记 owner)。
- **若经济 `Debit` 失败(现金不足)**:购买事务整体不发生——不登记 owner、不扣款(CR-2 原子契约;现金不足时购买不可用,经济 CR-4)。

**抵押边界**
- **若抵押已抵押格 / 赎回未抵押格**:拒绝(CR-3 入口 `ensure` 校验 owned 且 状态匹配)。
- **若抵押带房地产**:本系统**不校验 `house_count`**(读不到,防 6↔8 环);`house_count==0` 前置由建房8/决策点在调本系统 `Mortgage` 前保证。若被错误调用(带房抵押),属上游 bug,本系统照置标记但 dev 日志(无法自验)——职责边界:防带房抵押归调用方,本系统只防 owned/已抵押态。
- **若赎回经济 `Debit` 失败(付不起赎回费)**:赎回事务不发生,标记保持 mortgaged(经济 CR-5 现金不足无法赎回)。

**垄断/计数边界(systems-designer B-1/B-2/B-5)**
- **若 `colorGroup==None` / 空组**:`is_monopoly → false`(B-1 + 空集守门 B-5,不得 vacuous-true)。
- **若 `playerId==INDEX_NONE` 传入任一谓词**:早出 `false`/`0`(B-2,防把无主格累计进"INDEX_NONE 玩家")。
- **无主格不被误计入任何玩家 count**(B-3,严格等值比较 `owner_map[t]==playerId` 自然排除)。
- **count 整型溢出**:经典不可能(≤4/≤2);自定义棋盘 `count≤N-1<INT32_MAX` 无溢出;建议棋盘加载加软上界告警 `Railroad>4`/`Utility>2`(B-4,数据异常检测非拒绝)。

**破产移交边界**
- **若收租破产**:债务人全部地(含已抵押)in-kind 转债权人,`bIsMortgaged` 保持(经济 F-11),新主继承已抵押地赎回义务。
- **若银行破产**:债务人全部地回退 `owner=INDEX_NONE`、`bIsMortgaged` 清零(守 CR-1 不变式:无主不可抵押)。
- **移交后垄断/计数自动重算**(派生态,无需手动同步,见 States and Transitions)。

**确定性 / 数据完整性**
- **计数低报是严重 bug(B-7)**:本系统须保证不低报(实持 2 站绝不返 1);经济守门(`count≥1 否则 0`)只防不崩、不防低报。
- 3 谓词纯整数/集合、确定性(B-6),无浮点无随机。

## Dependencies

### 上游(本系统依赖)
| 系统 | 强度 | 接口 |
|---|---|---|
| 棋盘数据(1) | 硬 | `GetTileData(index)`(`TileType`/`ColorGroup`/`PurchasePrice`/`MortgageValue`)、`GetTilesInGroup(group)→[indices]` |
| 经济(5) | 硬 | 本系统调 `Credit`/`Debit` 执行买地/抵押/赎回现金(6→5);本系统 push 归属快照供经济算租(经济不反向调本系统) |

### 下游(依赖本系统)
| 系统 | 强度 | 读/写本系统 |
|---|---|---|
| 建房(8) | 硬 | 读 `owner`/`is_monopoly`/`is_mortgaged` 判可建房;8 拥有 `house_count`(本系统不读 8) |
| 破产胜负(9) | 硬 | 破产裁决后调本系统改 owner(收租转债权人 / 银行回退无主) |
| AI(10) | 硬 | 读归属/垄断/计数估值买/建/抵押决策 |
| 地产卡 UI(17) | 硬 | 读 `owner`/`is_mortgaged`/`is_monopoly` 呈现归属/抵押/垄断 |
| 存档(21) | 硬 | 序列化 owner map + `bIsMortgaged` |
| HUD(16) | 软 | 读各玩家持有地产/垄断概览(资产面板) |
| 交易(11)/拍卖(12) | 软(Alpha) | 经受控写转移 owner(换手/中标) |
| 教程(24) | 软(Alpha) | 引导买地/集组示例 |

> **双向一致性(待 systems-index 同步,Phase 5 / propagate):**
> - `9 depends-on` 须补 **6**(9→6 破产归属移交);现 index 仅 2,5。
> - `16 depends-on` 若 HUD 直接读归属须补 **6**;现仅 1,2,5。
> - `8→6` / `17→6` / `21→6` 已在 index ✓。
> - **economy CR-4/CR-5/F-11 反向调用措辞**须 propagate 修正(见 Interactions + Open Questions),否则 5↔6 成环破坏 index"无环"声明。

## Tuning Knobs

> **本系统无自有调参旋钮。** 地产所有权是归属状态层,不引入任何影响平衡/玩法的数值。下表全部**指向真值来源**,不在本系统重复定义:

| 旋钮 | 作用 | 安全范围 | 归属 |
|---|---|---|---|
| `monopoly_rent_multiplier` | 垄断翻倍(影响 `is_monopoly` 的收租后果) | MVP 锁定 **2** | **经济(5)**;本系统只供 `is_monopoly`、不持系数 |
| `PurchasePrice` / `MortgageValue`(每格) | 买地/抵押额(影响事务现金腿) | `>0`、`MortgageValue ≤ PurchasePrice` | **棋盘(1)** base / **经济(5)** 平衡;本系统不持 |
| 色组构成(布局) | 哪些格属哪组(影响垄断判定边界) | 经典锁定;编辑器自由 | **棋盘(1)** CR-5/CR-6 |

> 本系统设计上刻意无旋钮——归属是确定性事实,买地/抵押/垄断的**数值后果**全由经济(5)/棋盘(1)的旋钮决定,本系统只忠实登记归属并把事实供下游。

## Visual/Audio Requirements

**n/a —— 本系统是归属状态层,不渲染视觉、不触发音频。** 但地产归属的可见时刻——地产变色(被买下时格子染上拥有者颜色)、垄断高亮、抵押灰显/标记、破产清算时地产易主——是核心循环的关键反馈,由 VFX(19)/HUD(16)/地产卡 UI(17)/音频(22) 拥有。本系统为其提供数据与事件,钉最低呈现契约:

- **端到端 owner = VFX(19)/HUD(16)/地产卡 UI(17)**:地产变色动画、垄断高亮、抵押态呈现、易主反馈 juice。具体动画/时长归呈现层。
- **本系统义务边界**:提供 `OnOwnershipChanged(TileIndex, OldOwner, NewOwner)` + `OnMortgageChanged(TileIndex, bIsMortgaged)` + 归属/垄断/抵押查询;结果在动画开始前已产出(归属是确定性事实),呈现层只回放既定结果,不影响归属逻辑。
- **好/坏语境**:`OnOwnershipChanged` 携 old/new owner,呈现层据此区分"买下(获得感)"vs"破产易主(清算)";`is_monopoly` 查询供垄断高亮。
- **回链**:登记 systems-index 继承义务表(VFX19 / 地产卡 UI17 行)。

> 📌 **Asset Spec**:art bible 批准后,可运行 `/asset-spec system:property-ownership` 产出地产归属角标/垄断高亮/抵押态的视觉规格(若需要)。

## UI Requirements

**n/a —— 本系统无任何 UI。** 它不绘制屏幕元素,只暴露归属/抵押/垄断查询 + 受控写接口(`Buy`/`Mortgage`/`Unmortgage`)+ 2 个事件。地产归属的呈现——地产卡归属角标、抵押态、垄断高亮、玩家资产面板——由 **地产卡 UI(17)** 与 **HUD(16)** 实现;买地/抵押/赎回按钮的输入路由由 HUD/输入层处理后回调本系统接口。本系统的"接口"是数据契约与事件,不是界面。

> 📌 **UX Flag —— 地产所有权**:地产卡归属呈现(角标/抵押/垄断高亮)、玩家资产面板须在 Pre-Production 由 `/ux-design` 为相关 HUD/卡牌屏出规格,再交 `/team-ui`。Stories 引用 `design/ux/[screen].md`,非本 GDD。

## Acceptance Criteria

> *由 `qa-lead`(lean 模式 H 节高风险派发)产出。* **测试分层(对齐 economy/movement/player-turn):** `[Logic]` 纯 fixture struct、确定性整数、headless `-nullrhi` 可跑、BLOCKING PR merge gate(`tests/unit/property-ownership/`);`[Advisory]` 集成/code-review/跨进程,非门控。
> **核心原则**:本系统无 RNG、无平衡数值,全是逻辑断言——"给定 owner map + 输入,谓词/状态转换算得对"。受控写软约束在 BP-only 下仅 `[Advisory]` code-review,C++ 强封装可升 `[Logic]`(沿 economy AC-4 / movement AC-3 生命周期 ADR)。

### A. 核心规则——owner map & 受控写(CR-1/CR-7)
- **AC-1** [Logic] 初始状态:GIVEN 空局初始化,WHEN 读任意可购买格 owner,THEN `owner==INDEX_NONE` ∧ `bIsMortgaged==false`。
- **AC-2** [Logic] 无主格守门:GIVEN 可购买格 `owner==INDEX_NONE`,WHEN `GetOwner`/`IsMortgaged`,THEN 返回 `INDEX_NONE`/`false`,不崩、不触发事件。
- **AC-3** [Advisory·code-review] 受控写软约束:全代码库无外部系统直写 owner map / `bIsMortgaged`,只经受控入口。强度坦诚:BP-primary → code-review;C++ 强封装可升 [Logic] 静态扫描(OQ-Prop-1 ADR)。
- **AC-4** [Logic] 非可购买格不进 owner map:GIVEN `TileType ∈ {Go/Tax/Jail/FreeParking/Chance/CommunityChest}`,WHEN `GetOwner(tile)`,THEN `INDEX_NONE` + dev ensure。

### B. 买地登记事务(CR-2)
- **AC-5** [Logic] 正常买地全路径:GIVEN tile 属 Property、`owner==INDEX_NONE`、Cash≥price(economy Debit 预判通过),WHEN 经济 Debit 成功后调 `RegisterPurchase(tile,player)`,THEN `GetOwner==player` ∧ `bIsMortgaged==false` ∧ `OnOwnershipChanged(tile,INDEX_NONE,player)` 恰 1 次。
- **AC-6** [Logic] 前置守门——只能买无主格:GIVEN `owner!=INDEX_NONE`,WHEN `RegisterPurchase`,THEN 拒绝、owner 不变、无广播、dev ensure。
- **AC-7** [Advisory·code-review·架构方向] economy 不反向调 6:spy 确认买地路径是 **6 调 economy.Debit**,economy `Credit`/`Debit` 内部**不调** `RegisterPurchase`(防 5↔6 环)。BP 下难精细 spy → [Advisory];C++ 编排层可升 [Logic]。沿 economy OQ-Econ-1/OQ-Prop-1。
- **AC-8** [Logic] 原子性——同成同败:GIVEN economy Debit 成功 → `owner==buyer`(无"扣钱没地");GIVEN Debit 失败(现金不足)→ `owner` 仍 `INDEX_NONE`(无"有地没扣钱")。两子例独立 fixture,mock economy 注入成功/失败,不依赖真实 Cash 值。

### C. 抵押标记(CR-3)
- **AC-9** [Logic] 正常抵押:GIVEN owner==player、`bIsMortgaged==false`、house_count==0(由建房8 保证,6 不重检),WHEN 经济 Credit 成功后调 `RegisterMortgage`,THEN `bIsMortgaged==true` ∧ `OnMortgageChanged(tile,true)` 恰 1 次。
- **AC-10** [Logic] 重复抵押守门:GIVEN `bIsMortgaged==true`,WHEN `RegisterMortgage`,THEN 拒绝、不变、无广播、dev ensure。
- **AC-11** [Logic] 正常赎回:GIVEN `bIsMortgaged==true`,WHEN 经济 Debit 成功后调 `RegisterUnmortgage`,THEN `bIsMortgaged==false` ∧ `OnMortgageChanged(tile,false)` 恰 1 次。
- **AC-12** [Logic] 赎回守门——非抵押态:GIVEN `bIsMortgaged==false`,WHEN `RegisterUnmortgage`,THEN 拒绝、不变、无广播、dev ensure。
- **AC-13** [Logic] 入口守门——只能 owner 操作:GIVEN `owner==playerA`,WHEN `RegisterMortgage(tile, playerB)`,THEN 拒绝、不变、dev ensure。
- **AC-14** [Advisory·code-review] 6 不校验 house_count:`RegisterMortgage` 实现内无 `house_count>0` 判断(防职责蠕变,该前置归建房8)。

### D. 垄断判定(CR-4 + F-1)
- **AC-15** [Logic] 垄断正例:GIVEN 红组 {21,23,24} 全 `owner==playerId`,WHEN `IsMonopoly(playerId, RedGroup)`,THEN `true`。
- **AC-16** [Logic] 他人破坏:GIVEN {21,23}==playerId、{24}==other,THEN `false`。
- **AC-17** [Logic] INDEX_NONE 破坏:GIVEN {21,23}==playerId、{24}==`INDEX_NONE`,THEN `false`。
- **AC-18** [Logic·关键] 抵押不破坏垄断:GIVEN 红组全归 playerId、其中 21 `bIsMortgaged==true`,WHEN `IsMonopoly`,THEN `true`(租金=0 由 economy 管,本系统只报归属)。
- **AC-19** [Logic] `colorGroup==None` 早出:THEN `false`(不进 vacuous-true,B-5)。
- **AC-20** [Logic·关键空集守门 B-1/B-5] 空组 vacuous-true 防护:GIVEN colorGroup 无任何登记 TileIndex,WHEN `IsMonopoly`,THEN `false`。fixture 直接断言"空集不得返回 true",不得降 Advisory。
- **AC-21** [Logic] `playerId==INDEX_NONE` 早出 false(B-2):不查 map。

### E. 计数派生(CR-5 + F-2 + F-3)
- **AC-22** [Logic] `StationCount` 基础:Railroad {5,15,25,35} 中 {5,15}==playerId → `2`。
- **AC-23** [Logic] `StationCount` 全持:{5,15,25,35} 全持 → `4`(不溢出 B-4)。
- **AC-24** [Logic] `StationCount(INDEX_NONE)` → `0`(不查 map,B-2)。
- **AC-25** [Logic] 非 Railroad 不计:持 tile 1(Property)+ 5(Railroad)→ `StationCount==1`(B-3)。
- **AC-26** [Logic] `UtilityCount` 基础:Utility {12,28} 中 {12}==playerId → `1`。
- **AC-27** [Logic] `UtilityCount` 全持:{12,28} 全持 → `2`(上限 2,B-4)。
- **AC-28** [Logic] `UtilityCount(INDEX_NONE)` → `0`。
- **AC-29** [Logic·关键] 计数不低报(B-7):持全 4 站 + 2 公用 → 严格 ==4/==2;fixture 遍历棋盘 TileType 驱动,不硬传 count 绕过枚举。

### F. 归属快照 push(CR-6 + economy OQ-T-Econ-1)
- **AC-30** [Logic] 快照字段完整性:GIVEN owner map 已配,WHEN `BuildOwnershipSnapshot(P1)`,THEN snapshot 含 `owner`/`is_mortgaged`/`is_monopoly`/`station_count`/`utility_count` 且值正确;**`house_count` 不在 snapshot 内**(归建房8)。
- **AC-31** [Advisory·code-review] push 方向:snapshot 由 6 push 给 economy 租金入口(economy 不反查 6 字段)。BP 下 [Advisory];C++ 可升 [Logic]。对齐 economy OQ-T-Econ-1。
- **AC-32** [Advisory·code-review] 6 不读建房(8):spy 对系统#8 调用次数==0(快照不含 house_count)。C++ 可升 [Logic]。

### G. 破产移交(CR-1 / economy F-11,触发方=系统#9)
- **AC-33** [Logic] 收租破产 in-kind——保留抵押:GIVEN debtor 持 A(mortgaged)+B(未抵押),9 调 `TransferOwnership(tile, debtor→creditor)`,THEN `GetOwner(A)==creditor` ∧ `IsMortgaged(A)==true`(随地转移不清)∧ B 同转、未抵押保持。
- **AC-34** [Logic] 银行破产回退:GIVEN 债权方==Bank,9 调 `ReturnTileToBank(tile)`,THEN `owner==INDEX_NONE` ∧ `bIsMortgaged==false`(清标记)。
- **AC-35** [Advisory·code-review] 移交由系统#9 触发:spy `TransferOwnership` 调用方是 9 编排路径,**economy 不直接调**(地产侧归 9→6 链,economy 只管 Cash 侧)。

### H. 职责边界(CR-7,均 [Advisory·code-review])
- **AC-36** 6 不持 Cash(实现内无 `PlayerState.Cash` 读写)。
- **AC-37** 6 不持决策点(`Register*` 入口无"是否应该买/抵押"策略判断)。
- **AC-38** 6 不持静态棋盘数据(colorGroup 成员/TileType 来自棋盘1 `GetTileData`,不硬编码组成员)。
- **AC-39** 6 不持租金公式(谓词只返归属,无 RentTable 查找/乘法)。

### I. 确定性 / 整数纯净(B-6)
- **AC-40** [Logic·确定性] owner map 全整数操作(键值 int32,无 float);固定 fixture 连调 `IsMonopoly`/`StationCount`/`UtilityCount` 各两次,位级一致、不受帧/时序影响。

### J. 接口稳定性(对齐 economy AC-38/39 / movement AC-20/21)
- **AC-41** [Logic·CI 构建] 公共接口标 `UFUNCTION(BlueprintCallable)`、`OnOwnershipChanged`/`OnMortgageChanged` 标 `BlueprintAssignable`、payload `USTRUCT(BlueprintType)`、编译通过(headless CI `-nullrhi`)。

> **[Logic] gate(BLOCKING PR)= 31 条;[Advisory·code-review]= 10 条(AC-3/7/14/31/32/35/36/37/38/39);总计 41 条。**
> **CR/F/B 覆盖对照:** CR-1→1/2/3/4;CR-2→5/6/7/8;CR-3→9/10/11/12/13/14;CR-4→15~21;CR-5→22~29;CR-6→30/31/32;CR-7→36/37/38/39;F-1→15~21;F-2→22~25/29;F-3→26~29;B-1→20;B-2→21/24/28;B-3→25;B-4→23/27;B-5→19/20;B-6→40;B-7→29;破产保留抵押→33;银行回退→34;5↔6 无环→7/31/35。**CR-1~7 / F-1~3 / B-1~7 全覆盖。**

### 跨系统测试义务(OQ-T-Prop-*,回链 systems-index 继承义务表)
- **OQ-T-Prop-1 → 经济(5)**:6 向 economy 提供 `is_monopoly`/`station_count`/`utility_count`/`is_mortgaged` 快照,对齐 economy AC-9(抵押短路)/AC-15(station guard)/AC-17(utility guard);economy guard 只防竞态残值 0,6 的 AC-22~24/29 保证不低报。
- **OQ-T-Prop-2 → 存档(21)**:21 序列化完整 owner map + `bIsMortgaged`;读档后三谓词结果与存档前一致。
- **OQ-T-Prop-3 → 建房(8)**:8 须在调 `RegisterMortgage` 前完成卖房(house_count→0),此前置由 8 的 AC 断言;6 的 AC-9 假设 house_count==0 已由 8 保证,不重测。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-Prop-1 ⚠ ADR** | owner map 容器形态(`TMap<int32,int32>` vs 以 TileIndex 索引的 `TArray`)+ 受控写封装强度。与 player-turn OQ-1 / board OQ-6 / economy OQ-Econ-1 / movement OQ-Move-3 **协调同一生命周期层 ADR**。选 C++ 强封装(owner map private + 受控访问器)→ AC-3/AC-7/AC-31/AC-32 升 [Logic] 静态扫描;BP 约定 → 软约束 code-review。 | 下游 8/9 实现开始前 `/architecture-decision` |
| **OQ-Prop-2 🔴 propagate** | economy CR-4/CR-5/F-11 的反向调用措辞("经济…通知所有权6登记/标记""经济执行资产转移")= **5→6**,与本档 6→5(快照 push,CR-6)并存 → **5↔6 环**,破坏 systems-index"无环"声明 + economy OQ-T-Econ-1 push 模型本意。须 `/propagate-design-change` 修正为"现金腿被 6 调用执行 / 破产 owner 写由破产9 经 6 执行,economy 只管现金值侧"。本档不单方改 economy。 | producer 协调 propagate(本档推 Approved 前或同批) |
| **OQ-Prop-3 propagate** | economy registry `property_rent` 的 push 清单写"`house_count` 由所有权6供"措辞不精确——`house_count` 归建房8,6 不供(否则 6↔8 环)。须修正为"6 供 owner/monopoly/mortgage/count,8 供 `house_count`"。 | producer 协调 propagate(与 OQ-Prop-2 同批) |
| **OQ-Prop-4** | systems-index 依赖列:`9 depends-on` 须补 **6**(9→6 破产归属移交);`16 depends-on` 若 HUD 直接读归属须补 **6**(现仅 1,2,5)。`8→6`/`17→6`/`21→6` 已在 index。 | Phase 5 / propagate 同步 |
| **OQ-Prop-5** | `is_monopoly` 的"抵押不破坏垄断归属"(完整色组的未抵押无房地享 ×2,即便组内有地被抵押)是经典大富翁规则;**Rento Fortune 实际行为待事实核查**(类比 board OQ-3 / movement OQ-Move-1 的 Rento 核查项)。MVP 暂取经典忠实(AC-18),核查后回填。 | MVP 设计冻结前核查 |
