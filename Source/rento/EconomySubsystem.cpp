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
    // 车站持有数上界（econ-005 F-3）：1..4 站 → index 0..3。
    constexpr int32 RAILROAD_STATION_MAX = 4;
    // 公用持有数上界（econ-005 F-4）：1..2 → index 0..1。
    constexpr int32 UTILITY_COUNT_MAX = 2;
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
// ComputeRailroadRent —— 车站租金 F-3（纯函数）
// =============================================================================
int32 UEconomySubsystem::ComputeRailroadRent(int32 StationCount, const TArray<int32>& RentTable) const
{
    // 守 station_count≥1（AC-15 关键 guard）：无站置 0，不查 RentTable[−1]，dev 信号暴露地产6 竞态。
    if (StationCount < 1)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputeRailroadRent: station_count=%d < 1 -- rent 0 (ownership race?)"),
            StationCount);
        return 0;
    }

    if (RentTable.Num() == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputeRailroadRent: RentTable empty -- rent 0"));
        return 0;
    }

    // index = clamp(station−1, 0, 3) ∩ [0,Num-1]（双重 clamp 防表短越界；station>4 静默兜底）。
    const int32 MaxIndex = FMath::Min(RAILROAD_STATION_MAX - 1, RentTable.Num() - 1);
    const int32 Index    = FMath::Clamp(StationCount - 1, 0, MaxIndex);
    return RentTable[Index];
}

// =============================================================================
// ComputeUtilityRent —— 公用租金 F-4（纯函数，dice_total 显式参数 AC-18）
// =============================================================================
int32 UEconomySubsystem::ComputeUtilityRent(int32 UtilityCount, int32 DiceTotal, const TArray<int32>& DiceMultiplierTable) const
{
    // 守 utility_count≥1（AC-17 关键 guard）：无公用置 0，不查 [−1]，dev 信号暴露地产6 竞态。
    if (UtilityCount < 1)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputeUtilityRent: utility_count=%d < 1 -- rent 0 (ownership race?)"),
            UtilityCount);
        return 0;
    }

    // dice_total 防御（economy 侧独立防线，对抗 review CONCERN-3）：≤0 = holder/上游异常
    //   （ADR-0015 holder 应保 dice_total∈[2,12]）；拒绝算负/零租、不返负值，dev log 暴露上游 bug。
    if (DiceTotal <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputeUtilityRent: dice_total=%d <= 0 -- rent 0 (holder/upstream bug)"),
            DiceTotal);
        return 0;
    }

    if (DiceMultiplierTable.Num() == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputeUtilityRent: DiceMultiplierTable empty -- rent 0"));
        return 0;
    }

    // index = clamp(utility−1, 0, 1) ∩ [0,Num-1]；rent = dice_total（显式参数，不缓存）× 倍率。
    //   Num()≥1 由上方空表早返保证（NIT-1：MaxIndex 的 Num-1≥0 依赖此顺序）。
    const int32 MaxIndex = FMath::Min(UTILITY_COUNT_MAX - 1, DiceMultiplierTable.Num() - 1);
    const int32 Index    = FMath::Clamp(UtilityCount - 1, 0, MaxIndex);
    return DiceTotal * DiceMultiplierTable[Index];
}

