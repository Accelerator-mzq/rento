// TurnEventPayloadsTest.cpp
// =============================================================================
// 集成/逻辑测试：story-004 6 回合事件 USTRUCT payload + AI 可观察 hook + 建房通告 beat
//
// 物理路径：Source/rentoTests/Private/TurnEventPayloadsTest.cpp
// Automation 类目：Rento.PlayerTurn.TurnEventPayloads
//
// 覆盖 AC（story-004）：
//   TC1 (AC-1) — 6 事件均 DYNAMIC_MULTICAST + BlueprintAssignable（可 AddDynamic + IsBound）
//   TC2 (AC-2) — payload USTRUCT 字段 round-trip + OrderedPlayerIds 包入 struct
//   TC3 (AC-3) — OnPhaseChanged.ConsecutiveDoubles 0/1/2 三变体（双点链）
//   TC4 (AC-4) — OnTurnEnded.bGrantsExtraTurn true（双点）/ false（非双点）
//   TC5 (AC-5) — InitializeFromConfig 广播 OnTurnOrderResolved + payload
//   TC6 (AC-6) — OnAIActionExecuted 每执行动作广播 ActionIndex 0..M-1，跳过不广播
//   TC7 (AC-7) — HandleBuildingChanged 仅 ResolvePhase/PostRollAction 广播
//
// 机制（headless -nullrhi）：
//   - 独立命名空间 TurnEventTestHelpers 避免 ODR
//   - UTurnEventSpy（UCLASS）捕获 5 新事件 + OnPhaseChanged + OnGameWon
//   - 复用 FAIDecisionMakerSpy / FActionValidatorSpy（簇A/B）测 OnAIActionExecuted
//
// ⚠ 已知坑：
//   G1 EAutomationTestFlags_ApplicationContextMask | ProductFilter
//   G3 DYNAMIC delegate spy 用 UCLASS（UTurnEventSpy）
//   G4 例外：UObject spy 用 NewObject+AddToRoot/RemoveFromRoot 防 GC
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"
#include "GameStateSnapshot.h"
#include "AIDecisionMakerInterface.h"
#include "AIDecisionMakerSpy.h"
#include "ActionValidatorInterface.h"
#include "ActionValidatorSpy.h"
#include "TurnEventSpy.h"

// =============================================================================
// 测试辅助（独立命名空间避免 ODR）
// =============================================================================
namespace TurnEventTestHelpers
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

    static FGameSetupConfig MakeConfig(int32 N, int32 DoublesThreshold = 3, int32 TiebreakRounds = 5)
    {
        FGameSetupConfig Config;
        Config.DoublesJailThreshold = DoublesThreshold;
        Config.MaxTiebreakRounds    = TiebreakRounds;
        Config.RandomSeed           = 42;

        const EPlayerColor Colors[] = {
            EPlayerColor::Red, EPlayerColor::Blue, EPlayerColor::Green, EPlayerColor::Yellow };
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

    static int32 FindFirstPlayerId(const UPlayerTurnSubsystem* Sub)
    {
        for (const TObjectPtr<URentoPlayerState>& State : Sub->GetPlayerStates())
        {
            if (State && State->TurnOrderIndex == 0) { return State->PlayerId; }
        }
        return -1;
    }

    /** StartTurn(p)→PRR(false)→MovePhase→AdvanceFromMovePhase→ResolvePhase。 */
    static bool AdvanceToResolvePhase(UPlayerTurnSubsystem* Sub, int32 PlayerId)
    {
        URentoPlayerState* PS = Sub->FindPlayerById(PlayerId);
        if (PS) { PS->bIsInJail = false; PS->ConsecutiveDoubles = 0; }
        Sub->StartTurn(PlayerId);
        if (Sub->GetCurrentPhase() != ETurnPhase::RollPhase) { return false; }
        bool bSent = false;
        Sub->ProcessRollResult(/*bIsDouble=*/false, bSent);
        if (Sub->GetCurrentPhase() != ETurnPhase::MovePhase) { return false; }
        Sub->AdvanceFromMovePhase();
        return Sub->GetCurrentPhase() == ETurnPhase::ResolvePhase;
    }

    /** 续到 PostRollAction（在 ResolvePhase 基础上 AdvanceFromResolvePhase）。 */
    static bool AdvanceToPostRollAction(UPlayerTurnSubsystem* Sub, int32 PlayerId)
    {
        if (!AdvanceToResolvePhase(Sub, PlayerId)) { return false; }
        Sub->AdvanceFromResolvePhase();
        return Sub->GetCurrentPhase() == ETurnPhase::PostRollAction;
    }

    static UTurnEventSpy* MakeSpy()
    {
        UTurnEventSpy* Spy = NewObject<UTurnEventSpy>(GetTransientPackage());
        Spy->AddToRoot(); // UObject spy 防 GC（G4 例外）
        return Spy;
    }
    static void ReleaseSpy(UTurnEventSpy* Spy) { if (Spy) { Spy->RemoveFromRoot(); } }
}

