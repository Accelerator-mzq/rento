# 地产所有权 (Property Ownership)

> **Status**: NEEDS REVISION → R1 Group A 修(8 BLOCKING)+ Group B propagate 闭合(2026-06-03)→ **R2 re-review(2026-06-03,full:economy/systems/game/qa/unreal + CD):2 hard blocker 已修** — ① AC-14b 重写(消"6 检测读不到的 house_count"逻辑不可实现,qa/economy/systems 三方 converge);② **OQ-Prop-7** economy F-5/292/AC-20 propagate 残留清理(Group B 第二代漏列,fresh-grep 双向核实 0 残留)+ 快改 REC 批(creditor 守门 AC-33e / 重入锁无条件解锁 / AC-11b 赎回失败 / AC-30 负断言 / dev-ensure build 注 / PROP-013 CI-stub / `TArray<bool>`→`uint8` 措辞软化 + unreal Issue1-7 折入 OQ-Prop-1 ADR)。**残留(非 GDD 完整性 blocker,deferred):OQ-Prop-5(Rento 抵押-垄断核查,冻结前)/ OQ-Prop-1(生命周期 ADR,下游8/9 前)。** → **✅ APPROVED-with-followups(用户裁定 2026-06-03)**;deferred followup 不阻下游开工,Player Fantasy 结构重构等 REC 留生产前批。
> **Author**: msc + agents
> **Last Updated**: 2026-06-03
> **Implements Pillar**: ③运气×策略交织（垄断是策略骨干）、②社交博弈（地产归属=互坑/交易标的）
> **System**: #6 in systems-index design order | Core | Economy | MVP
> **Review Mode**: lean（design-review 以 full 跑:economy/systems/game/qa/unreal + CD 综合,2026-06-03）

> **R1 修订摘要(design-review full,2026-06-03)**:本档 Group A 8 条 BLOCKING 已修——① 买地原子性前置前置化(先验 owner==NONE 再 Debit,消 2PC);② 公开接口统一 `Buy/Mortgage/Unmortgage`(+playerId),删 `Register*` 混名;③ 破产级联逐格广播 + 重入锁;④ 状态机不变式/批量原子性/快照隔离/push 时机补 AC;⑤ AC 可测性重写(AC-5/7/9/13/14/29/30/33/40);⑥ OQ-Prop-5 升 BLOCKING 实现前 Rento 核查;⑦ 容器 framing 纠正(TArray 优于 TMap,真决策归 OQ-Prop-1 ADR);⑧ ADR 未决时 AC-3/7/31/32 默认最严 [Logic](风险控制方向修正)。**Group B(P1–P5)是跨 Approved 邻居的真矛盾,须 producer `/propagate-design-change` + 逐档 fresh-grep,非本档单方改。**

## Overview

地产所有权系统是《骰子大亨》的**归属真值层**:它拥有"棋盘上每一格可购买地产现在归谁、是否已抵押、谁集齐了整组(垄断)"这一运行时事实。它有两个面:对内是一个**以 `TileIndex` 为键的运行时状态层**——持有 `TileIndex → ownerPlayerId` 归属 map 与每格 `is_mortgaged` 标记,并据棋盘(1)的色组/同类成员关系派生"是否垄断、持有几个车站/公用"等查询;对玩家则是**核心幻想的载体**——"这条街是我的、我集齐了整组、对手踩上来就得付钱给我"的地产帝国扩张感(支柱③策略骨干 / 支柱②互坑与交易的标的)。它**不拥有**现金(归经济5,买地/抵押的钱由经济 `Credit`/`Debit` 执行)、不拥有房屋建造规则(归建房8)、不拥有"买/不买"决策点(归回合2 的 `ResolvePhase`)、不拥有地价/租金/抵押额等静态数值(归棋盘1)、不拥有租金公式(归经济5);它只拥有**"谁拥有什么、是否抵押、是否垄断"这组归属事实**,并把它作为只读快照 **push 给经济(5)算租**(保依赖图无环)。它做得好时是隐形的正确——归属永远算得对、垄断判定永远准、已抵押的地永不被误收租——让玩家全部注意力投向"要不要买下这块凑齐整组、要不要抵押搏一把"的博弈本身。没有它,游戏就只剩掷骰移动而没有"地产"——无人能拥有任何东西,收租、垄断、交易、破产全部无从谈起。

## Player Fantasy

> *lean 模式:`creative-director` 未咨询(full 才派)。本节由主会话起草,生产前建议人工复核 framing。*

地产所有权兑现 concept 核心幻想最实在的那块——**"地产大亨"**:把一格格中性的棋盘格变成**你的**领土,集齐整组,看着对手一个个踩进你的收租陷阱。它服务两层体验:

- **直接层(掌控与扩张)**:每次落在无主地产,玩家面对"买不买"的抉择;买下后**本系统把该格归属变更为你**(VFX(19) 据此把格子染成你的颜色——本系统 enable,呈现 own),而真正的张力在**集齐整组**——"还差最后一块红组,值不值得抵押两块地凑钱拿下?"垄断是支柱③"运气×策略"里玩家**对抗运气的核心杠杆**:骰子决定你走到哪,但**买什么、何时集组、要不要抵押搏一把**是纯粹的算计(SDT Autonomy 主战场)。归属从零散连成垄断片区的成长快感由本系统的真值变更**驱动**、由 HUD(16)/VFX(19) **呈现**(concept 雪球 Dynamics)。

> **〔enable-not-own 体验契约〕**:本系统是**归属真值层**,不渲染任何玩家可见效果。本节描述的"变色/雪球/收租爽快"是本系统**使能(enable)**的体验——它产出正确的归属事实与变更事件,VFX(19)/HUD(16)/地产卡 UI(17)/音频(22) **拥有(own)** 其呈现。本系统做得好时完全隐形:VFX 靠它染色、HUD 靠它显示资产、收租靠它判断归属。详见 Visual/Audio Requirements 的义务边界。
- **基础层(归属的重量)**:玩家永不会想"这是一张 `TileIndex→owner` 的 map"。他们感受到的是**对手停在你满组地产上、现金哗啦转进你账户**的爽快,以及**自己踩进别人垄断区**的肉痛与"赶紧逃离这条街"的紧张(支柱②互坑的燃料)。归属必须有**重量**——"这条街归谁、有没有垄断、抵押没"要时刻清晰可感,收租与被收租才成为一桌人互坑的戏剧核心。

两层合起来制造 concept 点名的社交博弈:领先者**垄断收租雪球碾压**,落后者**抵押地产延命搏翻盘**。

> **〔MVP 范围诚实标注〕**:地产作为**交易/拍卖标的**(把地当筹码讨价还价)依赖交易(11)/拍卖(12),**全在 Alpha**;MVP 内本系统兑现的是**垄断收租雪球**与**抵押延命**两条主动线,"围绕同色组讨价还价"的 Dynamics 待 Alpha 上线。
>
> **〔Pillar② MVP 交付程度,诚实分层(game-designer R-2)〕**:concept 支柱②列举的乐趣源(收租/交易/拍卖/使坏)中,**交易与拍卖在 Alpha**。故 **MVP 的支柱②是被动版互坑**——收租/付租 + 看对手被迫抵押求生,触发取决于骰子落点(玩家无法主动"设局让你踩")。**主动互坑(同色组议价、拍卖抢标)待 Alpha**。这是**有意识的分层而非设计缺陷**:MVP 验证的是核心循环跑通,支柱②的主动维度由 Alpha 的交易(11)/拍卖(12)补全。stakeholder 不应据此误认为 MVP 已全量兑现支柱②。

> **MDA 锚点**:本系统主要服务 **Challenge/精通**(垄断布局的策略深度)与 **Fellowship/社交**(收租互坑、地产作交易标的),并驱动 concept 的雪球/翻盘 Dynamics。`Sensation`(地产变色/收租金币 juice)由 VFX(19)/HUD(16)/地产卡 UI(17) 承载,非本系统。

成功标准是**隐形的正确**:归属永远算得对、垄断判定永远准、抵押的地永不误收租——玩家从不怀疑"这块归谁、算没算翻倍",全部情绪投向"要不要拿下这块凑齐整组"的博弈本身。

## Detailed Design

### Core Rules

