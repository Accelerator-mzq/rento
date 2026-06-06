# ADR-0008: HUD 动画驱动机制与 IGameClock DI

## Status

Accepted

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

msc（用户）+ Technical Director + ue-umg-specialist — 2026-06-06 Accepted（用户裁定全接受推荐选型：counter NativeTick + HUD 状态机独立 UObject + IGameClock DI + BP 绑定显式解绑）

## Summary

HUD 的 counter 动画（F-1/F-2）与 AI 行动过期判定（F-4）要求**运行时逐帧动态求值**，不可用烘焙固定时长的 `UWidgetAnimation`；同时，AC-12/AC-24a/AC-30/AC-57 的 headless 可测性要求时钟依赖可注入。本 ADR 裁定：counter 动画由 **NativeTick（C++ 侧，每帧驱动）** 实现；HUD 状态机抽独立 `UObject`（非 `UUserWidget`）作为逻辑承载体；时钟依赖经 **`IGameClock` 接口注入**，支持 mock 做 headless 确定性测试；BP `Bind Event` 订阅须显式解绑防双注册。

---

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | UI |
| **Knowledge Risk** | HIGH — CommonUI、UMG NativeTick 在 5.7 的精确行为超 LLM ~5.3 训练截止；`IGameClock` 为项目自定义接口 |
| **References Consulted** | `docs/engine-reference/unreal/modules/ui.md`、`docs/engine-reference/unreal/current-best-practices.md`、`docs/engine-reference/unreal/deprecated-apis.md`、`docs/engine-reference/unreal/VERSION.md` |
| **Post-Cutoff APIs Used** | `UCommonActivatableWidget`（CommonUI 5.7 稳定）；`NativeTick` 在 `UUserWidget` 的行为（pre-5.3 稳定，5.7 无已知破坏性变更）；`meta=(BindWidget)` C++/BP 绑定（pre-5.3 稳定）；`DECLARE_DYNAMIC_MULTICAST_DELEGATE`（pre-5.3 稳定）。⚠ `FSlateFontInfo` tnum feature tag 暴露情况在 5.7 **未见** release note 变化（见 hud.md CR-3 注解），实现期须 ue-umg-specialist 核验 |
| **Verification Required** | ① 确认 5.7 `UUserWidget::NativeTick` 在有 Viewport 的非 -nullrhi 模式下每帧触发；② 确认 `-nullrhi` 模式下 `NativeTick` **不触发**（此即本 ADR 抽独立 UObject 的根因，须 spike 验证）；③ 确认 `UCommonActivatableWidget` 在 5.7 CommonUI 插件中 API 无重大变化（重点：`NativeOnActivated`/`NativeOnDeactivated` 签名） |

> **Note**: Knowledge Risk = HIGH。本 ADR 的 Engine Compatibility 节须在 Pre-Production sprint 开工前由 ue-umg-specialist 对照 5.7 引擎源码核验上述三项，任一验证失败须修订本 ADR 并重新 Accepted。

