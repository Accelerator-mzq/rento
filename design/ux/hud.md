---
Status: In Design
Last Updated: 2026-06-06
Template: HUD Design
Screen: HUD（局内抬头显示）
GDD Source: design/gdd/hud.md (Approved R-3, 2026-06-05)
Author: msc + ux-designer agent
---

# HUD UX Spec — 骰子大亨 (Dice Tycoon)

## HUD Philosophy

《骰子大亨》的 HUD 是**零认知负担的可读机器**——它把玩家需要知道的一切放在固定位置，用大而稳的数字和清晰的视觉层级呈现，让玩家永远不必费力搜寻答案。

核心哲学三条：

**1. 可读性第一（art-bible §7.1）**
HUD 是重度 UI 游戏。现金、资产、回合状态在任何时刻非 hover 即可读，不藏在二级菜单里。字体层级 L1–L4、最小 10px、等宽数字（art-bible §7.2），是可读性的物质基础。

**2. 呈现层纯叶子**
HUD 只读、只显、只监听。它不写游戏状态，不驱动流程，不裁决结果（GDD 核心约束 CR-14）。所有按钮点击转发意图给 owner 系统；所有显示值来自 owner 快照和事件。这条哲学决定了本 spec 的每一个数据标注——凡是 Write 项都是 architectural concern，须 flag。

**3. Submission 放松优先（Player Fantasy）**
在「增加策略深度」与「保持可读不吓人」之间，HUD 永远优先后者。数字用 ease-out 插值滚动而非跳切，回合高亮用 Ease-Out Cubic 去超冲而非弹跳，通告 banner 在不打断主操作的区域滑入滑出。玩家的情绪底色是放松，而非焦虑地盯着数字猜发生了什么。

> **provisional（player-journey.md 未建立）:** 上述玩家情绪状态推断自 GDD Player Fantasy（GDD hud.md §Player Fantasy / OQ-HUD-1 Submission 维度）及 art-bible 状态 B（Gameplay Normal Turn）。正式的回合内玩家情绪曲线须待 player-journey.md 建立后校验。见 Open Questions OQ-UX-HUD-1。

---

## Information Architecture

### Full Information Inventory（完整信息清单）

以下为 HUD 屏范围内全部信息项，来源均为上游 Approved GDD 义务（GDD UI Requirements 节 + CR 系列）。

| # | 信息项 | 内容描述 | 数据来源 | Read/Write | GDD 来源 |
|---|--------|----------|----------|------------|----------|
| 1 | 玩家现金 | 每位在局玩家当前持有现金值 | 经济系统(5) `GetCash` / `OnCashChanged` payload `NewCash` | **Read** | CR-3 |
| 2 | 玩家名 | 玩家显示名 | 回合系统(2) 玩家配置 | **Read** | CR-2 |
| 3 | 玩家色 / 编号 | 玩家唯一颜色（P1–P8）+ 编号标签 | 棋盘系统(1) / 回合系统(2) 玩家配置 | **Read** | CR-2 / art-bible §5.2 |
| 4 | 资产摘要 | 持有地产数（provisional：net_worth 待 OQ-HUD-8 裁定） | 棋盘系统(1) `GetTileData` / 所有权系统(6) | **Read** | CR-2 / OQ-HUD-8 |
| 5 | 当前回合玩家 | 当前轮到哪位玩家 | 回合系统(2) `OnTurnStarted` payload `PlayerId` | **Read** | CR-4 |
| 6 | 回合阶段 | 当前所处阶段（RollPhase / MovePhase / ResolvePhase 等） | 回合系统(2) `OnPhaseChanged` payload `NewPhase` | **Read** | CR-6 |
| 7 | ConsecutiveDoubles 计数 | 连续双点次数（区分首次 Roll 与双点额外回合） | 回合系统(2) `OnPhaseChanged` payload `ConsecutiveDoubles` | **Read** | CR-6 |
| 8 | bIsAI 标识 | 当前活动玩家是否为 AI | 回合系统(2) `OnTurnStarted` payload `bIsAI` | **Read** | CR-4 |
| 9 | 回合顺序 | 本局最终入场顺序（P1→PN 序列） | 回合系统(2) `OnTurnOrderResolved` payload `OrderedPlayerIds` | **Read** | CR-9 |
| 10 | 席位裁定标志 | 是否由席位裁定决出顺序 | 回合系统(2) `OnTurnOrderResolved` payload `bResolvedBySeatTiebreak` | **Read** | CR-9 |
| 11 | 破产标志（每玩家） | 该玩家是否已破产（终态） | 经济系统(5) `OnBankruptcyDeclared` payload `Debtor` | **Read** | CR-5 |
| 12 | AI 行动流 | AI 当前回合已执行的行动列表（类型/目标/金额） | 回合系统(2) `OnAIActionExecuted` payload `FAIActionDetails` | **Read** | CR-10 |
| 13 | 建房通告 | 本局任意玩家刚完成的建房事件（地块/数量/建房者） | 回合系统(2) `OnBuildingAnnounced` payload | **Read** | CR-12 |
| 14 | 胜利结果 | 胜者 PlayerId（可为 INDEX_NONE） | 回合系统(2) `OnGameWon` payload `winnerId` | **Read** | CR-13 |
| 15 | 骰子点数（合计） | 本次掷骰总点数 `Total` | 骰子系统(3) `FDiceRollResult.Total` | **Read** | CR-15 |
| 16 | 收租信息 | 付租玩家/收租玩家/金额/地块索引 | 经济系统(5) `OnRentPaid` payload | **Read** | CR-15 |
| 17 | 当前玩家可用操作集合 | 哪些操作按钮当前启用（掷骰/买地/建房/抵押/赎回） | 回合系统(2) / 建房系统(8) / 经济系统(5) — affordance 只读反映 | **Read** | CR-14 |
| 18 | 发薪基准（SalaryAmount） | 过 GO 发薪金额（=200 标准/快速变体） | 棋盘系统(1) `SalaryAmount` | **Read** | GDD Dependencies |

> **architectural concern（Write 标记）:** 上表所有项均为 Read。HUD 不写任何上游系统状态。若实现期发现任何 HUD widget 向 owner 系统写状态（非转发意图），立即升 ADR 讨论——违反 ADR-0007/0008 叶子边界。

---

### Information Categorization

基于「零认知负担可读」哲学和 GDD UI Requirements §信息架构，将全部信息项分为四层：

#### Must-Show（常驻必显，任何时刻非 hover 即可读）

| 信息项 | 理由 | 位置区域 |
|--------|------|----------|
| 每位玩家现金（#1） | 核心决策基础——不显无法判断"我有没有钱买/建" | 顶部 HUD 栏 · 玩家面板 |
| 玩家名 + 色/编号（#2/#3） | 区分玩家身份，色盲下靠编号冗余 | 顶部 HUD 栏 · 玩家面板 |
| 当前回合玩家高亮（#5） | 核心社交时刻——"聚光灯落在我身上"，无此则玩家不知轮到谁 | 顶部 HUD 栏 · 玩家面板激活态 |
| 当前回合阶段（#6） | 玩家判断"我现在能做什么"的前提 | 底部操作栏 · 阶段指示区 |
| 可用操作集（#17） | 直接映射玩家意志——按钮 affordance 反映实际可做 | 底部操作栏 · 操作按钮组 |

