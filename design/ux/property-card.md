---
title: "地产卡 UI — UX Spec"
screen: property-card
status: In Design
last_updated: 2026-06-06
template: UX Spec
author: ux-designer
upstream_gdd: design/gdd/property-card-ui.md (APPROVED R-2)
---

# UX Spec — 地产卡 UI（Property Card & Tile Status Overlay）

> **Scope**: 产权契据卡详情面板（Modal）+ 棋盘 tile 状态常驻叠加。
> **Pattern References**: P-06 Modal Card Activation / P-15 Persistent Tile Status Overlay / P-04 Confirm/Cancel / P-05 Hold-to-Build / P-16 Read-Only Affordance Buttons
> **ADR Anchors**: ADR-0011 Enhanced Input · ADR-0012 CommonUI 激活栈 · ADR-0007 BP vs C++ 边界 · ADR-0008 HUD 驱动 + IGameClock DI
> **Accessibility Baseline**: WCAG-AA（design/accessibility-requirements.md）

---

## 1. Purpose & Player Need

**目的（系统层）**

地产卡 UI（#17）是《骰子大亨》的产权信息中枢。它把棋盘(1)、地产所有权(6)、建房升级(8)三源的只读数据翻译成玩家一眼可读的**产权契据卡**，并为建房/卖房/抵押/赎回意图提供转发入口。作为呈现层纯叶子，#17 只读取/订阅数据、只显示与转发玩家意图，从不写游戏状态。

**玩家需求（体验层）**

玩家在棋盘上反复面临以下决策点：
- "这块地收多少租？再建一栋要多少？"
- "我手头这块地抵押能套现多少，赎回要还多少？"
- "对手垄断了吗？那块地有没有抵押缺钱？"

没有产权卡，这些决策退化为盲猜。产权卡就是大富翁桌面上那张**产权契据**——翻开它，底牌透明，决策不再盲猜（Player Fantasy：「翻开底牌——这块地值多少、能榨多少、押多少，我一眼看穿。」）。

---

## 2. Player Context on Arrival

> **[provisional]** player-journey.md 尚未建立（见 Open Questions OQ-UX-PC-1），以下玩家情绪状态据 GDD Player Fantasy 推断，标 provisional。

| 入口路径 | 玩家前序动作 | 情绪状态（provisional） | 认知负荷 |
|----------|--------------|------------------------|---------|
| 棋子落到可购地块 → 自动打开 | 刚掷完骰、移棋，等待买地决策 | 兴奋/紧张（是否买）→ 需要快速读取价格和租金阶梯 | 高（正在做花钱决策） |
| 点击棋盘 tile（任意玩家回合） | 主动查探对手/自己地产 | 侦察/算计（「对手垄断了吗？」）→ 需要快速扫垄断+抵押状态 | 中（主动查阅，非决策压力） |
| HUD 底部「地产卡快览」入口 | 自己回合操作期间主动打开 | 策略确认（「我的这块地再建一栋划算吗？」）→ 需要建后租金预览+卖回额 | 中高（正在做建房/抵押决策） |

**核心挑战**：信息密度与易读性的平衡——租金阶梯（6 行）+ 建房区块 + 抵押区块 + 状态视觉在一张卡片上，休闲玩家可能被淹没。设计解法：**永远直读**核心三项（地块名 / 购买价 / 当前生效租金行），次要信息（费率算式推导）可小字号呈现。

---

## 3. Navigation Position

```
主菜单/大厅 (WBP_MainMenu)
    └── 棋盘对局 (WBP_GameBoard)
            ├── HUD (WBP_HUD)   ← 常驻，拥玩家面板/现金/回合/操作栏
            │       └── 地产卡快览入口（转发 OpenCard + TileIndex）
            ├── 棋盘 tile 状态叠加 (WBP_TileStatusOverlay)   ← 常驻，独立于卡片开关
            └── 地产卡详情面板 (WBP_PropertyCard)   ← 按需，CommonUI 激活栈栈顶
                    ├── 产权契据内容（Property / Railroad / Utility 三路）
                    └── 动作按钮组（买地 / 建房 / 卖房 / 抵押 / 赎回）
```

**CommonUI 激活栈位置**：`WBP_PropertyCard : UCommonActivatableWidget`，push 时调 `SwitchToModal()`，激活 `IMC_Modal`（Priority=20），截获游戏操作输入。Pop 时调 `ReturnFromModal()`，恢复下层 `IMC_Gameplay`。（P-06 Modal Card Activation）

---

## 4. Entry & Exit Points

### 入口

| 入口来源 | 触发机制 | 传入数据 | 优先级 |
|----------|----------|---------|--------|
| 点击棋盘 tile | `IMC_Gameplay` → `IA_SelectTile`(Digital, Started) → 命中 TileIndex | `TileIndex` | 主路径 |
| HUD 底部地产卡快览入口 | HUD `UCommonButtonBase` 点击 → `OpenCard(TileIndex)` | 当前/选定 `TileIndex` | 主路径 |
| 玩家回合落无主地产自动触发 | 回合2 ResolvePhase 调 `OpenCard` | 落地 `TileIndex` | 系统路径 |
| 手柄：棋盘焦点导航至 tile + 确认 | `IMC_Gameplay` + CommonUI 焦点链 + `IA_Confirm` | `TileIndex` | 手柄路径 |

