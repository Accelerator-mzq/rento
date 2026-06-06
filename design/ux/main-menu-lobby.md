---
title: "UX Spec — 主菜单与大厅配置屏 (Main Menu & Lobby)"
screen-slug: main-menu-lobby
status: In Design
last-updated: 2026-06-06
template: UX Spec
version: v0.1
author: ux-designer
source-gdd: design/gdd/main-menu-lobby.md
---

# UX Spec — 主菜单与大厅配置屏 (Main Menu & Lobby)

## 1. Purpose & Player Need

**屏目标**：让玩家从启动游戏到坐进一局对战之间的每一步都是**明确、轻量、无挫败**的。本屏是游戏的入口，承担两个串联职责：

1. **主菜单层**——提供顶层导航入口（新游戏、续局、设置、退出；MVP 续局/联机占位禁用）。
2. **房间大厅配置层**——收集本地热座（Pass'N Play）2–4 玩家的开局配置（席位数、每席玩家名/棋子色/人类或 AI/AI 难度），在"开始游戏"时把配置打包为 `FGameSetupConfig` 传给玩家与回合管理（系统 #2），移交后本屏退场。

**玩家需求核心**：低摩擦、快速开局——"想玩了，三秒就能开一局"（GDD Player Fantasy，Pillar④）。配置过程不需要填表单、不需要读说明，全靠点击完成。

---

## 2. Player Context on Arrival

> *[provisional — player-journey.md 尚未建立；以下据 GDD Player Fantasy §2 + art-bible 状态 A 推断玩家情绪状态]*

| 到达路径 | 玩家情绪状态 | 期望 |
|---|---|---|
| **冷启动（游戏刚启动）** | 期待感（Anticipation）+ 轻松欢迎（Casual Welcome）；朋友在旁，准备开局 | 快速找到"新游戏"按钮，无需读说明 |
| **上一局结束后返回主菜单** | 短暂放松，可能想立刻再来一局；或需要调整配置（换 AI 难度） | 低阻力再次配置，默认值保持上一局合理预设 |
| **误触返回（大厅配置中返回）** | 轻微不适（配置丢失） | 返回主菜单是可恢复的，损失可预期（EC-7：草稿丢弃，这是明确行为） |

**情感设计目标**（art-bible §2 状态 A）：暖白光、棋盘缓慢呼吸 idle、玩家棋子"落座"弹性入场动画——让配置过程本身就有"马上要开一局了"的愉悦预期，而非冷冰冰的设置墙。

---

## 3. Navigation Position

```
[ 应用启动 ]
      ↓
[ 主菜单 (Main Menu) ]  ← 当前屏入口
      ↓ 「新游戏」点击
[ 大厅配置 (Lobby Config) ]  ← 当前屏内层
      ↓ 「开始游戏」（ConfigIsValid=true）
[ 游戏载入 → 对局 ]  → 本屏退场（移交回合 #2）

[ 大厅配置 ] ← 「返回」 → [ 主菜单 ]

[ 主菜单 ] → 「设置」 → [ 设置屏 (TBD) ]
[ 主菜单 ] → 「退出」 → 应用退出
[ 主菜单 ] → 「续局」 → 禁用占位 (MVP，OQ-Lobby-2)
[ 主菜单 ] → 「联机」 → 禁用占位 (MVP，OQ-Lobby-5)
```

**层级深度**：2 层（主菜单 → 大厅配置），扁平、无隐藏子菜单。ADR-0012 CommonUI 激活栈：`WBP_MainMenu` push `WBP_Lobby` on stack；大厅"返回"pop 回 `WBP_MainMenu`。

---

## 4. Entry & Exit Points

### 4.1 Entry Points

| 入口来源 | 触发条件 | 期望状态 |
|---|---|---|
| **应用启动（冷启动）** | 游戏二进制启动后，UMG 初始化完成 | 主菜单；手柄默认焦点=「新游戏」按钮（ADR-0012 FR-3） |
| **对局结束 → 返回主菜单** | GameOver 覆盖层玩家选择「返回主菜单」（P-12 终局覆盖层） | 主菜单；大厅配置重置为默认值（不保留上局配置，EC-7 逻辑） |
| **大厅配置中点「返回」** | 玩家在大厅配置屏点击「返回主菜单」 | 主菜单；大厅草稿丢弃（EC-7） |

### 4.2 Exit Points

| 出口 | 触发条件 | 目标屏 | 状态写入 |
|---|---|---|---|
| **开始游戏** | `ConfigIsValid=true` + 玩家点击「开始游戏」 | 游戏场景（对局，由回合 #2 接管） | `FGameSetupConfig` 传回合 #2（F-2 打包，见 Data Requirements §14）[architectural concern — 唯一写持久状态出口] |
| **进入大厅** | 主菜单「新游戏」点击 | 大厅配置层（屏内 push） | 无持久状态写入（大厅是纯配置草稿） |
| **进入设置** | 主菜单「设置」点击 | 设置屏（TBD，MVP 占位） | — |
| **退出** | 主菜单「退出」点击 → 二次确认 | 应用退出 | — |
| **返回主菜单（从大厅）** | 大厅「返回」点击 | 主菜单 | 大厅草稿丢弃，无写入 |

---

## 5. Layout Specification

### 5.1 Information Hierarchy

**主菜单层优先级**：

| Level | 内容 | 理由 |
|---|---|---|
| L1（最高） | 「新游戏」主 CTA 按钮 | 最高频动作，低摩擦开局核心路径（Pillar④） |
| L2 | 游戏标题/Logo | 品牌识别 |
| L3 | 「续局」「设置」「退出」次级入口 | 低频但必备；续局/联机 MVP 禁用灰化 |
| L4 | 棋盘背景 idle 动画、版本号 | 氛围承载，不承载操作信息 |

**大厅配置层优先级**：

