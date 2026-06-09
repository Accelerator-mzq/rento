// RaisingFundsBankruptcyTest.cpp
// =============================================================================
// 单元/集成测试：凑钱状态机清算顺序 spec + 破产现金侧 F-11（econ-009）
//   清算顺序 spec（economy 拥有）= DecideNextLiquidationStep（纯静态，mortgage-empty-first）
//   破产现金侧 F-11 = SettleBankruptcy（玩家债主转移 / 银行蒸发 + OnBankruptcyDeclared）
//
// 物理路径：Source/rentoTests/Private/RaisingFundsBankruptcyTest.cpp
// 逻辑路径：tests/integration/economy-cash/raising_funds_bankruptcy_test.cpp
// Automation 类目：Rento.Economy.RaisingFundsBankruptcy
//
// 被测：
//   DecideNextLiquidationStep(Cash, Due, Assets, &OutTile)
//     → None / MortgageTile(+OutTile=MV最小空地) / SellBuilding / Insolvent（结构性耗尽）
//   SettleBankruptcy(Debtor, Creditor)
//     → 玩家债主 creditor.Cash+=debtor.Cash（AC-30）/ 银行(INDEX_NONE)蒸发（AC-31）/ 广播 OnBankruptcyDeclared×1（AC-36）
//
// 断言非 vacuous（flip 致真 FAIL）：
//   - TC1：mortgage-empty-first——换序为 sell-first 则返 SellBuilding≠MortgageTile FAIL；选 MV 最小——选错格 OutTile FAIL
//   - TC5/TC6：SettleBankruptcy 现金守恒——漏转移/误蒸发则 Cash 断言 FAIL
//
// 规范依据：story econ-009 AC-30/31/35/36/43；ADR-0001/0003（OnBankruptcyDeclared 现金侧）。
//   AC-43 完整清算循环（回合2 驱动 6/8）的集成覆盖在 GameStateSnapshotAiHooksTest TC8/TC8b（已改用本 spec）。
//   AC-43 变体C 逐栋 floor（111≠112）= econ-008 ComputeNetLiquidationValue TC5 已覆盖（NLV 值口径，非顺序）。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "EconomySubsystem.h"
#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnTypes.h"
#include "EconomyTypes.h"
#include "EconomyEventSpy.h"

// =============================================================================
// 测试辅助（static 内部链接）
// =============================================================================
namespace RaisingFundsBankruptcyTestHelpers
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
        Econ->OnBankruptcyDeclared.AddDynamic(Spy, &UEconomyEventSpy::HandleBankruptcyDeclared);
        return Spy;
    }

    static void UnbindSpy(UEconomySubsystem* Econ, UEconomyEventSpy* Spy)
    {
        if (Econ && Spy)
        {
            Econ->OnCashChanged.RemoveDynamic(Spy, &UEconomyEventSpy::HandleCashChanged);
            Econ->OnInsufficientFunds.RemoveDynamic(Spy, &UEconomyEventSpy::HandleInsufficientFunds);
            Econ->OnBankruptcyDeclared.RemoveDynamic(Spy, &UEconomyEventSpy::HandleBankruptcyDeclared);
        }
        if (Spy) { Spy->RemoveFromRoot(); }
    }

    /** 构造清算资产 FNlvAssetEntry（含 TileIndex）。 */
    static FNlvAssetEntry MakeAsset(int32 Tile, int32 MV, bool bMort, int32 House)
    {
        FNlvAssetEntry E;
        E.TileIndex     = Tile;
        E.MortgageValue = MV;
        E.bIsMortgaged  = bMort;
        E.HouseCount    = House;
        return E;
    }
} // namespace RaisingFundsBankruptcyTestHelpers

