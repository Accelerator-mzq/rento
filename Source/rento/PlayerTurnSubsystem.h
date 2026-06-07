// PlayerTurnSubsystem.h
// =============================================================================
// UPlayerTurnSubsystem — 回合系统 per-match 宿主（pt-001/pt-002 / TR-turn-001/002/015）
//
// 继承关系（ADR-0001 §per-match 服务一律继承 UWorldSubsystem）：
//   UMatchSubsystemBase（FF-001，纯骨架）
//     └─ UPlayerTurnSubsystem（本文件，回合2宿主）
//
// Story pt-001 scope（开局入口 + 建队 + 定序）：
//   1. 持 TArray<TObjectPtr<URentoPlayerState>> PlayerStates（玩家列表）
//   2. InitializeFromConfig — 建队（分配 PlayerId/TokenColor，P 边界校验 AC-10）
//   3. AssignTurnOrderByRollTotals — 排名纯方法（按点数和降序，可测，TC-3 注入 [5,9,3]）
//   4. ResolveInitialTurnOrderWithTiebreak — 生产定序 + F-4 平手重掷（pt-002 取代 pt-001 旧定序）
//   5. ValidateTurnOrderUniqueness — AC-5 唯一性校验（TurnOrderIndex 0..P-1 唯一性）
//   6. GetPlayerStates — 只读访问接口（供测试/下游）
//
// Story pt-002 scope（回合阶段状态机，禁 Latent，双点链 F-3，平手 F-4）：
//   7. CurrentPhase — 当前回合阶段（ETurnPhase 枚举字段，禁 Latent）
//   8. OnPhaseChanged — 最小 seam delegate（story-004 在此 enrich payload）
//   9. StartTurn / AdvanceTurnPhase — 阶段推进接口
//  10. ProcessRollResult — 消费 bIsDouble，驱动 F-3 双点链计数
//  11. ShouldGoToJail — F-3 纯函数，可单测（AC-7）
//  12. ResolveInitialTurnOrderWithTiebreak — F-4 平手重掷确定性裁定（AC-11）
//
// Out of Scope（本 story 不实现）：
//   - F-1 NextActivePlayer 寻路 + 破产移出 + OnGameWon → story-003
//   - 6 回合事件 USTRUCT payload → story-004
//   - 受控写接口面 SetPosition/SetCash → story-005
//   - RollPhase 真实骰子消费 → story-006
//   - 读档 switch(CurrentPhase) round-trip → story-008
//
// 规范依据：
//   - story pt-001 AC-1~5，story pt-002 AC-1~11
//   - ADR-0001（per-match 宿主，UWorldSubsystem；禁 Latent，枚举字段 + delegate 推进）
//   - ADR-0004（骰子权威流，F-3 时序契约）
//   - ADR-0007（回合状态机落 C++，[Logic] BLOCKING AC 被测逻辑落 C++）
//   - control-manifest Foundation Layer §宿主与生命周期（ADR-0001）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnTypes.h"
#include "PlayerTurnSubsystem.generated.h"

// 前向声明：DiceRngService（防循环 include，仅在 .cpp 完整 include）
class UDiceRngService;

// =============================================================================
// OnPhaseChanged 最小 seam delegate 声明（pt-002 / GDD delegate 形态约束 L265）
//
// 最小 seam：仅携带 NewPhase（story-004 在此 delegate 上 enrich FPhaseChangedInfo payload）。
// DYNAMIC_MULTICAST：1-对-多广播，BP 与 C++ 均可绑定（GDD L265 事件广播形态）。
// 参照 DiceRngService::OnDiceRolled 最小 seam 惯例（不重声明）。
//
// ⚠ 字段扩展代价（GDD L274 R5 unreal）：未来 enrich payload 须检查全部下游 BP 图引脚连接。
// =============================================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnPhaseChanged, ETurnPhase, NewPhase);

/**
 * UPlayerTurnSubsystem — 回合系统 per-match 宿主（story pt-001/pt-002）
 *
 * 持有 PlayerState 列表，执行开局建队与定序（pt-001），
 * 并驱动回合阶段状态机（pt-002）。
 *
 * 挂 UWorldSubsystem，生命周期 = 一局（PIE Stop 即销毁，Start 即重建）。
 *
 * 访问方式（标准 Subsystem 发现，O(1) 哈希，ADR-0001）：
 * @code
 *   UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
 * @endcode
 */
