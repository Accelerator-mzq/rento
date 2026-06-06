# ADR-0011: Enhanced Input Mapping Context 结构

## Status

Accepted

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

msc（用户）+ Technical Director + unreal-specialist（主笔）— 2026-06-06 Accepted（用户裁定全接受推荐选型：三层 IMC Gameplay/Menu/Modal Priority 叠加可单层起步 + UBoardInputManager:UWorldSubsystem 集中宿主 + 每意图独立 IA 资产 + 禁 legacy BindAction）

## Summary

为《骰子大亨》首个交互前裁定 Enhanced Input 的 Input Action 资产划分、IMC 分层策略、
BindAction 宿主位置与 Trigger 配置方式。决定：采用三层 IMC（`IMC_Gameplay` /
`IMC_Menu` / `IMC_Modal`）按优先级叠加切换，由 `UWorldSubsystem`（ADR-0001 对局宿主）
集中管理增删；每个交互意图对应独立 `IA_` 资产（`Digital`/`Axis2D` 两种 Value Type）；
legacy `BindAction`/`BindAxis`/`GetInputAxisValue` 全项目禁用。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Input |
| **Knowledge Risk** | **HIGH** — Enhanced Input 是 UE5 新增系统，legacy 在 5.7 已弃用；IMC 优先级叠加、`UEnhancedInputLocalPlayerSubsystem`、`ETriggerEvent` 枚举等 API 均在模型 ~5.3 训练数据边界附近 |
| **References Consulted** | `docs/engine-reference/unreal/modules/input.md`（5.7 Enhanced Input 全流程，2026-02-13 核验）；`docs/engine-reference/unreal/deprecated-apis.md`（L14-16 legacy 弃用明确）；`docs/engine-reference/unreal/current-best-practices.md`（L125-159 Enhanced Input 模式）；`docs/architecture/architecture.md` §1.1 输入裁定节；ADR-0001（宿主） |
| **Post-Cutoff APIs Used** | `UEnhancedInputLocalPlayerSubsystem::AddMappingContext` / `RemoveMappingContext`（input.md L80/L177 核验存在）；`ETriggerEvent::Started` / `Triggered` / `Completed`（input.md L93-96 核验存在）；`Hold` Trigger 配置（input.md L137 核验存在）。**以上均在 engine-ref 有文档根据，非模型记忆臆造。** `PlayerMappableInputConfig`（input.md L248）仅作手柄按键重绑用，MVP 不启用，标注风险待 Full Vision 核验。 |
| **Verification Required** | ① `-nullrhi` headless 下 `UEnhancedInputLocalPlayerSubsystem::AddMappingContext` 不 crash（IMC 管理宿主迁移至 WorldSubsystem 需验）；② `IMC_Modal` priority=20 下，`IMC_Gameplay` 绑定的 `IA_Roll` 确实被截获不触发（Input 分层截获验证）；③ AI 回合移除 `IMC_Gameplay` 后，`IA_Roll` / `IA_BuyProperty` 等 trigger 不触发（越权阻断验证）；④ 手柄追加映射后无需改 C++ 代码（Gamepad 就绪验证，input.md L214-222） |

> **Note**: Knowledge Risk = HIGH。Enhanced Input 在 UE5 系列持续迭代（5.4+ 新增
> `PlayerMappableInputConfig` 重绑 API）。如项目引擎从 5.7 升级，须重新对照 engine-ref
> 核验 IMC 优先级语义与 `AddMappingContext` 签名。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001（`UWorldSubsystem` 宿主裁定 — 本 ADR IMC 管理方挂在 `UWorldSubsystem`，须 ADR-0001 先 Accepted）；ADR-0007（BP-vs-C++ 边界 — IMC 切换调用点归 C++ WorldSubsystem vs BP Widget 需 ADR-0007 边界支撑，**本 ADR 可 Proposed 态先写，Accepted 前须 ADR-0007 到位**） |
| **Enables** | ADR-0012（CommonUI 输入路由与激活栈 — Modal IMC 与 CommonUI 激活栈协作边界须本 ADR 先裁）；首个可交互 story（掷骰/买地/主菜单按钮 — `/story-readiness` 在本 ADR Accepted 前阻塞引用输入层的 story） |
| **Blocks** | 首个交互 story（掷骰、买地、建房、菜单点击）——无 Accepted 输入 ADR 时这些 story 无法通过 `/story-readiness` 门 |
| **Ordering Note** | Sprint 1+。ADR-0001 Accepted 后可并行起草；Accepted 须在首个交互 story 开工前完成。ADR-0007（BP-vs-C++ 边界，同 Sprint 1+）可与本 ADR 并行起草，Accepted 时须对齐 §Decision IMC 切换调用点描述 |