**CR-1 归属状态所有权(以 `TileIndex` 为键)。** 本系统拥有两份运行时可变状态,均以 `TileIndex` 为键、仅对可购买格(`Property`/`Railroad`/`Utility`)有效:① 归属 map `TileIndex → OwnerPlayerId`(无主 = `INDEX_NONE`);② 每格 `bIsMortgaged` 标记。开局全部无主、未抵押。经受控写接口改这两份状态,不直接暴露写字段(沿 player-turn AC-35 受控写契约,强度待生命周期 ADR)。本系统**不拥有**:现金(经济5)、房屋数 `house_count`(建房8)、买/不买决策点(回合2 `ResolvePhase`)、地价/抵押额静态值(棋盘1)。

**CR-2 买地事务 `Buy(tileIndex, playerId)`(6 拥有事务,归属写;现金归经济)。** 公开事务入口 **`Buy(tileIndex, playerId)`**(唯一公开接口,见接口稳定承诺)由回合2 `ResolvePhase` 决策点调用。本系统不持有价格、不执行扣款、不持买/不买决策点(归回合2)。

**执行顺序(前置前置化,消 2PC,D1 裁定):**
1. **前置校验先行**:该格可购买(`Property`/`Railroad`/`Utility`)且当前无主(`owner==INDEX_NONE`)。违反 → **拒绝整个事务**(不调 economy、不改 owner、dev `ensure`),返回失败。
2. 前置通过后,本系统调经济5 `Debit(playerId, PurchasePrice)` 执行扣款(**6→5,economy 只执行不回调本系统**)。
3. 扣款返回成功后,本系统把 `owner` 置为 `playerId` 并广播 `OnOwnershipChanged`。

**原子性(顺序契约 + 单线程不变式)**:因前置(owner==NONE)在第 1 步**已验**,单线程同步模型下第 3 步的归属写**不可能再失败**(无并发改 owner 的路径),故不需要 2PC 回滚。若 `Debit` 返回失败(现金不足) → 事务在第 2 步终止,owner 保持 `INDEX_NONE`(无"有地没扣钱");前置失败 → 第 1 步终止,不扣款(无"扣了钱没拿到地")。**联网/并发(Full Vision)须重评此单线程假设**(届时 owner 在 Debit 与归属写之间可能被改,需补回滚或锁,见 OQ-Prop-1)。

**economy 不反向调本系统**(保 5↔6 无环)。⚠ **真矛盾(Group B propagate,非本档可单方闭合)**:economy CR-4(economy-cash.md line 60)现写「本系统(经济)…Debit 并通知所有权(6)登记归属」= 5→6 反向调用,与本档 6→5 **直接矛盾**(两档对"谁拥有买地事务"给出相反答案)。**CD 绑定裁定:事务归 6、方向 6→5**(唯一保 index 无环拓扑);economy CR-4 本体 ✅ 已 propagate 改为「现金扣减被 6 调用执行、归属登记由 6 完成,economy 不调 6」(2026-06-03 + 逐档 grep 核实,见 OQ-Prop-2 RESOLVED)。

**CR-3 抵押事务 `Mortgage(tileIndex, playerId)` / `Unmortgage(tileIndex, playerId)`(标记归 6,现金归经济)。** 本系统拥有 `bIsMortgaged` 标记。公开事务入口 `Mortgage(tileIndex, playerId)` / `Unmortgage(tileIndex, playerId)`(唯一公开接口,带 `playerId` 用于 owner 校验)由回合2/AI 决策点调用。

**执行顺序(前置前置化,同 CR-2):**
1. **前置校验先行**:`owner == playerId`(只能 owner 操作)且状态匹配(抵押要求 `bIsMortgaged==false`、赎回要求 `bIsMortgaged==true`)。违反 → 拒绝整个事务、不调 economy、不改标记、dev `ensure`。
2. 前置通过后,本系统调经济5 `Credit(playerId, MortgageValue)`(抵押)/`Debit(playerId, 赎回费)`(赎回,赎回费 = 经济 F-6)执行现金(**6→5**)。
3. 现金返回成功后,本系统自置/清 `bIsMortgaged` 并广播 `OnMortgageChanged`;现金失败(赎回付不起)→ 事务终止,标记不变。

**economy 不反向调本系统**(同 CR-2 保无环;economy CR-5「通知所有权(6)标记」5→6 措辞(Group B 真矛盾)✅ 已 propagate 修正,见 OQ-Prop-2 RESOLVED)。**已抵押地产本系统报 `is_mortgaged=true`,经济 F-2 据此短路置租金 0**(经济拥有该短路)。

**`house_count==0`(带房不可抵押)前置的责任收口(economy F-4 / CD)**:本系统**读不到 `house_count`**(归建房8,防 6↔8 环),故**不校验**带房。由于 **6 拥有 Mortgage() 事务入口**,该前置必须由**调用方(回合2 `ResolvePhase` / AI10 决策点)在调 `Mortgage()` 之前**完成校验(决策点能读 8 的 `house_count`)——**不是 economy 在 `Credit` 内校验**(economy `Credit` 是通用入账接口,不知该 Credit 是抵押还是收租)。economy-cash.md line 291「本系统(经济)校验 house_count==0」措辞与本裁定冲突,归 Group B propagate(并入 OQ-Prop-2:校验归决策点,非 economy)。若被错误调用(带房抵押),本系统照置标记但 dev `ensure` + log 暴露上游 bug(见 Edge Cases / AC-14)。

**CR-4 垄断判定(6 拥有,经济消费)。** 本系统计算 `is_monopoly(playerId, colorGroup)`:经棋盘1 `GetTilesInGroup(colorGroup) → [indices]` 取该组全部格,若 `playerId` 拥有**该组全部格**则 `is_monopoly=true`。**抵押不破坏垄断归属**:垄断 = 拥有整组(与各格是否抵押无关);某格抵押的收租后果由该格 `is_mortgaged` 短路(经济 F-2),其余未抵押无房格仍享 ×2(经典忠实:完整色组的未抵押无房地享双倍租,即便组内有地被抵押)。⚠ 此忠实细节待 OQ Rento 行为核查。本系统 depends-on 棋盘1 ✓,可调 `GetTilesInGroup`。

**CR-5 同类持有计数(6 拥有,经济消费)。** 本系统据 owner map + 棋盘1 `GetTileData(index).TileType` 计算:`station_count(playerId)` = 该玩家拥有的 `Railroad` 格数(全盘,非按组);`utility_count(playerId)` = 拥有的 `Utility` 格数。供经济 F-3/F-4 算租(经济守 `count≥1` 否则 0,不假设本系统恒正确)。

> **职责收口(systems BLOCKING-7):本系统只供 `station_count` 整数,不做"选 RentTable 哪一档"——`RentTable[station_count−1]` 的档位查表归经济 F-3**(本系统无 RentTable 访问权,见 CR-7 / AC-39)。⚠ board-data CR-4(board-data.md line 90)现写"'选 RentTable 哪一档'的运行时逻辑归地产所有权(6)"是**错误归属**(给 6 派了 6 从不认领的职责),✅ **已 Group B 独立 propagate 改 board-data line 90**(2026-06-03,改为"档位查表归经济(5) F-3,6 只供 count");此债与 OQ-Prop-2 不同侧不同错型,独立追踪(见 OQ-Prop-6 RESOLVED)。

**CR-6 归属快照 push(保依赖图无环,economy OQ-T-Econ-1 对端)。** 本系统把自己拥有的归属事实组装成**只读值快照**(`owner` / `is_mortgaged` / `is_monopoly` / `station_count` / `utility_count`)push 给经济5 算租调用(本系统 depends-on 5 ✓,单向)。**`house_count` 不在本系统的快照内**——它归建房8,由 8 单独供;**本系统绝不读取 8 的 `house_count`**(否则 6→8 与既有 8→6 成环)。完整 rent 参数 = 6 的归属快照 + 8 的 `house_count`,二者在落地结算路径(回合2 `ResolvePhase` 编排)聚合后传经济。