---

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001（UObject 宿主生命周期，HUD 状态机 UObject 宿主由此 ADR 定义的 World 边界 Subsystem 初始化）；ADR-0007（BP-vs-C++ 边界，本 ADR 是其在 HUD 域的具体落地） |
| **Enables** | ADR-0012（CommonUI 输入路由，须 HUD 状态机架构已定才能裁 CommonActivatableWidget 激活栈与输入焦点路由） |
| **Blocks** | HUD(#16) 实现 Story；地产卡 UI(#17) 实现 Story（共享 BP 绑定解绑纪律）；AC-12/AC-24a/AC-30/AC-57 的 [Logic·门控] 测试关闭（依赖 IGameClock DI + 状态机独立 UObject 产出） |
| **Ordering Note** | ADR-0001/0007 须先 Accepted；本 ADR 可与 ADR-0006（GameStateSnapshot）并行起草，二者不触同文件 |

---

## Context

### Problem Statement

HUD GDD（hud.md）为 AC-12/AC-24a/AC-30/AC-57 打了如下门控标注：

> **headless 前提 OQ-HUD-2 ADR 产出状态机抽独立 UObject，否则 UMG NativeTick 在 -nullrhi 不触发 → false-green**

同时 hud.md F-2 精确排除理由指出：

> **UMG sequencer 无整数文本插值轨道，滚动数字必须每帧 `SetText(FText::AsNumber(round(displayed(t))))` ——本质即 NativeTick/Timeline 逐帧驱动；即便用 `PlaybackSpeed` 缩放 `UWidgetAnimation` 改变有效时长仍不可行。**

这意味着：
1. counter 动画（F-1/F-2）**必须**走逐帧驱动机制，无法用 `UWidgetAnimation` 烘焙实现。
2. HUD 时序逻辑（F-3 高亮时序、F-4 AI 行动过期判定中的 `unpaused_elapsed`）依赖游戏时钟，而 `GetWorld()->GetTimeSeconds()` 在 `-nullrhi` 无 World tick 驱动时行为不确定，不可 headless 测试。
3. BP `Bind Event to [Delegate]` 底层是 `FMulticastScriptDelegate::Add`（非 `AddUnique`），读档重绑若不先显式解绑将导致重复触发（hud.md EC-11/AC-48 R-3 裁定）。

若不解决上述三点，AC-12（高亮逻辑态转换）、AC-24a（Pass'N Play 不叠帧）、AC-30（F-4 过期判定四变体）、AC-57（reduce_motion 配置参数读回）全部**无法在 `-nullrhi` headless 模式下真正验绿**——它们的 `[Logic·门控]` 标签将永久失效，只能靠 Advisory 截图覆盖，测试质量大幅降级。

### Current State

当前项目无任何 HUD 实现文件（仍在设计阶段）。本 ADR 在**实现开始前**裁定架构边界，防止工程师走错路（如直接在 `UUserWidget::NativeTick` 中写时序逻辑，导致 headless 测试全部 false-green）。

### Constraints

- **引擎约束**：`UUserWidget::NativeTick` 在 `-nullrhi` 模式下**不触发**（无渲染管线驱动 Widget Tick），HUD 时序逻辑若住在 Widget 内无法 headless 测试。
- **逐帧插值约束**：counter 动画（F-1/F-2）须每帧调 `SetText(FText::AsNumber(round(displayed(t))))`，`UWidgetAnimation`（sequencer 轨道）无整数文本插值轨道，无法用 `PlaybackSpeed` 变速代替。
- **Blueprint 为主**：项目采用 Blueprint 为主策略（ADR-0007），HUD Widget 树/布局/动画资产在 WBP_ 内编辑；但**时序逻辑/状态机**须在 C++ 中可测。
- **存档重绑约束**：BP `Bind Event`（`FMulticastScriptDelegate::Add`）非幂等，读档重绑须先 `Unbind Event from` 解绑（hud.md EC-11 R-3）。
- **平台约束**：PC/Steam 主目标，手柄适配预留（CommonUI 为此选型，ADR-0012 裁）。

### Requirements

- **R1**：counter 动画（F-1/F-2）在运行时逐帧动态求值（时长随 ΔCash 变），终点精确等于 `NewCash`。
- **R2**：HUD 时序逻辑（高亮 onset/complete F-3；AI 行动 `unpaused_elapsed` F-4；reduce_motion 切换 AC-57）须能注入 mock 时钟，在 `-nullrhi` headless 做确定性断言。
- **R3**：HUD 状态机（面板 Normal/Active/Bankrupt；全局 InGame/GameOver；AI 行动播放 Idle/Playing）住在**独立 UObject**（非 UUserWidget），可在无渲染环境下实例化并驱动。
- **R4**：BP `Bind Event` 订阅在读档重绑时须先显式解绑，防重复触发（AC-48）。
- **R5**：Widget 树/布局/动画资产保留在 WBP_ Blueprint 内，C++ 状态机通过 `meta=(BindWidget)` 持有控件引用并设值（保持 BP 为主策略）。
- **R6（性能）**：HUD 帧预算目标 < 2ms；NativeTick 仅在动画活跃期消耗（静止态跳过插值计算）。

---

## Decision

### 核心裁定（三项）

**裁定 1 — counter 动画驱动：NativeTick（C++ 侧）**

选 **`UUserWidget::NativeTick`（由 C++ 基类 override）** 逐帧驱动 counter 插值，在帧回调内调 `SetText(FText::AsNumber(round(displayed(t))))`。

> 注：NativeTick 运行在有渲染的游戏进程中（PIE / Shipping），不在 `-nullrhi` headless 中触发——因此 counter 动画本身标 `[Advisory]`（截图/录帧验证），这是已知设计取舍，符合 hud.md 的 AC 分类。

**裁定 2 — HUD 状态机：独立 `UHudStateMachine`（C++ UObject）+ `IGameClock` DI**

把所有**可 headless 测试的逻辑**（面板状态机转换、`E_cur` 回合代次计数、`unpaused_elapsed` 累计、F-4 过期判定、F-1 时长公式、F-5 beat 生命周期）抽入独立 `UHudStateMachine : public UObject`。该类：
- 不继承 `UUserWidget`，不持 Slate 引用，可在 `-nullrhi` 下实例化；
- 通过 `IGameClock` 接口取游戏时间（生产注入 `FWorldGameClock`，测试注入 `FMockGameClock`）；
- Widget（`UWB_HUD : public UCommonActivatableWidget`）持有 `UHudStateMachine*`，在 `NativeConstruct` 创建/注入，在 `NativeTick` 中调用 `StateMachine->Tick(DeltaTime)` 并读取输出状态更新控件显示值。

**裁定 3 — BP 事件绑定：显式解绑纪律**

任何 BP/C++ 对 owner 系统 `DYNAMIC_MULTICAST_DELEGATE` 的订阅，须遵守：
- **C++ 路径**：`AddDynamic`（宏展开为 `AddUnique`，按 UObject+FName 去重，无双绑风险）。读档时仍推荐 `RemoveDynamic` 后 `AddDynamic`，防止 Widget 重建导致旧实例残留。
- **BP 路径**：`Bind Event to [Delegate]`（非 `AddUnique`）。读档重绑**必须**先 `Unbind Event from [Delegate]` 解绑，再重绑（hud.md EC-11 R-3/AC-48）。code-review 须核验此路径（AC-48 [Integration]）。

---

### Architecture

```
┌───────────────────────────────────────────────────────────────────────┐
│  PRESENTATION LAYER — HUD (#16)                                        │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  WBP_HUD (UWB_HUD : UCommonActivatableWidget)  [Blueprint 树]     │  │
│  │   ├─ UPROPERTY(meta=(BindWidget))  UTextBlock* CashText_P1..P4   │  │
│  │   ├─ UPROPERTY(meta=(BindWidget))  UTextBlock* PhaseLabel        │  │
│  │   ├─ UPROPERTY(meta=(BindWidget))  UOverlay*   BannerContainer   │  │
│  │   ├─ TObjectPtr<UHudStateMachine>  StateMachine   ← 裁定2        │  │
│  │   │                                                              │  │
│  │   │  NativeConstruct():                                          │  │
│  │   │    StateMachine = NewObject<UHudStateMachine>(this)          │  │
│  │   │    StateMachine->Init(InjectGameClock(), ...)                │  │
│  │   │    BindDelegates()   ← 显式绑定 owner multicast delegates    │  │
│  │   │                                                              │  │
│  │   │  NativeTick(DeltaTime):                                      │  │
│  │   │    StateMachine->Tick(DeltaTime)   ← 推进所有动画/时序       │  │
│  │   │    ApplyStateMachineOutput()       ← SetText/SetColorAndOpacity │  │
│  │   │                                                              │  │
│  │   │  OnGameLoaded() [EC-11 重绑]:                                │  │
│  │   │    UnbindDelegates()  ← 先全部解绑                          │  │
│  │   │    RebindDelegates()  ← 再重绑                              │  │
│  │   │    StateMachine->RebuildFromSnapshot(Snapshot)               │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                          │ owns                                         │
│  ┌───────────────────────▼──────────────────────────────────────────┐  │
│  │  UHudStateMachine : public UObject   [C++ 权威逻辑，-nullrhi 可测] │  │
│  │                                                                   │  │
│  │  依赖注入:                                                         │  │
│  │    IGameClock* Clock  ← 生产: FWorldGameClock(GetWorld())        │  │
│  │                         测试: FMockGameClock(手动推帧)           │  │
│  │                                                                   │  │
│  │  面板状态机 (per-panel, AC-12/13/15/16):                          │  │
│  │    EPanelState { Normal, Active, Bankrupt }                       │  │
│  │    规则: Active 互斥; Bankrupt 终态; GameOver 冻结逻辑态         │  │
│  │                                                                   │  │
│  │  Counter 动画 (F-1/F-2, AC-3~9):                                 │  │
│  │    struct FCounterAnim { OldCash, NewCash, T, t_elapsed }        │  │
│  │    Tick(): displayed(t) = 插值(ease_out); 静止态 early-return    │  │
│  │                                                                   │  │
│  │  回合代次 (F-4, AC-30):                                           │  │
│  │    int32 E_cur   每收 OnTurnStarted +1                            │  │
│  │    float unpaused_elapsed  经 IGameClock 取游戏时间(暂停不计)     │  │
│  │    bool expired(i) = (E_cur > E_action(i)) || elapsed > T_stale  │  │
│  │                                                                   │  │
│  │  Banner 队列 (F-5, AC-34~38):                                     │  │
│  │    TArray<FBannerEntry> Banners; k 槽管理; N_max 驱逐             │  │
│  │                                                                   │  │
│  │  reduce_motion (AC-57):                                           │  │
│  │    bool bReduceMotion  从 DA_HudConfig 读; 旁路 T_min 下界        │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │  IGameClock  [C++ 纯虚接口]                                        │  │
│  │    virtual float GetGameTimeSec() = 0;  // 游戏时间,暂停不计      │  │
│  │    virtual float GetUnpausedTimeSec() = 0;                        │  │
│  │                                                                   │  │
│  │  FWorldGameClock : IGameClock   (生产实现)                        │  │
│  │    GetGameTimeSec() → GetWorld()->GetTimeSeconds()                │  │
│  │                                                                   │  │
│  │  FMockGameClock : IGameClock    (测试实现)                        │  │
│  │    float SimTime = 0;                                             │  │
│  │    void AdvanceTime(float dt) { SimTime += dt; }                  │  │
│  │    GetGameTimeSec() → SimTime                                     │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────────┘

  数据流 (单向，呈现层纯叶子):
  owner delegate 广播
    │  OnTurnStarted / OnPhaseChanged / OnCashChanged / OnBankruptcyDeclared
    │  OnBuildingAnnounced / OnAIActionExecuted / OnGameWon / OnGameLoaded
    ▼
  WBP_HUD.HandleXxx()  [UFUNCTION, AddDynamic]
    │  将 payload 传给 StateMachine
    ▼
  UHudStateMachine.OnXxx(payload)  [C++ 方法]
    │  更新内部逻辑态
    │  [不直接写 Widget 控件]
    ▼
  WBP_HUD.NativeTick → ApplyStateMachineOutput()
    │  读 StateMachine 输出态
    ▼
  SetText / SetColorAndOpacity / SetVisibility  [Slate 写操作,仅在此处]
```

### Key Interfaces

```cpp
// ─── IGameClock — 时钟 DI 接口 ───────────────────────────────────────
// 文件: src/ui/hud/IGameClock.h
class IGameClock
{
public:
    virtual ~IGameClock() = default;
    // 返回游戏运行时间(秒),暂停期间不累计
    virtual float GetGameTimeSec() const = 0;
    // 返回未暂停的累计时间(与 GetGameTimeSec 一致,暂停不计)
    virtual float GetUnpausedTimeSec() const = 0;
};

// 生产实现 (依赖 UWorld::GetTimeSeconds)
// 文件: src/ui/hud/FWorldGameClock.h
class FWorldGameClock final : public IGameClock
{
public:
    explicit FWorldGameClock(UWorld* InWorld) : World(InWorld) {}
    float GetGameTimeSec() const override { return World ? World->GetTimeSeconds() : 0.f; }
    float GetUnpausedTimeSec() const override { return World ? World->GetTimeSeconds() : 0.f; }
private:
    TWeakObjectPtr<UWorld> World;
};

// 测试 Mock
// 文件: tests/unit/hud/FMockGameClock.h
class FMockGameClock final : public IGameClock
{
public:
    float SimTime = 0.f;
    void AdvanceTime(float Dt) { SimTime += Dt; }
    float GetGameTimeSec() const override { return SimTime; }
    float GetUnpausedTimeSec() const override { return SimTime; }
    // 读档重置: 模拟 EC-11 unpaused_elapsed 清零
    void ResetOnLoad() { SimTime = 0.f; }
};

// ─── UHudStateMachine — HUD 逻辑状态机 ─────────────────────────────
// 文件: src/ui/hud/HudStateMachine.h
UCLASS()
class UHudStateMachine : public UObject
{
    GENERATED_BODY()

public:
    // 初始化: Widget 在 NativeConstruct 调用
    void Init(IGameClock* InClock, int32 InPlayerCount);

    // 每帧驱动 (由 WBP_HUD::NativeTick 调)
    void Tick(float DeltaTime);

    // 读档重建 (EC-11: 直接设最终态,不回放历史动画)
    void RebuildFromSnapshot(const FHudSnapshot& Snapshot);

    // ── 事件处理入口 (owner delegate 订阅回调转发到此) ──
    void OnTurnStarted(const FTurnStartedInfo& Info);      // E_cur +1
    void OnTurnEnded(const FTurnEndedInfo& Info);
    void OnPhaseChanged(const FPhaseChangedInfo& Info);
    void OnCashChanged(int32 PlayerId, int32 OldCash, int32 NewCash, EChangeReason Reason);
    void OnBankruptcyDeclared(int32 Debtor, int32 Creditor);
    void OnBuildingAnnounced(int32 TileIndex, int32 NewHouseCount, int32 ActingPlayerId);
    void OnAIActionExecuted(const FAIActionDetails& Details);
    void OnGameWon(int32 WinnerId);

    // ── 输出查询 (WBP_HUD::ApplyStateMachineOutput 读) ──
    EPanelState GetPanelState(int32 PlayerId) const;
    int32       GetDisplayedCash(int32 PlayerId) const;   // counter 插值当前帧值
    EChangeReason GetCashAnimReason(int32 PlayerId) const; // 当前着色语义
    bool        IsCashAnimating(int32 PlayerId) const;
    int32       GetTurnEpoch() const { return E_cur; }
    bool        IsActionExpired(int32 ActionIndex, int32 ActionEpoch) const; // F-4
    const TArray<FBannerEntry>& GetActiveBanners() const;
    bool        IsReduceMotion() const { return bReduceMotion; }

private:
    IGameClock*   Clock    = nullptr;  // 注入,不 UPROPERTY(GC 由 Widget 持有)
    int32         E_cur    = 0;        // HUD 回合代次 (F-4 主判据)
    bool          bReduceMotion = false;

    // 面板状态 (per-player)
    TArray<EPanelState>    PanelStates;   // 下标 = PlayerId
    TArray<FCounterAnim>   CounterAnims;  // counter 动画状态

    // AI 行动队列
    TArray<FAIActionQueueEntry> ActionQueue;

    // Banner 队列
    TArray<FBannerEntry>   Banners;

    // 全局态
    bool bGameOver = false;
};

// ─── WBP_HUD 基类 (C++ 侧) ──────────────────────────────────────────
// 文件: src/ui/hud/WB_HUD.h
UCLASS()
class UWB_HUD : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // ── BindWidget 引用 (与 WBP_HUD Blueprint 内控件同名) ──
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CashText_P0;  // 依玩家数追加 P1..P3

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> PhaseLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UOverlay> BannerContainer;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY()
    TObjectPtr<UHudStateMachine> StateMachine;  // UPROPERTY 保护 GC

    // owner delegate 绑定/解绑
    void BindDelegates();
    void UnbindDelegates();

    // 每帧将状态机输出写入 Slate 控件
    void ApplyStateMachineOutput();

    // ── 事件回调 (AddDynamic 注册) ──
    UFUNCTION()
    void HandleTurnStarted(const FTurnStartedInfo& Info);
    UFUNCTION()
    void HandleCashChanged(int32 PlayerId, int32 Old, int32 New, EChangeReason Reason);
    UFUNCTION()
    void HandleBankruptcyDeclared(int32 Debtor, int32 Creditor);
    UFUNCTION()
    void HandleBuildingAnnounced(int32 TileIndex, int32 NewCount, int32 ActingPlayer);
    UFUNCTION()
    void HandleAIActionExecuted(const FAIActionDetails& Details);
    UFUNCTION()
    void HandleGameWon(int32 WinnerId);
    UFUNCTION()
    void HandleGameLoaded();  // EC-11 读档重建

    // 读档重建路径
    void RebuildFromSnapshot();
};
```

### Implementation Guidelines

1. **UHudStateMachine 不继承 UUserWidget，不持 Slate 控件引用。** 它只持数据（EPanelState、FCounterAnim 等），对 Widget 控件的写操作**全部**在 `ApplyStateMachineOutput()` 内，由 Widget 调用。

2. **IGameClock 注入时机**：`WB_HUD::NativeConstruct` 中创建 `UHudStateMachine`，同时创建 `FWorldGameClock` 并注入。测试中改为注入 `FMockGameClock`。`IGameClock*` 是裸指针（非 `UPROPERTY`），生命周期由 Widget 负责（Widget 被 GC 前 StateMachine 先被销毁），实现需注意 `NativeDestruct` 中清理。

3. **NativeTick 优化**：`UUserWidget` 默认不 Tick，须在构造或 `NativeConstruct` 中设 `bCanEverTick = true`（蓝图侧也可在 Widget Blueprint Class Settings 开启）。**静止态（无活跃 Counter / Banner / AI 队列）时，StateMachine::Tick 做 early-return**，不做插值计算，降低无动画时的 Tick 开销。

4. **BP 绑定解绑路径（核心防 bug）**：
   - **推荐**：事件处理在 C++ 中用 `AddDynamic`（自动去重）。BP 仅做 Widget 树配置，不在 BP 中做 `Bind Event`。
   - **若 BP 绑定不可避免**：须配套 `Unbind Event from`，在 `NativeDestruct` 与 `HandleGameLoaded`（读档）时各调一次。code-review 须核验此路径（AC-48）。

5. **reduce_motion 参数读取**：从 `DA_HudConfig`（Primary Data Asset）读取 `bReduceMotion`，**不硬编码**。`UHudStateMachine::Init` 时读入。reduce_motion=On 时：
   - counter 动画旁路 F-1 的 `T_min` 下界，有效时长可低至 `T_min_reduce=0.0`（≤200ms 快过），AC-57① 可 PASS；
   - 高亮动画去超冲（瞬切或线性短帧，AC-57②）；
   - banner 入/出场 `T_slide_in`/`out` 可降至 0（AC-57③）；
   - AI 行动改 Instant（AC-57④）；
   - 操作栏阶段切换去动效（AC-57⑤）。

6. **F-4 过期判定实现要点**：
   - `E_cur` 每收到一次 `OnTurnStarted`（不含双点额外回合，因 player-turn L197 双点链不重发 `OnTurnStarted`）在 `StateMachine::OnTurnStarted` 中 `E_cur++`。
   - `E_action(i)` 在 `OnAIActionExecuted` 到达时记录当时 `E_cur`。
   - `unpaused_elapsed(i)` 用 `IGameClock::GetUnpausedTimeSec()` 快照，读档时（`HandleGameLoaded`）重置：以 `FMockGameClock::ResetOnLoad()` 为测试锚点。
   - 主判据（`E_cur > E_action(i)`）优先；辅判据（`unpaused_elapsed > T_stale`）为软上界。

7. **Counter 动画 EC-1（中途重定向）**：`OnCashChanged` 到达时，若当前 `PlayerId` 面板有活跃 Counter，以**当前插值显示值**（`GetDisplayedCash`）为新 `OldCash`，新 `NewCash` 为目标重启 F-1/F-2，不排队。

8. **测试可达性要求**（满足 [Logic·门控] AC 关闭条件）：
   - `UHudStateMachine` 可在 UE Automation Test (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) 中直接 `NewObject<UHudStateMachine>` 实例化。
   - 注入 `FMockGameClock`，手动调 `OnTurnStarted`/`OnCashChanged` 等驱动状态机，断言 `GetPanelState`/`GetDisplayedCash`/`IsActionExpired` 输出。
   - 无需启动 UMG，无需 World，实现纯 UObject 层的单元测试。

---

## Alternatives Considered

### Alternative 1: 全部逻辑住在 UUserWidget（NativeTick 直驱）

- **Description**: 不抽独立 UObject，直接在 `WB_HUD` 的 `NativeTick` 中写所有时序逻辑和状态机。Widget 持时钟引用，直接读 `GetWorld()->GetTimeSeconds()`。
- **Pros**: 实现最简单，文件数最少；NativeTick 在运行时完整可用。
- **Cons**: `UUserWidget::NativeTick` 在 `-nullrhi` 模式下不触发 → **AC-12/AC-24a/AC-30/AC-57 全部 false-green（虚假通过）**，测试质量严重降级；`GetWorld()->GetTimeSeconds()` 不可 mock → F-4 过期判定无法 headless 测试；违反 ADR-0007 C++ 权威逻辑原则。
- **Estimated Effort**: 比选中方案低 20%（省独立 UObject 文件）。
- **Rejection Reason**: headless 测试 false-green 是不可接受的质量风险，会导致多个 [Logic·门控] AC 永久无法关闭。

### Alternative 2: UMG Timeline 驱动 counter 动画

- **Description**: 用 `UWidgetAnimation`（UMG sequencer）做 counter 动画，通过 `SetPlaybackRate` 改变有效时长。
- **Pros**: 无需 NativeTick，动画资产在 WBP_ 内可视化编辑。
- **Cons**: UMG sequencer **无整数文本插值轨道**（`UTextBlock::SetText` 无对应轨道类型）→ 无法驱动滚动数字；`PlaybackSpeed` 缩放只影响时长不影响数值，终点精度无法保证（hud.md F-2 精确排除理由）。即便用 Timeline Blueprint 节点，也须每帧回调 `SetText`，本质退化为 NativeTick 路径。
- **Estimated Effort**: 相当，但需大量 BP 胶水代码。
- **Rejection Reason**: 技术上不可行（无整数文本插值轨道），hud.md F-2 已明确排除此路径。

### Alternative 3: Blueprint Timeline 节点 + 事件驱动（不抽独立 UObject）

- **Description**: 在 WBP_HUD Blueprint 中用 Timeline 节点驱动 counter float，每帧绑 Set Text。状态机也在 BP 中用变量/分支实现。
- **Pros**: 全 BP，不增加 C++ 文件；设计师可视化调参。
- **Cons**: BP Timeline 同样住在 UUserWidget 生命周期内，`-nullrhi` 下不触发 → headless 测试同 Alt-1 同等问题；BP 状态机难以单元测试（uasset 二进制，无 headless 框架）；BP Timeline float → `round(int)` → `SetText` 链在蓝图图中冗长易错；违反 ADR-0007（状态机逻辑归 C++）。
- **Estimated Effort**: BP 图构建时间较长，调试困难。
- **Rejection Reason**: headless 测试失效 + 违反 ADR-0007 C++ 权威逻辑原则。

### Alternative 4: MVVM（UE5 Model-View-ViewModel 插件）

- **Description**: 使用 UE5 的 MVVM 插件（`UMVVMViewModelBase`）做数据绑定，自动双向同步现金 / 状态到 Widget。
- **Pros**: 声明式绑定减少手写 Tick；架构更现代；property change 通知自动驱动 Widget 更新。
- **Cons**: MVVM 插件在 UE 5.4+ 引入，属 post-cutoff，UE 5.7 具体 API 稳定性须 spike 验证（HIGH 知识风险）；绑定依赖 property change 通知，**不适合逐帧插值**（counter 需每帧精确计算显示值，非单次 property set）；学习曲线对 BP-为主团队更陡；额外插件依赖。
- **Estimated Effort**: 高（需 spike + 团队培训）。
- **Rejection Reason**: counter 逐帧插值不适合 property change 驱动模型；post-cutoff 知识风险过高；project 技术偏好优先 Blueprint 为主 + C++ 权威逻辑（ADR-0007），不引入额外 UI 框架依赖。

---

## Consequences

### Positive

- **headless 测试可达**：`UHudStateMachine` 在 `-nullrhi` 下可实例化，AC-12/AC-24a/AC-30/AC-57 从 [Advisory] 升为真正的 [Logic·门控]，自动化 CI 可绿。
- **counter 动画精确**：NativeTick 逐帧 `SetText(round(displayed(t)))` 保证 t=0 精确等于 `OldCash`，t=T 精确等于 `NewCash`（AC-6 可硬断言）。
- **时钟 mock 隔离**：F-4 过期判定的 `unpaused_elapsed` 四变体（AC-30）可用 `FMockGameClock::AdvanceTime` 精确驱动，无帧抖动。
- **BP 为主策略保留**：Widget 树/布局/动画资产在 WBP_ 内编辑，C++ 只提供基类和状态机，满足 ADR-0007。
- **防双注册**：显式解绑纪律（AC-48）消除读档后重复触发事件的静默 bug。

### Negative

- **文件数增加**：需新增 `IGameClock.h`、`FWorldGameClock.h`、`FMockGameClock.h`、`HudStateMachine.h/.cpp`、`WB_HUD.h/.cpp`（约 5 个新文件）。
- **NativeTick 常驻**：即使无动画，`WB_HUD::NativeTick` 每帧触发一次（调用 StateMachine::Tick，后者早返）。须注意 `bCanEverTick = true` 的隐性开销；可通过在无活跃动画时 `SetCanTick(false)` 优化。
- **Widget–StateMachine 二段写**：状态更新路径分两步（StateMachine 更新内部状态 → Widget 读取输出并写控件），代码路径比单体实现稍长，需要清晰的接口契约防止状态读取错乱。

### Neutral

- `IGameClock` 是项目自定义接口，不是引擎 API，不存在 post-cutoff 破坏性变更风险。
- `UHudStateMachine` 作为 `UPROPERTY()` 成员由 Widget 持有，GC 安全。
- `WB_HUD` 继承 `UCommonActivatableWidget`（CommonUI），与 ADR-0012 要求一致；具体激活栈和输入路由细节留 ADR-0012 裁定。

---

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| UE 5.7 NativeTick 在 -nullrhi 行为与假设不符（触发而非不触发）| LOW | MED | 开工前 spike 验证（见 Engine Compatibility § Verification Required ②）；若触发则 Alternative 1 可行，但仍推荐保留 UObject 分离以遵循 ADR-0007 |
| UCommonActivatableWidget 5.7 API 变更（NativeOnActivated 签名等）| LOW | MED | 开工前查 engine-reference/ui.md + 5.7 release notes；若变更调整基类实现，ADR-0012 同步更新 |
| FSlateFontInfo tnum API 暴露（hud.md CR-3 核验标注）| LOW | LOW | 结论不影响本 ADR（IGameClock DI 与字体无关）；fallback（等宽字形字体资产）已在 hud.md 登记，非 BLOCKING |
| MVVM 插件在 5.7 稳定性未核验，未来引入可能与本架构冲突 | MED | LOW | 本 ADR 不阻止未来引入 MVVM；若引入，counter 逐帧插值路径仍保留 NativeTick，其余只读字段可迁 ViewModel |
| BP 绑定漏解绑（双注册 bug）| MED | MED | AC-48 [Integration] 强制 code-review；推荐 C++ AddDynamic 路径，减少 BP Bind Event 使用 |

---

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time, HUD) | N/A（未实现） | ~0.3ms（有动画期：4 面板×counter+banner+AI 队列）；~0.05ms（静止态 early-return） | < 2ms 总 UI 预算（含 WBP 渲染） |
| Memory | N/A | `UHudStateMachine`：~2KB per instance（TArray×4 + 小 struct）；`FMockGameClock`：仅测试用，生产不存在 | 可忽略 |
| Load Time | N/A | 无额外资产加载（IGameClock 纯代码接口） | N/A |