## Context

### Problem Statement

《骰子大亨》回合制棋盘有三类截然不同的交互上下文：① 对局期玩家操作（掷骰、买地、
建房、结束回合）；② 菜单/大厅界面操作（席位配置、开始游戏）；③ 模态弹窗操作（卡牌
详情、确认/取消）。若不明确裁定 IMC 分层策略与绑定宿主，各 story 会各自为政建 IMC、
BindAction 散落各 Actor/Widget，导致：

- AI 回合无法从输入层杜绝越权，退化为 `if(bIsMyTurn)` 防御，违反 architecture.md §1.1
  明确已裁定的「移除 IMC_Gameplay 优于 handler 内 if」原则
- Modal 弹窗无法自动截获输入，须每个 Widget 手动 flag 阻断
- 手柄支持（technical-preferences：Partial，便于后续客厅模式）须改代码，违反
  Enhanced Input「只追加 IMC 映射、无需改代码」的核心收益
- IMC/IA 资产散乱，同一意图被多个 IMC 重复定义，运行时优先级不确定

**本 ADR 须在首个交互前裁定，否则每个 story 各自建 context 将积累不可逆技术债。**

### Current State

项目尚无任何 Input Action 或 Input Mapping Context 资产（0 实现）。
`technical-preferences.md` 已声明「使用 UE Enhanced Input 管理输入映射」。
`architecture.md` §1.1 已就方向给出明确指引（`IMC_Gameplay/Menu/Modal` 三层、
`AddMappingContext` 增删切换、AI 回合移除 `IMC_Gameplay`），但未以 ADR 形式固化，
`IA_` 粒度与 BindAction 宿主位置尚未裁定。

### Constraints

- **技术约束**：legacy `InputComponent->BindAction()`/`BindAxis()` 已弃用
  （deprecated-apis.md L14-16），UE 5.7 新项目须全量使用 Enhanced Input
- **平台约束**：目标 PC/Steam，鼠标点击为主；Gamepad Partial 支持须可后期追加映射
  无需改代码（technical-preferences 明文要求）
- **交互约束**：禁 hover-only（technical-preferences + architecture.md §1.2）——所有
  可操作元素须有显式点击/确认路径
- **架构约束**：呈现层纯叶子（HUD/卡牌 UI），不写游戏状态；IMC 切换须由逻辑层驱动
  （与 ADR-0007 BP-vs-C++ 边界联动）
- **宿主约束**：对局期服务宿主 = `UWorldSubsystem`（ADR-0001 Accepted）；跨局持久
  服务（菜单/存档/音频）= `UGameInstanceSubsystem`
- **测试约束**：`-nullrhi` headless 下 IMC 管理宿主须能初始化不 crash（门控
  回合/输入层 headless 测试）

### Requirements

- **FR-1**：每个独立交互意图对应一个具名 `IA_` 资产，Value Type 明确
  （`Digital`/`Axis2D`/`Axis1D`），Trigger/Modifier 在资产内配置不写死代码
- **FR-2**：IMC 按上下文分层，运行时经 `AddMappingContext`/`RemoveMappingContext`
  增删切换；优先级高的 IMC 截获输入后低优先级 IMC 不重复触发
- **FR-3**：AI 回合须从输入层杜绝 `IMC_Gameplay` 触发（移除 IMC，非 handler 内 if）
- **FR-4**：Modal 层（卡牌详情/确认弹窗）截获输入时 Gameplay 层 `IA_` 不触发
- **FR-5**：手柄追加仅需在对应 IMC 内增加 Gamepad 键位映射，**无需改 C++ 代码**
- **FR-6**：长按确认（Hold Trigger，时长可调）走 `IA` 资产内 `Hold` Trigger 配置
- **非功能 PR-1**：BindAction 宿主集中单一，不散落多 Actor；IMC 生命周期随对局
  World 创建/销毁，符合 ADR-0001 `UWorldSubsystem` 边界
- **非功能 PR-2**：input.md 已核验的 API，不依赖模型 ~5.3 训练记忆臆造

---

## Decision

### 选项对比与裁定

#### DECISION FORK A：IMC 分层策略

**选项 A1：三层 IMC + Priority 叠加（已裁定 Accepted）**

