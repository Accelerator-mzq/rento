// GameStateSnapshotAiHooksTest.cpp
// =============================================================================
// 集成测试：GameStateSnapshot 值语义 + AI 决策 hook DI（story pt-007 簇 A）
//
// 物理路径：Source/rentoTests/Private/GameStateSnapshotAiHooksTest.cpp
// Automation 类目：Rento.PlayerTurn.GameStateSnapshotAiHooks
//
// 覆盖 AC：
//   TC-1 (AC-1) — FGameStateSnapshot 值语义深拷贝（改 Copy 不影响 Original）
//   TC-3 (AC-3) — AI PostRollAction 非空动作列表→DecidePostRollActions 1 次→EndTurn→移交
//   TC-37d (AC-37d) — AI PostRollAction 空数组[]→0 条→仍 EndTurn→移交
//
// 测试机制（headless -nullrhi）：
//   - 独立命名空间 GssAiHooksTestHelpers 避免 ODR（不复用 TurnPhaseTestHelpers）
//   - CreateGameWorld / DestroyGameWorld / MakeConfig / FindFirstPlayerId 套路相同
//   - FAIDecisionMakerSpy 纯 C++ struct（非 UObject），直接注入 DI（G3 符合约束）
//   - 阶段推进序列：StartTurn → ProcessRollResult(false) → AdvanceFromMovePhase
//     → AdvanceFromResolvePhase → 进入 PostRollAction → RunAiPostRollActions
//
// 断言非 vacuous 保证：
//   TC-1：改 Copy 字段后断言 Original 未变；若内嵌 UObject* 浅拷贝则 Original 被改 → FAIL
//   TC-3：DecidePostRollActions 计数==1；若 RunAiPostRollActions 未调 → 0 → FAIL
//         GetCurrentActivePlayerId != ActiveBefore 断言移交；若 EndTurn 未执行 → 同 ID → FAIL
//   TC-37d：同 TC-3 结构，空数组路径单独验证
//
// ⚠ 已知坑（来自项目坑清单）：
//   G1 EAutomationTestFlags：用 EAutomationTestFlags_ApplicationContextMask | ProductFilter
//   G2 错误路径 UE_LOG Error，不 ensure（headless ensure callstack 不可靠）
//   G3 纯 C++ 接口 spy 用 struct（FAIDecisionMakerSpy），不需要 UCLASS
//   G4 禁 *new 堆分配 / AddToRoot（非 UObject 无 GC；UObject spy 才需 AddToRoot）
//
// 规范依据：
//   - story pt-007 AC-1 / AC-3 / AC-37d
//   - ADR-0006（GameStateSnapshot 值语义、const& 传参）
//   - ADR-0007（AI 决策 C++ 落地，headless 可测）
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

// =============================================================================
// 测试辅助（独立命名空间避免 ODR，不依赖 TurnPhaseTestHelpers）
// =============================================================================
namespace GssAiHooksTestHelpers
{
    /** 创建 Game World（headless，不通知引擎）。调用方负责 DestroyWorld。 */
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
     * TiebreakRounds 默认 1：快速结束平手重掷。
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
     * 返回 -1 表示找不到。
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
     * 将状态机推进到 PostRollAction 阶段（非双点、非在狱路径）。
     *
     * 序列：StartTurn(PlayerId) → ProcessRollResult(false) → AdvanceFromMovePhase
     *       → AdvanceFromResolvePhase → 进入 PostRollAction
     *
     * 前提：PlayerId 对应玩家 bIsInJail=false。
     *
     * @param Sub      已初始化的 UPlayerTurnSubsystem
     * @param PlayerId 当前行动玩家 PlayerId
     * @return true = 成功进入 PostRollAction；false = 推进失败（测试应 early-out）
     */
    static bool AdvanceToPostRollAction(UPlayerTurnSubsystem* Sub, int32 PlayerId)
    {
        check(Sub);

        // 确认玩家不在狱（避免走 JailTurn 分支）
        URentoPlayerState* PS = Sub->FindPlayerById(PlayerId);
        if (PS) { PS->bIsInJail = false; PS->ConsecutiveDoubles = 0; }

        // 1. StartTurn → TurnStart → RollPhase（非在狱路由）
        Sub->StartTurn(PlayerId);
        if (Sub->GetCurrentPhase() != ETurnPhase::RollPhase)
        {
            UE_LOG(LogTemp, Error,
                TEXT("GssAiHooksTestHelpers::AdvanceToPostRollAction — "
                     "StartTurn 后未到 RollPhase（实际 %d）"),
                static_cast<int32>(Sub->GetCurrentPhase()));
            return false;
        }

        // 2. ProcessRollResult(非双点) → MovePhase
        bool bSentToJail = false;
        Sub->ProcessRollResult(/*bIsDouble=*/false, bSentToJail);
        if (Sub->GetCurrentPhase() != ETurnPhase::MovePhase)
        {
            return false;
        }

        // 3. AdvanceFromMovePhase → ResolvePhase
        Sub->AdvanceFromMovePhase();
        if (Sub->GetCurrentPhase() != ETurnPhase::ResolvePhase)
        {
            return false;
        }

        // 4. AdvanceFromResolvePhase → PostRollAction
        Sub->AdvanceFromResolvePhase();
        return Sub->GetCurrentPhase() == ETurnPhase::PostRollAction;
    }

} // namespace GssAiHooksTestHelpers