| Level | 内容 | 理由 |
|---|---|---|
| L1 | 「开始游戏」主 CTA 按钮 + 配置合法性提示 | 目标终点，配置结束后第一眼看这里 |
| L2 | 席位列表（每席：名字/颜色/类型/AI 难度） | 核心配置区 |
| L3 | 玩家数增减控件（+ / -） | 频繁操作，但从属于席位列表 |
| L4 | 「返回主菜单」 | 退出路径，低频 |

### 5.2 Layout Zones

**主菜单屏** (1920×1080 基准，art-bible §7.1):

```
Zone A: 品牌区（上方居中）   — 游戏标题 Logo，视觉锚点
Zone B: 主 CTA 区（中央偏下）— 「新游戏」按钮，视觉重心
Zone C: 次级入口区（B 之下）  — 「续局」「设置」「退出」纵向排列
Zone D: 背景/氛围区（全屏）   — 棋盘 idle 动画、桌面背景（art-bible 状态 A）
Zone E: 版本/版权（左下角）   — L4 信息，最小字号底线
```

**大厅配置屏** (1920×1080 基准):

```
Zone A: 标题区（顶部）         — 「房间配置」标签 + 「返回」按钮（左上角）
Zone B: 玩家数控件区（Zone C 上方）— [−] [当前 P 数] [+] 三件套
Zone C: 席位列表区（中央主体）  — 每行一个席位卡片（2–4 行），可纵向滚动预留接口
Zone D: 「开始游戏」区（底部居中）— 主 CTA + 合法性提示文案（就近显示）
Zone E: 背景/氛围区（全屏）     — 同主菜单背景，状态 A
```

### 5.3 Component Inventory

**主菜单层组件**：

| 组件 | Widget 名 | 类型 | 状态 | 备注 |
|---|---|---|---|---|
| 游戏 Logo/标题 | `WBP_MainMenu_Logo` | Image/TextBlock | 常驻 | art-bible L1 字体 |
| 「新游戏」主按钮 | `WBP_Btn_NewGame` | `UCommonButtonBase` | 恒启用 | Fortune Gold 主 CTA；默认焦点（ADR-0012 FR-3） |
| 「续局」按钮 | `WBP_Btn_Continue` | `UCommonButtonBase` | MVP 禁用（OQ-Lobby-2） | 灰化 + 「即将推出」tooltip；provisional：若存档系统 #21 上线则按存档状态驱动 |
| 「设置」按钮 | `WBP_Btn_Settings` | `UCommonButtonBase` | 启用（占位） | 点击进设置屏（设置屏 spec TBD） |
| 「联机」按钮 | `WBP_Btn_Online` | `UCommonButtonBase` | MVP 禁用（OQ-Lobby-5） | 灰化 + 「即将推出」tooltip |
| 「退出」按钮 | `WBP_Btn_Quit` | `UCommonButtonBase` | 恒启用 | 点击→二次确认 modal（P-04） |
| 版本号 | `WBP_VersionLabel` | TextBlock | 常驻 | L4 最小字号；左下角 |

**大厅配置层组件**：

| 组件 | Widget 名 | 类型 | 状态 | 备注 |
|---|---|---|---|---|
| 「返回」按钮 | `WBP_Btn_BackToMenu` | `UCommonButtonBase` | 恒启用 | 点击草稿丢弃返回主菜单（EC-7） |
| 「−」减座位按钮 | `WBP_Btn_RemoveSeat` | `UCommonButtonBase` | P==P_min 时禁用 | P-17；边界禁用（GDD CR-3/EC-1） |
| 当前玩家数显示 | `WBP_SeatCountLabel` | TextBlock | 实时绑定 | 显示当前 P 值（2–4） |
| 「+」加座位按钮 | `WBP_Btn_AddSeat` | `UCommonButtonBase` | P==P_max 时禁用 | P-17；边界禁用（GDD CR-3/EC-2） |
| 席位卡片列表 | `WBP_SeatList` | VerticalBox/ListView | 动态 P 行 | 每行一个 `WBP_SeatCard` |
| 单席位卡片 | `WBP_SeatCard` | 复合 Widget | 有/无 AI 难度选择器 | 包含以下子组件 |
| &nbsp;&nbsp;└ 玩家名输入框 | `WBP_SeatCard_NameInput` | EditableTextBox | 最大 `NAME_MAX_LEN`=16 字符 | EC-4/5；达上限阻止输入；空名时提示 |
| &nbsp;&nbsp;└ 棋子色选择器 | `WBP_SeatCard_ColorPicker` | 8 色按钮组 | 已占用色置灰 | P-17；色块+P 编号双冗余（CA-01） |
| &nbsp;&nbsp;└ 人类/AI 切换 | `WBP_SeatCard_TypeToggle` | ToggleButton | 双态 | 切 AI 显难度选择器（CR-4） |
| &nbsp;&nbsp;└ AI 难度选择器 | `WBP_SeatCard_DifficultyPicker` | 3 按钮组（Casual/Normal/Sharp） | AI 时显示；人类时隐藏/折叠 | 默认 Normal（GDD CR-4） |
| 「开始游戏」主按钮 | `WBP_Btn_StartGame` | `UCommonButtonBase` | `ConfigIsValid` 驱动 | Fortune Gold；禁用时就近提示原因（CR-5） |
| 配置提示文案 | `WBP_ConfigErrorHint` | TextBlock | 非法时显示 | 就近于「开始游戏」按钮；L4 字号 |
| 退出二次确认 modal | `WBP_Modal_QuitConfirm` | `UCommonActivatableWidget` | 按需 push | P-04 确认/取消（见 Interaction Map） |

### 5.4 ASCII Wireframe

**主菜单屏**：

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│                  [Zone D: 棋盘 idle 背景 · 状态A]               │
│                                                                 │
│              ┌──────────────────────────────────┐              │
│              │   [Zone A] 骰子大亨 LOGO/标题      │              │
│              │          (L1, art-bible Level 1)  │              │
│              └──────────────────────────────────┘              │
│                                                                 │
│              ┌──────────────────────────────────┐              │
│              │  [Zone B] ★ 新游戏 ★  (主CTA金色) │  ← L1 焦点   │
│              └──────────────────────────────────┘              │
│              ┌──────────────────────────────────┐              │
│              │  [Zone C] 续局  (禁用·灰化·MVP)   │              │
│              ├──────────────────────────────────┤              │
│              │           设置                    │              │
│              ├──────────────────────────────────┤              │
│              │  联机  (禁用·灰化·MVP)             │              │
│              ├──────────────────────────────────┤              │
│              │           退出                    │              │
│              └──────────────────────────────────┘              │
│                                                                 │
│ [Zone E] v0.1.0                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**大厅配置屏**：

