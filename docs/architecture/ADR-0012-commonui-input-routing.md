# ADR-0012: CommonUI 输入路由与激活栈

## Status

Accepted

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

msc（用户）+ Technical Director + ue-umg-specialist（主笔）— 2026-06-06 Accepted（用户裁定全接受推荐选型：UI 框架采 CommonUI 全套〔`UCommonActivatableWidget` + `UCommonButtonBase` + `UCommonActivatableWidgetStack`〕，纯 UMG 备选被排除；HIGH post-cutoff 风险经「实现首日对照引擎源码核验签名」缓解）

## Summary

为《骰子大亨》全部呈现层屏（主菜单/大厅、HUD、地产卡 UI、模态弹窗）裁定 UI 框架与
屏切换/输入路由机制。决定：采用 **CommonUI 全套** —— `UCommonActivatableWidget`
（可激活屏）+ `UCommonActivatableWidgetStack`（激活栈管理屏层叠/modal）+
`UCommonButtonBase`（输入无关按钮，手柄/鼠标统一）。所有可操作元素禁 hover-only，
为手柄/客厅模式预留。CommonUI 的输入路由与 ADR-0011 的 `IMC_Modal` 协作：CommonUI
激活栈 push/pop 屏时调用 `UBoardInputManager::SwitchToModal()`/`ReturnFromModal()`。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | UI（CommonUI） |
| **Knowledge Risk** | **HIGH** — CommonUI 在 UE5.x 系列大幅迭代（5.4–5.7 持续演进），`UCommonActivatableWidget` 激活栈语义、`UCommonButtonBase` 输入路由、`UCommonUIInputData`/`FUIActionBindingHandle` 等 API 均超出模型 ~5.3 训练边界，属 LLM 盲区 |
| **References Consulted** | `docs/engine-reference/unreal/modules/ui.md`（5.7 UMG/CommonUI 模式）；`docs/engine-reference/unreal/current-best-practices.md`（UI 框架推荐）；`docs/architecture/architecture.md` §1.2（UI 域 B 策略裁定 CommonUI）；ADR-0007（BP-vs-C++ 边界）；ADR-0011（Modal IMC 时序） |
| **Post-Cutoff APIs Used** | `UCommonActivatableWidget`（ui.md 核验为 5.7 稳定 CommonUI 基类）；`UCommonActivatableWidgetStack`（激活栈容器）；`UCommonButtonBase`（输入无关按钮基类）；`UCommonActivatableWidget::GetDesiredFocusTarget`（手柄默认焦点）。**以上须实现首日对照引擎源码（Engine/Plugins/Runtime/CommonUI）逐一核验签名——见 Verification Required。** |
| **Verification Required** | ① **实现首日**：对照 UE5.7 源码 `Engine/Plugins/Experimental/CommonUI`（或已转正式路径）核验 `UCommonActivatableWidget` 激活/反激活回调名（`NativeOnActivated`/`NativeOnDeactivated`）+ `UCommonActivatableWidgetStack::AddWidget`/`RemoveWidget` 签名；② CommonUI 插件在 5.7 是否仍为 Experimental（影响 cook/打包）；③ `-nullrhi` headless 下 CommonUI Widget 可否实例化（呈现层 AC 多为 Advisory，但激活栈逻辑若可 headless 测则升 [Logic]）；④ 手柄默认焦点 `GetDesiredFocusTarget` 与 `IMC_Modal`（ADR-0011）截获不冲突 |

> **Note**: Knowledge Risk = HIGH，是 12 个 Foundation/Core ADR 中引擎不确定性**最高**的一条。
> CommonUI 持续迭代，签名核验**必须在写第一行 UI 代码前完成**，不得凭模型记忆推进。
> 若 5.7 CommonUI API 与本 ADR 描述有出入，以引擎源码为准并回修本 ADR + architecture.md §1.2。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0007（BP-vs-C++ 边界 — CommonUI Widget 为 BP 呈现层，激活栈切换调用点的 C++/BP 归属须 ADR-0007 支撑）；ADR-0011（Enhanced Input — Modal IMC 激活/关闭时机与 CommonUI 激活栈 push/pop 协作，本 ADR 参照 ADR-0011 §Modal IMC 时序） |
| **Enables** | HUD(16) / 地产卡 UI(17) / 主菜单大厅(20) 实现 story —— `/story-readiness` 在本 ADR Accepted 前阻塞引用 UI 框架的 story；ADR-0008（HUD 驱动）的 UMG widget 宿主即 CommonUI 基类 |
| **Blocks** | 全部呈现层屏 story（主菜单/HUD/卡牌/模态弹窗）——无 Accepted UI 框架 ADR 时无法通过 `/story-readiness` 门 |
| **Ordering Note** | Sprint 1+，可与 ADR-0008（HUD 驱动）同期起草。Accepted 须在首个 UI 屏 story 开工前完成；**Verification Required ① 须在该 story 第一行代码前执行** |