| IMC 资产 | Priority | 激活时机 | 覆盖意图 |
|---------|---------|---------|---------|
| `IMC_Gameplay` | 0（最低） | 对局中人类玩家回合；AI 回合**移除** | 掷骰/买地/建房/结束回合/棋盘平移缩放 |
| `IMC_Menu` | 10 | 主菜单/大厅/设置界面激活时 Add；退出时 Remove | 席位配置/开始游戏/返回/上下导航 |
| `IMC_Modal` | 20（最高） | Modal 弹窗（卡牌详情/确认框）激活时 Add；关闭时 Remove | 确认(`IA_Confirm`)/取消(`IA_Cancel`)/卡牌翻阅 |

- 代价：需在 WorldSubsystem 管理 Add/Remove 时机（首个 sprint 额外配置约 1 天）
- 收益：AI 越权从输入层杜绝（架构级，FR-3）；Modal 截获天然（FR-4）；手柄追加只改 IMC（FR-5）；与 architecture.md §1.1 已裁定方向完全一致

**选项 A2：单一 IMC + handler 内 if(bIsMyTurn)**

- 代价：违反 architecture.md §1.1 明确裁定（「移除 IMC_Gameplay，优于 handler 内 if」）；
  Modal 截获须每个 Widget 手动处理；手柄支持需改代码；技术债随交互增多指数增长
- 收益：首个 sprint 配置最少

**裁定：选 A1（三层 IMC Priority 叠加）**，与 architecture.md §1.1 已裁定方向一致。
**Fallback 渐进路**：首个交互 sprint 只建 `IMC_Gameplay` 单层起步，菜单 sprint 追加
`IMC_Menu`，Modal sprint 追加 `IMC_Modal`；三层资产彼此独立，Reversibility 高。

---

#### DECISION FORK B：BindAction 宿主位置

**选项 B1：`UWorldSubsystem`（`UBoardInputManager`）集中管理 IMC（已裁定 Accepted）**

- WorldSubsystem 在 `OnWorldBeginPlay` 获取 `PlayerController`，集中
  `AddMappingContext` 三层 IMC；同时持有所有 `TObjectPtr<UInputAction>` 资产引用
- 在对局状态机各阶段（ADR-0001 `ETurnPhase`）切换 Add/Remove `IMC_Gameplay`（AI 回合）
- 呈现层 BP（HUD/卡牌 UI）经 `GetSubsystem<UBoardInputManager>()` 调用 `SwitchToModal()`
  / `ReturnFromModal()`，符合 ADR-0007「BP 路由 → C++ 逻辑」
- IA 资产挂 `UPROPERTY(EditDefaultsOnly)` 由 Blueprint 子类（`BP_BoardInputManager`）在编辑器赋值

代价：首个 sprint 需新建一个 WorldSubsystem 子类（约 0.5 天）

收益：IMC 生命周期随 World 创建/销毁（ADR-0001 对局边界一致）；集中单一宿主便于测试
和排查；`-nullrhi` headless 下无 Pawn 也能初始化；无 Pawn 切换复杂度

**选项 B2：PlayerController 的 `SetupPlayerInputComponent` 内绑定**

- 标准角色游戏做法（input.md L86-99 范例），但本项目**无 Pawn 切换**
- IMC 管理需在 PlayerController BeginPlay 手写，与 ADR-0001 WorldSubsystem 宿主策略
  结构不一致（分散在两个宿主）
- AI 回合切换 IMC 须跨宿主调用，耦合增加

**选项 B3：分散于各 Widget（HUD/卡牌 UI BP）**

- 违反 ADR-0007「状态机/输入路由归 C++ 权威」原则，明确排除

**裁定：选 B1（`UBoardInputManager : UWorldSubsystem`）**，IMC 集中管理 + ADR-0001 宿主体系一致。

---

#### IA 粒度：每意图一 IA（固定）

回合制棋盘交互意图有限，每意图独立 `IA_` 资产（Digital/Axis2D），
**不共享 IA + 上下文区分**（避免同一 IA 在不同 IMC 内语义混淆，维护成本高）。

---

### Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  PLATFORM LAYER — Enhanced Input Plugin（UE 5.7 默认开启）          │
│  UEnhancedInputLocalPlayerSubsystem（per-LocalPlayer）             │
│  IA_* 资产（Input Action）  IMC_* 资产（Input Mapping Context）     │
└──────────────────────────────▲───────────────────────────────────┘
                                │ AddMappingContext / RemoveMappingContext
