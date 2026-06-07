// PlayerTurnSubsystem.h
// =============================================================================
// UPlayerTurnSubsystem — 回合系统 per-match 宿主（pt-001/pt-002/pt-003 / TR-turn-001/002/006/015）
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
// Story pt-003 scope（F-1 NextActivePlayer + 破产移出 + OnGameWon + IResolveBankruptcy DI）：
//  13. NextActivePlayer — 静态纯函数，守卫先行，入口规范化（AC-1/2/3）
//  14. ActivePlayerCount — 静态计数辅助（AC-4；绝非胜负裁决器）
//  15. OnGameWon — 最小 seam delegate（story-004 enrich；本 story 验触发语义/次数）
//  16. HandlePlayerBankruptcy — 注入 IResolveBankruptcy stub，读返回值驱动 bIsBankrupt + OnGameWon
//  17. SetBankruptcyResolver — DI 注入接口（测试注入 stub）
//
// Out of Scope（不在本 story）：
//   - ETurnPhase 状态机阶段转换序列（pt-002）
//   - SetBankrupt 封装强度（story-005）
//   - OnGameWon delegate 完整契约声明（story-004）
//   - bankruptcy9 内部 ResolveBankruptcy 算法（bankruptcy epic）
//   - OnPlayerBankrupt / OnBankruptcyDeclared（不在本 story）
//
// 规范依据：
//   - story pt-001 AC-1~5，story pt-002 AC-1~11，story pt-003 AC-1~8
//   - ADR-0001（per-match 宿主，UWorldSubsystem；禁 Latent，枚举字段 + delegate 推进）
//   - ADR-0003（OnGameWon 单一事件源：回合2 触发，9 return-only）
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
#include "BankruptcyInterface.h"   // IResolveBankruptcy + FBankruptcyResolution（story-003 DI 接缝）
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