// =============================================================================
// TC-1（AC-1）— FGameStateSnapshot 值语义深拷贝
//
// GIVEN：构造 FGameStateSnapshot，填充字段（含 Tiles 加 1 条 FTileSnapshotEntry）
// WHEN：深拷贝一份 Copy=Original；修改 Copy.Tiles[0].HouseCount 和 Copy.SelfCash
// THEN：Original.Tiles[0].HouseCount 和 Original.SelfCash **未变**（值语义深拷贝）
//
// 非 vacuous 证明：
//   若内嵌 UObject* 则拷贝是浅拷贝（共享指针），改 Copy 会同时改 Original → FAIL。
//   若 TArray 是浅拷贝（不符合 UE TArray 契约），改 Copy.Tiles[0] 同样影响 Original → FAIL。
//   本测试用字面值填充，无 UObject 活指针，验证真正的值复制。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC1_SnapshotValueSemantics,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC1_SnapshotValueSemantics",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC1_SnapshotValueSemantics::RunTest(const FString& Parameters)
{
    // ---- 构造原始 snapshot（无需 World，纯 USTRUCT 值操作）----

    FTileSnapshotEntry TileEntry;
    TileEntry.TileIndex      = 5;
    TileEntry.OwnerId        = 1;
    TileEntry.HouseCount     = 2;           // 关键字段：拷贝后修改，验原始不变
    TileEntry.bIsMortgaged   = false;
    TileEntry.bIsMonopoly    = true;
    TileEntry.ColorGroup     = 3;
    TileEntry.PurchasePrice  = 300;
    TileEntry.MortgageValue  = 150;
    TileEntry.BuildingCost   = 200;
    TileEntry.UnmortgageCost = 165;
    TileEntry.PreaggregatedNlv = 250;

    FGameStateSnapshot Original;
    Original.SelfPlayerId          = 1;
    Original.SelfCash              = 1000;  // 关键字段：拷贝后修改，验原始不变
    Original.Rent_top1             = 50;
    Original.Rent_top2             = 30;
    Original.StartingCash          = 1500;
    Original.BoardTileCountClassic = 40;
    Original.JailBailAmount        = 50;
    Original.MaxJailTurns          = 3;
    Original.bHasJailCard          = false;
    Original.JailTurnsServed       = 0;
    Original.Tiles.Add(TileEntry);

    // ---- 深拷贝（C++ 值语义，struct 默认拷贝构造）----
    FGameStateSnapshot Copy = Original;

    // ---- 修改 Copy（不应影响 Original）----
    Copy.SelfCash               = 9999;   // 改 Copy 的标量字段
    Copy.Tiles[0].HouseCount    = 5;      // 改 Copy 的 Tiles 数组元素字段

    // ---- 断言 Original 未受影响（值语义深拷贝保证）----

    // 断言 Original.SelfCash 未变
    TestEqual(
        TEXT("TC-1: 改 Copy.SelfCash 后，Original.SelfCash 应未变（值语义）"),
        Original.SelfCash, 1000);

    // 断言 Original.Tiles[0].HouseCount 未变
    if (TestTrue(TEXT("TC-1: Original.Tiles 应有 1 条"), Original.Tiles.Num() == 1))
    {
        TestEqual(
            TEXT("TC-1: 改 Copy.Tiles[0].HouseCount 后，Original.Tiles[0].HouseCount 应未变（值语义深拷贝）"),
            Original.Tiles[0].HouseCount, 2);
    }

    // 附加验证：Copy 的字段确实被改（避免 vacuous——若改赋值失效，Copy 仍==Original 则以下断言 FAIL）
    TestEqual(TEXT("TC-1: Copy.SelfCash 应为 9999"), Copy.SelfCash, 9999);
    TestEqual(TEXT("TC-1: Copy.Tiles[0].HouseCount 应为 5"), Copy.Tiles[0].HouseCount, 5);

    return true;
}

