// EconomyCashSubsystem.h
// =============================================================================
// UEconomyCashSubsystem —— 经济与现金服务（econ-001 / TR-econ-001/002/018，amount≥0 部分 TR-econ-014）
//
// 形态：per-match UWorldSubsystem（继承 UMatchSubsystemBase，一局边界、PIE 隔离、ADR-0001）。
//
// 职责（econ-001 scope）：
//   - 现金受控写：Credit / Debit / TransferCash（CR-8 原子守恒）
//   - 现金只读：GetCash
//   - 起始资金：GiveStartingCash（数据驱动 Tuning Knob，coding-standards 非硬编码）
//   - 不变式：Cash≥0（Debit 不足额不落地、转 Raising Funds）/ amount≥0（负 amount 非法拒绝）/ amount==0 早返静默
//
// 单源真相（用户 2026-06-08 裁定 Q1）：
//   本服务【不持任何 cash 容器】。Cash 的 field of record = URentoPlayerState.Cash（player-turn 拥有字段、
//   SetCash 字段级 setter）。本服务 Credit/Debit 读 PS->Cash、经 PS->SetCash 写——pt-007 读 PS->Cash 的
//   清算/快照/竞拍逻辑因此看到真值（禁用 private TMap，否则双源 stale）。
//
// 解析（用户裁定 Q2）：
//   playerId → URentoPlayerState* 经 GetWorld()->GetSubsystem<UPlayerTurnSubsystem>()->FindPlayerById(id)。
//   运行时依赖（story 头 Dependencies:None 指 build/story 级；此为运行时）：解析依赖回合2 已
//   InitializeFromConfig 建好 player states。turn/player 未就绪时各操作安全 no-op（dev log + return false），
//   不崩溃、不改 Cash、不广播——调用方（回合2 ResolvePhase）保证在建队后才结算。
//
// 事件（占位，story-002 升级 payload+EChangeReason）：
//   OnCashChanged(PlayerId, OldCash, NewCash) / OnInsufficientFunds(PlayerId, AmountDue, AmountShort)。
//   ⚠ story-002 将这两个 delegate 升级为带 EChangeReason / payload USTRUCT（pt-004 升级先例），
//     并新增 OnRentPaid / OnBankruptcyDeclared。本 story 只落占位签名供不变式逻辑验证。
//
// 规范依据：
//   - GDD economy-cash.md CR-1（受控写/Cash≥0/amount≥0）、CR-8（原子守恒/无限银行）、AC-1~5、amount<0 契约
//   - ADR-0001（UObject 宿主：per-match UWorldSubsystem）
//   - ADR-0007（写权威状态→C++；Credit/Debit/GetCash 标 BlueprintCallable）
//   - ADR-0014（金钱运算整数纯净；本 story 仅加减无 num/den，溢出硬防护在 econ-003+）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "EconomyCashSubsystem.generated.h"

class URentoPlayerState;

// 占位 delegate（story-002 升级为带 EChangeReason / payload USTRUCT）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnCashChangedSignature, int32, PlayerId, int32, OldCash, int32, NewCash);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnInsufficientFundsSignature, int32, PlayerId, int32, AmountDue, int32, AmountShort);

/**
 * UEconomyCashSubsystem —— 经济现金权威服务。
 *
 * 不持 cash 容器；Cash 单源 = URentoPlayerState.Cash，经 SetCash 受控写。
 */
UCLASS()
class RENTO_API UEconomyCashSubsystem : public UMatchSubsystemBase
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
     * 成功 → PS->SetCash(Old+Amount) + 广播 OnCashChanged。
     *
     * @return true=已入账并广播；false=非法/零额/玩家不存在（未改 Cash）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool Credit(int32 PlayerId, int32 Amount);

    /**
     * 出账（Cash -= Amount），守 Cash≥0 不变式。
     *
     * amount<0 非法 → dev log + 拒绝、不改 Cash、不广播。
     * amount==0 早返静默 → 不改 Cash、不广播。
     * Amount > Cash（不足额，AC-1）→ 不扣、广播 OnInsufficientFunds、不广播 OnCashChanged（转 Raising Funds，状态机在 story-009）。
     * 成功 → PS->SetCash(Old-Amount) + 广播 OnCashChanged。
     *
     * @return true=已出账并广播；false=非法/零额/不足额/玩家不存在（未改 Cash）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool Debit(int32 PlayerId, int32 Amount);

    /**
     * 原子转移（CR-8 守恒）：付方扣减额 == 收方入账额，无中间态、无造币/丢币。
     *
     * 用【单一 Amount 局部变量】同时驱动 Debit(Payer,Amount)+Credit(Payee,Amount)，绝不两次重算（AC-2）。
     * 先验付方充足（Amount<=Payer.Cash）：不足则不发生任一腿、广播 OnInsufficientFunds（防 Debit 失败而 Credit 仍执行 → 造币）。
     * 银行（发薪/抵押放款/税）走 Credit/Debit 单腿，不经本接口（CR-8 银行=无限池）。
     *
     * @return true=转移完成（payer/payee 各 1 次 OnCashChanged）；false=非法/零额/付方不足/任一玩家不存在。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool TransferCash(int32 PayerId, int32 PayeeId, int32 Amount);

    /**
     * 发放起始资金（数据驱动 Tuning Knob StartingCash，经 Credit 入账）。
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
    // 事件（占位，story-002 升级 payload+EChangeReason）
    // =========================================================================

    /** 任何现金变动后广播（AC-33 完整 payload 在 story-002）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnCashChangedSignature OnCashChanged;

    /** 强制扣款超现金时广播（进 Raising Funds，AC-35）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnInsufficientFundsSignature OnInsufficientFunds;

private:
    /**
     * 解析 playerId → URentoPlayerState*（经回合2 FindPlayerById；用户裁定 Q2）。
     * @return 找到返回指针；World/turn/玩家缺失返回 nullptr。
     */
    URentoPlayerState* ResolvePlayer(int32 PlayerId) const;
};
