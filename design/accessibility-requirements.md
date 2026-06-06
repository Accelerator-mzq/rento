# 无障碍需求（Accessibility Requirements）— 骰子大亨

> **Status**: v0.1 Draft（2026-06-06，用户裁定 WCAG-AA 基线）
> **Author**: msc + ux-designer agent
> **Scope**: 跨系统呈现层无障碍基线。约束 HUD(16)、地产卡 UI(17)、VFX(19)、art-bible 视觉规格、CommonUI/Enhanced Input 输入层。
> **引擎**: Unreal Engine 5.7（Blueprint 为主 + CommonUI / Enhanced Input）
> **目标平台**: PC (Steam)，鼠标点击为主 + 手柄 Partial 预留
> **本文件定位**: 它**不是**新游戏系统，而是**汇聚并固化**散落在 art-bible、HUD、property-card-ui 等已 Approved 文档中的无障碍承诺，给出统一可验收的基线层级。各源文档的局部 a11y 规则（art-bible §4.6 / §7.2 / §7.4 / §7.5、HUD AC-57、property-card CA-01..05）是本文件的**实例化落点**；本文件提供它们共同遵循的**层级声明 + 横切 QA checklist + 可证伪 AC**。

---

## 1. 层级声明（Conformance Level Declaration）

**本作无障碍基线 = WCAG 2.1 Level AA。**

**为何选 AA**：AA 是业界游戏与软件无障碍的事实标准（对比度 4.5:1、可键盘操作、不依赖单一感官通道），并与本作既有承诺**天然吻合**——art-bible 已规定色盲非颜色冗余（§4.6）、最小 10px 字号（§7.2）、reduce-motion 选项（HUD AC-57），这些恰是 AA 的核心要求。选 AA 而非 AAA，是为**不过度承诺**：AAA 的若干条款（如全文 7:1 对比、无任何时间限制、完整屏幕阅读器叙述）对一款实时回合制棋盘游戏成本高、收益边际递减，且部分（如倒计时拍卖的时间压迫感）与游戏设计意图（状态 C 拍卖的 Urgency）冲突。AAA 难达项作为 **Open Question / Full Vision** 留档（见 §9），不在 MVP 强制范围。

**适用范围**：本基线约束**所有呈现层产出**（UMG widget、棋盘 tile 叠加、语义色应用、动效、输入路由）。游戏逻辑层（经济/回合/破产数值）不在 a11y 范围，但其**呈现**（如现金数字、租金表）受本基线约束。

**裁定边界**：WCAG 原为 Web 标准，本作按其**精神**适配 PC 游戏——对比度/非颜色冗余/键盘可达/动效可关等条款直接套用；纯 Web 专有条款（如 HTML 语义标签、ARIA role）按等效原生 UMG 实践兑现，不字面套用。

---

## 2. 对比度（Contrast）

### 2.1 AA 对比度红线（继承 art-bible §4 色彩系统）

| 元素类别 | 最低对比度 | WCAG 依据 | 本作落点 |
|---|---|---|---|
| **正文文本 / 数字**（< 18pt 常规 或 < 14pt 粗体） | **4.5:1** | AA 1.4.3 | 现金数字、租金阶梯、地块名、tooltip（HUD CR-3 / property-card V-Own-01..03） |
| **大号文本**（≥ 18pt 或 ≥ 14pt 粗体） | **3:1** | AA 1.4.3 | 事件标题、面板大金额、购买价（art-bible §7.2 Level 1/2） |
| **UI 组件 / 图形对象**（按钮边界、图标、状态指示、聚焦框） | **3:1** | AA 1.4.11 | 操作按钮、所有权徽章、色组色条、聚焦指示器（art-bible §7.3/§7.4） |

**主文字色基准**：art-bible 主文字用 Dark Ink `#1A1A2E`（非纯黑）置于 Board Cream `#F5EED8` / Off White `#FAFAF5` 背景——此组合须实测 ≥ 4.5:1（深墨对暖白名义对比约 14:1，安全；但**次要文字 Dark Ink 60% 不透明度**须复测，半透明叠加后可能逼近红线，见 AC-3）。

