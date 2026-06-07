// OnGameWonSpy.h
// =============================================================================
// UOnGameWonSpy — OnGameWon 广播序列捕获 spy（story pt-003 AC-6/7/8 测试专用）
//
// 用途：
//   AC-6/7/8 要求断言「OnGameWon 广播计数精确 N 次 + WinnerId payload 正确」——
//   须捕获 DYNAMIC_MULTICAST_DELEGATE 的广播次数和 payload 序列。
//
// 机制：
//   DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameWon, int32, WinnerId) 要求
//   绑定方为 UObject 的 UFUNCTION——故 spy 须是文件作用域 UCLASS（UHT 不能处理
//   namespace/函数内的 UCLASS，与 TurnPhaseSpy.h 同模式）。
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
//
// 规范依据：
//   - story pt-003 AC-6（OnGameWon 广播计数验证）
//   - story pt-003 AC-7（边沿幂等：第二次被拦截不重发）
//   - story pt-003 AC-8（封堵2↔9：正常 TurnEnd 不广播）
//   - TurnPhaseSpy.h（相同 UCLASS spy 模式，参照）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OnGameWonSpy.generated.h"

/**
 * UOnGameWonSpy — 记录 OnGameWon 广播的 WinnerId 序列与计数（测试专用）。
 *
 * 用法：
 * @code
 *   UOnGameWonSpy* Spy = NewObject<UOnGameWonSpy>(GetTransientPackage());
 *   Spy->AddToRoot();  // 防 GC（测试同步执行，稳妥起见）
 *   Sub->OnGameWon.AddDynamic(Spy, &UOnGameWonSpy::HandleGameWon);
 *   // ... 驱动破产逻辑 ...
 *   // 断言 Spy->BroadcastCount == N, Spy->RecordedWinnerIds[0] == ExpectedId
 *   Sub->OnGameWon.RemoveDynamic(Spy, &UOnGameWonSpy::HandleGameWon);
 *   Spy->RemoveFromRoot();
 * @endcode
 */
UCLASS()
class UOnGameWonSpy : public UObject
{
    GENERATED_BODY()

public:
    /** 广播总次数（AC-6: ==1 / AC-7: 精确==1 / AC-8: ==0） */
    UPROPERTY()
    int32 BroadcastCount = 0;

    /** 按广播先后顺序记录的 WinnerId 序列（AC-6 payload 验证） */
    UPROPERTY()
    TArray<int32> RecordedWinnerIds;

    /**
     * OnGameWon 绑定 handler：记录每次广播的 WinnerId + 递增计数。
     *
     * UFUNCTION() 必须：DYNAMIC_MULTICAST_DELEGATE AddDynamic 要求绑定函数为 UFUNCTION。
     *
     * @param WinnerId 获胜玩家 PlayerId（来自 FOnGameWon payload）
     */
    UFUNCTION()
    void HandleGameWon(int32 WinnerId)
    {
        ++BroadcastCount;
        RecordedWinnerIds.Add(WinnerId);
    }
};
