# 交互模式库 (Interaction Pattern Library) — 《骰子大亨》

> **Status**: Draft v0.1（catalog-driven，从 16 个 Approved MVP GDD + art-bible §7 + ADR-0008/0011/0012 编目）
> **Author**: msc + ux-designer agent
> **Last Updated**: 2026-06-06
> **Scope**: 本库**只编目已 Approved GDD 实际用到的交互模式**——不臆造新模式。每个模式回链其 GDD/ADR 来源。
> **Engine**: Unreal Engine 5.7 · Blueprint 为主 · PC/Steam 鼠标点击为主 + 手柄 Partial 预留
> **权威输入栈**: Enhanced Input（ADR-0011，三层 IMC + 每意图独立 IA）+ CommonUI（ADR-0012，`UCommonActivatableWidget` 激活栈 + `UCommonButtonBase` 输入无关按钮）+ HUD 驱动（ADR-0008，counter NativeTick + IGameClock DI）

---

## Overview

本交互模式库是《骰子大亨》呈现层的**交互词汇表**——它把分散在 HUD(16)、地产卡 UI(17)、VFX(19)、主菜单大厅(20)、玩家回合(2) 等 GDD 中的"玩家如何与游戏对话"的具体规则,抽象为一组**可复用、可复审、跨屏一致**的命名模式。它服务三个目的:① 给 Pre-Production `/ux-design` 与 `/team-ui` 实现提供单一权威的交互参照,避免每个屏各自发明"点一下会发生什么";② 把禁 hover-only、鼠标+手柄双路径、WCAG-AA a11y 红线统一钉在模式层,而非每个 widget 重复约定;③ 把"掷骰怎么点""卡片怎么开关""AI 行动怎么播"这些反复出现的交互沉淀为**目录条目**,让一致性成为默认而非偶然。

本库是**呈现层 catalog,非游戏逻辑**:所有模式只描述**输入采集 → 意图转发 → 视/听反馈**这条呈现回路;模式从不裁决游戏状态(裁决归 owner 系统),与各 GDD "呈现层纯叶子"边界完全一致。模式的输入映射一律落到 ADR-0011 的具名 `IA_`/`IMC_` 资产、UI 框架一律落到 ADR-0012 的 CommonUI 组件——本库不重新发明输入或 UI 框架,只编目它们在每类交互中的**组合用法**。

> **诚实边界**:本库 v0.1 覆盖 MVP 16 GDD 的交互。Alpha 的交易(11)/拍卖(12)/命运之轮(13)/策略卡(15) UI 交互模式尚未编目(对应 GDD 未撰写或 deferred),见 **Gaps & Patterns Needed**。

---

## Pattern Catalog（索引表）

| # | 模式名 | Category | 主要 Used In | 输入/框架锚点 | 来源 |
|---|--------|----------|--------------|---------------|------|
| P-01 | 掷骰点击触发 (Roll-to-Trigger) | Input | 底部操作栏(HUD) / 棋盘骰子 | `IA_Roll`(Digital, Started) | dice CR-4 / VFX CR-3 / ADR-0011 |
| P-02 | 翻滚定格反馈 (Roll Juice Three-Beat) | Feedback | 棋盘世界空间 | `OnDiceRolled` 订阅 | VFX CR-3 / dice §呈现契约 |
| P-03 | 买/不买二选一决策 (Binary Resolve Modal) | Modal | 回合 ResolvePhase | `IMC_Modal` + `IA_Confirm`/`IA_Cancel` | player-turn CR-3 / ADR-0011/0012 |
| P-04 | 确认/取消 (Confirm / Cancel) | Modal | 所有 modal 弹窗 | `IA_Confirm`/`IA_Cancel`(P=20) | ADR-0011 / ADR-0012 |
| P-05 | 长按建房 (Hold-to-Build) | Input | 地产卡 / 操作栏建房按钮 | `IA_BuildHouse`(Hold 0.5s) | building CR-4 / #17 CR-7 / ADR-0011 |
| P-06 | 地产卡详情激活栈 (Modal Card Activation) | Modal / Overlay | 地产卡 UI(17) | CommonUI Stack push + `SwitchToModal` | #17 CR-1/CR-11 / ADR-0012 / ADR-0011 |
| P-07 | 回合移交不叠帧 (Turn Handoff Serialization) | Feedback | HUD 玩家面板 | `OnTurnStarted`/`OnTurnEnded` 订阅 | hud CR-8 / player-turn R3 B-2 |
| P-08 | AI 行动逐步非阻塞播放 (AI Action Step Playback) | Feedback | HUD AI 行动区 | `OnAIActionExecuted(ActionIndex)` 升序 | hud CR-10/F-4 / player-turn R6 / ai L95 |
| P-09 | 现金 Counter 滚动 (Cash Counter Roll) | Data Display | HUD 玩家面板现金 | `OnCashChanged` + NativeTick | hud CR-3/F-1/F-2 / ADR-0008 |
| P-10 | 建房通告 Banner (Building Announce Beat) | Overlay | HUD 通告区 | `OnBuildingAnnounced` 全员可见 | hud CR-12/F-5 / player-turn CR-3.5 |
| P-11 | 通知 Toast 上滑 (Notification Toast) | Feedback | HUD 通告区(通用) | ease-out 上滑 200ms | art-bible §7.5 / hud F-5 |
| P-12 | 终局覆盖层 (Endgame Overlay) | Overlay | 全屏(破产/胜利) | `OnBankruptcyDeclared`/`OnGameWon` | hud CR-5/CR-13 / bankruptcy §VA / VFX CR-7 |
| P-13 | 菜单导航 (Menu Navigation) | Navigation | 主菜单/大厅(20) | `IMC_Menu`(P=10) + CommonUI 焦点 | main-menu-lobby CR-2..6 / ADR-0011/0012 |
| P-14 | 棋盘平移缩放 (Board Pan & Zoom) | Navigation | 中央棋盘区 | `IA_BoardPan`(Axis2D)/`IA_BoardZoom`(Axis1D) | ADR-0011 / hud CR-1 |
| P-15 | 地产状态常驻叠加 (Persistent Tile Status Overlay) | Overlay | 棋盘 tile(抵押/垄断) | `OnMortgageChanged`/`OnOwnershipChanged` | #17 CR-10/V-Own-08 / hud F-4 |
| P-16 | 只读 Affordance 按钮态 (Read-Only Affordance Buttons) | Data Display | 操作栏/卡片按钮组 | 读 owner affordance(禁 hover-only) | hud CR-14 / #17 CR-7/CR-8 |
| P-17 | 席位配置控件 (Seat Configuration Controls) | Input | 大厅席位列表 | `IMC_Menu` + 互斥选择器 | main-menu-lobby CR-2/CR-3/CR-4 |