## Context

### Problem Statement

《骰子大亨》是重度 UI 项目（HUD 62 AC、地产卡 41 AC、主菜单/大厅多屏、模态弹窗），
technical-preferences 明确要求**禁 hover-only**（所有可操作元素须有显式点击/确认路径）、
**Gamepad Partial 支持**（为后续手柄/客厅模式预留）。若用裸 UMG：

- 屏切换（菜单→对局→模态弹窗）须手工管理 Widget 栈层叠、Z-order、输入截获
- 手柄导航（焦点移动、默认焦点、确认/返回语义）须每个屏手写 `NativeOnKeyDown`/焦点链
- modal 弹窗输入截获须手动 flag 下层 Widget 不响应
- 鼠标与手柄输入语义统一（同一按钮两种输入）须自造抽象

这等价于**重造一个简化版 CommonUI**，且长期技术债高、Reversibility 低。

### Current State

项目尚无任何 UI 实现（0 widget）。`architecture.md` §1.2 已就 UI 域裁定 B 策略
（采 CommonUI 现代范式为手柄/客厅模式预留），但未以 ADR 形式固化激活栈/输入路由机制。
HUD/地产卡/主菜单三档 GDD 均登记「禁 hover-only + 手柄预留」需求。

### Constraints

- **平台约束**：PC/Steam 鼠标点击为主，Gamepad Partial 须可后期零代码追加（technical-preferences）
- **交互约束**：禁 hover-only（technical-preferences + architecture.md §1.2）
- **架构约束**：呈现层纯叶子（HUD/卡牌 UI 只读/订阅/显示/转发意图，不写状态）；UI 框架须与 ADR-0007 边界一致（UMG widget=BP，输入路由逻辑=C++ 权威）
- **输入约束**：modal 弹窗输入截获须与 ADR-0011 `IMC_Modal`(P=20) 协作，不重复造截获机制
- **引擎约束**：CommonUI 是 HIGH post-cutoff 域，签名须对照引擎源码核验

### Requirements

- **FR-1**：屏切换经激活栈统一管理（push/pop），不手工管理 Z-order/输入截获
- **FR-2**：所有按钮经输入无关基类，鼠标点击与手柄确认走同一路径（禁 hover-only）
- **FR-3**：手柄默认焦点、焦点导航开箱即用，不每屏手写焦点链
- **FR-4**：modal 弹窗激活时下层屏输入不响应，与 ADR-0011 `IMC_Modal` 协作
- **FR-5**：手柄支持后期零代码追加（CommonUI 输入无关抽象天然支持）
- **非功能 PR-1**：UI 框架与 ADR-0007 边界一致（BP widget + C++ 激活栈切换调用点）

---

## Decision

### DECISION FORK：UI 框架选型

#### 选项 A：CommonUI 全套（已裁定 Accepted）

采用 CommonUI 三件套：

| 组件 | 角色 |
|------|------|
| `UCommonActivatableWidget` | 可激活屏基类（主菜单/HUD/卡牌弹窗各派生），含激活/反激活生命周期 + 手柄默认焦点 `GetDesiredFocusTarget` |
| `UCommonActivatableWidgetStack` | 激活栈容器，push/pop 屏管理层叠与输入截获（栈顶激活、下层暂停） |
| `UCommonButtonBase` | 输入无关按钮基类，鼠标点击/手柄确认统一路径，禁 hover-only |

- **代价**：HIGH post-cutoff 知识风险（签名须实现首日对照源码核验）；团队需补 CommonUI 熟悉度；CommonUI 插件须启用（可能 Experimental，影响打包）
- **收益**：手柄导航/modal 截获/输入无关按钮**开箱即用**（FR-2/3/4/5 全满足）；与 architecture.md §1.2 B 域裁定一致；Epic 官方为「禁 hover-only + 手柄」专门设计

#### 选项 B：纯 UMG（手工实现激活栈/手柄导航/modal 截获）

- **代价**：自建激活栈 + `NativeOnKeyDown` 手工焦点导航 + 手动 modal 截获 = 重造简化版 CommonUI；长期技术债高；手柄适配须每屏手写；Reversibility 低（后期迁 CommonUI 须重构全部屏）
- **收益**：无 CommonUI post-cutoff 风险（裸 UMG `meta=(BindWidget)` pre-5.3 稳定）；无额外插件依赖