#### Contextual（事件触发出现，自动消失，不常驻）

| 信息项 | 触发条件 | 消失条件 | 位置区域 |
|--------|----------|----------|----------|
| AI 行动流（#12） | AI 回合 `OnAIActionExecuted` 到达 | 队列播完 / 下一回合 `OnTurnStarted` | 独立 AI 行动区（非操作栏） |
| 建房通告（#13） | `OnBuildingAnnounced` | T_beat_total = 2400ms 后自动消失 | 通告区（屏幕上方 / 非棋盘区） |
| 回合顺序 + 席位裁定标志（#9/#10） | `OnTurnOrderResolved` | 首个 `OnTurnStarted` 到达后消失 | 通告区 |
| ConsecutiveDoubles 计数（#7） | `OnPhaseChanged` 含 CD > 0 | 非 RollPhase / 回合结束 | 底部操作栏 · 当前玩家信息区 |
| 骰子点数合计（#15） | 掷骰后 `FDiceRollResult.Total` | 棋子移动开始后淡出 | 底部操作栏 · 骰子数值区（provisional） |
| bIsAI 标识（#8） | `OnTurnStarted` with `bIsAI=true` | `OnTurnEnded` | 对应玩家面板内"AI 思考中"标签 |
| 收租信息（#16） | `OnRentPaid` | 约 2s 后淡出（toast 模式） | 通告区（与 VFX19 金币飞溅锚点协同） |

#### On-Demand（按需开启，玩家主动触发）

| 信息项 | 触发方式 | 承载 UI | GDD 边界 |
|--------|----------|---------|----------|
| 地块详情/租金表/建房选项（地产卡） | 点击棋盘 tile / 底部操作栏"地产卡快览"入口 | **#17 地产卡 UI**（非 HUD 拥有） | HUD 仅拥有入口触发；详情面板归 #17 |
| 资产摘要（地产数 / provisional net_worth） | 点击玩家面板展开（若设计支持） | 玩家面板扩展区 | 待 OQ-HUD-8 裁定 |

#### Hidden（设计上不在 HUD 层呈现，有其他 owner）

| 信息项 | 理由 | 实际 Owner |
|--------|------|------------|
| 骰面（Die1 / Die2 各自点数） | 归 VFX19 棋盘世界空间呈现（GDD CR-15） | VFX(19) |
| 金币飞溅 juice | enable-not-own；VFX19 直订经济5 `OnRentPaid` | VFX(19) |
| 地块租金详情表 | 归 #17 地产卡 UI | #17 Property Card UI |
| 监狱状态 | 待 player-turn(2)/事件(7) 暴露字段后再规划 HUD 面板呈现 | 待定（OQ-HUD-10） |
| 债务关系 | 大富翁即时结算，无持续债务账（GDD 已裁定不补） | N/A |
| 对手持有地产抵押/垄断状态（常驻棋盘） | 归 #17 地产状态常驻叠加（P-15） | #17 + 棋盘层 |

---

## Layout Zones

基于 art-bible §7.1「HUD 布局区域划分」（1920×1080 基准）：

```
┌──────────────────────────────────────────────────────────────────┐
│  [Zone A: 顶部 HUD 栏 — 80px]                                     │
│  [P1面板] [P2面板] [P3面板] [P4面板]        [回合信息/阶段]         │
├──────────────────────────────────────────────────────────────────┤
│  [Zone C: 通告/AI行动浮层 — 可变高，不入侵棋盘主区域]               │
│                                                                    │
│            [Zone B: 中央棋盘区域 — ≥65% 屏幕]                      │
│                  (HUD 不拥有此区域)                                 │
│                                                                    │
│  [Zone D: AI 行动区 — 独立专属，不挤占 Zone E]                      │
├──────────────────────────────────────────────────────────────────┤
│  [Zone E: 底部操作栏 — 120px]                                      │
│  [骰子按钮] [当前玩家信息/阶段] [操作组] [地产卡快览入口]              │
└──────────────────────────────────────────────────────────────────┘
```

### Zone A — 顶部 HUD 栏（80px 高）

**内容：** 玩家面板 ×N（N = 2–4 MVP，接口预留 8）+ 回合信息区

**玩家面板（每面板）：**
- 圆角矩形卡片，左侧 4px 玩家色边框（art-bible §7.4a）
- 面板背景 Off White `#FAFAF5`，软阴影 blur ≥8px（art-bible §3.4）
- 内部信息层级：玩家名（Level 3, 14–18px SemiBold）/ 现金（Level 2, 20–28px SemiBold, Tabular Figures）/ 资产摘要（Level 4, 10–12px Regular, Dark Ink 60%）/ P编号圆标（≥10px，色盲冗余）
- 激活态：亮度 +15%，scale 1.05，投影加深（art-bible §7.4a / GDD V-Own-02）
- 破产终态：去饱和 60% + 灰对角线纹理叠加（20–30% 不透明度）+ 面板整体不透明度 60%（art-bible §7.4a / GDD V-Own-04）

**N 玩家下的面板分布规则（provisional）：**
- N=2：均等平铺左半顶部，回合信息占右半
- N=3：均等三等分顶部，回合信息占右侧约 1/4
- N=4：均等四等分，回合信息内嵌于最右面板旁紧凑区
- N=5–8（Full Vision）：面板紧凑且 ≤ 10px 最小字号（EC-12 约束，待 OQ-HUD-6 量化）

**回合信息区（Zone A 右侧）：**
- 当前阶段文字（Level 3）
- 轮次指示（当前玩家名/色标 Level 3）
- AI 思考中标签（Level 4，Dark Ink 60%，仅 bIsAI=true 时显示）

> **provisional（布局尺寸）：** 80px 高是 art-bible §7.1 权威，玩家面板等宽分布规则与 N=5–8 紧凑布局的精确像素数字归 OQ-HUD-6 `/ux-design` 量化。

---

### Zone B — 中央棋盘区域（≥65% 屏幕）

**HUD 不拥有此区域。** 棋盘是舞台（art-bible §1 原则1）。HUD 的顶部栏和底部操作栏贴屏幕边缘，不入侵棋盘。

**HUD 与此区域的唯一交互：**
- 点击 tile → 触发"打开地产卡"意图（转发 #17，P-06 Modal Card Activation 模式）
- 棋盘平移/缩放由 P-14 Board Pan & Zoom 处理（`IA_BoardPan` / `IA_BoardZoom`）

---

### Zone C — 通告区 / 浮层（Zone A 下方，不入侵 Zone B 主区域）

**内容：** 建房通告 banner 堆叠 ×N_max / 席位裁定顺序通知 / 回合开始提示 toast

**布局规则：**
- Banner 堆叠从屏幕上方边缘向下排列，`Y_offset(k) = k × (H_banner + GAP_banner)`（F-5：H=48px，GAP=6px，k=0 最新在最上）
- 最多 N_max=4 条同时可见；超出驱逐最旧（EC-8）
- 通告区不遮挡底部操作栏和棋盘主区域，水平居中或右对齐（不遮挡玩家面板操作——具体位置 OQ-HUD-6）

> **实现约束（P-10 模式）：** banner 由 `UCommonActivatableWidget` 管理堆叠，不进激活栈（非 modal），不截获输入。

