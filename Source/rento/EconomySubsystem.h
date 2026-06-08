// EconomySubsystem.h
// =============================================================================
// UEconomySubsystem —— 经济与现金服务（系统5）
//   econ-001（Cash 受控写/不变式）+ econ-002（事件契约）。
//   TR-econ-001/002/018（受控写）+ TR-econ-004/005/006/007（事件契约），amount≥0 部分 TR-econ-014。
//
// 形态：per-match UWorldSubsystem（继承 UMatchSubsystemBase，一局边界、PIE 隔离、ADR-0001）。
//
// 职责：
//   - 现金受控写：Credit / Debit / TransferCash（CR-8 原子守恒）—— 均带 EChangeReason
//   - 现金只读：GetCash
//   - 起始资金：GiveStartingCash（数据驱动 Tuning Knob）
//   - 事件契约（econ-002，ADR-0003）：OnCashChanged / OnRentPaid / OnInsufficientFunds / OnBankruptcyDeclared
//   - 不变式：Cash≥0（Debit 不足额转 Raising Funds）/ amount≥0 拒绝 / amount==0 早返静默
//
// 单源真相（用户 2026-06-08 裁定 Q1）：
//   本服务【不持任何 cash 容器】。Cash 的 field of record = URentoPlayerState.Cash（player-turn 拥有字段、
//   SetCash 字段级 setter）。Credit/Debit 读 PS->Cash、经 PS->SetCash 写。
//
// 解析（用户裁定 Q2）：
//   playerId → URentoPlayerState* 经 GetWorld()->GetSubsystem<UPlayerTurnSubsystem>()->FindPlayerById(id)。
//   运行时依赖：解析依赖回合2 已 InitializeFromConfig；未就绪时各操作安全 no-op（dev log + return false）。
//
// 命名（用户 2026-06-09 裁定）：UEconomySubsystem（对齐 Foundation EventBusDelegateContract 预期 +
//   系统5「经济与现金」全职责语义；econ-001 旧名 UEconomyCashSubsystem 已重命名）。
//
// 规范依据：
//   - GDD economy-cash.md CR-1（受控写/Cash≥0/amount≥0）、CR-8（原子守恒/无限银行）、
//     Events 节（4 事件 + EChangeReason 方向派生）、AC-1~5/33~39
//   - ADR-0001（UObject 宿主）、ADR-0003（事件总线 owner-held delegate）、ADR-0007（C++/BlueprintCallable 边界）、
//     ADR-0014（金钱运算整数纯净）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "EconomyTypes.h"            // EChangeReason + 4 Info USTRUCT + 4 delegate 签名
#include "EconomySubsystem.generated.h"

class URentoPlayerState;

/**
 * UEconomySubsystem —— 经济现金权威服务。
 *
 * 不持 cash 容器；Cash 单源 = URentoPlayerState.Cash，经 SetCash 受控写。
 */
UCLASS()
class RENTO_API UEconomySubsystem : public UMatchSubsystemBase
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 只读
    // =========================================================================

    /**
     * 查询玩家现金。
     * @param PlayerId 玩家 ID（0..P-1）
     * @return 玩家当前 Cash；玩家不存在返回 0（dev Warning）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    int32 GetCash(int32 PlayerId) const;

    // =========================================================================
    // 受控写
    // =========================================================================

    /**
     * 入账（Cash += Amount）。
     *
     * amount<0 非法（CR-1 R1 blocker#2）→ dev log + 拒绝、不改 Cash、不广播。
     * amount==0 早返静默（AC-5）→ 不改 Cash、不广播。
     * 成功 → PS->SetCash(Old+Amount) + 广播 OnCashChanged（携 Reason）。
     *
     * @param Reason 变动类型语境（EChangeReason，无方向位；方向由消费方派生，AC-33）。
     * @return true=已入账并广播；false=非法/零额/玩家不存在（未改 Cash）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool Credit(int32 PlayerId, int32 Amount, EChangeReason Reason);

    /**
     * 出账（Cash -= Amount），守 Cash≥0 不变式。
     *
     * amount<0 非法 → dev log + 拒绝。 amount==0 早返静默。
     * Amount > Cash（不足额，AC-1）→ 不扣、广播 OnInsufficientFunds、不广播 OnCashChanged（转 Raising Funds，story-009）。
     * 成功 → PS->SetCash(Old-Amount) + 广播 OnCashChanged（携 Reason）。
     *
     * @return true=已出账并广播；false=非法/零额/不足额/玩家不存在（未改 Cash）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool Debit(int32 PlayerId, int32 Amount, EChangeReason Reason);

    /**
     * 原子转移（CR-8 守恒）：付方扣减额 == 收方入账额，无中间态、无造币/丢币。
     *
     * 用【单一 Amount 局部变量】同时驱动 Debit(Payer,Amount,Reason)+Credit(Payee,Amount,Reason)，绝不两次重算（AC-2）。
     * 先验付方充足；不足则不发生任一腿、广播 OnInsufficientFunds（防造币）。
     * 收租用 Reason=Rent → 两腿 OnCashChanged 均 reason=Rent（AC-37）；OnRentPaid 由收租路径另发（story-004/005）。
     *
     * @return true=转移完成（payer/payee 各 1 次 OnCashChanged）；false=非法/零额/付方不足/任一玩家不存在。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool TransferCash(int32 PayerId, int32 PayeeId, int32 Amount, EChangeReason Reason);

    /**
     * 发放起始资金（数据驱动 Tuning Knob StartingCash，经 Credit 入账，reason=Salary 入账 faucet）。
     * @param PlayerId 玩家 ID
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    void GiveStartingCash(int32 PlayerId);

    // =========================================================================
    // Tuning Knob（数据驱动，coding-standards：gameplay 值不硬编码）
    // =========================================================================

    /** 每玩家起始现金（GDD Tuning：经典 1500；快速档 750 由上游配置覆盖）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy|Tuning")
    int32 StartingCash = 1500;

    // =========================================================================
    // 事件（econ-002，owner-held DYNAMIC_MULTICAST_DELEGATE，payload USTRUCT，ADR-0003）
    // =========================================================================

    /** 任何现金变动后广播（payload FCashChangedInfo，含 EChangeReason，AC-33）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnCashChangedSignature OnCashChanged;

    /** 收租转移完成时广播（payload FRentPaidInfo，AC-34；触发在 story-004/005）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnRentPaidSignature OnRentPaid;

    /** 强制扣款超现金时广播（payload FInsufficientFundsInfo，进 Raising Funds，AC-35）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnInsufficientFundsSignature OnInsufficientFunds;

    /** 破产现金侧转移完成时广播（payload FBankruptcyDeclaredInfo，2 字段，AC-36；触发在 story-009）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnBankruptcyDeclaredSignature OnBankruptcyDeclared;

private:
    /**
     * 解析 playerId → URentoPlayerState*（经回合2 FindPlayerById；用户裁定 Q2）。
     * @return 找到返回指针；World/turn/玩家缺失返回 nullptr。
     */
    URentoPlayerState* ResolvePlayer(int32 PlayerId) const;
};