> **NativeTick 优化建议**：静止态（`!bHasActiveAnims`）时 StateMachine::Tick 早返，约 0.02ms；有动画时约 0.3ms（4 路 counter + 4 个 banner + AI 队列遍历）。若 profiling 显示超 0.5ms，可改为在事件到达时 `SetCanTick(true)`、动画完成时 `SetCanTick(false)` 动态开关 Tick。

---

## Migration Plan

本 ADR 为新实现（无现有代码迁移）。

1. **Step 1 — 接口层**：创建 `src/ui/hud/IGameClock.h`、`FWorldGameClock.h`；提交 code-review 确认接口签名对齐 F-4 的 `GetUnpausedTimeSec` 需求。
2. **Step 2 — 状态机**：实现 `UHudStateMachine`（`HudStateMachine.h/.cpp`）；**先写 UE Automation Test**（`tests/unit/hud/HudStateMachine_Test.cpp`），用 `FMockGameClock` 驱动，确认 AC-12/AC-30 的四变体 headless 绿。
3. **Step 3 — Widget 基类**：实现 `UWB_HUD`（`WB_HUD.h/.cpp`）；在 WBP_HUD Blueprint 中设置基类为 `UWB_HUD`；配置 `meta=(BindWidget)` 绑定控件名。
4. **Step 4 — 集成**：在 WBP_HUD Blueprint 中绑定 owner delegate 事件（推荐 C++ AddDynamic；若 BP Bind Event 路径不可避免须补 Unbind）；验证 AC-48 code-review 通过。
5. **Step 5 — `DA_HudConfig`**：创建 `DA_HudConfig` Primary Data Asset，暴露 `reduce_motion`、`K_dur`、`T_min`、`T_max` 等 Tuning Knobs；在 `UHudStateMachine::Init` 读入。