// =============================================================================
// GetUnmortgageCostForDisplay —— 赎回价 F-6（纯函数，整数 ceil，单一口径）
// =============================================================================
int32 UEconomySubsystem::GetUnmortgageCostForDisplay(int32 MortgageValue) const
{
    // MV 防御（board 应保 1..PurchasePrice）：≤0 = 无效数据 → 0 + dev log（防负 cost）。
    if (MortgageValue <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::GetUnmortgageCostForDisplay: MortgageValue=%d <= 0 -- cost 0 (invalid board data?)"),
            MortgageValue);
        return 0;
    }

    // fee_den 防御（对抗 review CONCERN-2）：den≤0 = 配置异常 → 退化为零 fee（cost=MV），避免除零崩溃。
    //   meta ClampMin 仅约束 editor，code/data-asset 仍可置 0，故 runtime guard 必需。
    if (UnmortgageFeeDen <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::GetUnmortgageCostForDisplay: UnmortgageFeeDen=%d <= 0 -- zero-fee fallback (config bug)"),
            UnmortgageFeeDen);
        return MortgageValue;                          // 零 fee 退化（不崩、不返负）
    }

    // 整数 ceil（ADR-0014，零 float）：fee = ceil(MV × num/den) = (MV×num + den−1)/den。
    //   fee 恒≥1（MV≥1 且 fee_num/den=1/10 默认时；fee_num=0 零 fee 是合法 House Rules 调参 GDD Tuning）。
    //   溢出境界（CONCERN-1）：MV≤PurchasePrice（board 校验，经典≤400）× num（ClampMax 束缚）< INT32_MAX。
    const int32 Fee = (MortgageValue * UnmortgageFeeNum + UnmortgageFeeDen - 1) / UnmortgageFeeDen;
    return MortgageValue + Fee;
}

// =============================================================================
// MortgagePayout —— 抵押放款现金腿 F-5（被地产6 调，6→5）
// =============================================================================
bool UEconomySubsystem::MortgagePayout(int32 PlayerId, int32 MortgageValue)
{
    // F-5：payout = MortgageValue（Credit，reason=Mortgage）。前置归6/决策点，非此处（AC-20）。
    return Credit(PlayerId, MortgageValue, EChangeReason::Mortgage);
}

// =============================================================================
// UnmortgagePayment —— 赎回现金腿 F-6（被地产6 调，6→5）
// =============================================================================
bool UEconomySubsystem::UnmortgagePayment(int32 PlayerId, int32 MortgageValue)
{
    const int32 Cost = GetUnmortgageCostForDisplay(MortgageValue);
    if (Cost <= 0)
    {
        return false;                                  // 无效 MV（已 log）
    }

    // AC-22：赎回是自愿行为，现金不足 → 不可用（不 Debit、不广播、return false）。
    //   显式 pre-check 避免 Debit 内 OnInsufficientFunds 误触发 raising-funds（raising-funds 仅强制扣款 rent/tax 才入）。
    if (GetCash(PlayerId) < Cost)
    {
        return false;                                  // 地保持抵押（6 不解除标记）
    }

    return Debit(PlayerId, Cost, EChangeReason::Unmortgage);
}

// =============================================================================
// GiveStartingCash —— 数据驱动起始资金（reason=Salary：起始资金=入账 faucet 语境）
// =============================================================================
void UEconomySubsystem::GiveStartingCash(int32 PlayerId)
{
    Credit(PlayerId, StartingCash, EChangeReason::Salary);
}

// =============================================================================
// PayTax —— 缴税现金腿 F-7（被事件7 调，7→5；强制扣款，sink 蒸发）
// =============================================================================
bool UEconomySubsystem::PayTax(int32 PlayerId, int32 TaxAmount)
{
    // F-7：flat Debit(reason=Tax)，款蒸发（无 Credit 任何玩家，CR-6）。
    //   税强制：不足额 → Debit 内广播 OnInsufficientFunds 进 Raising Funds（同 AC-1）。
    //   amount<0 拒绝 / ==0 静默 由 Debit 通用守门处理（不在此重复）。
    return Debit(PlayerId, TaxAmount, EChangeReason::Tax);
}

// =============================================================================
// BuyPropertyCashLeg —— 买地现金腿 CR-4（被地产6 Buy 事务调，6→5；可选，不足不可用）
// =============================================================================
bool UEconomySubsystem::BuyPropertyCashLeg(int32 PlayerId, int32 PurchasePrice)
{
    // PurchasePrice 防御（board 应保正，经典 60..400）：≤0 = 无效数据 → 不购买 + dev log。
    if (PurchasePrice <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::BuyPropertyCashLeg: PurchasePrice=%d <= 0 (PlayerId=%d) -- no-op (invalid board data?)"),
            PurchasePrice, PlayerId);
        return false;
    }

    // CR-4 买地可选：现金不足 → 购买不可用（不 Debit、不广播、不进 raising-funds）。
    //   显式 pre-check 避免 Debit 内 OnInsufficientFunds 误触发 raising-funds（仅强制扣款 rent/tax 才入，仿 UnmortgagePayment/AC-22）。
    if (GetCash(PlayerId) < PurchasePrice)
    {
        return false;                                   // 购买不可用，非强制债务
    }

    // 扣款腿（reason=Purchase）；economy 只扣款，不登记归属、不通知6（5↔6 无环）。
    return Debit(PlayerId, PurchasePrice, EChangeReason::Purchase);
}