**入口规则**（GDD CR-1/CR-2）：
- `TileType ∈ {Property, Railroad, Utility}` → 状态机 Closed→Open，读三源渲染
- `TileType ∉ 三类（Tax/Jail/Go/Chance/CommunityChest/四角）` → 不开卡，静默忽略（EC-4）
- 越界/无效 `TileIndex` → 不开卡，记告警（EC-3）
- Open 态下 `OpenCard(其他三类)` → 切换内容，不堆叠（单卡不变式）

### 出口

| 出口方式 | 触发 | 下一状态 | 备注 |
|----------|------|---------|------|
| 关闭按钮 | 点击 ✕ 按钮 | 返回棋盘 / HUD 焦点 | `IA_Cancel` 兼容 |
| 点击面板外空白 | 鼠标点击卡片范围外 | 返回棋盘 | GDD CR-11 |
| Esc 键 / 手柄 Face Right | `IA_Cancel`（`IMC_Modal`） | 返回棋盘 | P-04 |
| 执行意图后自动关闭 | 买地/抵押/赎回意图转发成功 | 返回棋盘 | [provisional] 仅买地自动关闭，建房保持 Open 供连续操作；待确认（OQ-UX-PC-2） |

**退出时**：释放卡片内容订阅引用；棋盘 tile 状态叠加订阅**保留**（独立于卡片开关，GDD 状态机不变式②）。

---

## 5. Layout Specification

### 5.1 Information Hierarchy（信息优先级）

| 层级 | 信息项 | 优先级 | 呈现策略 |
|------|--------|--------|---------|
| **L1 永远直读** | 地块名 / 购买价 / 当前生效租金行 | 核心决策信息 | 正常字号，高亮，无需操作可见 |
| **L2 次要直读** | 完整租金阶梯（全 6/4 行） / 归属徽章 / 建筑图标行 | 策略深算信息 | 正常字号，非高亮行稍暗 |
| **L3 操作区** | 动作按钮组（买地/建房/卖房/抵押/赎回） + 建房区块（卖回额+建后预览） | 行动触发信息 | 按钮明显，建房区块紧邻按钮 |
| **L4 状态标记** | 「已抵押」标签 / ×2 垄断徽章 / 「数据异常」占位 | 状态警示信息 | 突出但不遮核心 |
| **L5 次要文字** | 抵押区块（套现额+赎回价）/ 门控原因 tooltip / Utility 倍率说明文案 | 辅助核算信息 | Level 4（10–12px）|

**信息架构原则**（GDD UI Requirements §信息架构）：
- 可次级的是费率**算式/推导**，赎回价**数字本身**属核心必显（EC-13/AC-42 强制，不可折叠）
- 卡片不压盖棋盘中央操作区（art-bible §7.1 ≥65% 留棋盘）

### 5.2 Layout Zones

```
┌─────────────────────────────────────────┐
│  ZONE A: 色组色条（卡高 22%，饱和 ≥70%）   │
│  [地块名 Level3]        [垄断完成角标]     │
│  [购买价 Level2 金色]                     │
├─────────────────────────────────────────┤
│  ZONE B: 租金表区                         │
│  无房    ¥XX         ← (Property 6行)    │
│  1栋     ¥XX  ← 高亮行（生效行）           │
│  2栋     ¥XX                             │
│  3栋     ¥XX                             │
│  4栋     ¥XX                             │
│  酒店    ¥XX                             │
│  (Railroad 4行 / Utility 2倍率行)         │
├─────────────────────────────────────────┤
│  ZONE C: 建房区块（Property 专属）          │
│  每栋造价 ¥XX  卖回额 ¥XX  建后租金→¥XX   │
├─────────────────────────────────────────┤
│  ZONE D: 抵押区块                         │
│  抵押可得 ¥XX    赎回需付 ¥XX             │
├─────────────────────────────────────────┤
│  ZONE E: 动作按钮组                       │
│  [买地] [建房▼] [卖房] [抵押] [赎回]       │
│  [门控原因 tooltip 占位]                   │
├─────────────────────────────────────────┤
│  [所有权徽章 右下] [建筑图标行 左下]  [✕]  │
└─────────────────────────────────────────┘
```

> Railroad 无 ZONE C（无建房）；Utility 的 ZONE B 替换为倍率区块（2 行 ×4/×10）+ 固定文案「实租 = 骰点 × 倍率」。

### 5.3 Component Inventory

| 组件 | Widget 候选 | 数据绑定 | 来源 |
|------|-------------|---------|------|
| 色组色条 | `UImage`（Color fill） | `ColorGroup → §4.4 色值` | #1 `GetTileData` |
| 地块名 | `UTextBlock` Level3 14–18px SemiBold | `DisplayName` | #1 |
| 购买价 | `UTextBlock` Level2 20–28px Fortune Gold | `PurchasePrice` | #1 |
| 租金表（6/4 行） | `UVerticalBox` × `URentRowWidget` | `RentTable[0..N]` | #1 |
| 高亮行背景 | `UImage` 色组色 30% 叠加 + 左 4px 竖线 | `F1_highlight_row` | F-1 推导 |
| ×2 垄断徽章 | `UTextBlock` + 圆角矩形背景 | `show_monopoly_badge` | F-1(a) |
| 建房区块（卖回额+建后预览） | `UHorizontalBox` Level4 | `GetSellback` / `RentTable[hc+1]` | #8 / #1 |
| 抵押区块 | `UHorizontalBox` Level4，**双值必显** | `MortgageValue` / `GetUnmortgageCostForDisplay(MV)` | #1 / economy5 |
| 所有权徽章 | `UImage`（圆形，玩家色）+ `UTextBlock` P编号 | `GetOwner → 玩家色+编号` | #6 |
| 建筑图标行 | `UHorizontalBox` × 建筑图标 | `GetHouseCount` | #8 |
| 「已抵押」标签 | `UTextBlock` Level3 SemiBold | `IsMortgaged` | #6 |
| 抵押斜纹遮罩 | `UImage`（斜纹 Brush，HSV×0.4） | `IsMortgaged` | #6 |
| 垄断完成角标 | 圆角矩形 Fortune Gold + 「全」/★ | `IsMonopoly ∧ ¬is_mortgaged` | #6 |
| 动作按钮组 | `UCommonButtonBase` × 5 | affordance 读 #6/#8/现金 | P-16 |
| 门控原因 tooltip | `UToolTipWidget`（可聚焦非 hover-only） | `CanBuildHouse 原因码` | #8 |
| 关闭按钮 | `UCommonButtonBase` ✕ | `IA_Cancel` 触发 | P-04 |
| 「数据异常」占位 | `UTextBlock` Level4（条件可见） | EC-6/G-0b 触发条件 | F-1 G-0 |
| Utility 倍率区块 | `UVerticalBox` × 2行 + 固定文案 | `DiceMultiplierTable[0..1]` | #1 |
| 「实租=骰点×倍率」文案 | `UTextBlock` Level4（**不可省略**） | 静态文本 | CR-5/EC-10 |