### 2.2 语义色 + 非颜色冗余（核心要求，继承 art-bible §4.3 / §4.6）

**WCAG AA 1.4.1（不仅用颜色传达信息）是本作硬约束。** art-bible §4.3 的语义色系统——**红=危险/扣钱、金=金钱/奖励、绿=持有/确认、蓝=信息/中性、灰=失效/破产**——**绝不可单独承载语义**，每一处语义色都必须叠加**至少一个非颜色冗余通道**（图标 / 文字标签 / 纹理 / 形状 / 位置）。

| 语义色 | 语义 | 非颜色冗余（必达） | 源 |
|---|---|---|---|
| 红 Property Red | 危险/扣钱/付租 | 金额前缀「−」+ 下行箭头图标 + 付出语境 | art-bible §4.3 / HUD CR-3 |
| 金 Fortune Gold | 金钱/奖励/CTA | 金额前缀「+」+ 奖励图标 + 入账语境 | art-bible §4.3 |
| 绿 Tycoon Green | 持有/确认/安全 | ✓ 图标 + 文字（「确认」「我的」） | art-bible §4.3 / §4.6 |
| 蓝 Sky Blue | 信息/中性/交易 | ⓘ 图标 + 文字标签 | art-bible §4.3 |
| 灰（去饱和） | 失效/不可用/破产 | **去饱和 + 对角斜纹 + 文字（「已抵押」「破产」）** | art-bible §7.4 / property-card CA-03 |

> **范例（破产去饱和 + 对角纹，art-bible §7.4 / §G）**：破产面板 = 去饱和 60% + 灰对角纹 + （可选）哭丧脸表情 + 不透明度 60%。这是「不仅靠灰色」的标杆实现——色觉正常与色盲玩家都能从**纹理 + 饱和度 + 文字**三通道辨识，颜色仅是第四冗余。抵押地产卡（property-card V-Own-06）、棋盘 tile 抵押叠加（CR-10）复用同一三通道模式。

**色盲安全（art-bible §4.6）纳入 UI QA checklist**：红绿色盲高风险对（红色组 vs 绿色组地产、绿确认 vs 红危险、P1 红 vs P3 绿）必须以 Coblis（Deuteranopia/Protanopia）模拟器逐项验证非颜色信号可辨。见 §7 checklist + AC-2。

---

## 3. 字体（Typography）

### 3.1 最小字号红线（继承红线，跨档一致）

**游戏内可读文字 ≥ 10px @1080p**（art-bible §7.2「最小字号」+ §5.2 玩家编号标签 ≤10px 下限 + §7.2 Level 4 注释 10–12px）。此值为**继承自 art-bible 的硬下限**，HUD（EC-12 八玩家紧凑面板仍须满足 §7.2 10px）与 property-card（CA-01 P 编号 ≥10px）均回引同一红线——三档一致，不得单档放宽。

> **⚠ 跨档一致性（property-card OQ-PC-12）**：10px 是名义下限，但 a11y 角度 10px 偏小（AA 无绝对像素下限，但低 DPI/大屏远观下 10px 可读性脆弱）。**10px → 文本缩放（text-scale）的关系**登记为 **OQ-PC-12 / OQ-A11Y-3**（见 §9）：10px 是**未缩放基线**下限，文本缩放选项开启后实际渲染像素须按缩放因子放大，10px 红线针对的是「布局设计基准」而非「最终渲染上限」。

### 3.2 数字用 Tabular Figures（防跳动）

所有动态变化的数字（现金 counter、租金额、骰点）须用 **Tabular Figures（等宽数字字形）**，使数字横向不跳动（art-bible §7.2 / HUD CR-3 / property-card V-Own-04）。

