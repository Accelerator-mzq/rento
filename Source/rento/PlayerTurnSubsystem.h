// PlayerTurnSubsystem.h
// =============================================================================
// UPlayerTurnSubsystem — 回合系统 per-match 宿主（pt-001 / TR-turn-001/015）
//
// 继承关系（ADR-0001 §per-match 服务一律继承 UWorldSubsystem）：
//   UMatchSubsystemBase（FF-001，纯骨架）
//     └─ UPlayerTurnSubsystem（本文件，回合2宿主）
//
// Story pt-001 scope（开局入口 + 建队 + 定序）：
//   1. 持 TArray<TObjectPtr<URentoPlayerState>> PlayerStates（玩家列表）
//   2. InitializeFromConfig — 建队（为每位玩家创建 URentoPlayerState，分配唯一 PlayerId/TokenColor）
//   3. AssignTurnOrderByRollTotals — 排名纯方法（按点数和降序，可测，TC-3 注入 [5,9,3]）
//   4. PerformInitialTurnOrder — 生产定序（调 DiceRngService 取点数和，喂排名方法）
//   5. ValidateTurnOrderUniqueness — AC-5 唯一性校验（TurnOrderIndex 0..P-1 唯一性）
//   6. GetPlayerStates — 只读访问接口（供测试/下游）
//
// Out of Scope（本 story 不实现）：
//   - 回合阶段状态机 ETurnPhase → story-002
//   - 受控写接口面 SetPosition/SetCash/SetJailState/SetBankrupt → story-005
//   - 平手重掷 / MAX_TIEBREAK_ROUNDS → story-002
//   - 序列化/读档 → story-008
//
// 测试可测性设计（TC-3 mock 注入策略）：
//   AssignTurnOrderByRollTotals 是纯确定性方法，直接注入 TArray<int32>:
//     rolls=[5,9,3] → seat1:0, seat0:1, seat2:2
//   生产路径 PerformInitialTurnOrder 调 DiceRngService 后喂此方法（ADR-0004）。
//   测试绕过 DiceRngService 直接调 AssignTurnOrderByRollTotals，既守 ADR-0004 生产接线
//   又保持 headless 确定性。
//
// 规范依据：
//   - story pt-001 AC-1~5，TC-1~4，Implementation Notes 1~7
//   - ADR-0001（per-match 宿主，UWorldSubsystem，不持状态写语义）
//   - ADR-0004（定序掷骰走骰子3 权威流，禁旁路引擎 RNG）
//   - control-manifest Foundation Layer §宿主与生命周期（ADR-0001）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnSubsystem.generated.h"

// 前向声明：DiceRngService（防循环 include，仅在 .cpp 完整 include）
class UDiceRngService;

/**
 * UPlayerTurnSubsystem — 回合系统 per-match 宿主（story pt-001）
 *
 * 持有 PlayerState 列表，执行开局建队与定序。
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
     * 本 story 不在此处建队——建队在 StartNewGame/InitializeFromConfig 时机（OnWorldBeginPlay 后）。
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** 宿主销毁（Deinitialize 在 World 销毁时调用，清理 PlayerStates 列表） */
    virtual void Deinitialize() override;

    // =========================================================================
    // 开局建队 + 定序接口（story pt-001 core）
    // =========================================================================

    /**
     * 据配置建队并执行开局定序（StartNewGame 转发调用点）。
     *
     * 执行顺序：
     *   1. 清空旧 PlayerStates（幂等，防重复调用）
     *   2. 为每位 FPlayerSetupEntry 创建 URentoPlayerState
     *      - 分配唯一 PlayerId（0..P-1）
     *      - 复制 DisplayName/TokenColor/bIsAI/AIDifficulty
     *      - 校验 TokenColor 唯一性（EPlayerColor::None 拒绝，重复拒绝）
     *      - 置所有 ConsecutiveDoubles=0（GDD CR-2，定序不累加）
     *   3. 调 PerformInitialTurnOrder 执行定序（走 DiceRngService 权威流）
     *   4. 调 ValidateTurnOrderUniqueness 校验结果（AC-5）
     *
     * @param Config 新局配置（Players.Num() = P）
     * @return true = 建队成功；false = 校验失败（TokenColor 重复/None，TurnOrderIndex 不唯一）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|Init",
        meta=(DisplayName="Initialize From Config"))
    bool InitializeFromConfig(const FGameSetupConfig& Config);

    /**
     * 排名纯方法：按点数和降序将 TurnOrderIndex 分配到 PlayerStates（pt-001 AC-3/4/TC-3）。
     *
     * 可测性设计：此方法接受外部注入的 RollTotals，不调 DiceRngService。
     * 测试直接注入 [5,9,3] 做确定性断言；生产由 PerformInitialTurnOrder 提供真实 rolls。
     *
     * 语义约定（GDD CR-2 / story Note 7）：
     *   - rolls[i] 对应 PlayerStates[i]（seat i）的掷骰点数和
     *   - 点数高者 TurnOrderIndex=0（先手），降序排布
     *   - 定序只取点数和、忽略 bIsDouble（GDD CR-2 定序形态）
     *   - 平手处理归 story-002（本 story 假设点数互异无平手）
     *
     * @param RollTotals 每位玩家（按 PlayerStates 顺序）的掷骰点数和
     */
    void AssignTurnOrderByRollTotals(const TArray<int32>& RollTotals);

    /**
     * 生产定序：对每位玩家调 UDiceRngService 取点数和，喂 AssignTurnOrderByRollTotals。
     *
     * 走骰子3 唯一权威 FRandomStream（ADR-0004 Required）。
     * 定序只取 Total、忽略 bIsDouble、ConsecutiveDoubles 保持 0（GDD CR-2）。
     *
     * @param DiceService 权威 RNG 服务（per-match World Subsystem，ADR-0004）
     */
    void PerformInitialTurnOrder(UDiceRngService* DiceService);

    /**
     * AC-5 唯一性校验：验证 TurnOrderIndex 覆盖 0..P-1 唯一。
     *
     * 若存在重复或跳值，记 Error 日志并返回 false。
     * 与 board-data 索引唯一性校验同模式（BoardValidator 设计惯例）。
     *
     * @return true = 唯一覆盖；false = 校验失败
     */
    bool ValidateTurnOrderUniqueness() const;

    // =========================================================================
    // 只读访问接口（供测试/下游消费）
    // =========================================================================

    /**
     * 获取 PlayerState 列表只读引用（O(1)，无拷贝）。
     *
     * 供测试断言和下游系统（移动4/经济5/事件7/破产9）读取状态。
     * story-005 实现受控写接口后，外部系统经受控接口改写，不直接引用此数组。
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

    // =========================================================================
    // 旋钮（开局配置锁定值，供下游系统读取）
    // =========================================================================

    /** 双点入狱阈值（开局锁定，GDD CR-4；story-002 回合链计数消费） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Config")
    int32 DoublesJailThreshold = 3;

    /** 平手最大重掷轮数（开局锁定；story-002 平手分支消费） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Config")
    int32 MaxTiebreakRounds = 5;

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
};