---

### Zone D — AI 行动区（独立专属浮层）

**内容：** AI 行动逐步呈现（文字 + 图标 + 语义色金额）/ "AI 思考中"进度指示

**关键约束（GDD V-Own-07 R-3 硬约束）：**
- **不得挤占 Zone E（底部操作栏）空间。** Zone E 的 affordance 按钮须常在、常可读。
- AI 行动区为独立专属浮层，候选位置：棋盘左侧边栏 / Zone A 下方 Zone C 旁 / 棋盘内右下角浮动区（具体位置归 OQ-HUD-6）

**尺寸预算（provisional）：** 单条 AI 行动行高约 32–40px，最多同时显示 M≤5 条（F-4 MVP 上界），总高度约 160–200px（待 OQ-HUD-6 量化）

**设计约束来源：** GDD §UI Requirements § 边界「AI 行动区不挤占底部操作栏」（R-1 硬约束）

---

### Zone E — 底部操作栏（120px 高）

**内容：** 掷骰按钮 / 当前玩家信息区（阶段 + ConsecutiveDoubles） / 操作按钮组（买地/建房/抵押/赎回）/ 地产卡快览入口

**布局规则（左至右）：**
- 左区（约 160px）：掷骰按钮 — CTA，Fortune Gold 底，Dark Ink 文字，胶囊形（art-bible §3.4 主按钮 = 圆角半径 40% 高度）
- 中区（弹性）：当前玩家信息 — 阶段文字 + ConsecutiveDoubles 计数 + 骰子点数（事件触发）
- 右区（约 300px）：操作按钮组（买地/建房/抵押/赎回）— 次级按钮（圆角半径 20%）
- 最右（约 80px）：地产卡快览入口图标按钮

**affordance 规则（P-16 只读 Affordance）：**
- 按钮启用/禁用态读 owner affordance，非 HUD 自行裁决
- 掷骰：仅当前玩家 = 人类 ∧ 处 RollPhase 时启用
- 买地/建房/抵押/赎回：读回合系统(2) / 建房系统(8) / 经济系统(5) 对应 affordance
- 禁用态：去饱和灰 + 不透明度 50% + 图标形状变化（空心/实心区分，art-bible §4.5）

---

## HUD Elements

### Element 1 — 玩家面板（PlayerPanel Widget）

**对应 WBP：** `WBP_PlayerPanel`（候选）

**显示信息：** 玩家名 / 玩家色 / P编号圆标 / 现金（Tabular Figures）/ 资产摘要 / 激活态高亮 / 破产终态

**状态机（每面板独立，来自 GDD §States and Transitions）：**

| 当前态 | 触发事件 | 新态 | 视觉变化 |
|--------|----------|------|----------|
| Normal | `OnTurnStarted(PlayerId == 本面板)` | **Active** | 亮度 +15%，scale 1.05，投影加深；onset ≤100ms，完成 ≤500ms（F-3） |
| Active | `OnTurnStarted(PlayerId ≠ 本面板)` | Normal | 退回基础态（互斥不变式） |
| Active | `OnTurnEnded(bGrantsExtraTurn == false)` 且移交他人后 | Normal | 移交过场 outro（CR-7 B 分支） |
| Active | `OnTurnEnded(bGrantsExtraTurn == true)` | Active | 高亮停留，显"双点——继续!" badge |
| Normal / Active | `OnBankruptcyDeclared(Debtor == 本面板)` | **Bankrupt（终态）** | 去饱和 60% + 灰对角纹 + 不透明度 60% |
| Bankrupt | 任何后续事件 | Bankrupt | 终态，忽略后续 `OnTurnStarted`/`OnCashChanged` |

**交互规则：**
- 面板本身为只读展示，无可点击动作
- 单击玩家面板可触发"查看该玩家资产概况"（On-Demand 展开，待 OQ-HUD-8 裁定是否实现）

**数据要求：**

| 字段 | 来源系统 | 接口 | R/W | 刷新触发 |
|------|----------|------|-----|----------|
| 玩家名 | 回合(2) / 大厅配置 | 初始化读取 | Read | 初始化一次 |
| 玩家色 | 棋盘(1) / 回合(2) | 初始化读取 | Read | 初始化一次 |
| 当前现金 | 经济(5) | `GetCash(PlayerId)` 初始快照 + `OnCashChanged` 增量 | Read | `OnCashChanged` |
| 资产摘要（地产数） | 所有权(6) | 初始快照 + `OnOwnershipChanged` 增量 | Read | `OnOwnershipChanged`（provisional） |
| 激活态 | 回合(2) | `OnTurnStarted` / `OnTurnEnded` | Read | 事件驱动 |
| 破产态 | 经济(5) | `OnBankruptcyDeclared` | Read | 事件驱动，终态 |

**现金 Counter 动画（P-09 模式）：**
- 驱动：`NativeTick`（C++ 侧，ADR-0008 裁定1；不可用 UWidgetAnimation PlaybackSpeed）
- 时长：`T_counter = clamp(K_dur × log10(|ΔCash|+1), T_min, T_max)`（F-1，≤1.5s）
- 插值：`ease_out(u) = 1 − (1−u)^E_pow`，E_pow=2.0（F-2）
- 语义色：Salary/正向 = Fortune Gold；Rent/Tax/负向 = Property Red；完成后恢复 Dark Ink（V-Own-03）
- 中途重定向（EC-1）：以当前显示值为新 OldCash 重启，不排队
- 破产优先（EC-3 / CR-5 并发优先级）：收到 `OnBankruptcyDeclared` 时立即锁定当前显示值为终态，停止 counter

**a11y 标注：**
- 现金正负变化：金/红语义色 + 数字方向（±符号）双冗余（CA-01 / accessibility-requirements §2.2）
- 玩家色：左边框 + P编号圆标双冗余（CA-02 Deuteranopia 高风险对 P1/P3）
- 破产态：去饱和 + 对角斜纹 + "破产"文字/图标三冗余（CA-01）
- 最小字号 ≥10px（EC-12 8玩家紧凑布局仍须满足）
- 面板等宽：4→5位数字切换时面板宽度不变（Tabular Figures 保证）

---

### Element 2 — 底部操作栏（ActionBar Widget）

**对应 WBP：** `WBP_ActionBar`（候选）

**子元素：**

#### 2a — 掷骰按钮（RollButton）

**模式引用：** P-01（掷骰点击触发 Roll-to-Trigger）+ P-16（只读 Affordance）

**交互规范：**
- 输入：`IA_Roll`（Digital，`ETriggerEvent::Started`，单击即触发）
- 鼠标：左键单击
- 手柄：`IMC_Gameplay` 内 Face Button Bottom（ADR-0011 FR-5）
- 启用条件：当前玩家 = 人类 ∧ `NewPhase == RollPhase`（只读 affordance）
- 禁用条件：AI 回合时 `IMC_Gameplay` 被移除（ADR-0011 双重阻断）+ 按钮视觉禁用
- 按下反馈：Ease-In scale 0.95 / 80ms；松开 Ease-Out Back scale 1.0→1.05→1.0 / 150ms（art-bible §7.5）
- 点击后向回合系统(2) 公开接口转发"掷骰意图"（不自行执行，CR-14）
- **禁 hover-only**：启用/禁用状态非 hover 即可读（AC-45）

