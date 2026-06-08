// RebindConsumerSpy.h
// =============================================================================
// URebindConsumerSpy — story-006 重绑协调器测试 spy（测试专用）
//
// 职责：
//   1. RegisterWithCoordinator() — 向协调器 OnRebindRequested 一次性注册
//      PerformRebind（模拟「呈现层消费者在 Init 注册」）。
//   2. PerformRebind() — OnRebindRequested 广播时触发：
//      ① 先 RemoveDynamic 解绑 HandleOnPhaseChanged（幂等解绑）
//      ② 再 AddDynamic 重绑 HandleOnPhaseChanged（幂等重绑）
//      ③ RebindCallCount++ （统计重绑被触发次数，AC-3 计数验证核心）
//   3. HandleOnPhaseChanged() — 真 owner delegate 回调，记 PhaseChangedCount++。
//
// 设计规范（ADR-0003 纯叶子不变式，AC-5）：
//   spy 仅订阅（呈现层纯叶子），不回写任何状态，不调任何 owner 方法写 delegate payload。
//   PerformRebind 只操作订阅关系，不触发 owner 广播。
//
// 已知坑规避：
//   G1 EAutomationTestFlags_ApplicationContextMask（见 ReadLoadDelegateRebindTest.cpp）
//   G3 DYNAMIC delegate spy 必须 UCLASS（AddDynamic 要求）
//   G4 UObject spy 用 NewObject + AddToRoot/RemoveFromRoot（禁裸 new）
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlayerTurnTypes.h"         // FPhaseChangedInfo
#include "EventBusRebindCoordinator.h"
#include "PlayerTurnSubsystem.h"
#include "RebindConsumerSpy.generated.h"

/**
 * URebindConsumerSpy — 重绑协调器测试 spy（stand-in 模拟呈现层消费者）
 *
 * 用法：
 * @code
 *   URebindConsumerSpy* Spy = NewObject<URebindConsumerSpy>(GetTransientPackage());
 *   Spy->AddToRoot();
 *   Spy->RegisterWithCoordinator(Coord, Sub);
 *
 *   // 触发重绑（模拟 OnGameLoaded 路径）
 *   Coord->RebindPresentationDelegates();
 *
 *   // 断言：RebindCallCount == 1（OnRebindRequested 广播了 1 次）
 *   // 断言：Sub->OnPhaseChanged.IsBound() == true（消费者已绑回 owner delegate）
 *
 *   Spy->RemoveFromRoot();
 * @endcode
 */
UCLASS()
class URebindConsumerSpy : public UObject
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 注册接口
    // =========================================================================

    /**
     * 向协调器注册重绑回调，并保存 owner subsystem 引用（供 PerformRebind 使用）。
     *
     * 模拟「呈现层消费者在自身 Init 阶段一次性注册」：
     *   Coordinator->OnRebindRequested.AddDynamic(this, &URebindConsumerSpy::PerformRebind)
     *
     * ⚠ 只在初始化时调用一次，不随每次读档重绑再调（避免 meta-delegate EC-8）。
     *
     * @param Coordinator  重绑协调器（不可为 null）
     * @param TurnSubsystem 回合系统 owner（不可为 null，PerformRebind 需绑其 OnPhaseChanged）
     */
    void RegisterWithCoordinator(
        UEventBusRebindCoordinator* Coordinator,
        UPlayerTurnSubsystem* TurnSubsystem)
    {
        if (!Coordinator || !TurnSubsystem) { return; }
        OwnerSubsystem = TurnSubsystem;
        // 一次性注册：绑 meta-delegate OnRebindRequested（不随读档重绑再绑）
        Coordinator->OnRebindRequested.AddDynamic(
            this, &URebindConsumerSpy::PerformRebind);
    }

    // =========================================================================
    // meta-delegate 回调（OnRebindRequested 触发点）
    // =========================================================================

    /**
     * OnRebindRequested 回调：幂等重绑 HandleOnPhaseChanged 到真 owner delegate。
     *
     * 执行序（AC-6 C++ 路径：RemoveDynamic → AddDynamic）：
     *   1. RemoveDynamic HandleOnPhaseChanged（解绑，已绑时移除，未绑时无操作，幂等）
     *   2. AddDynamic HandleOnPhaseChanged（重绑，无论之前是否已绑，保证恰一个订阅）
     *   3. RebindCallCount++（统计重绑被触发次数）
     *
     * ⚠ 纯叶子方向（AC-5）：仅操作订阅关系，不回写任何 owner 状态，不触发广播。
     */
    UFUNCTION()
    void PerformRebind()
    {
        if (!OwnerSubsystem) { return; }
        // 步骤1：先解绑（幂等，未绑时 RemoveDynamic 安全无操作）
        OwnerSubsystem->OnPhaseChanged.RemoveDynamic(
            this, &URebindConsumerSpy::HandleOnPhaseChanged);
        // 步骤2：再重绑（保证恰一个订阅）
        OwnerSubsystem->OnPhaseChanged.AddDynamic(
            this, &URebindConsumerSpy::HandleOnPhaseChanged);
        // 步骤3：计数
        ++RebindCallCount;
    }

    // =========================================================================
    // 真 owner delegate 回调（订阅 OnPhaseChanged 的处理函数）
    // =========================================================================

    /**
     * OnPhaseChanged 回调：记录收到的阶段变更广播次数。
     * 供测试断言「重绑后 owner delegate 广播确实能抵达 spy」（订阅通路验证）。
     */
    UFUNCTION()
    void HandleOnPhaseChanged(const FPhaseChangedInfo& Info)
    {
        ++PhaseChangedCount;
    }

    // =========================================================================
    // 计数器（供测试断言）
    // =========================================================================

    /** OnRebindRequested 触发次数（即 PerformRebind 被调次数）。AC-3 核心计数。 */
    int32 RebindCallCount = 0;

    /** HandleOnPhaseChanged 收到次数（验证 owner delegate → spy 通路）。 */
    int32 PhaseChangedCount = 0;

private:
    // =========================================================================
    // 内部状态
    // =========================================================================

    /**
     * 真 owner subsystem 弱引用（非 UPROPERTY TObjectPtr 因 spy 不参与 GC 持有周期）。
     * 测试确保 spy 生命周期 <= World 生命周期（AddToRoot 守住 spy，DestroyWorld 先销毁 Sub）。
     */
    UPROPERTY()
    TObjectPtr<UPlayerTurnSubsystem> OwnerSubsystem = nullptr;
};
