// TurnPhaseSpy.h
// =============================================================================
// UTurnPhaseSpy — OnPhaseChanged 广播序列捕获 spy（pt-002 AC-1 测试专用）
//
// 用途：
//   AC-1 要求「OnPhaseChanged 事件按精确顺序 fire，无跳跃、无缺失」——这是对
//   delegate **广播**的契约，须捕获广播序列验证（轮询 GetCurrentPhase() 无法捕获
//   TurnEnd 这类瞬态广播：EndTurn 内 SetPhase(TurnEnd) 紧接 StartTurn 覆写 CurrentPhase，
//   轮询读到的已是下一玩家阶段，漏发 SetPhase(TurnEnd) 广播也测不出）。
//
// 机制：
//   DYNAMIC_MULTICAST_DELEGATE（FOnTurnPhaseChanged，BlueprintAssignable）只能由
//   UObject 的 UFUNCTION 经 AddDynamic 绑定——故 spy 须是 UCLASS。UHT 不能处理
//   namespace/函数内的 UCLASS，故落独立文件作用域 header（与 PIEIsolationProbeSubsystem
//   同模式，rentoTests 模块 UHT 处理本文件）。
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlayerTurnTypes.h"
#include "TurnPhaseSpy.generated.h"

/**
 * UTurnPhaseSpy — 记录 OnPhaseChanged 广播的 ETurnPhase 序列（测试专用）。
 *
 * 用法：
 * @code
 *   UTurnPhaseSpy* Spy = NewObject<UTurnPhaseSpy>(GetTransientPackage());
 *   Spy->AddToRoot();  // 防 GC（测试同步执行，稳妥起见）
 *   Sub->OnPhaseChanged.AddDynamic(Spy, &UTurnPhaseSpy::HandlePhaseChanged);
 *   // ... 驱动状态机 ...
 *   // 断言 Spy->Recorded 序列
 *   Sub->OnPhaseChanged.RemoveDynamic(Spy, &UTurnPhaseSpy::HandlePhaseChanged);
 *   Spy->RemoveFromRoot();
 * @endcode
 */
UCLASS()
class UTurnPhaseSpy : public UObject
{
    GENERATED_BODY()

public:
    /** 按广播先后顺序记录的阶段序列。 */
    UPROPERTY()
    TArray<ETurnPhase> Recorded;

    /** OnPhaseChanged 绑定 handler：把每次广播的新阶段追加到 Recorded。 */
    UFUNCTION()
    void HandlePhaseChanged(ETurnPhase NewPhase)
    {
        Recorded.Add(NewPhase);
    }
};