// =============================================================================
// ComputeBuildingSellback —— 建筑卖回价 F-8（纯函数，整数 floor）
// =============================================================================
int32 UEconomySubsystem::ComputeBuildingSellback(int32 BuildingCost) const
{
    // BuildingCost 防御（board 应保正）：≤0 = 无效数据 → 0 + dev log（防负卖回）。
    if (BuildingCost <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputeBuildingSellback: BuildingCost=%d <= 0 -- sellback 0 (invalid board data?)"),
            BuildingCost);
        return 0;
    }

    // den 防御（meta ClampMin 仅约束 editor，code/data-asset 仍可置 0）：→ 零卖回退化，防除零。
    if (BuildingSellbackDen <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::ComputeBuildingSellback: BuildingSellbackDen=%d <= 0 -- sellback 0 (config bug)"),
            BuildingSellbackDen);
        return 0;
    }

    // 整数 floor（ADR-0014，零 float）：floor(BC×num/den) = (BC×num)/den（操作数≥0 截断==floor）。
    return (BuildingCost * BuildingSellbackNum) / BuildingSellbackDen;
}

// =============================================================================
// ComputeNetLiquidationValue —— NLV F-9（纯函数，单一枚举入口，逐栋 floor 先于求和）
// =============================================================================
int32 UEconomySubsystem::ComputeNetLiquidationValue(const TArray<FNlvAssetEntry>& Assets) const
{
    int32 Nlv = 0;
    for (const FNlvAssetEntry& Entry : Assets)
    {
        // MV 侧：仅未抵押地计入（已抵押贡献 0，AC-26）。
        if (!Entry.bIsMortgaged)
        {
            Nlv += Entry.MortgageValue;
        }

        // 卖回侧：逐栋 floor 先于求和（🔴 AC-27/AC-32 变体C）——
        //   ComputeBuildingSellback 已对单栋 floor；× HouseCount 再累加，绝不 floor(Σ)。
        // 负 HouseCount（建房8 上游 bug）dev 信号暴露（review W-1，对齐 ComputePropertyRent house_count 纪律）——
        //   不影响数值（贡献 0），但不静默吞上游 bug。
        if (Entry.HouseCount < 0)
        {
            UE_LOG(LogTemp, Error,
                TEXT("UEconomySubsystem::ComputeNetLiquidationValue: HouseCount=%d < 0 -- skipped (building-8 bug?)"),
                Entry.HouseCount);
        }
        else if (Entry.HouseCount > 0)
        {
            Nlv += Entry.HouseCount * ComputeBuildingSellback(Entry.BuildingCost);
        }
    }
    return Nlv;
}

// =============================================================================
// IsInsolvent —— 破产谓词 F-10（纯谓词，严格 <，消费传入 nlv 不重算）
// =============================================================================
bool UEconomySubsystem::IsInsolvent(int32 PlayerId, int32 AmountDue, int32 PreaggregatedNlv) const
{
    // 负 nlv 防御（review W-2）：F-9 恒 ≥0，传入负值=调用方 bug → dev log + 保守按 0
    //   （不让负 nlv 污染破产判定；亦避免极端负值的 int32 加法异常）。
    if (PreaggregatedNlv < 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UEconomySubsystem::IsInsolvent: PreaggregatedNlv=%d < 0 (caller bug) -- treating as 0"),
            PreaggregatedNlv);
        return GetCash(PlayerId) < AmountDue;
    }

    // 严格 <：付到恰好 0 算能付（Cash+nlv==due → false，AC-28）。消费传入 nlv，不枚举/不重算（AC-29）。
    return (GetCash(PlayerId) + PreaggregatedNlv) < AmountDue;
}