```
┌─────────────────────────────────────────────────────────────────┐
│ [Zone A] ← 返回                   房间配置                       │
│─────────────────────────────────────────────────────────────────│
│                  [Zone E: 棋盘 idle 背景 · 状态A]               │
│                                                                 │
│              [Zone B]  [−]  当前玩家数: 2  [+]                  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ [Zone C] 席位卡片 — 席位 0                               │   │
│  │  名字: [   Player 1          ]  色: [●][●][●][●][●][●][●][●] │
│  │  类型: [人类 ●] [AI  ○]                                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ [Zone C] 席位卡片 — 席位 1                               │   │
│  │  名字: [   Player 2          ]  色: [○][●][○][○][○][○][○][○] │
│  │  类型: [人类 ○] [AI  ●]  难度: [Casual][Normal●][Sharp]  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│              [Zone D] ┌──────────────────────────┐             │
│                       │   ▶ 开始游戏  (主CTA金色)  │             │
│                       └──────────────────────────┘             │
│                       [配置提示文案（错误时显示）]               │
└─────────────────────────────────────────────────────────────────┘
```

> 色选择器图例：`●` = 已选，`○` = 可选，`░` = 置灰不可选。AI 难度选择器仅在 `bIsAI=true` 时显示；默认选中 Normal。

---

## 6. States & Variants

### 6.1 主菜单屏状态

| 状态 ID | 名称 | 触发条件 | 视觉变化 |
|---|---|---|---|
| MM-S1 | 默认态 | 屏激活 | 全按钮正常；「续局」「联机」灰化（MVP 禁用） |
| MM-S2 | 退出确认 | 点击「退出」 | 退出确认 modal push（P-04）；主菜单按钮暗化 |
| MM-S3 | 设置屏激活 | 点击「设置」 | 设置屏 push（TBD；本屏待 spec） |

### 6.2 大厅配置屏状态

| 状态 ID | 名称 | 触发条件 | 视觉变化 |
|---|---|---|---|
| LB-S1 | 配置合法 | `ConfigIsValid=true` | 「开始游戏」启用（Fortune Gold），提示文案隐藏 |
| LB-S2 | 配置非法 | `ConfigIsValid=false` | 「开始游戏」禁用（灰化），就近提示原因文案（红色文字+警告图标） |
| LB-S2a | 颜色冲突 | C3 违反 | 提示："颜色重复，请重新选择"；冲突色选择器高亮边框警告 |
| LB-S2b | 名字为空 | C2 违反 | 提示："玩家名不能为空"；对应输入框显红色底线/边框 |
| LB-S2c | 名字超长 | C2 违反（输入层已阻断，F-1 二次防守） | 输入框达限制，阻止继续输入（截断在输入层 EC-5） |
| LB-S2d | AI 档不一致 | C4 违反（正常路径不发生，防守层） | 显"配置异常"兜底提示 |
| LB-S3 | 座位下界 | P==P_min=2 | 「−」按钮禁用 |
| LB-S4 | 座位上界 | P==P_max=4 | 「+」按钮禁用 |
| LB-S5 | 开局移交中 | 点击「开始游戏」且 `ConfigIsValid=true` 后 | 「开始游戏」按钮 loading 态（微短暂），等回合 #2 响应；若回合 #2 返回失败则恢复 LB-S2（EC-9） |
| LB-S6 | 回合 #2 拒绝 | `StartNewGame` 返回错误（EC-9） | 停留大厅，显示错误提示（如"配置被拒绝：玩家数超界"）；不切场景 |

### 6.3 席位卡片组件状态

| 状态 | 触发 | 视觉 |
|---|---|---|
| SC-S1 人类席 | `bIsAI=false` | AI 难度选择器隐藏/折叠 |
| SC-S2 AI 席 | `bIsAI=true` | AI 难度选择器展开（Casual/Normal/Sharp），默认 Normal 高亮 |
| SC-S3 名字焦点编辑 | 输入框获得焦点 | 边框高亮（焦点指示，art-bible §4.5 hover 语言衍生，需与 hover 可区分） |
| SC-S4 名字为空警告 | 输入框失焦后为空 | 输入框底线红色 + 旁边警告图标（非仅靠颜色：图标+文字同步） |
| SC-S5 色选择器：颜色已占用 | 其他席位已选该色 | 该色按钮灰化+不透明 50%（art-bible §4.5 禁用态）；不可点击 |
| SC-S6 席位落座入场 | 加座位/屏初始化 | 席位卡片弹性入场动画（art-bible 状态 A"落座"） |

---

## 7. Interaction Map

### 7.1 主菜单层交互

| 触发元素 | 输入方式 | 事件/动作 | 响应 | 模式引用 |
|---|---|---|---|---|
| 「新游戏」按钮 | 鼠标左键点击 / 手柄确认键 | `OnClicked` → `Handle_NewGame` | Push `WBP_Lobby`（CommonUI 栈），大厅配置初始化默认值 | P-13 菜单导航 |
| 「续局」按钮（禁用） | 鼠标左键点击 / 手柄确认键 | 按钮禁用，不响应点击；显"即将推出"tooltip | — | P-16 只读 Affordance；EC-8 |
| 「联机」按钮（禁用） | 同上 | 同上 | — | 同上 |
| 「设置」按钮 | 鼠标左键点击 / 手柄确认键 | `OnClicked` → `Handle_Settings` | Push 设置屏（TBD） | P-13 |
| 「退出」按钮 | 鼠标左键点击 / 手柄确认键 | `OnClicked` → Push 退出确认 modal | P-04 确认/取消 modal（见下） | P-04 确认/取消 |
| 退出确认 modal「确认」 | 点击 / 手柄确认键（`IA_Confirm`） | `OnClicked` → `UKismetSystemLibrary::QuitGame` | 应用退出 | P-04 |
| 退出确认 modal「取消」 | 点击 / Esc / 手柄取消键（`IA_Cancel`） | `OnClicked` → Pop modal | 返回主菜单 | P-04 |
| 键盘 Tab / 手柄方向键 | 键盘/手柄 | 焦点切换（CommonUI 导航） | 焦点按纵向列表顺序循环：新游戏 → 续局（跳过禁用，见 §12 焦点序） → 设置 → 退出 | ADR-0012 |

