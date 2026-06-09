// TaxBuyLegsTest.cpp
// =============================================================================
// 单元测试：缴税现金腿 F-7 + 买地现金腿 CR-4（econ-007）—— 强制/可选区分 + sink 蒸发
//
// 物理路径：Source/rentoTests/Private/TaxBuyLegsTest.cpp
// 逻辑路径：tests/unit/economy-cash/tax_buy_legs_test.cpp
// Automation 类目：Rento.Economy.TaxBuyLegs
//
// 被测：
//   PayTax(player, TaxAmount)           → Debit(player, TaxAmount, Tax)（F-7；强制）
//   BuyPropertyCashLeg(player, Price)   → pre-check GetCash≥Price → Debit(Price, Purchase)（CR-4；可选）
//
// 断言非 vacuous（flip 致真 FAIL）：
//   - TC2：税是强制 —— 不足触 InsufficientFunds（对照 TC4 买地可选不触）
//   - TC4：去掉 BuyPropertyCashLeg pre-check → Debit 走 OnInsufficientFunds → InsufficientCount 1≠0 FAIL
//   - TC5：PurchasePrice≤0 dev log → Automation 判 Fail（须 AddExpectedError 吞）
//
// 规范依据：story econ-007 AC-23 / CR-4 / CR-6；GDD economy-cash F-7/CR-4/CR-6/CR-8；ADR-0007/0014。
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
// 测试辅助（static 内部链接，namespace 独立防名称碰撞）
// =============================================================================
namespace TaxBuyLegsTestHelpers
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

} // namespace TaxBuyLegsTestHelpers

