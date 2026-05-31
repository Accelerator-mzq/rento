# Game Concept: 骰子大亨 (Dice Tycoon, 工作标题)

*Created: 2026-05-31*
*Status: Draft*

> 本作是对 Steam《Rento Fortune: Online Dice Board Game (大富翁)》的复刻(clone)。
> 设计目标是**还原原作玩法、规则与体验**,不做创意发散。本文档据此收敛。

---

## Elevator Pitch

> 一款 2–8 人的数字骰子棋盘游戏:掷骰移动、买地建房收租,用拍卖、命运之轮、
> 俄罗斯轮盘和策略卡互相博弈,目标是垄断棋盘、逼所有对手破产。
>
> 它就是经典大富翁(Monopoly)的数字化,叠加 Rento Fortune 的高风险赌博机制
> 与策略卡——运气的刺激与算计的掌控交织。

---

## Core Identity

| Aspect | Detail |
| ---- | ---- |
| **Genre** | 数字桌游 / 回合制商业博弈(Monopoly-like dice board game) |
| **Platform** | PC(Steam),键鼠为主 |
| **Target Audience** | 见下方 Target Player Profile |
| **Player Count** | MVP:本地 2–4(Pass'N Play + AI);完整愿景:2–8(在线多人) |
| **Session Length** | 约 30–60 分钟一局(目标:可调局长以压缩拖沓) |
| **Monetization** | none yet(以复刻学习为目标的项目) |
| **Estimated Scope** | Large(分层实现;周数待 `/estimate` 细化) |
| **Comparable Titles** | Rento Fortune(原作)、Monopoly Plus、Mario Party(部分派对感) |

---

## Core Fantasy

你是一个精明的地产大亨:用骰子的运气和算计的眼光,把一整块棋盘变成你的垄断帝国,
看着对手一个个付不起租金、相继破产出局,最后只剩你笑到最后。

这个幻想的情感内核是**"在朋友之间又坑又赢的爽快"**——既有滚雪球碾压的成长快感,
也有逆风翻盘、靠一次命运之轮/俄罗斯轮盘绝处逢生的戏剧性。它不是孤独的经济模拟,
而是围坐一桌、互相使坏的社交博弈。

---

## Unique Hook

> 诚实说明:作为复刻作,本作没有"原创新机制"。它的钩子是**忠实还原 Rento Fortune
> 在经典大富翁之上叠加的那套组合**。

"它像 Monopoly,**AND ALSO** 把命运之轮(wheel of fortune)、俄罗斯轮盘
(Russian Roulette)这种高风险赌博,和一手只能用一次的策略卡(strategy cards),
叠进经典的买地—收租—建房循环里。" ——运气的刺激被放大,但策略卡和交易/拍卖
又给了玩家对抗运气的掌控手段,二者交织是体验的核心。

---

## Player Experience Analysis (MDA Framework)

### Target Aesthetics (What the player FEELS)

| Aesthetic | Priority | How We Deliver It |
| ---- | ---- | ---- |
| **Sensation**(感官愉悦) | 4 | 掷骰、建房、收租金币飞溅的"juice";明亮饱和的桌面视觉 |
| **Fantasy**(扮演) | 5 | 地产大亨的身份幻想(轻量,非叙事驱动) |
| **Narrative**(叙事) | N/A | 无剧情;故事来自每局玩家自己制造的博弈过程 |
| **Challenge**(挑战/精通) | 3 | 交易、建房时机、策略卡运用的策略深度;AI 难度分级 |
| **Fellowship**(社交连接) | **1** | 核心:与朋友同屏/在线对战、互坑、交易、拍卖 |
| **Discovery**(探索) | N/A | 无探索维度 |
| **Expression**(自我表达) | 6 | 皮肤、自定义棋子/骰子、(后期)地图编辑器 |
| **Submission**(放松) | 2 | 易上手、低压力、随时开一局的休闲派对体验 |

### Key Dynamics(期望涌现的玩家行为)

- 玩家会自发地**结盟与背叛**:临时联手压制领先者,再在关键时刻反水。
- 玩家会**围绕同色地产组讨价还价**,形成交易博弈。
- 落后玩家会**赌一把高风险机制**(俄罗斯轮盘/命运之轮)寻求翻盘。
- 玩家会**留存策略卡到关键回合**,制造戏剧性反转。

### Core Mechanics(我们实际构建的系统)

1. **骰子移动系统**:掷骰、沿棋盘格移动、经过/落在格子的结算。
2. **地产经济系统**:购买、付租、集齐同色组、建房/酒店升级、抵押。
3. **事件系统**:机会/命运卡、税收、监狱、起点过路费。
4. **博弈机制系统**(Alpha 层):拍卖、命运之轮、俄罗斯轮盘、策略卡。
5. **回合与胜负系统**:回合流转、破产判定、最后存活者获胜。

---

## Player Motivation Profile

### Primary Psychological Needs Served

| Need | How This Game Satisfies It | Strength |
| ---- | ---- | ---- |
| **Autonomy**(自主) | 买/不买、建房时机、交易报价、策略卡使用——大量有意义的选择 | Core |
| **Competence**(精通) | 学会评估地产价值、控制现金流、把握建房与交易时机;战胜更高难度 AI | Supporting |
| **Relatedness**(连接) | 与朋友同屏/在线对战的社交互动、互坑与谈判 | Core |

### Player Type Appeal (Bartle Taxonomy)

- [x] **Achievers**:垄断棋盘、逼破产对手、解锁皮肤与棋盘——明确的达成目标。
- [ ] **Explorers**:本作探索维度弱(地图编辑器算轻量创造,非探索)。
- [x] **Socializers**:核心受众——和朋友一起玩、谈判、使坏。
- [x] **Killers/Competitors**:压制并淘汰对手、(后期)在线排名。

### Flow State Design

- **Onboarding curve**:前 10 分钟用熟悉的"掷骰-买地"循环自然教学;首局可开提示。
- **Difficulty scaling**:AI 分级(休闲/普通/精明);玩家通过策略而非反应取胜。
- **Feedback clarity**:现金、资产、租金涨幅清晰可视;每次收租/付租有明确反馈。
- **Recovery from failure**:破产即出局,但单局短(可调),"再来一局"成本低。

---

## Core Loop

### Moment-to-Moment (30 seconds)
掷骰 → 棋子移动 → 落地决策:买地 / 付租 / 抽机会命运卡 / 触发事件(税收、监狱、起点)。
这一动作必须本身就爽——靠掷骰的期待感 + 落地结算的明确反馈(金币、建房、地产变色)。

### Short-Term (5-15 minutes)
绕棋盘一圈圈累积:逐步集齐同色地产组、建房升级抬高租金、与对手交易/拍卖博弈。
"再走一圈说不定就集齐了"的心理在此层运转。

### Session-Level (30-120 minutes)
一局从所有人平等起步,到有人率先垄断、现金流碾压,对手陆续破产,直到决出唯一赢家。
自然结束点 = 一局分胜负;回来的理由 = 换对手/换棋盘/赢回上一局。

### Long-Term Progression
跨多局:解锁更多棋盘与皮肤、挑战更高 AI 难度、(Full Vision 后)在线对战与排名。
"完成"标准:本作是可反复重玩的对战游戏,无固定终点。

### Retention Hooks
- **Curiosity**:未解锁的棋盘/皮肤、没试过的策略卡组合。
- **Investment**:对战记录、解锁进度、提升的对 AI 胜率。
- **Social**:朋友约局(同屏 / 后期在线)。
- **Mastery**:更精的交易与建房决策、战胜更高难度 AI、(后期)爬排名。

---

## Game Pillars

### Pillar 1: 桌游忠实(Board-Game Faithful)
还原经典大富翁的规则、节奏与手感,让熟悉桌游的玩家立刻上手。

*Design test*:在"更接近原版桌游体验"与"自创新规则"之间纠结时,选前者。

### Pillar 2: 社交博弈 / 互坑(Social Mischief)
乐趣的源头是玩家之间的收租、交易、拍卖与使坏,而非独自最优解。

*Design test*:在"提升单人最优策略"与"制造玩家间互动/冲突"之间,选后者。

### Pillar 3: 运气 × 策略交织(Luck Meets Strategy)
骰子、命运之轮、俄罗斯轮盘提供运气的刺激;建房、交易、策略卡提供对抗运气的掌控。

*Design test*:在"纯运气"与"纯策略"之间,永远选两者交织——既不让运气碾压算计,也不让算计消灭刺激。

### Pillar 4: 易上手轻松局(Pick-Up Accessible)
规则清晰、一局时长可控、随时能和朋友开一局。

*Design test*:在"增加策略深度"与"保持易懂不吓跑休闲玩家"之间,优先后者。

### Anti-Pillars (What This Game Is NOT)

- **NOT 硬核经济模拟**:不做复杂的市场/供需/利率系统——那会破坏 Pillar 4 的易上手。
- **NOT 强叙事剧情**:不做主线故事——故事来自玩家每局自己制造的博弈,叙事系统会稀释社交焦点。
- **NOT 实时动作**:纯回合制,不引入任何实时操作——与桌游忠实(Pillar 1)冲突。
- **NOT 在 MVP 阶段做联网**:联网是最大技术风险,放 Full Vision;过早做会拖垮核心玩法验证。

---

## Inspiration and References

| Reference | What We Take From It | What We Do Differently | Why It Matters |
| ---- | ---- | ---- | ---- |
| Rento Fortune(原作) | 完整玩法蓝本:四种模式、命运之轮、俄罗斯轮盘、策略卡、地图编辑器 | 自定义地块命名以规避 IP;MVP 先单机 | 这是复刻的直接目标,定义"一致"的标准 |
| Monopoly Plus | 经典规则的数字化呈现、建房/抵押 UI 范式 | 叠加 Rento 的赌博机制与策略卡 | 验证经典规则数字化的成熟做法 |
| Mario Party | 派对感、随机事件制造的戏剧性与欢乐氛围 | 我们是地产博弈而非小游戏合集 | 印证"运气+社交"对休闲多人的吸引力 |

**Non-game inspirations**:实体桌游聚会的氛围——灯光、围坐、互相调侃使坏的社交感,
是视觉与音频基调的来源。

---

## Target Player Profile

| Attribute | Detail |
| ---- | ---- |
| **Age range** | 18–40 |
| **Gaming experience** | 休闲 ~ 中核(Casual to Mid-core) |
| **Time availability** | 平日晚上 30–60 分钟一局,周末可连开多局 |
| **Platform preference** | PC / Steam |
| **Current games they play** | Monopoly Plus、Rento Fortune、Mario Party、各类派对桌游 |
| **What they're looking for** | 能随时和朋友开一局、又坑又欢乐的数字桌游 |
| **What would turn them away** | 规则太复杂、一局太长太拖、纯看运气毫无策略、上手门槛高 |

---

## Technical Considerations

| Consideration | Assessment |
| ---- | ---- |
| **Recommended Engine** | **Unreal Engine 5.7**(项目已切换并 pin,Blueprint 为主 + C++ 框架)。注:UE 的 3D/写实强项本项目用不到,对 2.5D 卡通棋盘偏重;Lumen/Nanite 大概率不启用以降成本与构建体积(架构阶段定 ADR) |
| **Key Technical Challenges** | (1) AI 对手要"像人"且不犯蠢;(2) 棋盘/经济状态的清晰建模与可存档;(3) Full Vision 的联网(状态同步、回合权威、断线重连) |
| **Art Style** | 2.5D / 3D stylized,休闲圆润卡通,明亮饱和 |
| **Art Pipeline Complexity** | Medium(自定义 2D/2.5D 资产 + 棋盘/卡牌/UI) |
| **Audio Needs** | Moderate(骰子、金币、建房、事件音效 + 轻快 BGM) |
| **Networking** | MVP:None;Full Vision:Client-Server(权威服务器,防作弊) |
| **Content Volume** | MVP:1 张棋盘、~16 机会命运卡、3 档 AI;Full Vision:多棋盘、全套博弈机制、地图编辑器 |
| **Procedural Systems** | 无(棋盘为手工设计;地图编辑器是玩家创作而非程序生成) |

---

## Risks and Open Questions

### Design Risks
- 运气占比过高,可能让偏策略的玩家感到挫败 → 需用策略卡/交易/建房时机平衡。
- 一局过长、后期翻盘无望时拖沓(经典大富翁通病)→ 需可调局长、破产加速或回合上限。

### Technical Risks
- AI 对手做得太蠢会破坏单机体验(MVP 核心是 vs AI)→ 需可靠的估值与决策逻辑。
- Full Vision 的联网(同步/权威/重连)是最大技术风险 → 严格隔离到最后一层,先单机验证玩法。

### Market Risks
- 大富翁类竞品密集(Monopoly 官方、Rento 原作本身),复刻无差异化 → 定位为学习/自用项目,商业预期保守。
- **法律/IP 风险**:玩法不受版权保护(可复刻),但"Monopoly"商标、原版棋盘的具体地名/文字、Rento 美术资产均受保护 → MVP 必须使用**自定义地块命名与原创美术**。

### Scope Risks
- Full Vision(联网 + 地图编辑器 + 多棋盘/皮肤)体量大 → 必须严格按 MVP → Alpha → Full Vision 分层推进。

### Open Questions
- 单局目标时长定多少?用什么规则压缩拖沓(破产加速 / 回合上限 / 起始资金调节)?→ MVP 原型期实测调参。
- AI 难度分级如何实现(估值权重?现金流阈值?)?→ `/design-system` 阶段细化 AI GDD。
- 法律边界确认:哪些地块命名/视觉绝对要避开?→ 进入美术与内容阶段前先确认。

---

## MVP Definition

**Core hypothesis**:"经典的掷骰—买地—收租—建房—破产循环 + 基础机会命运卡,
在本地/AI 对战(2–4 人)下,一局 30 分钟左右是好玩的。"

**Required for MVP**:
1. 单张棋盘 + 掷骰移动 + 落地结算
2. 地产购买、付租、集齐同色组、建房/酒店升级、抵押
3. 机会/命运卡、税收、监狱、起点过路费
4. 破产判定 + 最后存活者获胜
5. **Pass'N Play(同屏轮流)+ 简单 AI(SOLO)**,2–4 人
6. 基础 UI(现金/资产/地产状态可读)+ 单局存档

**Explicitly NOT in MVP**(后置):
- 联网(ONLINE / WiFi-LAN)
- 拍卖、命运之轮、俄罗斯轮盘、策略卡
- house rules 设置、多棋盘、皮肤、地图编辑器
- 在线排名 / 账号系统

### Scope Tiers (if budget/time shrinks)

| Tier | Content | Features | Timeline |
| ---- | ---- | ---- | ---- |
| **MVP** | 单棋盘,本地 + AI | 核心循环:掷骰/买地/收租/建房/机会卡/破产 | 待 `/estimate` |
| **Vertical Slice** | 一张打磨好的棋盘 | 核心循环 + 一项特色机制(如拍卖)+ 完整一局体验 | 待 `/estimate` |
| **Alpha** | 多棋盘,本地 + AI | 全特色机制:拍卖/命运之轮/俄罗斯轮盘/策略卡 + house rules | 待 `/estimate` |
| **Full Vision** | 完整内容 | ONLINE 联网 + 地图编辑器 + 皮肤 + 2–8 人 | 待 `/estimate` |

---

## Visual Identity Anchor

> 本节是 art bible 的种子——锁定核心视觉决策,避免跨会话遗忘。

**视觉方向名称**:**明亮欢快的数字桌面(Bright Tabletop Cheer)**

**一句话视觉规则**:"像一盒灯光明亮、摆在桌上的实体大富翁——色彩饱和、棋子骰子有
玩具般的实体质感、斜俯视棋盘一眼可读。"

**支撑原则**:
1. **棋盘即舞台**:斜俯视角,地块/同色分组/玩家位置/资产状态一眼可读。
   *Design test*:在"可读性"与"炫酷视效"之间,选可读性。
2. **获得感视觉化**:建房、收租金币飞溅、破产清算都有夸张而满足的反馈("juice")。
   *Design test*:在"朴素呈现"与"有 juice 的反馈"之间,选 juice。
3. **休闲圆润卡通**:不写实、不黑暗,亲和友好。
   *Design test*:风格化方向有疑问时,选更明亮、更友好的一侧。

**色彩哲学**:高饱和、明快、欢乐;地产同色组沿用经典大富翁的色块传统,用鲜明色彩
区分,确保棋盘信息一目了然。

---

## Next Steps

- [x] Fill in CLAUDE.md technology stack based on engine choice (`/setup-engine` — 已完成:Unreal Engine 5.7 + Blueprint)
- [ ] Create the visual identity specification (`/art-bible` — 基于上方 Visual Identity Anchor)
- [ ] Validate concept completeness (`/design-review design/gdd/game-concept.md`)
- [ ] **Prototype core idea** (`/prototype` — 验证"掷骰-买地-收租-破产"核心循环是否好玩)
- [ ] If prototype PROCEEDS: Decompose concept into systems (`/map-systems`)
- [ ] Design each system (`/design-system [system-name]`)
- [ ] Build vertical slice in Pre-Production (`/vertical-slice`)
- [ ] Validate core loop with playtest (`/playtest-report`)
- [ ] Plan first milestone (`/sprint-plan new`)