**平台变体**：
- **Keyboard/Mouse（Primary）**：鼠标 hover 显 scale +2%/亮度 +10%（art-bible §4.5）；点击触发。禁用按钮鼠标 hover 显"即将推出"tooltip（非 hover-only——禁用态通过颜色+不透明度已传达不可用）。
- **Gamepad（Partial）**：启动时默认焦点「新游戏」（`GetDesiredFocusTarget`，ADR-0012 FR-3）；方向键/左摇杆导航；Face Button Bottom 确认；Face Button Right / Esc 取消。**禁 hover-only**：所有状态（禁用、可用）在无 hover 的手柄模式下通过颜色+不透明度+图标即可读。

### 7.2 大厅配置层交互

| 触发元素 | 输入方式 | 事件/动作 | 响应 | 模式引用 |
|---|---|---|---|---|
| 「返回」按钮 | 鼠标左键点击 / 手柄取消键 | `OnClicked` → Pop `WBP_Lobby`，草稿丢弃 | 返回主菜单；`WBP_Lobby` 销毁（EC-7） | P-13 |
| 「+」加座位 | 点击 / 手柄确认 | `OnClicked` → `P += 1`（若 P < P_max）→ 新增 `WBP_SeatCard` 入场动画 | 新席位卡片弹性落座（art-bible 状态 A）；若 P==P_max 则「+」禁用 | P-17；GDD CR-3 |
| 「−」减座位 | 点击 / 手柄确认（P>P_min 时） | `OnClicked` → `P -= 1`（若 P > P_min）→ 末座位卡片退场 | 末席位卡片淡出退场；若 P==P_min 则「−」禁用 | P-17；GDD CR-3/EC-1 |
| 玩家名输入框 | 键盘输入 | `OnTextChanged` → 字符过滤（最大 16 字符）→ 实时更新 `NameOf(s)` | 超 16 字符阻止输入（EC-5）；更新 `ConfigIsValid` | P-17；GDD CR-2 |
| 玩家名输入框失焦 | Tab / 点击其他元素 / 手柄移焦 | `OnTextCommitted` → 验证非空（C2） | 若空：输入框红线+警告图标；更新「开始游戏」可用性 | P-17；GDD EC-4 |
| 棋子色按钮（可选色） | 点击 / 手柄确认 | `OnClicked(ColorIndex)` → 席位 s 选色，其余席位该色置灰 | 互斥更新全席位色可选集；`ConfigIsValid` 重评估 | P-17；GDD CR-3 |
| 棋子色按钮（置灰色） | 点击 / 手柄确认 | 按钮禁用，不响应（EC-3） | 无响应；tooltip"该颜色已被其他玩家使用" | P-16；GDD EC-3 |
| 人类/AI 切换 | 点击 / 手柄确认 | `OnToggled(bIsAI)` → 若切 AI：`AIDifficulty=Normal`，展开难度选择器；若切人类：`AIDifficulty=None`，折叠难度选择器（CR-4/AC-20） | 动态展/折 `WBP_SeatCard_DifficultyPicker`；`ConfigIsValid` 重评估 | P-17；GDD CR-4 |
| AI 难度按钮（Casual/Normal/Sharp） | 点击 / 手柄确认（`bIsAI=true` 时） | `OnClicked(EAIDifficulty)` → 更新席位 `AIDifficulty` | 选中项高亮；`ConfigIsValid` 重评估 | P-17；GDD CR-4 |
| 「开始游戏」（启用，`ConfigIsValid=true`） | 点击 / 手柄确认 | `OnClicked` → `BuildSetupConfig` → `StartNewGame(FGameSetupConfig&)` 调回合 #2 | 进入 LB-S5（loading 态）；回合 #2 接管成功则本屏退场；若失败则 LB-S6 | P-13；GDD CR-5/F-2 |
| 「开始游戏」（禁用，`ConfigIsValid=false`） | 点击（被禁用） | 不响应；提示文案已就近显示 | 无操作（EC-4/EC-3 等非法态下就近提示） | P-16；GDD CR-5 |
| 键盘 Tab / 手柄方向键 | 键盘/手柄 | CommonUI 焦点导航 | 按焦点序循环（见 §12 焦点序） | ADR-0012 |
| Esc / 手柄取消键 | 键盘/手柄（大厅内） | `IA_Cancel` → 同「返回」 | 草稿丢弃返回主菜单（EC-7） | P-04；ADR-0011 |

**禁 hover-only 硬约束（ADR-0011/0012 红线）**：全部上述交互均有 `OnClicked` / `IA_Confirm` 路径。tooltip 仅作辅助，不承载功能。

---

## 8. Events Fired

> 每个玩家动作对应游戏事件。标注 [Write] = 写持久状态，[architectural concern]。

