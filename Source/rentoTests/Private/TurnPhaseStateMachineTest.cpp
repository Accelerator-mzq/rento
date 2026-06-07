// TurnPhaseStateMachineTest.cpp
// =============================================================================
// 单元测试：回合阶段状态机 ETurnPhase（pt-002 AC-1~11 全覆盖）
//
// 物理路径：Source/rentoTests/Private/TurnPhaseStateMachineTest.cpp
// 逻辑路径：tests/unit/player-turn/turn_phase_state_machine_test.cpp
// Automation 类目：Rento.PlayerTurn.TurnPhaseStateMachine
//
// 测试机制（headless -nullrhi）：
//   - UWorld::CreateWorld(EWorldType::Game) 创建 Game World
//   - 引擎自动 Initialize UPlayerTurnSubsystem（World Subsystem 自动发现）
//   - AC-1 阶段序列：逐步调用推进方法后断言 GetCurrentPhase()；
//     同步状态机每次 SetPhase 立即更新 CurrentPhase，等价于 spy 序列。
//     （避免 namespace 内 UCLASS/UHT 限制——AddDynamic spy 需文件作用域 UCLASS，
//      同步断言达到相同非 vacuous 效果）
//   - F-3 注入 seam：ProcessRollResult 接受 bool bIsDouble，无需真实骰子
//   - F-4 注入 seam：ResolveInitialTurnOrderWithTiebreak 接受 InjectRolls
//   - AddExpectedError 吞非法转移/边界 Error（AC-2 / AC-10 P 边界）
//
// 断言非 vacuous 保证（每条 flip 均可致真 FAIL）：
//   - AC-1：逐步阶段 TestEqual；若推进顺序错误，对应 TestEqual 真 FAIL
//   - AC-2：EndTurn 在 RollPhase 被拒，TestEqual RollPhase；若跳转则 FAIL
//   - AC-7：ShouldGoToJail(3,3)→true；flip (3,4)→false → TestTrue FAIL
//   - AC-11：rank(seat0)=0；若子组内乱序则 TestEqual FAIL
//
// 规范依据：
//   - story pt-002 AC-1~11，TC-1~11，Edge
//   - ADR-0001（禁 Latent，枚举字段 + delegate 推进）
//   - ADR-0007（状态机 C++ 落地，-nullrhi 可测）
//   - control-manifest Foundation Layer
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"
#include "TurnPhaseSpy.h"  // AC-1 OnPhaseChanged 广播序列捕获 spy

// =============================================================================
// 测试辅助（纯 C++ 函数，不涉及 UHT/UCLASS，headless 安全）
// =============================================================================
namespace TurnPhaseTestHelpers
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

    /**
     * 构建含 N 个玩家的合法 FGameSetupConfig。
     * DoublesJailThreshold / MaxTiebreakRounds 可覆盖。
     * TiebreakRounds 默认 1：第一轮即为最后轮，快速触发席位裁定。
     */
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
            EPlayerColor::Red,    // ordinal 1
            EPlayerColor::Blue,   // ordinal 2
            EPlayerColor::Green,  // ordinal 3
            EPlayerColor::Yellow, // ordinal 4
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
     * 在 PlayerStates 中找 TurnOrderIndex==0 的玩家 PlayerId。
     * 返回 -1 表示找不到（测试 fixture 异常）。
     */
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

    /**
     * 在 PlayerStates 中找 TurnOrderIndex==Order 的玩家 PlayerId。
     */
    static int32 FindPlayerByOrder(const UPlayerTurnSubsystem* Sub, int32 Order)
    {
        for (const TObjectPtr<URentoPlayerState>& State : Sub->GetPlayerStates())
        {
            if (State && State->TurnOrderIndex == Order)
            {
                return State->PlayerId;
            }
        }
        return -1;
    }

} // namespace TurnPhaseTestHelpers