> **⚠ 实现约束（继承 HUD CR-3 / property-card V-Own-04 propagate 债）**：UE `FSlateFontInfo` 公开层**未见** per-run OpenType `tnum` feature 控制 API（截止 ≤UE5.3 知识 + engine-ref 核验；UE5.7 须架构期 ue-umg-specialist 正向核验）。故「等宽数字」**保守按选用数字字形本身等宽的字体资产**实现，不依赖渲染器激活 tnum。**art-bible §7.2 须落具体等宽数字字体资产名**——此为已登记的 propagate 债（缺则 WBP TextBlock 无可绑字体）。本文件仅引用该债，不重复 owner。

### 3.3 文本缩放选项（Text Scaling）

提供**文本缩放旋钮**（`text_scale`，见 Tuning Knobs），让低视力玩家放大全局 UI 文字。MVP 提供离散档位（如 100% / 125% / 150%），缩放须不破坏布局（文本容器须可换行/自适应，不溢出截断）。缩放上限留 **OQ-A11Y-3**（过高缩放下重度 UI 的布局重排可行性须 `/ux-design` 验证）。

---

## 4. 输入与导航（Input & Navigation）

### 4.1 禁 hover-only（技术偏好硬约束）

**所有交互必须有显式点击 / 确认路径**——禁止任何仅靠鼠标悬停（hover）才能触发或读取的功能（technical-preferences「避免 hover-only 交互，以利后续手柄适配」；HUD CR-14 / property-card CR-11/CA）。tooltip 等悬停辅助信息**可保留**，但其承载的关键信息（如建房门控原因）须另有**可达**呈现（点击/聚焦展开），不得是 hover 独占。对应 AA 2.1.1（键盘可操作）+ 1.4.13（hover/focus 内容可达）。

### 4.2 全键盘可达 + 手柄完整导航

| 要求 | 兑现机制 | ADR |
|---|---|---|
| **全键盘可达**：所有可操作元素（掷骰、买地、建房、抵押、菜单、关卡片）可经键盘到达并激活 | Enhanced Input 三层 IMC（`IMC_Gameplay` / `IMC_Menu` / `IMC_Modal` 优先级叠加），每意图独立 `IA_` 资产；禁 legacy BindAction | **ADR-0011** |
| **手柄完整导航**：手柄可遍历全部 UI，无鼠标专属死角 | CommonUI 全套（`UCommonActivatableWidget` + `UCommonButtonBase` 输入无关按钮 + 激活栈）；屏激活时 `GetDesiredFocusTarget` 给默认焦点；焦点链由 widget 树定义 | **ADR-0012** |
| **默认焦点 + 焦点链** | 每可激活屏（HUD/地产卡/模态）`GetDesiredFocusTarget` 返回首焦点元素；CommonUI 激活栈 push/pop 调 `SwitchToModal`/`ReturnFromModal`（ADR-0012↔0011 协作） | ADR-0011 + ADR-0012 |

> 注：手柄支持目标为 **Partial**（technical-preferences）——MVP 须保证「无鼠标专属死角 + 可聚焦导航结构就位」，完整手柄手感打磨（震动、提示图标切换）留 Pre-Production `/ux-design` + 后续。

### 4.3 焦点指示器（Focus Indicator，AA 2.4.7 必备）

**当前聚焦元素必须有清晰可见的视觉指示器**（键盘 Tab / 手柄方向导航时）。指示器须满足 §2.1 的 3:1 UI 组件对比（聚焦框对其背景），且**不依赖颜色单一通道**（推荐：描边 + 轻微放大/亮度提升，复用 art-bible §4.5 hover 的 +10% 亮度 / scale 1.02 视觉语言，但聚焦态须与 hover 态可区分）。具体描边规格留 `/ux-design`（OQ-A11Y-4）。

---

## 5. 动效（Motion）

### 5.1 reduce-motion（防眩晕，AA 2.3.3 精神 + 前庭安全）

提供 **reduce-motion 开关**（`reduce_motion` 旋钮，全局布尔），开启时**关闭或降级**以下动效，防止前庭敏感玩家眩晕：