┌──────────────────────────────┴───────────────────────────────────┐
│  FOUNDATION — UBoardInputManager : UWorldSubsystem （C++）         │
│  ┌─ 持有 IA 资产引用 ──────────────────────────────────────────┐  │
│  │  TObjectPtr<UInputAction> IA_Roll                          │  │
│  │  TObjectPtr<UInputAction> IA_BuyProperty                  │  │
│  │  TObjectPtr<UInputAction> IA_BuildHouse                   │  │
│  │  TObjectPtr<UInputAction> IA_EndTurn                      │  │
│  │  TObjectPtr<UInputAction> IA_Confirm                      │  │
│  │  TObjectPtr<UInputAction> IA_Cancel                       │  │
│  │  TObjectPtr<UInputAction> IA_OpenMenu                     │  │
│  │  TObjectPtr<UInputAction> IA_BoardPan    // Axis2D        │  │
│  │  TObjectPtr<UInputAction> IA_BoardZoom   // Axis1D        │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌─ 持有 IMC 资产引用 + 管理增删 ─────────────────────────────┐  │
│  │  TObjectPtr<UInputMappingContext> IMC_Gameplay (P=0)       │  │
│  │  TObjectPtr<UInputMappingContext> IMC_Menu     (P=10)      │  │
│  │  TObjectPtr<UInputMappingContext> IMC_Modal    (P=20)      │  │
│  │  // OnWorldBeginPlay → AddMappingContext(IMC_Gameplay, 0)  │  │
│  │  // AI 回合 → RemoveMappingContext(IMC_Gameplay)            │  │
│  │  // SwitchToModal() → AddMappingContext(IMC_Modal, 20)     │  │
│  │  // ReturnFromModal() → RemoveMappingContext(IMC_Modal)    │  │
│  └────────────────────────────────────────────────────────────┘  │
│  ┌─ EnhancedInputComponent 绑定（BeginPlay 后取 PC）──────────┐  │
│  │  EIC->BindAction(IA_Roll, ETriggerEvent::Started, ...)     │  │
│  │  EIC->BindAction(IA_BuyProperty, ETriggerEvent::Started,…) │  │
│  │  // 长按：IA_BuildHouse 内配 Hold Trigger（资产层配置）     │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────▲───────────────────────────────────┘
                                │ 意图回调（handler 调用回合2/所有权6/建房8 受控写接口）
┌──────────────────────────────┴───────────────────────────────────┐
│  GAMEPLAY LAYER — 回合状态机(2) / 所有权(6) / 建房(8)               │
│  输入意图 → 调用对应系统受控写接口                                   │
│  （IMC_Gameplay 已被移除时，AI 回合 handler 根本不触发）             │
└──────────────────────────────────────────────────────────────────┘
                                │ 只读订阅（delegate）
┌──────────────────────────────┴───────────────────────────────────┐
│  PRESENTATION LAYER — HUD(16) / 卡牌 UI(17) / 主菜单(20)           │
│  BP 经 GetSubsystem<UBoardInputManager>() 调 SwitchToModal() 等    │
│  （纯叶子，不持输入状态，不写游戏状态）                              │
└──────────────────────────────────────────────────────────────────┘
```

**IMC 切换时序（AI 回合越权阻断）**：

```
人类回合开始（OnTurnStarted, bIsAI=false）
  → UBoardInputManager::ActivateGameplayInput()
  → AddMappingContext(IMC_Gameplay, 0)  // 允许掷骰/买地/建房/EndTurn

AI 回合开始（OnTurnStarted, bIsAI=true）
  → UBoardInputManager::DeactivateGameplayInput()
  → RemoveMappingContext(IMC_Gameplay)  // IA_Roll 等 handler 根本不触发
  → AI 同步决策（回合2 直接调 AI10 hook，无输入层参与）

Modal 弹窗打开（SwitchToModal，BP 经 GetSubsystem 调用）
  → AddMappingContext(IMC_Modal, 20)    // P=20 截获 Confirm/Cancel
  → IMC_Gameplay(P=0) 对应 IA_ 被高优先级覆盖不触发

Modal 弹窗关闭（ReturnFromModal）
  → RemoveMappingContext(IMC_Modal)     // 回到 IMC_Gameplay 可用状态
```

---

### Key Interfaces

```cpp
// =========================================================
// UBoardInputManager — 集中管理 IMC 切换的 WorldSubsystem
// =========================================================
UCLASS()
class UBoardInputManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ---- 生命周期 ----
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    // ---- IMC 切换公开接口（BP 经 GetSubsystem 调用） ----

    /** 人类回合开始时激活 Gameplay 输入层 */
    UFUNCTION(BlueprintCallable, Category="Input")
    void ActivateGameplayInput();

    /** AI 回合开始时从输入层移除 Gameplay IMC（越权阻断） */
    UFUNCTION(BlueprintCallable, Category="Input")
    void DeactivateGameplayInput();

    /** 打开 Modal 弹窗时叠加 Modal IMC（P=20 截获） */
    UFUNCTION(BlueprintCallable, Category="Input")
    void SwitchToModal();

    /** 关闭 Modal 弹窗时移除 Modal IMC */
    UFUNCTION(BlueprintCallable, Category="Input")
    void ReturnFromModal();

    /** 进入主菜单时激活 Menu IMC */
    UFUNCTION(BlueprintCallable, Category="Input")
    void ActivateMenuInput();

    /** 离开主菜单时移除 Menu IMC */
    UFUNCTION(BlueprintCallable, Category="Input")
    void DeactivateMenuInput();