**视觉规格：**
- 启用：Fortune Gold 底 `#F4C430`，Dark Ink 文字，胶囊形（art-bible §3.4 主 CTA）
- 禁用：去饱和灰，不透明度 50%，骰子图标由实心变空心（CA-01 ③）
- 尺寸：最小点击目标（provisional，具体数值待 OQ-HUD-9 WCAG 2.5.5 ≥24×24px 量化）

**数据要求：**

| 字段 | 来源 | R/W | 刷新触发 |
|------|------|-----|----------|
| 按钮启用态 | 回合(2) 阶段状态只读 | Read | `OnPhaseChanged` |
| 当前活动玩家是否为人类 | 回合(2) `bIsAI` | Read | `OnTurnStarted` |

#### 2b — 操作按钮组（ActionButtonGroup）

**包含：** 买地按钮 / 建房按钮 / 抵押按钮 / 赎回按钮

**模式引用：** P-03（买/不买二选一 Binary Resolve Modal）/ P-05（长按建房 Hold-to-Build）/ P-04（确认/取消 Confirm/Cancel）/ P-16（只读 Affordance）

**交互规范（按钮公共规则）：**
- 所有按钮使用 `UCommonButtonBase`（ADR-0012）
- 启用/禁用读 owner affordance（P-16），禁用时显门控原因 tooltip（手柄可达，非 hover-only，P-05 / #17 building L265）
- 所有按钮点击仅转发意图，不自行裁决（CR-14）
- **禁 hover-only**：门控原因须有点击/聚焦可达路径（accessibility-requirements §4.1）

**建房按钮特化（P-05 长按建房）：**
- 输入：`IA_BuildHouse`（Hold Trigger，Hold Time=0.5s 可调）
- 鼠标：按住左键 0.5s 触发；短按不触发
- 手柄：同 IA Hold Trigger，零代码适配
- 进度反馈：长按期间显进度环/填充（具体形态待 OQ-UX-HUD-2）
- 启用条件：建房系统(8) `CanBuildHouse` = true

**数据要求（操作组公共）：**

| 字段 | 来源 | R/W | 刷新触发 |
|------|------|-----|----------|
| 买地可用 | 回合(2) ResolvePhase affordance | Read | `OnPhaseChanged` |
| 建房可用 | 建房(8) `CanBuildHouse` | Read | `OnBuildingChanged` / 阶段变化 |
| 抵押可用 | 所有权(6) / 建房(8) `GetHouseCount==0 ∧ owner==当前玩家 ∧ ¬is_mortgaged` | Read | affordance 查询 |
| 赎回可用 | 经济(5) `GetCash >= GetUnmortgageCost` ∧ `is_mortgaged` | Read | affordance 查询 |

#### 2c — 当前玩家信息区（TurnInfoArea）

**显示：** 当前玩家名 + 阶段文字 + ConsecutiveDoubles 计数（事件触发显示）+ 骰子合计点数（provisional）

**交互规范：** 只读展示，无可点击动作

**阶段切换过渡：** Ease-Out Cubic/Quart 200–300ms（GDD V-Own-10 R-3，去超冲；reduce_motion=On 时瞬切）

**ConsecutiveDoubles 显示规则：**
- CD=0：不显计数（正常回合）
- CD≥1：显"第 N 次双点"标记 + 视觉区分（Level 4，Fortune Gold 底）
- 防止首次 RollPhase 与双点额外回合 RollPhase 混淆（GDD CR-6 / AC-20）

#### 2d — 地产卡快览入口（PropertyCardEntry）

**模式引用：** P-06（地产卡详情激活栈 Modal Card Activation）

**交互规范：**
- 鼠标：单击图标按钮，打开最近相关地块的地产卡（或触发"选择要查看的地块"界面，待 OQ-HUD-6 裁定）
- 手柄：焦点导航到此按钮 + 确认键
- 点击后向 #17 地产卡 UI 传递意图 + TileIndex，HUD 不拥有卡片内容（CR-14 边界）

---

### Element 3 — 通告 Banner（AnnouncementBanner Widget）

**对应 WBP：** `WBP_AnnouncementBanner`（候选）

**模式引用：** P-10（建房通告 Banner Building Announce Beat）/ P-11（通知 Toast 上滑 Notification Toast）

**触发事件：** `OnBuildingAnnounced(TileIndex, NewHouseCount, ActingPlayerId)` / `OnTurnOrderResolved` / `OnRentPaid`（toast 降级版，可选）

**生命周期（F-5）：**
- 入场：ease-out 上滑 200ms（art-bible §7.5，**固定，不可调**）
- 停留：2.0s（T_dwell 可调，1.5–3.0s 安全范围）
- 出场：ease-in 上滑 200ms（**固定，不可调**）
- 总生命周期 = 2400ms（默认配置）

**并发堆叠规则（F-5）：**
- k=0 最新在最上；Y_offset(k) = k × (48 + 6)px
- 同屏上限 N_max=4；超出驱逐最旧（进入 slide_out，立即释放 k 槽）
- 正在 slide_out 的 banner 走独立 render 层，不占 k 槽

**Banner 内容（建房通告）：**
- 格式："[玩家色小圆点 + 玩家名] 在 [地产名] 建第 N 栋"
- 背景 Off White 圆角，左 3px 发起玩家色线
- 文字 Level 3（14–18px）
- 无论当前活动玩家是谁均显示（CR-12；MVP 单屏 = 持屏者可见，见 OQ-HUD-13）

**a11y 标注：**
- 文案须文字可读（非纯图标）；停留 2s 足够阅读（T_dwell 为 Tuning Knob）
- 玩家色 + 玩家名双冗余（色盲安全）
- reduce_motion=On 时：`T_slide_in` = `T_slide_out` = 0（瞬现瞬隐，不滑动）
- 不抢焦点（非 modal，不打断键盘/手柄焦点链）

**数据要求：**

| 字段 | 来源 | R/W | 刷新触发 |
|------|------|-----|----------|
| TileIndex → 地产名 | 棋盘(1) `GetTileData(TileIndex).DisplayName` | Read | 事件驱动 |
| NewHouseCount | `OnBuildingAnnounced` payload | Read | 事件驱动 |
| ActingPlayerId → 玩家名/色 | 回合(2) 玩家配置 | Read | 事件驱动 |

---

### Element 4 — AI 行动区（AIActionDisplay Widget）

**对应 WBP：** `WBP_AIActionDisplay`（候选）

**模式引用：** P-08（AI 行动逐步非阻塞播放 AI Action Step Playback）

**触发事件：** `OnAIActionExecuted(FAIActionDetails{ActionIndex, EActionType, ActingPlayerId, TargetTileIndex, Amount})`

**播放规则（F-4）：**
- 按 `ActionIndex` 升序入队，非阻塞播放（框架不等 HUD 回放完成，R6 RETREAT）
- 重复 ActionIndex 去重（幂等，防事件重传）
- 单条显示时长：`T_slot(i, M) = clamp(T_base − K_compress × (M−1), T_slot_min, T_base)`
- M=0 时不进入播放循环（GDD F-4 缺陷8）