**Rollback plan**: 本 ADR 已 Accepted 但尚无实现，修订不影响任何已提交实现。若实现期 spike 发现 Alternative 1（全住 Widget）在 5.7 headless 测试通过，可经 TD 重新 gate 修订本 ADR 移除 UObject 抽离要求，保留 IGameClock DI（DI 收益无论如何保留）。

---

## Validation Criteria

- [ ] Spike 验证：`UUserWidget::NativeTick` 在 `-nullrhi` 模式下**不触发**（证明 UObject 分离必要性）
- [ ] `UHudStateMachine` 在 UE Automation Test (`-nullrhi`) 实例化成功，注入 `FMockGameClock`，无崩溃
- [ ] AC-12 [Logic]：`OnTurnStarted(P)` 到达后，1 游戏帧内 `GetPanelState(P) == Active`（`FMockGameClock::AdvanceTime(0.016)` 驱动）
- [ ] AC-30 [Logic] 四变体：`FMockGameClock` 精确控制时间，四路过期/未过期判定全部正确
- [ ] AC-6 [Logic]：counter 插值 t=0 精确 `OldCash`，t=T 精确 `NewCash`，误差 ≤ 1
- [ ] AC-48 [Integration]：读档重绑路径 code-review 通过，旧 Handler 不被重复调用（spy 断言）
- [ ] 性能：有 4 面板活跃 counter 动画时，HUD Tick CPU ≤ 0.5ms（`stat slate` / profiler 截图）