### 5.4 ASCII Wireframe

**Property 契据卡（1080p 参考，宽约 360px，高约 580px）**

```
┌──────────────────────────────────────────┐
│ ████████████████ 色组色条 ████████████████ │  ← 22% 卡高，饱和≥70%
│ ████████████████████████████████████████ │
├──────────────────────────────────────────┤
│ 台北路                          [全★]    │  ← 地块名 L3 + 垄断角标（条件）
│ 购买价  ¥240                             │  ← L2 Fortune Gold
├──────────────────────────────────────────┤
│ 租金                                      │
│   无房        ¥ 20                        │
│ ▶ 1栋         ¥ 100  ←━━━ 高亮行（竖线+背景块+SemiBold）
│   2栋         ¥ 300                       │
│   3栋         ¥ 900                       │
│   4栋         ¥ 1100                      │
│   酒店        ¥ 1275                      │
├──────────────────────────────────────────┤
│ 建房  每栋¥200  卖回¥100  建后→¥300       │  ← L4，三者并列
├──────────────────────────────────────────┤
│ 抵押可得 ¥120    赎回需付 ¥132            │  ← L4，两值必显
├──────────────────────────────────────────┤
│  [买地]  [建房▼]  [卖房]  [抵押]  [赎回] │  ← 动作按钮（affordance 态）
├──────────────────────────────────────────┤
│ 🏠🏠  P2 ●                          [✕] │  ← 建筑图标行 + 所有权徽章 + 关闭
└──────────────────────────────────────────┘

图例：▶=高亮竖线  ●=玩家色徽章  [全★]=垄断完成角标（条件）
抵押态：整卡去饱和 60%+斜纹覆盖，「已抵押」标签替换购买价下方
数据异常态：所有高亮清空，顶部「⚠ 数据异常」占位标签
```

**Railroad 契据卡（差异部分）**

```
│ 租金（站数）                              │
│   拥1站       ¥ 25                       │
│ ▶ 拥2站       ¥ 50  ←━━━ 高亮行
│   拥3站       ¥ 100                      │
│   拥4站       ¥ 200                      │
│ [无建房区块，无建房按钮]                   │
```

**Utility 契据卡（差异部分）**

```
│ 倍率                                     │
│   拥1个       ×4                         │
│ ▶ 拥2个       ×10  ←━━━ 高亮行
│                                           │
│ 实租 = 骰点合计 × 倍率  ← 不可省略文案    │
│ [无建房区块]                              │
```

---

## 6. States & Variants

### 卡片面板状态机（6 态）

| 状态 | 触发条件 | 视觉表现 | 交互可达性 |
|------|---------|---------|-----------|
| **Closed** | 初始 / CloseCard / Esc | 无卡片面板（tile 叠加常驻） | 棋盘全可交互 |
| **Open — 正常态** | `OpenCard(三类 TileIndex)`，三源全读 | 完整卡片，高亮生效行 | 动作按钮按 affordance 启/禁 |
| **Open — 抵押态** | `IsMortgaged==true` | 整卡去饱和 60%+斜纹+「已抵押」标签；`highlight_row==-1`；租金行灰显仍可读；抵押按钮禁用 | 赎回按钮按现金判启/禁 |
| **Open — 垄断完成态** | `IsMonopoly==true ∧ ¬IsMortgaged` | 色组环圈描边 3px；无房时显×2 徽章 + 高亮 row0；有房时高亮 `hc` 行无徽章 | 同正常态 |
| **Open — 数据异常态** | G-0b：`owner==NONE ∧ (hc>0 ∨ IsMonopoly)` | `highlight_row==-1`；`show_monopoly_badge=false`；「⚠ 数据异常」占位；其余字段原样 | 动作按钮全禁用 |
| **Open — 降级态** | EC-2：三源某源读取失败 | 缺失字段显「—」，其余正常；不崩溃；记告警 | 可用字段正常，缺失字段对应按钮禁用 |

### 动作按钮组 Affordance 变体（P-16 只读 Affordance）