// =============================================================================
// TC1（AC-1）— 6 事件均 DYNAMIC_MULTICAST + BlueprintAssignable（可绑定 + IsBound）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnEvents_TC1_AllSixDynamicAssignable,
    "Rento.PlayerTurn.TurnEventPayloads.TC1_AllSixDynamicAssignable",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnEvents_TC1_AllSixDynamicAssignable::RunTest(const FString& Parameters)
{
    UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC1_World"));
    if (!TestNotNull(TEXT("TC-1: World"), World)) return false;
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-1: Sub"), Sub)) { TurnEventTestHelpers::DestroyGameWorld(World); return false; }

    UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();

    // 6 事件各 AddDynamic（编译即证 DYNAMIC+BlueprintAssignable；IsBound 证绑定成功）
    Sub->OnPhaseChanged.AddDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);
    Sub->OnGameWon.AddDynamic(Spy, &UTurnEventSpy::HandleGameWon);
    Sub->OnTurnStarted.AddDynamic(Spy, &UTurnEventSpy::HandleTurnStarted);
    Sub->OnTurnEnded.AddDynamic(Spy, &UTurnEventSpy::HandleTurnEnded);
    Sub->OnTurnOrderResolved.AddDynamic(Spy, &UTurnEventSpy::HandleOrderResolved);
    Sub->OnAIActionExecuted.AddDynamic(Spy, &UTurnEventSpy::HandleAIActionExecuted);
    Sub->OnBuildingAnnounced.AddDynamic(Spy, &UTurnEventSpy::HandleBuildingAnnounced);

    TestTrue(TEXT("TC-1: OnPhaseChanged IsBound"),       Sub->OnPhaseChanged.IsBound());
    TestTrue(TEXT("TC-1: OnGameWon IsBound"),            Sub->OnGameWon.IsBound());
    TestTrue(TEXT("TC-1: OnTurnStarted IsBound"),        Sub->OnTurnStarted.IsBound());
    TestTrue(TEXT("TC-1: OnTurnEnded IsBound"),          Sub->OnTurnEnded.IsBound());
    TestTrue(TEXT("TC-1: OnTurnOrderResolved IsBound"),  Sub->OnTurnOrderResolved.IsBound());
    TestTrue(TEXT("TC-1: OnAIActionExecuted IsBound"),   Sub->OnAIActionExecuted.IsBound());
    TestTrue(TEXT("TC-1: OnBuildingAnnounced IsBound"),  Sub->OnBuildingAnnounced.IsBound());

    Sub->OnPhaseChanged.RemoveDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);
    Sub->OnGameWon.RemoveDynamic(Spy, &UTurnEventSpy::HandleGameWon);
    Sub->OnTurnStarted.RemoveDynamic(Spy, &UTurnEventSpy::HandleTurnStarted);
    Sub->OnTurnEnded.RemoveDynamic(Spy, &UTurnEventSpy::HandleTurnEnded);
    Sub->OnTurnOrderResolved.RemoveDynamic(Spy, &UTurnEventSpy::HandleOrderResolved);
    Sub->OnAIActionExecuted.RemoveDynamic(Spy, &UTurnEventSpy::HandleAIActionExecuted);
    Sub->OnBuildingAnnounced.RemoveDynamic(Spy, &UTurnEventSpy::HandleBuildingAnnounced);

    TurnEventTestHelpers::ReleaseSpy(Spy);
    TurnEventTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC2（AC-2）— payload USTRUCT 字段 round-trip + OrderedPlayerIds 包入 struct
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnEvents_TC2_PayloadStructFields,
    "Rento.PlayerTurn.TurnEventPayloads.TC2_PayloadStructFields",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnEvents_TC2_PayloadStructFields::RunTest(const FString& Parameters)
{
    // FTurnOrderResult：OrderedPlayerIds 包在 struct 内（非裸 TArray 作 delegate 参数）
    FTurnOrderResult Order;
    Order.OrderedPlayerIds = {7, 8, 9};
    Order.bResolvedBySeatTiebreak = true;
    TestEqual(TEXT("TC-2: FTurnOrderResult.OrderedPlayerIds.Num()==3"), Order.OrderedPlayerIds.Num(), 3);
    TestEqual(TEXT("TC-2: OrderedPlayerIds[1]==8"), Order.OrderedPlayerIds[1], 8);
    TestTrue(TEXT("TC-2: bResolvedBySeatTiebreak==true"), Order.bResolvedBySeatTiebreak);

    FAIActionDetails AI;
    AI.ActionIndex = 1; AI.ActingPlayerId = 2; AI.TargetTileIndex = 5; AI.Amount = 0;
    AI.ActionType = EActionType::BuildHouse;
    TestEqual(TEXT("TC-2: FAIActionDetails.ActionIndex==1"), AI.ActionIndex, 1);
    TestEqual(TEXT("TC-2: FAIActionDetails.ActionType==BuildHouse"),
        static_cast<int32>(AI.ActionType), static_cast<int32>(EActionType::BuildHouse));

    FPhaseChangedInfo PC;
    PC.OldPhase = ETurnPhase::RollPhase; PC.NewPhase = ETurnPhase::MovePhase; PC.ConsecutiveDoubles = 2;
    TestEqual(TEXT("TC-2: FPhaseChangedInfo.ConsecutiveDoubles==2"), PC.ConsecutiveDoubles, 2);
    TestEqual(TEXT("TC-2: FPhaseChangedInfo.NewPhase==MovePhase"),
        static_cast<int32>(PC.NewPhase), static_cast<int32>(ETurnPhase::MovePhase));

    FTurnStartedInfo TS; TS.PlayerId = 3; TS.bIsAI = true;
    TestEqual(TEXT("TC-2: FTurnStartedInfo.PlayerId==3"), TS.PlayerId, 3);
    TestTrue(TEXT("TC-2: FTurnStartedInfo.bIsAI==true"), TS.bIsAI);

    FTurnEndedInfo TE; TE.PlayerId = 4; TE.bGrantsExtraTurn = true;
    TestTrue(TEXT("TC-2: FTurnEndedInfo.bGrantsExtraTurn==true"), TE.bGrantsExtraTurn);

    FBuildingAnnouncedInfo BA; BA.TileIndex = 15; BA.NewHouseCount = 2; BA.ActingPlayerId = 1;
    TestEqual(TEXT("TC-2: FBuildingAnnouncedInfo.NewHouseCount==2"), BA.NewHouseCount, 2);
    return true;
}