protected:
    // ---- IA 资产引用（Blueprint 子类 BP_BoardInputManager 在编辑器赋值） ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_Roll;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_BuyProperty;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_BuildHouse;  // 资产内配 Hold Trigger（资产层）

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_EndTurn;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_Confirm;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_Cancel;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_OpenMenu;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_BoardPan;    // Value Type: Axis2D（鼠标拖拽/摇杆）

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Actions")
    TObjectPtr<UInputAction> IA_BoardZoom;   // Value Type: Axis1D（滚轮/扳机）

    // ---- IMC 资产引用 ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Contexts")
    TObjectPtr<UInputMappingContext> IMC_Gameplay;  // Priority = 0

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Contexts")
    TObjectPtr<UInputMappingContext> IMC_Menu;       // Priority = 10

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Contexts")
    TObjectPtr<UInputMappingContext> IMC_Modal;      // Priority = 20

private:
    // ---- BindAction 绑定（BeginPlay 后取 PlayerController） ----
    void BindAllActions(UEnhancedInputComponent* EIC);

    // ---- 意图 handler（转发至 gameplay 层） ----
    void Handle_Roll        (const FInputActionValue& Value);
    void Handle_BuyProperty (const FInputActionValue& Value);
    void Handle_BuildHouse  (const FInputActionValue& Value);
    void Handle_EndTurn     (const FInputActionValue& Value);
    void Handle_Confirm     (const FInputActionValue& Value);
    void Handle_Cancel      (const FInputActionValue& Value);
    void Handle_OpenMenu    (const FInputActionValue& Value);
    void Handle_BoardPan    (const FInputActionValue& Value);
    void Handle_BoardZoom   (const FInputActionValue& Value);

    // ---- 内部辅助 ----
    UEnhancedInputLocalPlayerSubsystem* GetEISubsystem() const;

    bool bGameplayInputActive = false;  // 防重复 Add/Remove
    bool bModalActive         = false;
};
```

**资产命名规范**：

| 资产类型 | 命名模式 | 示例 |
|---------|---------|------|
| Input Action | `IA_<意图>` | `IA_Roll`, `IA_BuyProperty`, `IA_BuildHouse`, `IA_EndTurn`, `IA_Confirm`, `IA_Cancel`, `IA_OpenMenu`, `IA_BoardPan`, `IA_BoardZoom` |
| Input Mapping Context | `IMC_<上下文>` | `IMC_Gameplay`, `IMC_Menu`, `IMC_Modal` |
| WorldSubsystem（C++） | `UBoardInputManager` | `BoardInputManager.h` / `.cpp` |
| WorldSubsystem（BP 子类） | `BP_BoardInputManager` | 编辑器内赋值 IA_*/IMC_* 资产引用 |

**资产存放路径**：

```
Content/
└── Input/
    ├── Actions/         ← IA_*.uasset
    └── Contexts/        ← IMC_*.uasset
src/
└── input/
    ├── BoardInputManager.h
    └── BoardInputManager.cpp