| 玩家动作 | 事件名 | 接收方 | 状态写入 | 备注 |
|---|---|---|---|---|
| 点击「开始游戏」（合法配置） | `StartNewGame(FGameSetupConfig)` 调用 | 玩家与回合管理（#2） | [Write · **architectural concern**] `PlayerState` 列表初始化（写入归回合 #2，本屏仅构造传参） | F-2 打包；纯叶子不拥有运行态（GDD CR-1） |
| 加减座位 | 本屏内部：`P` 变更，UI 状态更新 | 无外部事件；仅驱动 `ConfigIsValid` 重评估 | [Read only，无持久写入] | — |
| 修改玩家名 | 本屏内部：`NameOf(s)` 变更 | 同上 | [Read only] | — |
| 切换棋子色 | 本屏内部：`ColorOf(s)` 变更 | 同上 | [Read only] | — |
| 切换人类/AI | 本屏内部：`bIsAI(s)` 变更 | 同上 | [Read only] | — |
| 切换 AI 难度 | 本屏内部：`AIDifficulty(s)` 变更 | 同上 | [Read only] | — |
| 点击「退出」→ 确认 | `UKismetSystemLibrary::QuitGame` | 系统 | [Write · **architectural concern**] 应用生命周期结束 | 二次确认 modal 防误触（P-04） |
| 点击「返回」（从大厅） | 本屏内部：Pop `WBP_Lobby` | CommonUI 栈 | [无写入]，草稿丢弃（EC-7） | — |

**继承义务（GDD §8 继承义务回链）**：
- `StartNewGame` 对应 GDD AC-13/AC-14/AC-15；事件名和 payload `FGameSetupConfig` 字段（含 `DisplayName:FText`，B-1 对齐）待 OQ-Lobby-1 闭合冻结。
- `ConfigIsValid=false` 时本屏**不发出** `StartNewGame`，防守层对齐 GDD F-1/CR-5。

---

## 9. Transitions & Animations

> 所有时长遵 art-bible §7.5 动效手感目标（弹性、有重量感、即时响应）。`reduce_motion=On` 降级规则见 §12 Accessibility 及 design/accessibility-requirements.md §5。

| 过渡 | 动效 | 时长 | 缓动曲线 | `reduce_motion=On` 降级 |
|---|---|---|---|---|
| 主菜单屏入场（冷启动/返回） | 整屏 Ease-Out 从上淡入+小幅下移（整体欢迎感） | 300ms | Ease-Out Cubic | 瞬现，无位移 |
| 按钮按下 | scale 0.95 | 80ms | Ease-In | 保留（无前庭风险，短暂缩放） |
| 按钮松开 | scale 1.0→1.05→1.0 | 150ms | Ease-Out Back | 去 overshoot（scale 1.0→1.0 线性） |
| 「新游戏」→ 大厅配置 push | 大厅 WBP 从右侧或向上 Ease-Out 滑入 | 250ms | Ease-Out Cubic | 瞬现 |
| 席位卡片"落座"入场（加座位） | Y 方向弹性落入，scale 0→1.05→1.0 | 250ms | Ease-Out Bounce | 瞬现，直接显示目标尺寸 |
| 席位卡片退场（减座位） | 淡出 + 轻微上移 | 150ms | Ease-In | 瞬隐 |
| AI 难度选择器展开/折叠 | 高度从 0 展开，内容淡入 | 200ms | Ease-Out Back | 瞬现/瞬隐 |
| 「返回」→ 主菜单 pop | 大厅 WBP 反向退出（与入场对称） | 200ms | Ease-In | 瞬切 |
| 「开始游戏」按钮 loading 态 | 按钮文字变「配置中…」+ 轻微 pulse | 持续至回合 #2 响应（预计 <500ms） | — | 去 pulse，静态文字 |
| 错误提示文案出现（非法配置） | 淡入，就近于「开始游戏」 | 200ms | Ease-Out | 瞬现 |
| 退出确认 modal 弹入 | Ease-Out Bounce | 200–300ms | Ease-Out Bounce | 瞬现 |
| 棋盘背景 idle（呼吸动画） | 棋盘缓慢旋转/缩放呼吸（art-bible 状态 A） | 循环 3–5s | Sine 缓动 | 静止（停止呼吸动画） |
| 玩家棋子落座（大厅初始化） | 弹性入场（art-bible 状态 A） | 每个棋子 0.2–0.3s，错开入场 | Ease-Out Bounce | 瞬现，直接目标位置 |

> [provisional] 动画时长为基于 art-bible §7.5 的初始估值，须 playtest 校准（`reduce_motion` 精确参数降幅由 OQ-IP-5 闭合）。

---

## 10. Data Requirements

> UI 是纯叶子只读（ADR-0007/0008 边界）。标 [Write] 的出口是架构关注点。

### 10.1 主菜单层

| 显示项 | Source System | 数据字段 | Read/Write | 备注 |
|---|---|---|---|---|
| 「续局」按钮可用性 | 存档系统（#21） | 存档槽是否有效 | Read | MVP=禁用（#21 GDD 未撰写，OQ-Lobby-2）；[provisional] |
| 游戏版本号 | 构建系统 | `GAME_VERSION` 常量 | Read | 仅展示，不互动 |

### 10.2 大厅配置层（草稿态，全为本屏内存状态，不持久化）

| 显示/编辑项 | Source | 字段 | Read/Write | 备注 |
|---|---|---|---|---|
| 当前玩家数 `P` | 本屏内部草稿 | `SeatCount` | Read+Write（本屏内） | 不持久化；配置草稿 |
| 每席玩家名 | 本屏内部草稿（setup 层 FString） | `NameOf(s)` | Read+Write（本屏内） | 边界 `FText::FromString` 转换在 `BuildSetupConfig`（F-2，B-1 GDD） |
| 每席棋子色 | 本屏内部草稿 | `ColorOf(s): EPlayerColor` | Read+Write（本屏内） | 枚举归回合 #2（GDD §6.1）；互斥集由本屏维护 |
| 每席类型（人类/AI） | 本屏内部草稿 | `bIsAI(s)` | Read+Write（本屏内） | — |
| 每席 AI 难度 | 本屏内部草稿 | `AIDifficulty(s): EAIDifficulty` | Read+Write（本屏内） | 枚举归回合 #2；值含义归 AI #10（GDD §6.1） |
| `ConfigIsValid` | 本屏实时计算（F-1 谓词） | 计算字段 | Read only（绑定按钮可用性） | 不暴露给外部；驱动「开始游戏」按钮态 |
| 颜色互斥可选集 | 本屏实时计算 | `AvailableColors(s)` | Read only | 由所有席位当前 `ColorOf` 集合派生 |