---

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/hud.md` | HUD (#16) | OQ-HUD-2：动画驱动机制 + 状态机抽独立 UObject + `IGameClock` DI（AC-12/24a/30/57 headless 硬前提） | 裁定 UHudStateMachine（UObject）+ IGameClock DI，使 [Logic·门控] AC 可在 -nullrhi headless 真正验绿 |
| `design/gdd/hud.md` | HUD (#16) | F-2 精确排除理由：UMG sequencer 无整数文本插值轨道，滚动数字须每帧 SetText | 裁定 NativeTick（C++ 侧）逐帧驱动，明确排除 UWidgetAnimation / PlaybackSpeed 路径 |
| `design/gdd/hud.md` | HUD (#16) | EC-11/AC-48：读档重绑防双注册（BP Bind Event 非 AddUnique） | 裁定显式解绑纪律（先 Unbind 后 Bind），code-review 强制核验 |
| `design/gdd/hud.md` | HUD (#16) | AC-57：reduce_motion 配置参数读回 + T_min 旁路 | 裁定从 DA_HudConfig 读取 bReduceMotion，Init 时注入 StateMachine，旁路逻辑在 StateMachine::Tick 内 |
| `design/gdd/hud.md` | HUD (#16) | F-4：unpaused_elapsed 用游戏时间（暂停不计、读档重置）| IGameClock::GetUnpausedTimeSec + HandleGameLoaded 时 Reset，FMockGameClock 可精确控制 |
| `docs/architecture/architecture.md` | 架构总纲 §8 | ADR-0008 覆盖 HUD OQ-HUD-2 | 本 ADR 即覆盖件 |
| `docs/architecture/architecture.md` | 架构原则 §原则3 | 依赖注入优于单例（DI over Singleton） | IGameClock 接口注入，生产/测试各走不同实现，不直取 GetWorld() 全局 |
| `docs/architecture/architecture.md` | 架构原则 §原则4 | C++ 权威逻辑 / Blueprint 呈现交互 | 状态机逻辑在 C++ UHudStateMachine；Widget 树/布局在 WBP_ Blueprint |

---

## Related

- [ADR-0001](ADR-0001-uobject-host-lifecycle.md) — UObject 宿主生命周期（HUD Widget 宿主 + Subsystem 初始化顺序，本 ADR 依赖）
- [ADR-0007] — BP-vs-C++ 边界（本 ADR 是其在 HUD 域的具体落地，须 ADR-0007 Accepted 后本 ADR 才可 Accepted）
- ADR-0012（待起草）— CommonUI 输入路由与激活栈（`UCommonActivatableWidget` 激活栈细节在此裁定；本 ADR 已确定继承基类为 `UCommonActivatableWidget`）
- `design/gdd/hud.md` — HUD GDD（权威设计来源，所有 AC/F/CR 均以此文件为准）
- `design/gdd/property-card-ui.md` — 地产卡 UI（共享 BP 绑定解绑纪律，OQ-PC-4）
- 实现文件（创建后填入）：`src/ui/hud/IGameClock.h`、`src/ui/hud/HudStateMachine.h/.cpp`、`src/ui/hud/WB_HUD.h/.cpp`
- 测试文件（创建后填入）：`tests/unit/hud/HudStateMachine_Test.cpp`