// =============================================================================
// TC3（AC-3）— OnPhaseChanged.ConsecutiveDoubles 0/1/2（双点链三变体）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnEvents_TC3_PhaseChangedConsecutiveDoubles,
    "Rento.PlayerTurn.TurnEventPayloads.TC3_PhaseChangedConsecutiveDoubles",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnEvents_TC3_PhaseChangedConsecutiveDoubles::RunTest(const FString& Parameters)
{
    UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC3_World"));
    if (!TestNotNull(TEXT("TC-3: World"), World)) return false;
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-3: Sub"), Sub)) { TurnEventTestHelpers::DestroyGameWorld(World); return false; }
    if (!TestTrue(TEXT("TC-3: Init"), Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(2))))
    { TurnEventTestHelpers::DestroyGameWorld(World); return false; }

    const int32 FirstId = TurnEventTestHelpers::FindFirstPlayerId(Sub);
    UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
    Sub->OnPhaseChanged.AddDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);

    // 驱动双点链：StartTurn(RollPhase CD=0) → PRR(true) CD=1 → 推进 → EndTurn(双点回 RollPhase CD=1)
    //            → PRR(true) CD=2 → 推进 → EndTurn(双点回 RollPhase CD=2)
    bool bSent = false;
    Sub->StartTurn(FirstId);                              // RollPhase, CD=0
    Sub->ProcessRollResult(true, bSent);                 // CD=1, MovePhase
    Sub->AdvanceFromMovePhase();                         // ResolvePhase
    Sub->AdvanceFromResolvePhase();                      // PostRollAction
    Sub->EndTurn(false);                                 // 双点 → RollPhase, CD=1
    Sub->ProcessRollResult(true, bSent);                 // CD=2, MovePhase
    Sub->AdvanceFromMovePhase();                         // ResolvePhase
    Sub->AdvanceFromResolvePhase();                      // PostRollAction
    Sub->EndTurn(false);                                 // 双点 → RollPhase, CD=2

    Sub->OnPhaseChanged.RemoveDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);

    // 过滤 RollPhase 进入事件的 ConsecutiveDoubles 序列
    TArray<int32> RollPhaseCDs;
    for (const FPhaseChangedInfo& Info : Spy->PhaseChanges)
    {
        if (Info.NewPhase == ETurnPhase::RollPhase) { RollPhaseCDs.Add(Info.ConsecutiveDoubles); }
    }
    if (TestTrue(
        FString::Printf(TEXT("TC-3: RollPhase 进入事件应 >=3 次（实际 %d）"), RollPhaseCDs.Num()),
        RollPhaseCDs.Num() >= 3))
    {
        TestEqual(TEXT("TC-3 ①: 首次 RollPhase ConsecutiveDoubles==0"), RollPhaseCDs[0], 0);
        TestEqual(TEXT("TC-3 ②: 第一次双点后 RollPhase ConsecutiveDoubles==1"), RollPhaseCDs[1], 1);
        TestEqual(TEXT("TC-3 ③: 第二次双点后 RollPhase ConsecutiveDoubles==2"), RollPhaseCDs[2], 2);
    }

    TurnEventTestHelpers::ReleaseSpy(Spy);
    TurnEventTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC4（AC-4）— OnTurnEnded.bGrantsExtraTurn true（双点）/ false（非双点）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnEvents_TC4_TurnEndedGrantsExtraTurn,
    "Rento.PlayerTurn.TurnEventPayloads.TC4_TurnEndedGrantsExtraTurn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnEvents_TC4_TurnEndedGrantsExtraTurn::RunTest(const FString& Parameters)
{
    // ---- 场景①：双点 → bGrantsExtraTurn==true ----
    {
        UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC4a_World"));
        if (!TestNotNull(TEXT("TC-4a: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(2));
        const int32 FirstId = TurnEventTestHelpers::FindFirstPlayerId(Sub);
        UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
        Sub->OnTurnEnded.AddDynamic(Spy, &UTurnEventSpy::HandleTurnEnded);

        bool bSent = false;
        Sub->StartTurn(FirstId);
        Sub->ProcessRollResult(true, bSent);   // 双点
        Sub->AdvanceFromMovePhase();
        Sub->AdvanceFromResolvePhase();
        Sub->EndTurn(false);

        Sub->OnTurnEnded.RemoveDynamic(Spy, &UTurnEventSpy::HandleTurnEnded);
        if (TestTrue(TEXT("TC-4a: OnTurnEnded 至少广播 1 次"), Spy->TurnEnded.Num() >= 1))
        {
            TestTrue(TEXT("TC-4a: 双点 bGrantsExtraTurn==true"),
                Spy->TurnEnded[0].bGrantsExtraTurn);
            TestEqual(TEXT("TC-4a: PlayerId==FirstId"), Spy->TurnEnded[0].PlayerId, FirstId);
        }
        TurnEventTestHelpers::ReleaseSpy(Spy);
        TurnEventTestHelpers::DestroyGameWorld(World);
    }

    // ---- 场景②：非双点 → bGrantsExtraTurn==false ----
    {
        UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC4b_World"));
        if (!TestNotNull(TEXT("TC-4b: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(2));
        const int32 FirstId = TurnEventTestHelpers::FindFirstPlayerId(Sub);
        UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
        Sub->OnTurnEnded.AddDynamic(Spy, &UTurnEventSpy::HandleTurnEnded);

        bool bSent = false;
        Sub->StartTurn(FirstId);
        Sub->ProcessRollResult(false, bSent);  // 非双点
        Sub->AdvanceFromMovePhase();
        Sub->AdvanceFromResolvePhase();
        Sub->EndTurn(false);

        Sub->OnTurnEnded.RemoveDynamic(Spy, &UTurnEventSpy::HandleTurnEnded);
        if (TestTrue(TEXT("TC-4b: OnTurnEnded 至少广播 1 次"), Spy->TurnEnded.Num() >= 1))
        {
            TestFalse(TEXT("TC-4b: 非双点 bGrantsExtraTurn==false"),
                Spy->TurnEnded[0].bGrantsExtraTurn);
        }
        TurnEventTestHelpers::ReleaseSpy(Spy);
        TurnEventTestHelpers::DestroyGameWorld(World);
    }
    return true;
}

// =============================================================================
// TC5（AC-5）— InitializeFromConfig 广播 OnTurnOrderResolved + payload
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnEvents_TC5_TurnOrderResolvedBroadcast,
    "Rento.PlayerTurn.TurnEventPayloads.TC5_TurnOrderResolvedBroadcast",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnEvents_TC5_TurnOrderResolvedBroadcast::RunTest(const FString& Parameters)
{
    UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC5_World"));
    if (!TestNotNull(TEXT("TC-5: World"), World)) return false;
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-5: Sub"), Sub)) { TurnEventTestHelpers::DestroyGameWorld(World); return false; }

    // 先绑定再 Init（OnTurnOrderResolved 在 InitializeFromConfig 内广播）
    UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
    Sub->OnTurnOrderResolved.AddDynamic(Spy, &UTurnEventSpy::HandleOrderResolved);

    const bool bInit = Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(3));
    Sub->OnTurnOrderResolved.RemoveDynamic(Spy, &UTurnEventSpy::HandleOrderResolved);

    TestTrue(TEXT("TC-5: Init 成功"), bInit);
    if (TestEqual(TEXT("TC-5: OnTurnOrderResolved 广播恰 1 次"), Spy->OrderResolved.Num(), 1))
    {
        TestEqual(TEXT("TC-5: OrderedPlayerIds.Num()==3"),
            Spy->OrderResolved[0].OrderedPlayerIds.Num(), 3);
        // seed=42 正常 RNG 定序，非席位裁定
        TestFalse(TEXT("TC-5: 正常定序 bResolvedBySeatTiebreak==false"),
            Spy->OrderResolved[0].bResolvedBySeatTiebreak);
    }

    TurnEventTestHelpers::ReleaseSpy(Spy);
    TurnEventTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC6（AC-6）— OnAIActionExecuted 每执行动作广播 ActionIndex 0..M-1，跳过不广播
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnEvents_TC6_AIActionExecutedSequence,
    "Rento.PlayerTurn.TurnEventPayloads.TC6_AIActionExecutedSequence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnEvents_TC6_AIActionExecutedSequence::RunTest(const FString& Parameters)
{
    // ---- 场景①：全部可行（无 validator）→ 2 广播 ActionIndex 0,1 ----
    {
        UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC6a_World"));
        if (!TestNotNull(TEXT("TC-6a: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(2));
        const int32 FirstId = TurnEventTestHelpers::FindFirstPlayerId(Sub);
        if (!TestTrue(TEXT("TC-6a: 推进到 PostRollAction"),
            TurnEventTestHelpers::AdvanceToPostRollAction(Sub, FirstId)))
        { TurnEventTestHelpers::DestroyGameWorld(World); return false; }
        const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

        TSharedPtr<FAIDecisionMakerSpy> AISpy = MakeShared<FAIDecisionMakerSpy>();
        AISpy->PostRollActionsToReturn = {
            FTurnAction{ EAIActionType::BuyProperty, 5 },
            FTurnAction{ EAIActionType::BuildHouse,  5 } };
        Sub->SetAIDecisionMaker(AISpy);

        UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
        Sub->OnAIActionExecuted.AddDynamic(Spy, &UTurnEventSpy::HandleAIActionExecuted);

        FGameStateSnapshot Snap;
        Sub->RunAiPostRollActions(ActiveBefore, Snap);

        Sub->OnAIActionExecuted.RemoveDynamic(Spy, &UTurnEventSpy::HandleAIActionExecuted);
        if (TestEqual(TEXT("TC-6a: 全可行广播 2 次"), Spy->AIActions.Num(), 2))
        {
            TestEqual(TEXT("TC-6a: [0].ActionIndex==0"), Spy->AIActions[0].ActionIndex, 0);
            TestEqual(TEXT("TC-6a: [1].ActionIndex==1"), Spy->AIActions[1].ActionIndex, 1);
            TestEqual(TEXT("TC-6a: [0].ActionType==BuyProperty"),
                static_cast<int32>(Spy->AIActions[0].ActionType), static_cast<int32>(EActionType::BuyProperty));
            TestEqual(TEXT("TC-6a: [1].ActionType==BuildHouse"),
                static_cast<int32>(Spy->AIActions[1].ActionType), static_cast<int32>(EActionType::BuildHouse));
            TestEqual(TEXT("TC-6a: [0].ActingPlayerId==ActiveBefore"),
                Spy->AIActions[0].ActingPlayerId, ActiveBefore);
        }
        TurnEventTestHelpers::ReleaseSpy(Spy);
        TurnEventTestHelpers::DestroyGameWorld(World);
    }

    // ---- 场景②：含不可行（validator {true,false,true}）→ 2 广播 ActionIndex 0,1（跳过不占号）----
    {
        UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC6b_World"));
        if (!TestNotNull(TEXT("TC-6b: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(2));
        const int32 FirstId = TurnEventTestHelpers::FindFirstPlayerId(Sub);
        if (!TestTrue(TEXT("TC-6b: 推进到 PostRollAction"),
            TurnEventTestHelpers::AdvanceToPostRollAction(Sub, FirstId)))
        { TurnEventTestHelpers::DestroyGameWorld(World); return false; }
        const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

        TSharedPtr<FAIDecisionMakerSpy> AISpy = MakeShared<FAIDecisionMakerSpy>();
        AISpy->PostRollActionsToReturn = {
            FTurnAction{ EAIActionType::BuyProperty,      5 },
            FTurnAction{ EAIActionType::BuildHouse,       5 },
            FTurnAction{ EAIActionType::MortgageProperty, 7 } };
        Sub->SetAIDecisionMaker(AISpy);

        TSharedPtr<FActionValidatorSpy> VSpy = MakeShared<FActionValidatorSpy>();
        VSpy->FeasibilityByCallOrder = { true, false, true }; // 第2条不可行
        Sub->SetActionValidator(VSpy);

        UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
        Sub->OnAIActionExecuted.AddDynamic(Spy, &UTurnEventSpy::HandleAIActionExecuted);

        FGameStateSnapshot Snap;
        Sub->RunAiPostRollActions(ActiveBefore, Snap);

        Sub->OnAIActionExecuted.RemoveDynamic(Spy, &UTurnEventSpy::HandleAIActionExecuted);
        if (TestEqual(TEXT("TC-6b: 跳过 1 条后广播 2 次"), Spy->AIActions.Num(), 2))
        {
            TestEqual(TEXT("TC-6b: [0].ActionIndex==0"), Spy->AIActions[0].ActionIndex, 0);
            TestEqual(TEXT("TC-6b: [1].ActionIndex==1（连续不占号）"), Spy->AIActions[1].ActionIndex, 1);
            TestEqual(TEXT("TC-6b: [1].ActionType==Mortgage（第2条跳过，第3条执行）"),
                static_cast<int32>(Spy->AIActions[1].ActionType), static_cast<int32>(EActionType::Mortgage));
        }
        TurnEventTestHelpers::ReleaseSpy(Spy);
        TurnEventTestHelpers::DestroyGameWorld(World);
    }
    return true;
}

// =============================================================================
// TC7（AC-7）— HandleBuildingChanged 仅 ResolvePhase/PostRollAction 广播
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTurnEvents_TC7_BuildingAnnouncedBeat,
    "Rento.PlayerTurn.TurnEventPayloads.TC7_BuildingAnnouncedBeat",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTurnEvents_TC7_BuildingAnnouncedBeat::RunTest(const FString& Parameters)
{
    // ---- 场景①：ResolvePhase → 广播 1 次 ----
    {
        UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC7a_World"));
        if (!TestNotNull(TEXT("TC-7a: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(2));
        const int32 FirstId = TurnEventTestHelpers::FindFirstPlayerId(Sub);
        if (!TestTrue(TEXT("TC-7a: 推进到 ResolvePhase"),
            TurnEventTestHelpers::AdvanceToResolvePhase(Sub, FirstId)))
        { TurnEventTestHelpers::DestroyGameWorld(World); return false; }
        const int32 ActiveNow = Sub->GetCurrentActivePlayerId();

        UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
        Sub->OnBuildingAnnounced.AddDynamic(Spy, &UTurnEventSpy::HandleBuildingAnnounced);

        Sub->HandleBuildingChanged(15, 2);

        Sub->OnBuildingAnnounced.RemoveDynamic(Spy, &UTurnEventSpy::HandleBuildingAnnounced);
        if (TestEqual(TEXT("TC-7a: ResolvePhase 广播 1 次"), Spy->Announced.Num(), 1))
        {
            TestEqual(TEXT("TC-7a: TileIndex==15"), Spy->Announced[0].TileIndex, 15);
            TestEqual(TEXT("TC-7a: NewHouseCount==2"), Spy->Announced[0].NewHouseCount, 2);
            TestEqual(TEXT("TC-7a: ActingPlayerId==当前回合玩家"),
                Spy->Announced[0].ActingPlayerId, ActiveNow);
        }
        TurnEventTestHelpers::ReleaseSpy(Spy);
        TurnEventTestHelpers::DestroyGameWorld(World);
    }

    // ---- 场景②（对照）：TurnStart（Init 后阶段）→ 不广播 ----
    {
        UWorld* World = TurnEventTestHelpers::CreateGameWorld(TEXT("TC7b_World"));
        if (!TestNotNull(TEXT("TC-7b: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        Sub->InitializeFromConfig(TurnEventTestHelpers::MakeConfig(2));
        // Init 后 CurrentPhase==TurnStart（非 Resolve/PostRoll）
        TestEqual(TEXT("TC-7b: Init 后阶段为 TurnStart"),
            static_cast<int32>(Sub->GetCurrentPhase()), static_cast<int32>(ETurnPhase::TurnStart));

        UTurnEventSpy* Spy = TurnEventTestHelpers::MakeSpy();
        Sub->OnBuildingAnnounced.AddDynamic(Spy, &UTurnEventSpy::HandleBuildingAnnounced);

        Sub->HandleBuildingChanged(15, 2);

        Sub->OnBuildingAnnounced.RemoveDynamic(Spy, &UTurnEventSpy::HandleBuildingAnnounced);
        TestEqual(TEXT("TC-7b: 非 Resolve/PostRoll 阶段不广播（对照）"), Spy->Announced.Num(), 0);

        TurnEventTestHelpers::ReleaseSpy(Spy);
        TurnEventTestHelpers::DestroyGameWorld(World);
    }
    return true;
}
