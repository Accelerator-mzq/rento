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
// 发薪 F-1 常量（economy 内部锁定，ADR-0014）
// =============================================================================
namespace
{
    // 溢出硬防护：clamp 上界 1000 × SalaryAmount≤2,000,000(board 加载期 fatal，propagate 债)
    //   = 2e9 < INT32_MAX(2,147,483,647)。真运行时 clamp（Shipping 亦生效）。
    constexpr int32 PASSED_GO_SAFE_MAX = 1000;
    // dev 暴露阈值：正常 passed_go ∈ -2..+2；>12 = 上游(移动/传送链)异常，UE_LOG(Error) 暴露。
    constexpr int32 PASSED_GO_ENSURE_MAX = 12;
    // SalaryAmount 溢出防护上界（ADR-0014 硬防护，economy 侧独立防线，对抗 review CONCERN-1）：
    //   board 加载期 fatal(≤2e6) 是首要防线(propagate 债)；此处 economy 自保——防 board 债未解期间
    //   1000×SalaryAmount 溢出 int32（仅 clamp passed_go 不足以保证不溢出，需两因子均有界）。
    //   2,000,000 × 1000 = 2e9 < INT32_MAX。
    constexpr int32 SALARY_AMOUNT_SAFE_MAX = 2000000;
    // 地产房屋档位上界（econ-004 F-2）：0=无房 .. 5=酒店；clamp 防建房8 越界。
    constexpr int32 PROPERTY_HOUSE_MAX = 5;
}

// =============================================================================
// PaySalary —— 发薪（F-1 / CR-2）
//   salary = clamp(max(passed_go,0),0,PASSED_GO_SAFE_MAX) × SalaryAmount；gate 在 passed_go。
//   AC-8「dev ensure」逸脱为 UE_LOG(Error)（G2 + 决定性，见 econ-003-LOCKED-brief §2）：
//     ensure once-only 顺序依赖且本仓库无 AddExpectedError 捕获 ensure 先例；硬保证=runtime clamp。
// =============================================================================
bool UEconomySubsystem::PaySalary(int32 PlayerId, int32 PassedGo, int32 SalaryAmount)
{
    // gate 在 passed_go（CR-2 防双重发薪）：passed_go≤0 一律不发、不广播。
    if (PassedGo <= 0)
    {
        return false;
    }

    // SalaryAmount 防御（economy 侧独立防线，CONCERN-1）：base 应为正（经典 200）且不超溢出上界。
    //   board 加载期 fatal(≤2e6) 是首要防线（propagate 债）；此处自保防 board 债未解期 1000×SalaryAmount UB。
    //   非正/超界 → dev log + 不发（refuse-not-clamp：拒绝算潜在溢出的薪资，暴露数据 bug，非静默错付）。
    if (SalaryAmount <= 0 || SalaryAmount > SALARY_AMOUNT_SAFE_MAX)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::PaySalary: SalaryAmount out of safe range [1,%d] (got %d, PlayerId=%d) -- no-op"),
            SALARY_AMOUNT_SAFE_MAX, SalaryAmount, PlayerId);
        return false;
    }

    // dev 信号（替 ensure，G2+决定性）：passed_go 异常大暴露上游 bug，不阻断、不替代 clamp。
    if (PassedGo > PASSED_GO_ENSURE_MAX)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::PaySalary: passed_go=%d abnormally large (>%d) -- upstream movement/teleport bug; clamped"),
            PassedGo, PASSED_GO_ENSURE_MAX);
    }

    // F-1 真运行时 clamp（Shipping 亦生效）：clamp(max(passed_go,0),0,1000) × SalaryAmount。
    //   max 保 F-1 形式（gate 已保 >0）；clamp 上界 1000 防 int32 溢出（AC-8：10⁷→1000）。
    const int32 ClampedPassedGo = FMath::Clamp(FMath::Max(PassedGo, 0), 0, PASSED_GO_SAFE_MAX);
    const int32 Salary = ClampedPassedGo * SalaryAmount;

    // 经 Credit 入账（reason=Salary）；Credit 内保 amount≥0/玩家存在/广播 OnCashChanged。
    return Credit(PlayerId, Salary, EChangeReason::Salary);
}

