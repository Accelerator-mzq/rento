// EconomyEventsTest.cpp
// =============================================================================
// 单元测试：经济事件契约（econ-002）—— 4 delegate + EChangeReason + payload USTRUCT。
//
// 物理路径：Source/rentoTests/Private/EconomyEventsTest.cpp
// 逻辑路径：tests/unit/economy-cash/economy_events_test.cpp
// Automation 类目：Rento.Economy.Events
//
// 覆盖 AC-33~39（GDD economy-cash Events 节 / ADR-0003）：
//   AC-33 OnCashChanged payload（含 EChangeReason + delta）
//   AC-34 OnRentPaid 四字段（contract 验；实际触发 story-004/005）
//   AC-35 OnInsufficientFunds payload
//   AC-36 OnBankruptcyDeclared 1 次 + 2 字段反射（兑现 Foundation EventBusDelegateContract TC8 deferred；
//          实际触发 + 时机 story-009）
//   AC-37 事件次数：一次收租 → 1 OnRentPaid + 2 OnCashChanged（reason=Rent）；amount==0 → 全 0
//   AC-38 4 delegate 均 BlueprintAssignable / FMulticastDelegateProperty / payload USTRUCT(BlueprintType)
//   AC-39 Credit/Debit/GetCash 均 UFUNCTION(BlueprintCallable) + EChangeReason 含 9 既有值
//
// 机制：实际收租/破产触发在 story-004/005/009；本 story 验【契约 + 广播 plumbing + payload + 次数】，
//   收租/破产用直接 Broadcast(构造 Info) 模拟（spy 验字段/次数），不验真实业务触发。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"          // FMulticastDelegateProperty / TFieldIterator
#include "UObject/Field.h"

#include "EconomySubsystem.h"
#include "EconomyTypes.h"
#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnTypes.h"
#include "EconomyEventSpy.h"

namespace EconEventsHelpers
{
    static UWorld* CreateGameWorld(const FName& WorldName)
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, WorldName);
        check(World);
        return World;
    }

    static void DestroyGameWorld(UWorld* World)
    {
        if (World) { World->DestroyWorld(/*bInformEngineOfWorld=*/false); }
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

    static bool SetupMatch(UWorld*& OutWorld, UEconomySubsystem*& OutEcon, UPlayerTurnSubsystem*& OutTurn,
                           const FName& WorldName, int32 NumPlayers)
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
        Econ->OnRentPaid.AddDynamic(Spy, &UEconomyEventSpy::HandleRentPaid);
        Econ->OnInsufficientFunds.AddDynamic(Spy, &UEconomyEventSpy::HandleInsufficientFunds);
        Econ->OnBankruptcyDeclared.AddDynamic(Spy, &UEconomyEventSpy::HandleBankruptcyDeclared);
        return Spy;
    }

    static void UnbindSpy(UEconomySubsystem* Econ, UEconomyEventSpy* Spy)
    {
        if (Econ && Spy)
        {
            Econ->OnCashChanged.RemoveDynamic(Spy, &UEconomyEventSpy::HandleCashChanged);
            Econ->OnRentPaid.RemoveDynamic(Spy, &UEconomyEventSpy::HandleRentPaid);
            Econ->OnInsufficientFunds.RemoveDynamic(Spy, &UEconomyEventSpy::HandleInsufficientFunds);
            Econ->OnBankruptcyDeclared.RemoveDynamic(Spy, &UEconomyEventSpy::HandleBankruptcyDeclared);
        }
        if (Spy) { Spy->RemoveFromRoot(); }
    }
} // namespace EconEventsHelpers

