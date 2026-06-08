// MortgageLegsTest.cpp
// =============================================================================
// 单元测试：抵押/赎回现金腿 F-5/F-6（econ-006）—— 整数 ceil + 无套利 + 自愿赎回
//
// 物理路径：Source/rentoTests/Private/MortgageLegsTest.cpp
// 逻辑路径：tests/unit/economy-cash/mortgage_legs_test.cpp
// Automation 类目：Rento.Economy.MortgageLegs
//
// 被测：
//   GetUnmortgageCostForDisplay(MV) → MV + ceil(MV×1/10)（纯函数，赎回价单一口径）
//   MortgagePayout(player, MV)      → Credit(player, MV, Mortgage)（F-5）
//   UnmortgagePayment(player, MV)   → pre-check GetCash≥cost → Debit(cost, Unmortgage)（F-6，自愿）
//
// 断言非 vacuous（flip 致真 FAIL）：
//   - TC2：ceil 改 floor → MV=75 cost 82≠83 → FAIL
//   - TC4：UnmortgagePayment 去 pre-check → Debit 走 InsufficientFunds → OnInsufficientFunds 1≠0 FAIL
//
// 规范依据：story econ-006 AC-19/21/22/42；GDD economy-cash F-5/F-6/CR-5/CR-8；ADR-0014（整数 ceil）。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "EconomySubsystem.h"
#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnTypes.h"
#include "EconomyEventSpy.h"

// =============================================================================
// 测试辅助（static 内部链接）
// =============================================================================
namespace MortgageLegsTestHelpers
{
    static UWorld* CreateGameWorld(const FName& WorldName)
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, WorldName);
        check(World);
        return World;
    }

    static void DestroyGameWorld(UWorld* World)
    {
        if (World)
        {
            World->DestroyWorld(/*bInformEngineOfWorld=*/false);
        }
    }

    static FGameSetupConfig MakeConfig(int32 N)
    {
        FGameSetupConfig Config;
        Config.DoublesJailThreshold = 3;
        Config.MaxTiebreakRounds    = 1;
        Config.RandomSeed           = 42;

        const EPlayerColor Colors[] = {
            EPlayerColor::Red, EPlayerColor::Blue, EPlayerColor::Green, EPlayerColor::Yellow,
        };
        for (int32 i = 0; i < N && i < 4; ++i)
        {
            FPlayerSetupEntry Entry;
            Entry.DisplayName  = FText::FromString(FString::Printf(TEXT("P%d"), i));
            Entry.TokenColor   = Colors[i];
            Entry.bIsAI        = false;
            Entry.AIDifficulty = EAIDifficulty::None;
            Config.Players.Add(Entry);
        }
        return Config;
    }

    static bool SetupMatch(
        UWorld*& OutWorld,
        UEconomySubsystem*& OutEcon,
        UPlayerTurnSubsystem*& OutTurn,
        const FName& WorldName,
        int32 NumPlayers)
    {
        OutWorld = CreateGameWorld(WorldName);
        if (!OutWorld) { return false; }
        OutTurn = OutWorld->GetSubsystem<UPlayerTurnSubsystem>();
        OutEcon = OutWorld->GetSubsystem<UEconomySubsystem>();
        if (!OutTurn || !OutEcon) { return false; }
        OutTurn->InitializeFromConfig(MakeConfig(NumPlayers));
        return true;
    }

    static UEconomyEventSpy* BindSpy(UEconomySubsystem* Econ)
    {
        UEconomyEventSpy* Spy = NewObject<UEconomyEventSpy>(GetTransientPackage());
        Spy->AddToRoot();
        Econ->OnCashChanged.AddDynamic(Spy, &UEconomyEventSpy::HandleCashChanged);
        Econ->OnInsufficientFunds.AddDynamic(Spy, &UEconomyEventSpy::HandleInsufficientFunds);
        return Spy;
    }

    static void UnbindSpy(UEconomySubsystem* Econ, UEconomyEventSpy* Spy)
    {
        if (Econ && Spy)
        {
            Econ->OnCashChanged.RemoveDynamic(Spy, &UEconomyEventSpy::HandleCashChanged);
            Econ->OnInsufficientFunds.RemoveDynamic(Spy, &UEconomyEventSpy::HandleInsufficientFunds);
        }
        if (Spy) { Spy->RemoveFromRoot(); }
    }

} // namespace MortgageLegsTestHelpers