**过期判定（F-4 主/辅判据）：**
- **主判据**：HUD 本地 `turn_epoch` 推进（`E_cur > E_action(i)`）——下一回合 `OnTurnStarted` 到达使 `E_cur` +1
- **辅判据**：`unpaused_elapsed(i) > T_stale`（游戏时间，暂停不计，读档重置）
- critical 动作（BuyProperty / BuildHouse / Bankruptcy）expired 时仍呈现最终态；非 critical expired 时整条跳过

**文案/图标规则：**
- 文案模板由 `EActionType` 派生（BuyProperty → "AI 购入 [地产名]"，BuildHouse → "AI 在 [地产名] 建房"，等）
- **HUD 不从 `ActionIndex` 臆断动作类型**（R-2 约束，必须读 payload `EActionType`）
- 涉资金变动的金额数字按 V-Own-03 语义色标注

**交互规范：**
- 玩家可主动跳过/加速 AI 行动（EC-6：剩余动作视为过期，直跳最终态）
- 跳过触发形式待定（OQ-HUD-4），须不绕过 handoff outro 窗口

**数据要求：**

| 字段 | 来源 | R/W | 刷新触发 |
|------|------|-----|----------|
| AI 行动详情 | 回合(2) `OnAIActionExecuted` payload | Read | 事件驱动 |
| TargetTileIndex → 地产名 | 棋盘(1) `GetTileData` | Read | 事件驱动 |
| 回合代次 E_cur | HUD 自持计数器（每收 `OnTurnStarted` +1） | **HUD 内部状态**（非游戏状态写入，呈现层自持） | `OnTurnStarted` |
| unpaused_elapsed | IGameClock DI（ADR-0008 / OQ-HUD-2） | Read | NativeTick（游戏时间） |

> **architectural concern（E_cur）：** HUD 自持的 `turn_epoch` 是**呈现层内部状态**，用于过期判定，不写任何游戏系统状态。与 ADR-0007/0008 叶子边界一致。

**a11y 标注：**
- 行动须有文案 + 图标（非仅动画；色盲/低视力可读）
- 非阻塞——玩家不被强制观看（可跳过）
- reduce_motion=On 时：降级为 Instant 模式（瞬显文案，无逐步动画）

---

### Element 5 — 回合顺序通知（TurnOrderToast）

**触发事件：** `OnTurnOrderResolved(OrderedPlayerIds, bResolvedBySeatTiebreak)`

**模式引用：** P-11（Notification Toast 上滑）

**显示内容：**
- 最终顺序列表（玩家色标签 + 名字：N1 → N2 → N3 → N4）
- `bResolvedBySeatTiebreak == true` 时额外显示"席位决定先手"文字（Level 4）——**不得静默**（GDD CR-9 / AC-27）

**生命周期：** 同 P-11 toast；首个 `OnTurnStarted` 到达后自动消失（不阻塞流程）

---

### Element 6 — 胜利终局覆盖层（VictoryOverlay Widget）

**对应 WBP：** `WBP_VictoryOverlay`（候选）

**触发事件：** `OnGameWon(winnerId)`

**模式引用：** P-12（终局覆盖层 Endgame Overlay）

**显示内容：**
- 胜者名 + 总资产 `net_worth`（Level 1，Fortune Gold）
- 非胜者面板：灰/低透明（幅度略浅于单个破产终态）
- `winnerId == INDEX_NONE`（退化局）：中性"游戏结束"文字，不臆造赢家（EC-10）

**交互（覆盖层内）：**
- 「返回主菜单」按钮（`UCommonButtonBase`，P-04 确认路径）
- 「再来一局」按钮（Full Vision 阶段，MVP 可隐藏）

**juice（enable-not-own）：**
- 彩带/烟花归 VFX(19)（V-Enable-01）
- 胜利 stinger 归音频(22)（A-06）
- HUD 不向 VFX/音频推送额外信号

**GameOver 后面板冻结：**
- 收到 `OnGameWon` 后，所有玩家面板冻结逻辑态，不再响应 `OnTurnStarted` / `OnCashChanged` / `OnTurnEnded`（GDD 状态机不变式）
- 破产面板的已有破产视觉不叠加二次灰化

---

## Dynamic Behaviors

### DB-1 现金 Counter 滚动（P-09）

**触发：** `OnCashChanged(Player, OldCash, NewCash, EChangeReason)`

**行为序列：**
1. 读 ΔCash = NewCash − OldCash
2. ΔCash == 0 → 早返特判，不调 F-1/F-2（防御性，EC-2）
3. 破产终态面板 → 忽略（EC-3）
4. 动画进行中 → 以当前显示值为新 OldCash 重启（EC-1，不排队）
5. 调 F-1 计算 T_counter；调 F-2 NativeTick 逐帧插值
6. 动画期按 EChangeReason 显语义色（V-Own-03）
7. 动画完成 → 显示值精确等于 NewCash，颜色恢复 Dark Ink

**reduce_motion=On：** 旁路 F-1/F-2，直接以新值瞬切（有效时长覆盖为 ≤200ms，旁路 T_min 下界）

---

### DB-2 当前回合高亮（P-07）

**触发：** `OnTurnStarted(PlayerId, bIsAI)` / `OnTurnEnded(PlayerId, bGrantsExtraTurn)`

**行为序列（正常路径，CR-8 不叠帧）：**
1. 收 `OnTurnStarted(P_new)`
2. 旧 Active 面板 outro 动画完成后（bP1OutroComplete==true），P_new 面板切 Active
3. Active 互斥：任意时刻至多一块面板处于 Active
4. onset ≤100ms，完成 ≤500ms（F-3 双约束）

**行为序列（被迫切换路径，EC-4 唯一例外）：**
- 触发条件：读档重建 / 框架快进重建到新当前玩家
- 行为：丢弃 outro 余帧，直接起新高亮（状态正确优先动画完整）

**bGrantsExtraTurn 分支（V-Own-05）：**
- `==true`：高亮停留同玩家，显"双点——继续!" badge（约 1s 后消失），不触发移交过场
- `==false`：旧 outro + 新 handoff（串行，不叠帧）

**reduce_motion=On：** D_highlight 覆盖为瞬切或 ≤200ms 线性，去 scale 弹性

---

### DB-3 AI 行动逐步播放（P-08）

**触发：** `OnAIActionExecuted` （系列事件）

**核心行为：**
- 非阻塞：HUD 落后于框架时丢弃过期中间动画，直显最终快照，不回灌框架（CR-11 R6 RETREAT）
- critical 动作即使 expired 也呈现最终态（F-4 / AC-56）
- 双点额外回合链内 E_cur 不递增（依赖 player-turn L197 `OnTurnStarted` 每链只发一次语义）

**参数可视化（默认配置）：** M=1→1.0s；M=3→0.84s；M=5→0.68s；M=10→0.5s（触底 T_slot_min）

---

### DB-4 建房通告 Beat（P-10）

**触发：** `OnBuildingAnnounced(TileIndex, NewHouseCount, ActingPlayerId)`

**行为：** 无论当前活动玩家是谁均触发（CR-12；EC-7：自己回合建房同样触发）

**并发处理：** 新 banner 入 k=0，旧 banner 下移；超出 N_max=4 时最旧立即进入 slide_out，释放 k 槽

---

### DB-5 破产终态处理（P-12）

**触发：** `OnBankruptcyDeclared(Debtor, Creditor)`

