# Systems Index: 骰子大亨 (Dice Tycoon)

> **Status**: Draft
> **Created**: 2026-05-31
> **Last Updated**: 2026-06-02
> **Source Concept**: design/gdd/game-concept.md

---

## Overview

《骰子大亨》是对 Rento Fortune 的复刻——一款 2–8 人回合制数字骰子棋盘游戏。其机械范围围绕经典大富翁核心循环展开：掷骰移动、买地、收租、建房、逼对手破产；并叠加原作的四项特色博弈机制（拍卖、命运之轮、俄罗斯轮盘、策略卡）。系统分解的核心地基是 **经济现金系统**（租金/抵押/建房公式的发源地，几乎所有交互依赖它）与 **棋盘数据 / 玩家回合** 框架。MVP 聚焦本地 + AI 跑通核心循环，特色机制留待 Alpha，联网与地图编辑器留待 Full Vision。设计与实现严格按依赖顺序自底向上推进，呼应支柱①桌游忠实、②社交博弈、③运气×策略、④易上手。

---

## Systems Enumeration

| # | System Name | Category | Priority | Status | Design Doc | Depends On |
|---|-------------|----------|----------|--------|------------|------------|
| 1 | 棋盘数据 Board/Tile Data (inferred) | Core | MVP | Approved | [board-data.md](board-data.md) | — |
| 2 | 玩家与回合管理 Player & Turn (inferred) | Core | MVP | Approved | [player-turn.md](player-turn.md) | 3 |
| 3 | 骰子 Dice | Core | MVP | Approved | [dice.md](dice.md) | — |
| 4 | 移动 Movement | Gameplay | MVP | Approved† | [movement.md](movement.md) | 1, 2, 3 |
| 5 | 经济与现金 Economy & Cash | Economy | MVP | Approved* | [economy-cash.md](economy-cash.md) | 1, 2 |
| 6 | 地产所有权 Property Ownership | Economy | MVP | Designed‡ | [property-ownership.md](property-ownership.md) | 1, 5 |
| 7 | 事件格 Tile Events（机会/命运/税/监狱/起点） | Gameplay | MVP | Not Started | — | 1, 2, 3, 4, 5 |
| 8 | 建房升级 Building & Rent Scaling | Economy | MVP | Not Started | — | 1, 5, 6 |
| 9 | 破产与胜负 Bankruptcy & Win | Gameplay | MVP | Not Started | — | 2, 5 |
| 10 | AI 对手 AI Opponent (inferred) | Gameplay | MVP | Not Started | — | 3, 4, 5, 6, 7, 8 |
| 11 | 交易 Trading (inferred) | Economy | Alpha | Not Started | — | 5, 6 |
| 12 | 拍卖 Auction | Economy | Alpha | Not Started | — | 5, 6 |
| 13 | 命运之轮 Fortune Wheel | Gameplay | Alpha | Not Started | — | 5, 7 |
| 14 | 俄罗斯轮盘 Russian Roulette | Gameplay | Alpha | Not Started | — | 2, 9 |
| 15 | 策略卡 Strategy Cards | Gameplay | Alpha | Not Started | — | 5, 6, 7 |
| 16 | HUD（玩家面板/现金/回合）(inferred) | UI | MVP | Not Started | — | 1, 2, 5 |
| 17 | 地产卡与卡牌 UI (inferred) | UI | MVP | Not Started | — | 1, 6, 7 |
| 18 | 交易/拍卖 UI (inferred) | UI | Alpha | Not Started | — | 11, 12 |
| 19 | 游戏反馈 VFX（骰子/金币/建房 juice）(inferred) | UI | MVP | Not Started | — | 3, 4, 5, 8 |
| 20 | 主菜单与房间大厅 (inferred) | UI | MVP | Not Started | — | 2 |
| 21 | 存档/读档 Save/Load (inferred) | Persistence | MVP | Not Started | — | 1, 2, 5, 6 |
| 22 | 音频 Audio SFX/BGM (inferred) | Audio | MVP | Not Started | — | 7 |
| 23 | 设置 & House Rules | Meta | Alpha | Not Started | — | 1, 5 |
| 24 | 教程/引导 (inferred) | Meta | Alpha | Not Started | — | 4, 5, 6 |
| 25 | 联网 Online（同步/匹配/重连） | Networking | Full Vision | Not Started | — | 几乎全部 |
| 26 | 地图编辑器 Map Editor | Meta | Full Vision | Not Started | — | 1 |