**裁定：选 A（CommonUI 全套）**。理由：GDD 三档均要求禁 hover-only/手柄预留，CommonUI
是 Epic 官方为此专门设计的框架；纯 UMG 替代等价于重造一个简化版 CommonUI，长期技术债
更大且 Reversibility 低。唯一代价（HIGH post-cutoff 风险）由「实现首日对照引擎源码核验
签名」缓解——这是一次性前置成本，远低于纯 UMG 的持续自造成本。

---

### Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  PLATFORM — CommonUI Plugin（UE5.7；实现首日核验是否 Experimental）  │
│  UCommonActivatableWidget / UCommonActivatableWidgetStack /        │
│  UCommonButtonBase / UCommonUIInputData（输入动作映射）             │
└──────────────────────────────▲───────────────────────────────────┘
                                │ 派生 + 激活栈 push/pop
┌──────────────────────────────┴───────────────────────────────────┐
│  PRESENTATION — 屏（BP 派生 CommonActivatableWidget）              │
│  ┌─ 根激活栈（UCommonActivatableWidgetStack，挂 GameViewport/HUD）┐ │
│  │   ├ WBP_MainMenu      （主菜单，ADR-0008 HUD 驱动同栈）        │ │
│  │   ├ WBP_Lobby         （房间大厅）                            │ │
│  │   ├ WBP_HUD           （对局 HUD，常驻底层）                  │ │
│  │   └ WBP_PropertyCard  （地产卡 modal，push 到栈顶）           │ │
│  └────────────────────────────────────────────────────────────┘ │
│  按钮：WBP_* 内一律用 UCommonButtonBase 派生（禁 hover-only）      │
└──────────────────────────────▲───────────────────────────────────┘
                                │ 激活栈 push/pop modal 时
┌──────────────────────────────┴───────────────────────────────────┐
│  FOUNDATION — UBoardInputManager : UWorldSubsystem（ADR-0011）     │
│  modal push → SwitchToModal()   → AddMappingContext(IMC_Modal,20)  │
│  modal pop  → ReturnFromModal() → RemoveMappingContext(IMC_Modal)  │
│  （CommonUI 激活栈与 Enhanced Input IMC 双层协作，输入截获不重复）  │
└───────────────────────────────────────────────────────────────────┘
```

**CommonUI 激活栈 ↔ IMC_Modal 协作时序**：

```
打开地产卡弹窗
  → Stack.AddWidget<UPropertyCardWidget>()   // CommonUI 激活栈 push
  → WBP_PropertyCard::NativeOnActivated()
      → GetSubsystem<UBoardInputManager>()->SwitchToModal()  // ADR-0011 IMC_Modal(P=20)
  → 下层 WBP_HUD 暂停（CommonUI 栈语义）+ IMC_Gameplay 被 IMC_Modal 覆盖（Enhanced Input）

关闭地产卡弹窗
  → Stack.RemoveWidget(PropertyCardWidget)    // CommonUI 激活栈 pop
  → WBP_PropertyCard::NativeOnDeactivated()
      → GetSubsystem<UBoardInputManager>()->ReturnFromModal() // 移除 IMC_Modal
  → WBP_HUD 恢复激活 + IMC_Gameplay 可用