### 10.3 开局参数打包（唯一外部写出口）

| 字段 | 目标 | 类型 | Read/Write | 备注 |
|---|---|---|---|---|
| `FGameSetupConfig.PlayerCount` | 回合 #2 `StartNewGame` | int32 | **[Write · architectural concern]** | 写入由回合 #2 接管；本屏仅构造传参（GDD F-2/CR-1） |
| `FGameSetupConfig.Players[i].DisplayName` | 回合 #2 `PlayerState.DisplayName` | FText | **[Write · architectural concern]** | `FText::FromString` 边界转换（B-1 GDD；字段名 `DisplayName` 非 `PlayerName`，对齐 player-turn L79） |
| `FGameSetupConfig.Players[i].TokenColor` | 回合 #2 `PlayerState.TokenColor` | EPlayerColor | **[Write · architectural concern]** | — |
| `FGameSetupConfig.Players[i].bIsAI` | 回合 #2 `PlayerState.bIsAI` | bool | **[Write · architectural concern]** | — |
| `FGameSetupConfig.Players[i].AIDifficulty` | 回合 #2 `PlayerState.AIDifficulty` | EAIDifficulty | **[Write · architectural concern]** | — |

> [architectural concern 汇总] 本屏**唯一**的持久状态写出口是 `StartNewGame(FGameSetupConfig)` 调用，且实际写入由回合 #2 执行（本屏是调用方，不拥有任何运行态）。这与 GDD CR-1"纯叶子编排"边界完全对齐。

---

## 11. Accessibility

> 逐条对 WCAG-AA checklist（design/accessibility-requirements.md v0.1）。

### 11.1 键盘完整可达

**要求**（AA 2.1.1 + ADR-0011）：本屏全部可操作元素键盘可到达且可激活。

**焦点序（主菜单，Tab 顺序）**：
```
[新游戏] → [设置] → [退出]
（「续局」「联机」MVP 禁用；禁用元素手柄/Tab 默认跳过，不陷死）
```

**焦点序（大厅配置，Tab 顺序）**：
```
[返回] → [−] → [+]
  → 席位 0: [名字输入框] → [色·P1] → ... → [色·P8] → [人类/AI 切换] → [Casual/Normal/Sharp（若 AI）]
  → 席位 1: [名字输入框] → ... （同上，重复 P 次）
  → [开始游戏（合法时）]
```

> [provisional] 精确 Tab 顺序须 `/team-ui` 实现阶段与 ue-umg-specialist 协商 widget 树排列（OQ-IP-1）。焦点应**绕过**禁用的色块按钮（置灰色），不在禁用元素上停留。

### 11.2 手柄导航（Partial）

- 冷启动默认焦点：「新游戏」（`GetDesiredFocusTarget`，ADR-0012 FR-3）— 这是首屏，明确要求（任务 brief "GetDesiredFocusTarget 手柄默认焦点=「开始游戏」" 即指此"开始游戏"在主菜单层为「新游戏」，在大厅层为「开始游戏」）。
- 大厅配置屏默认焦点：第一个席位名字输入框（或「开始游戏」，待 OQ-IP-1 定）。
- Face Button Bottom（Gamepad South）= 确认；Face Button Right（Gamepad East）/ Esc = 取消/返回。
- 方向键/左摇杆在席位卡片列表内上下切换席位卡片行；在席位卡片内左右切换控件。
- 禁用按钮（「续局」「联机」置灰，「−」在 P_min，「+」在 P_max，禁用色块）：手柄导航跳过禁用元素，不陷死。

### 11.3 对比度

| 元素 | 背景 | 最低要求 | 验证状态 |
|---|---|---|---|
| 主菜单按钮文字（Dark Ink #1A1A2E，L3） | 按钮背景（Board Cream 系） | ≥ 4.5:1 | [provisional — 须实测] |
| 「新游戏」按钮（Dark Ink 文字 on Fortune Gold #F4C430） | Fortune Gold | ≥ 4.5:1 | [provisional — Fortune Gold 偏亮，Dark Ink 对其名义约 9:1，须实测确认实现色] |
| 配置提示文案（L4，Dark Ink 60% 不透明度） | Off White / Board Cream | ≥ 4.5:1 | [provisional — 半透明叠加后须实测，见 accessibility-requirements AC-1 特别注意] |
| 禁用按钮文字（灰化 50% 不透明） | 背景 | AA 豁免（禁用态不要求对比度） | WCAG 1.4.3 注释：禁用状态不适用对比度要求 |
| 焦点指示器描边 | 按钮/背景 | ≥ 3:1（AA 1.4.11） | [provisional — 描边规格待 OQ-A11Y-4] |

### 11.4 颜色不单独承载信息

**棋子色选择器**（最高风险区）：
- 每个色块按钮**同时显示 P 编号文字标签**（如 "P1"）或色名（如 "红"），非仅靠色块（art-bible §5.2 + accessibility-requirements §6.1）。
- 置灰（已占用）的色块：去饱和+不透明度 50%+可选"已选"图标（CA-01）；色盲用户可通过饱和度明显降低+位置+编号辨识。
- 当前席位已选中的色块：色块+勾选图标（✓）双重标识，非仅靠高亮色。

**「续局」「联机」禁用态**：去饱和灰化+不透明度 50%+"即将推出"文字 tooltip（非 hover-only；见禁 hover-only）。

**配置错误提示**：红色警告图标（⚠）+文字描述，非仅靠红色文字（CA-04②）。

**「开始游戏」禁用态**：灰化+不透明度 50%+就近提示文案（非仅颜色）。

### 11.5 焦点指示器

- 所有可聚焦元素（按钮、输入框、色块、切换器、难度选择器）获得焦点时必须有**可见描边/高亮**（art-bible §4.5 hover 语言衍生，但须与 hover 态视觉可区分——如 hover=亮度+10%/scale 1.02，焦点=额外 2px 描边）。
- 焦点指示器对背景对比度 ≥ 3:1（AA 2.4.7）。
- 具体描边规格待 OQ-A11Y-4。

