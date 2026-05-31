# Systems Index: 骰子大亨 (Dice Tycoon)

> **Status**: Draft
> **Created**: 2026-05-31
> **Last Updated**: 2026-05-31
> **Source Concept**: design/gdd/game-concept.md

---

## Overview

《骰子大亨》是对 Rento Fortune 的复刻——一款 2–8 人回合制数字骰子棋盘游戏。其机械范围围绕经典大富翁核心循环展开：掷骰移动、买地、收租、建房、逼对手破产；并叠加原作的四项特色博弈机制（拍卖、命运之轮、俄罗斯轮盘、策略卡）。系统分解的核心地基是 **经济现金系统**（租金/抵押/建房公式的发源地，几乎所有交互依赖它）与 **棋盘数据 / 玩家回合** 框架。MVP 聚焦本地 + AI 跑通核心循环，特色机制留待 Alpha，联网与地图编辑器留待 Full Vision。设计与实现严格按依赖顺序自底向上推进，呼应支柱①桌游忠实、②社交博弈、③运气×策略、④易上手。

---

## Systems Enumeration

| # | System Name | Category | Priority | Status | Design Doc | Depends On |
|---|-------------|----------|----------|--------|------------|------------|
| 1 | 棋盘数据 Board/Tile Data (inferred) | Core | MVP | Not Started | — | — |
| 2 | 玩家与回合管理 Player & Turn (inferred) | Core | MVP | Not Started | — | — |
| 3 | 骰子 Dice | Core | MVP | Not Started | — | — |
| 4 | 移动 Movement | Gameplay | MVP | Not Started | — | 1, 2, 3 |
| 5 | 经济与现金 Economy & Cash | Economy | MVP | Not Started | — | 2 |
| 6 | 地产所有权 Property Ownership | Economy | MVP | Not Started | — | 1, 5 |
| 7 | 事件格 Tile Events（机会/命运/税/监狱/起点） | Gameplay | MVP | Not Started | — | 1, 2, 5 |
| 8 | 建房升级 Building & Rent Scaling | Economy | MVP | Not Started | — | 5, 6 |
| 9 | 破产与胜负 Bankruptcy & Win | Gameplay | MVP | Not Started | — | 2, 5 |
| 10 | AI 对手 AI Opponent (inferred) | Gameplay | MVP | Not Started | — | 4, 5, 6, 7, 8 |
| 11 | 交易 Trading (inferred) | Economy | Alpha | Not Started | — | 5, 6 |
| 12 | 拍卖 Auction | Economy | Alpha | Not Started | — | 5, 6 |
| 13 | 命运之轮 Fortune Wheel | Gameplay | Alpha | Not Started | — | 5, 7 |
| 14 | 俄罗斯轮盘 Russian Roulette | Gameplay | Alpha | Not Started | — | 2, 9 |
| 15 | 策略卡 Strategy Cards | Gameplay | Alpha | Not Started | — | 5, 6, 7 |
| 16 | HUD（玩家面板/现金/回合）(inferred) | UI | MVP | Not Started | — | 2, 5 |
| 17 | 地产卡与卡牌 UI (inferred) | UI | MVP | Not Started | — | 6, 7 |
| 18 | 交易/拍卖 UI (inferred) | UI | Alpha | Not Started | — | 11, 12 |
| 19 | 游戏反馈 VFX（骰子/金币/建房 juice）(inferred) | UI | MVP | Not Started | — | 4, 5, 8 |
| 20 | 主菜单与房间大厅 (inferred) | UI | MVP | Not Started | — | 2 |
| 21 | 存档/读档 Save/Load (inferred) | Persistence | MVP | Not Started | — | 1, 2, 5, 6 |
| 22 | 音频 Audio SFX/BGM (inferred) | Audio | MVP | Not Started | — | 7 |
| 23 | 设置 & House Rules | Meta | Alpha | Not Started | — | 1, 5 |
| 24 | 教程/引导 (inferred) | Meta | Alpha | Not Started | — | 4, 5, 6 |
| 25 | 联网 Online（同步/匹配/重连） | Networking | Full Vision | Not Started | — | 几乎全部 |
| 26 | 地图编辑器 Map Editor | Meta | Full Vision | Not Started | — | 1 |

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

### Foundation Layer（零依赖）

1. 棋盘数据（1）— 格子布局/地图定义，所有空间逻辑的地基
2. 玩家与回合管理（2）— 2–8 玩家、回合顺序、当前玩家，流程框架
3. 骰子（3）— 随机数与掷骰结果，移动与多个事件的输入源

### Core Layer（依赖 Foundation）

1. 经济与现金（5）— depends on: 2 ｜⚠ bottleneck：租金/抵押/建房公式发源地
2. 移动（4）— depends on: 1, 2, 3
3. 地产所有权（6）— depends on: 1, 5
4. 事件格（7）— depends on: 1, 2, 5

### Feature Layer（依赖 Core）

1. 建房升级（8）— depends on: 5, 6
2. 破产与胜负（9）— depends on: 2, 5
3. AI 对手（10）— depends on: 4, 5, 6, 7, 8 ｜⚠ 消费几乎所有 gameplay
4. 交易（11）— depends on: 5, 6
5. 拍卖（12）— depends on: 5, 6
6. 命运之轮（13）— depends on: 5, 7
7. 俄罗斯轮盘（14）— depends on: 2, 9
8. 策略卡（15）— depends on: 5, 6, 7

### Presentation Layer（包裹 gameplay）

1. HUD（16）— depends on: 2, 5
2. 地产卡与卡牌 UI（17）— depends on: 6, 7
3. 交易/拍卖 UI（18）— depends on: 11, 12
4. 游戏反馈 VFX（19）— depends on: 4, 5, 8
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
| Design docs started | 0 |
| Design docs reviewed | 0 |
| Design docs approved | 0 |
| MVP systems designed | 0 / 16 |
| Alpha systems designed | 0 / 8 |
| Full Vision systems designed | 0 / 2 |

---

## Next Steps

- [ ] Design MVP-tier systems first（按设计顺序，从 棋盘数据 开始，用 `/design-system`）
- [ ] 优先验证高风险系统：经济现金、AI（早原型）
- [ ] Run `/design-review design/gdd/[system].md` on each completed GDD
- [ ] Run `/gate-check systems-design` when all MVP GDDs are complete
- [ ] Validate highest-risk systems with `/vertical-slice` before committing to Production