// =============================================================================
// TC1（AC-23）—— 缴税强制扣款 + sink 蒸发（款不进任何玩家）
//   P0 Cash=500, P1 Cash=500；PayTax(0, 200) → 返 true；P0 Cash==300；
//   P1 Cash 仍 500（无 Credit，sink 蒸发）；CashChangedCount==1（仅 P0 出账）；
//   LastReason==Tax；InsufficientCount==0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTaxBuy_TC1_TaxForcedSinkEvaporate,
    "Rento.Economy.TaxBuyLegs.TC1_TaxForcedSinkEvaporate",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTaxBuy_TC1_TaxForcedSinkEvaporate::RunTest(const FString& Parameters)
{
    using namespace TaxBuyLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-1: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("TaxBuy_TC1_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(500);
    Turn->FindPlayerById(1)->SetCash(500);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bTax = Econ->PayTax(/*PlayerId=*/0, /*TaxAmount=*/200);
    TestTrue(TEXT("TC-1: PayTax(200) 返回 true（强制扣款成功）"), bTax);
    TestEqual(TEXT("TC-1: P0 Cash==300（已扣税 200）"), Econ->GetCash(0), 300);
    // sink 蒸发：款不 Credit 任何玩家（CR-6，CR-8 无限银行模型）。
    TestEqual(TEXT("TC-1: P1 Cash 仍 500（款蒸发，无 Credit）"), Econ->GetCash(1), 500);
    // 仅 P0 出账 1 次 OnCashChanged，无第二次 Credit：
    TestEqual(TEXT("TC-1: CashChangedCount==1（仅 P0 出账，无 Credit 腿）"), Spy->CashChangedCount, 1);
    TestEqual(TEXT("TC-1: LastReason==Tax"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Tax));
    TestEqual(TEXT("TC-1: InsufficientCount==0（充足，不进 Raising Funds）"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC2（AC-23 强制对照）—— 缴税不足额 → 强制进 Raising Funds（OnInsufficientFunds）
//   P0 Cash=150；PayTax(0, 200) → 返 false；Cash 仍 150；
//   CashChangedCount==0；InsufficientCount==1（LastDue==200, LastShort==50）。
//   非 vacuous：此 TC 与 TC4 形成强制 vs 可选对照 ——
//     tax 无 pre-check → Debit 内走 OnInsufficientFunds（InsufficientCount 1）；
//     buy pre-check → 不进 Debit → InsufficientCount 0（TC4）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTaxBuy_TC2_TaxInsufficientForcedRaisingFunds,
    "Rento.Economy.TaxBuyLegs.TC2_TaxInsufficientForcedRaisingFunds",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTaxBuy_TC2_TaxInsufficientForcedRaisingFunds::RunTest(const FString& Parameters)
{
    using namespace TaxBuyLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-2: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("TaxBuy_TC2_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(150);              // < TaxAmount(200)

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bTax = Econ->PayTax(/*PlayerId=*/0, /*TaxAmount=*/200);
    TestFalse(TEXT("TC-2: 现金不足 → PayTax false（强制债务触 Raising Funds）"), bTax);
    TestEqual(TEXT("TC-2: Cash 仍 150（未扣）"), Econ->GetCash(0), 150);
    TestEqual(TEXT("TC-2: CashChangedCount==0（不足未扣，无 OnCashChanged）"), Spy->CashChangedCount, 0);
    // 税是强制：不足额走 Debit 内 OnInsufficientFunds → 进 Raising Funds（AC-23/AC-1）。
    TestEqual(TEXT("TC-2: InsufficientCount==1（税强制，不足触 Raising Funds）"), Spy->InsufficientCount, 1);
    TestEqual(TEXT("TC-2: LastDue==200"), Spy->LastDue, 200);
    TestEqual(TEXT("TC-2: LastShort==50（200−150）"), Spy->LastShort, 50);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC3（CR-4）—— 买地现金腿充足 → Debit + reason=Purchase
//   P0 Cash=500；BuyPropertyCashLeg(0, 300) → 返 true；Cash==200；
//   CashChangedCount==1；LastReason==Purchase；InsufficientCount==0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTaxBuy_TC3_BuyPropertySufficient,
    "Rento.Economy.TaxBuyLegs.TC3_BuyPropertySufficient",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTaxBuy_TC3_BuyPropertySufficient::RunTest(const FString& Parameters)
{
    using namespace TaxBuyLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-3: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("TaxBuy_TC3_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(500);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bBuy = Econ->BuyPropertyCashLeg(/*PlayerId=*/0, /*PurchasePrice=*/300);
    TestTrue(TEXT("TC-3: BuyPropertyCashLeg(300) 返回 true（现金充足）"), bBuy);
    TestEqual(TEXT("TC-3: Cash==200（500−300）"), Econ->GetCash(0), 200);
    TestEqual(TEXT("TC-3: CashChangedCount==1（1 次出账）"), Spy->CashChangedCount, 1);
    TestEqual(TEXT("TC-3: LastReason==Purchase"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Purchase));
    TestEqual(TEXT("TC-3: InsufficientCount==0（充足，不触 Raising Funds）"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC4（CR-4 可选对照）—— 买地现金不足 → 购买不可用，**不进** Raising Funds
//   P0 Cash=200；BuyPropertyCashLeg(0, 300) → 返 false；Cash 仍 200；
//   CashChangedCount==0；InsufficientCount==0（关键：买地可选，不触 raising-funds）。
//   非 vacuous：去掉 BuyPropertyCashLeg 的 pre-check → Debit 内 OnInsufficientFunds
//     → InsufficientCount 1≠0 FAIL（对照 TC2 税强制 InsufficientCount==1）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTaxBuy_TC4_BuyPropertyInsufficientUnavailable,
    "Rento.Economy.TaxBuyLegs.TC4_BuyPropertyInsufficientUnavailable",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTaxBuy_TC4_BuyPropertyInsufficientUnavailable::RunTest(const FString& Parameters)
{
    using namespace TaxBuyLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-4: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("TaxBuy_TC4_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(200);              // < PurchasePrice(300)

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bBuy = Econ->BuyPropertyCashLeg(/*PlayerId=*/0, /*PurchasePrice=*/300);
    TestFalse(TEXT("TC-4: 现金不足 → BuyPropertyCashLeg false（购买不可用）"), bBuy);
    TestEqual(TEXT("TC-4: Cash 仍 200（未扣，不 Debit）"), Econ->GetCash(0), 200);
    TestEqual(TEXT("TC-4: CashChangedCount==0（不足，无 OnCashChanged）"), Spy->CashChangedCount, 0);
    // 买地是可选行为：现金不足 ≠ 强制债务，pre-check 拦截确保不触 OnInsufficientFunds（非 raising-funds）。
    TestEqual(TEXT("TC-4: InsufficientCount==0（买地可选，不足不进 Raising Funds）"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC5（CR-4 防御）—— BuyPropertyCashLeg PurchasePrice≤0 无效数据 → false + dev log
//   price=0 与 price=−5 各触发 1 条含 "PurchasePrice" 的 Error（共 2 条，须 AddExpectedError 吞）。
//   BuyPropertyCashLeg(0, 0)==false；BuyPropertyCashLeg(0, -5)==false；Cash 不变；CashChangedCount==0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTaxBuy_TC5_InvalidPriceGuard,
    "Rento.Economy.TaxBuyLegs.TC5_InvalidPriceGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTaxBuy_TC5_InvalidPriceGuard::RunTest(const FString& Parameters)
{
    using namespace TaxBuyLegsTestHelpers;

    // price=0 + price=−5 各触 1 条 "PurchasePrice" Error（UE_LOG Error，G2，须 AddExpectedError 捕获）。
    AddExpectedError(TEXT("PurchasePrice"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("TaxBuy_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(500);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // price==0：无效（board 数据 bug）→ false + dev log，不扣款。
    TestFalse(TEXT("TC-5: PurchasePrice=0 → false（无效 price 守门）"), Econ->BuyPropertyCashLeg(0, 0));
    TestEqual(TEXT("TC-5: price=0 后 Cash 仍 500（未扣）"), Econ->GetCash(0), 500);

    // price=−5：无效 → false + dev log，不扣款。
    TestFalse(TEXT("TC-5: PurchasePrice=−5 → false（负价守门）"), Econ->BuyPropertyCashLeg(0, -5));
    TestEqual(TEXT("TC-5: price=−5 后 Cash 仍 500（未扣）"), Econ->GetCash(0), 500);

    // 两次非法调用均未触 OnCashChanged。
    TestEqual(TEXT("TC-5: CashChangedCount==0（无效 price 均 no-op）"), Spy->CashChangedCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC6（CR-4 边界）—— 买地 price==cash 恰好等额 → 成功，Cash 归零（合法）
//   P0 Cash=300, price=300 → BuyPropertyCashLeg true；Cash==0；CashChangedCount==1；reason=Purchase。
//   非 vacuous：pre-check 用 `GetCash < PurchasePrice`（严格 <）——
//     若误改为 `<=`，等额买地会错误返 false（Cash 仍 300），TestTrue/TestEqual(0) 即 FAIL。
//   钉死「买地后 cash 归零仍合法」边界（< vs <= 语义）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTaxBuy_TC6_BuyPropertyExactCashBoundary,
    "Rento.Economy.TaxBuyLegs.TC6_BuyPropertyExactCashBoundary",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTaxBuy_TC6_BuyPropertyExactCashBoundary::RunTest(const FString& Parameters)
{
    using namespace TaxBuyLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-6: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("TaxBuy_TC6_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(300);              // 恰等 price

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bBuy = Econ->BuyPropertyCashLeg(/*PlayerId=*/0, /*PurchasePrice=*/300);
    TestTrue(TEXT("TC-6: price==cash → BuyPropertyCashLeg true（等额合法，钉 < 非 <=）"), bBuy);
    TestEqual(TEXT("TC-6: Cash==0（300−300，归零仍合法）"), Econ->GetCash(0), 0);
    TestEqual(TEXT("TC-6: CashChangedCount==1（1 次出账）"), Spy->CashChangedCount, 1);
    TestEqual(TEXT("TC-6: LastReason==Purchase"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Purchase));
    TestEqual(TEXT("TC-6: InsufficientCount==0（充足，不触 Raising Funds）"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC7（AC-23 零额对照）—— PayTax TaxAmount==0 → false 且静默（不广播、非 raising-funds）
//   P0 Cash=500；PayTax(0, 0) → 返 false；Cash 仍 500；CashChangedCount==0；InsufficientCount==0。
//   钉死「零额 vs 不足额」两种 false 的区分（对照 TC2 不足额 InsufficientCount==1）：
//     零额走 Debit 内 Amount==0 静默早返（不触 OnInsufficientFunds），无 Error log（无需 AddExpectedError）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTaxBuy_TC7_TaxZeroAmountSilent,
    "Rento.Economy.TaxBuyLegs.TC7_TaxZeroAmountSilent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTaxBuy_TC7_TaxZeroAmountSilent::RunTest(const FString& Parameters)
{
    using namespace TaxBuyLegsTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-7: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("TaxBuy_TC7_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(500);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bTax = Econ->PayTax(/*PlayerId=*/0, /*TaxAmount=*/0);
    TestFalse(TEXT("TC-7: TaxAmount==0 → PayTax false（零额静默）"), bTax);
    TestEqual(TEXT("TC-7: Cash 仍 500（零额未扣）"), Econ->GetCash(0), 500);
    TestEqual(TEXT("TC-7: CashChangedCount==0（零额不广播 OnCashChanged）"), Spy->CashChangedCount, 0);
    // 零额 ≠ 不足额：Debit Amount==0 静默早返，不触 OnInsufficientFunds（对照 TC2 不足额 InsufficientCount==1）。
    TestEqual(TEXT("TC-7: InsufficientCount==0（零额非 raising-funds，区分于 TC2 不足额）"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}