### 11.6 reduce-motion

| 动效 | `reduce_motion=Off`（默认） | `reduce_motion=On` |
|---|---|---|
| 棋盘背景呼吸 idle | 3–5s 缓慢循环呼吸 | 静止 |
| 席位卡片落座入场 | Ease-Out Bounce 弹性 | 瞬现，目标位置 |
| 屏间过渡（push/pop） | 200–300ms 滑入 | 瞬切 |
| AI 难度选择器展/折 | 200ms 展开 | 瞬现/瞬隐 |
| 按钮松开弹性 | scale overshoot 1.05 | 线性到 1.0，无 overshoot |
| 错误提示淡入 | 200ms 淡入 | 瞬现 |

`reduce_motion` 为全局单一布尔（shared with HUD/VFX19/property-card，accessibility-requirements §5.1）。

### 11.7 最小字号

全部文字 ≥ 10px @1080p（art-bible §7.2 红线）：
- 版本号（Zone E）：10px（L4 下限）。
- 配置提示文案：10–12px（L4）。
- 色块 P 编号标签：≥ 10px（art-bible §5.2 P 编号标签说明）。
- AI 难度按钮文字（Casual/Normal/Sharp）：≥ 12px（L3）。
- 其余按钮、席位名：14px+（L3）。

---

## 12. Localization Considerations

| 考量 | 规则 |
|---|---|
| **玩家名输入**（`NameOf(s)` FString） | `NAME_MAX_LEN=16` 是**字符数**红线（GDD F-1 C2）。中文等 CJK 字符 1 字符=1 单位计，但渲染宽度约为拉丁字母 2 倍。HUD/大厅布局须在 16 字 CJK 场景下测试（最坏宽度约=16 CJK ≈ 32 拉丁字符宽），确认不溢出席位卡片名字区。[provisional — 当前 L10n 需求未定，若有中文本地化须复核布局] |
| **按钮文案**（「新游戏」「续局」「开始游戏」等） | 预留文字扩展空间：德语/法语等拉丁语言文案长度可达中文 1.5–2 倍。按钮宽度应弹性自适应（art-bible §3.4 圆角矩形可拉伸），或设定最大字符宽度上限。 |
| **配置提示文案**（错误信息） | 多语言下提示文案可变长，就近显示区域（「开始游戏」按钮下方）须预留至少 2 行（约 24px × 2）高度，避免单语文案恰好 1 行、其他语言 2 行时布局塌陷。 |
| **AI 难度名（Casual/Normal/Sharp）** | 三档选择器按钮宽度须能容纳最长本地化字符串（中文"休闲/普通/锋利"相对短；俄语可能较长）。建议固定三按钮宽度等分席位卡片宽。 |
| **棋子色名标签**（色块 P 编号/色名） | 若使用色名（"红色"/"蓝色"）作为非颜色冗余，须 L10n 翻译表；若仅用"P1"/"P2"等编号则无 L10n 压力（推荐 MVP 用 P 编号，色名作 tooltip 备选）。 |
| **RTL（右到左布局）** | MVP 无 RTL 需求（Pillar 未提）。若 Arabic/Hebrew 本地化，席位卡片镜像布局须架构支持。登记为 Full Vision OQ。 |

---

## 13. Acceptance Criteria

> 每条可测，QA 不读其他文档即可验。类型标注与 GDD §8 一致（[Logic] / [Advisory·code-review]）。

- [ ] **AC-Screen-1 [Logic·性能]** GIVEN 游戏冷启动，WHEN 主菜单屏 UMG 初始化完成，THEN 首帧交互可达时间（从进程启动到「新游戏」按钮可点击） ≤ 5s @目标硬件（60 FPS 目标，16.6ms 帧预算）；「新游戏」作为默认焦点元素可被手柄确认键激活。FAIL 条件：> 5s 或默认焦点未落在「新游戏」。

- [ ] **AC-Screen-2 [Logic·路由导航]** GIVEN 主菜单屏已显示，WHEN 点击「新游戏」，THEN 大厅配置屏 push（CommonUI 栈），默认席位数=2，席位 0 类型=人类，席位 1 类型可为人类或 AI（按 `DefaultAIDifficulty=Normal`）；WHEN 大厅配置屏中点击「返回」，THEN 返回主菜单且大厅草稿丢弃（下次重新进入为默认值）。FAIL 条件：push/pop 后屏状态不正确，或草稿未丢弃。

- [ ] **AC-Screen-3 [Logic·错误/空态]** GIVEN 大厅配置屏，WHEN 玩家清空任一席位名字（`NameOf=""`），THEN 「开始游戏」按钮禁用，就近显示提示文案"玩家名不能为空"（或等效中文），且**不**自动填入默认名；WHEN 玩家重新输入非空名字，THEN 提示消失，「开始游戏」在其他条件满足时重新启用。FAIL 条件：空名时按钮未禁用，或提示未出现，或自动填名。

- [ ] **AC-Screen-4 [Logic·A11y WCAG-AA]** GIVEN 游戏配置 `reduce_motion=Off`，WHEN 以 Coblis Deuteranopia 模拟运行大厅配置屏截图，THEN 以下元素仅凭非颜色信号（图标+文字+纹理）即可区分：① 棋子色块已占用（置灰）vs 可选；② 人类 vs AI 切换当前选中态；③ 「开始游戏」禁用 vs 启用态；④ 配置错误提示（⚠ 图标+文字）。FAIL 条件：任一项在 Deuteranopia 模拟下仅靠颜色差异可辨（无图标/文字冗余）。

