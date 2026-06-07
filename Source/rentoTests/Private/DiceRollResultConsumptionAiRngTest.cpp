// DiceRollResultConsumptionAiRngTest.cpp
// =============================================================================
// 单元测试：RollPhase 消费 FDiceRollResult + holder PULL + 程间非重入 + SentToJail 抑制
// （pt-006 AC-1/AC-2/AC-3/AC-4；AC-5 为 Advisory 静态约束，见 evidence markdown）
//
// 物理路径：Source/rentoTests/Private/DiceRollResultConsumptionAiRngTest.cpp
// 逻辑路径：tests/unit/player-turn/dicerollresult_consumption_ai_rng_test.cpp
// Automation 类目：Rento.PlayerTurn.DiceRollResultConsumption
//
// 测试机制（headless -nullrhi）：
//   - UWorld::CreateWorld(EWorldType::Game) 创建 Game World，自动 Init UPlayerTurnSubsystem
//   - 骰子3 用受控构造 FDiceRollResult 注入（不调真实 RollDice，确定性 fixture）
//   - 移动4 OnPawnLanded 用 FPawnMoverSpy（IPawnMover DI seam）
//   - 落地结算 hook 用 FLandingResolverSpy（ILandingResolver DI seam）计数 DecideBuyProperty
//
// 断言非 vacuous 保证（每条 flip 均可致真 FAIL）：
//   - TC-1：holder 逐字段断言；若 ConsumeRollResult 未写 holder，TestEqual 真 FAIL
//   - TC-2：事件额外掷骰更新 holder 后 PULL 读到新 Total；若读到 stale，TestEqual FAIL
//   - TC-3：第二程 Advance 发起时 FlagAtAdvanceIssue==false；若回调内同步递归发起，==true → FAIL
//   - TC-4：SentToJail 时 DecideBuyProperty==0、DiceMove 时==1；若抑制失效则计数 FAIL
//
// 规范依据：
//   - story pt-006 AC-1~5，TC-1~5，Edge
//   - ADR-0004（确定性 RNG / holder 单源）、ADR-0001（宿主）、ADR-0003（事件总线）
//   - GDD CR-3/CR-3.1（holder=回合2/程间非重入）、Edge Cases L392 / AC-46 / AC-47
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"
#include "DiceRngService.h"        // FDiceRollResult
#include "LandingResolverSpy.h"    // FLandingResolverSpy（AC-4）
#include "PawnMoverSpy.h"          // FPawnMoverSpy（AC-3 / AC-47）

// =============================================================================
// 测试辅助（纯 C++ 函数，static 内部链接，避免与其他 TU 同名冲突）
// =============================================================================
namespace Pt006TestHelpers
{
    /** 创建 Game World（headless，非通知引擎）。调用方负责 DestroyWorld。 */
    static UWorld* CreateGameWorld(const FName& WorldName)
    {
        UWorld* World = UWorld::CreateWorld(
            EWorldType::Game,
            /*bInformEngineOfWorld=*/false,
            WorldName);
        check(World);
        return World;
    }

    /** 销毁测试 World。 */
    static void DestroyGameWorld(UWorld* World)
    {
        if (World)
        {
            World->DestroyWorld(/*bInformEngineOfWorld=*/false);
        }
    }

