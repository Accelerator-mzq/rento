// SalaryF1Test.cpp
// =============================================================================
// 单元测试：发薪 F-1（econ-003）—— 公式 + gate(防双发) + 溢出 guard/runtime clamp
//
// 物理路径：Source/rentoTests/Private/SalaryF1Test.cpp
// 逻辑路径：tests/unit/economy-cash/salary_f1_test.cpp
// Automation 类目：Rento.Economy.SalaryF1
//
// 被测：UEconomySubsystem::PaySalary(PlayerId, PassedGo, SalaryAmount)
//   salary = clamp(max(passed_go,0),0,1000) × SalaryAmount；gate 在 passed_go（仅 >0 发）。
//   SalaryAmount 由调用方注入（caller-injected；board epic 未实现，economy 不直读 board）。
//
// 测试机制（headless -nullrhi）：复用 econ-001 harness 模式
//   - UWorld::CreateWorld(EWorldType::Game) 自动 Init UEconomySubsystem + UPlayerTurnSubsystem
//   - UPlayerTurnSubsystem::InitializeFromConfig(2 人) 建 PlayerId 0/1（Cash 默认 0）
//   - PS->SetCash(...) 直接 arrange（夹具，非被测路径）
//   - UEconomyEventSpy（AddDynamic）计 OnCashChanged。
//
// 断言非 vacuous（每条 flip 致真 FAIL）：
//   - TC1：去乘法/错系数 → Cash != 期望 → FAIL
//   - TC2：去 gate(passed_go≤0 也发) → CashChangedCount 0->1 / Cash 变 → FAIL
//   - TC3：去 clamp 上界 → passed_go=10⁷ 溢出/负值 → TestEqual(2e5) FAIL
//
// 规范依据：story econ-003 AC-6/7/8；GDD economy-cash F-1/CR-2；ADR-0014（整数确定性/溢出）/0001。
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
namespace SalaryF1TestHelpers
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

} // namespace SalaryF1TestHelpers

// =============================================================================
// TC-1（AC-6）—— 发薪公式：salary = passed_go × SalaryAmount
//   passed_go=1, Salary=200 → Cash+200，Credit 恰 1 次；passed_go=2 → +400。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSalaryF1_TC1_Formula,
    "Rento.Economy.SalaryF1.TC1_Formula",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSalaryF1_TC1_Formula::RunTest(const FString& Parameters)
{
    using namespace SalaryF1TestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-1: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Salary_TC1_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(0);              // arrange：起始 0

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // passed_go=1, Salary=200 → 200。
    const bool bPay1 = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/1, /*SalaryAmount=*/200);
    TestTrue(TEXT("TC-1: PaySalary(1,200) 返回 true"), bPay1);
    TestEqual(TEXT("TC-1: Cash == 200"), Econ->GetCash(0), 200);
    TestEqual(TEXT("TC-1: OnCashChanged 恰 1 次"), Spy->CashChangedCount, 1);
    TestEqual(TEXT("TC-1: reason == Salary"), static_cast<int32>(Spy->LastReason), static_cast<int32>(EChangeReason::Salary));
    TestEqual(TEXT("TC-1: OldCash==0"), Spy->LastOldCash, 0);
    TestEqual(TEXT("TC-1: NewCash==200"), Spy->LastNewCash, 200);

    // passed_go=2, Salary=200 → +400（累计 600）。
    const bool bPay2 = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/2, /*SalaryAmount=*/200);
    TestTrue(TEXT("TC-1: PaySalary(2,200) 返回 true"), bPay2);
    TestEqual(TEXT("TC-1: Cash == 200+400 == 600"), Econ->GetCash(0), 600);
    TestEqual(TEXT("TC-1: OnCashChanged 现 2 次"), Spy->CashChangedCount, 2);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-2（AC-7）—— gate 在 passed_go（防双重发薪）