UCLASS()
class RENTO_API UPlayerTurnSubsystem : public UMatchSubsystemBase
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 生命周期 override（ADR-0001 四骨架扩展点）
    // =========================================================================

    /**
     * 宿主初始化（Initialize 在 World BeginPlay 前调用）。
     * 建队在 StartNewGame/InitializeFromConfig 时机（OnWorldBeginPlay 后）。
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** 宿主销毁（Deinitialize 在 World 销毁时调用，清理 PlayerStates 列表） */
    virtual void Deinitialize() override;

    // =========================================================================
    // 开局建队 + 定序接口（story pt-001 + pt-002 AC-10 P 边界）
    // =========================================================================

    /**
     * 据配置建队并执行开局定序（StartNewGame 转发调用点）。
     *
     * pt-002 AC-10 新增 P 边界校验：
     *   - P<2 → 拒绝（UE_LOG Error，返回 false）
     *   - P>4 → 拒绝（UE_LOG Error，返回 false；MVP 上限 4）
     *
     * 执行顺序：
     *   1. P 边界校验（AC-10）
     *   2. 清空旧 PlayerStates（幂等）
     *   3. 锁定开局旋钮（DoublesJailThreshold / MaxTiebreakRounds）
     *   4. TokenColor 唯一性预校验
     *   5. 建队（分配 PlayerId/TokenColor，置 ConsecutiveDoubles=0）
     *   6. 执行开局定序（ResolveInitialTurnOrderWithTiebreak，带 F-4 平手重掷）
     *   7. ValidateTurnOrderUniqueness 校验（AC-5）
     *   8. 置初始阶段 CurrentPhase=TurnStart
     *
     * @param Config 新局配置（Players.Num() = P）
     * @return true = 建队成功；false = 校验失败或 P 越界
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|Init",
        meta=(DisplayName="Initialize From Config"))
    bool InitializeFromConfig(const FGameSetupConfig& Config);

    /**
     * 排名纯方法：按点数和降序将 TurnOrderIndex 分配到 PlayerStates（pt-001 AC-3/4/TC-3）。
     *
     * 可测性设计：此方法接受外部注入的 RollTotals，不调 DiceRngService。
     * 测试直接注入 [5,9,3] 做确定性断言；生产由 ResolveInitialTurnOrderWithTiebreak 提供真实 rolls。
     *
     * @param RollTotals 每位玩家（按 PlayerStates 顺序）的掷骰点数和
     */
    void AssignTurnOrderByRollTotals(const TArray<int32>& RollTotals);

    /**
     * AC-5 唯一性校验：验证 TurnOrderIndex 覆盖 0..P-1 唯一。
     *
     * 若存在重复或跳值，记 Error 日志并返回 false。
     *
     * @return true = 唯一覆盖；false = 校验失败
     */
    bool ValidateTurnOrderUniqueness() const;

    /**
     * F-4 平手裁定：带平手重掷的完整定序（pt-002 AC-11）。
     *
     * 算法：
     *   round ∈ [0, MaxTiebreakRounds-1]：所有平手子组同步重掷
     *   达 MaxTiebreakRounds 后剩余平手按子组内席位升序裁定
     *   保证有限终止 + 确定性（子组不相交 rank 段）
     *
     * 注入 seam（测试可控性）：
     *   InjectRolls 非空时直接消费（不调 DiceService），用于确定性测试。
     *   每次调用消费一批 P 个值（对应一轮所有玩家）。
     *   InjectRolls 为空时走 DiceService 权威流（生产路径）。
     *
     * @param DiceService     权威 RNG（生产路径，InjectRolls 非空时不调用）
     * @param InjectRolls     测试注入 rolls 序列（每 P 个一轮，用尽后继续用 DiceService）
     */
    void ResolveInitialTurnOrderWithTiebreak(
        UDiceRngService* DiceService,
        const TArray<int32>& InjectRolls = TArray<int32>());

    // =========================================================================
    // 状态机接口（story pt-002）
    // =========================================================================

    /**
     * 启动指定玩家的回合（置 TurnStart 并广播 OnPhaseChanged，路由正常/在狱分支）。
     *
     * TurnStart → 路由：
     *   bIsInJail=true  → JailTurn → 等待 AdvanceFromJailTurn() 推进
     *   bIsInJail=false → RollPhase（正常阶段序列起点）
     *
     * @param PlayerId 当前行动玩家 ID
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    void StartTurn(int32 PlayerId);

    /**
     * 消费掷骰结果，驱动 F-3 双点链计数并推进到 MovePhase（或 TurnEnd 如送监狱）。
     *
     * F-3 时序契约（ADR-0004 / GDD L356，pt-002 Note 4）：
     *   ① bIsDouble=T → ConsecutiveDoubles+=1（先累加）
     *   ② 判 DoublesToJail（>= DoublesJailThreshold）：
     *      T → bSentToJail=true，ConsecutiveDoubles=0，无额外回合
     *      F → 正常移动（无操作，MovePhase 委派移动4）
     *   ③ bIsDouble=F → ConsecutiveDoubles=0
     *
     * 仅在 RollPhase 合法调用；其他阶段调用为非法转移（AC-2），记 Error 并保持当前阶段。
     *
     * @param bIsDouble       本次掷骰是否双点（来自 FDiceRollResult.bIsDouble）
     * @param OutSentToJail   [out] 是否因三连双点触发送监狱（F-3）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    void ProcessRollResult(bool bIsDouble, bool& OutSentToJail);

    /**
     * 从 MovePhase 推进到 ResolvePhase（移动4 回报落点后调用）。
     *
     * 仅在 MovePhase 合法；其他阶段为非法转移（AC-2）。
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    void AdvanceFromMovePhase();

    /**
     * 从 ResolvePhase 推进到 PostRollAction（落地结算完成后调用）。
     *
     * 仅在 ResolvePhase 合法；其他阶段为非法转移（AC-2）。
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    void AdvanceFromResolvePhase();

    /**
     * 结束回合（PostRollAction → TurnEnd，判定额外回合或移交下一玩家）。
     *
     * 逻辑（GDD (b) L224 / CR-4 / CR-5）：
     *   - bIsDouble && !bSentToJailThisTurn && !bIsBankrupt → TurnEnd 后回同玩家 RollPhase（双点额外回合）
     *     此路径 ConsecutiveDoubles 不归零（AC-6 关键）
     *   - 否则 → TurnEnd 后移交下一玩家 TurnStart
     *     ConsecutiveDoubles 归零（AC-6：行动权实际移交时归零）
     *
     * 仅在 PostRollAction 合法；其他阶段为非法转移（AC-2）。
     *
     * @param bSentToJailThisTurn  本回合是否被送监狱（suppresses extra turn，AC-9）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    void EndTurn(bool bSentToJailThisTurn = false);

    /**
     * 从 JailTurn 推进到 TurnEnd（在狱玩家出狱决策完成后调用）。
     *
     * 语义（AC-8）：
     *   bRemainsInJail=true → JailTurnsServed+=1（留狱增计）
     *   bRemainsInJail=false → 不增计（出狱不算服刑回合）
     *
     * 仅在 JailTurn 合法；其他阶段为非法转移（AC-2）。
     *
     * @param bRemainsInJail  是否仍留狱（true=留狱并服刑，false=出狱）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    void AdvanceFromJailTurn(bool bRemainsInJail);

    /**
     * F-3 纯函数：判定是否应送监狱（可单测，AC-7）。
     *
     * bShouldJail = (ConsecutiveDoubles >= Threshold)
     *
     * 用 >= 非 == 做额外防御（GDD L354 澄清）：
     *   正常路径 ConsecutiveDoubles ∈ [0, threshold]，>= 逻辑等价 ==；
     *   异常路径（内存损坏/读档越级）>= 兜住越级情形（ADR-0004 belt-and-suspenders）。
     *
     * @param ConsecutiveCount  当前连续双点计数（本次掷骰更新后的值）
     * @param Threshold         入狱阈值（DoublesJailThreshold）
     * @return true = 应送监狱
     */
    static bool ShouldGoToJail(int32 ConsecutiveCount, int32 Threshold);

    // =========================================================================
    // 只读访问接口（供测试/下游消费）
    // =========================================================================

    /**
     * 获取 PlayerState 列表只读引用（O(1)，无拷贝）。
     *
     * @return PlayerState 列表只读引用
     */
    const TArray<TObjectPtr<URentoPlayerState>>& GetPlayerStates() const;

    /**
     * 按 PlayerId 查找 PlayerState（O(N)，玩家数 <=4 可接受）。
     *
     * @param PlayerId 玩家唯一 ID
     * @return 找到时返回对应指针；未找到返回 nullptr
     */
    URentoPlayerState* FindPlayerById(int32 PlayerId) const;

    /**
     * 获取当前回合阶段（ETurnPhase 枚举字段，禁 Latent，只读）。
     *
     * 供测试断言阶段序列（AC-1 spy OnPhaseChanged 序列）+ 读档恢复（story-008）。
     *
     * @return 当前阶段
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    ETurnPhase GetCurrentPhase() const;

    /**
     * 获取当前行动玩家 ID（-1 表示无活跃回合）。
     *
     * @return 当前行动玩家 PlayerId；-1 表示尚未 StartTurn
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|StateMachine")
    int32 GetCurrentActivePlayerId() const;

    // =========================================================================
    // OnPhaseChanged 事件（最小 seam，pt-002；story-004 在此 enrich）
    // =========================================================================

    /**
     * 阶段变更广播事件（最小 seam，pt-002）。
     *
     * 每次 CurrentPhase 改变时广播新阶段值。
     * story-004 将在此既有 delegate 上 enrich 完整 FPhaseChangedInfo payload，不重新声明。
     *
     * 绑定示例（C++）：
     * @code
     *   Sub->OnPhaseChanged.AddDynamic(this, &MyClass::OnTurnPhaseChanged);
     * @endcode
     */
    UPROPERTY(BlueprintAssignable, Category="PlayerTurn|StateMachine")
    FOnTurnPhaseChanged OnPhaseChanged;

    // =========================================================================
    // 旋钮（开局配置锁定值，供下游系统读取）
    // =========================================================================

    /** 双点入狱阈值（开局锁定，GDD CR-4；story-002 回合链计数消费） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Config")
    int32 DoublesJailThreshold = 3;

    /** 平手最大重掷轮数（开局锁定；story-002 F-4 平手重掷消费） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Config")
    int32 MaxTiebreakRounds = 5;

    // =========================================================================
    // 状态机字段（pt-002，禁 Latent，ADR-0001 §4）
    // =========================================================================

    /**
     * 当前回合阶段（ETurnPhase 枚举字段，禁 Latent，ADR-0001 §4）。
     *
     * 禁止用 FTimerHandle/Blueprint Delay/WaitForEvent 实现阶段等待——
     * Latent 状态在 Save 时丢失，读档无法从 switch(CurrentPhase) 重入（ADR-0001）。
     * 本字段配合 story-008 存档，使读档后从精确阶段续行成为可能。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|StateMachine")
    ETurnPhase CurrentPhase = ETurnPhase::TurnStart;

    /**
     * 当前行动玩家 ID（-1 = 无活跃回合）。
     * StartTurn(PlayerId) 设置，TurnEnd 移交后更新。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|StateMachine")
    int32 CurrentActivePlayerId = -1;

    /**
     * 本回合最后一次掷骰是否双点（F-3/CR-4 双点额外回合判据）。
     * ProcessRollResult 更新，EndTurn 消费后归零。
     */
    UPROPERTY()
    bool bLastRollWasDouble = false;

    /**
     * 本回合是否已因 F-3 触发送监狱（抑制额外回合，AC-9）。
     * ProcessRollResult 设置，EndTurn 消费后归零。
     */
    UPROPERTY()
    bool bSentToJailThisTurnInternal = false;