| 动效 | 默认（Off） | reduce_motion=On | 源 |
|---|---|---|---|
| **现金 counter 滚动** | F-1/F-2 逐帧插值（≤1.5s） | 瞬切到最终值（旁路 counter） | HUD AC-57 / CR-3 |
| **当前回合高亮过渡** | Ease-Out Cubic 200–400ms + scale 1.05 | 瞬切 / 更短线性，去缩放弹性 | HUD AC-57 / F-3 |
| **通知 / 通告 banner 滑入** | ease-out 上滑 200ms | 瞬现（无滑动） | HUD AC-57⑤ / F-5 |
| **卡片 pop-in / flip** | 250 / 300ms 弹性 | 瞬现，按钮去 scale | property-card Tuning（`reduce_motion` 共享同一布尔）|
| **juice（飞溅/弹跳/粒子节奏）** | VFX19 全效 | 降级 / 抑制（VFX19 据同一布尔旁路） | art-bible §7.5 / HUD enable-not-own |

> **统一开关原则**：`reduce_motion` 是**全局单一布尔**，HUD（AC-57）、property-card（Tuning 旋钮）、VFX19 **共享同一值**，不得各自独立——避免「关了一处还在动」的割裂（property-card Tuning「与 HUD 全局 a11y 开关共享同一布尔」）。

### 5.2 reduce-motion 谓词的可验证形态（继承 HUD AC-57 R-3 修订）

reduce_motion=On 的验收**断言「各动画有效配置参数被抑制」**（如 counter 时长读回为 0/旁路、滑入位移为 0），**非**断言「scale 不越 1.05」之类永真陷阱（HUD R-3 已修正该谓词为配置参数读回 + T_min 旁路）。渲染侧的「确实无超冲视觉」归 [Advisory] 截图（headless 仅断言配置参数）。本文件 AC-6 复用此形态。

---

## 6. 色盲模式（Color Blindness）

### 6.1 MVP 基线 = 非颜色冗余（必达）

**MVP 不依赖专用色盲滤镜，而以「非颜色冗余」为底线**（§2.2）——这是比滤镜更稳健的方案：所有语义/状态在**无任何色盲辅助**下，仅凭图标/文字/纹理/形状/位置即可辨识。具体落点：

- 所有权：玩家色 + **P 编号文字**双冗余（property-card CA-01；P1 红 vs P3 绿高风险对靠编号）
- 抵押：去饱和 + 斜纹 + 「已抵押」文字三冗余（CA-03）
- 垄断完成：色组环圈（形状）+ 「全」/★（文字）+ 金色三冗余（CA-02 / V-Own-07）
- 高亮租金行：背景块 + 左竖线（位置）+ 字重 + 「×2」文字（CA-04）
- 确认/危险按钮：✓ / ✗ 图标 + 文字（art-bible §4.6）

### 6.2 专用色盲滤镜模式 = Alpha（OQ）

全屏色盲校正滤镜（Deuteranopia/Protanopia/Tritanopia 三档后处理）作为**锦上添花**，留 **OQ-A11Y-1 / Alpha**——MVP 不实现，因非颜色冗余已保证可玩性底线，滤镜为体验增强非可玩性前提。

---

## 7. 屏幕阅读器（Screen Reader）

### 7.1 MVP 基线 = 文本可读性（回合制棋盘的低门槛）

本作为**回合制棋盘游戏**（非实时动作），其状态变化节奏慢、信息以文本/数字为主——MVP 的 SR 基线是**确保关键信息以文本形式存在且字号/对比合规**（现金、当前玩家、租金、事件结果均有文字表述，非纯图标/纯听觉）。这为后续 SR 接入留出可能，但 **MVP 不实现 OS 级屏幕阅读器朗读**。

### 7.2 完整 SR 支持 = Full Vision（OQ）

完整屏幕阅读器支持（焦点元素朗读、状态变化语音播报、棋盘空间的可听导航）留 **OQ-A11Y-2 / Full Vision**——涉及 CommonUI 可访问性 API、UMG `AccessibleWidget` 数据、平台 SR 桥接，成本高，超出 MVP 范围。

---

## 8. QA Checklist（可勾项）

> 在 UI 实现阶段与每次呈现层 PR 前执行。每项给出 PASS/FAIL；FAIL 须开 bug 或登记 OQ。