| 按钮 | 启用条件 | 禁用原因（tooltip 显示） |
|------|---------|------------------------|
| 买地 | `owner==NONE ∧ 当前玩家回合 RollPhase 后` | 「非购买时机」/ 「已有归属」 |
| 建房 | `owner==当前玩家 ∧ IsMonopoly ∧ ¬IsMortgaged ∧ CanBuildHouse` | 四原因码（P-05 门控原因）：非垄断/组内有抵押/非最低档需补齐/现金不足 |
| 卖房 | `owner==当前玩家 ∧ GetHouseCount>0 ∧ CanSellHouse` | 「无建筑可卖」 |
| 抵押 | `owner==当前玩家 ∧ ¬IsMortgaged ∧ GetHouseCount==0` | 「有建筑须先卖房」/ 「已抵押」 |
| 赎回 | `owner==当前玩家 ∧ IsMortgaged ∧ 现金≥GetUnmortgageCostForDisplay(MV)` | 「现金不足」 |

**对非 owner**：全部动作按钮禁用（可看租金阶梯，不可操作，GDD CR-7/AC-19）。

### 棋盘 tile 叠加变体（P-15，常驻）

| 状态 | 视觉（tile 上） | 优先级 |
|------|----------------|--------|
| 正常 | 无叠加 | — |
| 抵押 | 整格去饱和 60%+对角斜纹 | 高（覆盖垄断） |
| 垄断完成 | 2px 色组环圈+Fortune Gold 完成圆点（★） | 低 |
| 抵押+垄断 | 斜纹+环圈共存（EC-8，环圈随去饱和降饱和度保轮廓） | — |

---

## 7. Interaction Map

### 7.1 Keyboard / Mouse 路径（主路径）

| 用户动作 | 输入 | 组件响应 | 意图转发 / 事件 | 视听反馈 |
|----------|------|---------|----------------|---------|
| 点击棋盘 tile | 鼠标左键 / `IA_SelectTile` | `OpenCard(TileIndex)` → 三源读取 → 渲染 | — | 卡牌翻转入场 250ms / 翻牌音 A-01 |
| 点击 HUD 快览入口 | 鼠标左键 | HUD 转发 `OpenCard(当前 TileIndex)` | — | 同上 |
| 点击关闭按钮 ✕ | 鼠标左键 / `IA_Cancel` | `CloseCard` → Stack.RemoveWidget | — | 关闭收叠音 A-02 |
| 点击面板外空白 | 鼠标左键（卡片范围外） | `IA_Cancel` 触发 `CloseCard` | — | 同上 |
| Esc 键 | `IA_Cancel` (`IMC_Modal` P=20) | `CloseCard` | — | — |
| 点击「买地」 | 鼠标左键（启用态） | 转发买地意图→回合2 ResolvePhase | 买地意图 × 1 次 | A-03 买地确认音 |
| 按住「建房」0.5s | 鼠标左键 Hold (`IA_BuildHouse`, Hold 0.5s) | 转发建房意图→回合2 | 建房意图 × 1 次 | 见 P-05；VFX V-Enable-01 |
| 点击「卖房」 | 鼠标左键（启用态） | 转发卖房意图→回合2 | 卖房意图 × 1 次 | — |
| 点击「抵押」 | 鼠标左键（启用态） | 转发抵押意图→回合2 | 抵押意图 × 1 次 | A-05 低沉音 + 整卡灰化 |
| 点击「赎回」 | 鼠标左键（启用态） | 转发赎回意图→回合2 | 赎回意图 × 1 次 | A-06 赎回 chime |
| 建房按钮禁用时查看原因 | 鼠标左键点击禁用按钮（或聚焦） | tooltip 显示门控原因码 | — | — |
| 点击非三类 tile | 鼠标左键 | 不开卡，静默忽略 | — | — |

**禁止 hover-only**：门控原因 tooltip 必须在点击禁用按钮 OR 手柄聚焦时可读，hover 悬停可提前显示但**不能是唯一路径**（GDD CR-7/AC-40，P-16）。

### 7.2 Partial Gamepad 路径

| 用户动作 | 输入 | 注意事项 |
|----------|------|---------|
| 棋盘 tile 焦点导航 | 方向键 / 左摇杆 → 焦点到 tile | CommonUI `GetDesiredFocusTarget` 定 tile 初始焦点 |
| 打开地产卡 | Face Button Bottom（确认） | `IA_Confirm` → `OpenCard` |
| 卡内按钮导航 | 方向键 / 左摇杆 | CommonUI 焦点链，顺序：买地→建房→卖房→抵押→赎回→关闭 |
| 执行动作 | Face Button Bottom | 对应按钮激活（建房用 Hold Trigger 0.5s，P-05） |
| 查看门控原因 | 聚焦到禁用建房按钮时 tooltip 自动显示 | 非 hover-only 实现 |
| 关闭卡片 | Face Button Right / Back | `IA_Cancel` |

**焦点顺序**（GDD UI Requirements §输入要求 + ADR-0012）：
- 卡片激活时默认焦点=买地按钮（若可购）或首个可用按钮，否则=关闭按钮
- Tab 键 / 手柄方向键遍历：色组信息区（只读，可聚焦但无动作）→ 租金表行（只读）→ 建房/抵押区块（只读）→ 动作按钮组 → 关闭按钮
- 焦点指示器：3px 描边 + 亮度 +10%（art-bible §4.5 hover 规格基础上加描边，与 hover 态可区分，OQ-A11Y-4）

**Platform Variants 覆盖声明**：
- Keyboard/Mouse（主）：全路径覆盖，见 7.1
- Partial Gamepad：全功能可达（无鼠标专属死角），焦点链就绪；完整手柄手感打磨留 Pre-Production
- Touch：不支持（technical-preferences Touch=None）