```

> 双层协作分工：**CommonUI 激活栈管"哪个屏可见/激活"**（widget 层），
> **Enhanced Input IMC 管"哪些输入意图触发"**（输入层）。两层各司其职、协作但不重叠。

---

### Implementation Guidelines

1. **实现首日先做 Verification Required ①**：对照 UE5.7 引擎源码核验
   `UCommonActivatableWidget`/`UCommonActivatableWidgetStack`/`UCommonButtonBase` 签名
   与激活回调名，**核验通过后再写第一个 WBP_**。
2. **启用 CommonUI 插件**：Project Settings → Plugins → CommonUI；核验是否 Experimental
   （影响 Shipping cook）；配置 `UCommonUIInputData`（确认/返回默认动作）。
3. **所有按钮派生 `UCommonButtonBase`**：禁直接用 `UButton`（无手柄/输入无关支持）；
   禁 hover-only——所有交互须有 OnClicked/确认路径。
4. **屏派生 `UCommonActivatableWidget`**：override `GetDesiredFocusTarget` 返回手柄默认焦点
   （如主菜单的"开始游戏"按钮），满足 FR-3。
5. **激活栈宿主**：根 `UCommonActivatableWidgetStack` 挂在 HUD/GameViewport；
   modal（地产卡/确认框）push 到栈顶，菜单/HUD 为底层屏。
6. **激活栈 ↔ IMC 协作**：modal 屏的 `NativeOnActivated`/`NativeOnDeactivated` 调用
   ADR-0011 `SwitchToModal`/`ReturnFromModal`（BP 经 `GetSubsystem` 调用，ADR-0007 边界：
   BP 路由 → C++ 逻辑）。
7. **headless 测试**：CommonUI Widget 多为渲染层（AC 多 Advisory）；激活栈 push/pop 的
   **状态逻辑**若可 `-nullrhi` 实例化则升 [Logic]（Verification Required ③）。

---

## Alternatives Considered

### Alternative 1：纯 UMG（手工激活栈 + 手柄导航 + modal 截获）

- **Description**：用裸 `UUserWidget` + 自建 Widget 栈 + `NativeOnKeyDown` 手工焦点导航 + 手动 flag modal 截获
- **Pros**：无 CommonUI post-cutoff 风险；无额外插件；`meta=(BindWidget)` pre-5.3 稳定
- **Cons**：等价重造简化版 CommonUI；手柄适配须每屏手写焦点链；modal 截获手动易漏；长期技术债高；后期迁 CommonUI 须重构全部屏（Reversibility 低）
- **Estimated Effort**：单屏相近，跨屏 + 手柄 + modal 累计远高
- **Rejection Reason**：与 architecture.md §1.2 B 域裁定矛盾；自造成本随屏数与手柄需求指数增长，违反 FR-2/3/4/5

### Alternative 2：CommonUI 仅按钮（`UCommonButtonBase`）+ 裸 UMG 屏管理

- **Description**：用 CommonUI 输入无关按钮，但屏切换仍裸 UMG 手工管理
- **Pros**：拿到输入无关按钮收益，CommonUI 暴露面小
- **Cons**：激活栈/modal 截获/手柄焦点仍须自造；半套方案不获完整收益，反增两套混用复杂度
- **Estimated Effort**：中
- **Rejection Reason**：半套不获 FR-1/FR-3/FR-4 收益，混用复杂度高于全套

---

## Consequences

### Positive

- 屏切换/modal 截获/手柄导航/输入无关按钮**开箱即用**，满足禁 hover-only + 手柄预留
- 与 ADR-0011 Enhanced Input 双层协作清晰（widget 层 vs 输入层各司其职）
- 与 architecture.md §1.2 B 域裁定一致；Epic 官方框架长期受支持
- 手柄支持后期零代码追加（CommonUI 输入无关抽象）

### Negative

- HIGH post-cutoff 风险：签名须实现首日对照引擎源码核验（一次性前置成本约 0.5 天）
- 团队需补 CommonUI 熟悉度（激活栈/输入动作映射心智模型）
- CommonUI 插件若 5.7 仍 Experimental，须评估 Shipping cook 稳定性

### Neutral

- 所有屏继承 CommonUI 基类，UMG 设计器工作流基本不变（仍 WBP + BindWidget）
- 激活栈 + IMC 双层协作需在 code-review 核对 push/pop ↔ SwitchToModal/ReturnFromModal 配对

---

## Risks

| 风险 | 概率 | 影响 | 缓解措施 |
|------|-----|------|---------|
| 5.7 CommonUI API 签名与本 ADR 描述出入 | 中 | 高（UI 代码返工） | **Verification Required ①：实现首日对照引擎源码核验**，核验通过再写 WBP_；出入则回修本 ADR + architecture.md §1.2 |
| CommonUI 插件 5.7 仍 Experimental，Shipping cook 不稳 | 低 | 中 | Verification Required ②；若不稳退化 Alternative 2（仅按钮）或 1（纯 UMG），但须重新 gate |
| 激活栈 pop 与 IMC_Modal 移除时机不配对致输入卡死 | 中 | 中 | code-review 强制核对 NativeOnActivated/Deactivated ↔ SwitchToModal/ReturnFromModal 配对；加 `bModalActive` 幂等守卫（ADR-0011） |
| 手柄默认焦点与 IMC_Modal 截获冲突 | 低 | 低 | Verification Required ④ 实机验证 |

---

## Performance Implications

| 指标 | 当前 | 预期 | 预算 |
|------|-----|------|------|
| CPU（帧时间，UI tick） | 0 ms | < 0.3 ms（回合制低频 UI 更新） | 16.6 ms |
| 内存（CommonUI 插件 + widget） | 0 | < 2 MB（轻量回合制 UI） | — |
| Draw Calls（UI） | 0 | 适中（重度 UI 但单场景） | 待目标硬件 |

CommonUI 相较裸 UMG 无显著运行时开销；激活栈 push/pop 频率低（屏切换/弹窗，非帧内 Hot Path）。

---

## Migration Plan

初次实现（无现有 UI 代码，无迁移成本）：

1. **Sprint 1 Step 0**（约 0.5 天）：**Verification Required ① 引擎源码签名核验** + 启用 CommonUI 插件 + 配置 `UCommonUIInputData`。
2. **Sprint 1**（HUD）：`WBP_HUD : UCommonActivatableWidget`，根激活栈挂 HUD；按钮派生 `UCommonButtonBase`。
3. **Sprint 菜单**：`WBP_MainMenu`/`WBP_Lobby` 入激活栈；`GetDesiredFocusTarget` 设默认焦点。
4. **Sprint 卡牌 UI**：`WBP_PropertyCard` modal push/pop + `SwitchToModal`/`ReturnFromModal`（ADR-0011 协作）。
5. **Alpha 手柄**：CommonUI 输入无关抽象 + IMC Gamepad 键位（ADR-0011）零 widget 代码变更。

**Rollback plan**：本 ADR 尚无实现。若 Verification Required ① 发现 CommonUI 5.7 不可用/不稳，
退化 Alternative 2（仅按钮）或 1（纯 UMG），须修 architecture.md §1.2 + technical-director 重新 gate。

---

## Validation Criteria

- [ ] **实现首日**：`UCommonActivatableWidget`/`Stack`/`ButtonBase` 签名对照 UE5.7 源码核验通过（Verification Required ①）
- [ ] 所有按钮派生 `UCommonButtonBase`，无 hover-only 交互路径（FR-2 验证）
- [ ] 手柄默认焦点经 `GetDesiredFocusTarget` 正确返回，焦点导航可用（FR-3 验证）
- [ ] modal 弹窗 push 时下层屏输入不响应 + `IMC_Modal` 同步激活（FR-4 + ADR-0011 协作验证）
- [ ] 手柄键位追加后 widget C++/BP 代码零修改（FR-5 验证）
- [ ] 激活栈 pop 与 `ReturnFromModal` 配对，无输入卡死（code-review + 幂等守卫）

---

## GDD Requirements Addressed

| GDD 文档 | 系统 | 需求 | 本 ADR 满足方式 |
|---------|------|------|---------------|
| `design/gdd/hud.md` | HUD(16) | 重度 UI 可读性第一；操作栏按钮（掷骰/建房/结束回合）；禁 hover-only；UMG widget 结构 + 数据绑定 | `WBP_HUD : UCommonActivatableWidget`（根激活栈底层）；操作栏按钮 `UCommonButtonBase`；与 ADR-0008 IGameClock DI 状态机协作 |
| `design/gdd/property-card-ui.md` | 地产卡 UI(17) | 契据卡 modal 弹窗；建房/抵押动作转发；卡牌详情确认/取消 | `WBP_PropertyCard` push 到激活栈顶 modal；`SwitchToModal`（ADR-0011）截获输入；`UCommonButtonBase` 确认/取消 |
| `design/gdd/main-menu-lobby.md` | 主菜单大厅(20) | 本地热座 2–8 玩家配置 + AI 难度选择 + 开始游戏；席位/turn order 设置；低摩擦快速开局 | `WBP_MainMenu`/`WBP_Lobby : UCommonActivatableWidget`；`GetDesiredFocusTarget` 手柄默认焦点；`IMC_Menu`（ADR-0011）协作 |
| `.claude/docs/technical-preferences.md` | 全项目 | 禁 hover-only；Gamepad Partial 支持须可后期追加；鼠标点击为主 | CommonUI 输入无关按钮统一鼠标/手柄；禁 hover-only；手柄零代码追加 |
| `docs/architecture/architecture.md` §1.2 | 引擎集成（UI 域） | B 域采 CommonUI 现代范式为手柄/客厅模式预留；`CommonActivatableWidget`/`CommonButtonBase` | 本 ADR 完整固化 §1.2 方向性裁定为可执行架构决策 |

---

## Related

- ADR-0007（BP-vs-C++ 边界）— CommonUI widget=BP 呈现层，激活栈切换调用点经 `GetSubsystem` → C++ 逻辑
- ADR-0008（HUD 驱动与 IGameClock DI）— `WBP_HUD` 即 CommonUI 基类派生，HUD 状态机（C++ UObject）与之协作
- ADR-0011（Enhanced Input Mapping Context）— CommonUI 激活栈 push/pop modal 时调 `SwitchToModal`/`ReturnFromModal` 切换 `IMC_Modal`，双层协作
- `docs/engine-reference/unreal/modules/ui.md`（5.7 CommonUI 核验依据）
- `docs/architecture/architecture.md` §1.2（UI 域 B 策略裁定）
