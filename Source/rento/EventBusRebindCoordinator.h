// EventBusRebindCoordinator.h
// =============================================================================
// UEventBusRebindCoordinator — Event Bus 读档集中解绑/重绑协调器（story-006）
//
// 职责（ADR-0003 §统一规范 #6 + ADR-0005 CR-5/CR-6）：
//   1. 注册机制：消费者（呈现层 widget / 测试 spy）在自身 Init 阶段向
//      OnRebindRequested 一次性 AddDynamic，完成注册。
//   2. RebindPresentationDelegates()：读档拓扑序「SetSeed 之后、
//      switch(CurrentPhase) 之前」的重绑入口。广播 OnRebindRequested，
//      触发所有已注册消费者各自执行「RemoveDynamic → AddDynamic」（幂等）。
//   3. UnbindAllOwnerDelegates()：物理遍历全部已知 owner delegate，逐一
//      调 .Clear() 清空 invocation list。锚在 Deinitialize（PIE Stop）全量拆除。
//      ⚠ 不在读档路径（RebindPresentationDelegates）调用，避免误删内部系统订阅。
//   4. Deinitialize()：调 UnbindAllOwnerDelegates() + 清空 OnRebindRequested，
//      PIE Stop 后不留野绑定（ADR-0001/0003）。
//
// 设计要点（防 EC-8 双订阅）：
//   消费者只在自身 Init 绑 meta-delegate OnRebindRequested **一次**。
//   OnRebindRequested 被广播时，消费者回调内执行「对真 owner delegate 先解后绑」，
//   使读档后订阅数 == 初次开局订阅数（无 2N 叠加，AC-3 核心）。
//
// Owner delegate 覆盖（story-006 已知范围）：
//   PlayerTurnSubsystem（7 个）:
//     OnPhaseChanged / OnGameWon / OnTurnStarted / OnTurnEnded
//     OnTurnOrderResolved / OnAIActionExecuted / OnBuildingAnnounced
//   DiceRngService（1 个）:
//     OnDiceRolled
//
// 生命周期边界：UWorldSubsystem（per-match），World 重建即重建，PIE Stop 即销毁。
//
// 规范依据：
//   ADR-0003（primary）— §统一规范 #6 读档重绑纪律
//   ADR-0005（secondary）— CR-5 拓扑序 + CR-6 delegate 重绑
//   ADR-0001 — UObject 宿主与生命周期边界
//   control-manifest Foundation Layer §事件总线 (ADR-0003)
//   story-006 AC-1~AC-6 + QA TC-1~5
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EventBusRebindCoordinator.generated.h"

// =============================================================================
// FOnPresentationRebindRequested — meta-delegate（无参，消费者注册重绑回调用）
//
// 消费者（呈现层 widget / 测试 spy）在 Init 时 AddDynamic 绑定一次。
// 读档时协调器广播此 delegate → 消费者回调内自行「RemoveDynamic + AddDynamic」
// 对真 owner delegate 的订阅（幂等，防 EC-8 双订阅）。
// =============================================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPresentationRebindRequested);

/**
 * UEventBusRebindCoordinator — 读档集中解绑/重绑协调器（per-match）
 *
 * Foundation「② Event Bus」纪律层的集中工具，物理遍历已知 owner delegate，
 * 提供幂等重绑入口与 PIE Stop 全量拆除能力。
 *
 * 访问方式（标准 Subsystem 发现）：
 * @code
 *   UEventBusRebindCoordinator* Coord =
 *       World->GetSubsystem<UEventBusRebindCoordinator>();
 * @endcode
 */
UCLASS()
class RENTO_API UEventBusRebindCoordinator : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 生命周期 override
    // =========================================================================

    /**
     * 宿主初始化（仅 Game/PIE World 创建，editor-preview 已由 ShouldCreateSubsystem 排除）。
     * 本实现仅调 Super，无额外初始化——消费者在自身 Init 时注册 OnRebindRequested。
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * 宿主销毁（PIE Stop / World 销毁）。
     *
     * 执行顺序：
     *   1. UnbindAllOwnerDelegates()  — 清空全部已知 owner delegate invocation list
     *   2. OnRebindRequested.Clear()  — 清空 meta-delegate（消费者注册列表）
     *   3. Super::Deinitialize()
     *
     * 保证 PIE Stop 后无野绑定残留（ADR-0001/0003，control-manifest Required）。
     */
    virtual void Deinitialize() override;

    /**
     * 仅 Game/PIE World 创建本 Subsystem，排除 Editor/EditorPreview（ADR-0001 §1）。
     */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    // =========================================================================
    // 核心 API
    // =========================================================================

    /**
     * 读档重绑入口（ADR-0005 CR-5 拓扑序中「骰子 SetSeed 之后、switch(CurrentPhase) 之前」）。
     *
     * 广播 OnRebindRequested → 所有已注册消费者收到通知后，
     * 各自执行「RemoveDynamic → AddDynamic」（幂等），使订阅数恢复至初次开局 N 个
     * 而非叠加为 2N（防 EC-8 双订阅，AC-3）。
     *
     * ⚠ 本函数不调用 UnbindAllOwnerDelegates()——避免误删内部 C++ 系统订阅。
     *   读档后 owner subsystem 为全新对象（per-match World 重建），invocation list 本就空；
     *   重绑的真实工作 = 触发「持久消费者」（跨读档存活的 widget）自身先解后绑。
     *
     * 线程安全：仅在 Game Thread 调用。
     */
    UFUNCTION(BlueprintCallable, Category="EventBus|Rebind")
    void RebindPresentationDelegates();

    /**
     * 物理遍历全部已知 owner delegate，逐一 .Clear() 清空 invocation list。
     *
     * 覆盖 owner：
     *   PlayerTurnSubsystem (7): OnPhaseChanged / OnGameWon / OnTurnStarted /
     *       OnTurnEnded / OnTurnOrderResolved / OnAIActionExecuted / OnBuildingAnnounced
     *   DiceRngService (1): OnDiceRolled
     *
     * 调用时机：仅在 Deinitialize（PIE Stop 全量拆除）。
     * ⚠ 不在 RebindPresentationDelegates 路径调用（避免误删内部系统订阅）。
     *
     * 容错：GetSubsystem null 检查，owner 不可用时安全跳过，不崩（QA Edge cases 要求）。
     */
    void UnbindAllOwnerDelegates();

    // =========================================================================
    // 消费者注册 meta-delegate（OnRebindRequested）
    // =========================================================================

    /**
     * 消费者注册入口：在消费者 Init 时一次性绑定重绑回调。
     *
     * 用法（C++ 消费者）：
     * @code
     *   Coord->OnRebindRequested.AddDynamic(this, &UMyClass::PerformRebind);
     * @endcode
     *
     * 用法（BP 消费者）：
     *   Bind Event to OnRebindRequested（仅在 Init 绑一次，不随读档重绑）。
     *
     * ⚠ 消费者只在 Init 绑 OnRebindRequested 一次（不随每次读档重绑），
     *   避免 meta-delegate 自身出现 EC-8 叠加。
     */
    UPROPERTY(BlueprintAssignable, Category="EventBus|Rebind")
    FOnPresentationRebindRequested OnRebindRequested;
};