**并发优先级（CR-5）：**
1. 若 counter 动画正在播放 → 立即以当前显示值锁定为终态现金，停止插值
2. 起破产去饱和渐变（Ease-Out 200–300ms）
3. 面板进入 Bankrupt 终态，不再响应任何后续事件

---

### DB-6 读档重建（EC-11）

**触发：** 读档完成事件

**行为序列：**
1. **先显式解绑旧 delegate 监听器**（BP 路径用 `Unbind Event from`；C++ 路径用 `RemoveDynamic`——BP 绑定非 AddUnique，不解绑会重复注册，GDD EC-11 R-3 强调）
2. 重绑全部事件 delegate 监听器
3. 从快照重读全部显示值（现金/当前玩家/阶段/面板态）
4. 直接呈现最终态，不回放历史动画
5. HUD 本地 `turn_epoch` 重置为重建代次

---

### DB-7 HUD Juice RNG 隔离（架构约束 — CR-16 / AC-47）

**约束：** HUD 本地任何 juice 随机（面板轻微抖动、粒子节奏 timing、高亮脉冲相位扰动等表现层随机）**必须走 HUD 自持的独立非权威 `FRandomStream`**；**严禁引用、别名或复用骰子(3)的权威 RNG 流**。

**理由：** 复用权威流会把表现层抽取插入权威调用序列，污染确定性重放（同种子不同结果，Full Vision 联网 desync 根源）。GDD CR-16 / EC-14 / ai-opponent CR-5④；dice 模型 = 各系统自建流（dice.md L187）。

**实现归属：** ADR-0004 流隔离纪律 + control-manifest Cross-Cutting「RNG 流隔离」。BP 环境为 code-review 软约束；架构若选 C++ 强封装独立流可升静态扫描 [Logic]（GDD OQ-HUD-3）。本 spec 传播此约束以确保只读 spec 的实现者亦知须隔离。

---

## Platform & Input Variants

### Keyboard + Mouse（主输入，Primary）

所有可操作元素均提供鼠标点击路径：

| 交互 | 鼠标路径 | 键盘路径 | 模式引用 |
|------|----------|----------|----------|
| 掷骰 | 左键单击掷骰按钮 / 点击棋盘骰子区域 | Tab 导航至按钮 + Enter/Space | P-01 |
| 买地确认 | 左键点击「买下」按钮 | Enter（IA_Confirm） | P-03 |
| 买地取消 | 左键点击「不买」/ Esc | Esc（IA_Cancel） | P-03 / P-04 |
| 建房 | 按住左键 0.5s（Hold Trigger） | 按住键盘建房绑定键 0.5s | P-05 |
| 打开地产卡 | 左键点击 tile / 点击快览入口 | Tab 至快览入口 + Enter | P-06 |
| 关闭地产卡 | 左键点击关闭按钮 / Esc / 点击外部空白 | Esc | P-04 |
| 棋盘平移 | 左键拖拽 | 方向键（可选） | P-14 |
| 棋盘缩放 | 滚轮 | +/- 键（可选） | P-14 |
| 跳过 AI 行动 | 点击跳过按钮（待 OQ-HUD-4 裁定） | 对应键位 | P-08 |

**焦点顺序（Tab 顺序，provisional）：**
掷骰按钮 → 操作按钮组（左至右：买地/建房/抵押/赎回）→ 地产卡快览入口 → （Zone A 玩家面板 Tab 可读，不设为默认焦点链的主路径）

> **provisional：** 精确 Tab 顺序、手柄焦点链、handoff 输入锁定窗口归 OQ-HUD-9 / OQ-HUD-6。

**禁 hover-only 清单（全面审计，accessibility-requirements §4.1）：**
- 建房门控原因：禁用建房按钮时，门控原因须在聚焦/点击时展示，非仅 hover tooltip
- 抵押/赎回门控：同上
- 玩家面板信息（现金/资产）：常驻可见，无任何 hover-only 信息

---

### Gamepad（Partial 预留）

目标：无鼠标专属死角（accessibility-requirements §4.2）。具体完整手感打磨留 Pre-Production `/ux-design`。

| 区域 | 手柄路径 | CommonUI 机制 |
|------|----------|---------------|
| 底部操作栏 | 方向键/摇杆切换焦点；`GetDesiredFocusTarget` 初始焦点落在掷骰按钮 | `UCommonButtonBase` 输入无关 |
| 买地 Modal | Face Button Bottom = 确认（IA_Confirm）；Face Button Right = 取消（IA_Cancel） | `IMC_Modal` P=20 |
| 建房长按 | 长按 Face Button（Hold Trigger 同 IA，零代码适配） | ADR-0011 Hold |
| 地产卡 | 方向键切焦点；Face Button Bottom = 打开；Face Button Right = 关闭 | CommonUI 激活栈 |
| AI 行动跳过 | 对应 Face Button（待 OQ-HUD-4 裁定） | — |

**手柄专属 affordance 规则：**
- AI 回合：`IMC_Gameplay` 已被移除（ADR-0011 FR-3），手柄无法误触发任何操作
- Modal 开启时：`IMC_Modal`（P=20）覆盖 Gameplay（P=0），手柄聚焦在 Modal 内部

---

### Events Fired — 每个玩家动作对应游戏事件

| 玩家动作 | HUD 转发的意图/事件 | 接收方系统 | 写持久状态？ | 备注 |
|----------|---------------------|-----------|------------|------|
| 点击掷骰 | 转发"掷骰意图"至回合(2) 公开接口 | 回合系统(2) | **是（架构关注）** | 回合2 请求骰子3 `RollDice()`，骰子结果改变游戏状态 |
| 点击「买下」 | 转发"买地意图"至回合(2) ResolvePhase 接口 | 回合系统(2) | **是（架构关注）** | 经济(5) 扣钱 + 所有权(6) 变更 |
| 点击「不买」 | 转发"放弃意图"至回合(2) | 回合系统(2) | 否（流程推进） | — |
| 长按建房（0.5s）| 转发"建房意图"至回合(2) ResolvePhase → 建房(8) | 建房系统(8) | **是（架构关注）** | 建房8 执行扣钱+建房 |
| 点击抵押 | 转发"抵押意图"至回合(2) → 经济(5) | 经济系统(5) | **是（架构关注）** | 经济(5) 抵押结算 |
| 点击赎回 | 转发"赎回意图"至回合(2) → 经济(5) | 经济系统(5) | **是（架构关注）** | 经济(5) 赎回结算 |
| 点击地产卡快览 | 转发"打开卡片"意图 + TileIndex 至 #17 | 地产卡 UI(17) | 否（UI 状态） | #17 拥有卡片内容，HUD 仅传递 TileIndex |
| 跳过 AI 行动 | 内部呈现控制（剩余动作标记 expired）| HUD 内部 | 否 | 不改变游戏状态（EC-6） |
| 返回主菜单（GameOver） | 触发场景切换至主菜单 | 主菜单大厅(20) | 否（导航） | Full Vision 留 |

> **architectural concern 标注：** 写持久状态的动作（掷骰/买地/建房/抵押/赎回）意味着 HUD 作为「意图收集器」与 owner 系统之间须有明确的接口协议。HUD 转发意图后**不自行验证结果**——owner 系统权威执行后通过事件通知 HUD 更新显示。若发现 HUD 在意图转发外额外写状态，须立即升 ADR 审查（ADR-0007/0008 边界）。