**分类分布**: Input ×3(P-01/P-05/P-17)· Feedback ×4(P-02/P-07/P-08/P-11)· Modal ×3(P-03/P-04/P-06)· Overlay ×4(P-06†/P-10/P-12/P-15)· Data Display ×3(P-09/P-16)· Navigation ×2(P-13/P-14)。
(† P-06 跨 Modal+Overlay 双类,主类记 Modal。)

---

## Patterns（逐条）

### P-01 掷骰点击触发 (Roll-to-Trigger)
**Category**: Input
**Used In**: HUD 底部操作栏「掷骰按钮」(hud CR-1);棋盘半 Diegetic 骰子(art-bible §7.1——玩家点击触发,骰子在棋盘空间滚动)
**Description**: 玩家在自己回合的 `RollPhase` 点击掷骰按钮,发出"我要掷骰"的输入意图。这是核心循环每个回合的起手动作——一次单击(无长按、无拖拽),意图经输入层转发给回合2 公开接口,由其请求骰子3 `RollDice()`。骰子结果在按钮点击**之前**已由逻辑层产出(dice 同步返回),呈现层的翻滚动画只是回放既定结果(见 P-02)。
**Specification**:
- **组件行为**: HUD 操作栏的掷骰按钮(`UCommonButtonBase` 派生,ADR-0012)。仅在当前玩家=人类 ∧ 处 `RollPhase` 时启用(只读 affordance 反映,P-16);AI 回合该按钮禁用且 `IMC_Gameplay` 已被移除(ADR-0011 双重阻断)。点击 → `Handle_Roll` → 回合2 受控接口。
- **输入映射**: `IA_Roll`(Value Type `Digital`,`ETriggerEvent::Started`,无 Trigger——单击即触发,ADR-0011 §Trigger 配置)。**鼠标**: 左键点击按钮 / 点击棋盘骰子。**手柄**: `IMC_Gameplay` 内为 `IA_Roll` 追加 Gamepad 键位(如 Face Button Bottom),零代码追加(ADR-0011 FR-5);CommonUI 默认焦点可落在掷骰按钮(`GetDesiredFocusTarget`)。
- **视/听反馈**: 按钮按下 Ease-In scale 0.95 / 80ms,松开 Ease-Out Back scale 1.0→1.05→1.0 / 150ms(art-bible §7.5);触发后进入 P-02 翻滚 juice。
- **a11y (WCAG-AA)**: 按钮**禁 hover-only**——启用/禁用态非 hover 即可读(hud AC-45);禁用态经不透明度 50% + 灰度 + 图标形状区分(非仅靠色,CA-04①③);点击目标 ≥ 最小尺寸(具体值 OQ-HUD-9);文字/图标对比 ≥ 4.5:1。
**When to Use**: 任何"单次、不可逆触发一个游戏动作"的核心交互(掷骰是范式)。
**When NOT to Use**: 需要确认的破坏性/花钱动作(用 P-03/P-04);需防误触的动作(用 P-05 长按);连续/模拟量输入(用 P-14 Axis)。

---

