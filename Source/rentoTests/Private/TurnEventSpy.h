// TurnEventSpy.h
// =============================================================================
// UTurnEventSpy — story-004 新增 5 回合事件 + OnPhaseChanged 广播捕获 spy（测试专用）
//
// DYNAMIC_MULTICAST_DELEGATE（BlueprintAssignable）只能由 UObject 的 UFUNCTION 经
// AddDynamic 绑定——故 spy 须 UCLASS（UHT 不能处理 namespace/函数内 UCLASS，落独立文件作用域，
// 与 TurnPhaseSpy/PIEIsolationProbeSubsystem 同模式）。
//
// G3：DYNAMIC delegate spy 必 UCLASS。
// G4 例外：UObject spy 用 NewObject + AddToRoot/RemoveFromRoot 防 GC（G4 禁的是非 UObject 裸 *new）。
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlayerTurnTypes.h"
#include "TurnEventSpy.generated.h"

/**
 * UTurnEventSpy — 捕获 story-004 回合事件广播 payload 序列（测试专用）。
 *
 * 用法：
 * @code
 *   UTurnEventSpy* Spy = NewObject<UTurnEventSpy>(GetTransientPackage());
 *   Spy->AddToRoot();
 *   Sub->OnTurnStarted.AddDynamic(Spy, &UTurnEventSpy::HandleTurnStarted);
 *   // ... 驱动 ...
 *   // 断言 Spy->TurnStarted 等
 *   Spy->RemoveFromRoot();
 * @endcode
 */
UCLASS()
class UTurnEventSpy : public UObject
{
    GENERATED_BODY()

public:
    /** OnTurnStarted 广播序列。 */
    UPROPERTY() TArray<FTurnStartedInfo> TurnStarted;
    UFUNCTION() void HandleTurnStarted(const FTurnStartedInfo& I) { TurnStarted.Add(I); }

    /** OnTurnEnded 广播序列。 */
    UPROPERTY() TArray<FTurnEndedInfo> TurnEnded;
    UFUNCTION() void HandleTurnEnded(const FTurnEndedInfo& I) { TurnEnded.Add(I); }

    /** OnTurnOrderResolved 广播序列。 */
    UPROPERTY() TArray<FTurnOrderResult> OrderResolved;
    UFUNCTION() void HandleOrderResolved(const FTurnOrderResult& R) { OrderResolved.Add(R); }

    /** OnAIActionExecuted 广播序列（按广播先后）。 */
    UPROPERTY() TArray<FAIActionDetails> AIActions;
    UFUNCTION() void HandleAIActionExecuted(const FAIActionDetails& D) { AIActions.Add(D); }

    /** OnBuildingAnnounced 广播序列。 */
    UPROPERTY() TArray<FBuildingAnnouncedInfo> Announced;
    UFUNCTION() void HandleBuildingAnnounced(const FBuildingAnnouncedInfo& I) { Announced.Add(I); }

    /** OnPhaseChanged 广播序列（AC-3 ConsecutiveDoubles 验证）。 */
    UPROPERTY() TArray<FPhaseChangedInfo> PhaseChanges;
    UFUNCTION() void HandlePhaseChanged(const FPhaseChangedInfo& I) { PhaseChanges.Add(I); }

    /** OnGameWon 广播 WinnerId 序列（AC-1 绑定验证用；OnGameWon 单 int 无 USTRUCT）。 */
    UPROPERTY() TArray<int32> GameWonIds;
    UFUNCTION() void HandleGameWon(int32 WinnerId) { GameWonIds.Add(WinnerId); }
};
