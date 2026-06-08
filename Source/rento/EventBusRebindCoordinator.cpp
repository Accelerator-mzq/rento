// EventBusRebindCoordinator.cpp
// =============================================================================
// UEventBusRebindCoordinator 实现（story-006）
//
// 依赖头文件：
//   PlayerTurnSubsystem.h — 7 个 owner delegate（OnPhaseChanged 等）
//   DiceRngService.h      — OnDiceRolled
// 以上仅读取/遍历 delegate，不修改 delegate 声明（story-006 Out of Scope 铁线）。
// =============================================================================

#include "EventBusRebindCoordinator.h"

// owner subsystem 完整 include（仅 .cpp 引用，避免 .h 循环依赖）
#include "PlayerTurnSubsystem.h"
#include "DiceRngService.h"

// =========================================================================
// 生命周期
// =========================================================================

void UEventBusRebindCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // 无额外初始化；消费者在自身 Init 时注册 OnRebindRequested
    UE_LOG(LogTemp, Log,
        TEXT("UEventBusRebindCoordinator::Initialize — 协调器就绪，等待消费者注册。"));
}

void UEventBusRebindCoordinator::Deinitialize()
{
    // 步骤 1：物理遍历全部 owner delegate，Clear() invocation list
    UnbindAllOwnerDelegates();

    // 步骤 2：清空 meta-delegate（消费者注册列表），不留野绑定
    OnRebindRequested.Clear();

    UE_LOG(LogTemp, Log,
        TEXT("UEventBusRebindCoordinator::Deinitialize — "
             "全部 owner delegate 已 Clear，OnRebindRequested 已 Clear，"
             "PIE Stop 无野绑定残留（ADR-0001/0003）。"));

    Super::Deinitialize();
}

bool UEventBusRebindCoordinator::ShouldCreateSubsystem(UObject* Outer) const
{
    // 仅 Game/PIE World 创建，排除 Editor/EditorPreview（ADR-0001 §1）
    if (!Super::ShouldCreateSubsystem(Outer)) { return false; }
    const UWorld* World = Cast<UWorld>(Outer);
    if (!World) { return false; }
    return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
}

// =========================================================================
// 核心 API
// =========================================================================

void UEventBusRebindCoordinator::RebindPresentationDelegates()
{
    // 广播 meta-delegate → 各注册消费者收到通知后自行
    // 「RemoveDynamic → AddDynamic」对真 owner delegate 的订阅（幂等）。
    // 本函数不调 UnbindAllOwnerDelegates()，避免误删内部 C++ 系统订阅。
    UE_LOG(LogTemp, Log,
        TEXT("UEventBusRebindCoordinator::RebindPresentationDelegates — "
             "广播 OnRebindRequested（拓扑序: SetSeed 后、switch(CurrentPhase) 前）。"));

    OnRebindRequested.Broadcast();
}

void UEventBusRebindCoordinator::UnbindAllOwnerDelegates()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UEventBusRebindCoordinator::UnbindAllOwnerDelegates — "
                 "World 为 null，跳过 owner delegate 遍历。"));
        return;
    }

    // -------------------------------------------------------------------------
    // PlayerTurnSubsystem — 7 个 delegate
    // -------------------------------------------------------------------------
    UPlayerTurnSubsystem* PTS = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (PTS)
    {
        // Clear 每个 public UPROPERTY(BlueprintAssignable) delegate
        PTS->OnPhaseChanged.Clear();
        PTS->OnGameWon.Clear();
        PTS->OnTurnStarted.Clear();
        PTS->OnTurnEnded.Clear();
        PTS->OnTurnOrderResolved.Clear();
        PTS->OnAIActionExecuted.Clear();
        PTS->OnBuildingAnnounced.Clear();

        UE_LOG(LogTemp, Log,
            TEXT("UEventBusRebindCoordinator::UnbindAllOwnerDelegates — "
                 "PlayerTurnSubsystem 7 个 delegate 已 Clear。"));
    }
    else
    {
        // owner 不可用时安全跳过，不崩（QA Edge cases 要求）
        UE_LOG(LogTemp, Log,
            TEXT("UEventBusRebindCoordinator::UnbindAllOwnerDelegates — "
                 "PlayerTurnSubsystem 不可用，安全跳过（无订阅者场景）。"));
    }

    // -------------------------------------------------------------------------
    // DiceRngService — 1 个 delegate
    // -------------------------------------------------------------------------
    UDiceRngService* Dice = World->GetSubsystem<UDiceRngService>();
    if (Dice)
    {
        Dice->OnDiceRolled.Clear();

        UE_LOG(LogTemp, Log,
            TEXT("UEventBusRebindCoordinator::UnbindAllOwnerDelegates — "
                 "DiceRngService OnDiceRolled 已 Clear。"));
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("UEventBusRebindCoordinator::UnbindAllOwnerDelegates — "
                 "DiceRngService 不可用，安全跳过。"));
    }
}