// =============================================================================
// DecideNextLiquidationStep —— 强制清算顺序 spec（econ-009，纯静态，mortgage-empty-first）
//   economy 拥有顺序；回合2 据返回驱动调 6/8（economy 不直调，防环 ADR-0006）。
// =============================================================================
ELiquidationAction UEconomySubsystem::DecideNextLiquidationStep(
    int32 Cash, int32 AmountDue, const TArray<FNlvAssetEntry>& Assets, int32& OutTargetTile)
{
    OutTargetTile = INDEX_NONE;

    // 偿付成功（Cash≥应付）→ 停止清算。
    if (Cash >= AmountDue)
    {
        return ELiquidationAction::None;
    }

    // ① mortgage-empty-first（止损优先）：未抵押 ∧ 无房地中选 MortgageValue 最小者。
    //   抵押可赎回（零损），故先于永久半价亏损的卖房；带房地不可抵押（经典规则），故 HouseCount==0 限定。
    //   Assets 已由回合2 筛 owner==player。
    int32 BestTile = INDEX_NONE;
    int32 BestMV   = MAX_int32;
    for (const FNlvAssetEntry& E : Assets)
    {
        // MV>0 守门（review W-1）：MV==0 异常数据（board bug）抵押筹不到现金，跳过——
        //   否则会选中无价值格做无效抵押（Credit 0 静默），真实地产6 不标记则死循环靠 SafetyGuard 兜底。
        if (!E.bIsMortgaged && E.HouseCount == 0 && E.MortgageValue > 0)
        {
            if (E.MortgageValue < BestMV)
            {
                BestMV   = E.MortgageValue;
                BestTile = E.TileIndex;
            }
        }
    }
    if (BestTile != INDEX_NONE)
    {
        OutTargetTile = BestTile;
        return ELiquidationAction::MortgageTile;
    }

    // ② 无可抵押空地但有建筑 → 卖房腿（回合2 调 8.ForcedSellNextBuilding 选全盘最高档）。
    for (const FNlvAssetEntry& E : Assets)
    {
        if (E.HouseCount > 0)
        {
            return ELiquidationAction::SellBuilding;
        }
    }

    // ③ 资产耗尽仍 Cash<应付 → 破产（结构性判定 ⟺ Cash+总 nlv<应付；NLV canonical 口径见 ComputeNetLiquidationValue）。
    return ELiquidationAction::Insolvent;
}

// =============================================================================
// SettleBankruptcy —— 破产现金侧结算 F-11（econ-009，被破产9 资产移交后调）
// =============================================================================
void UEconomySubsystem::SettleBankruptcy(int32 DebtorId, int32 CreditorId)
{
    // 前置条件（调用方破产9 保证，review INFO）：DebtorId != CreditorId（自转语义无效，会双发 OnCashChanged）。
    const int32 DebtorCash = GetCash(DebtorId);

    if (CreditorId == INDEX_NONE)
    {
        // 银行破产（税/银行）：现金蒸发，不入任何玩家（AC-31）。DebtorCash==0 时 Debit 静默早返。
        if (DebtorCash > 0)
        {
            Debit(DebtorId, DebtorCash, EChangeReason::Bankruptcy);
        }
    }
    else
    {
        // 玩家债主：全额现金转移（AC-30，MVP 不收继承利息）。TransferCash 守恒（Amount==DebtorCash 不超额）。
        if (DebtorCash > 0)
        {
            TransferCash(DebtorId, CreditorId, DebtorCash, EChangeReason::Bankruptcy);
        }
    }

    // 现金侧移交完成 → 广播 OnBankruptcyDeclared 恰一次（AC-36，即便 DebtorCash==0 破产仍成立）。
    //   资产 in-kind 移交归破产9 经6/8（本函数不写 owner map/不拆建筑）。
    FBankruptcyDeclaredInfo Info;
    Info.DebtorId   = DebtorId;
    Info.CreditorId = CreditorId;
    OnBankruptcyDeclared.Broadcast(Info);
}