---

## 8. Events Fired

| 用户动作 | 游戏事件 / 意图 | 持久状态写入？ | 备注 |
|----------|----------------|--------------|------|
| `OpenCard(TileIndex)` | —（UI 层读取事件，非游戏事件） | 否 | #17 不写状态 |
| 点击「买地」→ 转发回合2 | 回合2 → `OnOwnershipChanged`（最终） | **架构关注点**（#6 写状态） | #17 仅转发意图，实际写操作由 #6 执行 |
| 按住「建房」→ 转发回合2 | 回合2 → `OnBuildingChanged`（最终） | **架构关注点**（#8 写状态） | #17 仅转发意图；状态 Write 在 #8 |
| 点击「卖房」→ 转发回合2 | 回合2 → `OnBuildingChanged`（最终） | **架构关注点**（#8 写状态） | — |
| 点击「抵押」→ 转发回合2 | 回合2 → `OnMortgageChanged`（最终） | **架构关注点**（#6 写状态） | — |
| 点击「赎回」→ 转发回合2 | 回合2 → `OnMortgageChanged`（最终） | **架构关注点**（#6 写状态） | — |
| 卡片 Close | — | 否 | 仅 UI 状态变化 |
| 订阅刷新（CR-9） | `OnOwnershipChanged` / `OnMortgageChanged` / `OnBuildingChanged` | 否（#17 只读响应） | 刷新对应字段，不关卡 |
| tile 叠加更新（CR-10） | `OnMortgageChanged` / `OnOwnershipChanged` | 否（#17 只读响应） | 常驻叠加 cached 状态变量更新 |

**架构关注点标注**（ADR-0007 边界）：所有 Write 路径均经回合2 ResolvePhase → 对应 owner 系统执行；#17 是纯意图转发，不持任何写引用。CI 须 code-review 验 #17 源码无 `SetOwner`/`AddHouse`/`Mortgage` 等写方法引用（GDD AC-16b）。

---

## 9. Transitions & Animations

> 动效时长常量引自 art-bible §7.5，为硬约束非旋钮。`reduce_motion=On` 时动效旁路规则见 §12 Accessibility。

| 动效 | 触发 | 规格 | reduce_motion=On |
|------|------|------|-----------------|
| **卡片 pop-in** | Closed→Open | Ease-Out Bounce/Back，250ms（可调 200–300ms，Tuning Knob `T_card_open`） | 瞬现（≤50ms） |
| **卡片关闭** | Open→Closed | Ease-In，150ms | 瞬现 |
| **卡片翻转入场** | OpenCard（MVP 可选） | 3D Y 轴 Ease-In-Out，300ms（§7.5 固定） | 瞬现 |
| **按钮按下** | 点击 / 手柄确认 | Ease-In scale 0.95，80ms（§7.5 固定） | 无 scale 动效，但仍视觉响应 |
| **按钮松开** | 点击松开 | Ease-Out Back scale 1.0→1.05→1.0，150ms | 直接复位 |
| **整卡抵押灰化** | `IsMortgaged` 从 false→true（订阅刷新） | HSV 去饱和渐变，300ms；斜纹淡入 | 瞬切 |
| **高亮行切换** | `OnBuildingChanged` 刷新 F-1 | 旧行淡出 / 新行淡入，150ms | 瞬切 |
| **垄断完成环圈出现** | `IsMonopoly` 变 true | 描边脉冲扩散，200ms | 瞬现 |
| **VFX juice（建房星星/买地脉冲）** | V-Enable-01..04（VFX19 订 #8/#6 直接触发） | 见 art-bible §6.3（enable-not-own，#17 不持引用） | VFX19 据全局 `reduce_motion` 布尔降级 |

**与 HUD 衔接**：卡片 Open 时 HUD 操作栏部分按钮状态联动（HUD 快览入口高亮），但 HUD 不拥卡片内容；卡片 Close 时 HUD 焦点回归操作栏默认焦点（`ReturnFromModal` 触发 ADR-0012 栈恢复）。

**与 VFX19 衔接**（enable-not-own）：#17 提供 V-Enable-01..08 事件锚点（tile 坐标、NewCount 等），VFX19 直订 #6/#8 事件。卡片 Open/Close 不影响 VFX19 的独立订阅生命周期。

---

## 10. Data Requirements

> #17 是呈现层纯叶子，所有数据项为 Read-only。Write 路径均经回合2 ResolvePhase → owner 执行，#17 不持写引用（ADR-0007/ADR-0008 边界）。

### 10.1 产权契据卡数据清单