    /** 构建含 N 个玩家的合法 FGameSetupConfig（阈值/平手轮可覆盖）。 */
    static FGameSetupConfig MakeConfig(
        int32 N,
        int32 DoublesThreshold = 3,
        int32 TiebreakRounds   = 1)
    {
        FGameSetupConfig Config;
        Config.DoublesJailThreshold = DoublesThreshold;
        Config.MaxTiebreakRounds    = TiebreakRounds;
        Config.RandomSeed           = 42;

        const EPlayerColor Colors[] = {
            EPlayerColor::Red,
            EPlayerColor::Blue,
            EPlayerColor::Green,
            EPlayerColor::Yellow,
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

    /** 找 TurnOrderIndex==0 的玩家 PlayerId（-1 表示找不到）。 */
    static int32 FindFirstPlayerId(const UPlayerTurnSubsystem* Sub)
    {
        for (const TObjectPtr<URentoPlayerState>& State : Sub->GetPlayerStates())
        {
            if (State && State->TurnOrderIndex == 0)
            {
                return State->PlayerId;
            }
        }
        return -1;
    }

    /** 构造受控 FDiceRollResult（逐字段赋值，FDiceRollResult 含 GENERATED_BODY 不可聚合初始化）。 */
    static FDiceRollResult MakeRoll(int32 Die1, int32 Die2)
    {
        FDiceRollResult R;
        R.Die1      = Die1;
        R.Die2      = Die2;
        R.Total     = Die1 + Die2;
        R.bIsDouble = (Die1 == Die2);
        return R;
    }

} // namespace Pt006TestHelpers

// =============================================================================
// TC-1（AC-1）— RollPhase 消费完整 FDiceRollResult：holder 持有 + bIsDouble 驱动 F-3
//
// GIVEN：2 人正常对局，先手玩家不在狱，处于 RollPhase
// WHEN：ConsumeRollResult(FDiceRollResult{Die1=2,Die2=2,Total=4,bIsDouble=true})
// THEN：① 当前玩家 CurrentRollContext 逐字段 == 注入值（holder 持完整结果）
//       ② ConsecutiveDoubles==1（bIsDouble 驱动 F-3 双点链 +1）
//       ③ GetCurrentRollContext() 读回同值（PULL accessor 一致）
//
// 非 vacuous：若 ConsumeRollResult 不写 holder，CurrentRollContext.Total!=4 → TestEqual FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceConsume_TC1_RollPhaseConsumesFullResult,
    "Rento.PlayerTurn.DiceRollResultConsumption.TC1_RollPhaseConsumesFullResult",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceConsume_TC1_RollPhaseConsumesFullResult::RunTest(const FString& Parameters)
{
    UWorld* World = Pt006TestHelpers::CreateGameWorld(TEXT("Pt006_TC1_World"));
    if (!TestNotNull(TEXT("TC-1: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-1: Sub"), Sub))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(Pt006TestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId = Pt006TestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-1: P0"), P0))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail = false;
    P0->ConsecutiveDoubles = 0;

    Sub->StartTurn(FirstId);
    if (!TestEqual(TEXT("TC-1 pre: 应处于 RollPhase"),
            Sub->GetCurrentPhase(), ETurnPhase::RollPhase))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }

    // WHEN：消费完整 FDiceRollResult{2,2,4,true}（双点）
    const FDiceRollResult Roll = Pt006TestHelpers::MakeRoll(2, 2);
    bool bSentToJail = false;
    Sub->ConsumeRollResult(Roll, bSentToJail);

    // THEN ①：holder 逐字段持完整结果
    TestEqual(TEXT("TC-1: holder.Die1==2"),      P0->CurrentRollContext.Die1, 2);
    TestEqual(TEXT("TC-1: holder.Die2==2"),      P0->CurrentRollContext.Die2, 2);
    TestEqual(TEXT("TC-1: holder.Total==4"),     P0->CurrentRollContext.Total, 4);
    TestTrue (TEXT("TC-1: holder.bIsDouble==true"), P0->CurrentRollContext.bIsDouble);

    // THEN ②：bIsDouble 驱动 F-3 双点链 +1（阈值 3，未触发送监狱）
    TestEqual(TEXT("TC-1: ConsecutiveDoubles==1（bIsDouble 已消费）"),
        P0->ConsecutiveDoubles, 1);
    TestFalse(TEXT("TC-1: 第 1 次双点不触发 F-3，bSentToJail=false"), bSentToJail);

    // THEN ③：PULL accessor 读回同值
    const FDiceRollResult Pulled = Sub->GetCurrentRollContext();
    TestEqual(TEXT("TC-1: GetCurrentRollContext().Total==4"), Pulled.Total, 4);
    TestTrue (TEXT("TC-1: GetCurrentRollContext().bIsDouble==true"), Pulled.bIsDouble);
    TestEqual(TEXT("TC-1: GetCurrentRollTotal()==4"), Sub->GetCurrentRollTotal(), 4);

    Pt006TestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-2（AC-2）— holder 单源 PULL：事件额外掷骰更新 holder 后读到当前程（不串程）
//
// GIVEN：先手玩家 RollPhase 消费正常程 {3,4,7,false}
// WHEN：① PULL GetCurrentRollTotal() → 7
//       ② 事件额外掷骰经受控写 SetCurrentRollContext({5,5,10,true}) 更新 holder
//       ③ 再 PULL
// THEN：第二次 PULL 读到 10（当前程），不是 stale 7（holder 单源、不串程）
//
// 非 vacuous：若 PULL 缓存了旧值或读错玩家，GetCurrentRollTotal()!=10 → TestEqual FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceConsume_TC2_HolderSingleSourcePull,
    "Rento.PlayerTurn.DiceRollResultConsumption.TC2_HolderSingleSourcePull",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceConsume_TC2_HolderSingleSourcePull::RunTest(const FString& Parameters)
{
    UWorld* World = Pt006TestHelpers::CreateGameWorld(TEXT("Pt006_TC2_World"));
    if (!TestNotNull(TEXT("TC-2: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-2: Sub"), Sub))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(Pt006TestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId = Pt006TestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-2: P0"), P0))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail = false;
    P0->ConsecutiveDoubles = 0;

    Sub->StartTurn(FirstId);

    // 正常程：消费 {3,4,7,false}
    const FDiceRollResult Leg1 = Pt006TestHelpers::MakeRoll(3, 4);
    bool bSent = false;
    Sub->ConsumeRollResult(Leg1, bSent);

    // PULL 正常程 Total
    TestEqual(TEXT("TC-2: 正常程 PULL GetCurrentRollTotal()==7"),
        Sub->GetCurrentRollTotal(), 7);

    // 事件额外掷骰：经受控写 SetCurrentRollContext 更新 holder（事件7 路径，{5,5,10,true}）
    const FDiceRollResult ExtraRoll = Pt006TestHelpers::MakeRoll(5, 5);
    P0->SetCurrentRollContext(ExtraRoll);

    // 再 PULL：读到当前程 10（不串程，不读 stale 7）
    TestEqual(TEXT("TC-2: 事件额外掷骰后 PULL 读当前程 Total==10（不串程）"),
        Sub->GetCurrentRollTotal(), 10);
    TestTrue(TEXT("TC-2: 事件额外掷骰后 holder.bIsDouble==true（读当前程）"),
        Sub->GetCurrentRollContext().bIsDouble);

    Pt006TestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-3（AC-3 / AC-47）— 程间非重入：第二程 Advance 在第一程落地回调返回之后发起
//
// GIVEN：注入 FPawnMoverSpy（Advance 内模拟落地回调窗口，第一程回调内请求第二程）
// WHEN：OrchestrateMove(7) 启动第一程
// THEN：① AdvanceCount==2（第二程真被发起，非仅日志）
//       ② FlagAtAdvanceIssue[1]==false（第二程发起时不处于任何落地回调内 → 严格串行）
//
// 非 vacuous（变异自检）：若回合2 在 HandlePawnLanded 回调栈内同步递归发起第二程，
//   第二程发起时 FPawnMoverSpy::bInLandedCallback==true → FlagAtAdvanceIssue[1]==true → FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceConsume_TC3_InterLegNonReentrancy,
    "Rento.PlayerTurn.DiceRollResultConsumption.TC3_InterLegNonReentrancy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceConsume_TC3_InterLegNonReentrancy::RunTest(const FString& Parameters)
{
    UWorld* World = Pt006TestHelpers::CreateGameWorld(TEXT("Pt006_TC3_World"));
    if (!TestNotNull(TEXT("TC-3: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-3: Sub"), Sub))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(Pt006TestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId = Pt006TestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-3: P0"), P0))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail = false;
    Sub->StartTurn(FirstId);  // 置 CurrentActivePlayerId

    // 注入移动 spy：第一程落地回调内请求第二程
    TSharedPtr<FPawnMoverSpy> Mover = MakeShared<FPawnMoverSpy>();
    Mover->Sub               = Sub;
    Mover->ArrivalToReport   = EArrivalContext::DiceMove;
    Mover->bRequestSecondLeg = true;
    Mover->SecondLegSteps    = 3;
    Sub->SetPawnMover(Mover);

    // WHEN：启动第一程（7 步）
    Sub->OrchestrateMove(7);

    // THEN ①：第二程真被蹦床发起
    TestEqual(TEXT("TC-3: 共发起 2 程 Advance（第二程真被发起）"),
        Mover->AdvanceCount, 2);
    if (!TestEqual(TEXT("TC-3: FlagAtAdvanceIssue 记录 2 程"),
            Mover->FlagAtAdvanceIssue.Num(), 2))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }

    // THEN ②：第一程在顶层发起（flag false）；第二程在第一程回调返回后发起（flag false，AC-47 核心）
    TestFalse(TEXT("TC-3: 第一程 Advance 发起时 bInLandedCallback==false"),
        Mover->FlagAtAdvanceIssue[0]);
    TestFalse(TEXT("TC-3: 第二程 Advance 发起时 bInLandedCallback==false（严格串行，无同步嵌套重入）"),
        Mover->FlagAtAdvanceIssue[1]);

    Pt006TestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4（AC-4 / AC-46）— SentToJail 落地抑制：DecideBuyProperty 不调；DiceMove 对照组调 1 次
//
// GIVEN：注入 FLandingResolverSpy，先手玩家活跃
// WHEN：① HandlePawnLanded(SentToJail) → ResolveArrival
//       ② HandlePawnLanded(DiceMove)   → ResolveArrival（对照组）
// THEN：① SentToJail 后 DecideBuyProperty 调用次数==0（抑制全部落地结算）
//       ② DiceMove 后 DecideBuyProperty 调用次数==1（证明抑制由 arrivalContext 驱动）
//
// 非 vacuous：若 SentToJail 未抑制，计数变 1 → TestEqual 0 FAIL；
//             若 DiceMove 未调买地，计数仍 0 → TestEqual 1 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceConsume_TC4_SentToJailSuppressesResolve,
    "Rento.PlayerTurn.DiceRollResultConsumption.TC4_SentToJailSuppressesResolve",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceConsume_TC4_SentToJailSuppressesResolve::RunTest(const FString& Parameters)
{
    UWorld* World = Pt006TestHelpers::CreateGameWorld(TEXT("Pt006_TC4_World"));
    if (!TestNotNull(TEXT("TC-4: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-4: Sub"), Sub))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(Pt006TestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId = Pt006TestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-4: P0"), P0))
    {
        Pt006TestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail = false;
    Sub->StartTurn(FirstId);  // 置 CurrentActivePlayerId

    // 注入落地结算 spy 计数 DecideBuyProperty
    TSharedPtr<FLandingResolverSpy> Resolver = MakeShared<FLandingResolverSpy>();
    Sub->SetLandingResolver(Resolver);

    // WHEN ①：SentToJail 落地 → 抑制全部落地结算分支
    Sub->HandlePawnLanded(EArrivalContext::SentToJail);
    TestEqual(TEXT("TC-4: SentToJail 落地，DecideBuyProperty 调用次数==0（抑制）"),
        Resolver->DecideBuyPropertyCallCount, 0);

    // WHEN ②：DiceMove 落地（对照组）→ 调 DecideBuyProperty 恰 1 次
    Sub->HandlePawnLanded(EArrivalContext::DiceMove);
    TestEqual(TEXT("TC-4: DiceMove 落无主地产，DecideBuyProperty 调用次数==1（抑制由 arrivalContext 驱动）"),
        Resolver->DecideBuyPropertyCallCount, 1);

    Pt006TestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// AC-5（TR-turn-010）— AI 内部随机走骰子3 可种子化 RNG（Advisory 软约束）
//
// 本 AC 为 BP-only 软约束（code-review 守，OQ-T-3②）；C++ 强封装静态扫描硬门
// 待 OQ-1 ADR 裁定（story AC-5 明确），本 story 不写永远 pending 的 [Logic] gate。
// 静态检查 + code-review checklist 证据落：
//   production/qa/evidence/pt-006-ai-rng-static-checklist.md
// 本 story 编排路径（ConsumeRollResult/OrchestrateMove/ResolveArrival/HandlePawnLanded）
// 不含任何随机调用——grep 权威模块无 FMath::Rand/FRand/rand 旁路。故此处无自动测试。
// =============================================================================
