// EconomyCashSubsystemTest.cpp
// =============================================================================
// 单元测试：经济现金服务受控写 + Cash>=0 不变式 + 原子守恒 + amount 契约（econ-001）
//
// 物理路径：Source/rentoTests/Private/EconomyCashSubsystemTest.cpp
// 逻辑路径：tests/unit/economy-cash/cash_service_test.cpp
// Automation 类目：Rento.Economy.CashService
//
// 测试机制（headless -nullrhi）：
//   - UWorld::CreateWorld(EWorldType::Game) 自动 Init UEconomySubsystem + UPlayerTurnSubsystem
//   - 经 UPlayerTurnSubsystem::InitializeFromConfig(2 人) 建 PlayerId 0/1（Cash 默认 0）
//   - 起始 Cash 用 PS->SetCash(...) 直接 arrange（测试夹具，非被测路径）
//   - 事件用 UEconomyEventSpy（AddDynamic）计数
//
// 断言非 vacuous（每条 flip 均可致真 FAIL）：
//   - TC1：去掉 Cash>=0 守卫则 Cash 落 -30 → TestEqual(50) FAIL；InsufficientCount 0->? FAIL
//   - TC2：两腿重算/缺一腿则总和 != 120 → FAIL
//   - TC5：amount==0 若仍广播则 CashChangedCount 0->1 FAIL
//   - amount<0：若不拒绝则 Cash 变 → FAIL（且无 AddExpectedError 时 Error log 致 FAIL）
//
// 规范依据：story econ-001 AC-1~5 + amount<0；GDD economy-cash CR-1/CR-8；ADR-0001/0007/0014。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "EconomySubsystem.h"
#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnTypes.h"       // EPlayerColor / EAIDifficulty / FPlayerSetupEntry
#include "EconomyEventSpy.h"

// =============================================================================
// 测试辅助（static 内部链接，避免与其他 TU 同名冲突）
// =============================================================================
namespace EconTestHelpers
{
    /** 创建 Game World（headless）。调用方负责 DestroyWorld。 */
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

    /** 构建含 N 个玩家的合法 FGameSetupConfig（PlayerId 将分配为 0..N-1）。 */
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

    /**
     * 标准夹具：建 World + 建 N 人队 + 拿 economy 子系统。
     * 返回 false 表示建立失败（调用方应清理 World 并 return false）。
     */
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

    /** 绑定 spy 到 economy 两事件。 */
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

} // namespace EconTestHelpers