### P-02 翻滚定格反馈 (Roll Juice Three-Beat)
**Category**: Feedback
**Used In**: 棋盘世界空间(VFX19 拥);骰点数值显示(HUD 拥)
**Description**: 掷骰触发后的**期待→翻滚→定格**三阶段感官回放。两颗骰面**分别**用 `Die1`/`Die2` 呈现(严禁从 `Total` 拆解),双点强化、连掷三次双点切警告色调。这是本作的签名感官时刻(支柱③运气刺激 + MDA Sensation),纯呈现——动画时长/跳过不改变已定的骰点。
**Specification**:
- **组件行为**: VFX19 订阅骰子3 `OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})` 播三阶段 Niagara/动画;双点差异化经 player-turn `OnPhaseChanged` payload 的 `ConsecutiveDoubles`(非裸 `bIsDouble`,VFX CR-3)读取;定序阶段抑制双点强化。HUD 仅显示 `Total` 数值(hud CR-15),骰面滚动归 VFX19。
- **输入映射**: 无(纯事件驱动反馈,不采集输入)。
- **视/听反馈**: 期待(掷骰前)→翻滚(过程)→定格(结果)节奏(dice §呈现契约);双点=庆祝强化,第3连=警告色调;音频22 订 `OnDiceRolled` 播翻滚音 + 双点/警告音(声画分离,dice L209)。juice 随机走 VFX19 独立 `FRandomStream`,严禁复用骰子权威流(VFX CR-9)。
- **a11y (WCAG-AA)**: `reduce_motion=On` 时翻滚降级为瞬现/低强度(VFX CR-10,与 HUD/#17 共享同一全局 a11y 布尔);庆祝 vs 警告不得仅靠色调区分——须辅以图标/文案/音色冗余(对齐 CA 色盲安全)。
**When to Use**: 有明确"过程→结果"节拍、需要营造期待的随机/命运时刻反馈。
**When NOT to Use**: 信息层即时反馈(用 P-11 toast);阻塞框架的反馈(所有 juice 非阻塞,落后即丢中间动画直呈最终态,VFX CR-8)。

---

### P-03 买/不买二选一决策 (Binary Resolve Modal)
**Category**: Modal
**Used In**: 玩家回合 `ResolvePhase`——落无主地产时暴露买/不买决策点(player-turn CR-3)
**Description**: 玩家棋子落到无主可购地块时,弹出一个**二选一模态决策**:买下(花 `PurchasePrice`)或放弃(MVP 直接跳过;Alpha 进拍卖)。这是核心循环最频繁的策略抉择之一。模态期间 Gameplay 输入被截获,玩家必须做出选择才能推进回合。
**Specification**:
- **组件行为**: CommonUI 激活栈 push 一个决策 widget(`UCommonActivatableWidget`),内含「买下」「不买」两个 `UCommonButtonBase`;push 时经 `NativeOnActivated` 调 `SwitchToModal()`(ADR-0012↔0011 协作)。决策结果经输入意图转发回合2 ResolvePhase 公开接口;widget pop 时 `ReturnFromModal()`。地产卡详情可同栈呈现(P-06)辅助决策(读价/租)。
- **输入映射**: `IMC_Modal`(P=20)激活,截获 `IA_Confirm`(买下)/`IA_Cancel`(不买)(ADR-0011)。**鼠标**: 点击对应按钮。**手柄**: `GetDesiredFocusTarget` 默认焦点落在「买下」,确认/返回经 CommonUI 输入无关路径(ADR-0012 FR-2/3)。
- **视/听反馈**: 弹窗 Ease-Out Back/Bounce 200–300ms 弹性入场(art-bible §7.5);确认按钮 Fortune Gold,取消/不买按钮中性。下层 HUD 暂停激活(CommonUI 栈语义)。
- **a11y (WCAG-AA)**: 二选一**两路径都非 hover-only**——明确可点击/可确认(ADR-0012 FR-2);两按钮经 ✓/✗ 图标 + 文案区分,非仅靠色(CA-04④);焦点必须可见;无静默超时强制选择(玩家自主决策)。
**When to Use**: 不可逆且影响游戏状态的二元抉择(花钱/放弃),需要玩家显式确认。
**When NOT to Use**: 可逆/无代价的浏览(用 P-06 详情卡);多于两个选项(留 Alpha,本库未编目);非阻塞信息提示(用 P-11)。

---

### P-04 确认/取消 (Confirm / Cancel)
**Category**: Modal
**Used In**: 所有需要二次确认的 modal 弹窗(地产卡动作、决策框、破坏性操作)
**Description**: 跨所有模态弹窗复用的**确认/取消原子对**——一个肯定动作(`IA_Confirm`)、一个否定/退出动作(`IA_Cancel`)。它是 P-03/P-05/P-06 等模态交互的公共底座,保证"确认走 ✓、取消走 ✗/Esc"在全游戏语义一致。
**Specification**:
- **组件行为**: 任一 modal `UCommonActivatableWidget` 内,肯定按钮绑 `IA_Confirm`、否定按钮绑 `IA_Cancel`(均 `UCommonButtonBase`);`IA_Cancel` 同时映射 Esc / 点击面板外空白(#17 CR-11)。两动作专属 `IMC_Modal`(P=20),不与 Gameplay 层混淆。
- **输入映射**: `IA_Confirm`/`IA_Cancel`(Digital,`IMC_Modal` P=20,ADR-0011)。**鼠标**: 点击按钮 / Esc 键取消 / 点击外部空白取消。**手柄**: Face Button Bottom=确认、Face Button Right=取消(`IMC_Modal` 内追加键位);CommonUI `UCommonUIInputData` 配确认/返回默认动作(ADR-0012 §2)。
- **视/听反馈**: 确认 Fortune Gold(或接受方 Tycoon Green)、取消/危险 Property Red(art-bible §7.4e/§4.3);按钮按下/松开弹性(§7.5)。
- **a11y (WCAG-AA)**: ✓/✗ 图标 + 文字双重区分(色盲安全,CA-04④);确认与取消位置全局一致(降低误触);**Esc/取消路径恒在**(可逆退出);禁 hover-only。
**When to Use**: 任何模态弹窗的标准退出/提交对——作为其他 modal 模式的复用底座。
**When NOT to Use**: 非模态 toast(无确认语义,P-11);单按钮信息确认(降级为单 `IA_Confirm`)。

---

### P-05 长按建房 (Hold-to-Build)
**Category**: Input
**Used In**: 地产卡建房按钮(#17 CR-7)/ HUD 操作栏建房入口
**Description**: 建房意图经**长按(Hold Trigger)**触发,而非单击——因为建房花钱且改变博弈格局(把垄断从 ×2 变成碾压),用长按防止误触一栋房。玩家按住建房按钮 0.5s 才确认发起 `BuildHouse` 意图;意图转发给回合2 ResolvePhase → 建房8 执行。
**Specification**:
- **组件行为**: 建房按钮(`UCommonButtonBase`)绑 `IA_BuildHouse`;按钮启用条件=读建房8 `CanBuildHouse`(只读 affordance,P-16);禁用时显门控原因 tooltip(非垄断/组内有抵押/非最低档/现金不足,#17 CR-7,building L265)。`Handle_BuildHouse` 经 ADR-0011 转发至建房8 `BuildHouse` 受控接口(#17 不裁决,#17 CR-7)。
- **输入映射**: `IA_BuildHouse`(Value Type `Digital`,资产内配 `Hold` Trigger,`Hold Time=0.5s` 可调 Tuning,ADR-0011 §Trigger + Validation Criteria)。**鼠标**: 按住左键 0.5s。**手柄**: 长按 Gamepad 键位(同 IA 的 Hold Trigger 自动适用,零代码)。短按不触发(ADR-0011 Validation: 短按不触发 handler、长按≥0.5s 触发)。
- **视/听反馈**: 长按期间显进度环/填充反馈(建议,具体归 `/ux-design`);完成触发后建房弹出弹性 + 星星 juice(VFX19 V-Enable-01,#17 enable-not-own)。
- **a11y (WCAG-AA)**: 门控原因 tooltip **手柄可达、非 hover-only**(#17 CA/AC-40);长按时长可调(运动障碍 a11y——Hold Time 是 Tuning Knob);禁用态经不透明度+图标区分。
**When to Use**: 花钱/改变格局/不易撤销的动作,需防误触(建房是范式)。
**When NOT to Use**: 高频核心动作(掷骰用 P-01 单击,长按会拖慢节奏);需立即响应的浏览/导航。

---

### P-06 地产卡详情激活栈 (Modal Card Activation)
**Category**: Modal / Overlay
**Used In**: 地产卡 UI(17)——产权契据卡详情面板
**Description**: 玩家点击棋盘地块 / HUD 快览入口,打开一张**模态产权契据卡**(地块名、租金阶梯、建房/抵押选项),经 CommonUI 激活栈 push 到栈顶。卡片是按需开关的瞬态 UI(不序列化),与常驻棋盘叠加(P-15)分离。MVP 单卡——打开新卡直接切换内容,不堆叠。
**Specification**:
- **组件行为**: `WBP_PropertyCard : UCommonActivatableWidget`(ADR-0012 架构图)。`OpenCard(TileIndex)` → `Stack.AddWidget` push → `NativeOnActivated` 调 `SwitchToModal()`(ADR-0011 IMC_Modal P=20)。卡片读三源(棋盘1/所有权6/建房8)渲染,打开期订阅刷新(#17 CR-9)。关闭(关闭按钮/Esc/点外部)→ `Stack.RemoveWidget` pop → `NativeOnDeactivated` 调 `ReturnFromModal()`。HUD 触发"打开卡片"意图+TileIndex,不拥卡片内容(hud CR-14 边界)。
- **输入映射**: 打开经棋盘点击 / HUD 快览入口(Gameplay 层意图);卡内动作经 `IMC_Modal` + P-04 确认/取消 + P-05 长按建房。**鼠标**: 点击开/关、点外部空白关。**手柄**: `GetDesiredFocusTarget` 设卡内默认焦点;Esc/Face Right 关闭。
- **视/听反馈**: 卡牌 3D Y 轴翻转入场 300ms Ease-In-Out(art-bible §7.4b/§7.5);翻面粒子归 VFX19(V-Enable-03/05,enable-not-own);下层 HUD 暂停。
- **a11y (WCAG-AA)**: 卡内所有动作按钮禁 hover-only(#17 CR-11);租金生效行高亮须**字重+竖线+×2 角标多重冗余**(非仅靠背景色,#17 V-Own-04/CA-04);抵押/垄断状态经斜纹/环圈形状冗余(CA-01);≥10px 字号(art-bible §7.2);赎回价/卖回额必显(不让玩家心算,#17 EC-13/EC-14)。
**When to Use**: 按需深读一个对象的详细信息 + 就地发起其相关动作(产权卡是范式)。
**When NOT to Use**: 需常驻可见的状态(用 P-15 叠加);非购买地块(MVP 不开详情卡,#17 CR-2);多卡并列(MVP 单卡)。

---

### P-07 回合移交不叠帧 (Turn Handoff Serialization)
**Category**: Feedback
**Used In**: HUD 玩家面板当前回合高亮(hud CR-4/CR-7/CR-8)
**Description**: 回合在玩家间移交时,新玩家面板的 handoff 高亮**必须等前一回合 outro 动画完成才起**,不与前一回合 outro 叠帧。这保证"聚光灯"在 Pass'N Play 本地轮流下干净转移——同时只有一块面板 Active(互斥不变式),避免两个面板同时高亮的视觉混乱。
**Specification**:
- **组件行为**: HUD 订阅回合2 `OnTurnStarted(FTurnStartedInfo{PlayerId,bIsAI})` / `OnTurnEnded(FTurnEndedInfo{PlayerId,bGrantsExtraTurn})`。正常路径严格串行:前一回合 outro 完成 → 才起新玩家 handoff 高亮(hud CR-8)。`bGrantsExtraTurn==true`(双点)→高亮停留同玩家;`==false`→高亮转移。面板 Active 互斥(任意时刻至多一块 Active,hud 状态机不变式)。
- **输入映射**: 无(事件驱动反馈)。
- **视/听反馈**: 激活面板亮度 +15% / scale 1.05 / 投影加深(art-bible §7.4a);起呈现 ≤100ms、完成 ≤500ms(hud F-3);曲线 Ease-Out Cubic/Quart(去超冲,对齐"大而稳不跳动"Submission 定位,hud L343);超冲/弹性归 VFX19,非面板高亮自身。
- **a11y (WCAG-AA)**: 当前回合**不仅靠高亮色**——经亮度+缩放+(可选)"轮到你了"文案多重提示;`reduce_motion=On` 进一步去动效(瞬切,hud AC-57);**唯一例外**——读档/框架快进重建到新当前玩家时,状态正确优先于动画完整(丢弃 outro 余帧,hud EC-4/CR-8 例外)。
**When to Use**: 序列化的"焦点转移"反馈,须保证同时只有一个焦点态。
**When NOT to Use**: 并发可叠加的反馈(多 toast 可堆叠,P-11);被迫状态重建路径(走 hud EC-4 例外,状态优先)。

---

### P-08 AI 行动逐步非阻塞播放 (AI Action Step Playback)
**Category**: Feedback
**Used In**: HUD AI 行动区(hud CR-10/F-4)
**Description**: AI 回合时,AI 的买地/建房/付租等行动被**逐步、可见、非阻塞**地播放出来(支柱②"对手在跟我斗")——按 `ActionIndex` 升序播放,每条有节奏地展示。**框架不为 HUD 回放 gating**(player-turn R6 RETREAT):框架 AI 决策执行完即时 EndTurn,HUD 自主非阻塞呈现;若 HUD 落后于框架,丢弃过期中间动画、直呈最终态,不回灌框架。
**Specification**:
- **组件行为**: HUD 订阅回合2 `OnAIActionExecuted(FAIActionDetails{ActionIndex,EActionType,ActingPlayerId,TargetTileIndex,Amount})`,按 `ActionIndex` 升序入队播放(0..M-1);重复 index 去重(幂等)。过期判定主判据=HUD 本地 `turn_epoch` 推进(下一回合 `OnTurnStarted` 到达,hud F-4);critical 动作(买地/建房/破产,由 `EActionType` 派生)即使过期也呈现最终态,非 critical 整条跳过。AI 不自 emit,经回合2 中转(ai L95)。
- **输入映射**: 无(AI 回合 `IMC_Gameplay` 已移除,人类无输入,ADR-0011 FR-3);玩家可主动跳过/加速 → 剩余动作视为过期直跳最终态(hud EC-6)。
- **视/听反馈**: 逐条节奏 `T_slot`(M 条压缩,hud F-4);`EActionType` 供行动文案/图标(hud V-Own-07);"AI 思考中"指示经 `bIsAI` 路由(hud CR-4)。AI 行动的 juice 视觉归 VFX19。
- **a11y (WCAG-AA)**: 行动须有**文案+图标**(非仅动画,色盲/低视力可读,ai L341 经 HUD 呈现);非阻塞——不强制玩家观看(可跳过);`reduce_motion` 下降级低强度。
**When to Use**: 把"非玩家驱动的一连串状态变化"逐步可见化以传递社交对抗感。
**When NOT to Use**: 需要 gating 框架的呈现(已 RETREAT,框架不等 HUD);玩家自己的即时动作反馈(用对应即时模式)。

---

### P-09 现金 Counter 滚动 (Cash Counter Roll)
**Category**: Data Display
**Used In**: HUD 玩家面板现金显示(hud CR-3/F-1/F-2)
**Description**: 现金变动时,数字从旧值**滚动**到新值(非瞬跳),时长随变动幅度对数缩放(≤1.5s),按变动原因着语义色(发薪=金/正,付租/税=红/痛)。滚动用等宽 Tabular Figures 防横向跳动。这是 HUD 最高频的数据呈现,服务"零认知负担可读"的 Submission 放松。
**Specification**:
- **组件行为**: HUD 订阅经济5 `OnCashChanged(Player,OldCash,NewCash,EChangeReason)`,经 counter 动画过渡(hud F-1 `T_counter=clamp(K_dur×log10(|ΔCash|+1),T_min,T_max)`,F-2 ease-out 插值)。**驱动机制=NativeTick**(C++ 侧,ADR-0008 裁定1)——UMG sequencer 无整数文本插值轨道,必须每帧 `SetText(FText::AsNumber(round(displayed(t))))`,**不可用 UWidgetAnimation 烘焙/PlaybackSpeed**。逐帧时序逻辑住独立 `UHudStateMachine`(C++ UObject)+ `IGameClock` DI,使 headless 可测(ADR-0008 裁定2)。动画中又收 `OnCashChanged` → 以当前显示值为新 OldCash 重启(hud EC-1);破产终态锁定优先于进行中 counter(hud CR-5)。
- **输入映射**: 无(数据驱动显示)。
- **视/听反馈**: ≤1.5s 滚动(art-bible §7.5 硬上界);语义色 Salary=金、Rent/Tax=红(§4.3);音频22 据 `EChangeReason` 播正/负音色(economy L334)。
- **a11y (WCAG-AA)**: **金 vs 红正负色不可作唯一区分**——色盲下经数字方向(增/减)+ 符号区分(CA-04①);**Tabular Figures 等宽**(art-bible §7.2,须指定字形等宽字体资产,hud CR-3 propagate art-director——UE5.7 无暴露 `tnum` feature API,靠字体本身等宽);≥10px;counter 本身的动画呈现标 [Advisory](NativeTick 不在 -nullrhi 触发,ADR-0008,逻辑标 [Logic] via IGameClock)。
**When to Use**: 数值变化需要"被注意到"且要传递幅度感的显示(现金是范式)。
**When NOT to Use**: 不变/高频抖动的数值(直接显示);需精确即读无延迟的关键值(可瞬切 + `reduce_motion`)。

---

### P-10 建房通告 Banner (Building Announce Beat)
**Category**: Overlay
**Used In**: HUD 通告区(hud CR-12/F-5)
**Description**: 任何玩家盖楼时触发一条**全员可见**的通告 banner(上滑入场、停留 2s、上滑消失),**无论当前是否其回合**——为持屏玩家恢复"仅自己回合建房"削弱的支柱②社交引信("对手变强了,我得当心")。这是"廉价社交点火"手段。
**Specification**:
- **组件行为**: HUD 订阅回合2 `OnBuildingAnnounced(TileIndex,NewHouseCount,ActingPlayerId)`,呈现 banner(hud CR-12)。生命周期 `T_beat_total=T_slide_in(200ms)+T_dwell(2s)+T_slide_out(200ms)`(hud F-5);并发堆叠 `Y_offset(k)=k×(H+GAP)`,超 `N_max`(4)驱逐最旧。通告是信息层事件,不改游戏状态(player-turn CR-3.5)。
- **输入映射**: 无(只读通告,不可交互)。
- **视/听反馈**: ease-out 上滑入场 200ms / ease-in 上滑消失 200ms(art-bible §7.5 通知弹出/消失);携 tile 名 + 新 house_count + 建房玩家。
- **a11y (WCAG-AA)**: banner 须**文案可读**(非纯图标,色盲/低视力);停留 2s 足够阅读(可作 Tuning,运动/认知 a11y);≥10px;`reduce_motion` 下可降为瞬现(去滑动)。**MVP 单屏诚实限定**: "全员可见"实为持屏者可见(hud CR-12 注),离屏玩家补偿归 OQ-HUD-13。
**When to Use**: 需要被所有人注意到的、跨当前焦点的**短时社交/状态事件通告**。
**When NOT to Use**: 需持久可读的状态(用 P-15 叠加);需玩家响应的(用 P-03 modal);玩家私有的即时反馈。

---

### P-11 通知 Toast 上滑 (Notification Toast)
**Category**: Feedback
**Used In**: HUD 通告区通用通知(art-bible §7.5 通知弹出/消失;hud F-5 是其建房特化)
**Description**: 通用的非阻塞信息提示——从上滑入(ease-out 200ms)、停留、上滑消失(ease-in 200ms),不打断主操作。它是 P-10 建房通告 banner 的泛化底座,适用于任何"告知一件已发生的事、不需玩家响应"的呈现(席位裁定可见、过 GO 发薪提示等)。
**Specification**:
- **组件行为**: 通用 toast 容器(HUD 通告区)。事件驱动 push 一条 toast,走 F-5 同款生命周期(入场/停留/出场)+ 堆叠/驱逐规则。不截获输入、不进激活栈(非模态)。
- **输入映射**: 无(非交互;不属任何 IMC)。
- **视/听反馈**: 通知弹出 Ease-Out 从上滑入 200ms,延迟 2s 后 Ease-In 上滑消失 200ms(art-bible §7.5);并发堆叠最旧提前消失。
- **a11y (WCAG-AA)**: 文案可读 + 语义图标(非仅色);停留时长足够阅读且可调;不抢焦点(不打断主操作 = 不破坏键盘/手柄焦点链);`reduce_motion` 下去滑动改瞬现/淡入。
**When to Use**: 一次性、信息层、无需响应的事件告知(范式底座,P-10 是其特化)。
**When NOT to Use**: 需玩家决策/确认(用 P-03/P-04);需持久状态(用 P-15);关键阻塞流程的提示(用 modal)。

---

### P-12 终局覆盖层 (Endgame Overlay)
**Category**: Overlay
**Used In**: 破产玩家面板态(hud CR-5)/ 胜利终局(hud CR-13 / bankruptcy §VA / VFX CR-7)
**Description**: 一局的情绪曲线在两极收束的全屏/面板级覆盖呈现——破产玩家面板去饱和退场(终态),胜者被彩带烟花包围。这是 concept Core Fantasy"最后只剩你笑到最后"的兑现时刻。HUD 不独立判胜负(裁决归破产9/回合2),只呈现既定结果。
**Specification**:
- **组件行为**: HUD 订阅经济5 `OnBankruptcyDeclared(Debtor,Creditor)` → Debtor 面板置破产终态(去饱和 60% + 灰对角纹,art-bible §7.4a,hud CR-5);订阅回合2 `OnGameWon(winnerId)` → GameOver 覆盖层(hud CR-13);破产终态 > 进行中 counter(hud CR-5 并发优先级);GameOver 时面板冻结逻辑态(hud 状态机不变式)。胜利彩带/破产消散 juice 归 VFX19(CR-7,受 `N_confetti_max` 上界)。`winnerId==INDEX_NONE`(退化局)→ 中性 game-over,不臆造赢家(hud EC-10)。
- **输入映射**: GameOver 覆盖层可含「返回主菜单/再来一局」按钮(`UCommonButtonBase`,经 P-04 确认路径);破产面板态无交互。
- **视/听反馈**: 破产去饱和渐变 + 棋子消散(VFX,art-bible 状态 G);胜利彩带烟花从四周涌入(art-bible 状态 F)+ 胜利音乐(bankruptcy §VA);破产音 + BGM duck(economy L334)。
- **a11y (WCAG-AA)**: 破产态**不仅靠去饱和**——叠加灰对角纹(形状冗余,art-bible §7.4a,与抵押同语义);胜者经棋子高亮+文案明示(非仅彩带);覆盖层文字 ≥10px、对比达标;`reduce_motion` 下彩带降强度但终局信息不丢。
**When to Use**: 标志一局/一名玩家终态的不可逆全屏/面板级状态收束。
**When NOT to Use**: 可恢复的中间态(用面板态 P-07);非终局的庆祝(用 toast/juice)。

---

### P-13 菜单导航 (Menu Navigation)
**Category**: Navigation
**Used In**: 主菜单 / 房间大厅(main-menu-lobby CR-2..CR-6)
**Description**: 玩家从启动游戏到坐进对局之间的配置与编排导航——主菜单顶层入口(新游戏/续局/联机/设置/退出)、大厅席位配置、开始游戏。每一步都应明确、轻量、无挫败(支柱④易上手:随时和朋友开一局)。专属 `IMC_Menu` 层,与对局输入互不干扰。
**Specification**:
- **组件行为**: `WBP_MainMenu`/`WBP_Lobby : UCommonActivatableWidget`(ADR-0012),入根激活栈;进入菜单调 `ActivateMenuInput()`(`IMC_Menu` P=10),进对局调 `DeactivateMenuInput()`(ADR-0011)。MVP 仅"新游戏→大厅"路径完整;续局/联机/House Rules 入口禁用/隐藏(OQ-Lobby-2/3/5)。"开始游戏"在配置合法时启用(main-menu-lobby F-1/CR-5)。
- **输入映射**: `IMC_Menu`(P=10)+ CommonUI 焦点导航。**鼠标**: 点击入口/按钮。**手柄**: `GetDesiredFocusTarget` 设默认焦点(如"开始游戏"),方向键/摇杆移焦点、确认/返回经输入无关路径(ADR-0012 FR-3,零代码手柄)。
- **视/听反馈**: art-bible 状态 A(暖白光、棋盘呼吸、玩家"落座"弹性入场,main-menu-lobby L26);按钮弹性(§7.5)。
- **a11y (WCAG-AA)**: 焦点恒可见、有合理初始焦点(FR-3);所有入口禁 hover-only;禁用入口经不透明度+图标区分;开始按钮禁用时就近提示原因("颜色重复"/"名字为空",main-menu-lobby CR-5);≥10px。
**When to Use**: 屏间导航 + 前置配置流(菜单/大厅是范式)。
**When NOT to Use**: 对局内交互(用 Gameplay 层模式);模态弹窗(用 P-06/P-03)。

---

### P-14 棋盘平移缩放 (Board Pan & Zoom)
**Category**: Navigation
**Used In**: 中央棋盘区(ADR-0011;hud CR-1 棋盘区 ≥65% 非 HUD 拥)
**Description**: 玩家平移/缩放棋盘视角以查看远处地块或纵览全盘——这是唯一的**模拟量(Axis)输入**交互(其余皆 Digital 点击)。鼠标拖拽/滚轮、手柄摇杆/扳机统一映射到 Axis IA。
**Specification**:
- **组件行为**: 棋盘视角控制经 `Handle_BoardPan`/`Handle_BoardZoom`(ADR-0011)转发至棋盘视角逻辑。属 `IMC_Gameplay`(P=0)——modal 弹窗(P=20)激活时被截获,不在弹窗中误触平移。
- **输入映射**: `IA_BoardPan`(Value Type **Axis2D**,配 `Dead Zone` Modifier 阈值 0.1 防鼠标微抖,ADR-0011)、`IA_BoardZoom`(Value Type **Axis1D**)。**鼠标**: 拖拽平移 / 滚轮缩放。**手柄**: 右摇杆平移 / 扳机或肩键缩放(IMC 内追加,零代码)。
- **视/听反馈**: 平滑跟随,无弹性(导航非动作);缩放有合理上下限(具体归 `/ux-design`)。
- **a11y (WCAG-AA)**: Dead Zone 防误触(运动 a11y);**非 hover-only**——平移/缩放有显式输入路径;缩放级别须保证最小可读(≥10px tile 信息)不被缩没;手柄速度可调(预留)。
**When to Use**: 连续/模拟量的视角或值调整(平移缩放是 MVP 唯一 Axis 用例)。
**When NOT to Use**: 离散触发(用 Digital `IA_*`);需精确单步的调整(用按钮增减,如 P-17 席位增减)。

---

### P-15 地产状态常驻叠加 (Persistent Tile Status Overlay)
**Category**: Overlay
**Used In**: 棋盘 tile 抵押/垄断状态(#17 CR-10/V-Own-08;HUD F-4 依赖)
**Description**: **即使不开详情卡**,棋盘各 tile 常驻呈现抵押(去饱和+斜纹)与垄断组完成(环圈+金角标)状态叠加,让"谁垄断了、谁抵押缺钱"不开卡即可一眼扫(支柱②即时侦察)。它是 HUD F-4 把 AI `Mortgage`/`Unmortgage` 判为非 critical 可跳过的前提通道——其结果状态在此常驻可见。与按需开关的详情卡(P-06)分层:叠加承担即时侦察、详情卡承担深算。
**Specification**:
- **组件行为**: #17 订阅所有权6 `OnMortgageChanged(TileIndex,bMortgaged)`/`OnOwnershipChanged(TileIndex,Old,New)` + 建房8 `OnBuildingChanged`,维护常驻叠加(独立于卡片开关,#17 CR-10/状态机不变式②)。叠加 widget 所有权/生命周期(候选: 每 tile world-space `UWidgetComponent` / 单一 screen-space overlay)归架构 ADR(OQ-PC-4,与 OQ-HUD-2 同门控),由常驻 owner 持有覆盖整局。
- **输入映射**: 无(纯只读状态呈现);点击 tile 触发 P-06 开卡是另一交互。
- **视/听反馈**: 抵押=整格去饱和 60%+对角斜纹(art-bible §6.2);垄断完成=2px 色组环圈+Fortune Gold 完成圆点(#17 V-Own-07/08);共存时斜纹+环圈各自独立(抵押覆盖优先级高)。
- **a11y (WCAG-AA)**: 抵押**不仅靠去饱和**——叠斜纹(形状冗余)+ 可选"M"图标(CA-01);垄断完成=环圈(形状)+「全」/★(文字/图标)+ 金色三重冗余(#17 V-Own-07);小 tile 尺寸下仍可辨(降级为纯金点);所有权徽章经玩家色+P 编号双重(CA-01)。
**When to Use**: 需要"不打开任何面板即可一眼扫"的持久状态侦察。
**When NOT to Use**: 一次性事件(用 P-10/P-11);深度信息(用 P-06);需玩家响应的。

---

### P-16 只读 Affordance 按钮态 (Read-Only Affordance Buttons)
**Category**: Data Display
**Used In**: HUD 操作栏(hud CR-14)/ 地产卡动作按钮组(#17 CR-7/CR-8)
**Description**: 所有交互按钮的**启用/禁用态是对只读 affordance 的呈现反映,非呈现层决策**——如非自己 RollPhase 时掷骰按钮禁用、`CanBuildHouse==false` 时建房按钮禁用。呈现层读 owner 系统的 affordance 反映,点击只转发意图;权威门始终以 owner 为准,affordance 非权威闸。这是一条贯穿所有按钮的呈现纪律,保证"按钮看上去能不能点"与"实际能不能做"一致。
**Specification**:
- **组件行为**: 按钮启用条件读 owner affordance——掷骰读回合阶段(hud CR-14)、建房读建房8 `CanBuildHouse`(#17 CR-7)、抵押读 `owner==当前玩家 ∧ ¬is_mortgaged ∧ GetHouseCount==0`(#17 CR-8)、赎回读现金≥`unmortgage_cost`(读 owner `GetUnmortgageCost`,#17 不自算)。误启用 → owner 权威拒绝(无害,#17 EC-11);禁用建房按钮显门控原因 tooltip(building L265)。
- **输入映射**: 启用时走对应 IA(P-01/P-04/P-05);禁用时不触发(且 AI 回合 `IMC_Gameplay` 移除,双重阻断,ADR-0011)。
- **视/听反馈**: 禁用态去饱和至灰度 + 不透明度 50%(art-bible §4.5);启用态正常。
- **a11y (WCAG-AA)**: 禁用态**经不透明度+灰度+图标形状区分,非仅靠色**(CA-04③);所有 affordance **非 hover 即可读**(hud AC-45,禁 hover-only);门控原因 tooltip 手柄可达(#17 AC-40);启用/禁用状态对屏幕阅读/低视力清晰可辨。
**When to Use**: 任何呈现层按钮——作为按钮启用/禁用的统一纪律(横切所有交互按钮)。
**When NOT to Use**: 呈现层自己裁决可点性(违反纯叶子边界——affordance 必读 owner)。

---

### P-17 席位配置控件 (Seat Configuration Controls)
**Category**: Input
**Used In**: 房间大厅席位列表(main-menu-lobby CR-2/CR-3/CR-4)
**Description**: 大厅维护一份席位列表(2–4 启用 MVP,接口预留 8),每席位可配:启用/停用、玩家名、棋子色(席位间互斥)、人类/AI、AI 难度档。调整全靠点击(增减座位、切颜色、换难度),不需填表单、不需读说明(支柱④低摩擦)。颜色互斥经选择器置灰实现。
**Specification**:
- **组件行为**: 席位增减按钮(`P==P_min` 时减按钮禁用、`P==P_max` 时增按钮禁用,main-menu-lobby CR-3/EC-1/EC-2);颜色选择器——已占用色在其余席位**置灰不可选**(CR-3),释放后重新可选;玩家类型切 AI 时 `AIDifficulty` 从 None 变默认 Normal 并显三档选择器(CR-4);难度档仅选择,行为差异归 AI10。属 `IMC_Menu`(P-13)。
- **输入映射**: `IMC_Menu`(P=10);各控件 `UCommonButtonBase` 点击。**鼠标**: 点击增减/选色/切类型/选难度。**手柄**: CommonUI 焦点导航 + 确认(零代码,ADR-0012)。
- **视/听反馈**: 置灰=去饱和至灰度+不透明度 50%(art-bible §4.5 禁用态);席位"落座"弹性入场(art-bible 状态 A)。
- **a11y (WCAG-AA)**: 棋子色选择**不仅靠色块**——须辅以 P 编号/色名标签(色盲安全,CA-01 同源);禁用增减/置灰色经不透明度+(图标)区分非仅色;名字为空/颜色重复时就近提示文案(CR-5/EC-3/EC-4),**不自动改色/填名**(尊重玩家意图);≥10px;全控件禁 hover-only。
**When to Use**: 前置配置的多字段表单式选择(席位配置是范式;Alpha 交易/House Rules 配置可复用)。
**When NOT to Use**: 对局内交互(菜单专属 IMC);连续量(用 P-14 Axis)。

---

## Gaps & Patterns Needed（缺口与待编目模式）

> 以下交互模式在 MVP 范围外或对应 GDD 未撰写,**本库 v0.1 不编目**——避免臆造。Alpha 阶段对应 GDD 落定后回补。

| 缺口 | 预期 Category | 阻塞来源 | 何时编目 |
|------|---------------|----------|----------|
| **交易 UI 拖拽/选择** (Trade Drag-Select) | Input / Modal | 交易(11) GDD = Alpha(art-bible §7.4e 有视觉方向但无交互 GDD);ADR-0011 预留 `IMC_PostRollAction` 第四层 | Alpha 交易 GDD Approved 后 |
| **拍卖出价输入** (Auction Bid Input) | Modal / Input | 拍卖(12) GDD = Alpha(art-bible §7.4c 有视觉方向;player-turn `DecideAuctionBid` 已预留 sentinel 值域但 UI 未定) | Alpha 拍卖 GDD Approved 后 |
| **命运/社区卡翻牌** (Card Flip Reveal) | Modal / Feedback | 事件格卡牌 UI deferred Alpha(#17 CR-2;依赖未定 #7 `FCardData`,OQ-PC-1) | Alpha #7 `FCardData` 定 + #17 翻牌卡设计后 |
| **监狱动作卡** (Jail Action Card) | Modal | 同上 deferred Alpha(#17 CR-2) | Alpha 后 |
| **命运之轮交互** (Wheel Spin) | Input / Feedback | 命运之轮(13) = Alpha,GDD 未撰写 | Alpha 后 |
| **策略卡选用** (Strategy Card Play) | Modal / Input | 策略卡(15) = Alpha,GDD 未撰写 | Alpha 后 |
| **续局/读档入口** (Load Game Entry) | Navigation | 存档(21) GDD 未撰写,MVP 入口禁用(OQ-Lobby-2) | #21 GDD 定后 |

---

## Open Questions

| ID | 问题 | Owner | 期限 |
|----|------|-------|------|
| **OQ-IP-1** | 各模式的**精确点击目标最小尺寸 / 手柄焦点导航顺序 / handoff 输入锁定窗口** | ux-designer | Pre-Production `/ux-design`(对齐 OQ-HUD-9/OQ-HUD-6) |
| **OQ-IP-2** | P-05 长按建房进度反馈的具体呈现(进度环/填充)+ Hold Time 默认值校准 | ux-designer + ue-umg-specialist | Pre-Production(playtest 校准误触率) |
| **OQ-IP-3** | P-15 常驻叠加 widget 所有权/生命周期(world-space `UWidgetComponent` vs screen-space overlay) | unreal-specialist(架构 ADR) | 架构期(OQ-PC-4,与 OQ-HUD-2 同门控) |
| **OQ-IP-4** | Tabular Figures 等宽——UE5.7 `FSlateFontInfo` 是否暴露 `tnum` feature API(P-09);若否须 art-bible §7.2 指定具体等宽数字字体资产名 | ue-umg-specialist + art-director | Pre-Production 前引擎源码核验(hud CR-3 propagate 债) |
| **OQ-IP-5** | `reduce_motion=On` 时各模式的**具体降幅参数**(瞬现 vs 短线性 vs 淡入) | ux-designer | Pre-Production `/ux-design`(OQ-HUD-9,全 a11y 共享布尔) |
| **OQ-IP-6** | P-10 离屏玩家"回看上一轮对手建房摘要"补偿机制(MVP 单屏 banner 对下一持屏者不可见) | ux-designer + game-designer | Pre-Production(OQ-HUD-13,非 MVP 阻塞) |

---

> **验证**: 运行 `/ux-review design/ux/interaction-patterns.md` 校验完整性、a11y 合规、GDD 对齐后,再交 `/team-ui` 实现。
> **维护**: 任一交互 GDD 修订(尤其新增交互意图)须回查本库——新意图须落 ADR-0011 `IA_`/`IMC_` 资产并在此编目对应模式。