> **\*经济(5) Approved-with-followups**(re-review full 2026-06-02):5 前轮 blocker 全闭合,R2 落 2 诚实硬线 + 软化。留 followup(不阻塞下游开工):**P-1 ✅ RESOLVED 2026-06-03**(`/propagate-design-change` 闭合:board-data line 272/273/332/338/403 + entities.yaml line 646 套利 framing → 数据质量/数据错误,对齐经济 AC-42;grep 核验残留仅否定式);**OQ-Econ-10** SalaryAmount 上界派发棋盘(1)/编辑器(26);Recommended AC(CR-4 买地/AC-43 fixture/破产终态/F-4 clamp/fee_den 校验)下一 pass 处理。

> **†移动(4) APPROVED**(fresh re-review full 2026-06-02,6 agent opus + CD;propagate 闭合 2026-06-02):本档内部已收敛(R3 改 3 处)。CD 裁定的剩余真 blocker(system-of-systems propagate 债,处方=propagate 非 RETREAT)已经 producer 协调 `/propagate-design-change` + grep 逐档核实全部落地 → **移动 APPROVED**:**① ✅** economy.md line 57/102/330 旧 push 清理为 PULL + Interactions 玩家回合行补 holder 源(holder owner=回合2);**② ✅** player-turn 补 AC-46(SentToJail 抑制)+ CR-3.1/Edge Cases 规则,实 AC 号回填 movement AC-15;**③ ✅** player-turn 补 AC-47(程间非重入,CR-3.1 承接 CR-4);**④ ✅** board-data 补 AC-23j + Validation + Tuning(`DiceMultiplierTable[i]` 上界,OQ-Move-6)。OQ-Move-5/6 均 RESOLVED。

> **‡地产所有权(6) Designed(pending review,2026-06-03)**:8 必需节 + Visual/UI/Open Questions 全写,41 AC(31 [Logic])。lean 模式(CD-GDD-ALIGN 跳过,Player Fantasy framing 待人工复核)。**须 fresh session 跑 `/design-review design/gdd/property-ownership.md`。** 关键裁定:**house_count 归建房(8)、6 不持**(避 6↔8 环);**is_monopoly 由 6 算**(查 board 组成员 + owner map);**买地/抵押事务由 6 拥有、调 economy `Debit`/`Credit`(6→5),economy 不反调 6**(解 5↔6 环)。**设计揪出 3 条 producer propagate 债**:**OQ-Prop-2** economy CR-4/CR-5/F-11 反向调用措辞(5→6,与 6→5 成 5↔6 环);**OQ-Prop-3** economy registry `property_rent`「house_count 由6供」应改「8 供」;**OQ-Prop-4** index `9 depends-on` 补 6(破产移交)、`16 depends-on` 可能补 6。registry 已注册 `is_monopoly`/`station_count`/`utility_count`(6-owned)。

---

## Inherited Test Obligations (跨系统测试义务追踪)

> **目的(player-turn R2 / qa-lead BLK-3 引入):** 上游 GDD 审查时产生的"下游须含某集成测试"承诺(OQ-T-*),若只写在上游 GDD 的 Open Questions 里,下游 GDD 作者不会读到 → 责任真空(board-data 教训)。此表使承诺在**下游系统这一行**可见。**`/design-review` 审查下游系统时,qa-lead 须核对本表对应义务已落入该 GDD 的 Acceptance Criteria。**

