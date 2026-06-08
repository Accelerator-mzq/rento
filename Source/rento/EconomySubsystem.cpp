// EconomySubsystem.cpp
// =============================================================================
// UEconomySubsystem 实现 —— econ-001（受控写/不变式）+ econ-002（事件契约广播）。
// 单源 Cash（写 URentoPlayerState.Cash via SetCash）；解析经回合2 FindPlayerById。
// =============================================================================

#include "EconomySubsystem.h"

#include "RentoPlayerState.h"
#include "PlayerTurnSubsystem.h"
#include "Engine/World.h"

// =============================================================================
// EChangeReason append-only 编译期门（AC-39：核验 9 既有值不改序）。
// 新 reason 只追加在 Bankruptcy 之后；既有值序号永不改。
// =============================================================================
static_assert(static_cast<uint8>(EChangeReason::Salary)     == 0, "EChangeReason append-only: Salary must stay 0");
static_assert(static_cast<uint8>(EChangeReason::Rent)       == 1, "EChangeReason append-only: Rent must stay 1");
static_assert(static_cast<uint8>(EChangeReason::Purchase)   == 2, "EChangeReason append-only: Purchase must stay 2");
static_assert(static_cast<uint8>(EChangeReason::Mortgage)   == 3, "EChangeReason append-only: Mortgage must stay 3");
static_assert(static_cast<uint8>(EChangeReason::Unmortgage) == 4, "EChangeReason append-only: Unmortgage must stay 4");
static_assert(static_cast<uint8>(EChangeReason::Tax)        == 5, "EChangeReason append-only: Tax must stay 5");
static_assert(static_cast<uint8>(EChangeReason::Build)      == 6, "EChangeReason append-only: Build must stay 6");
static_assert(static_cast<uint8>(EChangeReason::Trade)      == 7, "EChangeReason append-only: Trade must stay 7");
static_assert(static_cast<uint8>(EChangeReason::Bankruptcy) == 8, "EChangeReason append-only: Bankruptcy must stay 8");

// =============================================================================
// ResolvePlayer —— playerId → URentoPlayerState*（用户裁定 Q2）
// =============================================================================
URentoPlayerState* UEconomySubsystem::ResolvePlayer(int32 PlayerId) const
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
int32 UEconomySubsystem::GetCash(int32 PlayerId) const
{
    const URentoPlayerState* PS = ResolvePlayer(PlayerId);
    if (!PS)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UEconomySubsystem::GetCash: player %d not found -- returning 0"),
            PlayerId);
        return 0;
    }
    return PS->Cash;
}

// =============================================================================
// Credit —— 入账
// =============================================================================
bool UEconomySubsystem::Credit(int32 PlayerId, int32 Amount, EChangeReason Reason)
{
    // amount<0 非法（CR-1 R1 blocker#2）：丢弃 + dev log，不改 Cash、不广播。
    // 用 UE_LOG(Error) 而非 ensure（G2：Shipping 下 ensure 空，且 test 须可 AddExpectedError 捕获）。
    if (Amount < 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::Credit: amount must be non-negative (got %d, PlayerId=%d) -- rejected"),
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
            TEXT("UEconomySubsystem::Credit: player %d not found -- no-op"),
            PlayerId);
        return false;
    }

    const int32 OldCash = PS->Cash;
    const int32 NewCash = OldCash + Amount;
    PS->SetCash(NewCash);                              // 单源受控写（经济5 唯一合法调用方）

    FCashChangedInfo Info;
    Info.PlayerId = PlayerId;
    Info.OldCash  = OldCash;
    Info.NewCash  = NewCash;
    Info.Reason   = Reason;
    OnCashChanged.Broadcast(Info);
    return true;
}

// =============================================================================
// Debit —— 出账（守 Cash>=0）
// =============================================================================
bool UEconomySubsystem::Debit(int32 PlayerId, int32 Amount, EChangeReason Reason)
{
    if (Amount < 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::Debit: amount must be non-negative (got %d, PlayerId=%d) -- rejected"),
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
            TEXT("UEconomySubsystem::Debit: player %d not found -- no-op"),
            PlayerId);
        return false;
    }

    const int32 OldCash = PS->Cash;

    // Cash>=0 不变式硬守（AC-1）：不足额不落地为负，转 Raising Funds。
    if (Amount > OldCash)
    {
        FInsufficientFundsInfo Info;
        Info.PlayerId    = PlayerId;
        Info.AmountDue   = Amount;
        Info.AmountShort = Amount - OldCash;
        OnInsufficientFunds.Broadcast(Info);
        return false;                                 // 不扣、不广播 OnCashChanged
    }

    const int32 NewCash = OldCash - Amount;
    PS->SetCash(NewCash);

    FCashChangedInfo Info;
    Info.PlayerId = PlayerId;
    Info.OldCash  = OldCash;
    Info.NewCash  = NewCash;
    Info.Reason   = Reason;
    OnCashChanged.Broadcast(Info);
    return true;
}

// =============================================================================
// TransferCash —— 原子转移（CR-8）
// =============================================================================
bool UEconomySubsystem::TransferCash(int32 PayerId, int32 PayeeId, int32 Amount, EChangeReason Reason)
{
    if (Amount < 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::TransferCash: amount must be non-negative (got %d) -- rejected"),
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
            TEXT("UEconomySubsystem::TransferCash: payer %d or payee %d not found -- no-op"),
            PayerId, PayeeId);
        return false;
    }

    // CR-8 原子守恒：先验付方充足，避免「Debit 失败而 Credit 仍执行」造币。
    if (Amount > Payer->Cash)
    {
        FInsufficientFundsInfo Info;
        Info.PlayerId    = PayerId;
        Info.AmountDue   = Amount;
        Info.AmountShort = Amount - Payer->Cash;
        OnInsufficientFunds.Broadcast(Info);
        return false;                                 // 无中间态：任一腿不可行则整体不发生
    }

    // 单一 Amount 局部变量驱动两腿（AC-2：非两次重算）；同一 Reason（收租=Rent，两腿 OnCashChanged 均 Rent）。
    // CR-8 原子契约自强制：Debit 失败则整体 no-op；Credit 在 Payee 已验非空 + Amount>0 下不可失败，ensure 守毁币。
    if (!Debit(PayerId, Amount, Reason))               // 付方 1 次 OnCashChanged
    {
        return false;
    }
    const bool bCreditOk = Credit(PayeeId, Amount, Reason);  // 收方 1 次 OnCashChanged
    ensureAlwaysMsgf(bCreditOk,
        TEXT("UEconomySubsystem::TransferCash: Credit leg failed after Debit succeeded -- money destroyed"));
    return bCreditOk;
}

// =============================================================================
// GiveStartingCash —— 数据驱动起始资金（reason=Salary：起始资金=入账 faucet 语境）
// =============================================================================
void UEconomySubsystem::GiveStartingCash(int32 PlayerId)
{
    Credit(PlayerId, StartingCash, EChangeReason::Salary);
}