**快照时机与隔离契约(qa PROP-012)**:① **时机**:快照在调用方(回合2 `ResolvePhase`)需要算租的那一刻、且**在本程任何归属事务(`Buy`/`Mortgage` 等)完成之后**构建,保证 `BuildOwnershipSnapshot()` 反映最新 owner map(非过期值,见 AC-31b)。② **隔离**:快照是**值拷贝**(非活引用)——一旦构建,即便随后 owner map 被外部修改(如同程内其他写),经济使用的仍是构建瞬间的值,防结算期归属漂移(见 AC-30b)。③ **聚合接口**:6 的归属快照 + 8 的 `house_count` 在 ResolvePhase 聚合后传经济的**聚合接口形态**(单一合并 struct vs 两路参数)归回合2 编排 + OQ-Prop-1 ADR 定;本系统只保证自己的快照字段正确且不含 `house_count`。

✅ **propagate 已闭合 2026-06-03(Group B P2,fresh-grep 扩集 6→9)**:"`house_count` 由所有权6供"旧措辞经 fresh-grep 重盘实命中 **9 处**(登记 6 + 原漏列 economy 104/265/470 + entities 277),已全集统一改为"6 供 owner/monopoly/mortgage/count,8 供 `house_count`、经回合2 ResolvePhase 聚合"(见 OQ-Prop-3 RESOLVED)。

**CR-7 职责边界(本系统不拥有什么)。** 本系统只产出"归属真值 + 抵押标记 + 派生垄断/计数"。不拥有:现金(经济5)、房屋数与建造规则(建房8)、买/不买/抵押/赎回的**决策点**(回合2 `ResolvePhase` / AI10)、地价/抵押额/色组成员静态数据(棋盘1)、租金公式(经济5)。决策由回合2/AI10 触发,现金后果由经济5 执行,本系统只登记归属结果。

### States and Transitions

**本系统主要是服务/状态层,非逐回合状态机**(同棋盘/经济)。它拥有的持久状态是**每格的归属与抵押**(以 `TileIndex` 为键),随存档(21)序列化。有意义的状态机是**每个可购买格的归属生命周期**:

| 状态 | 含义 | 转换 |
|---|---|---|
| **Unowned(无主)** | `owner==INDEX_NONE` | 玩家购买 → `Buy(tile, player)`(CR-2)→ **Owned** |
| **Owned(已购·未抵押)** | `owner` 已置、`bIsMortgaged==false` | ① 抵押 → `SetMortgaged(true)` → **Mortgaged**;② 收租破产:此地 in-kind 转债权人(经济 F-11)→ owner 改、**留 Owned**;③ 银行破产:回退 → **Unowned**(经济 F-11) |
| **Mortgaged(已抵押)** | `owner` 已置、`bIsMortgaged==true` | ① 赎回 → `SetMortgaged(false)` → **Owned**;② 收租破产 in-kind 转债权人 → owner 改、**`bIsMortgaged` 保持 true**(经济 F-11 保留抵押状态)→ 仍 **Mortgaged**(新主);③ 银行破产 → 回退 **Unowned**(标记清除) |

> **关键不变式**:Owned/Mortgaged 状态恒有合法 owner(`!=INDEX_NONE`);Unowned 恒 `bIsMortgaged==false`(无主不可能抵押)。破产移交(改 owner)与抵押标记(改 `bIsMortgaged`)都经受控写,由经济5/破产9 通知触发,本系统不自决破产/抵押,只执行归属/标记的状态转换。**Unowned 不变式的维持**:任何写路径在 `owner==INDEX_NONE` 时拒绝置 `bIsMortgaged=true`(`Mortgage()` 前置要求 `owner==playerId≠NONE`,无主格调 `Mortgage` 被前置拒绝,见 AC-13b)。**`TransferOwnership` 的 creditor 合法性(systems FINDING-7)**:`TransferOwnership(tile, from, to)` 前置要求 `to != INDEX_NONE`(债权人必须合法;转给 `INDEX_NONE` 应走 `ReturnTileToBank` 而非 `TransferOwnership`),否则会产生 `owner==INDEX_NONE && bIsMortgaged==true` 的非法 Unowned 态——前置拒绝 + dev ensure(见 AC-33e)。

> **破产级联:逐格广播 + 重入锁(D3 裁定,systems BLOCKING-5 / unreal 3a)**。破产9 的批量移交对债务人每格调一次 `TransferOwnership(tile, debtor→creditor)`(收租)/ `ReturnTileToBank(tile)`(银行),**每格各广播一次** `OnOwnershipChanged`(VFX 自行视觉批处理)。⚠ `Broadcast()` 同步执行所有监听器,AI(10) 等监听者可能在级联中**同步回写 owner map**(收到事件后立即做买/抵押决策)。**本系统在批量移交期间置重入锁 `bIsMidBankruptcyTransfer`**:锁期内任何 `Buy`/`Mortgage`/`Unmortgage`/`TransferOwnership` 受控写被拒绝(dev `ensure` + log),移交完成后解锁。防级联中途 owner map 被监听器并发改写(见 AC-33c)。**锁须无条件解除(systems FINDING-2)**:批量移交结束时(无论全部成功或中途异常)`bIsMidBankruptcyTransfer` 必复位——异常情形按批量原子性 AC-33b/34b 处理(dev ensure+log),但锁本身绝不挂起,否则后续所有 `Buy`/`Mortgage` 被永久拒绝致死锁。⚠ **UE 实现候选(unreal Issue2,归 OQ-Prop-1 ADR)**:`Broadcast()` 同步执行,重入锁防直接受控写回写但防不住监听器经间接路径改共享态;`deferred broadcast queue`(移交期入队、完成后批量广播,监听器只见终态)为候选替代,亦解 AC-30b 读侧中间态问题(FINDING-4)。

> **批量移交原子性(systems BLOCKING-4)**:债务人"全部地"的移交(收租 in-kind / 银行回退)是**全或全无**——遍历债务人持有的每格执行转移/回退,任一格异常不得留下"部分转移"的中间态(否则违反不变式)。银行回退须 owner 与 `bIsMortgaged` **同格原子清零**(不可只清 owner 留 `bIsMortgaged==true` 的非法 Unowned 态)。见 AC-33b/AC-34b。

> **继承已抵押地的赎回弧(systems RECOMMENDED-3)**:收租破产 in-kind 转移后,新主继承的已抵押地 `bIsMortgaged==true`,新主可经正常 `Unmortgage(tile, newOwner)` 赎回(`Mortgaged(新主) → Owned`,赎回费 = 经济 F-6,MVP 不收继承利息见经济 F-11)。此弧 = 表中 Mortgaged 行 ① 赎回,对新主同样适用,无独立规则。

> **垄断/计数是派生态、非存储态**:`is_monopoly`/`station_count`/`utility_count` 不作为字段存储,而是每次查询时从 owner map + 棋盘1 实时派生(CR-4/CR-5),避免"买地/破产改 owner 后忘同步垄断标记"的脏状态。存档只存 owner map + `bIsMortgaged`,派生量读档后重算。

> **序列化**:owner map(每可购买格一 `PlayerId`)+ `bIsMortgaged`(每可购买格一 `bool`)随存档(21)序列化(以 `TileIndex` 为键)。本系统无回合内瞬态、无中途存档点问题。

### Interactions with Other Systems