// =============================================================================
// TC-3（AC-3）— AI PostRollAction 非空动作列表执行路径
//
// GIVEN：2 人对局，InitializeFromConfig(2)，推进到 PostRollAction
//        注入 FAIDecisionMakerSpy（PostRollActionsToReturn = 2 条非空动作）
//        构造 mock FGameStateSnapshot
// WHEN：RunAiPostRollActions(ActiveBefore, Snapshot)
// THEN：① Spy.DecidePostRollActionsCallCount == 1（hook 恰被调 1 次）
//       ② GetCurrentActivePlayerId() != ActiveBefore（EndTurn 已移交下一玩家）
//       ③ GetCurrentPhase() == RollPhase 或 JailTurn（下一玩家已进入新回合）
//
// 非 vacuous：
//   若 RunAiPostRollActions 未调 DecidePostRollActions → 计数 0 → ① FAIL
//   若 EndTurn 未执行 → 行动权未移交 → ActiveId 相同 → ② FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC3_NonEmptyActionsEndsTurn,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC3_NonEmptyActionsEndsTurn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC3_NonEmptyActionsEndsTurn::RunTest(const FString& Parameters)
{
    // ---- 创建 World + 初始化 ----
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC3_World"));
    if (!TestNotNull(TEXT("TC-3: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-3: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-3: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 找先手玩家 ID（TurnOrderIndex==0）
    const int32 FirstId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-3: 应找到先手玩家"), FirstId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // ---- 推进到 PostRollAction ----
    const bool bReached = GssAiHooksTestHelpers::AdvanceToPostRollAction(Sub, FirstId);
    if (!TestTrue(TEXT("TC-3: 应成功推进到 PostRollAction"), bReached))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }
    TestEqual(TEXT("TC-3: 推进后阶段应为 PostRollAction"),
        Sub->GetCurrentPhase(), ETurnPhase::PostRollAction);

    // 记录推进前的活跃玩家 ID
    const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();
    TestNotEqual(TEXT("TC-3: ActiveBefore 应有效"), ActiveBefore, -1);

    // ---- 注入 FAIDecisionMakerSpy（2 条非空动作）----
    TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
    // 注入 2 条非空动作（BuyProperty + BuildHouse 占位；本簇不执行实际经济操作）
    FTurnAction Action1;
    Action1.ActionType      = EAIActionType::BuyProperty;
    Action1.TargetTileIndex = 5;
    FTurnAction Action2;
    Action2.ActionType      = EAIActionType::BuildHouse;
    Action2.TargetTileIndex = 5;
    Spy->PostRollActionsToReturn.Add(Action1);
    Spy->PostRollActionsToReturn.Add(Action2);

    Sub->SetAIDecisionMaker(Spy);

    // ---- 构造 mock FGameStateSnapshot（字面量，无系统依赖，ADR-0006 IG#5）----
    FGameStateSnapshot MockSnapshot;
    MockSnapshot.SelfPlayerId = ActiveBefore;
    MockSnapshot.SelfCash     = 1500;
    MockSnapshot.Rent_top1    = 50;
    MockSnapshot.Rent_top2    = 30;

    // ---- 执行 RunAiPostRollActions ----
    Sub->RunAiPostRollActions(ActiveBefore, MockSnapshot);

    // ==== 断言 ====

    // ① Spy 的 DecidePostRollActions 调用次数 == 1
    TestEqual(
        TEXT("TC-3 ①: DecidePostRollActions 应被调用恰 1 次"),
        Spy->DecidePostRollActionsCallCount, 1);

    // ② EndTurn 已执行：行动权已移交下一玩家（GetCurrentActivePlayerId != ActiveBefore）
    const int32 ActiveAfter = Sub->GetCurrentActivePlayerId();
    TestNotEqual(
        TEXT("TC-3 ②: EndTurn 后 GetCurrentActivePlayerId 应已移交（!= ActiveBefore）"),
        ActiveAfter, ActiveBefore);

    // ③ 下一玩家已进入新回合（RollPhase 或 JailTurn，取决于是否在狱）
    //    只要不是 PostRollAction / TurnEnd（已结束），说明移交成功
    const ETurnPhase PhaseAfter = Sub->GetCurrentPhase();
    const bool bValidNextPhase =
        (PhaseAfter == ETurnPhase::RollPhase) ||
        (PhaseAfter == ETurnPhase::JailTurn)  ||
        (PhaseAfter == ETurnPhase::TurnStart);
    TestTrue(
        FString::Printf(
            TEXT("TC-3 ③: EndTurn 后阶段应为下一玩家的 RollPhase/JailTurn/TurnStart（实际 %d）"),
            static_cast<int32>(PhaseAfter)),
        bValidNextPhase);

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-37d（AC-37d）— AI PostRollAction 空数组[]路径直接 EndTurn
//
// GIVEN：2 人对局，推进到 PostRollAction
//        注入 FAIDecisionMakerSpy（PostRollActionsToReturn = 空数组 []）
// WHEN：RunAiPostRollActions(ActiveBefore, Snapshot)
// THEN：① Spy.DecidePostRollActionsCallCount == 1（hook 被调 1 次）
//       ② 执行 0 条动作后仍调 EndTurn → GetCurrentActivePlayerId() != ActiveBefore（移交）
//
// 非 vacuous：
//   若框架未调 DecidePostRollActions → 计数 0 → ① FAIL（无法靠「碰巧 0 条」蒙混）
//   若 EndTurn 未执行 → 行动权未移交 → ② FAIL
//
// 覆盖最高频「AI 不做 PostRollAction」路径（AC-37d 指定）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC37d_EmptyActionsEndsTurn,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC37d_EmptyActionsEndsTurn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC37d_EmptyActionsEndsTurn::RunTest(const FString& Parameters)
{
    // ---- 创建 World + 初始化 ----
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC37d_World"));
    if (!TestNotNull(TEXT("TC-37d: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-37d: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-37d: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 找先手玩家 ID
    const int32 FirstId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-37d: 应找到先手玩家"), FirstId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // ---- 推进到 PostRollAction ----
    const bool bReached = GssAiHooksTestHelpers::AdvanceToPostRollAction(Sub, FirstId);
    if (!TestTrue(TEXT("TC-37d: 应成功推进到 PostRollAction"), bReached))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 记录推进前的活跃玩家 ID
    const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

    // ---- 注入 FAIDecisionMakerSpy（空数组 []，PostRollActionsToReturn 默认空）----
    TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
    // PostRollActionsToReturn 默认为空 TArray，不需要额外设置
    Sub->SetAIDecisionMaker(Spy);

    // ---- 构造 mock snapshot ----
    FGameStateSnapshot MockSnapshot;
    MockSnapshot.SelfPlayerId = ActiveBefore;
    MockSnapshot.SelfCash     = 1500;

    // ---- 执行 RunAiPostRollActions ----
    Sub->RunAiPostRollActions(ActiveBefore, MockSnapshot);

    // ==== 断言 ====

    // ① Spy 的 DecidePostRollActions 调用次数 == 1（空数组也必须调一次 hook）
    TestEqual(
        TEXT("TC-37d ①: DecidePostRollActions 应被调用恰 1 次（即使返回空数组）"),
        Spy->DecidePostRollActionsCallCount, 1);

    // ② 执行 0 条动作后仍触发 EndTurn → 行动权已移交
    const int32 ActiveAfter = Sub->GetCurrentActivePlayerId();
    TestNotEqual(
        TEXT("TC-37d ②: 空数组 EndTurn 后 GetCurrentActivePlayerId 应已移交（!= ActiveBefore）"),
        ActiveAfter, ActiveBefore);

    // 附加诊断：打印移交结果（辅助 verify 时肉眼核对）
    UE_LOG(LogTemp, Log,
        TEXT("TC-37d 诊断：ActiveBefore=%d ActiveAfter=%d Phase=%d"),
        ActiveBefore, ActiveAfter, static_cast<int32>(Sub->GetCurrentPhase()));

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}