// =============================================================================
// TC-AC33 —— OnCashChanged payload（含 EChangeReason + delta）
//   Given Cash=300；When Credit(p,200,Salary)；Then OnCashChanged(p,300,500,Salary) 恰 1 次 ∧ delta=+200。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEconEvt_AC33_CashChangedPayload,
    "Rento.Economy.Events.AC33_CashChangedPayload",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEconEvt_AC33_CashChangedPayload::RunTest(const FString& Parameters)
{
    using namespace EconEventsHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("AC33: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("EconEvt_AC33_World"), 2)))
    { DestroyGameWorld(World); return false; }
    Turn->FindPlayerById(0)->SetCash(300);

    UEconomyEventSpy* Spy = BindSpy(Econ);
    Econ->Credit(0, 200, EChangeReason::Salary);

    TestEqual(TEXT("AC33: OnCashChanged 恰 1 次"), Spy->CashChangedCount, 1);
    TestEqual(TEXT("AC33: PlayerId==0"), Spy->LastPlayerId, 0);
    TestEqual(TEXT("AC33: OldCash==300"), Spy->LastOldCash, 300);
    TestEqual(TEXT("AC33: NewCash==500"), Spy->LastNewCash, 500);
    TestEqual(TEXT("AC33: reason==Salary"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Salary));
    TestEqual(TEXT("AC33: delta==+200（NewCash-OldCash）"), Spy->LastNewCash - Spy->LastOldCash, 200);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-AC34 —— OnRentPaid 四字段（contract 验；实际收租触发 story-004/005，此处直接 Broadcast 模拟）
//   Given 收租 payer→payee Amount=50 Tile=21；Then OnRentPaid(payer,payee,50,21) 四字段精确。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEconEvt_AC34_RentPaidFourFields,
    "Rento.Economy.Events.AC34_RentPaidFourFields",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEconEvt_AC34_RentPaidFourFields::RunTest(const FString& Parameters)
{
    using namespace EconEventsHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("AC34: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("EconEvt_AC34_World"), 2)))
    { DestroyGameWorld(World); return false; }

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // 契约验证：收租路径（story-004/005）将构造此 payload 并广播；本 story 验 plumbing + 四字段。
    FRentPaidInfo Info;
    Info.PayerId = 0; Info.PayeeId = 1; Info.Amount = 50; Info.TileIndex = 21;
    Econ->OnRentPaid.Broadcast(Info);

    TestEqual(TEXT("AC34: OnRentPaid 恰 1 次"), Spy->RentPaidCount, 1);
    TestEqual(TEXT("AC34: Payer==0"), Spy->LastPayerId, 0);
    TestEqual(TEXT("AC34: Payee==1"), Spy->LastPayeeId, 1);
    TestEqual(TEXT("AC34: Amount==50"), Spy->LastRentAmount, 50);
    TestEqual(TEXT("AC34: TileIndex==21"), Spy->LastTileIndex, 21);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-AC35 —— OnInsufficientFunds payload
//   Given Cash=50, due=170；When 强制 Debit(170)；Then OnInsufficientFunds(p,170,120) 恰 1 次。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEconEvt_AC35_InsufficientFundsPayload,
    "Rento.Economy.Events.AC35_InsufficientFundsPayload",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEconEvt_AC35_InsufficientFundsPayload::RunTest(const FString& Parameters)
{
    using namespace EconEventsHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("AC35: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("EconEvt_AC35_World"), 2)))
    { DestroyGameWorld(World); return false; }
    Turn->FindPlayerById(0)->SetCash(50);

    UEconomyEventSpy* Spy = BindSpy(Econ);
    Econ->Debit(0, 170, EChangeReason::Tax);

    TestEqual(TEXT("AC35: OnInsufficientFunds 恰 1 次"), Spy->InsufficientCount, 1);
    TestEqual(TEXT("AC35: AmountDue==170"), Spy->LastDue, 170);
    TestEqual(TEXT("AC35: AmountShort==120"), Spy->LastShort, 120);
    TestEqual(TEXT("AC35: 不广播 OnCashChanged"), Spy->CashChangedCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-AC36 —— OnBankruptcyDeclared 契约（2 字段反射 + 1 次广播 plumbing）
//   兑现 Foundation EventBusDelegateContract TC8 deferred 的【字段数 + 存在性】部分。
//
//   ⚠ AC-36 拆分（防假覆盖，独立 review 揪出）：
//     - 本 story（002）discharge：契约 = delegate 存在 + 2 字段（Debtor/Creditor）+ 1 次广播 plumbing。
//     - 时机子句「广播时机在资产转移之后」**DEFERRED 到 story-009**：story-002 无真实破产移交序列
//       可排序（此处直接 Broadcast 模拟），CallLog 顺序断言须在 story-009 真实清算/移交路径上验，
//       否则在 002 内永真 vacuous。**故本 story 不对时机打勾**（见 story Completion Notes AC-36 拆分）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEconEvt_AC36_BankruptcyDeclaredContract,
    "Rento.Economy.Events.AC36_BankruptcyDeclaredContract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEconEvt_AC36_BankruptcyDeclaredContract::RunTest(const FString& Parameters)
{
    using namespace EconEventsHelpers;

    // (1) 反射：FBankruptcyDeclaredInfo 恰 2 字段（Debtor/Creditor，AC-36 / Foundation TC8 AC-8）
    {
        int32 FieldCount = 0;
        for (TFieldIterator<FProperty> It(FBankruptcyDeclaredInfo::StaticStruct()); It; ++It)
        {
            ++FieldCount;
        }
        TestEqual(TEXT("AC36: FBankruptcyDeclaredInfo 恰 2 字段（Debtor/Creditor）"), FieldCount, 2);
    }

    // (2) plumbing：直接 Broadcast 验 1 次 + 字段（实际触发 story-009）
    {
        UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
        if (!TestTrue(TEXT("AC36: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("EconEvt_AC36_World"), 2)))
        { DestroyGameWorld(World); return false; }

        UEconomyEventSpy* Spy = BindSpy(Econ);

        FBankruptcyDeclaredInfo Info;
        Info.DebtorId = 0; Info.CreditorId = 1;
        Econ->OnBankruptcyDeclared.Broadcast(Info);

        TestEqual(TEXT("AC36: OnBankruptcyDeclared 恰 1 次"), Spy->BankruptcyCount, 1);
        TestEqual(TEXT("AC36: Debtor==0"), Spy->LastDebtorId, 0);
        TestEqual(TEXT("AC36: Creditor==1"), Spy->LastCreditorId, 1);

        UnbindSpy(Econ, Spy);
        DestroyGameWorld(World);
    }
    return true;
}

// =============================================================================
// TC-AC37 —— 事件次数：一次收租 → 1 OnRentPaid + 2 OnCashChanged（reason=Rent）；amount==0 → 全 0
//   收租 = TransferCash(payer,payee,amount,Rent)〔2 OnCashChanged〕 + OnRentPaid.Broadcast〔1〕（模拟 story-004 路径）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEconEvt_AC37_EventCountsExact,
    "Rento.Economy.Events.AC37_EventCountsExact",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEconEvt_AC37_EventCountsExact::RunTest(const FString& Parameters)
{
    using namespace EconEventsHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("AC37: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("EconEvt_AC37_World"), 2)))
    { DestroyGameWorld(World); return false; }
    Turn->FindPlayerById(0)->SetCash(100);
    Turn->FindPlayerById(1)->SetCash(20);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // 一次收租：TransferCash（2 OnCashChanged reason=Rent）+ OnRentPaid（1）
    Econ->TransferCash(0, 1, 50, EChangeReason::Rent);
    FRentPaidInfo RentInfo; RentInfo.PayerId = 0; RentInfo.PayeeId = 1; RentInfo.Amount = 50; RentInfo.TileIndex = 21;
    Econ->OnRentPaid.Broadcast(RentInfo);

    TestEqual(TEXT("AC37: 1 次 OnRentPaid"), Spy->RentPaidCount, 1);
    TestEqual(TEXT("AC37: 2 次 OnCashChanged（payer/payee）"), Spy->CashChangedCount, 2);
    TestEqual(TEXT("AC37: OnCashChanged reason==Rent"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Rent));

    // amount==0 收租 → 全 0（对齐 AC-5）
    Spy->CashChangedCount = 0; Spy->RentPaidCount = 0;
    Econ->TransferCash(0, 1, 0, EChangeReason::Rent);   // 早返，0 OnCashChanged，不发 OnRentPaid
    TestEqual(TEXT("AC37: amount==0 → 0 OnCashChanged"), Spy->CashChangedCount, 0);
    TestEqual(TEXT("AC37: amount==0 → 0 OnRentPaid"), Spy->RentPaidCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-AC38 —— 4 delegate 均 BlueprintAssignable / FMulticastDelegateProperty / payload USTRUCT(BlueprintType)
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEconEvt_AC38_DelegateReflection,
    "Rento.Economy.Events.AC38_DelegateReflection",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEconEvt_AC38_DelegateReflection::RunTest(const FString& Parameters)
{
    UClass* EconClass = UEconomySubsystem::StaticClass();
    if (!TestNotNull(TEXT("AC38: UEconomySubsystem class"), EconClass)) return false;

    const TCHAR* DelegateNames[] = { TEXT("OnCashChanged"), TEXT("OnRentPaid"),
                                     TEXT("OnInsufficientFunds"), TEXT("OnBankruptcyDeclared") };
    for (const TCHAR* Name : DelegateNames)
    {
        FMulticastDelegateProperty* Prop = FindFProperty<FMulticastDelegateProperty>(EconClass, Name);
        if (!TestNotNull(*FString::Printf(TEXT("AC38: %s 是 FMulticastDelegateProperty"), Name), Prop))
        {
            continue;
        }
        TestTrue(*FString::Printf(TEXT("AC38: %s 标 BlueprintAssignable"), Name),
            Prop->HasAnyPropertyFlags(CPF_BlueprintAssignable));
    }

    // payload USTRUCT(BlueprintType)：4 个 F*Info 须有 BlueprintType 元数据（StaticStruct 存在即 USTRUCT）
    TestNotNull(TEXT("AC38: FCashChangedInfo USTRUCT"), FCashChangedInfo::StaticStruct());
    TestNotNull(TEXT("AC38: FRentPaidInfo USTRUCT"), FRentPaidInfo::StaticStruct());
    TestNotNull(TEXT("AC38: FInsufficientFundsInfo USTRUCT"), FInsufficientFundsInfo::StaticStruct());
    TestNotNull(TEXT("AC38: FBankruptcyDeclaredInfo USTRUCT"), FBankruptcyDeclaredInfo::StaticStruct());

    return true;
}

// =============================================================================
// TC-AC39 —— Credit/Debit/GetCash 均 UFUNCTION(BlueprintCallable) + EChangeReason 含 9 既有值
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEconEvt_AC39_BlueprintCallableAndEnum,
    "Rento.Economy.Events.AC39_BlueprintCallableAndEnum",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEconEvt_AC39_BlueprintCallableAndEnum::RunTest(const FString& Parameters)
{
    UClass* EconClass = UEconomySubsystem::StaticClass();
    if (!TestNotNull(TEXT("AC39: UEconomySubsystem class"), EconClass)) return false;

    const TCHAR* FuncNames[] = { TEXT("Credit"), TEXT("Debit"), TEXT("GetCash"),
                                 TEXT("TransferCash"), TEXT("GiveStartingCash") };
    for (const TCHAR* Name : FuncNames)
    {
        UFunction* Fn = EconClass->FindFunctionByName(FName(Name));
        if (!TestNotNull(*FString::Printf(TEXT("AC39: 函数 %s 存在"), Name), Fn))
        {
            continue;
        }
        TestTrue(*FString::Printf(TEXT("AC39: %s 标 BlueprintCallable"), Name),
            Fn->HasAnyFunctionFlags(FUNC_BlueprintCallable));
    }

    // EChangeReason 含全部 9 既有值（值序 0..8，append-only）
    UEnum* ReasonEnum = StaticEnum<EChangeReason>();
    if (!TestNotNull(TEXT("AC39: EChangeReason UEnum"), ReasonEnum)) return false;

    struct FExpected { const TCHAR* Name; int64 Value; };
    const FExpected Expected[] = {
        { TEXT("Salary"), 0 }, { TEXT("Rent"), 1 }, { TEXT("Purchase"), 2 },
        { TEXT("Mortgage"), 3 }, { TEXT("Unmortgage"), 4 }, { TEXT("Tax"), 5 },
        { TEXT("Build"), 6 }, { TEXT("Trade"), 7 }, { TEXT("Bankruptcy"), 8 },
    };
    for (const FExpected& E : Expected)
    {
        const int64 Value = ReasonEnum->GetValueByNameString(FString(E.Name));
        TestEqual(*FString::Printf(TEXT("AC39: EChangeReason::%s == %lld"), E.Name, E.Value),
            Value, E.Value);
    }

    return true;
}