private:
    // =========================================================================
    // 内部成员
    // =========================================================================

    /**
     * PlayerState 列表（P 个元素，随局构建）。
     *
     * TObjectPtr<URentoPlayerState>：UPROPERTY 防 GC，TObjectPtr 满足 UE5 best-practices。
     * 生命周期：随 World 销毁（UPlayerTurnSubsystem Deinitialize 时清空，GC 回收）。
     */
    UPROPERTY()
    TArray<TObjectPtr<URentoPlayerState>> PlayerStates;

    /**
     * 推进状态机到新阶段（设置 CurrentPhase + 广播 OnPhaseChanged）。
     * 内部统一调用点，确保每次阶段变更都触发事件。
     *
     * @param NewPhase 目标阶段
     */
    void SetPhase(ETurnPhase NewPhase);

    /**
     * 找下一个未破产玩家的 PlayerId（pt-002 简单递增，story-003 完整 F-1 接管）。
     *
     * pt-002 实现：按 TurnOrderIndex 递增找下一个未破产玩家（不含当前）。
     * ⚠ story-003 注释：完整 F-1 寻路（破产跳过/OnGameWon/INDEX_NONE 哨兵）归 story-003。
     *
     * @return 下一未破产玩家的 PlayerId；无合法下一玩家时返回 -1
     */
    int32 FindNextActivePlayerId() const;
};