// =============================================================================
// TC1（AC-43）—— 清算顺序 spec：mortgage-empty-first + 选 MV 最小空地
//   Assets=[tile5(MV80,house0), tile7(MV60,house0), tile9(MV100,house2)]；Cash=50,due=200。
//   → MortgageTile，OutTile==7（MV 最小空地，非 tile5 的80、非带房 tile9）。
//   非 vacuous：换序为 sell-first → 返 SellBuilding≠MortgageTile FAIL；选错 MV → OutTile≠7 FAIL；
//     若误抵押带房 tile9 → OutTile==9 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC1_MortgageEmptyFirstMinMv,
    "Rento.Economy.RaisingFundsBankruptcy.TC1_MortgageEmptyFirstMinMv",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC1_MortgageEmptyFirstMinMv::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeAsset(/*Tile=*/5, /*MV=*/80,  /*bMort=*/false, /*House=*/0));
    Assets.Add(MakeAsset(/*Tile=*/7, /*MV=*/60,  /*bMort=*/false, /*House=*/0)); // MV 最小空地
    Assets.Add(MakeAsset(/*Tile=*/9, /*MV=*/100, /*bMort=*/false, /*House=*/2)); // 带房，不可抵押

    int32 OutTile = INDEX_NONE;
    const ELiquidationAction Action =
        UEconomySubsystem::DecideNextLiquidationStep(/*Cash=*/50, /*Due=*/200, Assets, OutTile);

    TestEqual(TEXT("TC1: 动作==MortgageTile（mortgage-empty-first，止损优先）"),
        static_cast<int32>(Action), static_cast<int32>(ELiquidationAction::MortgageTile));
    TestEqual(TEXT("TC1: OutTile==7（MV 最小空地，非80/非带房9）"), OutTile, 7);

    return true;
}

// =============================================================================
// TC2（AC-43）—— 无可抵押空地（全带房/全抵押）但有建筑 → SellBuilding
//   Assets=[tile9(MV100,house2)]（带房不可抵押）；Cash=50,due=200 → SellBuilding。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC2_SellBuildingWhenNoEmptyLand,
    "Rento.Economy.RaisingFundsBankruptcy.TC2_SellBuildingWhenNoEmptyLand",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC2_SellBuildingWhenNoEmptyLand::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeAsset(/*Tile=*/9, /*MV=*/100, /*bMort=*/false, /*House=*/2)); // 带房

    int32 OutTile = INDEX_NONE;
    const ELiquidationAction Action =
        UEconomySubsystem::DecideNextLiquidationStep(/*Cash=*/50, /*Due=*/200, Assets, OutTile);

    TestEqual(TEXT("TC2: 无空地有房 → SellBuilding"),
        static_cast<int32>(Action), static_cast<int32>(ELiquidationAction::SellBuilding));
    TestEqual(TEXT("TC2: SellBuilding 不返目标格 OutTile==INDEX_NONE"), OutTile, (int32)INDEX_NONE);

    return true;
}

// =============================================================================
// TC3（AC-43）—— Cash≥应付 → None（偿付成功停止）
//   Cash=200,due=200（恰好够，AC-28 付到 0 算能付）→ None。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC3_NoneWhenSolvent,
    "Rento.Economy.RaisingFundsBankruptcy.TC3_NoneWhenSolvent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC3_NoneWhenSolvent::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeAsset(/*Tile=*/5, /*MV=*/80, /*bMort=*/false, /*House=*/0));

    int32 OutTile = INDEX_NONE;
    // Cash==Due 恰好够（严格 < 边界：200<200==false 不需清算）。
    const ELiquidationAction Action =
        UEconomySubsystem::DecideNextLiquidationStep(/*Cash=*/200, /*Due=*/200, Assets, OutTile);

    TestEqual(TEXT("TC3: Cash≥应付 → None（偿付成功，不清算）"),
        static_cast<int32>(Action), static_cast<int32>(ELiquidationAction::None));

    return true;
}

// =============================================================================
// TC4（AC-43 变体A）—— 资产耗尽（全抵押无房 / 空清单）→ Insolvent（结构性破产判定）
//   Assets=[tile5(MV80,已抵押,house0)]；Cash=50,due=200 → 无可抵押无房 → Insolvent。
//   空清单同理 → Insolvent（证伪兜底死锁）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC4_InsolventWhenExhausted,
    "Rento.Economy.RaisingFundsBankruptcy.TC4_InsolventWhenExhausted",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC4_InsolventWhenExhausted::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;

    // 全抵押无房 → 无可清算动作。
    TArray<FNlvAssetEntry> Mortgaged;
    Mortgaged.Add(MakeAsset(/*Tile=*/5, /*MV=*/80, /*bMort=*/true, /*House=*/0));
    int32 OutTile = INDEX_NONE;
    TestEqual(TEXT("TC4: 全抵押无房 → Insolvent"),
        static_cast<int32>(UEconomySubsystem::DecideNextLiquidationStep(50, 200, Mortgaged, OutTile)),
        static_cast<int32>(ELiquidationAction::Insolvent));

    // 空清单 → Insolvent（无资产可清算）。
    TArray<FNlvAssetEntry> Empty;
    TestEqual(TEXT("TC4: 空 Assets → Insolvent（无死锁）"),
        static_cast<int32>(UEconomySubsystem::DecideNextLiquidationStep(50, 200, Empty, OutTile)),
        static_cast<int32>(ELiquidationAction::Insolvent));

    return true;
}