| 显示项 | 来源系统 | 接口 / 字段 | R/W | 注意事项 |
|--------|---------|------------|-----|---------|
| 地块名 (`DisplayName`) | 棋盘(1) | `GetTileData(TileIndex).DisplayName` | R | — |
| 色组 (`ColorGroup`) | 棋盘(1) | `GetTileData.ColorGroup` | R | 映射 §4.4 色值 |
| 购买价 (`PurchasePrice`) | 棋盘(1) | `GetTileData.PurchasePrice` | R | — |
| 租金表 (`RentTable`) | 棋盘(1) | `GetTileData.RentTable[0..N]` | R | Property=6项/Railroad=4项/Utility 不用；按 TileType 变长访问（OQ-PC-10 已闭合） |
| 骰子倍率 (`DiceMultiplierTable`) | 棋盘(1) | `GetTileData.DiceMultiplierTable[0..1]` | R | 仅 Utility 用；**绝不显示为固定现金**（CR-5/EC-10） |
| 建房单价 (`BuildingCost`) | 棋盘(1) | `GetTileData.BuildingCost` | R | 仅 Property 用 |
| 抵押套现额 (`MortgageValue`) | 棋盘(1) | `GetTileData.MortgageValue` | R | **必须与赎回价并列显示，不得单独显示（EC-13/AC-42）** |
| 归属 (`GetOwner`) | 所有权(6) | `GetOwner(TileIndex)` | R | 映射玩家色+P编号 |
| 是否抵押 (`IsMortgaged`) | 所有权(6) | `IsMortgaged(TileIndex)` | R | 驱动灰化+斜纹+F-1 分支 |
| 是否垄断 (`IsMonopoly`) | 所有权(6) | `IsMonopoly(TileIndex)` | R | 单源依赖，翻转靠 #6 不靠 #17 自算（EC-8） |
| 车站数 (`GetStationCount`) | 所有权(6) | `GetStationCount()` | R | 仅 Railroad |
| 公用数 (`GetUtilityCount`) | 所有权(6) | `GetUtilityCount()` | R | 仅 Utility |
| 建筑数 (`GetHouseCount`) | 建房升级(8) | `GetHouseCount(TileIndex)` | R | **必须取自 #8，严禁取自 #6（防 6↔8 环，CR-6/AC-6）** |
| 建房可行 + 原因码 (`CanBuildHouse`) | 建房升级(8) | `CanBuildHouse(TileIndex) → {bool, 原因码}` | R | 四原因码驱动 tooltip |
| 卖房可行 (`CanSellHouse`) | 建房升级(8) | `CanSellHouse(TileIndex)` | R | — |
| 卖回额 (`GetSellback`) | 建房升级(8) | `GetSellback(TileIndex)` | R | **必须取自 #8，#17 不自算 floor（EC-14/AC-43）** |
| 赎回价 (`GetUnmortgageCostForDisplay`) | 经济(5) | `GetUnmortgageCostForDisplay(MV) → int32` | R | **必须取自 owner 接口，#17 不自算 ceil（EC-13/AC-42）**；OQ-PC-8 已闭合（economy-cash L65） |

### 10.2 计算值（#17 内部推导，纯显示）

| 计算值 | 推导公式 | 输入来源 | 备注 |
|--------|---------|---------|------|
| `highlight_row` (Property) | F-1(a)：G-0b guard → `is_mortgaged?-1` → `(is_monopoly∧hc==0)?0` → `clamp(hc,0,5)` | #6/#8 | 越界走 EC-2 降级（G-0a），不 clamp 静默吸收 |
| `highlight_row` (Railroad) | F-1(b)：`(mortgaged∨station_count==0)?-1` → `clamp(station_count-1,0,3)` | #6 | — |
| `highlight_multiplier` (Utility) | F-1(c)：`(mortgaged∨utility_count==0)?-1` → `clamp(utility_count-1,0,1)` | #6 | — |
| `show_monopoly_badge` | `¬is_mortgaged ∧ is_monopoly ∧ hc==0` | #6/#8 | 严防发散：有房绝不显 ×2（AC-11 #6组） |
| `next_rent_preview` | `RentTable[clamp(hc+1,0,5)]`，`hc==5` 时隐藏 | #1/#8 | 建后租金预览（EC-14/AC-43） |

### 10.3 架构边界（ADR-0007 UI 纯叶子约束）

- #17 不调用任何 economy5 (#5) 接口（除 `GetUnmortgageCostForDisplay` 只读纯函数）
- #17 不调用 回合2 (#2) 读接口（只转发意图到 ResolvePhase 公开接口）
- #17 不持有 #7 事件格任何订阅（MVP；Alpha deferred）
- #17 不持有 VFX19/音频22 任何引用（enable-not-own）
- 所有 Write 路径的权威门在 ResolvePhase；#17 affordance 灰化是尽力呈现，非权威闸（EC-11）

---

## 11. Accessibility

> 全部条款对照 design/accessibility-requirements.md WCAG-AA 基线及 GDD CA-01..05。

### 11.1 键盘/手柄可达

- [ ] 所有动作按钮（买地/建房/卖房/抵押/赎回/关闭）均可键盘 Tab 到达并激活（AA 2.1.1）
- [ ] 手柄可遍历全部卡片 UI，无鼠标专属死角（ADR-0012 CommonUI 全套，P-06）
- [ ] 卡片激活时 `GetDesiredFocusTarget` 返回首可用按钮（AA 2.4.3）
- [ ] 关闭路径：Esc 键 / 手柄 Face Right / 点外部空白三路均可达（`IA_Cancel`，P-04）
- [ ] **禁 hover-only**：门控原因 tooltip 在聚焦禁用按钮时自动显示（GDD CR-7/AC-40，§4.1）

### 11.2 焦点指示器（AA 2.4.7）

- [ ] 当前聚焦元素有清晰可见描边（推荐：3px + 亮度 +10%），对比背景 ≥ 3:1
- [ ] 焦点态与 hover 态视觉可区分（描边+亮度 vs 仅亮度，OQ-A11Y-4）
- [ ] 焦点不陷入卡片内循环——关闭按钮可焦点可到达，不产生焦点陷阱（AA 2.1.2）

### 11.3 对比度（AA 1.4.3/1.4.11）