**对比度**
- [ ] 正文文本/数字 ≥ 4.5:1（含 Dark Ink 60% 次要文字实测）
- [ ] 大号文本 ≥ 3:1
- [ ] UI 组件/图标/聚焦框 ≥ 3:1

**色盲**
- [ ] Coblis Deuteranopia 模拟：红色组 vs 绿色组地产可辨（非纯色）
- [ ] Coblis Protanopia 模拟：绿确认 vs 红危险按钮可辨（✓/✗ 图标）
- [ ] P1 红 vs P3 绿玩家可辨（P 编号）
- [ ] 抵押 vs 正常、垄断完成 vs 普通持有可辨（纹理/环圈/文字）

**字体**
- [ ] 全部游戏内文字 ≥ 10px @1080p（含 8 玩家紧凑面板、P 编号标签）
- [ ] 动态数字用等宽数字字形（横向不跳动）
- [ ] 文本缩放各档位（100/125/150%）无溢出截断

**键盘**
- [ ] 所有可操作元素（掷骰/买地/建房/抵押/赎回/菜单/关卡片）键盘可达且可激活
- [ ] 无 hover-only 死角（hover 关键信息另有点击/聚焦可达路径）

**手柄**
- [ ] 手柄可遍历全部 UI，无鼠标专属死角
- [ ] 每屏激活有默认焦点（GetDesiredFocusTarget）

**焦点**
- [ ] 焦点指示器清晰可见（键盘 Tab + 手柄导航）且 ≥ 3:1 对比
- [ ] 焦点态与 hover 态视觉可区分

**reduce-motion**
- [ ] reduce_motion=On：counter 旁路瞬切、高亮去弹性、banner 瞬现、卡片去 pop-in、juice 降级
- [ ] reduce_motion 为全局单一布尔（HUD/卡片/VFX 共享，无割裂）

**hover**
- [ ] 全项目无 hover-only 交互（已含于「键盘」项，独立复核）

---

## 9. Tuning Knobs

| 旋钮 | 来源 | 默认 | 安全范围 | 影响 |
|---|---|---|---|---|
| `reduce_motion` | 全局 a11y（HUD AC-57 owner） | Off | {Off, On} | On=各动画旁路/降级（§5），防眩晕；HUD/卡片/VFX 共享同一值 |
| `text_scale` | 全局 a11y | 100% | {100%, 125%, 150%}（上限留 OQ-A11Y-3） | 全局 UI 文字缩放；过高可能布局溢出（须自适应容器） |
| `colorblind_filter` | 全局 a11y（Alpha） | Off | {Off, Deuter, Protan, Tritan}（**Alpha 实现，MVP 仅占位**） | 全屏色盲校正后处理；MVP 非颜色冗余已保底，本旋钮 Alpha |

> 对比度红线（4.5:1 / 3:1）、最小字号（10px）、非颜色冗余**非旋钮**——是硬约束红线，不可调低。

---

## 10. Acceptance Criteria

> 全部可证伪。对比度/字号/色盲为可测量或工具可模拟项；键盘/手柄/reduce-motion 为可执行验证项。