// =============================================================================
// TC5（AC-30 / AC-36）—— SettleBankruptcy 玩家债主：现金全额转移 + 广播
//   debtor(P0).Cash=30, creditor(P1).Cash=100 → SettleBankruptcy(0,1)：
//     creditor.Cash==130 ∧ debtor.Cash==0 ∧ OnBankruptcyDeclared×1(Debtor=0,Creditor=1)
//     ∧ 现金侧两腿 OnCashChanged reason=Bankruptcy。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC5_SettleBankruptcyToPlayer,
    "Rento.Economy.RaisingFundsBankruptcy.TC5_SettleBankruptcyToPlayer",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC5_SettleBankruptcyToPlayer::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("RFB_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(30);   // debtor
    Turn->FindPlayerById(1)->SetCash(100);  // creditor

    UEconomyEventSpy* Spy = BindSpy(Econ);

    Econ->SettleBankruptcy(/*Debtor=*/0, /*Creditor=*/1);

    TestEqual(TEXT("TC5: creditor.Cash==130（+=debtor30，AC-30）"), Econ->GetCash(1), 130);
    TestEqual(TEXT("TC5: debtor.Cash==0（全额转出）"), Econ->GetCash(0), 0);
    TestEqual(TEXT("TC5: OnBankruptcyDeclared 恰 1 次（AC-36）"), Spy->BankruptcyCount, 1);
    TestEqual(TEXT("TC5: payload Debtor==0"), Spy->LastDebtorId, 0);
    TestEqual(TEXT("TC5: payload Creditor==1"), Spy->LastCreditorId, 1);
    // 现金侧两腿（debit debtor + credit creditor）reason=Bankruptcy（逐腿断言，非仅末腿）。
    TestEqual(TEXT("TC5: OnCashChanged 2 次（转移两腿）"), Spy->CashChangedCount, 2);
    if (Spy->CashChangedReasons.Num() == 2)
    {
        TestEqual(TEXT("TC5: 第1腿 reason==Bankruptcy"),
            static_cast<int32>(Spy->CashChangedReasons[0]), static_cast<int32>(EChangeReason::Bankruptcy));
        TestEqual(TEXT("TC5: 第2腿 reason==Bankruptcy"),
            static_cast<int32>(Spy->CashChangedReasons[1]), static_cast<int32>(EChangeReason::Bankruptcy));
    }
    else
    {
        AddError(FString::Printf(TEXT("TC5: CashChangedReasons 期望 2 腿，实得 %d"), Spy->CashChangedReasons.Num()));
    }
    // 🔴 AC-36 时机：OnBankruptcyDeclared 广播在现金腿【之后】（顺序锁定，非仅计数）——
    //   若把 Broadcast 提前到 TransferCash 前，本断言 FAIL。
    const TArray<FString> Expected = { TEXT("CashChanged"), TEXT("CashChanged"), TEXT("BankruptcyDeclared") };
    TestTrue(TEXT("TC5: EventSequence==[CashChanged,CashChanged,BankruptcyDeclared]（AC-36 时机在现金腿后）"),
        Spy->EventSequence == Expected);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC6（AC-31 / AC-36）—— SettleBankruptcy 银行债主（INDEX_NONE）：现金蒸发 + 广播
//   debtor(P0).Cash=30 → SettleBankruptcy(0, INDEX_NONE)：
//     debtor.Cash==0（蒸发）∧ 其它玩家(P1)不入账 ∧ OnBankruptcyDeclared×1(Creditor=INDEX_NONE)。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC6_SettleBankruptcyToBank,
    "Rento.Economy.RaisingFundsBankruptcy.TC6_SettleBankruptcyToBank",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC6_SettleBankruptcyToBank::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC6: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("RFB_TC6_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(30);   // debtor
    Turn->FindPlayerById(1)->SetCash(100);  // 旁观玩家，不应变

    UEconomyEventSpy* Spy = BindSpy(Econ);

    Econ->SettleBankruptcy(/*Debtor=*/0, /*Creditor=*/INDEX_NONE); // 银行

    TestEqual(TEXT("TC6: debtor.Cash==0（蒸发，AC-31）"), Econ->GetCash(0), 0);
    TestEqual(TEXT("TC6: 旁观 P1 Cash 仍 100（款不入任何玩家）"), Econ->GetCash(1), 100);
    TestEqual(TEXT("TC6: OnBankruptcyDeclared 恰 1 次"), Spy->BankruptcyCount, 1);
    TestEqual(TEXT("TC6: payload Creditor==INDEX_NONE（银行）"), Spy->LastCreditorId, (int32)INDEX_NONE);
    // 仅 debtor 蒸发 1 腿 OnCashChanged（无 credit 腿）。
    TestEqual(TEXT("TC6: OnCashChanged 1 次（仅蒸发 debit）"), Spy->CashChangedCount, 1);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC7（AC-36）—— SettleBankruptcy 即便 debtor.Cash==0 仍广播恰一次（破产成立）
//   debtor(P0).Cash=0 → SettleBankruptcy(0,1)：无现金转移（0 额）但 OnBankruptcyDeclared×1。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC7_BankruptcyBroadcastEvenZeroCash,
    "Rento.Economy.RaisingFundsBankruptcy.TC7_BankruptcyBroadcastEvenZeroCash",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC7_BankruptcyBroadcastEvenZeroCash::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC7: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("RFB_TC7_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(0);    // debtor 已无现金
    Turn->FindPlayerById(1)->SetCash(100);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    Econ->SettleBankruptcy(/*Debtor=*/0, /*Creditor=*/1);

    TestEqual(TEXT("TC7: creditor.Cash 仍 100（0 额无转移）"), Econ->GetCash(1), 100);
    TestEqual(TEXT("TC7: OnCashChanged 0 次（0 额无现金腿）"), Spy->CashChangedCount, 0);
    TestEqual(TEXT("TC7: OnBankruptcyDeclared 恰 1 次（破产成立即广播，AC-36）"), Spy->BankruptcyCount, 1);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC8（AC-35）—— 进入 Raising Funds 时 OnInsufficientFunds 广播（payload 正确）
//   强制扣款不足额（Debit）→ OnInsufficientFunds(p, due=170, short=120)；Cash=50。
//   （AC-35 的瞬态进入信号由 econ-001 Debit 提供，此处验 payload 口径供凑钱状态机消费）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC8_InsufficientFundsOnEnter,
    "Rento.Economy.RaisingFundsBankruptcy.TC8_InsufficientFundsOnEnter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC8_InsufficientFundsOnEnter::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC8: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("RFB_TC8_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(50);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // 强制扣款 170 > Cash 50 → 不扣 + OnInsufficientFunds(170, short=120)（进 Raising Funds，AC-35）。
    const bool bDebit = Econ->Debit(/*PlayerId=*/0, /*Amount=*/170, EChangeReason::Tax);
    TestFalse(TEXT("TC8: 不足额 Debit 返 false"), bDebit);
    TestEqual(TEXT("TC8: OnInsufficientFunds 恰 1 次（进 Raising Funds，AC-35）"), Spy->InsufficientCount, 1);
    TestEqual(TEXT("TC8: AmountDue==170"), Spy->LastDue, 170);
    TestEqual(TEXT("TC8: AmountShort==120（170−50）"), Spy->LastShort, 120);
    TestEqual(TEXT("TC8: 不扣，Cash 仍 50"), Econ->GetCash(0), 50);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC9（AC-43 变体B）—— 带房地卖光房腾空后变为可抵押空地（多步状态转换）
//   纯静态逐步模拟（回合2 每轮重装配 Assets）：
//     初始 [PropX(空地MV80,house0), PropY(MV100,house1)]，Cash 逐步升（模拟 6/8 抬现金）。
//     step1（Cash=50）→ MortgageTile(PropX，止损优先空地)；置 PropX.bIsMortgaged=true。
//     step2（Cash=130）→ SellBuilding（PropX 已抵押，PropY 有房）；置 PropY.HouseCount=0（腾空）。
//     step3（Cash=180）→ MortgageTile(PropY)（**关键：腾空后可抵押**，OutTile==PropY）。
//   钉死"带房地卖光房后进入可抵押空地分支"——TC1/TC2 单步与 pt-007 TC8 早停均不覆盖此转换。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC9_VariantBHousedTileBecomesEmpty,
    "Rento.Economy.RaisingFundsBankruptcy.TC9_VariantBHousedTileBecomesEmpty",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC9_VariantBHousedTileBecomesEmpty::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeAsset(/*Tile=*/5, /*MV=*/80,  /*bMort=*/false, /*House=*/0)); // PropX 空地
    Assets.Add(MakeAsset(/*Tile=*/9, /*MV=*/100, /*bMort=*/false, /*House=*/1)); // PropY 带房

    int32 OutTile = INDEX_NONE;

    // step1（Cash=50）：止损优先 → 抵押空地 PropX(5)。
    TestEqual(TEXT("TC9: step1 → MortgageTile"),
        static_cast<int32>(UEconomySubsystem::DecideNextLiquidationStep(50, 300, Assets, OutTile)),
        static_cast<int32>(ELiquidationAction::MortgageTile));
    TestEqual(TEXT("TC9: step1 OutTile==5（PropX 空地）"), OutTile, 5);
    Assets[0].bIsMortgaged = true; // 模拟回合2 调 6.Mortgage 后状态

    // step2（Cash=130）：PropX 已抵押、PropY 有房 → 卖房（无可抵押空地）。
    TestEqual(TEXT("TC9: step2 → SellBuilding（PropX 抵押后无空地，PropY 有房）"),
        static_cast<int32>(UEconomySubsystem::DecideNextLiquidationStep(130, 300, Assets, OutTile)),
        static_cast<int32>(ELiquidationAction::SellBuilding));
    Assets[1].HouseCount = 0; // 模拟卖光房，PropY 腾空

    // step3（Cash=180）：PropY 腾空（house0 未抵押）→ 🔴 可抵押 → MortgageTile(PropY)。
    TestEqual(TEXT("TC9: step3 → MortgageTile（PropY 卖光房腾空后可抵押）"),
        static_cast<int32>(UEconomySubsystem::DecideNextLiquidationStep(180, 300, Assets, OutTile)),
        static_cast<int32>(ELiquidationAction::MortgageTile));
    TestEqual(TEXT("TC9: step3 OutTile==9（腾空的 PropY）"), OutTile, 9);

    return true;
}

// =============================================================================
// TC10（AC-43 主，确定性）—— 同 MV 平手取数组首个（`<` 非 `<=`，顺序确定）
//   Assets=[tile5(MV80,h0), tile7(MV80,h0), tile9(MV80,h0)] → OutTile==5（首个，平手不替换）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC10_TieBreakFirstTile,
    "Rento.Economy.RaisingFundsBankruptcy.TC10_TieBreakFirstTile",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC10_TieBreakFirstTile::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeAsset(5, 80, false, 0));
    Assets.Add(MakeAsset(7, 80, false, 0));
    Assets.Add(MakeAsset(9, 80, false, 0));

    int32 OutTile = INDEX_NONE;
    UEconomySubsystem::DecideNextLiquidationStep(50, 200, Assets, OutTile);
    TestEqual(TEXT("TC10: 同 MV 平手 → OutTile==5（数组首个，确定性 < 非 <=）"), OutTile, 5);

    return true;
}