---

## Accessibility

本节逐条对应 accessibility-requirements.md WCAG-AA checklist 中与 HUD 相关的项。

### 对比度

- [ ] **正文文本/数字 ≥ 4.5:1**：现金数字（Level 2，Dark Ink 对 Off White 约 14:1，安全）；Dark Ink 60% 次要文字须实测（半透明叠加后须复测，accessibility-requirements AC-1）
- [ ] **大号文本 ≥ 3:1**：Level 1 事件标题/胜利文字（art-bible §7.2）
- [ ] **UI 组件/图标/聚焦框 ≥ 3:1**：掷骰按钮边界、状态指示、焦点指示器（accessibility-requirements §2.1）

### 色盲安全

- [ ] **现金 counter 金/红语义色**：± 数字方向（前缀符号/箭头）+ EChangeReason 文字标签冗余（CA-01）
- [ ] **回合高亮**：亮度差异 + scale 1.05 尺寸差 + P 编号标签恒显（CA-01）
- [ ] **破产面板**：去饱和 + 对角斜纹 + "破产"文字/图标（CA-01）
- [ ] **掷骰按钮启用/禁用**：不透明度 50% + 图标实心/空心区分（CA-01 ③）
- [ ] **P1 红 vs P3 绿玩家**：P 编号圆标恒显（CA-02 Deuteranopia 高风险对）
- [ ] Coblis Deuteranopia + Protanopia 模拟验证上述项（accessibility-requirements AC-2）

### 字体

- [ ] **全部游戏内文字 ≥ 10px @1080p**（含 8 玩家紧凑面板、P 编号标签，art-bible §7.2 硬下限，EC-12）
- [ ] **动态数字等宽字形**（Tabular Figures；具体字体资产名待 OQ-A11Y-5 / OQ-IP-4 落定，art-bible §7.2）
- [ ] 文本缩放各档位（100/125/150%）HUD 无文本溢出截断（accessibility-requirements §3.3）

### 键盘可达

- [ ] 所有可操作元素（掷骰/买地/建房/抵押/赎回/打开地产卡/关闭）键盘 Tab 可达且可激活（accessibility-requirements AC-4）
- [ ] 无 hover-only 死角——门控原因等悬停信息须有聚焦/点击可达路径（accessibility-requirements §4.1 / AC-7）
- [ ] 焦点顺序合理（从掷骰按钮开始，逻辑顺序，待 OQ-HUD-9 量化）

### 手柄导航

- [ ] 手柄可遍历全部 HUD 可操作元素（accessibility-requirements AC-5 / ADR-0012 CommonUI）
- [ ] 每屏切入有默认焦点（`GetDesiredFocusTarget` 非空，掷骰按钮为 Zone E 默认焦点）
- [ ] 无鼠标专属死角

### 焦点指示器

- [ ] 焦点态有清晰可见指示器（键盘 Tab + 手柄导航时）；指示器对背景 ≥ 3:1（accessibility-requirements AC-8）
- [ ] 焦点态与 hover 态视觉可区分（accessibility-requirements §4.3 / OQ-A11Y-4）

### reduce-motion

- [ ] `reduce_motion` 开关存在（全局单一布尔，HUD/VFX/卡片 UI 共享同一值，accessibility-requirements §5.1）
- [ ] On 时：counter 旁路 F-1 T_min（有效时长覆盖为 ≤200ms）/ 高亮去弹性+去超冲 / banner 入出场瞬现（T_slide=0）/ AI 行动 Instant / 操作栏阶段切换去动效（GDD AC-57 五项）
- [ ] 参数抑制可验证：读回有效配置参数（非检查渲染输出，accessibility-requirements §5.2 / GDD AC-57 R-3 谓词形态）

### 无 flash 内容

- [ ] HUD 无快速闪烁内容（>3 次/秒）；双点链 badge 和通告 banner 为低频事件，无 flash 风险
- [ ] 若未来加"破产时屏幕闪白"效果，须在设置中提供关闭选项并在首次触发前警告

---

## Acceptance Criteria

> 本节为 UX spec 层面的可测标准（补充 GDD AC 层的系统级 AC）。QA 不读其他文档即可验证。前 5 条覆盖：1 perf + 1 nav + 1 error/empty + 1 a11y + 1 核心目的；AC-6 为架构约束传播（ux-review BLOCKING-1 闭合）。

- [ ] **AC-UX-HUD-1（perf — 开屏响应时间）** [Logic] GIVEN 游戏从加载完成到 HUD 初始化；WHEN `OnTurnOrderResolved` 第一次广播；THEN HUD 顶部 HUD 栏全部玩家面板已渲染且现金显示值正确，底部操作栏掷骰按钮处于正确初始 affordance 态（禁用，因首轮由定序结果决定，非 RollPhase）；整体 HUD 可见延迟 ≤ 1 帧（≤16.6ms @ 60fps）内完成状态反映。FAIL 条件：HUD 初始态与游戏状态不一致，或出现空白/占位符。