- **AC-1** [Logic·可测量] 用对比度计算器（WebAIM/APCA 等效）测全部正文文本/数字对其实际背景：**≥ 4.5:1**；大号文本 **≥ 3:1**；UI 组件/图标/聚焦框 **≥ 3:1**。任一文本对低于红线 = FAIL。**特别钉死**：Dark Ink 60% 次要文字对 Off White 背景须实测通过（半透明叠加后实际色）。
- **AC-2** [Logic·工具模拟] 以 Coblis（或等效）跑 Deuteranopia + Protanopia 模拟，对以下高风险对逐一验证非颜色信号可辨：① 红色组 vs 绿色组地产；② 绿确认 vs 红危险按钮；③ P1 红 vs P3 绿玩家；④ 抵押 vs 正常；⑤ 垄断完成 vs 普通持有。任一在模拟下**仅凭颜色不可辨** = FAIL。
- **AC-3** [Logic·可测量] 截取 8 玩家紧凑面板 + P 编号标签 + Level 4 注释，量测渲染字号：**全部 ≥ 10px @1080p**。任一 < 10px = FAIL（继承 art-bible §7.2 红线，跨档一致）。
- **AC-4** [Logic·可执行] 全键盘遍历测试：从游戏开始，仅用键盘到达并激活掷骰 / 买地 / 建房 / 抵押 / 赎回 / 打开地产卡 / 关闭卡片 / 主菜单全部入口。存在任一**键盘不可达或不可激活**的可操作元素 = FAIL（AA 2.1.1；ADR-0011 三层 IMC 兑现）。
- **AC-5** [Logic·可执行] 全手柄遍历测试：仅用手柄遍历全部 UI 屏（HUD / 地产卡 / 模态弹窗 / 菜单），每屏切入有默认焦点（GetDesiredFocusTarget 非空）。存在**鼠标专属死角**或**屏切入无焦点** = FAIL（ADR-0012 CommonUI 兑现）。
- **AC-6** [Logic·配置参数读回] 设 `reduce_motion=On`，断言各动画有效配置参数被抑制：现金 counter 时长读回为旁路/0、高亮过渡去 scale 弹性、banner 滑入位移为 0、卡片 pop-in 瞬现、`reduce_motion` 为 HUD/卡片/VFX 共享的同一布尔。任一动画仍按 Off 参数运行 = FAIL（继承 HUD AC-57 R-3 谓词形态，非永真陷阱）。渲染侧无超冲视觉确认归 [Advisory] 截图。
- **AC-7** [Logic·可执行] hover-only 审计：枚举全部承载关键信息的 hover/tooltip（建房门控原因、抵押费率等），逐一验证其信息**另有点击或聚焦可达路径**。存在任一 hover 独占的关键信息 = FAIL（AA 1.4.13 + technical-preferences 禁 hover-only）。
- **AC-8** [Logic·可执行] 焦点指示器测试：键盘 Tab + 手柄导航逐元素切换，当前焦点元素**有可见指示器**且其对背景 ≥ 3:1，且与 hover 态视觉可区分。存在**无可见焦点指示**的可聚焦元素 = FAIL（AA 2.4.7）。
- **AC-9** [Advisory·可执行] 文本缩放测试：`text_scale` 各档位（100/125/150%）下截图全部重度 UI 屏，无文本溢出截断 / 布局崩坏。FAIL 登记 OQ-A11Y-3（上限）。

---

## 11. Open Questions

| ID | 问题 | 归属 | 阶段 |
|----|------|------|------|
| **OQ-A11Y-1** | 专用色盲滤镜模式（全屏 Deuter/Protan/Tritan 后处理校正）——MVP 仅 `colorblind_filter` 占位旋钮，非颜色冗余已保底，滤镜实现 | technical-artist / ue-umg | **Alpha** |
| **OQ-A11Y-2** | 完整屏幕阅读器支持（焦点朗读 / 状态播报 / 棋盘可听导航 / CommonUI accessibility API + 平台 SR 桥接） | ue-umg-specialist | **Full Vision** |
| **OQ-A11Y-3**（= property-card OQ-PC-12） | 文本缩放上限 + 10px 基线与缩放因子关系（重度 UI 在高缩放下的布局重排可行性） | ux-designer / ue-umg | Pre-Production `/ux-design` |
| **OQ-A11Y-4** | 焦点指示器具体描边/亮度规格（与 hover 态区分、art-bible §4.5 视觉语言对齐） | ux-designer / art-director | Pre-Production `/ux-design` |
| **OQ-A11Y-5** | art-bible §7.2 等宽数字字体**资产名**落定（继承 HUD CR-3 / property-card V-Own-04 propagate 债，缺则 WBP 无可绑字体） | art-director | Pre-Production（asset-spec 前） |

---

*骰子大亨 无障碍需求 v0.1 — 2026-06-06 — WCAG-AA 基线*
*Author: msc + ux-designer agent*