// =============================================================================
// ComputePropertyRent —— 地产租金 F-2 piecewise（纯函数，RentTable 注入）
// =============================================================================
int32 UEconomySubsystem::ComputePropertyRent(bool bIsMortgaged, bool bIsMonopoly, int32 HouseCount, const TArray<int32>& RentTable) const
{
    // ① 短路抵押先于表查找（CR-3/AC-9）：抵押地不收租。
    if (bIsMortgaged)
    {
        return 0;
    }

    // RentTable 空表防御（board 数据 bug）：无法查找 → 0 + dev log。
    if (RentTable.Num() == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputePropertyRent: RentTable empty -- rent 0"));
        return 0;
    }

    // ② house_count 越界 dev 信号（替 ensure，G2+决定性，AC-13）：正常 0..5，越界暴露建房8 bug。
    if (HouseCount < 0 || HouseCount > PROPERTY_HOUSE_MAX)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputePropertyRent: house_count=%d out of range [0,%d] -- clamped"),
            HouseCount, PROPERTY_HOUSE_MAX);
    }

    // ③ 垄断翻倍仅作用无房 base（raw house==0，AC-10）；酒店/有房 raw≠0 落 else 不翻倍（AC-11）。
    //   溢出境界（N-1）：RentTable[0]（board base，加载期校验）× MonopolyRentMultiplier（MVP 锁定 2）
    //   = 经典值远 < INT32_MAX；两因子均静态数据（board RentTable + editor 配置），非 runtime abuse 输入。
    if (bIsMonopoly && HouseCount == 0)
    {
        return RentTable[0] * MonopolyRentMultiplier;
    }

    // ④ 否则按房数查表，index 双重 clamp（[0,5] ∩ [0,Num-1]，防 RentTable 短于 6 越界）。
    const int32 MaxIndex     = FMath::Min(PROPERTY_HOUSE_MAX, RentTable.Num() - 1);
    const int32 ClampedHouse = FMath::Clamp(HouseCount, 0, MaxIndex);
    return RentTable[ClampedHouse];
}

// =============================================================================
// SettleRent —— 收租结算（共有 F-2/F-3/F-4）：原子转移 + OnRentPaid
// =============================================================================
bool UEconomySubsystem::SettleRent(int32 PayerId, int32 PayeeId, int32 RentAmount, int32 TileIndex)
{
    // rent≤0（抵押/自有/无主）→ 不转移、不广播（AC-5/37）。
    //   不变式（N-2）：ComputePropertyRent 恒返 ≥0；负值不可达，<0 与零额合流静默 false 兜底。
    if (RentAmount <= 0)
    {
        return false;
    }

    // 原子转移（CR-8）：reason=Rent（两腿 OnCashChanged=Rent，AC-37）；
    //   付方不足额 → TransferCash 内广播 OnInsufficientFunds + 返回 false → 不广播 OnRentPaid（未真收租）。
    if (!TransferCash(PayerId, PayeeId, RentAmount, EChangeReason::Rent))
    {
        return false;
    }

    // 收租成功 → 广播 OnRentPaid（4 字段，AC-34；TileIndex=地产格非 Payee 所在格）。
    FRentPaidInfo Info;
    Info.PayerId   = PayerId;
    Info.PayeeId   = PayeeId;
    Info.Amount    = RentAmount;
    Info.TileIndex = TileIndex;
    OnRentPaid.Broadcast(Info);
    return true;
}

// =============================================================================
// GiveStartingCash —— 数据驱动起始资金（reason=Salary：起始资金=入账 faucet 语境）
// =============================================================================
void UEconomySubsystem::GiveStartingCash(int32 PlayerId)
{
    Credit(PlayerId, StartingCash, EChangeReason::Salary);
}