//   passed_go=0/−1 → 不发（无 Credit、无事件）；停 GO passed_go=1 单发 1 次。
//   CollectSalary SpecialAction 仅 UI 标记 → 不经本接口构成第二次发薪（结构性：只调一次 PaySalary）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSalaryF1_TC2_GateOnPassedGo,
    "Rento.Economy.SalaryF1.TC2_GateOnPassedGo",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSalaryF1_TC2_GateOnPassedGo::RunTest(const FString& Parameters)
{
    using namespace SalaryF1TestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-2: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Salary_TC2_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(100);            // arrange：非零基线，验"不变"

    UEconomyEventSpy* Spy = BindSpy(Econ);

    // passed_go=0 → 不发。
    const bool bPay0 = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/0, /*SalaryAmount=*/200);
    TestFalse(TEXT("TC-2: passed_go=0 返回 false"), bPay0);
    TestEqual(TEXT("TC-2: Cash 不变（仍 100）"), Econ->GetCash(0), 100);
    TestEqual(TEXT("TC-2: passed_go=0 无 OnCashChanged"), Spy->CashChangedCount, 0);

    // passed_go=−1（逆向穿越）→ 不发。
    const bool bPayNeg = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/-1, /*SalaryAmount=*/200);
    TestFalse(TEXT("TC-2: passed_go=-1 返回 false"), bPayNeg);
    TestEqual(TEXT("TC-2: Cash 仍不变（100）"), Econ->GetCash(0), 100);
    TestEqual(TEXT("TC-2: 仍无 OnCashChanged"), Spy->CashChangedCount, 0);

    // 停 GO：passed_go=1 单发一次（CollectSalary 标记不另触发第二次 —— 本接口只被调一次）。
    const bool bPayStop = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/1, /*SalaryAmount=*/200);
    TestTrue(TEXT("TC-2: 停 GO passed_go=1 发薪 true"), bPayStop);
    TestEqual(TEXT("TC-2: Cash == 100+200 == 300"), Econ->GetCash(0), 300);
    TestEqual(TEXT("TC-2: 恰 1 次 OnCashChanged（单发，无双发）"), Spy->CashChangedCount, 1);

    UnbindSpy(Econ, Spy);
    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-3（AC-8）—— 溢出 guard + 真运行时 clamp
//   passed_go=12 → 2400 无溢出无 dev log；passed_go=13 → 2600 + dev Error；
//   passed_go=10⁷ → clamp 1000 → salary=2e5 无溢出/不负 + dev Error。
//   G5：AddExpectedError("abnormally large", Contains, 2)（13 + 10⁷ 两次 dev Error）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSalaryF1_TC3_OverflowGuardClamp,
    "Rento.Economy.SalaryF1.TC3_OverflowGuardClamp",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSalaryF1_TC3_OverflowGuardClamp::RunTest(const FString& Parameters)
{
    using namespace SalaryF1TestHelpers;

    // 期望 2 条 Error（passed_go=13 + 10⁷ 各一），否则未捕获的 Error 致测试 FAIL。
    AddExpectedError(TEXT("abnormally large"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-3: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Salary_TC3_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(0);

    // passed_go=12（阈值内，无 dev log）→ 12×200 == 2400 无溢出。
    const bool bPay12 = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/12, /*SalaryAmount=*/200);
    TestTrue(TEXT("TC-3: passed_go=12 发薪 true"), bPay12);
    TestEqual(TEXT("TC-3: 12×200 == 2400 无溢出"), Econ->GetCash(0), 2400);

    // passed_go=13（>12，dev Error）→ 13×200 == 2600（仍在 clamp 上界内，正常计算）。
    Turn->FindPlayerById(0)->SetCash(0);              // 重置便于断言纯薪额
    const bool bPay13 = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/13, /*SalaryAmount=*/200);
    TestTrue(TEXT("TC-3: passed_go=13 发薪 true"), bPay13);
    TestEqual(TEXT("TC-3: 13×200 == 2600 无溢出"), Econ->GetCash(0), 2600);

    // passed_go=10⁷（异常溢出输入，dev Error）→ clamp 到 1000 → 1000×200 == 200000 无溢出/不负。
    Turn->FindPlayerById(0)->SetCash(0);
    const bool bPayHuge = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/10000000, /*SalaryAmount=*/200);
    TestTrue(TEXT("TC-3: passed_go=10⁷ 发薪 true（clamp 后）"), bPayHuge);
    TestEqual(TEXT("TC-3: clamp→1000，1000×200 == 200000 无溢出"), Econ->GetCash(0), 200000);
    TestTrue(TEXT("TC-3: 结果不负（clamp 下界 0 保证）"), Econ->GetCash(0) >= 0);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4（AC-8 Edge 决定性）—— 两次冷启动同输入位级一致（经济无 RNG，纯函数）