| 系统 | 数据流 | 谁拥有 |
|---|---|---|
| 棋盘数据(1) | 读 `GetTileData(index)` 的 `TileType`/`ColorGroup`/`PurchasePrice`/`MortgageValue` + `GetTilesInGroup(group)→[indices]` 算垄断/计数 | 棋盘供静态数据;本系统拥有运行时归属 |
| 经济(5) | **本系统调** `Debit`/`Credit` 执行买地/抵押/赎回现金(**6→5**);**本系统经 `BuildOwnershipSnapshot` 暴露归属快照**(`owner`/`is_mortgaged`/`is_monopoly`/`station_count`/`utility_count`),由回合(2) `ResolvePhase` 调本接口取快照、并入 8 `house_count` 后转发经济算租(F-2/3/4)——**经济只收参数、不反向调本系统**(AC-31 不变式;聚合编排归 player-turn CR-3.2,R-xreview 2026-06-03 确权) | 经济拥有现金与公式;本系统拥有归属事实并经 `BuildOwnershipSnapshot` 暴露 |
| 玩家回合(2) | `ResolvePhase` 决策点调本系统 `Buy`/`Mortgage`/`Unmortgage`(买/抵押/赎回决策);**抵押前 `house_count==0` 前置由决策点校验(决策点能读 8,本系统不读 8)**;落地结算编排聚合 6 快照 + 8 `house_count` 传经济 | 回合拥有流程/决策点 + house_count 前置校验;本系统执行归属登记 |
| 建房(8) | **8 读本系统** `owner`/`is_monopoly`/`is_mortgaged`(判可否建房:须垄断、未抵押)(**8→6**);8 拥有 `house_count` 与建造规则,**本系统不读 8**(防 6↔8 环) | 8 拥有房屋数/建造规则;本系统供归属/垄断/抵押 |
| 破产胜负(9) | 9 裁决破产后**调本系统**改 owner(**9→6**):收租破产 → 债务人全部地 in-kind 转债权人(保留 `bIsMortgaged`);银行破产 → 回退无主(清标记)。economy F-11 只执行**现金侧**转移 | 9 拥有破产裁决;本系统执行归属转移 |
| AI(10) | 读归属/垄断/计数(经快照/查询)做买/建/抵押决策估值 | AI 拥有决策;本系统供归属事实 |
| 地产卡 UI(17) | 读 `owner`/`is_mortgaged`/`is_monopoly` 呈现地产卡归属角标/抵押态/垄断高亮 | UI 拥有呈现 |
| HUD(16) | 读各玩家持有地产/垄断概览(资产面板) | HUD 拥有呈现 |
| 交易(11)/拍卖(12)(Alpha) | 经本系统受控写转移 owner(交易换手/拍卖中标登记) | 11/12 拥有谈判/出价;本系统执行归属写 |
| 存档(21) | 序列化 owner map + `bIsMortgaged`(以 `TileIndex` 为键) | 存档拥有序列化 |

**事件(本系统广播,`DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`,payload `USTRUCT(BlueprintType)`,字段全 blittable `int32`/`bool`,unreal 3b)**:
- `OnOwnershipChanged(TileIndex, OldOwner, NewOwner)` — 归属变更(买地/破产移交/交易/拍卖)供 UI/HUD/VFX/AI 挂接。**破产批量移交时逐格各广播一次**(N 格破产 = N 次),广播期间持重入锁拦截监听器回写(见 States 破产级联)。
- `OnMortgageChanged(TileIndex, bIsMortgaged)` — 抵押/赎回标记变更。
- ⚠ **收租不触发本系统任何事件**(落地收租不改归属)——收租 juice 的事件源是经济(5) `OnRentPaid`,VFX(19) 应订阅经济事件而非本系统事件(见 Visual/Audio Requirements,game R-3)。

**接口稳定承诺**:`Buy(tile,player)`/`Mortgage(tile,player)`/`Unmortgage(tile,player)`(**唯一公开事务接口,均带 `playerId`;无 `Register*` 别名**)/`GetOwner`/`IsMortgaged`/`IsMonopoly`/`GetStationCount`/`GetUtilityCount`/`BuildOwnershipSnapshot` 签名稳定;破产侧 `TransferOwnership`/`ReturnTileToBank` 供破产9 调;事件 payload 只增不改语义。给 8 个下游(8/9/10/11/12/16/17/21)的稳定保证。

**禁绕过受控写**:任何系统不得直接改 owner map / `bIsMortgaged`,只经本系统受控接口(沿 player-turn AC-35 软/硬约束)。

> **⚠ 双向一致性 / propagate 待办(均登记 Open Questions):**
> - **economy CR-4/CR-5/F-11 措辞**:现写"经济通知6登记/标记""经济执行资产转移"= 5→6 反向,与 6→5(快照 push)并存成 5↔6 环 → ✅ 已 propagate 改为"现金腿被 6 调用执行 / 资产 owner 写由破产9 经 6 执行,economy 只管现金值侧"(2026-06-03,OQ-Prop-2 RESOLVED)。
> - **systems-index 依赖列**:`9 depends-on` 须补 **6**(9→6 破产归属移交);`16 depends-on` 若 HUD 直接读归属须补 **6**(现仅 1,2,5)。`8→6`/`17→6`/`21→6` 已在 index,无需补。

## Formulas

> *由 `systems-designer`(lean 模式 D 节高风险派发)校验严谨性。* **本系统无平衡公式**——地价/租金/抵押额归棋盘(1)/经济(5);本系统只持 3 条从 owner map 派生的谓词/计数(布尔/整数,无平衡数值)。

### F-1 `is_monopoly(playerId, colorGroup)` — 是否集齐整色组