- [ ] 地块名/购买价/租金额/标签文字 ≥ 4.5:1（Dark Ink `#1A1A2E` on Off White `#FAFAF5`，名义约 14:1，安全）
- [ ] 次要文字（Dark Ink 60%，Level 4 注释）对 Off White 背景实测 ≥ 4.5:1（半透明叠加须复测）
- [ ] 色组色条、聚焦框、图标 ≥ 3:1（AA 1.4.11）
- [ ] 「已抵押」标签在去饱和卡片背景上对比 ≥ 4.5:1（灰化后背景变浅须重新实测）

### 11.4 非颜色冗余（AA 1.4.1）

所有权：
- [ ] 玩家色徽章 + 白色 P 编号（≥10px）双冗余；P1 红 vs P3 绿靠编号区分（CA-01）

垄断完成：
- [ ] 色组环圈（形状）+ 「全」/★（文字/图标）+ Fortune Gold（颜色）三重（CA-02/V-Own-07）

抵押：
- [ ] 去饱和（饱和度）+ 对角斜纹（纹理）+ 「已抵押」标签（文字）三重（CA-03/V-Own-06）

高亮租金行：
- [ ] 背景色块 + 左 4px 竖线（位置）+ SemiBold 字重 + ×2 文字徽章（非仅色）（CA-04）

禁用按钮：
- [ ] 去饱和至灰度 + 不透明度 50% + 图标形状区分（非仅靠色，art-bible §4.5）

### 11.5 最小字号（art-bible §7.2 / accessibility-requirements §3.1）

- [ ] 全卡所有文字 ≥ 10px @1080p（含 Level 4 注释、P 编号、门控原因 tooltip 内文字）
- [ ] 等宽数字字形用于金额显示（Tabular Figures，防横向跳动，OQ-UX-PC-7 字体资产名待落为 BLOCKING）

### 11.6 reduce-motion（AA 2.3.3 精神）

- [ ] `reduce_motion=On`：卡片 pop-in 瞬现（≤50ms）、按钮去 scale 弹性、整卡灰化渐变旁路为瞬切
- [ ] `reduce_motion` 为全局单一布尔，与 HUD/VFX19 共享（不独立开关，Tuning Knobs）
- [ ] 断言方式：配置参数读回为旁路/0（非「scale≤1.05」永真陷阱，继承 HUD AC-57 R-3 谓词形态）

### 11.7 Utility 特殊要求

- [ ] Utility 契据卡**强制显示**「实租 = 骰点合计 × 倍率」文案（≥10px），禁止显示固定现金金额（CR-5/EC-10/AC-8）

---

## 12. Localization Considerations

| 项目 | 本地化要求 | 风险 |
|------|-----------|------|
| 地块名 (`DisplayName`) | 须可本地化（来自 #1 DA 数据资产），不硬编码 | 中文名可能超出固定宽卡片布局，须容器允许换行或字号缩小 |
| 动作按钮文案（买地/建房/卖房/抵押/赎回） | 须可本地化 | 德语等膨胀语言按钮可能超宽；按钮须允许文本截断+tooltip 补全 |
| 「实租 = 骰点 × 倍率」文案 | 须可本地化，且**不可省略**（EC-10） | 长语言版本须换行不截断 |
| 「已抵押」/「数据异常」/「全」标签 | 须可本地化 | 短文本，低风险 |
| 数字/货币格式 | 全部金额走 `FText::AsNumber` / `FText::AsCurrencyBase`（本地化数字格式） | 等宽数字字形须覆盖目标语言的数字字符集 |
| 门控原因 tooltip 文案 | 四原因码须可本地化映射（非硬编码枚举显示） | 长原因文案在 tooltip 内须换行显示 |
| RTL 语言（从右到左） | [provisional] MVP 未计划 RTL 支持；布局若为固定左到右须登记为 OQ | 色组色条、抵押斜纹方向在 RTL 需镜像（OQ-UX-PC-10） |

---

## 13. Acceptance Criteria

> QA 不需读其他文档即可验收。全部条款可执行。

- [ ] **AC-UX-PC-1（性能 — 开屏响应）** GIVEN 玩家点击棋盘 tile；WHEN `OpenCard(TileIndex)` 触发；THEN 卡片面板在 **200ms 内进入 Open 态**（状态机转换，非 pop-in 动画完成），三源数据全部绑定；超时或数据未绑定 = FAIL。[Logic·可测量]

- [ ] **AC-UX-PC-2（导航 — entry/exit 路由）** GIVEN 玩家经「棋盘点击」→「HUD 快览入口」→「Esc 键」三路分别打开/关闭地产卡；WHEN 各路径触发；THEN 状态机在 Closed→Open（打开）/ Open→Closed（关闭）正确转换，CommonUI 激活栈 push/pop 对称（`SwitchToModal`/`ReturnFromModal` 各调一次），焦点在关闭后回到 HUD 操作栏；任一路径失败 = FAIL。[Logic]

- [ ] **AC-UX-PC-3（错误态/空态 — 数据异常 fail-safe）** GIVEN `owner==NONE ∧ GetHouseCount=3`（矛盾态，EC-6/G-0b）；WHEN `OpenCard`；THEN `highlight_row==-1`（无租金行高亮）、`show_monopoly_badge==false`、显「⚠ 数据异常」占位标签、动作按钮全禁用；绑定值无自修正（spy 无 #6/#8 写调用）。`highlight_row==3`（clamp 结果）= FAIL。[Logic]

- [ ] **AC-UX-PC-4（a11y — WCAG-AA 键盘/手柄）** GIVEN 仅用键盘 Tab + Enter（或手柄导航 + Face Button）；WHEN 遍历已打开的地产卡 UI；THEN 全部动作按钮（买地/建房/卖房/抵押/赎回/关闭）均可到达并激活；门控原因 tooltip 在聚焦禁用建房按钮时可见（非 hover-only）；Esc/Face Right 可关闭卡片。任一元素键盘/手柄不可达 = FAIL。[Logic·可执行]