//   两独立 World，passed_go=7, Salary=200 → 两次 salary == 1400 完全相等。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSalaryF1_TC4_ColdStartDeterminism,
    "Rento.Economy.SalaryF1.TC4_ColdStartDeterminism",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSalaryF1_TC4_ColdStartDeterminism::RunTest(const FString& Parameters)
{
    using namespace SalaryF1TestHelpers;

    auto RunOnce = [this](const FName& WorldName, int32& OutCash) -> bool
    {
        UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
        if (!SetupMatch(World, Econ, Turn, WorldName, 2)) { DestroyGameWorld(World); return false; }
        Turn->FindPlayerById(0)->SetCash(0);
        Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/7, /*SalaryAmount=*/200);
        OutCash = Econ->GetCash(0);
        DestroyGameWorld(World);
        return true;
    };

    int32 CashA = -1; int32 CashB = -1;
    if (!TestTrue(TEXT("TC-4: 冷启动 A"), RunOnce(TEXT("Salary_TC4_WorldA"), CashA))) { return false; }
    if (!TestTrue(TEXT("TC-4: 冷启动 B"), RunOnce(TEXT("Salary_TC4_WorldB"), CashB))) { return false; }

    TestEqual(TEXT("TC-4: A 结果 == 7×200 == 1400"), CashA, 1400);
    TestEqual(TEXT("TC-4: 两次冷启动位级一致（A == B）"), CashA, CashB);
    return true;
}

// =============================================================================
// TC-5（对抗 review CONCERN-1）—— SalaryAmount 溢出上界 economy 侧独立防线
//   board 加载期 fatal(≤2e6) 未实现期间，economy 自保：SalaryAmount>2e6 → 不发（refuse-not-clamp）。
//   防 1000×SalaryAmount 溢出 int32（仅 clamp passed_go 不足，需两因子均有界）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSalaryF1_TC5_SalaryAmountOverflowGuard,
    "Rento.Economy.SalaryF1.TC5_SalaryAmountOverflowGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSalaryF1_TC5_SalaryAmountOverflowGuard::RunTest(const FString& Parameters)
{
    using namespace SalaryF1TestHelpers;

    // SalaryAmount>2e6 与 ≤0 各触发 1 条 "out of safe range" Error（共 2 条）。
    AddExpectedError(TEXT("out of safe range"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Salary_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    Turn->FindPlayerById(0)->SetCash(0);

    // SalaryAmount=2,000,001（>上界）→ 拒绝、不发、不广播（防 1000×2,000,001 溢出风险）。
    const bool bOver = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/1, /*SalaryAmount=*/2000001);
    TestFalse(TEXT("TC-5: SalaryAmount>2e6 返回 false（拒绝）"), bOver);
    TestEqual(TEXT("TC-5: Cash 不变（仍 0，未错付）"), Econ->GetCash(0), 0);

    // 边界内最大 SalaryAmount=2,000,000 + passed_go clamp 上界 1000 → 2e9 不溢出（正确发薪）。
    const bool bBoundary = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/1, /*SalaryAmount=*/2000000);
    TestTrue(TEXT("TC-5: SalaryAmount==2e6 边界内发薪 true"), bBoundary);
    TestEqual(TEXT("TC-5: 1×2,000,000 == 2000000"), Econ->GetCash(0), 2000000);

    // SalaryAmount=0/负 → 拒绝（与 ≤0 防御合流，第 2 条 Error）。
    const bool bZero = Econ->PaySalary(/*PlayerId=*/0, /*PassedGo=*/1, /*SalaryAmount=*/0);
    TestFalse(TEXT("TC-5: SalaryAmount==0 返回 false"), bZero);
    TestEqual(TEXT("TC-5: Cash 仍 == 2000000（零额未改）"), Econ->GetCash(0), 2000000);

    DestroyGameWorld(World);
    return true;
}