// =============================================================================
// TC-1（AC-1）— 正常回合 OnPhaseChanged 精确 6 阶段序列
//
// GIVEN：2 人正常对局（无双点、不在狱）
// WHEN：逐步推进一个完整正常回合
// THEN：每步后 GetCurrentPhase() 精确按序：
//       TurnStart → RollPhase → MovePhase → ResolvePhase → PostRollAction → TurnEnd
//       无跳跃/缺失（AC-1 / GDD (b) L224）
//
// 非 vacuous：若任何推进方法越过一个阶段（如直接 TurnStart→MovePhase），
//   对应 TestEqual 真 FAIL（中间态被断言）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC1_NormalTurnPhaseSequence,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC1_NormalTurnPhaseSequence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC1_NormalTurnPhaseSequence::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC1_World"));
    if (!TestNotNull(TEXT("TC-1: World 应能创建"), World))
    {
        return false;
    }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-1: Subsystem 应能取得"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 初始化 P=2，阈值 3（默认）
    const bool bInit = Sub->InitializeFromConfig(
        TurnPhaseTestHelpers::MakeConfig(2, /*Threshold=*/3));
    if (!TestTrue(TEXT("TC-1: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 找先手玩家，确保不在狱
    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-1: 应找到先手玩家"), FirstId, -1))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (P0) { P0->bIsInJail = false; P0->ConsecutiveDoubles = 0; }

    // ==== 绑定 spy 捕获 OnPhaseChanged 广播序列（AC-1 验证 delegate 真 fire）====
    // AC-1 是对 OnPhaseChanged **广播**的契约；轮询 GetCurrentPhase() 无法捕获 TurnEnd
    // 这类瞬态广播（EndTurn 内 SetPhase(TurnEnd) 紧接 StartTurn 覆写 CurrentPhase）。
    // spy 记录每次广播，使「TurnEnd 广播缺失」可被检出（漏发则 Recorded[5]!=TurnEnd → FAIL）。
    UTurnPhaseSpy* Spy = NewObject<UTurnPhaseSpy>(GetTransientPackage());
    Spy->AddToRoot();  // 防 GC（测试同步执行，稳妥起见）
    Sub->OnPhaseChanged.AddDynamic(Spy, &UTurnPhaseSpy::HandlePhaseChanged);

    // ==== 逐步推进并断言每个中间态（GetCurrentPhase 轮询验字段转换）====

    // 步骤 1：StartTurn → TurnStart →（非在狱）RollPhase
    Sub->StartTurn(FirstId);
    TestEqual(TEXT("TC-1 [1]: StartTurn 后应为 RollPhase（TurnStart 路由完成）"),
        Sub->GetCurrentPhase(), ETurnPhase::RollPhase);

    // 步骤 2：ProcessRollResult(非双点) → MovePhase
    bool bSentToJail = false;
    Sub->ProcessRollResult(/*bIsDouble=*/false, bSentToJail);
    TestEqual(TEXT("TC-1 [2]: ProcessRollResult(非双点) 后应为 MovePhase"),
        Sub->GetCurrentPhase(), ETurnPhase::MovePhase);
    TestFalse(TEXT("TC-1 [2]: bSentToJail 应为 false"), bSentToJail);

    // 步骤 3：AdvanceFromMovePhase → ResolvePhase
    Sub->AdvanceFromMovePhase();
    TestEqual(TEXT("TC-1 [3]: AdvanceFromMovePhase 后应为 ResolvePhase"),
        Sub->GetCurrentPhase(), ETurnPhase::ResolvePhase);

    // 步骤 4：AdvanceFromResolvePhase → PostRollAction
    Sub->AdvanceFromResolvePhase();
    TestEqual(TEXT("TC-1 [4]: AdvanceFromResolvePhase 后应为 PostRollAction"),
        Sub->GetCurrentPhase(), ETurnPhase::PostRollAction);

    // 步骤 5：EndTurn → 广播 TurnEnd → 无额外回合 → 移交下一玩家（TurnStart→RollPhase）
    Sub->EndTurn(/*bSentToJailThisTurn=*/false);

    // ==== AC-1 核心断言：spy 捕获的前 6 个广播 == 第一玩家完整阶段序列 ====
    // 期望 Recorded 前 6 = [TurnStart, RollPhase, MovePhase, ResolvePhase, PostRollAction, TurnEnd]
    // （EndTurn 后续移交又广播下一玩家 TurnStart/RollPhase，故 Recorded 共 8 个，只验前 6）
    Sub->OnPhaseChanged.RemoveDynamic(Spy, &UTurnPhaseSpy::HandlePhaseChanged);

    static const ETurnPhase ExpectedSeq[] = {
        ETurnPhase::TurnStart,
        ETurnPhase::RollPhase,
        ETurnPhase::MovePhase,
        ETurnPhase::ResolvePhase,
        ETurnPhase::PostRollAction,
        ETurnPhase::TurnEnd,
    };
    constexpr int32 ExpectedNum = 6;

    if (TestTrue(
            FString::Printf(TEXT("TC-1: OnPhaseChanged 至少广播 6 次（实际 %d）"), Spy->Recorded.Num()),
            Spy->Recorded.Num() >= ExpectedNum))
    {
        // 逐个断言精确顺序（含 TurnEnd 瞬态广播——漏发 SetPhase(TurnEnd) 则此处 FAIL）
        for (int32 i = 0; i < ExpectedNum; ++i)
        {
            TestEqual(
                FString::Printf(TEXT("TC-1: OnPhaseChanged[%d] 应为期望阶段 %d（实际 %d）"),
                    i, static_cast<int32>(ExpectedSeq[i]), static_cast<int32>(Spy->Recorded[i])),
                Spy->Recorded[i], ExpectedSeq[i]);
        }
    }

    Spy->RemoveFromRoot();
    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-2（AC-2）— 非法转移拒绝：RollPhase 阶段调 EndTurn
//
// GIVEN：状态机处于 RollPhase
// WHEN：调 EndTurn（EndTurn 仅合法于 PostRollAction）
// THEN：状态保持 RollPhase（非法转移被拒，记 assertable Error），AC-2
//
// 非 vacuous：若 EndTurn 成功跳转，GetCurrentPhase() != RollPhase → TestEqual FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC2_IllegalTransitionRejected,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC2_IllegalTransitionRejected",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC2_IllegalTransitionRejected::RunTest(const FString& Parameters)
{
    // 吞 EndTurn 非法转移记录的 Error（ASCII 安全子串）
    AddExpectedError(TEXT("EndTurn"), EAutomationExpectedMessageFlags::Contains, 0);

    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC2_World"));
    if (!TestNotNull(TEXT("TC-2: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-2: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-2: InitializeFromConfig"), bInit))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (P0) P0->bIsInJail = false;

    Sub->StartTurn(FirstId);
    // 确认处于 RollPhase
    if (!TestEqual(TEXT("TC-2: StartTurn 后应为 RollPhase"),
            Sub->GetCurrentPhase(), ETurnPhase::RollPhase))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // WHEN：在 RollPhase 发 EndTurn（非法，仅 PostRollAction 合法）
    Sub->EndTurn(false);

    // THEN：阶段保持 RollPhase（非 vacuous：若跳转到 TurnEnd/TurnStart，TestEqual FAIL）
    TestEqual(TEXT("TC-2: RollPhase 下 EndTurn 应被拒绝，阶段保持 RollPhase"),
        Sub->GetCurrentPhase(), ETurnPhase::RollPhase);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-3（AC-3）— 在狱玩家 TurnStart → JailTurn 分支（非 RollPhase）
//
// GIVEN：当前玩家 bIsInJail=true
// WHEN：StartTurn
// THEN：路由到 JailTurn（非 RollPhase），AC-3
//
// 非 vacuous：若路由到 RollPhase，TestEqual JailTurn FAIL；TestNotEqual RollPhase FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC3_JailedPlayerRoutesToJailTurn,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC3_JailedPlayerRoutesToJailTurn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC3_JailedPlayerRoutesToJailTurn::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC3_World"));
    if (!TestNotNull(TEXT("TC-3: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-3: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2));

    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-3: 应找到玩家"), P0))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 设置在狱状态
    P0->bIsInJail = true;

    // WHEN：StartTurn
    Sub->StartTurn(FirstId);

    // THEN：路由到 JailTurn
    TestEqual(TEXT("TC-3: 在狱玩家应路由到 JailTurn"),
        Sub->GetCurrentPhase(), ETurnPhase::JailTurn);
    TestNotEqual(TEXT("TC-3: 在狱玩家不应路由到 RollPhase"),
        Sub->GetCurrentPhase(), ETurnPhase::RollPhase);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4（AC-4）— 双点未入狱未破产 → TurnEnd 后同玩家回 RollPhase，ConsecutiveDoubles +1
//
// GIVEN：正常玩家，ProcessRollResult(bIsDouble=true)，阈值=3（不触发 F-3）
// WHEN：EndTurn
// THEN：同玩家继续（ActivePlayerId 不变），CurrentPhase=RollPhase，ConsecutiveDoubles=1
//
// 非 vacuous：若移交下一玩家，GetCurrentActivePlayerId() 变化 → TestEqual FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC4_DoubleGrantsExtraTurn,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC4_DoubleGrantsExtraTurn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC4_DoubleGrantsExtraTurn::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC4_World"));
    if (!TestNotNull(TEXT("TC-4: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-4: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 阈值=3（默认），P=2
    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-4: 应找到玩家"), P0))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail   = false;
    P0->bIsBankrupt = false;
    P0->ConsecutiveDoubles = 0;

    Sub->StartTurn(FirstId);
    TestEqual(TEXT("TC-4 pre: 应处于 RollPhase"),
        Sub->GetCurrentPhase(), ETurnPhase::RollPhase);

    // WHEN：ProcessRollResult(双点，ConsecutiveDoubles 0→1，未到阈值 3)
    bool bSentToJail = false;
    Sub->ProcessRollResult(/*bIsDouble=*/true, bSentToJail);

    // 先断言 F-3 未触发（ConsecutiveDoubles=1 < 3）
    TestFalse(TEXT("TC-4: 第 1 次双点不触发 F-3，bSentToJail=false"), bSentToJail);
    TestEqual(TEXT("TC-4: ProcessRollResult 后 ConsecutiveDoubles 应=1"),
        P0->ConsecutiveDoubles, 1);
    // 应推进到 MovePhase（正常双点，未入狱）
    TestEqual(TEXT("TC-4: 双点未触发 F-3，应到 MovePhase"),
        Sub->GetCurrentPhase(), ETurnPhase::MovePhase);

    // 推进到 PostRollAction
    Sub->AdvanceFromMovePhase();
    Sub->AdvanceFromResolvePhase();

    // 记录当前 ActivePlayerId，EndTurn 后应不变（额外回合）
    const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

    Sub->EndTurn(/*bSentToJailThisTurn=*/false);

    // THEN：同玩家继续，回 RollPhase
    TestEqual(TEXT("TC-4: 双点额外回合，ActivePlayerId 应不变"),
        Sub->GetCurrentActivePlayerId(), ActiveBefore);
    TestEqual(TEXT("TC-4: 双点额外回合后应回 RollPhase"),
        Sub->GetCurrentPhase(), ETurnPhase::RollPhase);
    // ConsecutiveDoubles 保持 1（不归零，AC-6）
    TestEqual(TEXT("TC-4: 额外回合时 ConsecutiveDoubles 保持 1（不归零，AC-6）"),
        P0->ConsecutiveDoubles, 1);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-5（AC-5）— ConsecutiveDoubles 累至阈值 3 → F-3：送监狱+取消移动+归零+无额外回合
//
// GIVEN：ConsecutiveDoubles=2（前 2 次），阈值=3
// WHEN：ProcessRollResult(bIsDouble=true)（第 3 次双点）
// THEN：OutSentToJail=true；ConsecutiveDoubles 归零；阶段到 PostRollAction；
//       EndTurn 后移交下一玩家（无额外回合）
//
// 非 vacuous：若归零未执行（ConsecutiveDoubles=3），TestEqual 0 FAIL
//             若发额外回合（ActivePlayerId 不变），TestNotEqual FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC5_ThreeDoublesTriggersJail,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC5_ThreeDoublesTriggersJail",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC5_ThreeDoublesTriggersJail::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC5_World"));
    if (!TestNotNull(TEXT("TC-5: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-5: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-5: P0"), P0))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail   = false;
    P0->bIsBankrupt = false;
    // 模拟前 2 次双点（ConsecutiveDoubles=2）
    P0->ConsecutiveDoubles = 2;

    Sub->StartTurn(FirstId);
    const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

    // WHEN：第 3 次双点（ConsecutiveDoubles 2→3，>=3 触发 F-3）
    bool bSentToJail = false;
    Sub->ProcessRollResult(/*bIsDouble=*/true, bSentToJail);

    // THEN（F-3 触发）：
    // 1. OutSentToJail=true
    TestTrue(TEXT("TC-5: F-3 触发，bSentToJail 应为 true"), bSentToJail);
    // 2. ConsecutiveDoubles 归零
    TestEqual(TEXT("TC-5: F-3 后 ConsecutiveDoubles 应归零"),
        P0->ConsecutiveDoubles, 0);
    // 3. 阶段到 PostRollAction（取消移动，跳过 MovePhase/ResolvePhase）
    TestEqual(TEXT("TC-5: F-3 后应在 PostRollAction（取消移动）"),
        Sub->GetCurrentPhase(), ETurnPhase::PostRollAction);

    // EndTurn → 无额外回合（bSentToJailThisTurn=true 抑制额外回合）
    Sub->EndTurn(/*bSentToJailThisTurn=*/true);

    // 4. 行动权移交下一玩家（无额外回合）
    TestNotEqual(TEXT("TC-5: F-3 后应移交下一玩家（无额外回合）"),
        Sub->GetCurrentActivePlayerId(), ActiveBefore);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-6（AC-6）— ConsecutiveDoubles 归零时机
//
// Part A：第 1 次双点后 ConsecutiveDoubles==1；TurnEnd 判为额外回合时保持 1（不归零）
// Part B：额外回合内非双点结束，行动权实际移交时归零
//
// 非 vacuous：
//   Part A：若额外回合路径误归零，ConsecutiveDoubles=0 → TestEqual 1 FAIL
//   Part B：若移交路径未归零，ConsecutiveDoubles=1 → TestEqual 0 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC6_ConsecutiveDoublesZeroTiming,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC6_ConsecutiveDoublesZeroTiming",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC6_ConsecutiveDoublesZeroTiming::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC6_World"));
    if (!TestNotNull(TEXT("TC-6: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-6: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId  = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    const int32 SecondId = TurnPhaseTestHelpers::FindPlayerByOrder(Sub, 1);

    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-6: P0"), P0))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail   = false;
    P0->bIsBankrupt = false;
    P0->ConsecutiveDoubles = 0;

    Sub->StartTurn(FirstId);

    // ==== Part A：第 1 次双点 ====
    bool bSentToJail = false;
    Sub->ProcessRollResult(/*bIsDouble=*/true, bSentToJail);

    // ConsecutiveDoubles 已 +1（= 1）
    TestEqual(TEXT("TC-6 A: ProcessRollResult(double) 后 ConsecutiveDoubles=1"),
        P0->ConsecutiveDoubles, 1);

    Sub->AdvanceFromMovePhase();
    Sub->AdvanceFromResolvePhase();

    // EndTurn → 额外回合（同玩家继续）
    Sub->EndTurn(false);

    // Part A 结论：额外回合时 ConsecutiveDoubles 保持 1
    TestEqual(TEXT("TC-6 A: 额外回合时 ConsecutiveDoubles 保持 1（不归零）"),
        P0->ConsecutiveDoubles, 1);
    TestEqual(TEXT("TC-6 A: 额外回合时 ActivePlayerId 不变"),
        Sub->GetCurrentActivePlayerId(), FirstId);

    // ==== Part B：额外回合内掷非双点 ====
    Sub->ProcessRollResult(/*bIsDouble=*/false, bSentToJail);
    Sub->AdvanceFromMovePhase();
    Sub->AdvanceFromResolvePhase();
    Sub->EndTurn(false);  // 非双点 → 移交下一玩家

    // Part B 结论：移交时 P0 ConsecutiveDoubles 归零
    TestEqual(TEXT("TC-6 B: 行动权移交后 P0 ConsecutiveDoubles 归零"),
        P0->ConsecutiveDoubles, 0);
    // 行动权已移交（ActivePlayerId 变化）
    TestNotEqual(TEXT("TC-6 B: 行动权已移交"),
        Sub->GetCurrentActivePlayerId(), FirstId);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-7（AC-7）— F-3 ShouldGoToJail 纯函数五情形
//
// 五情形：
//   (3,3)→true；(2,3)→false；(4,3)→true（>= 越级防御）；
//   (2,2 阈值=2)→true；非双点→ConsecutiveDoubles=0 且 ShouldGoToJail(0,3)=false
//
// 纯函数部分无需 World，直接调 UPlayerTurnSubsystem::ShouldGoToJail（static）
// 非双点归零通过 ProcessRollResult 注入 seam 验证
//
// 非 vacuous：(3,3)→true；flip 为 ShouldGoToJail(3,4) 则返回 false → TestTrue FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC7_ShouldGoToJailPureFunction,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC7_ShouldGoToJailPureFunction",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC7_ShouldGoToJailPureFunction::RunTest(const FString& Parameters)
{
    // ---- 纯函数断言（不需 World）----

    // (3,3) → true（正常入狱，AC-7 情形1）
    TestTrue(TEXT("TC-7 [1]: ShouldGoToJail(3,3) 应为 true"),
        UPlayerTurnSubsystem::ShouldGoToJail(3, 3));

    // (2,3) → false（尚未达阈值，AC-7 情形2）
    TestFalse(TEXT("TC-7 [2]: ShouldGoToJail(2,3) 应为 false"),
        UPlayerTurnSubsystem::ShouldGoToJail(2, 3));

    // (4,3) → true（越级防御，4>=3，AC-7 情形3）
    TestTrue(TEXT("TC-7 [3]: ShouldGoToJail(4,3) 应为 true（>= 越级防御）"),
        UPlayerTurnSubsystem::ShouldGoToJail(4, 3));

    // (2,2 阈值=2) → true（house rule，AC-7 情形4）
    TestTrue(TEXT("TC-7 [4]: ShouldGoToJail(2,2) 应为 true（house rule 阈值=2）"),
        UPlayerTurnSubsystem::ShouldGoToJail(2, 2));

    // ---- 非双点归零验证（需 World，AC-7 情形5）----
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC7_World"));
    if (!TestNotNull(TEXT("TC-7: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-7: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2, 3));

    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-7: P0"), P0))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }
    P0->bIsInJail  = false;
    // 置 ConsecutiveDoubles=2，验证非双点时归零
    P0->ConsecutiveDoubles = 2;

    Sub->StartTurn(FirstId);

    // 非双点掷骰
    bool bSentToJail = false;
    Sub->ProcessRollResult(/*bIsDouble=*/false, bSentToJail);

    // 非双点 → ConsecutiveDoubles 归零（AC-7 情形5）
    TestEqual(TEXT("TC-7 [5]: 非双点 → ConsecutiveDoubles 归零"),
        P0->ConsecutiveDoubles, 0);
    TestFalse(TEXT("TC-7 [5]: 非双点 → bSentToJail=false"), bSentToJail);
    // ShouldGoToJail(0, 3) = false（确认归零后的纯函数结果）
    TestFalse(TEXT("TC-7 [5]: ShouldGoToJail(0,3) 应为 false"),
        UPlayerTurnSubsystem::ShouldGoToJail(0, 3));

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-8（AC-8）— 在狱玩家仍获回合（JailTurn），JailTurnsServed 留狱 +1 / 出狱不增
//
// GIVEN：下一玩家（P1）bIsInJail=true，非破产
// WHEN：P0 回合结束，移交 P1
// THEN：P1 路由 JailTurn（仍获回合）
//       JailTurn 进入时 JailTurnsServed 未变
//       留狱后 +1；出狱不增（AC-8）
//
// 非 vacuous：若路由到 RollPhase，TestEqual JailTurn FAIL
//             若留狱未增（JailTurnsServed=0），TestEqual 1 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC8_JailTurnServedCount,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC8_JailTurnServedCount",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC8_JailTurnServedCount::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC8_World"));
    if (!TestNotNull(TEXT("TC-8: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-8: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2, 3));

    const int32 FirstId  = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    const int32 SecondId = TurnPhaseTestHelpers::FindPlayerByOrder(Sub, 1);

    // P1（SecondId）设为在狱
    URentoPlayerState* P1 = Sub->FindPlayerById(SecondId);
    if (!TestNotNull(TEXT("TC-8: P1"), P1))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }
    P1->bIsInJail       = true;
    P1->JailTurnsServed = 0;
    P1->bIsBankrupt     = false;

    // P0 正常回合结束，移交 P1
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (P0) { P0->bIsInJail = false; P0->bIsBankrupt = false; }

    Sub->StartTurn(FirstId);
    bool bSent = false;
    Sub->ProcessRollResult(false, bSent);
    Sub->AdvanceFromMovePhase();
    Sub->AdvanceFromResolvePhase();
    Sub->EndTurn(false);

    // 行动权应已移交 P1
    TestEqual(TEXT("TC-8: 应已移交 P1（SecondId）"),
        Sub->GetCurrentActivePlayerId(), SecondId);

    // P1 路由到 JailTurn（仍获回合，AC-8）
    TestEqual(TEXT("TC-8: 在狱玩家仍获回合，路由 JailTurn"),
        Sub->GetCurrentPhase(), ETurnPhase::JailTurn);

    // JailTurn 进入时 JailTurnsServed 尚未变（=0）
    TestEqual(TEXT("TC-8: JailTurn 进入时 JailTurnsServed 未变（=0）"),
        P1->JailTurnsServed, 0);

    // ==== Part B：留狱 → JailTurnsServed +1 ====
    Sub->AdvanceFromJailTurn(/*bRemainsInJail=*/true);
    TestEqual(TEXT("TC-8 B: 留狱后 JailTurnsServed 应 +1（=1）"),
        P1->JailTurnsServed, 1);

    // ==== Part C：下一回合 P1 出狱 ====
    // 重新 StartTurn P1（模拟下次在狱回合）
    P1->JailTurnsServed = 1;
    Sub->StartTurn(SecondId);
    TestEqual(TEXT("TC-8 C: JailTurn 进入时 JailTurnsServed 仍=1（未变）"),
        P1->JailTurnsServed, 1);

    // 出狱：JailTurnsServed 不增加
    Sub->AdvanceFromJailTurn(/*bRemainsInJail=*/false);
    TestEqual(TEXT("TC-8 C: 出狱后 JailTurnsServed 不增加（仍=1）"),
        P1->JailTurnsServed, 1);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-9（AC-9）— 双点但本回合入狱 → 无额外回合；双点出狱 → 不累加、无额外回合
//
// Part A：F-3 三连双点入狱 → 无额外回合（EndTurn 移交下一玩家）
// Part B：在狱玩家 JailTurn 出狱 → ConsecutiveDoubles 不累加（不经 ProcessRollResult）
//
// 非 vacuous：
//   Part A：若发额外回合，ActivePlayerId 不变 → TestNotEqual FAIL
//   Part B：若出狱路径错误触发计数，ConsecutiveDoubles!=0 → TestEqual 0 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC9_DoubleSentToJailNoExtraTurn,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC9_DoubleSentToJailNoExtraTurn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC9_DoubleSentToJailNoExtraTurn::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC9_World"));
    if (!TestNotNull(TEXT("TC-9: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-9: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2, /*Threshold=*/3));

    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (!TestNotNull(TEXT("TC-9: P0"), P0))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // ==== Part A：F-3 三连双点 → 无额外回合 ====
    P0->bIsInJail          = false;
    P0->bIsBankrupt        = false;
    P0->ConsecutiveDoubles = 2;  // 模拟前 2 次

    Sub->StartTurn(FirstId);
    const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

    bool bSent = false;
    Sub->ProcessRollResult(/*bIsDouble=*/true, bSent);  // 第 3 次 → F-3

    TestTrue(TEXT("TC-9 A: bSentToJail=true（F-3 触发）"), bSent);
    TestEqual(TEXT("TC-9 A: F-3 后在 PostRollAction"),
        Sub->GetCurrentPhase(), ETurnPhase::PostRollAction);

    Sub->EndTurn(/*bSentToJailThisTurn=*/true);

    // 无额外回合：行动权移交
    TestNotEqual(TEXT("TC-9 A: 入狱后应移交下一玩家（无额外回合）"),
        Sub->GetCurrentActivePlayerId(), ActiveBefore);

    // ==== Part B：在狱玩家出狱，ConsecutiveDoubles 不累加 ====
    // 直接 StartTurn P0（模拟 P0 在下一局又处于在狱状态）
    P0->bIsInJail          = true;
    P0->ConsecutiveDoubles = 0;

    Sub->StartTurn(FirstId);
    TestEqual(TEXT("TC-9 B: 在狱玩家路由 JailTurn"),
        Sub->GetCurrentPhase(), ETurnPhase::JailTurn);

    // 出狱（JailTurn → TurnEnd，不走 ProcessRollResult，ConsecutiveDoubles 不改）
    Sub->AdvanceFromJailTurn(/*bRemainsInJail=*/false);

    // ConsecutiveDoubles 不因出狱路径累加（AC-9）
    TestEqual(TEXT("TC-9 B: 出狱路径 ConsecutiveDoubles 不累加（=0）"),
        P0->ConsecutiveDoubles, 0);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-10（AC-10）— P<2 / P>4 拒绝开局（防守层不静默接受越界 P）
//
// GIVEN：P=1（不足）/ P=5（超 MVP 上限 4）
// WHEN：InitializeFromConfig
// THEN：返回 false（开局拒绝，AC-10）
//
// 非 vacuous：若 P=1 未被拒绝，TestFalse 真 FAIL；P=5 同理
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC10_PBoundaryRejection,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC10_PBoundaryRejection",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC10_PBoundaryRejection::RunTest(const FString& Parameters)
{
    // 吞预期 Error（P 边界拒绝记录 Error，ASCII 子串匹配）
    // P=1 Error 含 "P=1"；P=5 Error 含 "P=5"（与 .cpp 中 UE_LOG 格式一致）
    AddExpectedError(TEXT("P=1"), EAutomationExpectedMessageFlags::Contains, 1);
    AddExpectedError(TEXT("P=5"), EAutomationExpectedMessageFlags::Contains, 1);

    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC10_World"));
    if (!TestNotNull(TEXT("TC-10: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-10: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // ---- P=1（不足 2 人）----
    FGameSetupConfig Config1;
    Config1.DoublesJailThreshold = 3;
    Config1.MaxTiebreakRounds    = 1;
    Config1.RandomSeed           = 0;
    {
        FPlayerSetupEntry E;
        E.DisplayName  = FText::FromString(TEXT("Solo"));
        E.TokenColor   = EPlayerColor::Red;
        E.bIsAI        = false;
        Config1.Players.Add(E);
    }
    const bool bP1 = Sub->InitializeFromConfig(Config1);
    TestFalse(TEXT("TC-10: P=1 应被拒绝（AC-10）"), bP1);

    // ---- P=5（超 MVP 上限）----
    FGameSetupConfig Config5 = TurnPhaseTestHelpers::MakeConfig(4);  // 基础 4 人
    FPlayerSetupEntry Extra;
    Extra.DisplayName  = FText::FromString(TEXT("P4_extra"));
    Extra.TokenColor   = EPlayerColor::Purple;   // 第 5 人
    Extra.bIsAI        = false;
    Config5.Players.Add(Extra);   // 共 5 人

    const bool bP5 = Sub->InitializeFromConfig(Config5);
    TestFalse(TEXT("TC-10: P=5 应被拒绝（AC-10）"), bP5);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-11（AC-11）— F-4 平手裁定子组内席位升序
//
// Case A：P=4 rolls=[9,5,9,5]，MaxTiebreakRounds=1
//   G1={seat0(9),seat2(9)} 争 rank0-1 → rank(0)=0, rank(2)=1
//   G2={seat1(5),seat3(5)} 争 rank2-3 → rank(1)=2, rank(3)=3
//
// Case B（Edge）：P=4 rolls=[9,9,9,9]，单一 TieGroup 达上限 → 席位升序 rank{0,1,2,3}
//
// 非 vacuous：
//   Case A：若 G1 内 seat0/seat2 rank 互换（rank(0)=1），TestEqual 0 FAIL
//   Case B：若 rank(0)!=0，TestEqual FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_TC11_TiebreakSeatOrderTieGroup,
    "Rento.PlayerTurn.TurnPhaseStateMachine.TC11_TiebreakSeatOrderTieGroup",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_TC11_TiebreakSeatOrderTieGroup::RunTest(const FString& Parameters)
{
    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("TC11_World"));
    if (!TestNotNull(TEXT("TC-11: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-11: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // ============================================================
    // Case A：rolls=[9,5,9,5]，MaxTiebreakRounds=1
    // ============================================================
    {
        FGameSetupConfig Config = TurnPhaseTestHelpers::MakeConfig(4, 3, /*TiebreakRounds=*/1);
        const bool bInit = Sub->InitializeFromConfig(Config);
        if (!TestTrue(TEXT("TC-11 A: InitializeFromConfig(P=4)"), bInit))
        {
            TurnPhaseTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // 手动重置 TurnOrderIndex，再直接注入定序
        const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
        if (!TestEqual(TEXT("TC-11 A: PlayerStates.Num() 应=4"), States.Num(), 4))
        {
            TurnPhaseTestHelpers::DestroyGameWorld(World);
            return false;
        }
        for (const TObjectPtr<URentoPlayerState>& S : States)
        {
            if (S) S->TurnOrderIndex = -1;
        }

        // 注入 rolls=[9,5,9,5]（seat0=9, seat1=5, seat2=9, seat3=5）
        // MaxTiebreakRounds=1：第一轮即为最后轮，直接席位裁定
        TArray<int32> Rolls = {9, 5, 9, 5};
        Sub->ResolveInitialTurnOrderWithTiebreak(/*DiceService=*/nullptr, Rolls);

        // 唯一性校验（完整排列）
        TestTrue(TEXT("TC-11 A: 唯一性校验"), Sub->ValidateTurnOrderUniqueness());

        // 找各 seat 的 TurnOrderIndex（PlayerId 与 seat 下标一致：InitializeFromConfig 顺序分配）
        // seat = PlayerStates 数组下标；PlayerId = seat（0..3）
        const int32 RankSeat0 = States[0] ? States[0]->TurnOrderIndex : -99;
        const int32 RankSeat1 = States[1] ? States[1]->TurnOrderIndex : -99;
        const int32 RankSeat2 = States[2] ? States[2]->TurnOrderIndex : -99;
        const int32 RankSeat3 = States[3] ? States[3]->TurnOrderIndex : -99;

        // G1={seat0,seat2}（roll=9），子组内席位升序 → rank(seat0) < rank(seat2)
        // G2={seat1,seat3}（roll=5），子组内席位升序 → rank(seat1) < rank(seat3)
        // G1 rank 段 < G2 rank 段（高点先手）
        TestEqual(TEXT("TC-11 A: seat0 rank=0（G1 席位最小）"), RankSeat0, 0);
        TestEqual(TEXT("TC-11 A: seat2 rank=1（G1 席位次小）"), RankSeat2, 1);
        TestEqual(TEXT("TC-11 A: seat1 rank=2（G2 席位最小）"), RankSeat1, 2);
        TestEqual(TEXT("TC-11 A: seat3 rank=3（G2 席位次小）"), RankSeat3, 3);

        // 子组内不相交（G1 rank 段 0-1 均小于 G2 rank 段 2-3）
        TestTrue(TEXT("TC-11 A: G1 rank 段 < G2 rank 段"),
            RankSeat0 < RankSeat1 && RankSeat2 < RankSeat3);
    }

    // ============================================================
    // Case B（Edge）：rolls=[9,9,9,9]，单一 TieGroup，席位升序
    // ============================================================
    {
        FGameSetupConfig Config = TurnPhaseTestHelpers::MakeConfig(4, 3, /*TiebreakRounds=*/1);
        const bool bInit = Sub->InitializeFromConfig(Config);
        if (!TestTrue(TEXT("TC-11 B: InitializeFromConfig(P=4)"), bInit))
        {
            TurnPhaseTestHelpers::DestroyGameWorld(World);
            return false;
        }

        const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
        for (const TObjectPtr<URentoPlayerState>& S : States)
        {
            if (S) S->TurnOrderIndex = -1;
        }

        TArray<int32> AllTie = {9, 9, 9, 9};
        Sub->ResolveInitialTurnOrderWithTiebreak(nullptr, AllTie);

        TestTrue(TEXT("TC-11 B: 唯一性校验"), Sub->ValidateTurnOrderUniqueness());

        const int32 R0 = States[0] ? States[0]->TurnOrderIndex : -99;
        const int32 R1 = States[1] ? States[1]->TurnOrderIndex : -99;
        const int32 R2 = States[2] ? States[2]->TurnOrderIndex : -99;
        const int32 R3 = States[3] ? States[3]->TurnOrderIndex : -99;

        // 单一 TieGroup，席位升序 rank{0,1,2,3}
        TestEqual(TEXT("TC-11 B: seat0 rank=0（全平席位升序）"), R0, 0);
        TestEqual(TEXT("TC-11 B: seat1 rank=1"), R1, 1);
        TestEqual(TEXT("TC-11 B: seat2 rank=2"), R2, 2);
        TestEqual(TEXT("TC-11 B: seat3 rank=3"), R3, 3);
    }

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// Edge — 非法信号在每个阶段均被拒绝（结构不可变，AC-2 扩展验证）
//
// 验证：
//   - RollPhase 下调 AdvanceFromMovePhase → 拒绝，保持 RollPhase
//   - RollPhase 下调 AdvanceFromResolvePhase → 拒绝，保持 RollPhase
//   - MovePhase 下调 ProcessRollResult → 拒绝，保持 MovePhase
//
// 非 vacuous：若拒绝失效（跳转成功），相应 TestEqual 真 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnPhase_Edge_IllegalSignalEveryPhase,
    "Rento.PlayerTurn.TurnPhaseStateMachine.Edge_IllegalSignalEveryPhase",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnPhase_Edge_IllegalSignalEveryPhase::RunTest(const FString& Parameters)
{
    // 吞各非法转移 Error（ASCII 子串，occurrences=0 表示 >=0 次均可）
    AddExpectedError(TEXT("AdvanceFromMovePhase"),    EAutomationExpectedMessageFlags::Contains, 0);
    AddExpectedError(TEXT("AdvanceFromResolvePhase"), EAutomationExpectedMessageFlags::Contains, 0);
    AddExpectedError(TEXT("ProcessRollResult"),       EAutomationExpectedMessageFlags::Contains, 0);

    UWorld* World = TurnPhaseTestHelpers::CreateGameWorld(TEXT("Edge_World"));
    if (!TestNotNull(TEXT("Edge: World"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("Edge: Sub"), Sub))
    {
        TurnPhaseTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(TurnPhaseTestHelpers::MakeConfig(2, 3));

    const int32 FirstId = TurnPhaseTestHelpers::FindFirstPlayerId(Sub);
    URentoPlayerState* P0 = Sub->FindPlayerById(FirstId);
    if (P0) P0->bIsInJail = false;

    Sub->StartTurn(FirstId);
    // 此时处于 RollPhase

    // ---- RollPhase 下发 AdvanceFromMovePhase（非法）----
    Sub->AdvanceFromMovePhase();
    TestEqual(TEXT("Edge [1]: RollPhase 下 AdvanceFromMovePhase 被拒，保持 RollPhase"),
        Sub->GetCurrentPhase(), ETurnPhase::RollPhase);

    // ---- RollPhase 下发 AdvanceFromResolvePhase（非法）----
    Sub->AdvanceFromResolvePhase();
    TestEqual(TEXT("Edge [2]: RollPhase 下 AdvanceFromResolvePhase 被拒，保持 RollPhase"),
        Sub->GetCurrentPhase(), ETurnPhase::RollPhase);

    // ---- 正常推进到 MovePhase ----
    bool bSent = false;
    Sub->ProcessRollResult(false, bSent);
    TestEqual(TEXT("Edge pre [3]: 应到 MovePhase"),
        Sub->GetCurrentPhase(), ETurnPhase::MovePhase);

    // ---- MovePhase 下发 ProcessRollResult（非法）----
    Sub->ProcessRollResult(false, bSent);
    TestEqual(TEXT("Edge [3]: MovePhase 下 ProcessRollResult 被拒，保持 MovePhase"),
        Sub->GetCurrentPhase(), ETurnPhase::MovePhase);

    TurnPhaseTestHelpers::DestroyGameWorld(World);
    return true;
}