// =============================================================================
// TC11（AC-43 变体C 自证）—— 逐栋 floor 口径在本 story 自证（防跨文件委托漂移）
//   引用 economy F-8 canonical：ComputeBuildingSellback(75)×3 == 3×37 == 111（≠ floor(3×75/2)=112）。
//   （NLV 值聚合的完整覆盖在 econ-008 ComputeNetLiquidationValue TC5；此处固化口径锚点。）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRaisingFunds_TC11_VariantCPerBuildingFloorTrace,
    "Rento.Economy.RaisingFundsBankruptcy.TC11_VariantCPerBuildingFloorTrace",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRaisingFunds_TC11_VariantCPerBuildingFloorTrace::RunTest(const FString& Parameters)
{
    using namespace RaisingFundsBankruptcyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC11: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("RFB_TC11_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    // 逐栋 floor：单栋 floor(75/2)=37；3 栋 = 111（≠ 合并 floor(112.5)=112）。
    TestEqual(TEXT("TC11: ComputeBuildingSellback(75)==37（单栋 floor）"), Econ->ComputeBuildingSellback(75), 37);
    TestEqual(TEXT("TC11: 3×37==111（逐栋 floor，≠112，变体C 自证）"), 3 * Econ->ComputeBuildingSellback(75), 111);

    DestroyGameWorld(World);
    return true;
}