// =============================================================================
// TC-1（AC-1）—— Cash>=0 不变式：不足额不落地为负，转 Raising Funds
//   GIVEN Cash=50；WHEN Debit(80)；THEN Cash==50 ∧ InsufficientFunds 1 次 ∧ CashChanged 0 次。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEcon_TC1_CashFloorInsufficientFunds,
    "Rento.Economy.CashService.TC1_CashFloorInsufficientFunds",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEcon_TC1_CashFloorInsufficientFunds::RunTest(const FString& Parameters)
{
    using namespace EconTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-1: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Econ_TC1_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    URentoPlayerState* PS = Turn->FindPlayerById(0);
    if (!TestNotNull(TEXT("TC-1: PS0"), PS)) { DestroyGameWorld(World); return false; }
    PS->SetCash(50);                                  // arrange（夹具直写，非被测路径）

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bResult = Econ->Debit(0, 80, EChangeReason::Tax);  // act（reason 任选；不足额走 InsufficientFunds 不携 reason）

    TestFalse(TEXT("TC-1: Debit(80) 应返回 false（不足额）"), bResult);
    TestEqual(TEXT("TC-1: Cash 仍 == 50（不落地为负）"), Econ->GetCash(0), 50);
    TestEqual(TEXT("TC-1: OnInsufficientFunds 触发 1 次"), Spy->InsufficientCount, 1);
    TestEqual(TEXT("TC-1: OnCashChanged 0 次"), Spy->CashChangedCount, 0);
    TestEqual(TEXT("TC-1: AmountDue==80"), Spy->LastDue, 80);
    TestEqual(TEXT("TC-1: AmountShort==30"), Spy->LastShort, 30);

    // Edge：Debit(50) 恰等额 → Cash==0 成功、不触 InsufficientFunds、CashChanged +1。
    const bool bEdge = Econ->Debit(0, 50, EChangeReason::Tax);
    TestTrue(TEXT("TC-1 Edge: Debit(50) 恰等额成功"), bEdge);
    TestEqual(TEXT("TC-1 Edge: Cash==0"), Econ->GetCash(0), 0);
    TestEqual(TEXT("TC-1 Edge: InsufficientFunds 仍 1 次（恰等额不触发）"), Spy->InsufficientCount, 1);
    TestEqual(TEXT("TC-1 Edge: OnCashChanged 现 1 次"), Spy->CashChangedCount, 1);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-2（AC-2）—— 原子转移守恒（CR-8）
//   GIVEN payer.Cash=100, payee.Cash=20, R=30；WHEN TransferCash；
//   THEN payer==70 ∧ payee==50 ∧ 总和 120 不变 ∧ 2 次 OnCashChanged。
//   Edge：R==payer.Cash（全额）→ payer==0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEcon_TC2_AtomicTransferConservation,
    "Rento.Economy.CashService.TC2_AtomicTransferConservation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEcon_TC2_AtomicTransferConservation::RunTest(const FString& Parameters)
{
    using namespace EconTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-2: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Econ_TC2_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    URentoPlayerState* Payer = Turn->FindPlayerById(0);
    URentoPlayerState* Payee = Turn->FindPlayerById(1);
    if (!TestNotNull(TEXT("TC-2: Payer"), Payer) || !TestNotNull(TEXT("TC-2: Payee"), Payee))
    {
        DestroyGameWorld(World); return false;
    }
    Payer->SetCash(100);
    Payee->SetCash(20);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bResult = Econ->TransferCash(/*Payer=*/0, /*Payee=*/1, /*R=*/30, EChangeReason::Rent);

    TestTrue(TEXT("TC-2: TransferCash 成功"), bResult);
    TestEqual(TEXT("TC-2: payer == 100-30 == 70"), Econ->GetCash(0), 70);
    TestEqual(TEXT("TC-2: payee == 20+30 == 50"), Econ->GetCash(1), 50);
    TestEqual(TEXT("TC-2: 总和守恒 == 120"), Econ->GetCash(0) + Econ->GetCash(1), 120);
    TestEqual(TEXT("TC-2: 2 次 OnCashChanged（payer/payee 各一）"), Spy->CashChangedCount, 2);
    TestEqual(TEXT("TC-2: 两腿 reason==Rent"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Rent));
    TestEqual(TEXT("TC-2: 0 次 InsufficientFunds"), Spy->InsufficientCount, 0);

    // Edge：全额转移 R==payer.Cash → payer==0。
    const bool bEdgeResult = Econ->TransferCash(/*Payer=*/0, /*Payee=*/1, /*R=*/70, EChangeReason::Rent);
    TestTrue(TEXT("TC-2 Edge: 全额转移返回 true"), bEdgeResult);
    TestEqual(TEXT("TC-2 Edge: 全额后 payer==0"), Econ->GetCash(0), 0);
    TestEqual(TEXT("TC-2 Edge: payee==50+70==120"), Econ->GetCash(1), 120);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-3（AC-3）—— Credit/Debit 单调 + 不副作用他人 Cash
//   GIVEN Cash=100；WHEN Credit(40)->140, Debit(25)->115；THEN 精确等值 ∧ 他玩家不变。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEcon_TC3_MonotonicNoSideEffect,
    "Rento.Economy.CashService.TC3_MonotonicNoSideEffect",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEcon_TC3_MonotonicNoSideEffect::RunTest(const FString& Parameters)
{
    using namespace EconTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-3: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Econ_TC3_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(100);
    Turn->FindPlayerById(1)->SetCash(500);            // 他玩家对照基线

    TestTrue(TEXT("TC-3: Credit(40)"), Econ->Credit(0, 40, EChangeReason::Salary));
    TestEqual(TEXT("TC-3: Cash == 140"), Econ->GetCash(0), 140);
    TestTrue(TEXT("TC-3: Debit(25)"), Econ->Debit(0, 25, EChangeReason::Tax));
    TestEqual(TEXT("TC-3: Cash == 115"), Econ->GetCash(0), 115);
    TestEqual(TEXT("TC-3: 他玩家 Cash 不受副作用（仍 500）"), Econ->GetCash(1), 500);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-5（AC-5）—— amount==0 早返静默
//   GIVEN amount=0；WHEN Credit(0)/Debit(0)；THEN Cash 不变 ∧ OnCashChanged 0 次。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEcon_TC5_ZeroAmountSilentEarlyReturn,
    "Rento.Economy.CashService.TC5_ZeroAmountSilentEarlyReturn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEcon_TC5_ZeroAmountSilentEarlyReturn::RunTest(const FString& Parameters)
{
    using namespace EconTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Econ_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(100);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bCredit0 = Econ->Credit(0, 0, EChangeReason::Salary);
    const bool bDebit0  = Econ->Debit(0, 0, EChangeReason::Tax);

    TestFalse(TEXT("TC-5: Credit(0) 返回 false（早返）"), bCredit0);
    TestFalse(TEXT("TC-5: Debit(0) 返回 false（早返）"), bDebit0);
    TestEqual(TEXT("TC-5: Cash 不变（== 100）"), Econ->GetCash(0), 100);
    TestEqual(TEXT("TC-5: OnCashChanged 0 次"), Spy->CashChangedCount, 0);
    TestEqual(TEXT("TC-5: OnInsufficientFunds 0 次"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-6（amount<0 契约）—— 负 amount 非法：拒绝、不改 Cash、不广播
//   WHEN Credit(-10)/Debit(-10)；THEN Cash 不变 ∧ dev log 捕获 ∧ 不广播。
//   G5：AddExpectedError ASCII 子串 "must be non-negative"，精确 2 次（Credit+Debit 各一）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEcon_TC6_NegativeAmountRejected,
    "Rento.Economy.CashService.TC6_NegativeAmountRejected",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEcon_TC6_NegativeAmountRejected::RunTest(const FString& Parameters)
{
    using namespace EconTestHelpers;

    // 期望 2 条 Error（Credit + Debit 各一），否则未捕获的 Error 会致测试 FAIL。
    AddExpectedError(TEXT("must be non-negative"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-6: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Econ_TC6_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(100);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    const bool bCreditNeg = Econ->Credit(0, -10, EChangeReason::Salary);
    const bool bDebitNeg  = Econ->Debit(0, -10, EChangeReason::Tax);

    TestFalse(TEXT("TC-6: Credit(-10) 拒绝"), bCreditNeg);
    TestFalse(TEXT("TC-6: Debit(-10) 拒绝"), bDebitNeg);
    TestEqual(TEXT("TC-6: Cash 不变（== 100）"), Econ->GetCash(0), 100);
    TestEqual(TEXT("TC-6: 不广播 OnCashChanged"), Spy->CashChangedCount, 0);
    TestEqual(TEXT("TC-6: 不广播 OnInsufficientFunds"), Spy->InsufficientCount, 0);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-7（GiveStartingCash）—— 数据驱动起始资金经 Credit 入账
//   WHEN GiveStartingCash(0)；THEN Cash == StartingCash（默认 1500）∧ OnCashChanged 1 次。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEcon_TC7_GiveStartingCash,
    "Rento.Economy.CashService.TC7_GiveStartingCash",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEcon_TC7_GiveStartingCash::RunTest(const FString& Parameters)
{
    using namespace EconTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-7: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Econ_TC7_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    // PlayerId 0 默认 Cash==0。
    TestEqual(TEXT("TC-7: 初始 Cash==0"), Econ->GetCash(0), 0);

    UEconomyEventSpy* Spy = BindSpy(Econ);

    Econ->GiveStartingCash(0);

    TestEqual(TEXT("TC-7: Cash == StartingCash(1500)"), Econ->GetCash(0), Econ->StartingCash);
    TestEqual(TEXT("TC-7: Cash == 1500（数据驱动默认）"), Econ->GetCash(0), 1500);
    TestEqual(TEXT("TC-7: OnCashChanged 1 次"), Spy->CashChangedCount, 1);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}