// =============================================================================
// TC-1（AC-19）—— 抵押放款 F-5：payout == MortgageValue
//   MV=100 → Credit(owner,100,Mortgage) → Cash+=100，OnCashChanged reason=Mortgage。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMortgage_TC1_PayoutEqualsMv,
    "Rento.Economy.MortgageLegs.TC1_PayoutEqualsMv",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMortgage_TC1_PayoutEqualsMv::RunTest(const FString& Parameters)
{
    using namespace MortgageLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-1: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Mort_TC1_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(0);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bMortgage = Econ->MortgagePayout(/*PlayerId=*/0, /*MV=*/100);
    TestTrue(TEXT("TC-1: MortgagePayout(100) 返回 true"), bMortgage);
    TestEqual(TEXT("TC-1: payout==MV → Cash==100"), Econ->GetCash(0), 100);
    TestEqual(TEXT("TC-1: OnCashChanged 1 次"), Spy->CashChangedCount, 1);
    TestEqual(TEXT("TC-1: reason==Mortgage"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Mortgage));

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-2（AC-21）—— 赎回价整数 ceil + 纯函数无副作用 + 决定性
//   MV=100→110；MV=75→ceil(7.5)=8→83；MV=1→fee=1→2。纯函数不改 Cash/不触事件。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMortgage_TC2_UnmortgageCostCeil,
    "Rento.Economy.MortgageLegs.TC2_UnmortgageCostCeil",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMortgage_TC2_UnmortgageCostCeil::RunTest(const FString& Parameters)
{
    using namespace MortgageLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-2: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Mort_TC2_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(500);            // 基线，验纯函数不改

    UEconomyEventSpy* Spy = BindSpy(Econ);

    TestEqual(TEXT("TC-2: MV=100 → cost==110"), Econ->GetUnmortgageCostForDisplay(100), 110);
    TestEqual(TEXT("TC-2: MV=75 → fee=ceil(7.5)=8 → cost==83（非 82）"), Econ->GetUnmortgageCostForDisplay(75), 83);
    TestEqual(TEXT("TC-2: MV=1 → fee=1 → cost==2（fee 恒≥1，非免费赎回）"), Econ->GetUnmortgageCostForDisplay(1), 2);
    TestEqual(TEXT("TC-2: MV=60 → fee=6 → cost==66"), Econ->GetUnmortgageCostForDisplay(60), 66);

    // 纯函数无副作用：Cash 不变、不触事件。
    TestEqual(TEXT("TC-2: 纯函数不改 Cash（仍 500）"), Econ->GetCash(0), 500);
    TestEqual(TEXT("TC-2: 纯函数不触 OnCashChanged"), Spy->CashChangedCount, 0);

    // 决定性：同 MV 两次一致。
    TestEqual(TEXT("TC-2: 决定性 MV=75 两次一致"),
        Econ->GetUnmortgageCostForDisplay(75), Econ->GetUnmortgageCostForDisplay(75));

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-3（AC-21 防御）—— MV≤0 无效数据 → 0 + dev log（防负 cost）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMortgage_TC3_InvalidMvGuard,
    "Rento.Economy.MortgageLegs.TC3_InvalidMvGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMortgage_TC3_InvalidMvGuard::RunTest(const FString& Parameters)
{
    using namespace MortgageLegsTestHelpers;

    // MV=0 + MV=−5 各触发 1 条 "MortgageValue" Error（共 2 条）。
    AddExpectedError(TEXT("MortgageValue"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-3: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Mort_TC3_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    TestEqual(TEXT("TC-3: MV=0 → cost==0（无效守门）"), Econ->GetUnmortgageCostForDisplay(0), 0);
    TestEqual(TEXT("TC-3: MV=−5 → cost==0（防负 cost）"), Econ->GetUnmortgageCostForDisplay(-5), 0);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4（AC-22）—— 赎回现金不足 → 不可用（不 Debit、不广播、自愿非 raising-funds）
//   Cash=50, cost=110 → UnmortgagePayment false、Cash 仍 50、OnCashChanged 0、OnInsufficientFunds 0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMortgage_TC4_UnmortgageInsufficientUnavailable,
    "Rento.Economy.MortgageLegs.TC4_UnmortgageInsufficientUnavailable",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMortgage_TC4_UnmortgageInsufficientUnavailable::RunTest(const FString& Parameters)
{
    using namespace MortgageLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-4: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Mort_TC4_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(50);             // < cost(110)

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bUnmortgage = Econ->UnmortgagePayment(/*PlayerId=*/0, /*MV=*/100);  // cost=110
    TestFalse(TEXT("TC-4: 现金不足 → UnmortgagePayment false（不可用）"), bUnmortgage);
    TestEqual(TEXT("TC-4: 不 Debit，Cash 仍 50"), Econ->GetCash(0), 50);
    TestEqual(TEXT("TC-4: 不广播 OnCashChanged"), Spy->CashChangedCount, 0);
    // 赎回是自愿行为：现金不足 ≠ raising-funds，故不触 OnInsufficientFunds。
    TestEqual(TEXT("TC-4: 自愿赎回不触 OnInsufficientFunds（非 raising-funds）"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-5（AC-42）—— 抵押无套利 + F-6 赎回成功
//   抵一赎净 = +MV − cost = −fee ≤ 0 恒非正。MV=100 → 净 −10；MV=60 → 净 −6。
//   赎回成功 reason=Unmortgage。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMortgage_TC5_NoArbitrageAndUnmortgageSuccess,
    "Rento.Economy.MortgageLegs.TC5_NoArbitrageAndUnmortgageSuccess",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMortgage_TC5_NoArbitrageAndUnmortgageSuccess::RunTest(const FString& Parameters)
{
    using namespace MortgageLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Mort_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // MV=100：Cash=500 → 抵押 +100 → 600 → 赎回 −110 → 490。净 = 490−500 = −10 = −fee。
    Turn->FindPlayerById(0)->SetCash(500);
    const int32 Before100 = Econ->GetCash(0);
    TestTrue(TEXT("TC-5: MortgagePayout(100)"), Econ->MortgagePayout(0, 100));
    TestEqual(TEXT("TC-5: 抵押后 Cash==600"), Econ->GetCash(0), 600);
    const bool bUnmort100 = Econ->UnmortgagePayment(0, 100);
    TestTrue(TEXT("TC-5: UnmortgagePayment(100) 成功"), bUnmort100);
    TestEqual(TEXT("TC-5: 赎回后 Cash==490（600−110）"), Econ->GetCash(0), 490);
    const int32 Net100 = Econ->GetCash(0) - Before100;
    TestEqual(TEXT("TC-5: 抵一赎净 == −10 == −fee（MV=100）"), Net100, -10);
    TestTrue(TEXT("TC-5: 无套利 净 ≤ 0（MV=100）"), Net100 <= 0);
    TestEqual(TEXT("TC-5: 赎回 reason==Unmortgage（末次 OnCashChanged）"),
        static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Unmortgage));

    // MV=60：净应为 −fee = −6（cost=66）。
    Turn->FindPlayerById(1)->SetCash(500);
    const int32 Before60 = Econ->GetCash(1);
    TestTrue(TEXT("TC-5: MortgagePayout(60)"), Econ->MortgagePayout(1, 60));
    TestTrue(TEXT("TC-5: UnmortgagePayment(60)"), Econ->UnmortgagePayment(1, 60));
    const int32 Net60 = Econ->GetCash(1) - Before60;
    TestEqual(TEXT("TC-5: 抵一赎净 == −6 == −fee（MV=60）"), Net60, -6);
    TestTrue(TEXT("TC-5: 无套利 净 ≤ 0（MV=60）"), Net60 <= 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-6（对抗 review CONCERN-2）—— UnmortgageFeeDen=0 除零防御
//   den≤0（配置异常）→ 退化零 fee（cost==MV），不崩溃、不返负值（meta ClampMin 仅约束 editor，
//   runtime guard 兜底 code/data-asset 置 0）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMortgage_TC6_ZeroDenGuard,
    "Rento.Economy.MortgageLegs.TC6_ZeroDenGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMortgage_TC6_ZeroDenGuard::RunTest(const FString& Parameters)
{
    using namespace MortgageLegsTestHelpers;

    // den=0 触发 1 条 "UnmortgageFeeDen" Error。
    AddExpectedError(TEXT("UnmortgageFeeDen"), EAutomationExpectedErrorFlags::Contains, 1);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-6: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Mort_TC6_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    Econ->UnmortgageFeeDen = 0;                        // 模拟 code/data-asset 异常配置

    // den=0 → 退化零 fee（cost==MV，不崩溃 / 不返负）。
    TestEqual(TEXT("TC-6: den=0 → cost==MV==100（零 fee 退化，不崩）"), Econ->GetUnmortgageCostForDisplay(100), 100);

    DestroyGameWorld(World);
    return true;
}