```

---

### Implementation Guidelines

1. **资产创建顺序**：先建 `IA_*`（Value Type + Trigger），再建 `IMC_*`（拖入 IA + 键位映射），最后建 `UBoardInputManager` C++ 类 + `BP_BoardInputManager` 赋值资产引用。

2. **Trigger / Modifier 配置在资产层，不在代码层**：
   - `IA_Roll`：无 Trigger（默认 `ETriggerEvent::Started` 足够）
   - `IA_BuildHouse`：`Hold` Trigger（`Hold Time = 0.5s`，可调 Tuning Knob）
   - `IA_BoardPan`：`Dead Zone` Modifier（阈值 `0.1`）防鼠标微抖
   - Gamepad 追加：只在 `IMC_Gameplay` 内为各 IA 追加 Gamepad 键位，**无需改 C++ handler**

3. **`ActivateGameplayInput` / `DeactivateGameplayInput` 调用时机**：
   - 由回合2（`UWorldSubsystem`，ADR-0001 宿主）在 `OnTurnStarted` handler 内
     根据 `bIsAI` 调用
   - 不由呈现层 HUD 调用（HUD 只读/只订阅）；HUD 可查询 `bGameplayInputActive` 决定
     按钮 Enable 状态显示，但不驱动 IMC 切换

4. **`SwitchToModal` / `ReturnFromModal` 调用时机**：
   - 由 CommonUI 激活栈（ADR-0012 裁定）在 Widget 激活/关闭时调用；若 ADR-0012 尚未
     Accepted，可由 HUD BP 经 `GetSubsystem<UBoardInputManager>()` 显式调用（fallback）

5. **headless 测试注意**：`-nullrhi` 下 `GetLocalPlayer()` 可能返回 null；
   `GetEISubsystem()` 须做 null guard，返回 null 时 `UE_LOG` 警告并早返，
   不 crash——这是 IMC 宿主迁移至 WorldSubsystem 的关键验证点

6. **Alpha / Full Vision 预留**：
   - `IMC_PostRollAction`（Alpha：交易/拍卖弹窗）可追加为第四层 IMC，不改既有结构
   - 联网（Full Vision #25）：本地 PlayerController 仍持有 Enhanced Input；服务器端
     不运行 IMC（服务器无输入）；本 ADR 结构与联网兼容，无需变更

---

## Alternatives Considered

### Alternative 1：单一 IMC + handler 内 if(bIsMyTurn)

- **Description**：所有 IA 放一个 `IMC_Default`，handler 内用 `if(bIsMyTurn)` 和
  `if(!bIsModal)` 过滤
- **Pros**：首个 sprint 配置工作量最小（半天）
- **Cons**：违反 architecture.md §1.1 明确裁定；AI 越权防线降为代码层（单点失效）；
  Modal 截获须每个弹窗手动处理；手柄追加需改代码逻辑；随交互数增长维护成本指数上升
- **Estimated Effort**：首次更小，中后期更大（技术债）
- **Rejection Reason**：与 architecture.md §1.1 已裁定方向直接矛盾，且违反 FR-3/FR-5

### Alternative 2：PlayerController `SetupPlayerInputComponent` 内绑定

- **Description**：标准 UE 角色游戏做法，在 PC 的 `SetupPlayerInputComponent` 内
  建立 `BindAction`，IMC 也由 PC BeginPlay 管理
- **Pros**：与 input.md 范例代码（L76-99）直接对齐，学习成本低
- **Cons**：对局 IMC 管理与 ADR-0001 `UWorldSubsystem` 宿主策略分裂（两个宿主各管一部分）；
  AI 回合切换 IMC 须跨宿主耦合调用；无 Pawn 时 `SetupPlayerInputComponent` 触发时机
  与对局 World 初始化时序不一致；回合制无 Pawn 切换，PC 绑定收益有限
- **Estimated Effort**：相近
- **Rejection Reason**：宿主分裂增加跨系统耦合，与 ADR-0001 宿主集中策略不一致

### Alternative 3：分散在各 Widget（HUD BP / 卡牌 UI BP）BindAction

- **Description**：HUD 订阅掷骰点击，卡牌 UI 订阅建房点击，各自独立 AddMappingContext
- **Pros**：开发各 Widget 时就近绑定，最局部
- **Cons**：直接违反 ADR-0007「状态机/输入路由归 C++ 权威」；BP Widget 大量 Input 逻辑
  无法 headless 测试；重复 IMC Add 产生优先级冲突；绑定散乱无法统一 AI 回合越权阻断
- **Estimated Effort**：单 Widget 最小，跨 Widget 协调最大
- **Rejection Reason**：违反 ADR-0007 BP-vs-C++ 边界，明确排除

---

## Consequences

### Positive

- IMC 分层 + 集中宿主：AI 越权从输入层架构级阻断（无单点漏洞）
- Gamepad 支持：只需在 IMC_* 内追加键位映射，**零代码变更**（technical-preferences 要求满足）
- Modal 截获：高优先级 IMC_Modal(P=20) 天然截获输入，无需每个弹窗手动 flag
- BindAction 集中：调试/排查输入问题有单一入口，不用 grep 多个 Actor
- 与 ADR-0001/0007 一致：宿主选择 WorldSubsystem、C++ 权威管理输入路由

### Negative

- 首个 sprint 须新建 `UBoardInputManager` WorldSubsystem 类（约 0.5 天额外工作）
- WorldSubsystem 内需订阅回合2 的 `OnTurnStarted` 事件以切换 IMC，引入跨系统订阅
  （但这是已有事件总线 ADR-0003 的正常使用，非新耦合）
- `-nullrhi` headless 下须 null guard `GetLocalPlayer()`，否则测试 crash

### Neutral

- IA 资产数量（9 个）比单一 IA 分组更多，但命名清晰、资产粒度合理
- `BP_BoardInputManager` Blueprint 子类需由 Project Settings 指定为 Subsystem 使用
  的类（`DefaultWorldSubsystems` 配置），首次配置需注意

---

## Risks

| 风险 | 概率 | 影响 | 缓解措施 |
|------|-----|------|---------|
| `UEnhancedInputLocalPlayerSubsystem` 在 `-nullrhi` headless 下 null（无 LocalPlayer） | 中 | 中（测试 crash） | `GetEISubsystem()` null guard + UE_LOG 警告早返；IMC 切换逻辑走 fail-silent，不阻塞游戏逻辑 |
| IMC 优先级截获语义在 5.4–5.7 之间有变更 | 低 | 高（Modal 无法截获） | engine-ref input.md L162-181 已核验多 Context 模式；实机测试 VERIFICATION REQUIRED ② |
| `BP_BoardInputManager` Blueprint 子类忘记在 Project Settings 注册导致 WorldSubsystem 未创建 | 低 | 中（输入全失效） | 实现时在 Implementation Guidelines 加 `ShouldCreateSubsystem` override，若依赖资产为 null 早返 + fatal log |
| Alpha 阶段新增交互意图（交易/拍卖）需新 IMC 层 | 中 | 低（结构已预留） | 已在 Implementation Guidelines §6 预留 `IMC_PostRollAction` 第四层位置，追加无需修改既有层 |

---

## Performance Implications

| 指标 | 当前 | 预期 | 预算 |
|------|-----|------|------|
| CPU（帧时间，输入轮询） | 0 ms（无实现） | < 0.1 ms | 16.6 ms |
| 内存（IA + IMC 资产） | 0 | < 0.5 MB（9 IA + 3 IMC 轻量资产） | — |
| 加载时间（IA/IMC 资产） | — | 热加载（引擎启动时随 Enhanced Input 插件载入） | — |
| 网络 | N/A（单机 MVP） | N/A | — |

Enhanced Input 资产（IA + IMC）是引擎轻量数据资产，不构成性能瓶颈。
回合制棋盘 `AddMappingContext`/`RemoveMappingContext` 调用频率极低（每回合开始/结束各一次），
无帧内 Hot Path 压力。

---

## Migration Plan

初次实现（无现有 input 代码，无迁移成本）：

1. **Sprint 1 Step 1**（约 0.5 天）：建 `UBoardInputManager` C++ 类 + `BP_BoardInputManager`；
   在 Project Settings `DefaultWorldSubsystems` 注册；`OnWorldBeginPlay` null guard 验证。
2. **Sprint 1 Step 2**（约 0.5 天）：建 `IA_Roll` / `IA_BuyProperty` / `IA_EndTurn` /
   `IA_Confirm` / `IA_Cancel` 五个首批 Digital IA 资产；建 `IMC_Gameplay`；
   `BindAllActions` 绑定首批 handler；跑 `-nullrhi` smoke check（null guard 不 crash）。
3. **Sprint 菜单**（约 0.5 天）：追加 `IA_OpenMenu` + `IMC_Menu`；接入主菜单(20) BP 流。
4. **Sprint 卡牌 UI**（约 0.5 天）：追加 `IMC_Modal`；接入 `SwitchToModal`/`ReturnFromModal`；
   验证 VERIFICATION REQUIRED ②。
5. **Sprint 棋盘视角**（约 0.5 天）：追加 `IA_BoardPan`（Axis2D + Dead Zone Modifier）+
   `IA_BoardZoom`（Axis1D）；接入棋盘平移/缩放逻辑。
6. **Alpha 手柄支持**（约 1 天）：在 `IMC_Gameplay`/`IMC_Menu`/`IMC_Modal` 内追加
   Gamepad 键位映射；**无需改 C++ 代码**——验证 FR-5。

**Rollback plan**：本 ADR 尚无实现，无需 Rollback。若 Accepted 后实现发现 IMC 分层
无法满足需求（极低概率），可退化为 Alternative 1（单 IMC + handler if）；但须修改
architecture.md §1.1 对应裁定节，并经 technical-director 重新 gate。

---

## Validation Criteria

- [ ] `IA_Roll` 仅在 `IMC_Gameplay` 激活（人类回合）时触发，AI 回合移除 IMC 后不触发（FR-3 验证）
- [ ] `IMC_Modal`(P=20) 激活时，`IMC_Gameplay`(P=0) 的 `IA_BuyProperty` 等不触发（FR-4 验证）
- [ ] 追加 Gamepad 键位映射后，handler C++ 代码零修改（FR-5 验证）
- [ ] `-nullrhi` headless 下 `UBoardInputManager::OnWorldBeginPlay` 不 crash（null guard 验证）
- [ ] `IA_BuildHouse` 配 `Hold` Trigger 后，短按不触发 handler，长按（≥0.5s）触发（Hold Trigger 验证）
- [ ] `AddMappingContext`/`RemoveMappingContext` 在帧内调用不产生可测量帧时间峰值（> 0.5 ms）

---

## GDD Requirements Addressed

| GDD 文档 | 系统 | 需求 | 本 ADR 满足方式 |
|---------|------|------|---------------|
| `design/gdd/player-turn.md` | 玩家与回合(2) | 人类玩家在 `RollPhase` 点击掷骰、`ResolvePhase` 点击买地/不买、`PostRollAction` 发送结束回合信号（§UI 节「HUD/输入层处理后回调本系统 EndTurn 接口」） | `IA_Roll`/`IA_BuyProperty`/`IA_EndTurn` 资产 + `IMC_Gameplay`；`Handle_Roll` 等 handler 转发意图至回合2 受控接口 |
| `design/gdd/player-turn.md` | 玩家与回合(2) | AI 回合须从输入层杜绝越权（player-turn CR-8 AI 契约：AI 走同步 hook，人类走 UI 意图，两路严格分离） | `DeactivateGameplayInput()` 在 AI 回合移除 `IMC_Gameplay`，`IA_Roll` 等 handler 根本不触发，架构级阻断 |
| `design/gdd/building-upgrade.md` | 建房升级(8) | 玩家在 `PostRollAction` 阶段通过 UI 发起建房意图 | `IA_BuildHouse`（Hold Trigger 防误触）+ `Handle_BuildHouse` 转发至建房(8) `BuildHouse` 受控接口 |
| `design/gdd/main-menu-lobby.md` | 主菜单与大厅(20) | 席位配置、"开始游戏"按钮、颜色/难度选择（AC-21 按钮可用性绑定）—— 菜单交互须独立输入上下文，不干扰对局 | `IMC_Menu`(P=10)；大厅期间 `ActivateMenuInput()` 激活，进入对局后 `DeactivateMenuInput()` 移除，两层互不干扰 |
| `design/gdd/property-card-ui.md` | 地产卡 UI(17) | 卡牌详情弹窗确认/取消操作；弹窗期间不触发 Gameplay 层输入 | `IMC_Modal`(P=20)；`SwitchToModal()` 叠加高优先级截获输入；`IA_Confirm`/`IA_Cancel` 专属 Modal 层绑定 |
| `.claude/docs/technical-preferences.md` | 全项目 | 使用 UE Enhanced Input 管理输入映射；禁 hover-only；Gamepad Partial 支持须可后期追加零代码修改 | 全量 Enhanced Input 架构；禁 hover-only（所有 IA 为 Digital 点击/确认，无纯悬停路径）；IMC 内追加 Gamepad 键位零代码变更 |
| `docs/architecture/architecture.md` §1.1 | 引擎集成（Input 域） | `IMC_Gameplay`/`IMC_Menu`/`IMC_Modal` 三层；`AddMappingContext` 增删切换；AI 回合移除 `IMC_Gameplay` 优于 handler 内 if | 本 ADR 完整固化 §1.1 方向性裁定为可执行架构决策 |
| `docs/engine-reference/unreal/deprecated-apis.md` L14-16 | 引擎约束 | 禁 legacy `BindAction`/`BindAxis`/`GetInputAxisValue` | 本 ADR 全量使用 `EnhancedInputComponent->BindAction`；Implementation Guidelines + Key Interfaces 均无 legacy API |

---

## Related

- ADR-0001（宿主决策）— 本 ADR 的 `UBoardInputManager` 宿主选 `UWorldSubsystem`，
  依赖 ADR-0001 Accepted
- ADR-0007（BP-vs-C++ 边界）— IMC 切换归 C++ WorldSubsystem，BP Widget 只调公开接口；
  须 ADR-0007 Accepted 后核对边界一致性
- ADR-0012（CommonUI 输入路由与激活栈）— Modal IMC 激活/关闭时机须与 CommonUI
  `CommonActivatableWidget` 激活栈协作，ADR-0012 须参照本 ADR §Modal IMC 时序
- `docs/engine-reference/unreal/modules/input.md`（5.7 Enhanced Input API 核验依据）
- `docs/engine-reference/unreal/deprecated-apis.md`（L14-16 legacy 弃用依据）