- [ ] **AC-Screen-5 [Logic·核心目的]** GIVEN 大厅配置屏有 2 个启用席位，席位 0 为人类"Alice"红色，席位 1 为 AI "Bot" 蓝色 Normal 难度（所有字段合法），WHEN 点击「开始游戏」，THEN `StartNewGame` 被调用，入参 `FGameSetupConfig.PlayerCount=2`、`Players[0].DisplayName=FText("Alice")` / `TokenColor=红` / `bIsAI=false` / `AIDifficulty=None`、`Players[1].DisplayName=FText("Bot")` / `TokenColor=蓝` / `bIsAI=true` / `AIDifficulty=Normal`；本屏退场（不再处于激活栈顶）。FAIL 条件：`StartNewGame` 入参字段任一不匹配，或本屏未退场（对齐 GDD AC-13 + CR-1 退场契约）。

- [ ] **AC-Screen-6 [Logic·占位入口]** GIVEN 主菜单屏（MVP），WHEN 检查「续局」「联机」按钮状态，THEN 两者均处于禁用/灰化状态（不可激活）；WHEN 手柄或键盘 Tab 导航到这两个按钮位置，THEN 焦点**跳过**或在这两个按钮上激活无效（不进入任何未实现流程）。FAIL 条件：MVP 下这两个入口可激活进入任何游戏流程。

- [ ] **AC-Screen-7 [Advisory·code-review]** 大厅配置屏中，棋子色选择器的每个色块按钮**同时**呈现颜色色块 AND 非颜色标识（P 编号或色名标签）；置灰的已占用色块在非颜色通道（去饱和+位置+标签）与可选色块可区分。归 UX walkthrough + Coblis 模拟验证。

---

## 14. Open Questions

| ID | 问题 | 归属/触发阶段 |
|---|---|---|
| **OQ-Screen-1（player journey）** | player journey map 未建立；本 spec §2 玩家情绪状态为 provisional 推断。模板待建：`.claude/docs/templates/player-journey.md`。player journey 建立后须回核 §2 并更新 provisional 标注。 | ux-designer / Pre-Production |
| **OQ-Screen-2（GDD OQ-Lobby-1 闭合）** | `StartNewGame` 入口签名和 `FGameSetupConfig`/`FPlayerSetupEntry` USTRUCT 归属（owner=回合 #2）尚待 producer propagate 确认（GDD OQ-Lobby-1）。本 spec §8 Events Fired、§13 AC-Screen-5 中接口名/字段名（尤其 `DisplayName:FText`）依赖此 OQ 闭合后冻结。 | producer / 设计冻结前 |
| **OQ-Screen-3（续局 UI 触点）** | 「续局」按钮在 MVP 禁用。存档系统（#21）GDD 撰写完成后，本屏须补充：① 存档检测逻辑（何时亮「续局」）；② 单槽覆盖确认 modal（「确续局会覆盖当前存档，确认？」）；③ 「继续游戏」读档流程 UX 触点。暂停菜单「保存并退出」flow spec 亦在此范围（Polish 阶段 pause/save flow spec 补完）。建议届时撰写独立子 spec 或扩展本 spec §4/§6/§8。 | #21 设计阶段 / Polish |
| **OQ-Screen-4（焦点序精确定义）** | §12 焦点序为初始设计，大厅配置屏内色块组（8个颜色按钮）和多席位的精确 Tab/手柄方向键序须 ue-umg-specialist 验证 widget 树可行性。特别是：席位卡片内 8 个色块的手柄左右导航——是否需要色块组作为子 focus scope（CommonUI FocusGroup）？建议 Pre-Production 原型验证（OQ-IP-1 同源）。 | ux-designer + ue-umg-specialist / Pre-Production |
| **OQ-Screen-5（文本缩放+布局）** | `text_scale=150%` 下大厅配置屏席位卡片布局可行性（席位名 16 CJK 字符 × 1.5x = 约 48 字节宽度）。可能触发卡片宽度溢出或需要卡片内容折行设计。须 Pre-Production `/ux-design` 二轮验证（accessibility-requirements OQ-A11Y-3 同源）。 | ux-designer / Pre-Production |
| **OQ-Screen-6（颜色选择器 P 编号 vs 色名）** | §11.4 建议 MVP 色块用 P 编号（P1–P8）作为非颜色冗余；GDD OQ-Lobby-7 待确认 `EPlayerColor` 枚举与 art-bible §5.2 P1–P8 颜色的一一对应关系。两 OQ 共同门控颜色选择器最终 a11y 实现。 | producer / 设计冻结前（与 GDD OQ-Lobby-7 同批） |
| **OQ-Screen-7（`EPlayerColor` 色名 L10n）** | 若最终决策用色名（"红色"/"蓝色"）作为颜色按钮文字标签（而非 P 编号），须建立色名 L10n 字符串表。P 编号方案无此负担，推荐 MVP 优先。 | ux-designer + L10n（若需） / 本地化计划阶段 |
| **OQ-Screen-8（退出确认 modal 文案）** | 「退出」二次确认 modal 文案（按钮文字「确认退出」vs「是」等）须产品/写手确认。本 spec 仅定义 modal 存在和 P-04 行为，不定文案。 | producer / Pre-Production |
| **OQ-Screen-9（设置屏 spec）** | 「设置」入口目标屏 UX spec 尚未撰写（本屏 exit point 之一）。MVP 设置屏含：音量/分辨率/`reduce_motion`/`text_scale`/`colorblind_filter`（占位）。建议独立 spec `design/ux/settings.md`。 | ux-designer / Pre-Production |
| **OQ-Screen-10（「开始游戏」loading 态最大等待时间）** | LB-S5 中「开始游戏」loading 态持续至回合 #2 响应。若回合 #2 初始化超时（预计 <500ms，但无硬性上界），loading 态无限等待将造成 UI 死锁。须定义超时阈值（如 3s）+ 超时后降级行为（回到 LB-S2 + 错误提示）。与回合 #2 接口定义同批闭合（OQ-Lobby-1）。 | ux-designer + unreal-specialist / Pre-Production |

---

*骰子大亨 UX Spec — 主菜单与大厅配置屏 v0.1 — 2026-06-06*
*Author: ux-designer agent*
*Upstream: design/gdd/main-menu-lobby.md (Approved) · design/art/art-bible.md · design/ux/interaction-patterns.md · design/accessibility-requirements.md*