| 下游系统 | 继承义务 | 来源 | 回链 AC |
|---|---|---|---|
| 玩家回合(2) | **(dice OQ-T-Dice-1)** 开局定序调 `RollDice()` 只取 `Total`、忽略 `bIsDouble`、不进双点链(已在 player-turn CR-2),并须告知 VFX"当前=定序阶段"语境使其抑制定序双点强化反馈;**(dice OQ-T-Dice-5 ✅ propagate RESOLVED 2026-06-02)** AC-34 序列化字段已从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`(含 Die1/Die2);player-turn 7 处 RECEIVE+SERIALIZE 契约已更新(IG-1 已执行);**(movement OQ-T-Move-1)** ResolvePhase 须消费 `OnPawnLanded.arrivalContext`,`SentToJail` 时**不触发**买地/收租结算分支(movement AC-15 抑制侧归 2)。**✅ propagate RESOLVED(2026-06-02,grep 核实)**:此前 player-turn.md 全文 grep `SentToJail`/`arrivalContext`/`OnPawnLanded` = 0 命中、核心规则("去坐牢却能买落地格地")无 BLOCKING 覆盖;现 player-turn 已补 **AC-46**(`arrivalContext==SentToJail`→ResolvePhase 买地决策/收租结算入口 hook 调用次数==0,对照组 DiceMove==1)+ **AC-47**(程间非重入)+ **CR-3.1**(holder + 程间原子性)+ Edge Cases 抑制规则;实 AC 号已回填 movement AC-15。movement 侧透传由 AC-12b[Logic] 覆盖 | dice OQ-T-Dice-1 / OQ-T-Dice-5 / movement OQ-T-Move-1 | player-turn 定序 AC / AC-34(已扩字段) / **✅ OQ-T-Move-1 RESOLVED → player-turn AC-46 / AC-47** |
| 事件格(7) | "双点出狱不进双点链"集成测试(player-turn OQ-T-1);**(dice OQ-T-Dice-3)** 掷双点出狱调 `RollDice()` 取 `bIsDouble`、出狱双点不进双点链、骰子只提供原值不解释出狱语境;**(economy OQ-T-Econ-3)** 须把"前进到最近公用/车站"额外骰点作 dice_total 传入经济 F-4、机会/命运卡金钱效果经经济 Credit/Debit;**(movement OQ-T-Move-2)** 须经移动 `TeleportTo(paysGo=true)` 实现"前进到最近X"、`TeleportTo(JailIndex,paysGo=false)` 实现去坐牢,并保证 `target∈[0,N−1]` | player-turn OQ-T-1 / dice OQ-T-Dice-3 / economy OQ-T-Econ-3 / movement OQ-T-Move-2 | AC-31 / (7)GDD 待写 |
| 存档(21) | "中途还原精确阶段 + ConsecutiveDoubles + 锁定阈值 + **完整 `FDiceRollResult`(含 Die1/Die2,不可只存 `(Total,bIsDouble)`——否则读档后 VFX 无法重现骰面;dice OQ-T-Dice-5/B1)** + 读档后重绑 delegate 监听器"集成测试;**(economy OQ-T-Econ-4)** 序列化每玩家 `Cash`;Raising Funds 瞬态不中途存档(与回合2协同,OQ-Econ-4) | player-turn OQ-T-2 / dice OQ-T-Dice-5 / economy OQ-T-Econ-4 | AC-34 |
| AI 对手(10) | ① "PostRollAction AI 同步决策正常完成 + 回合顺畅移交"集成测试;② "AI 内部 RNG 仅走骰子(3) `RandomRange`/`RandomFloat01`、禁引擎全局随机" code-review checklist + 集成测试(**dice OQ-T-Dice-2**:可重放路径优先整数 `RandomRange`,但与 `RandomFloat01` 跨平台同源、Full Vision 联网须服务器权威,见 dice F-4);③ GameStateSnapshot 字段齐备验证;④ **(R3)定义并回填 AC-37b 的 N(移交帧数上限)**;⑤ **(R3)钉"三档 AIDifficulty 产生玩家可感知行为差异"契约(OQ-6)** | player-turn OQ-T-3 / OQ-6 / dice OQ-T-Dice-2 | AC-37b |
| VFX(19) | **(dice OQ-T-Dice-4,B5/rec-11,CD R2 裁定 MUST-FULFILL 移交)** 掷骰爽感端到端 owner(期待→翻滚→定格节奏 + 双点强化反馈)+ 背 experiential acceptance criteria;用 `Die1`/`Die2` 分别呈现两骰面值(禁从 `Total` 拆解);双点强化反馈差异化(庆祝 vs 入狱警告)须从 player-turn 取 `ConsecutiveDoubles`、不可仅凭裸 `bIsDouble`;定序阶段抑制双点强化反馈(语境由 player-turn 告知)。**qa-lead 在 VFX(19) design-review 时须核对其 AC 已含此承接,未承接 = VFX(19) GDD 的 blocking 缺口**;**(economy Visual节)** 收租金币飞溅/发薪入账/破产清算 juice 端到端 owner(监听 `OnRentPaid`/`OnCashChanged`/`OnBankruptcyDeclared`,concept Sensation priority-4);**(movement OQ-T-Move-3)** 棋子逐格 hop 回放端到端 owner(监听 `OnPawnMoveStarted` + 过 GO 高亮 + `OnPawnLanded` 落地 juice);hop path 由 VFX 自建 vs 移动提供 `HopPath` 归 VFX 裁定(movement OQ-Move-2) | dice OQ-T-Dice-4 / economy Visual节 / movement OQ-T-Move-3 | (19)GDD 待写 |
| HUD(16) | "OnPhaseChanged/OnTurnStarted 驱动 UI 高亮(≤100ms 起/≤500ms 完成)" + "OnAIActionExecuted(ActionIndex) 非阻塞逐步播放 AI 行动(按 ActionIndex 排序)" + "OnTurnOrderResolved 席位裁定可见"集成测试;**(R3 B-2)Pass'N Play handoff 高亮与前一回合 outro 不叠帧**;**(R6 RETREAT,取代 R5 计数收敛)框架不再 gating——HUD 自主非阻塞呈现 AI 行动,不阻塞框架推进;删除原 OnAIActionPlaybackSegmentComplete 回放完成回调义务**;消费 OnTurnEnded(回合切换过场,payload bGrantsExtraTurn 区分同玩家继续/移交,两分支须各有动画 AC) | player-turn OQ-T-4 + Fantasy→HUD 契约 | AC-36 / AC-41 / AC-44 呈现侧 |
| 地产所有权(6) | **(economy OQ-T-Econ-1)** 须实现 owner/house_count/is_monopoly/station_count/utility_count/is_mortgaged 接口供经济算租,**且经 push 模型把归属快照作参数传入经济算租调用**(经济 Dep 双向一致性①,保 index 无环);字段供给正确性 + push 快照时机归 6 | economy OQ-T-Econ-1 | (6)GDD 待写 |
| 破产胜负(9) | **(economy OQ-T-Econ-2 🔴)** 资产移交口径须 verbatim 用经济 F-9 net_liquidation_value(AC-27 共享不变式下游侧),9 的 AC 须断言"移交总额==F-9 值";局末排名 net worth 口径(可能含未抵押 face value,≠nlv)归 9 裁定(OQ-Econ-5) | economy OQ-T-Econ-2 / OQ-Econ-5 | (9)GDD 待写 |
| 建房(8) | **(economy OQ-T-Econ-5)** F-8 building_sellback 比率归经济、建筑清单归 8 供 F-8/F-9 枚举;建/卖房现金经经济执行 | economy OQ-T-Econ-5 | (8)GDD 待写 |
| 经济(5) | **(movement OQ-T-Move-4，R2 Finding E 改 PULL)** 消费移动 push 的 `(passedGo, SalaryAmount)`:`passedGo>0` 触发 F-1 发薪(`passedGo×SalaryAmount`)、`passedGo≤0`/无 push 不发薪——发薪部分**已由 economy AC-6/AC-7 履行**。**Utility 租 `dice_total` 改为经济在 ResolvePhase 从回合掷骰上下文 PULL**(movement 不投递 Total，对齐 economy AC-18 不缓存)——经济须保证"当前正在结算这一程" `Total` **单源**、连续双点链/事件额外骰(OQ-T-Econ-3)下不取错值。**holder 机制 + 单源去歧义 = OQ-Move-5(跨 doc propagate，涉回合2/经济5/事件7);另含 `DiceMultiplierTable[i]` 上界校验防 `12×mult` int32 溢出(OQ-Econ-10 不覆盖)**。movement 侧只断言不投递(AC-7b)。**✅ propagate RESOLVED 2026-06-02:OQ-Move-5 holder 落 player-turn CR-3.1 + economy line 57/102/330 PULL 清理 + economy Interactions 玩家回合行 holder 源;`DiceMultiplierTable` 上界落 board-data AC-23j(economy OQ-Econ-10 并轨注)。** | movement OQ-T-Move-4 / OQ-Move-5 | economy AC-6 / AC-7（发薪）/ F-4 / **AC-18（dice_total 不缓存，PULL 契约）/ player-turn CR-3.1(holder)** |

---

## Categories

| Category | Description | 本作系统 |
|----------|-------------|----------|
| **Core** | 框架级地基，几乎所有系统依赖 | 棋盘数据、玩家回合、骰子 |
| **Gameplay** | 让游戏好玩的机制 | 移动、事件格、破产胜负、AI、命运之轮、俄罗斯轮盘、策略卡 |
| **Economy** | 资源/金钱的产生与消耗 | 经济现金、地产所有权、建房升级、交易、拍卖 |
| **UI** | 面向玩家的信息显示与反馈 | HUD、地产卡 UI、交易拍卖 UI、VFX、主菜单大厅 |
| **Persistence** | 存档与连续性 | 存档/读档 |
| **Audio** | 音效与音乐 | 音频 SFX/BGM |
| **Meta** | 核心循环之外的系统 | 设置&House Rules、教程、地图编辑器 |
| **Networking** | 多人联网（Full Vision） | 联网 |

---

## Priority Tiers

| Tier | Definition | Target Milestone | Design Urgency |
|------|------------|------------------|----------------|
| **MVP** | 核心循环运转所需。没有它们无法验证"是否好玩" | 首个可玩原型（本地+AI） | Design FIRST |
| **Alpha** | 全部特色机制（拍卖/命运之轮/俄罗斯轮盘/策略卡）就位 | Alpha 里程碑 | Design SECOND |
| **Full Vision** | 联网、地图编辑器、打磨 | Beta / Release | Design as needed |

> 本作未单列 Vertical Slice 系统层——垂直切片复用 MVP 系统集 + 一项特色机制（拍卖），不引入新系统。

---

## Dependency Map

### Foundation Layer（近零依赖）

1. 棋盘数据（1）— 格子布局/地图定义，所有空间逻辑的地基（零依赖）
2. 玩家与回合管理（2）— 2–8 玩家、回合顺序、当前玩家，流程框架。**depends-on: 3**（player-turn GDD 接口分析发现：RollPhase/双点 F-3/定序 F-4 均消费骰子输出；原标零依赖，已修正）
3. 骰子（3）— 随机数与掷骰结果，移动与多个事件的输入源（零依赖纯 RNG 服务）

> **设计顺序提示（R 发现）**：系统#2 依赖#3，但设计顺序中 #2 先于 #3。因 #3 是 S 级纯 RNG 服务且 #2 仅消费 `(点数, bIsDouble)` 契约（已在 player-turn GDD 定义），契约清晰、无返工风险。建议下一个设计 **骰子(3)** 以闭合此契约。

### Core Layer（依赖 Foundation）

1. 经济与现金（5）— depends on: 1, 2 ｜⚠ bottleneck：租金/抵押/建房公式发源地（economy GDD Phase 5 补 +1：经 GetTileData 读金额 base）
2. 移动（4）— depends on: 1, 2, 3｜**orchestrated → 经济(5)**：移动把 `(passedGo, SalaryAmount)` 在回合编排下 push 给经济发薪；Utility 租 `dice_total` 改由经济在 ResolvePhase 从回合掷骰上下文 **PULL**（移动不投递 Total，R2 Finding E 对齐 economy AC-18 不缓存）。非硬依赖、不成环（movement GDD R2 2026-06-02）
3. 地产所有权（6）— depends on: 1, 5
4. 事件格（7）— depends on: 1, 2, 3, 4, 5（+3：掷双点出狱调骰子，dice GDD Phase 5 同步；**+4：调移动 `TeleportTo` 实现"前进到最近X"/去坐牢，movement GDD R-move 2026-06-02 同步**）

### Feature Layer（依赖 Core）

1. 建房升级（8）— depends on: 1, 5, 6
2. 破产与胜负（9）— depends on: 2, 5
3. AI 对手（10）— depends on: 3, 4, 5, 6, 7, 8 ｜⚠ 消费几乎所有 gameplay（+3：AI 随机走骰子可种子化 RNG，dice GDD Phase 5 同步）
4. 交易（11）— depends on: 5, 6
5. 拍卖（12）— depends on: 5, 6
6. 命运之轮（13）— depends on: 5, 7
7. 俄罗斯轮盘（14）— depends on: 2, 9
8. 策略卡（15）— depends on: 5, 6, 7

### Presentation Layer（包裹 gameplay）

1. HUD（16）— depends on: 1, 2, 5
2. 地产卡与卡牌 UI（17）— depends on: 1, 6, 7
3. 交易/拍卖 UI（18）— depends on: 11, 12
4. 游戏反馈 VFX（19）— depends on: 3, 4, 5, 8（+3：监听 OnDiceRolled 读 Die1/Die2/Total 呈现两骰，dice GDD R2 同步）
5. 主菜单与房间大厅（20）— depends on: 2

### Polish Layer（依赖广泛）

1. 存档/读档（21）— depends on: 1, 2, 5, 6（横切所有状态系统）
2. 音频（22）— depends on: 7（事件触发）
3. 设置 & House Rules（23）— depends on: 1, 5
4. 教程/引导（24）— depends on: 4, 5, 6
5. 联网（25）— depends on: 几乎全部 ｜⚠ Full Vision 大重构
6. 地图编辑器（26）— depends on: 1

---

## Recommended Design Order

| Order | System | Priority | Layer | Agent(s) | Est. Effort |
|-------|--------|----------|-------|----------|-------------|
| 1 | 棋盘数据（1） | MVP | Foundation | game-designer | M |
| 2 | 玩家与回合管理（2） | MVP | Foundation | game-designer / systems-designer | M |
| 3 | 骰子（3） | MVP | Foundation | game-designer | S |
| 4 | 经济与现金（5） | MVP | Core | economy-designer | L |
| 5 | 移动（4） | MVP | Core | game-designer | M |
| 6 | 地产所有权（6） | MVP | Core | economy-designer | M |
| 7 | 事件格（7） | MVP | Core | game-designer | M |
| 8 | 建房升级（8） | MVP | Feature | economy-designer | M |
| 9 | 破产与胜负（9） | MVP | Feature | systems-designer | S |
| 10 | AI 对手（10） | MVP | Feature | systems-designer / ai-programmer | L |
| 11 | HUD（16） | MVP | Presentation | ux-designer | M |
| 12 | 地产卡与卡牌 UI（17） | MVP | Presentation | ux-designer | M |
| 13 | 游戏反馈 VFX（19） | MVP | Presentation | technical-artist | M |
| 14 | 主菜单与房间大厅（20） | MVP | Presentation | ux-designer | S |
| 15 | 存档/读档（21） | MVP | Polish | systems-designer | M |
| 16 | 音频（22） | MVP | Polish | sound-designer | S |
| 17 | 交易（11） | Alpha | Feature | economy-designer | M |
| 18 | 拍卖（12） | Alpha | Feature | economy-designer | M |
| 19 | 命运之轮（13） | Alpha | Feature | game-designer | S |
| 20 | 俄罗斯轮盘（14） | Alpha | Feature | game-designer | S |
| 21 | 策略卡（15） | Alpha | Feature | systems-designer | M |
| 22 | 交易/拍卖 UI（18） | Alpha | Presentation | ux-designer | M |
| 23 | 设置 & House Rules（23） | Alpha | Meta | game-designer | S |
| 24 | 教程/引导（24） | Alpha | Meta | game-designer | M |
| 25 | 联网（25） | Full Vision | Networking | network-programmer | L |
| 26 | 地图编辑器（26） | Full Vision | Meta | game-designer | L |

> Effort：S = 1 个会话，M = 2–3 个会话，L = 4+ 个会话。同层独立系统可并行设计。

---

## Circular Dependencies

- **None found**。依赖图为有向无环：UI 依赖 gameplay 而 gameplay 不反向依赖 UI；AI 消费多系统但不被它们依赖；存档依赖状态系统而非反向。无需打破环。

---

## High-Risk Systems

| System | Risk Type | Risk Description | Mitigation |
|--------|-----------|-----------------|------------|
| 经济与现金（5） | Design | 租金/抵押/建房公式是 bottleneck，平衡错误会让全盘玩法崩坏；且"一局拖沓"通病源于经济平衡 | 早原型调参（破产加速/局长可调），公式数据驱动 |
| AI 对手（10） | Design / Technical | MVP 靠 vs AI 验证，AI 太蠢直接毁掉核心体验；需可靠的地产估值与现金流决策 | 早原型，分难度档；估值逻辑数据驱动 |
| 联网（25） | Technical / Scope | 状态同步、回合权威、断线重连，是全项目最大技术风险 | 严格隔离到 Full Vision；先单机验证全玩法，联网作为叠加层 |
| 存档/读档（21） | Technical | 横切所有状态系统，序列化遗漏会导致读档损坏 | 状态系统设计时即定义可序列化契约 |

---

## Progress Tracker

| Metric | Count |
|--------|-------|
| Total systems identified | 26 |
| Design docs started | 6 |
| Design docs reviewed | 5 |
| Design docs approved | 5 |
| MVP systems designed | 6 / 16 |
| Alpha systems designed | 0 / 8 |
| Full Vision systems designed | 0 / 2 |

---

## Next Steps

- [ ] Design MVP-tier systems first（按设计顺序，从 棋盘数据 开始，用 `/design-system`）
- [ ] 优先验证高风险系统：经济现金、AI（早原型）
- [ ] Run `/design-review design/gdd/[system].md` on each completed GDD
- [ ] Run `/gate-check systems-design` when all MVP GDDs are complete
- [ ] Validate highest-risk systems with `/vertical-slice` before committing to Production