// =============================================================================
// OnGameWon 最小 seam delegate 声明（pt-003 / TR-turn-006）
//
// 最小 seam：仅携带 WinnerId（int32，开局后恒定 PlayerId）。
// story-004 将在此 delegate 上 enrich 完整 FGameWonInfo payload，不重新声明。
//
// 触发规则（ADR-0003 单一事件源）：
//   - 唯一触发来源 = HandlePlayerBankruptcy 内 IResolveBankruptcy 返回 WinnerId!=INDEX_NONE
//   - 9 永不直接广播（return-only 接口保证）
//   - 边沿触发：bGameWon=true 后再次触发被守卫拦截（AC-7）
//
// ⚠ story-004 enrich：完整 delegate 契约声明（USTRUCT payload、字段）归 story-004。
// =============================================================================
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameWon, int32, WinnerId);

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
    // F-1 / F-2 静态纯函数（story pt-003 / TR-turn-006）
    // =========================================================================

    /**
     * F-1 NextActivePlayer — 静态纯函数，守卫先行，入口规范化（pt-003 AC-1/2/3）。
     *
     * 守卫先行（GDD L289-293）：
     *   |A|=0 → INDEX_NONE + ensureMsgf（正常不可达）
     *   |A|=1 → INDEX_NONE（仅剩 1 名未破产玩家，对局已由 CR-6 终结；不返回唯一存活者）
     *   |A|≥2 → 入口规范化后枚举
     *
     * 入口规范化（GDD L295-297）：
     *   cur_safe = ((cur % P) + P) % P（欧几里得取模，保证 cur_safe ∈ [0,P-1]）
     *   枚举 k = min{k ∈ [1,P-1] | (cur_safe+k)%P ∈ ActiveIndices}
     *
     * dev 诊断：另加 ensureMsgf(cur>=0 && cur<P) 检查原始 cur（规范化前），
     *   规范化后的枚举不依赖此 ensure，仅作 dev 诊断。
     *
     * @param cur           当前行动玩家的 TurnOrderIndex
     * @param P             总玩家数（TurnOrderIndex 空间 [0,P-1]）
     * @param ActiveIndices 未破产玩家的 TurnOrderIndex 集合
     * @return 下一个活跃玩家的 TurnOrderIndex；|A|<=1 时返回 INDEX_NONE
     */
    static int32 NextActivePlayer(int32 cur, int32 P, const TSet<int32>& ActiveIndices);

    /**
     * F-2 ActivePlayerCount — 静态计数辅助（pt-003 AC-4）。
     *
     * 数 BankruptFlags 中 false 的个数（false=未破产=活跃）。
     *
     * ⚠ 重要约束（GDD L331 / story-003 §2）：
     *   本函数是纯计数辅助，绝非胜负裁决器。
     *   禁止在任何地方用「APC==1 → 广播 OnGameWon」逻辑。
     *   胜负裁决由破产9 在 ResolveBankruptcy 内部算，回传 WinnerId 给回合2。
     *
     * 边界（AC-4）：[T,T,T,T] → 0，触发 ensureMsgf（正常流程不可达）。
     *
     * @param BankruptFlags 每位玩家的破产标志（true=已破产，false=活跃）
     * @return 活跃玩家数（false 个数）
     */
    static int32 ActivePlayerCount(const TArray<bool>& BankruptFlags);

    // =========================================================================
    // 破产移出 + OnGameWon 触发接口（story pt-003）
    // =========================================================================

    /**
     * 注入 IResolveBankruptcy 实现（DI 注入面）。
     *
     * 测试注入 stub 返回受控值；生产由 bankruptcy epic 实现注入。
     * 调用时机：对局初始化后（InitializeFromConfig 后），在任何破产事件前注入。
     *
     * @param Resolver TSharedPtr 持有的注入实现（nullptr = 清除注入）
     */
    void SetBankruptcyResolver(TSharedPtr<IResolveBankruptcy> Resolver);

    /**
     * 处理玩家破产（story pt-003 AC-5/6/7/8 / CR-6 破产移出）。
     *
     * 执行顺序（GDD §CR-6 / story-003 Implementation Notes §5/6）：
     *   1. 调注入的 IResolveBankruptcy::ResolveBankruptcy（return-only，绝不回调）
     *   2. 若 bDebtorEliminated=true → 置 debtor.bIsBankrupt=true + 移出轮转
     *   3. 若 WinnerId!=INDEX_NONE 且未进 Winner 终态（!bGameWon）
     *      → 广播 OnGameWon(WinnerId) 恰一次 + 置 bGameWon=true（边沿触发）
     *
     * ⚠ AC-8 封堵 2↔9：本方法是 OnGameWon 唯一触发路径。
     *   正常 TurnEnd→F-1 移交路径（无 ResolveBankruptcy 调用）即便 APC==1 也绝不广播。
     *
     * @param DebtorId   债务玩家 PlayerId
     * @param CreditorId 债权玩家 PlayerId（-1 = 银行/无债权方）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|Bankruptcy")
    void HandlePlayerBankruptcy(int32 DebtorId, int32 CreditorId);

    // =========================================================================
    // OnGameWon 事件（最小 seam，pt-003；story-004 在此 enrich 完整契约声明）
    // =========================================================================

    /**
     * 游戏胜利广播事件（最小 seam，pt-003）。
     *
     * 每当 IResolveBankruptcy 返回 WinnerId!=INDEX_NONE 且对局尚未进入 Winner 终态时广播。
     * 边沿触发：bGameWon=true 后再次返回相同 WinnerId 不重发（AC-7 幂等）。
     *
     * story-004 将在此既有 delegate 上 enrich 完整 FGameWonInfo payload，不重新声明。
     *
     * 绑定示例（C++）：
     * @code
     *   Sub->OnGameWon.AddDynamic(this, &MyClass::HandleGameWon);
     * @endcode
     */
    UPROPERTY(BlueprintAssignable, Category="PlayerTurn|Bankruptcy")
    FOnGameWon OnGameWon;

    // =========================================================================
    // 胜负终态标志（pt-003，边沿触发守卫）
    // =========================================================================

    /**
     * 对局是否已进入 Winner 终态（边沿触发守卫，AC-7）。
     *
     * HandlePlayerBankruptcy 广播 OnGameWon 后置 true。
     * 此后再次调用 HandlePlayerBankruptcy 时，即便 IResolveBankruptcy 返回有效 WinnerId，
     * 也被此守卫拦截不重发（电平→边沿转换）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Bankruptcy")
    bool bGameWon = false;

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
     * 找下一个未破产玩家的 PlayerId（story-003 接管完整 F-1）。
     *
     * story-003 实现：调 static NextActivePlayer(cur, P, ActiveIndices)。
     * F-1 返回 INDEX_NONE（|A|<=1）时对局结束，轮转停止。
     *
     * @return 下一未破产玩家的 PlayerId；F-1 返回 INDEX_NONE 时返回 INDEX_NONE
     */
    int32 FindNextActivePlayerId() const;

    /**
     * IResolveBankruptcy DI 注入（story-003 §4）。
     *
     * 纯 C++ TSharedPtr（非 UObject，GC 不追踪）：
     *   - return-only 接口物理阻断 2↔9 回调环
     *   - 测试注入 stub，生产由 bankruptcy epic 提供
     *   - nullptr 时 HandlePlayerBankruptcy 记 Error 并返回（防御兜底）
     */
    TSharedPtr<IResolveBankruptcy> BankruptcyResolver;
};
