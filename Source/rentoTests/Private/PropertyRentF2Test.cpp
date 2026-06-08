// PropertyRentF2Test.cpp
// =============================================================================
// 单元测试：地产租金 F-2（econ-004）—— piecewise 公式 + 收租结算（OnRentPaid）
//
// 物理路径：Source/rentoTests/Private/PropertyRentF2Test.cpp
// 逻辑路径：tests/unit/economy-cash/property_rent_f2_test.cpp
// Automation 类目：Rento.Economy.PropertyRentF2
//
// 被测：
//   UEconomySubsystem::ComputePropertyRent(bIsMortgaged, bIsMonopoly, HouseCount, RentTable)
//     is_mortgaged → 0 / monopoly∧house==0 → RentTable[0]×2 / else → RentTable[clamp(house,0,5)]
//   UEconomySubsystem::SettleRent(payer, payee, rent, tileIndex)
//     rent>0 → TransferCash(Rent) + OnRentPaid；rent≤0 → no-op no broadcast
//
// RentTable fixture = {2,10,30,90,160,250}（[0]=2 匹配 AC-10）。
//
// 断言非 vacuous（flip 致真 FAIL）：
//   - TC1：去抵押短路 → rent==90≠0 → FAIL
//   - TC2：monopoly ×2 无条件（含 house==5）→ 酒店 250→500 → FAIL
//   - TC4：去 clamp → house=6 越界崩 / 错值
//   - TC5：SettleRent rent==0 仍转移 → CashChanged FAIL
//
// 规范依据：story econ-004 AC-9~13 + AC-34；GDD economy-cash F-2/CR-3/CR-8；ADR-0014/0006。
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
namespace PropertyRentF2TestHelpers
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

    /** 绑定 spy 到 economy 三事件（含 OnRentPaid）。 */
    static UEconomyEventSpy* BindSpy(UEconomySubsystem* Econ)
    {
        UEconomyEventSpy* Spy = NewObject<UEconomyEventSpy>(GetTransientPackage());
        Spy->AddToRoot();
        Econ->OnCashChanged.AddDynamic(Spy, &UEconomyEventSpy::HandleCashChanged);
        Econ->OnRentPaid.AddDynamic(Spy, &UEconomyEventSpy::HandleRentPaid);
        Econ->OnInsufficientFunds.AddDynamic(Spy, &UEconomyEventSpy::HandleInsufficientFunds);
        return Spy;
    }

    static void UnbindSpy(UEconomySubsystem* Econ, UEconomyEventSpy* Spy)
    {
        if (Econ && Spy)
        {
            Econ->OnCashChanged.RemoveDynamic(Spy, &UEconomyEventSpy::HandleCashChanged);
            Econ->OnRentPaid.RemoveDynamic(Spy, &UEconomyEventSpy::HandleRentPaid);
            Econ->OnInsufficientFunds.RemoveDynamic(Spy, &UEconomyEventSpy::HandleInsufficientFunds);
        }
        if (Spy) { Spy->RemoveFromRoot(); }
    }

    /** Property RentTable fixture：{2,10,30,90,160,250}（[0]=2 匹配 AC-10）。 */
    static TArray<int32> MakeRentTable()
    {
        return TArray<int32>({2, 10, 30, 90, 160, 250});
    }

} // namespace PropertyRentF2TestHelpers