- [ ] **AC-UX-HUD-2（nav — entry/exit 路由）** [Logic] GIVEN 玩家在 HUD 局内界面；WHEN 通过 GameOver 覆盖层点击「返回主菜单」；THEN HUD 正确退出，导航至主菜单大厅(#20)，无残留 HUD widget 悬浮，无内存泄漏（Widget 正确 remove from viewport）。FAIL 条件：HUD 残留 / 黑屏 / 导航至错误屏幕。

- [ ] **AC-UX-HUD-3（error/empty 态 — 退化局）** [Logic] GIVEN 收到 `OnGameWon(winnerId == INDEX_NONE)`（退化局，APC==0）；WHEN 胜利终局覆盖层处理；THEN 显示中性"游戏结束"文字，不臆造赢家，不崩溃，不显示任何玩家名作为胜者。FAIL 条件：显示错误玩家为胜者 / 崩溃 / 覆盖层不显示。

- [ ] **AC-UX-HUD-4（a11y — WCAG-AA 键盘全可达）** [Logic] GIVEN 仅使用键盘（无鼠标）；WHEN 从 HUD 初始状态；THEN 可经 Tab 键依次到达并激活：掷骰按钮、买地按钮（ResolvePhase 时）、建房按钮（可用时）、抵押按钮（可用时）、赎回按钮（可用时）、地产卡快览入口；不存在任何键盘不可达或不可激活的可操作元素。FAIL 条件：任一可操作元素键盘不可达。依据：accessibility-requirements AC-4 / WCAG AA 2.1.1。

- [ ] **AC-UX-HUD-5（核心目的 — 零 hover 即可读）** [Logic] GIVEN 鼠标不悬停于任何元素（或使用手柄无悬停）；WHEN 检查对局进行中 HUD；THEN 以下信息均非 hover 即可读：每位玩家现金、当前轮到谁（高亮指示）、当前回合阶段、自己的可用操作（按钮 affordance）；不存在任何"信息仅在 hover 后才显现"的情况。FAIL 条件：发现任何 hover-only 信息（特别是建房门控原因、抵押门控原因等）。依据：GDD CR-14 / accessibility-requirements §4.1 / technical-preferences 禁 hover-only。

- [ ] **AC-UX-HUD-6（架构约束 — juice RNG 隔离）** [Advisory·code-review] GIVEN HUD 实现提交含任何 juice 随机；WHEN reviewer 静态扫描 HUD 模块；THEN 确认所有 juice 随机来自 HUD 自持独立 `FRandomStream`，无任何对骰子3 权威流的引用/别名/复用。FAIL 条件：发现 HUD juice 随机复用骰子权威流（重放污染缺陷）。依据：GDD CR-16 / AC-47 / EC-14 / ADR-0004 流隔离 / control-manifest Cross-Cutting。[BP 无语言级拦截 → code-review 软约束，与 GDD AC-47 同形态；架构选 C++ 强封装可升 Logic 静态扫描，随 OQ-HUD-3] （见 DB-7）

---

## Open Questions

| ID | 问题 | Owner | 期限 | 来源 |
|----|------|-------|------|------|
| **OQ-UX-HUD-1** | player-journey.md 未建立——Player Context 节（玩家情绪状态）全为 provisional 推断。模板路径：`.claude/docs/templates/player-journey.md`。建立后须回验本 spec Player Fantasy 情绪描述（GDD OQ-HUD-1 Submission CD 复核同期） | ux-designer | Pre-Production（player-journey 建立后） | GDD OQ-HUD-1 / provisional 标注 |
| **OQ-UX-HUD-2** | 长按建房进度反馈的具体呈现形式（进度环 / 填充 / 脉冲）+ Hold Time 默认值 0.5s 的误触率校准（P-05 / interaction-patterns OQ-IP-2） | ux-designer + ue-umg-specialist | Pre-Production playtest 校准 | P-05 / OQ-IP-2 |
| **OQ-UX-HUD-3** | 资产摘要：仅显持有地产数 vs 显示 net_worth——信息架构核心决策。若显 net_worth，须确认棋盘/#17 已清晰呈现抵押状态作为"fallback 安全性前提"（GDD OQ-HUD-8 R-3 补充）。Pre-Production 拍板；playtest 校准呈现 | game-designer / ux-designer | Pre-Production `/ux-design`（playtest 校准） | GDD OQ-HUD-8 |
| **OQ-UX-HUD-4** | Zone D（AI 行动区）具体位置与尺寸方案——候选：棋盘左侧边栏 / Zone A 下方 / 棋盘内右下角浮动区。须验证 1920×1080 下顶部80px + 操作栏120px + 棋盘≥65% + 通告区 + AI 行动区的布局预算可满足（GDD OQ-HUD-6 R-3 F4-A 布局预算紧） | ux-designer | Pre-Production `/ux-design`（布局量化） | GDD OQ-HUD-6 |
| **OQ-UX-HUD-5** | "跳过 AI 行动"（EC-6）的具体 UI 触发形式（按键 / 专用按钮 / 点击任意处）。须不绕过 handoff outro 窗口，防 EC-6 全屏点击误触 AC-24a 不叠帧约束（GDD OQ-HUD-4 / AC-32 覆盖缺口 #1） | ux-designer | Pre-Production `/ux-design` | GDD OQ-HUD-4 / OQ-HUD-9 |
| **OQ-UX-HUD-6** | Pass'N Play 建房通告时序补偿——单屏下 banner 在设备移交前已消失，下一持屏玩家看不到对手建房 beat。设计"回合开始时回看上一轮对手关键行动摘要"补偿机制（非 MVP 阻塞，非纯 Full Vision，单屏即存在）（GDD OQ-HUD-13） | ux-designer / game-designer | Pre-Production `/ux-design`（非 MVP 阻塞） | GDD OQ-HUD-13 |
| **OQ-UX-HUD-7** | reduce_motion 各动画的具体降幅参数（各动画 On 时的目标时长 / 是否瞬切 / 线性时长上限）；最小点击目标尺寸（WCAG 2.5.5 ≥24×24px）；手柄焦点链精确顺序；handoff outro 窗口输入锁定（P1 outro 期设备移交点击的路由语义）；瞬态元素消失时手柄焦点逃逸归宿（GDD OQ-HUD-9）| ux-designer | Pre-Production `/ux-design` | GDD OQ-HUD-9 |
| **OQ-UX-HUD-8** | 跨系统并发动画仲裁（破产三路并发：HUD 去饱和 + VFX19 棋子消散 + 音频22 破产音）的注意力仲裁协议——HUD 内部优先级已在 CR-5 钉死，本 OQ 负责跨系统协调（GDD OQ-HUD-11）| ux-designer / VFX(19) / 音频(22) | Pre-Production `/ux-design` | GDD OQ-HUD-11 |
| **OQ-UX-HUD-9** | GameOver 后事件忽略完整列表——状态机不变式覆盖了 `OnTurnStarted`/`OnCashChanged`/`OnTurnEnded`，但未覆盖 `OnPhaseChanged`/`OnBankruptcyDeclared`（末玩家破产与 OnGameWon 极近到达时序）；GameOver → 操作栏全禁用规则（GDD 覆盖缺口 #10）| ux-designer | Pre-Production | GDD 覆盖缺口 #10 |
| **OQ-UX-HUD-10** | 等宽数字字体资产名（OQ-A11Y-5）——art-bible §7.2 须指定具体字体资产，否则 WBP TextBlock 无可绑字体；UE5.7 FSlateFontInfo tnum feature 核验（OQ-IP-4）。此为 art-director propagate 债 | art-director | Pre-Production（asset-spec 前）| GDD CR-3 / accessibility-requirements OQ-A11Y-5 |
| **OQ-UX-HUD-11** | Localization 字符上限/膨胀（ux-review ADVISORY-1）——layout-critical 文案模板（建房通告 banner "[玩家名] 在 [地产名] 建第 N 栋"、席位顺序列表）须标推荐字符上限 + 40% 膨胀自适应换行/截断降级。项目短期无多语言计划则保持 OQ；进多语言前须补 | ux-designer / localization-lead | 多语言计划确定前（非 MVP 阻塞） | ux-review ADVISORY-1 |
| **OQ-UX-HUD-12** | HUD 初始化 loading 态（ux-review ADVISORY-2）——Element 1 状态机须补 `Initializing` 态（`OnTurnOrderResolved` 到达前的占位规则：现金占位 `---`、掷骰按钮强制禁用），区分"冷启动初始化"与"读档重建"（DB-6）路径，防 1-2 帧闪烁 | ux-designer + ue-umg-specialist | Pre-Production `/ux-design` | ux-review ADVISORY-2 |
| **OQ-UX-HUD-13** | `GetTileData` 空返回 null 处理（ux-review ADVISORY-3）——Element 3/4 数据表须补 `TileIndex` 越界/null 的 fallback 显示值（建议 `"—"`）+ 防御时机（事件到达即验证有效性，早返不渲染而非渲染错字符串） | ux-designer + ue-umg-specialist | Pre-Production `/ux-design` | ux-review ADVISORY-3 |

---

*骰子大亨 HUD UX Spec v0.1 — In Design — 2026-06-06*
*Author: msc + ux-designer agent*
*上游权威：design/gdd/hud.md (Approved R-3) + design/art/art-bible.md §7 + design/ux/interaction-patterns.md + design/accessibility-requirements.md*