- [ ] **AC-UX-PC-5（本屏核心目的 — 赎回价与抵押价必须同时显示且口径正确）** GIVEN `MortgageValue=100`、`GetUnmortgageCostForDisplay(100)=110`、`IsMortgaged=true`；WHEN `OpenCard`；THEN 抵押区块同时绑定「抵押可得 ¥100」与「赎回需付 ¥110」，两值均可见；赎回价绑定值取自 `GetUnmortgageCostForDisplay` 返回（非 #17 自算 ceil），且**赎回价显示值 ≠ MortgageValue**（110≠100）。仅显其一 = FAIL；两值相同 = FAIL。[Logic·门控 OQ-PC-8（已闭合 R-2）]

- [ ] **AC-UX-PC-6（本屏核心目的 — 建后租金预览+卖回额双显）** GIVEN `GetSellback=50`、`house_count=2`、`RentTable=[2,10,30,90,160,250]`；WHEN `OpenCard`；THEN 建房区块绑卖回额==50 + 建后租金预览==90（`RentTable[3]`）；`house_count==5` 变体：预览隐藏不报错；缺任一 = FAIL。[Logic]

- [ ] **AC-UX-PC-7（reduce-motion — 参数读回断言）** GIVEN `reduce_motion=On`；WHEN `OpenCard`；THEN 卡片 pop-in 动画有效时长配置参数为旁路/≤50ms（非「scale≤1.05」永真谓词），按钮去 scale 弹性配置参数为 0；`reduce_motion` 布尔值与 HUD/VFX19 读取同一来源（共享布尔，非独立副本）。任一动画仍按 Off 参数运行 = FAIL。[Logic·配置参数读回]

---

## 14. Open Questions

| ID | 问题 | Owner | 目标期限 |
|----|------|-------|---------|
| **OQ-UX-PC-1** | **player-journey.md 未建立** — §2 Player Context 情绪状态为 provisional 推断；需建立 player journey map 模板 `.claude/docs/templates/player-journey.md` 后正式验证。 | ux-designer | Pre-Production |
| **OQ-UX-PC-2** | **买地/抵押/赎回意图转发后是否自动关闭卡片** — §4 出口标 provisional（仅买地自动关，建房保持 Open）；需 game-designer 确认最佳交互节奏。 | game-designer + ux-designer | Pre-Production |
| **OQ-UX-PC-3** | **焦点顺序精确 tab 序** — 卡片内完整 widget 树焦点链定义，需 ue-umg-specialist 落实 CommonUI `GetDesiredFocusTarget` 链。（对齐 OQ-IP-1） | ue-umg-specialist + ux-designer | Pre-Production |
| **OQ-UX-PC-4** | **棋盘 tile 叠加 widget 所有权/生命周期**（P-15 常驻叠加）— world-space `UWidgetComponent` per tile vs screen-space overlay，架构 ADR 未落定（GDD OQ-PC-4，与 OQ-HUD-2 同门控）。 | technical-director / unreal-specialist | 架构期 ADR |
| **OQ-UX-PC-5** | **点击目标最小尺寸** — 动作按钮组在 1080p 下精确最小点击区域（px）；Fitts's Law 推荐行动按钮 ≥ 44px 高；需确认 CommonUI 按钮默认最小尺寸。（对齐 OQ-IP-1/OQ-HUD-9） | ux-designer + ue-umg-specialist | Pre-Production |
| **OQ-UX-PC-6** | **Utility 色组色** — V-Own-03 fallback 天空蓝 `#3AACDB`，art-bible §4.4 尚未正式分组；此色也是 P2 默认色（§5.2），潜在混淆。（GDD OQ-PC-3） | art-director | art-bible 更新 |
| **OQ-UX-PC-7** | **等宽数字字体资产名** — art-bible §7.2 propagate 债未闭合（OQ-A11Y-5）；WBP TextBlock 无可绑字体则 Tabular Figures 规格无法实现。**BLOCKING（Pre-Production asset-spec 前）** | art-director | Pre-Production asset-spec 前 |
| **OQ-UX-PC-8** | **reduce_motion 具体降幅参数** — `T_card_open` 旁路精确阈值（≤50ms 当前为 provisional）；与 HUD OQ-HUD-9 统一裁定。（GDD OQ-PC-7，对齐 OQ-IP-5） | ux-designer | Pre-Production `/ux-design` |
| **OQ-UX-PC-9** | **焦点指示器视觉规格** — 描边宽度/颜色/与 hover 态区分方案待 art-director 落定（art-bible §4.5）。（OQ-A11Y-4） | ux-designer + art-director | Pre-Production `/ux-design` |
| **OQ-UX-PC-10** | **RTL 语言本地化支持** — §12 中未计划；若产品进入 RTL 市场，色组色条、斜纹方向、layout 均需重审。非 MVP 阻塞，Full Vision。 | ux-designer + ue-umg-specialist | Full Vision |
| **OQ-UX-PC-11** | **卡片最大高度约束与 HUD 不遮挡关系** — art-bible §7.1 要求棋盘 ≥65% 屏幕；1080p 下卡片高度上限（provisional ~580px）需与 HUD 边界联合验证，确保卡片不压棋盘核心区域。 | ux-designer + art-director | Pre-Production（layout 原型阶段） |