// =============================================================================
// TC-1（AC-9）—— 短路抵押先于表查找
//   is_mortgaged=true（任意 house/monopoly）→ rent==0（不查 RentTable）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPropRentF2_TC1_MortgageShortCircuit,
    "Rento.Economy.PropertyRentF2.TC1_MortgageShortCircuit",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPropRentF2_TC1_MortgageShortCircuit::RunTest(const FString& Parameters)
{
    using namespace PropertyRentF2TestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-1: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("PropRent_TC1_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> RentTable = MakeRentTable();

    // is_mortgaged=true ∧ monopoly=true ∧ house=3 → 0（短路，不查表）。
    const int32 Rent = Econ->ComputePropertyRent(/*mortgaged=*/true, /*monopoly=*/true, /*house=*/3, RentTable);
    TestEqual(TEXT("TC-1: 抵押地 rent==0（短路先于表查找）"), Rent, 0);

    // 对照：同 house/monopoly 但未抵押 → 走 else 表查找 == RentTable[3]==90（证明短路是 mortgage 致 0 非巧合）。
    const int32 RentUnmortgaged = Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/true, /*house=*/3, RentTable);
    TestEqual(TEXT("TC-1 对照: 未抵押同输入 == RentTable[3]==90"), RentUnmortgaged, 90);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-2（AC-10/11）—— 垄断翻倍仅无房 base，酒店绝不翻倍
//   monopoly∧house=0 → RentTable[0]×2==4；monopoly∧house=1 → RentTable[1]==10（不翻倍）；
//   monopoly∧house=5 → RentTable[5]==250（酒店无 ×2，关键 AC-11）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPropRentF2_TC2_MonopolyDoubleOnlyNoHouse,
    "Rento.Economy.PropertyRentF2.TC2_MonopolyDoubleOnlyNoHouse",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPropRentF2_TC2_MonopolyDoubleOnlyNoHouse::RunTest(const FString& Parameters)
{
    using namespace PropertyRentF2TestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-2: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("PropRent_TC2_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> RentTable = MakeRentTable();

    // monopoly ∧ house==0 → RentTable[0](2) × 2 == 4（AC-10）。
    TestEqual(TEXT("TC-2: 垄断无房 2×2==4"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/true, /*house=*/0, RentTable), 4);

    // monopoly ∧ house==1 → RentTable[1]==10（不翻倍，AC-10）。
    TestEqual(TEXT("TC-2: 垄断 1 房 == RentTable[1]==10（不翻倍）"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/true, /*house=*/1, RentTable), 10);

    // monopoly ∧ house==5 → RentTable[5]==250（酒店绝不翻倍，AC-11 关键；翻倍则 500）。
    TestEqual(TEXT("TC-2: 垄断酒店(house=5) == RentTable[5]==250（无 ×2）"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/true, /*house=*/5, RentTable), 250);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-3（AC-12）—— 非垄断按房数；非垄断无房不翻倍
//   monopoly=false∧house=3 → RentTable[3]==90；monopoly=false∧house=0 → RentTable[0]==2（无 ×2）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPropRentF2_TC3_NonMonopolyByHouseCount,
    "Rento.Economy.PropertyRentF2.TC3_NonMonopolyByHouseCount",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPropRentF2_TC3_NonMonopolyByHouseCount::RunTest(const FString& Parameters)
{
    using namespace PropertyRentF2TestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-3: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("PropRent_TC3_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> RentTable = MakeRentTable();

    // 非垄断 ∧ house=3 → RentTable[3]==90。
    TestEqual(TEXT("TC-3: 非垄断 3 房 == RentTable[3]==90"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/false, /*house=*/3, RentTable), 90);

    // 非垄断 ∧ house=0 → RentTable[0]==2（无翻倍，区别于垄断无房 4）。
    TestEqual(TEXT("TC-3: 非垄断无房 == RentTable[0]==2（无 ×2）"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/false, /*house=*/0, RentTable), 2);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4（AC-13）—— house_count clamp + dev 信号 + 决定性
//   house=6 → clamp→RentTable[5]==250 + dev log；house=−1 → RentTable[0]==2 + dev log。
//   G5：AddExpectedError("out of range", Contains, 2)（house=6 + house=−1 各一）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPropRentF2_TC4_HouseClampAndDevSignal,
    "Rento.Economy.PropertyRentF2.TC4_HouseClampAndDevSignal",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPropRentF2_TC4_HouseClampAndDevSignal::RunTest(const FString& Parameters)
{
    using namespace PropertyRentF2TestHelpers;

    // 越界调用共 3 次触发 "out of range" Error：house=6、house=−1、house=−1+monopoly（均 raw 越界）。
    AddExpectedError(TEXT("out of range"), EAutomationExpectedErrorFlags::Contains, 3);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-4: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("PropRent_TC4_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> RentTable = MakeRentTable();

    // house=6（>5，建房8 bug）→ clamp(0,5)→RentTable[5]==250 不崩。
    TestEqual(TEXT("TC-4: house=6 → clamp→RentTable[5]==250 不崩"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/false, /*house=*/6, RentTable), 250);

    // house=−1（<0）→ RentTable[0]==2（raw≠0 落 else，非垄断翻倍）。
    TestEqual(TEXT("TC-4: house=−1 → RentTable[0]==2"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/false, /*house=*/-1, RentTable), 2);

    // house=−1 + monopoly → 仍 RentTable[0]==2（raw −1≠0 不触 ×2，AC-13 边界）。
    TestEqual(TEXT("TC-4: house=−1 + 垄断 → RentTable[0]==2（raw≠0 不 ×2）"),
        Econ->ComputePropertyRent(/*mortgaged=*/false, /*monopoly=*/true, /*house=*/-1, RentTable), 2);

    // 决定性：固定输入两次算 rent 位级一致（无 float）。house=0 在界内不触发 dev log（不计入上方 3 次）。
    const int32 R1 = Econ->ComputePropertyRent(false, true, 0, RentTable);
    const int32 R2 = Econ->ComputePropertyRent(false, true, 0, RentTable);
    TestEqual(TEXT("TC-4: 决定性 两次同输入位级一致"), R1, R2);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-5（AC-34）—— SettleRent 收租结算 + OnRentPaid 广播
//   rent>0 → payer 扣/payee 增 + OnRentPaid{payer,payee,rent,tile} + 2×OnCashChanged(Rent)；
//   rent==0 → 不转移、不广播（false）；付方不足 → OnInsufficientFunds + 无 OnRentPaid（false）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPropRentF2_TC5_SettleRentBroadcast,
    "Rento.Economy.PropertyRentF2.TC5_SettleRentBroadcast",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPropRentF2_TC5_SettleRentBroadcast::RunTest(const FString& Parameters)
{
    using namespace PropertyRentF2TestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("PropRent_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(500);            // payer
    Turn->FindPlayerById(1)->SetCash(100);            // payee

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // rent=90, tile=5 → payer 500-90=410 / payee 100+90=190 + OnRentPaid + 2×OnCashChanged。
    const bool bSettle = Econ->SettleRent(/*Payer=*/0, /*Payee=*/1, /*Rent=*/90, /*Tile=*/5);
    TestTrue(TEXT("TC-5: SettleRent(90) 返回 true"), bSettle);
    TestEqual(TEXT("TC-5: payer == 410"), Econ->GetCash(0), 410);
    TestEqual(TEXT("TC-5: payee == 190"), Econ->GetCash(1), 190);
    TestEqual(TEXT("TC-5: OnRentPaid 1 次"), Spy->RentPaidCount, 1);
    TestEqual(TEXT("TC-5: OnRentPaid.PayerId==0"), Spy->LastPayerId, 0);
    TestEqual(TEXT("TC-5: OnRentPaid.PayeeId==1"), Spy->LastPayeeId, 1);
    TestEqual(TEXT("TC-5: OnRentPaid.Amount==90"), Spy->LastRentAmount, 90);
    TestEqual(TEXT("TC-5: OnRentPaid.TileIndex==5"), Spy->LastTileIndex, 5);
    TestEqual(TEXT("TC-5: 2×OnCashChanged（payer/payee 各一）"), Spy->CashChangedCount, 2);
    // C-2：两腿 reason 独立断言（LastReason 仅留末腿，须查全序列证 Payer 腿也是 Rent）。
    if (TestEqual(TEXT("TC-5: CashChangedReasons 恰 2 条"), Spy->CashChangedReasons.Num(), 2))
    {
        TestEqual(TEXT("TC-5: Payer 腿 reason==Rent"), static_cast<int32>(Spy->CashChangedReasons[0]), static_cast<int32>(EChangeReason::Rent));
        TestEqual(TEXT("TC-5: Payee 腿 reason==Rent"), static_cast<int32>(Spy->CashChangedReasons[1]), static_cast<int32>(EChangeReason::Rent));
    }

    // rent==0（抵押/自有/无主）→ 不转移、不广播（AC-5/37）。
    const bool bZero = Econ->SettleRent(/*Payer=*/0, /*Payee=*/1, /*Rent=*/0, /*Tile=*/5);
    TestFalse(TEXT("TC-5: rent==0 返回 false"), bZero);
    TestEqual(TEXT("TC-5: rent==0 payer 不变（410）"), Econ->GetCash(0), 410);
    TestEqual(TEXT("TC-5: rent==0 OnRentPaid 仍 1 次（未再广播）"), Spy->RentPaidCount, 1);

    // 付方不足额 → OnInsufficientFunds + 无 OnRentPaid（rent 未真付）。
    const bool bShort = Econ->SettleRent(/*Payer=*/0, /*Payee=*/1, /*Rent=*/9999, /*Tile=*/5);
    TestFalse(TEXT("TC-5: 不足额返回 false"), bShort);
    TestEqual(TEXT("TC-5: 不足额 payer 不变（410）"), Econ->GetCash(0), 410);
    TestEqual(TEXT("TC-5: 不足额 OnInsufficientFunds 1 次"), Spy->InsufficientCount, 1);
    TestEqual(TEXT("TC-5: 不足额 OnRentPaid 仍 1 次（未真收租）"), Spy->RentPaidCount, 1);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}