```
if playerId == INDEX_NONE: return false
if colorGroup == None:     return false
tiles = GetTilesInGroup(colorGroup)          // 顺序不影响结果(全持判定与遍历序无关)
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

**单格色组与组成员正确性假设(systems BLOCKING-1/2)**:① **`|tiles|==1`**(自定义棋盘 1 格色组):F-1 逻辑正确返回 true(玩家持那 1 格即"集齐整组")——这是**已接受行为**,数据层是否允许 1 格色组归棋盘(1)/编辑器(26)(board-data 现仅对成员数不符发警告,不拒绝);经典盘最小色组 2 格,不触发。② **无组内重复 TileIndex**:`GetTilesInGroup` 不会返回重复索引——`TileIndex` 全局唯一(board AC-19b 拒绝重号),故每格在组内至多出现一次,F-1 不会被 `[21,21,24]` 式重复列表骗过。③ **组成员归属正确性**(某格被误标到错误 `ColorGroup`)是**棋盘数据质量问题**,F-1 读不到也无法自检,归棋盘(1)加载校验(board Edge Cases 色组成员数警告);本系统忠实按 `GetTilesInGroup` 返回的成员判定,不承担数据纠错。

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
> **求值顺序约束(systems RECOMMENDED-2)**:F-2/F-3 的 `TileType==Railroad/Utility AND owner_map[t]==playerId` **必须 TileType 过滤先于 owner_map 比较**——遍历 `all_tiles`(全盘 N 格)含非可购买格(Tax/Chance/角格),这些格不在 owner map 语义内(CR-1 仅可购买格有效);先判 TileType 短路掉非可购买格,避免对其查 owner_map。实现不得为"优化"颠倒为先查 owner_map。
> **确定性(B-6)**:3 谓词均纯整数/集合,无浮点、无随机;owner map 快照输入 → 输出确定(联网回放安全),不持内部缓存(派生量每次实时重算,见 AC-40)。

## Edge Cases

**购买边界**
- **若买已有主格**:拒绝(CR-2 前置 `owner==INDEX_NONE`),dev `ensure`。
- **若买不可购买格**(Chance/Tax/角格等):拒绝(仅 Property/Railroad/Utility 可登记 owner)。
- **若经济 `Debit` 失败(现金不足)**:购买事务整体不发生——不登记 owner、不扣款(CR-2 原子契约;现金不足时购买不可用,经济 CR-4)。

**抵押边界**
- **若抵押已抵押格 / 赎回未抵押格**:拒绝(CR-3 入口 `ensure` 校验 owned 且 状态匹配)。
- **若抵押带房地产**:本系统**不校验 `house_count`**(读不到,防 6↔8 环);`house_count==0` 前置由**决策点(回合2 `ResolvePhase` / AI10,能读建房8 的 `house_count`)在调 `Mortgage()` 前**保证(非 economy 在 `Credit` 内校验,见 CR-3)。若被错误调用(带房抵押),属上游 bug,本系统照置标记但 dev `ensure` + 日志(无法自验)——职责边界:防带房抵押归决策点,本系统只防 `owner==playerId 且 未抵押` 态(见 AC-14)。
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

> **双向一致性(✅ propagate 已执行 2026-06-03):**
> - `9 depends-on` **已补 6** ✓(systems-index line28:`2,5`→`2,5,6`,9→6 破产归属移交)。(OQ-Prop-4)
> - `16 depends-on` **暂不补**(HUD 是否直读归属待 HUD GDD 裁定,留条件 OQ)。(OQ-Prop-4)
> - `8→6` / `17→6` / `21→6` 已在 index ✓。
> - **economy CR-4/CR-5/F-11 反向调用措辞** ✅ 已 propagate 修正(economy 改为事务归6/只执行现金侧,5↔6 环解除,index"无环"声明恢复)。(OQ-Prop-2)
> - **board-data 侧接口列不一致(R1 主审 fresh-grep,P5)** ✅ 已补:board-data Interactions 表 row6(line161)+ 下游依赖表(line301)均补 `TileType` + `GetTilesInGroup`,双向一致性恢复。

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
- ⚠ **收租 juice 的事件源不是本系统(game R-3)**:落地收租**不改变归属**,故本系统的 `OnOwnershipChanged`/`OnMortgageChanged` **均不在收租时触发**。"对手踩上来付钱给我"的金币飞溅/互坑反馈,VFX(19) 须订阅**经济(5)的 `OnRentPaid`**(收租现金执行归经济),**而非本系统事件**。本系统对收租的义务只是:结算前归属快照已产出且正确(CR-6)。此澄清防 VFX(19) 实现者误订阅本系统归属事件来触发收租动画、发现永不触发而困惑。
- **破产清算逐格反馈**:破产 in-kind/银行回退逐格各广播一次 `OnOwnershipChanged`(见 Interactions),VFX(19) 据 old/new owner 区分"易主清算"vs"回退无主",并可自行视觉批处理(N 格连发)。
- **好/坏语境**:`OnOwnershipChanged` 携 old/new owner,呈现层据此区分"买下(获得感)"vs"破产易主(清算)";`is_monopoly` 查询供垄断高亮。
- **回链**:登记 systems-index 继承义务表(VFX19 / 地产卡 UI17 行)。

> 📌 **Asset Spec**:art bible 批准后,可运行 `/asset-spec system:property-ownership` 产出地产归属角标/垄断高亮/抵押态的视觉规格(若需要)。

## UI Requirements

**n/a —— 本系统无任何 UI。** 它不绘制屏幕元素,只暴露归属/抵押/垄断查询 + 受控写接口(`Buy`/`Mortgage`/`Unmortgage`)+ 2 个事件。地产归属的呈现——地产卡归属角标、抵押态、垄断高亮、玩家资产面板——由 **地产卡 UI(17)** 与 **HUD(16)** 实现;买地/抵押/赎回按钮的输入路由由 HUD/输入层处理后回调本系统接口。本系统的"接口"是数据契约与事件,不是界面。

> 📌 **UX Flag —— 地产所有权**:地产卡归属呈现(角标/抵押/垄断高亮)、玩家资产面板须在 Pre-Production 由 `/ux-design` 为相关 HUD/卡牌屏出规格,再交 `/team-ui`。Stories 引用 `design/ux/[screen].md`,非本 GDD。

## Acceptance Criteria

> *由 `qa-lead`(lean 模式 H 节高风险派发)产出。* **测试分层(对齐 economy/movement/player-turn):** `[Logic]` 纯 fixture struct、确定性整数、headless `-nullrhi` 可跑、BLOCKING PR merge gate(`tests/unit/property-ownership/`);`[Advisory]` 集成/code-review/跨进程,非门控。
> **核心原则**:本系统无 RNG、无平衡数值,全是逻辑断言——"给定 owner map + 输入,谓词/状态转换算得对"。受控写软约束在 BP-only 下仅 `[Advisory]` code-review,C++ 强封装可升 `[Logic]`(沿 economy AC-4 / movement AC-3 生命周期 ADR)。
> **⚠ ADR 未决执行机制(qa PROP-013 / R2 落地 CI-stub)**:AC-3/7/31/32/35 覆盖 S1/S2 路径(受控写绕过、5↔6 环、push 方向、6↔8 环、破产移交方)。**OQ-Prop-1 ADR 未落地时,这 5 条标 `[Logic·CI-stub]`**:在 `tests/unit/property-ownership/` 置 **skip 占位测试**,CI 运行时报"需手动 code-review 审查"(不静默丢录、不阻塞编译但留痕);ADR 落地、C++ 封装路径选定后**解 skip + 补真实静态扫描测试**升为正式 [Logic] gate。**不得**降为无 CI 记录的纯人工 Advisory(无 merge 拦截 = 执行真空,PROP-013 双重态修正)。"ADR 未决 → 更松"是错误方向。
> **⚠ dev `ensure` 测试前提(qa 跨 AC 系统性)**:所有含 "dev `ensure`" 的 [Logic] AC(AC-4/6/10/12/13/13b/33c/33d/33e/34 等)默认在 **Development/Test build** 下运行(Shipping 剥离 ensure);fixture 须以**捕获 ensure 失败回调**为断言手段,不只断言返回值。

### A. 核心规则——owner map & 受控写(CR-1/CR-7)
- **AC-1** [Logic] 初始状态:GIVEN 空局初始化,WHEN 读任意可购买格 owner,THEN `owner==INDEX_NONE` ∧ `bIsMortgaged==false`。
- **AC-2** [Logic] 无主格守门:GIVEN 可购买格 `owner==INDEX_NONE`,WHEN `GetOwner`/`IsMortgaged`,THEN 返回 `INDEX_NONE`/`false`,不崩、不触发事件。
- **AC-3** [Advisory·code-review] 受控写软约束:全代码库无外部系统直写 owner map / `bIsMortgaged`,只经受控入口。强度坦诚:BP-primary → code-review;C++ 强封装可升 [Logic] 静态扫描(OQ-Prop-1 ADR)。
- **AC-4** [Logic] 非可购买格不进 owner map:GIVEN `TileType ∈ {Go/Tax/Jail/FreeParking/Chance/CommunityChest}`,WHEN `GetOwner(tile)`,THEN `INDEX_NONE` + dev ensure。

### B. 买地事务 `Buy(tile, player)`(CR-2,前置前置化)
> 测试统一用公开入口 `Buy(tile, player)`(非 `Register*`);经济 `Debit` 由 **mock 注入** Success/Failure,fixture 不依赖真实 Cash 值。
- **AC-5** [Logic] 正常买地全路径:GIVEN tile 属 Property、`owner==INDEX_NONE`、mock economy.Debit 返回 Success,WHEN `Buy(tile, player)`,THEN 内部顺序 = 先验前置 → 调 economy.Debit(恰 1 次)→ 置 owner;结果 `GetOwner(tile)==player` ∧ `IsMortgaged(tile)==false` ∧ `OnOwnershipChanged(tile, INDEX_NONE, player)` 恰 1 次。(与 AC-8 区别:AC-5 测 happy-path 全链,AC-8 测原子性两失败半。)
- **AC-6** [Logic] 前置守门——只能买无主格:GIVEN `owner!=INDEX_NONE`,WHEN `Buy(tile, player)`,THEN **前置即拒绝(不调 economy.Debit)**、owner 不变、无广播、dev ensure。(断言 mock economy.Debit 调用次数==0,证明前置先行。)
- **AC-7** [Advisory·code-review·架构方向 / OQ-Prop-1 未决时按最严 BLOCKING checklist] economy 不反向调 6:**可执行 checklist**——在 economy 实现(BP 图 / C++)中检索对本系统买地/归属写接口(`Buy`/`TransferOwnership`/owner-map 写)的调用,结果**必须为 0**(economy `Credit`/`Debit` 内部不调本系统,防 5↔6 环)。BP-primary:reviewer 在 economy BP 图 Ctrl+F 搜本系统写接口节点=0;C++ 编排层:静态扫描升 [Logic]。沿 economy OQ-Econ-1/OQ-Prop-1。
- **AC-8** [Logic] 原子性——前置前置化下的两失败半:① GIVEN 前置通过、mock economy.Debit 返回 Success → `GetOwner==buyer`(无"扣钱没地");② GIVEN mock economy.Debit 返回 Failure(现金不足)→ 事务在 Debit 后终止,`GetOwner` 仍 `INDEX_NONE`、无广播(无"有地没扣钱")。两子例独立 fixture,mock 注入成功/失败。
- **AC-8b** [Logic] 单线程无 2PC 残态:GIVEN owner==INDEX_NONE 前置已过、Debit Success,WHEN 置 owner,THEN 归属写不可能失败(单线程下前置已锁定 owner==NONE,无并发改写路径)——断言无"Debit 成功但 owner 未改"的中间态。(注:联网/并发的回滚由 OQ-Prop-1 重评,非 MVP 单线程 AC 范围。)

### C. 抵押事务 `Mortgage/Unmortgage(tile, player)`(CR-3,前置前置化)
> 测试用公开入口 `Mortgage(tile, player)`/`Unmortgage(tile, player)`(均带 playerId);economy `Credit`/`Debit` mock 注入。`house_count` 不在 6 测试域(6 读不到),fixture 不设也不验该前提。
- **AC-9** [Logic] 正常抵押:GIVEN `owner==player`、`bIsMortgaged==false`、mock economy.Credit 返回 Success,WHEN `Mortgage(tile, player)`,THEN 顺序 = 先验前置 → 调 economy.Credit → 置标记;结果 `IsMortgaged(tile)==true` ∧ `OnMortgageChanged(tile, true)` 恰 1 次。**(GIVEN 不含 house_count==0:该前置归决策点,不在 6 fixture 内,见 CR-3 / OQ-T-Prop-3。)**
- **AC-10** [Logic] 重复抵押守门:GIVEN `bIsMortgaged==true`,WHEN `Mortgage(tile, player)`,THEN 前置即拒绝(不调 economy.Credit)、不变、无广播、dev ensure。
- **AC-11** [Logic] 正常赎回:GIVEN `bIsMortgaged==true`、mock economy.Debit Success,WHEN `Unmortgage(tile, player)`,THEN `IsMortgaged(tile)==false` ∧ `OnMortgageChanged(tile, false)` 恰 1 次。
- **AC-11b** [Logic] 赎回原子性——Debit 失败:GIVEN `bIsMortgaged==true`、`owner==player`、mock economy.Debit 返回 Failure(付不起赎回费),WHEN `Unmortgage(tile, player)`,THEN `IsMortgaged(tile)` 仍 `true`、owner 不变、`OnMortgageChanged` 调用次数==0(事务终止、标记不变,CR-3 现金失败分支)。(qa 指出 AC-11/12/13 未覆盖赎回 Debit 失败路径。)
- **AC-12** [Logic] 赎回守门——非抵押态:GIVEN `bIsMortgaged==false`,WHEN `Unmortgage(tile, player)`,THEN 前置即拒绝、不变、无广播、dev ensure。
- **AC-13** [Logic] 入口守门——只能 owner 操作:GIVEN `owner==playerA`,WHEN `Mortgage(tile, playerB)`(`playerB!=owner`),THEN 前置拒绝(`owner!=playerId`)、不变、不调 economy、dev ensure。(`Mortgage` 签名含 `playerId`,见 CR-3 / 接口稳定承诺。)
- **AC-13b** [Logic·关键 Unowned 不变式维持(CR-1)] 无主格不可抵押:GIVEN `owner==INDEX_NONE`,WHEN `Mortgage(tile, anyPlayer)`,THEN 前置拒绝(`owner!=playerId`)、`bIsMortgaged` 仍 false、不调 economy、dev ensure。**专测"Unowned 恒 bIsMortgaged==false"在写路径下维持**(此前仅 AC-1 测初始态,不测维持)。
- **AC-14** [Advisory·code-review] 6 不校验 house_count:`Mortgage` 实现内无 `house_count>0`/读 8 的判断(防职责蠕变,该前置归决策点)。
- **AC-14b** [Logic] 6 不据 house 门控(职责边界焊死):GIVEN `owner==player`、`bIsMortgaged==false`、mock economy.Credit Success——**不论该地是否带房(6 读不到 house_count,无法也不应感知)**,WHEN `Mortgage(tile, player)`,THEN 本系统照常置 `bIsMortgaged=true` ∧ `OnMortgageChanged` 恰 1 次,且**实现路径中无任何 house_count 读取/分支**(spy 断言对系统#8 调用次数==0)。固化"6 只据 owned+未抵押 门控、带房防线归决策点(OQ-T-Prop-3)"的边界。(原 AC-14b 要求"6 检测带房并触发 ensure"逻辑不可实现——6 读不到 house_count 无从触发,本轮 re-review 重写;qa/economy/systems 三方独立指出。)

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
- **AC-29** [Logic·关键] 计数不低报 + 全盘遍历路径不丢格(B-7 / systems BLOCKING-3):GIVEN 构造 owner map 使 player1 **逐格**拥有 Railroad {5,15,25,35} + Utility {12,28}(经 owner_map[t]=player1 赋值,**不 mock count 返回值**),WHEN `GetStationCount(player1)`/`GetUtilityCount(player1)` 走全盘 `all_tiles` 遍历路径,THEN 严格 ==4 / ==2。**断言走真实遍历(TileType 过滤 + owner 比较),而非硬传 count 绕过枚举**——证明遍历不丢格(防某格 TileType 误读致漏计)。
- **AC-29b** [Logic] 中间数量不低报:GIVEN player1 逐格拥有 Railroad {5,15}(2 站),WHEN `GetStationCount(player1)` 经遍历,THEN 严格 ==2(绝不返回 1);证明部分持有经全盘遍历仍精确。

### F. 归属快照 push(CR-6 + economy OQ-T-Econ-1)
- **AC-30** [Logic] 快照字段存在性 + 具体场景值:① **字段存在性**——WHEN `BuildOwnershipSnapshot(P1)`,THEN 返回 struct 恰含 `owner`/`is_mortgaged`/`is_monopoly`/`station_count`/`utility_count` 五字段、类型正确(int32/bool)、**不含 `house_count`**(归建房8)——**字段集经 reflection/编译期 `static_assert` 核验:字段集 == 这 5 个且字段数==5;实现若误加 `house_count`(读了系统8)则本负断言失败**(economy R8/qa 指出原 AC-30 缺"不含 house_count"的负断言)。② **具体值**(给定 ground truth,非笼统"值正确")——GIVEN P1 拥有红组 {21,23,24} 全部且 21 已抵押、拥有 Railroad {5,15},WHEN 取 P1 快照,THEN `is_monopoly(Red)==true` ∧ `is_mortgaged(21)==true` ∧ `station_count==2`(fixture 须 mock 棋盘 `GetTilesInGroup(Red)→{21,23,24}`)。字段各场景的穷举正确性由 AC-15~29 覆盖,本 AC 只验快照装配如实搬运。
- **AC-30b** [Logic·快照隔离(systems BLOCKING-6)] 值拷贝防漂移:GIVEN 已 `BuildOwnershipSnapshot(P1)` 得 snapshot,WHEN 随后修改 owner map(如 `Buy` 另一格 / `TransferOwnership`),THEN snapshot 内字段**不随之改变**(经济使用的仍是构建瞬间的值)——断言快照是值拷贝而非活引用。
- **AC-31** [Advisory·code-review / OQ-Prop-1 未决时按最严] push 方向:snapshot 由 6 push 给 economy 租金入口(economy 不反查 6 字段)。可执行 checklist:economy 算租入口签名接收快照参数,economy 实现内无对本系统查询接口的调用。C++ 可升 [Logic] 静态扫描。对齐 economy OQ-T-Econ-1。
- **AC-31b** [Logic·push 时机(qa PROP-012)] 快照反映最新归属:GIVEN 同程内先 `Buy(tile, P1)` 完成,WHEN 随后 `BuildOwnershipSnapshot(P1)`,THEN snapshot.owner 反映本次 Buy 结果(`GetOwner(tile)==P1`),非 Buy 前的过期值——断言快照构建在归属事务之后、取最新 owner map。
- **AC-32** [Advisory·code-review / OQ-Prop-1 未决时按最严] 6 不读建房(8):spy/静态检查本系统对系统#8 调用次数==0(快照不含 house_count,防 6↔8 环)。C++ 可升 [Logic]。

### G. 破产移交(CR-1 / economy F-11,触发方=系统#9)
- **AC-33** [Logic] 收租破产 in-kind——保留抵押:GIVEN debtor 持 A(`bIsMortgaged==true`)+B(`bIsMortgaged==false`),9 调 `TransferOwnership(A, debtor, creditor)` 与 `TransferOwnership(B, debtor, creditor)`,THEN `GetOwner(A)==creditor` ∧ `IsMortgaged(A)==true`(随地转移不清)∧ `GetOwner(B)==creditor` ∧ `IsMortgaged(B)==false`(显式断言,非"保持"措辞)。
- **AC-33b** [Logic·批量原子性(systems BLOCKING-4)] 全部地全或全无:GIVEN debtor 持 3 格,WHEN 9 触发批量 in-kind 移交,THEN 3 格 owner 全部改为 creditor、无一遗漏(无"部分转移"中间态);每格各广播一次 `OnOwnershipChanged`(共 3 次)。
- **AC-33c** [Logic·重入锁(systems BLOCKING-5 / unreal 3a)] 级联防回写:GIVEN 批量移交进行中(`bIsMidBankruptcyTransfer==true`),WHEN 监听器在 `OnOwnershipChanged` 回调内尝试 `Buy`/`Mortgage`/`TransferOwnership` 受控写,THEN 该写被拒绝(dev ensure+log)、owner map 不被级联中途篡改;移交完成后锁解除、写恢复。
- **AC-33d** [Logic] 不可购买格守门:GIVEN `TileType ∈ 非可购买格`,WHEN `TransferOwnership`/`ReturnTileToBank`,THEN 拒绝、dev ensure(非可购买格无 owner 语义)。
- **AC-33e** [Logic·关键 Unowned 不变式维持(systems FINDING-7)] creditor 合法性守门:GIVEN 某 mortgaged 格,WHEN `TransferOwnership(tile, from, INDEX_NONE)`(债权人非法/误把回退当转移),THEN 前置拒绝、owner 与 `bIsMortgaged` 均不变、dev ensure——**绝不产生 `owner==INDEX_NONE && bIsMortgaged==true` 非法 Unowned 态**(转债权人须 `creditor!=INDEX_NONE`;回退无主才走 `ReturnTileToBank` 原子清零,见 AC-34/34b)。
- **AC-34** [Logic] 银行破产回退(单格):GIVEN 债权方==Bank 的某格 `bIsMortgaged==true`,9 调 `ReturnTileToBank(tile)`,THEN `owner==INDEX_NONE` ∧ `bIsMortgaged==false`(owner 与标记**同格原子清零**,守 Unowned 不变式)。
- **AC-34b** [Logic·批量原子性(systems BLOCKING-4)] 银行破产批量回退守不变式:GIVEN debtor 持 5 格(含 2 格 mortgaged),WHEN 批量 `ReturnTileToBank`,THEN 5 格全部 `owner==INDEX_NONE` ∧ 全部 `bIsMortgaged==false`——**绝不留 `Unowned 且 bIsMortgaged==true` 的非法态**(owner 清零与标记清零须同格原子,不可只清一半)。
- **AC-35** [Advisory·code-review / OQ-Prop-1 未决时按最严] 移交由系统#9 触发:spy/检查 `TransferOwnership`/`ReturnTileToBank` 调用方是 9 编排路径,**economy 不直接调**(地产侧归 9→6 链,economy 只管 Cash 侧)。⚠ economy F-11 现写"经济执行资产价值转移"是范围冲突(economy AC-30 把地产转移当 economy 测),归 Group B propagate(OQ-Prop-2:地产 owner 写归 9→6,economy 只管现金侧)。

### H. 职责边界(CR-7,均 [Advisory·code-review])
- **AC-36** 6 不持 Cash(实现内无 `PlayerState.Cash` 读写)。
- **AC-37** 6 不持决策点(`Register*` 入口无"是否应该买/抵押"策略判断)。
- **AC-38** 6 不持静态棋盘数据(colorGroup 成员/TileType 来自棋盘1 `GetTileData`,不硬编码组成员)。
- **AC-39** 6 不持租金公式(谓词只返归属,无 RentTable 查找/乘法)。

### I. 确定性 / 整数纯净(B-6)
- **AC-40** [Logic·确定性] owner map 全整数操作(键值 int32,无 float);固定 fixture 连调 `IsMonopoly`/`StationCount`/`UtilityCount` 各两次,位级一致、不受帧/时序影响(幂等性)。
- **AC-40b** [Logic·派生非存储(systems RECOMMENDED-4 / qa PROP-007)] 无错误缓存:GIVEN 第一次调 `IsMonopoly(P1,Red)` 返回 false(P1 缺一格),WHEN 修改 owner map 使 P1 补齐红组、再调 `IsMonopoly(P1,Red)`,THEN 返回 **true**(反映新 owner map,非返回旧缓存值)——证明派生量每次实时重算、无内部缓存(若实现误加缓存,本 AC 失败而 AC-40 不会)。

### J. 接口稳定性(对齐 economy AC-38/39 / movement AC-20/21)
- **AC-41** [Logic·CI 构建] 公共接口标 `UFUNCTION(BlueprintCallable)`、`OnOwnershipChanged`/`OnMortgageChanged` 标 `BlueprintAssignable`、payload `USTRUCT(BlueprintType)`、编译通过(headless CI `-nullrhi`)。

> **[Logic] gate(BLOCKING PR)= 44 条(R2 新增 AC-11b/AC-33e);[Logic·CI-stub]= 5 条(AC-3/7/31/32/35,OQ-Prop-1 ADR 未落地时 skip 占位 + CI 报手动审查,落地后升正式 [Logic]);[Advisory·code-review]= 5 条(AC-14/36/37/38/39);总计 54 条(R1 新增 11 + R2 新增 2:11b/33e)。**
> **⚠ ADR 未决执行机制**:AC-3/7/31/32/35 标 [Logic·CI-stub](见 §AC 引言 PROP-013 R2):未落地时 skip 占位 + CI 报手动审查留痕,ADR 落地后升正式 [Logic] 静态扫描;实际门控数(含 CI-stub 留痕)≥ 44。
> **CR/F/B 覆盖对照:** CR-1(含不变式维持)→1/2/3/4/**13b**/34/34b;CR-2→5/6/7/8/**8b**;CR-3→9/10/11/**11b**/12/13/13b/14/14b;CR-4→15~21;CR-5→22~29/**29b**;CR-6(含隔离/时机)→30/**30b**/31/**31b**/32;CR-7→36/37/38/39;F-1→15~21;F-2→22~25/29/29b;F-3→26~29;B-1→20;B-2→21/24/28;B-3→25;B-4→23/27;B-5→19/20;B-6(含无缓存)→40/**40b**;B-7→29/29b;破产保留抵押→33;批量原子性→**33b/34b**;级联重入锁→**33c**;不可购买格守门→**33d**;creditor 合法性守门→**33e**;银行回退→34;5↔6 无环→7/31/35。**CR-1~7 / F-1~3 / B-1~7 全覆盖;状态机不变式维持 + 批量原子性 + 快照隔离/时机 + 级联重入 R1 补齐。**

### 跨系统测试义务(OQ-T-Prop-*,回链 systems-index 继承义务表)
- **OQ-T-Prop-1 → 经济(5)**:6 向 economy 提供 `is_monopoly`/`station_count`/`utility_count`/`is_mortgaged` 快照,对齐 economy AC-9(抵押短路)/AC-15(station guard)/AC-17(utility guard);economy guard 只防竞态残值 0,6 的 AC-22~24/29 保证不低报。
- **OQ-T-Prop-2 → 存档(21)**:21 序列化完整 owner map + `bIsMortgaged`;读档后三谓词结果与存档前一致。
- **OQ-T-Prop-3 → 建房(8)**:8 拥有卖房动作(house_count→0);决策点(回合2/AI)在调 `Mortgage(tile, player)` 前校验 `house_count==0`(读 8 的 house_count)。此前置链由 8 的卖房 AC + 决策点校验 AC 断言;6 的 AC-9 假设 house_count==0 已由调用方保证,不重测(6 读不到 8)。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-Prop-1 ⚠ ADR(R1 扩展)** | owner map 实现层,与 player-turn OQ-1 / board OQ-6 / economy OQ-Econ-1 / movement OQ-Move-3 **协调同一生命周期层 ADR**。须裁定:① **容器形态**——候选 `TArray<int32> OwnerMap`(长度 = 全盘格数 N,-1=INDEX_NONE;**非可购买格槽位亦存 INDEX_NONE 哨兵**,使 F-2/F-3 全盘遍历 `OwnerMap[t]` 不越界——此槽位语义须 ADR 焊死,unreal Issue6)优于 `TMap<int32,int32>`(稠密 0..N-1 键的 cache locality/O(1) 索引;"TMap is BP-friendly"论据无效——BP 只调受控接口、容器对 BP 不可见)。**⚠ 抵押标记容器不可用 `TArray<bool>`(unreal Issue1)**:UE `TArray<bool>` 是 bitfield specialization——不能 memcpy、`operator[]` 返 proxy、range-for 赋值静默失败(会坑 AC-34b 批量清零)、序列化路径不透明,故"连续拷贝"论据对它**不成立**;**改用 `TArray<uint8>` 或 `TBitArray<>`**(ADR 裁定)。**前提假设(unreal Issue7)**:`TArray` 方案依赖 TileIndex 连续无洞(经典 0..39 成立;自定义棋盘若允许编号空洞须重评),ADR 须写明;② **受控写封装强度**——C++ private 容器 + `BlueprintCallable` 访问器(BP 无模块级访问控制,软约束实质不可执行;UE 可加 `meta=(BlueprintPrivate)` 限定属性仅本类 BP 图可见,比纯软约束稍强但仍非编译期强封装,unreal Issue4)→ AC-3/7/31/32/35 由 [Logic·CI-stub] 升正式 [Logic] 静态扫描;③ **UObject 宿主类型**(`UGameInstanceSubsystem` vs `UWorldSubsystem` vs GameState `UActorComponent`)——决定序列化钩子/PostLoad 顺序/GC;**约束(unreal Issue3,须并入 Dependencies 不可全推 ADR)**:宿主类型须保证 owner map 字段可被存档(21) `USaveGame` 体系访问并序列化(三种宿主与 `SaveGameToSlot` 接缝各异),且 PostLoad 派生量重算须晚于棋盘1 加载完成(`GetTilesInGroup` 可用);④ **破产级联广播策略**——D3 定逐格广播 + 重入锁,**ADR 须对比 `deferred broadcast queue`**(unreal Issue2,见 States 破产级联注)并固化**锁无条件解除**不变式(systems FINDING-2);⑤ **监听器读档重绑合约**(`OnOwnershipChanged` 绑定不随序列化保存,须定义读档后重绑时机防 dangling delegate;**UE 惯用法二选一(unreal Issue5)**:A=宿主 `PostLoad` 广播 `OnSystemReady` 触发监听者统一 `AddDynamic` 重绑;B=各监听者 `BeginPlay`/`Initialize` 无条件 `AddDynamic` + 一次性同步拉取当前 owner map 初始化显示,不依赖历史 Broadcast);⑥ **PostLoad 派生量重算顺序**(须等棋盘1 加载完成 `GetTilesInGroup` 可用)。**截止/默认(qa PROP-013,R2 机制落地):下游 8/9 实现开始前 `/architecture-decision`;未落地时 AC-3/7/31/32/35 标 `[Logic·CI-stub]`——tests 目录置 skip 占位测试 + CI 报"需手动审查"留痕,ADR 落地后解 skip 补真实静态扫描测试;不降为无 CI 记录的非门控 Advisory。** | 下游 8/9 实现开始前 `/architecture-decision` |
| **OQ-Prop-2 ✅ RESOLVED 2026-06-03** | economy CR-4/CR-5/F-11 的反向调用措辞 = **5→6** 与本档 6→5 成 **5↔6 环**。**已 propagate**:economy line 60(CR-4)/63/64(CR-5)/104(Interactions row6)改为"事务归6、6 调 economy `Debit`/`Credit`、economy 不通知6";line 70/87/106/118/241(F-11)+ AC-30/441/AC-31/442 改为"economy 只执行现金侧,地产 owner in-kind 转移由破产9 经所有权6/建房8";line 291 house_count==0 校验归决策点非 economy。**fresh-grep 验证 0 残留反向措辞**。 | ✅ 已闭合 |
| **OQ-Prop-3 ✅ RESOLVED 2026-06-03(fresh-grep 扩集 6→9)** | "`house_count` 由所有权6供"旧措辞不精确(归建房8,否则 6↔8 环)。**原登记 6 处,fresh-grep 实命中 9 处**——除登记的 `entities.yaml:267`、economy `58/152/318/333`、systems-index `67` 外,**新增 3 处原漏列**:economy `104`(Interactions 表「读…房屋数」)、economy `265`(Phase5 义务)、economy `470`(OQ-T-Econ-1 接口列)。**全集已统一改为"6 供 owner/monopoly/mortgage/count,8 供 `house_count`、经回合2 ResolvePhase 聚合"**(+ entities `277` notes 同步)。fresh-grep 验证 0 残留。 | ✅ 已闭合(再证登记清单漂移漏列,见记忆) |
| **OQ-Prop-4 ✅ PARTIAL 2026-06-03** | systems-index 依赖列:`9 depends-on` **已补 6**(line28:`2,5`→`2,5,6`,9→6 破产归属移交);`16 depends-on` **暂不补**(用户裁定:HUD 是否直读归属还是经回合2/经济快照间接拿,待 HUD GDD 设计裁定,留为条件 OQ)。`8→6`/`17→6`/`21→6` 已在 index。 | ✅ 9→6 已闭合;16→6 待 HUD GDD |
| **OQ-Prop-5 🔴 BLOCKING(R1 升级,game-designer B-1 / CD 裁定)** | `is_monopoly` 的"抵押不破坏垄断归属"(完整色组未抵押无房地享 ×2,即便组内有地被抵押)是经典大富翁规则;**本作是 Rento Fortune 复刻,fidelity 标准是 Rento 而非经典 Monopoly**。**AC-18 已是 [Logic] BLOCKING PR gate 焊死经典行为**——若 Rento 实际"抵押任一格破坏整组 ×2",则 AC-18 断言方向反、F-1 收租后果语义需改,属实现后高成本返工。**升级为实现前 BLOCKING 核查**:MVP 设计冻结前对照 Rento Fortune 实测该行为;**显式升级条件**:若核查结果与经典相反,AC-18 断言反转 + F-1/CR-4 抵押-垄断后果语义回填。当前"MVP 暂取经典忠实"仅为待核查临时立场,不得在未核查下进入实现。(类比 board OQ-3 / movement OQ-Move-1 Rento 核查项。)**附 game-designer/economy F-8 观察**:"部分抵押保垄断"会成为"抵押组内低价格保高价格 ×2"的策略,核查须一并确认其与 Rento 一致。 | MVP 设计冻结前核查(BLOCKING,先于 AC-18 实现) |
| **OQ-Prop-6 ✅ RESOLVED 2026-06-03** | board-data CR-4(`board-data.md line 90`)错把"选 RentTable 哪一档"归地产所有权(6)。**已 propagate** 改 board-data line90 为"数持有数(station_count)归6、`RentTable[station_count−1]` 档位查表 + 结算归经济(5) F-3(6 无 RentTable 访问权,只供 count)"。**附带 P5**:board-data line161/301 Interactions/依赖表 row6 补 `TileType`+`GetTilesInGroup`(原缺、且与 line301 自相矛盾,双向一致性)。 | ✅ 已闭合 |
| **OQ-Prop-7 ✅ RESOLVED 2026-06-03(R2 re-review propagate)** | economy F-5(line183)/Edge Cases(line292)/AC-20(line415) 残留断言"economy 自校抵押前置 `is_mortgaged==false`/`house_count==0`"——与 economy 同档 line291("economy 读不到 house_count、校验归决策点")**直接互斥**,是 Group B(OQ-Prop-2/3)propagate 的**第二代漏列**(原 grep 只搜"谁供 house_count",未搜"economy 自校"反向前置措辞)。**已 propagate 修正 economy F-5/292/AC-20**:抵押前置(未抵押归所有权6 `Mortgage()` AC-10 / 无房归决策点 OQ-T-Prop-3)由调用方保证,economy `Credit` 通用入账读不到二者、不在 `Credit` 内校验;AC-20 由 [Logic] 守门改 [Advisory·code-review](mirror property-ownership AC-14)。**fresh-grep 双向核实 economy 0 残留"economy 校验抵押前置"措辞**(F-2 收租短路读 is_mortgaged 是 push 快照消费、合法,不在此列)。 | ✅ 已闭合(producer propagate 本轮) |
