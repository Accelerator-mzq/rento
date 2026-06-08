// EconomyCashSubsystem.cpp
// =============================================================================
// UEconomyCashSubsystem 实现 —— econ-001。
// 单源 Cash（写 URentoPlayerState.Cash via SetCash）；解析经回合2 FindPlayerById。
// =============================================================================

#include "EconomyCashSubsystem.h"

#include "RentoPlayerState.h"
#include "PlayerTurnSubsystem.h"
#include "Engine/World.h"

// =============================================================================
// ResolvePlayer —— playerId → URentoPlayerState*（用户裁定 Q2）
// =============================================================================
URentoPlayerState* UEconomyCashSubsystem::ResolvePlayer(int32 PlayerId) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    UPlayerTurnSubsystem* Turn = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!Turn)
    {
        return nullptr;
    }

    return Turn->FindPlayerById(PlayerId);
}

// =============================================================================
// GetCash
// =============================================================================
int32 UEconomyCashSubsystem::GetCash(int32 PlayerId) const
{
    const URentoPlayerState* PS = ResolvePlayer(PlayerId);
    if (!PS)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UEconomyCashSubsystem::GetCash: player %d not found -- returning 0"),
            PlayerId);
        return 0;
    }
    return PS->Cash;
}

// =============================================================================
// Credit —— 入账
// =============================================================================
bool UEconomyCashSubsystem::Credit(int32 PlayerId, int32 Amount)
{
    // amount<0 非法（CR-1 R1 blocker#2）：丢弃 + dev log，不改 Cash、不广播。
    // 用 UE_LOG(Error) 而非 ensure（G2：Shipping 下 ensure 空，且 test 须可 AddExpectedError 捕获）。
    if (Amount < 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomyCashSubsystem::Credit: amount must be non-negative (got %d, PlayerId=%d) -- rejected"),
            Amount, PlayerId);
        return false;
    }

    // amount==0 早返静默（AC-5）：不记账、不广播。
    if (Amount == 0)
    {
        return false;
    }

    URentoPlayerState* PS = ResolvePlayer(PlayerId);
    if (!PS)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomyCashSubsystem::Credit: player %d not found -- no-op"),
            PlayerId);
        return false;
    }

    const int32 OldCash = PS->Cash;
    const int32 NewCash = OldCash + Amount;
    PS->SetCash(NewCash);                              // 单源受控写（经济5 唯一合法调用方）
    OnCashChanged.Broadcast(PlayerId, OldCash, NewCash);
    return true;
}

// =============================================================================
// Debit —— 出账（守 Cash>=0）
// =============================================================================
bool UEconomyCashSubsystem::Debit(int32 PlayerId, int32 Amount)
{
    if (Amount < 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomyCashSubsystem::Debit: amount must be non-negative (got %d, PlayerId=%d) -- rejected"),
            Amount, PlayerId);
        return false;
    }

    if (Amount == 0)
    {
        return false;
    }

    URentoPlayerState* PS = ResolvePlayer(PlayerId);
    if (!PS)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomyCashSubsystem::Debit: player %d not found -- no-op"),
            PlayerId);
        return false;
    }

    const int32 OldCash = PS->Cash;

    // Cash>=0 不变式硬守（AC-1）：不足额不落地为负，转 Raising Funds。
    if (Amount > OldCash)
    {
        const int32 AmountShort = Amount - OldCash;
        OnInsufficientFunds.Broadcast(PlayerId, /*AmountDue=*/Amount, AmountShort);
        return false;                                 // 不扣、不广播 OnCashChanged
    }

    const int32 NewCash = OldCash - Amount;
    PS->SetCash(NewCash);
    OnCashChanged.Broadcast(PlayerId, OldCash, NewCash);
    return true;
}

// =============================================================================
// TransferCash —— 原子转移（CR-8）
// =============================================================================
bool UEconomyCashSubsystem::TransferCash(int32 PayerId, int32 PayeeId, int32 Amount)
{
    if (Amount < 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomyCashSubsystem::TransferCash: amount must be non-negative (got %d) -- rejected"),
            Amount);
        return false;
    }

    if (Amount == 0)
    {
        return false;
    }

    URentoPlayerState* Payer = ResolvePlayer(PayerId);
    URentoPlayerState* Payee = ResolvePlayer(PayeeId);
    if (!Payer || !Payee)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomyCashSubsystem::TransferCash: payer %d or payee %d not found -- no-op"),
            PayerId, PayeeId);
        return false;
    }

    // CR-8 原子守恒：先验付方充足，避免「Debit 失败而 Credit 仍执行」造币。
    if (Amount > Payer->Cash)
    {
        const int32 AmountShort = Amount - Payer->Cash;
        OnInsufficientFunds.Broadcast(PayerId, /*AmountDue=*/Amount, AmountShort);
        return false;                                 // 无中间态：任一腿不可行则整体不发生
    }

    // 单一 Amount 局部变量驱动两腿（AC-2：非两次重算）。付方已验充足 → Debit 必成功。
    // CR-8 原子契约自强制（非仅依赖前置推理）：Debit 失败则整体 no-op（Debit 失败路径均在 SetCash 前
    // return，无任何腿落地）；Credit 在 Payee 已验非空 + Amount>0 下不可失败，ensure 守不可达的毁币。
    if (!Debit(PayerId, Amount))                        // 付方 1 次 OnCashChanged
    {
        return false;                                  // 付方扣款失败（理论不可达）→ 原子 no-op
    }
    const bool bCreditOk = Credit(PayeeId, Amount);    // 收方 1 次 OnCashChanged
    ensureAlwaysMsgf(bCreditOk,
        TEXT("UEconomyCashSubsystem::TransferCash: Credit leg failed after Debit succeeded -- money destroyed"));
    return bCreditOk;
}

// =============================================================================
// GiveStartingCash —— 数据驱动起始资金
// =============================================================================
void UEconomyCashSubsystem::GiveStartingCash(int32 PlayerId)
{
    Credit(PlayerId, StartingCash);
}
