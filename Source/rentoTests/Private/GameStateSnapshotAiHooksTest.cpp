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
#include "ActionValidatorSpy.h"
#include "RuleProviderSpies.h"    // FOwnershipProviderSpy/FBuildingProviderSpy/FEconomyResolverSpy（簇C1）

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

// =============================================================================
// 簇 B 辅助 Helper — AdvanceToJailTurn（供 TC-39 / TC-39b-1 / TC-39b-2 使用）
// =============================================================================
namespace GssAiHooksTestHelpers
{
    /**
     * 将指定玩家设为在狱状态并 StartTurn，使状态机进入 JailTurn。
     *
     * 设 PlayerState->bIsInJail=true、JailTurnsServed=0、ConsecutiveDoubles=0，
     * 然后 StartTurn(PlayerId)，路由到 JailTurn 阶段。
     *
     * @param Sub      已初始化的 UPlayerTurnSubsystem
     * @param PlayerId 目标玩家 PlayerId（须为当前先手玩家，保证 CurrentActivePlayerId 对齐）
     * @return true = 成功进入 JailTurn；false = 失败（测试应 early-out）
     */
    static bool AdvanceToJailTurn(UPlayerTurnSubsystem* Sub, int32 PlayerId)
    {
        check(Sub);
        URentoPlayerState* PS = Sub->FindPlayerById(PlayerId);
        if (PS)
        {
            // 设置在狱初始状态
            PS->bIsInJail = true;
            PS->JailTurnsServed = 0;
            PS->ConsecutiveDoubles = 0;
        }
        // StartTurn 路由：bIsInJail=true → JailTurn
        Sub->StartTurn(PlayerId);
        return Sub->GetCurrentPhase() == ETurnPhase::JailTurn;
    }
} // namespace GssAiHooksTestHelpers

// =============================================================================
// TC-39（AC-5 / AC-39）— AI 出狱决策三路径正确发起
//
// 覆盖 PayBail 可行 / UseCard 可行 / RollDouble 三条路径，每条路径独立 World。
//
// GIVEN：2 人对局，jail player = 先手（TurnOrderIndex==0），进入 JailTurn
// WHEN：RunAiJailAction(jailPlayerId, Snapshot)，spy 控制不同出狱决策
// THEN：
//   PayBail 可行（Cash=100 >= BailAmount=50）→ 出狱路径
//     ① JailTurnsServed 未 +1（== 进入前的 0）
//     ② GetCurrentActivePlayerId() != jailPlayerId（移交）
//   UseCard 可行（bHasJailCard=true）→ 出狱路径
//     ① JailTurnsServed 未 +1
//     ② 移交
//   RollDouble → 留狱待掷
//     ① JailTurnsServed == 1（+1）
//     ② 移交
//
// 非 vacuous：
//   若 PayBail 路径误判不可行（走 AdvanceFromJailTurn(true)）→ JailTurnsServed=1 → ① FAIL
//   若 RollDouble 路径误判出狱（走 AdvanceFromJailTurn(false)）→ JailTurnsServed=0 → ① FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC39_JailThreePaths,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC39_JailThreePaths",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC39_JailThreePaths::RunTest(const FString& Parameters)
{
    // ----- PayBail 可行路径 -----
    {
        UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC39_PayBail_World"));
        if (!TestNotNull(TEXT("TC-39 PayBail: World 应能创建"), World)) return false;

        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("TC-39 PayBail: Subsystem 应能取得"), Sub))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
        if (!TestTrue(TEXT("TC-39 PayBail: InitializeFromConfig(P=2) 应成功"), bInit))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // 找先手玩家（TurnOrderIndex==0）作为 jail player
        const int32 JailPlayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
        if (!TestNotEqual(TEXT("TC-39 PayBail: 应找到先手玩家"), JailPlayerId, -1))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // 推进到 JailTurn
        const bool bReached = GssAiHooksTestHelpers::AdvanceToJailTurn(Sub, JailPlayerId);
        if (!TestTrue(TEXT("TC-39 PayBail: 应成功进入 JailTurn"), bReached))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // 记录进入前 JailTurnsServed（应为 0）
        URentoPlayerState* PS = Sub->FindPlayerById(JailPlayerId);
        const int32 JailTurnsBeforePayBail = PS ? PS->JailTurnsServed : -1;

        // 注入 spy：PayBail 出狱决策 + Cash 充足
        TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
        Spy->JailActionToReturn = EJailAction::PayBail;
        Sub->SetAIDecisionMaker(Spy);

        // 构造 mock snapshot：Cash 充足（100 >= 50）
        FGameStateSnapshot Snap;
        Snap.SelfPlayerId    = JailPlayerId;
        Snap.SelfCash        = 100;
        Snap.JailBailAmount  = 50;
        Snap.bHasJailCard    = false;
        Snap.JailTurnsServed = 0;

        // 执行 RunAiJailAction
        Sub->RunAiJailAction(JailPlayerId, Snap);

        // 断言 ① JailTurnsServed 未 +1（出狱路径不服刑）
        const int32 JailTurnsAfterPayBail = PS ? PS->JailTurnsServed : -1;
        TestEqual(
            TEXT("TC-39 PayBail ①: 出狱路径 JailTurnsServed 应未 +1（== 进入前）"),
            JailTurnsAfterPayBail, JailTurnsBeforePayBail);

        // 断言 ② 行动权已移交
        const int32 ActiveAfterPayBail = Sub->GetCurrentActivePlayerId();
        TestNotEqual(
            TEXT("TC-39 PayBail ②: RunAiJailAction 后应已移交（ActiveId != jailPlayerId）"),
            ActiveAfterPayBail, JailPlayerId);

        GssAiHooksTestHelpers::DestroyGameWorld(World);
    }

    // ----- UseCard 可行路径 -----
    {
        UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC39_UseCard_World"));
        if (!TestNotNull(TEXT("TC-39 UseCard: World 应能创建"), World)) return false;

        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("TC-39 UseCard: Subsystem 应能取得"), Sub))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
        const int32 JailPlayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
        if (!TestNotEqual(TEXT("TC-39 UseCard: 应找到先手玩家"), JailPlayerId, -1))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        GssAiHooksTestHelpers::AdvanceToJailTurn(Sub, JailPlayerId);

        URentoPlayerState* PS = Sub->FindPlayerById(JailPlayerId);
        const int32 JailTurnsBeforeUseCard = PS ? PS->JailTurnsServed : -1;

        TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
        Spy->JailActionToReturn = EJailAction::UseCard;
        Sub->SetAIDecisionMaker(Spy);

        // 构造 mock snapshot：有出狱卡
        FGameStateSnapshot Snap;
        Snap.SelfPlayerId    = JailPlayerId;
        Snap.SelfCash        = 1500;
        Snap.JailBailAmount  = 50;
        Snap.bHasJailCard    = true;   // 有卡
        Snap.JailTurnsServed = 0;

        Sub->RunAiJailAction(JailPlayerId, Snap);

        // 断言 ① JailTurnsServed 未 +1（出狱路径不服刑）
        const int32 JailTurnsAfterUseCard = PS ? PS->JailTurnsServed : -1;
        TestEqual(
            TEXT("TC-39 UseCard ①: 出狱路径 JailTurnsServed 应未 +1"),
            JailTurnsAfterUseCard, JailTurnsBeforeUseCard);

        // 断言 ② 移交
        TestNotEqual(
            TEXT("TC-39 UseCard ②: RunAiJailAction 后应已移交"),
            Sub->GetCurrentActivePlayerId(), JailPlayerId);

        GssAiHooksTestHelpers::DestroyGameWorld(World);
    }

    // ----- RollDouble 留狱待掷路径 -----
    {
        UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC39_RollDouble_World"));
        if (!TestNotNull(TEXT("TC-39 RollDouble: World 应能创建"), World)) return false;

        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("TC-39 RollDouble: Subsystem 应能取得"), Sub))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
        const int32 JailPlayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
        if (!TestNotEqual(TEXT("TC-39 RollDouble: 应找到先手玩家"), JailPlayerId, -1))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        GssAiHooksTestHelpers::AdvanceToJailTurn(Sub, JailPlayerId);

        TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
        Spy->JailActionToReturn = EJailAction::RollDouble;
        Sub->SetAIDecisionMaker(Spy);

        FGameStateSnapshot Snap;
        Snap.SelfPlayerId    = JailPlayerId;
        Snap.SelfCash        = 1500;
        Snap.JailBailAmount  = 50;
        Snap.bHasJailCard    = false;
        Snap.JailTurnsServed = 0;

        Sub->RunAiJailAction(JailPlayerId, Snap);

        // 断言 ① JailTurnsServed == 1（留狱 +1）
        URentoPlayerState* PS = Sub->FindPlayerById(JailPlayerId);
        const int32 JailTurnsAfterRollDouble = PS ? PS->JailTurnsServed : -1;
        TestEqual(
            TEXT("TC-39 RollDouble ①: 留狱路径 JailTurnsServed 应 +1（== 1）"),
            JailTurnsAfterRollDouble, 1);

        // 断言 ② 移交
        TestNotEqual(
            TEXT("TC-39 RollDouble ②: RunAiJailAction 后应已移交"),
            Sub->GetCurrentActivePlayerId(), JailPlayerId);

        GssAiHooksTestHelpers::DestroyGameWorld(World);
    }

    return true;
}

// =============================================================================
// TC-39b-1（AC-5 / AC-39b）— PayBail 现金不足 → 降级留狱，Cash 不变
//
// GIVEN：2 人对局，jail player = 先手，Cash=10，JailBailAmount=50（不足）
//        注入 spy JailActionToReturn=PayBail
// WHEN：RunAiJailAction(jailPlayerId, Snapshot{SelfCash=10, JailBailAmount=50})
// THEN：① JailTurnsServed == 1（降级留狱 +1）
//       ② PlayerState->Cash == 10（未扣成负现金，AC-39b①）
//       ③ 行动权已移交
//
// 非 vacuous：
//   若框架误判 PayBail 可行 → AdvanceFromJailTurn(false) → JailTurnsServed==0 → ① FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC39b1_PayBailInsufficientFunds,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC39b1_PayBailInsufficientFunds",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC39b1_PayBailInsufficientFunds::RunTest(const FString& Parameters)
{
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC39b1_World"));
    if (!TestNotNull(TEXT("TC-39b-1: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-39b-1: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-39b-1: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 JailPlayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-39b-1: 应找到先手玩家"), JailPlayerId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 推进到 JailTurn
    const bool bReached = GssAiHooksTestHelpers::AdvanceToJailTurn(Sub, JailPlayerId);
    if (!TestTrue(TEXT("TC-39b-1: 应成功进入 JailTurn"), bReached))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 设置 PlayerState->Cash = 10（与 snapshot 一致，测试真实现金未被扣）
    URentoPlayerState* PS = Sub->FindPlayerById(JailPlayerId);
    if (PS)
    {
        PS->Cash = 10;
    }

    // 注入 spy：PayBail 但现金不足
    TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
    Spy->JailActionToReturn = EJailAction::PayBail;
    Sub->SetAIDecisionMaker(Spy);

    // 构造 mock snapshot：Cash 不足（10 < 50）
    FGameStateSnapshot Snap;
    Snap.SelfPlayerId    = JailPlayerId;
    Snap.SelfCash        = 10;    // 现金不足
    Snap.JailBailAmount  = 50;    // 保释金 50
    Snap.bHasJailCard    = false;
    Snap.JailTurnsServed = 0;

    // 执行
    Sub->RunAiJailAction(JailPlayerId, Snap);

    // 断言 ① JailTurnsServed == 1（降级留狱 +1）
    const int32 JailTurnsAfter = PS ? PS->JailTurnsServed : -1;
    TestEqual(
        TEXT("TC-39b-1 ①: PayBail 不足应降级留狱，JailTurnsServed 应 +1（== 1）"),
        JailTurnsAfter, 1);

    // 断言 ② Cash 未变（本系统从不扣款，AC-39b①）
    const int32 CashAfter = PS ? PS->Cash : -1;
    TestEqual(
        TEXT("TC-39b-1 ②: 降级路径 PlayerState->Cash 应未变（== 10，未扣成负现金，AC-39b①）"),
        CashAfter, 10);

    // 断言 ③ 移交
    TestNotEqual(
        TEXT("TC-39b-1 ③: RunAiJailAction 后应已移交"),
        Sub->GetCurrentActivePlayerId(), JailPlayerId);

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-39b-2（AC-5 / AC-39b）— UseCard 无卡 → 降级留狱
//
// GIVEN：2 人对局，jail player = 先手，bHasJailCard=false
//        注入 spy JailActionToReturn=UseCard
// WHEN：RunAiJailAction(jailPlayerId, Snapshot{bHasJailCard=false})
// THEN：① JailTurnsServed == 1（降级留狱 +1）
//       ② 行动权已移交
//       注：出狱卡持有数未变=结构性保证（player-turn 无卡存储，owner=事件格7）
//
// 非 vacuous：
//   若框架误判 UseCard 可行 → AdvanceFromJailTurn(false) → JailTurnsServed==0 → ① FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC39b2_UseCardNoCard,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC39b2_UseCardNoCard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC39b2_UseCardNoCard::RunTest(const FString& Parameters)
{
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC39b2_World"));
    if (!TestNotNull(TEXT("TC-39b-2: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-39b-2: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-39b-2: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 JailPlayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-39b-2: 应找到先手玩家"), JailPlayerId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 推进到 JailTurn
    const bool bReached = GssAiHooksTestHelpers::AdvanceToJailTurn(Sub, JailPlayerId);
    if (!TestTrue(TEXT("TC-39b-2: 应成功进入 JailTurn"), bReached))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 注入 spy：UseCard 但无卡
    TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
    Spy->JailActionToReturn = EJailAction::UseCard;
    Sub->SetAIDecisionMaker(Spy);

    // 构造 mock snapshot：无出狱卡
    FGameStateSnapshot Snap;
    Snap.SelfPlayerId    = JailPlayerId;
    Snap.SelfCash        = 1500;
    Snap.JailBailAmount  = 50;
    Snap.bHasJailCard    = false;   // 无卡
    Snap.JailTurnsServed = 0;

    // 执行
    Sub->RunAiJailAction(JailPlayerId, Snap);

    // 断言 ① JailTurnsServed == 1（降级留狱 +1）
    URentoPlayerState* PS = Sub->FindPlayerById(JailPlayerId);
    const int32 JailTurnsAfter = PS ? PS->JailTurnsServed : -1;
    TestEqual(
        TEXT("TC-39b-2 ①: UseCard 无卡应降级留狱，JailTurnsServed 应 +1（== 1）"),
        JailTurnsAfter, 1);

    // 断言 ② 移交
    TestNotEqual(
        TEXT("TC-39b-2 ②: RunAiJailAction 后应已移交"),
        Sub->GetCurrentActivePlayerId(), JailPlayerId);

    // 注释：出狱卡持有数未变=结构性保证（player-turn 无卡存储，owner=事件格7，AC-39b②）

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-42a（AC-6 / AC-42）— 抵押可行，后续建房不可行 → 被跳过
//
// GIVEN：2 人对局，推进到 PostRollAction
//        actions = [{MortgageProperty, 5}, {BuildHouse, 5}]
//        validator FeasibilityByCallOrder = {true, false}（抵押可行，建房不可行）
// WHEN：RunAiPostRollActions(ActiveBefore, Snapshot)
// THEN：① LastExecutedActions.Num() == 1（建房被跳过）
//       ② LastExecutedActions[0].ActionType == MortgageProperty（被跳过的建房不在执行记录）
//       ③ IsActionFeasibleCallCount == 2（两动作都重校验）
//       ④ GetCurrentActivePlayerId() != ActiveBefore（EndTurn 移交）
//
// 非 vacuous（注释）：若把 validator 改为全 true，建房也进 LastExecutedActions →
//   Num()==2 → ① FAIL；ActionType[0] 可能不同 → ② FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC42a_MortgageFeasibleBuildSkipped,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC42a_MortgageFeasibleBuildSkipped",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC42a_MortgageFeasibleBuildSkipped::RunTest(const FString& Parameters)
{
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC42a_World"));
    if (!TestNotNull(TEXT("TC-42a: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-42a: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-42a: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 FirstId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-42a: 应找到先手玩家"), FirstId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 推进到 PostRollAction
    const bool bReached = GssAiHooksTestHelpers::AdvanceToPostRollAction(Sub, FirstId);
    if (!TestTrue(TEXT("TC-42a: 应成功推进到 PostRollAction"), bReached))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

    // 注入 AIDecisionMakerSpy：2 条动作（MortgageProperty + BuildHouse）
    TSharedPtr<FAIDecisionMakerSpy> AISpy = MakeShared<FAIDecisionMakerSpy>();
    FTurnAction Act1;
    Act1.ActionType      = EAIActionType::MortgageProperty;
    Act1.TargetTileIndex = 5;
    FTurnAction Act2;
    Act2.ActionType      = EAIActionType::BuildHouse;
    Act2.TargetTileIndex = 5;
    AISpy->PostRollActionsToReturn.Add(Act1);
    AISpy->PostRollActionsToReturn.Add(Act2);
    Sub->SetAIDecisionMaker(AISpy);

    // 注入 FActionValidatorSpy：{true, false}（抵押可行，建房不可行）
    TSharedPtr<FActionValidatorSpy> ValSpy = MakeShared<FActionValidatorSpy>();
    ValSpy->FeasibilityByCallOrder.Add(true);    // 第1调用：MortgageProperty 可行
    ValSpy->FeasibilityByCallOrder.Add(false);   // 第2调用：BuildHouse 不可行
    Sub->SetActionValidator(ValSpy);

    // 构造 mock snapshot
    FGameStateSnapshot Snap;
    Snap.SelfPlayerId = ActiveBefore;
    Snap.SelfCash     = 1500;

    // 执行
    Sub->RunAiPostRollActions(ActiveBefore, Snap);

    // 断言 ① LastExecutedActions.Num() == 1（建房被跳过）
    TestEqual(
        TEXT("TC-42a ①: LastExecutedActions.Num() 应 == 1（建房被跳过）"),
        Sub->GetLastExecutedActions().Num(), 1);

    // 断言 ② LastExecutedActions[0].ActionType == MortgageProperty
    if (Sub->GetLastExecutedActions().Num() >= 1)
    {
        TestEqual(
            TEXT("TC-42a ②: LastExecutedActions[0].ActionType 应为 MortgageProperty（建房不在执行记录）"),
            Sub->GetLastExecutedActions()[0].ActionType, EAIActionType::MortgageProperty);
    }

    // 断言 ③ IsActionFeasibleCallCount == 2（两动作都重校验）
    TestEqual(
        TEXT("TC-42a ③: IsActionFeasibleCallCount 应 == 2（两动作均经重校验）"),
        ValSpy->IsActionFeasibleCallCount, 2);

    // 断言 ④ EndTurn 移交
    TestNotEqual(
        TEXT("TC-42a ④: EndTurn 后 GetCurrentActivePlayerId 应已移交"),
        Sub->GetCurrentActivePlayerId(), ActiveBefore);

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-42b（AC-6 / AC-42）— 第2建房不可行被跳过，后续合法 Mortgage 仍执行
//
// GIVEN：2 人对局，推进到 PostRollAction
//        actions = [{BuildHouse, 5}, {BuildHouse, 5}, {MortgageProperty, 7}]
//        validator FeasibilityByCallOrder = {true, false, true}
//          （第1建房可行、第2建房不可行、抵押可行）
// WHEN：RunAiPostRollActions(ActiveBefore, Snapshot)
// THEN：① LastExecutedActions.Num() == 2（第2建房被跳过）
//       ② LastExecutedActions[0].ActionType == BuildHouse
//          LastExecutedActions[1].ActionType == MortgageProperty（第2建房被跳过，后续 Mortgage 仍执行，AC-42②）
//       ③ IsActionFeasibleCallCount == 3（三动作均重校验）
//       ④ EndTurn 移交
//
// 非 vacuous（注释）：若把 validator 改为全 true，三条都执行 → Num()==3 → ① FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC42b_SecondBuildSkippedMortgageStillExecutes,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC42b_SecondBuildSkippedMortgageStillExecutes",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC42b_SecondBuildSkippedMortgageStillExecutes::RunTest(const FString& Parameters)
{
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC42b_World"));
    if (!TestNotNull(TEXT("TC-42b: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-42b: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-42b: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 FirstId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-42b: 应找到先手玩家"), FirstId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 推进到 PostRollAction
    const bool bReached = GssAiHooksTestHelpers::AdvanceToPostRollAction(Sub, FirstId);
    if (!TestTrue(TEXT("TC-42b: 应成功推进到 PostRollAction"), bReached))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 ActiveBefore = Sub->GetCurrentActivePlayerId();

    // 注入 AIDecisionMakerSpy：3 条动作（BuildHouse + BuildHouse + MortgageProperty）
    TSharedPtr<FAIDecisionMakerSpy> AISpy = MakeShared<FAIDecisionMakerSpy>();
    FTurnAction ActBuild1;
    ActBuild1.ActionType      = EAIActionType::BuildHouse;
    ActBuild1.TargetTileIndex = 5;
    FTurnAction ActBuild2;
    ActBuild2.ActionType      = EAIActionType::BuildHouse;
    ActBuild2.TargetTileIndex = 5;
    FTurnAction ActMortgage;
    ActMortgage.ActionType      = EAIActionType::MortgageProperty;
    ActMortgage.TargetTileIndex = 7;
    AISpy->PostRollActionsToReturn.Add(ActBuild1);
    AISpy->PostRollActionsToReturn.Add(ActBuild2);
    AISpy->PostRollActionsToReturn.Add(ActMortgage);
    Sub->SetAIDecisionMaker(AISpy);

    // 注入 FActionValidatorSpy：{true, false, true}
    TSharedPtr<FActionValidatorSpy> ValSpy = MakeShared<FActionValidatorSpy>();
    ValSpy->FeasibilityByCallOrder.Add(true);    // 第1调用：BuildHouse#1 可行
    ValSpy->FeasibilityByCallOrder.Add(false);   // 第2调用：BuildHouse#2 不可行
    ValSpy->FeasibilityByCallOrder.Add(true);    // 第3调用：MortgageProperty 可行
    Sub->SetActionValidator(ValSpy);

    // 构造 mock snapshot
    FGameStateSnapshot Snap;
    Snap.SelfPlayerId = ActiveBefore;
    Snap.SelfCash     = 1500;

    // 执行
    Sub->RunAiPostRollActions(ActiveBefore, Snap);

    // 断言 ① LastExecutedActions.Num() == 2（第2建房被跳过）
    TestEqual(
        TEXT("TC-42b ①: LastExecutedActions.Num() 应 == 2（第2建房被跳过）"),
        Sub->GetLastExecutedActions().Num(), 2);

    if (Sub->GetLastExecutedActions().Num() >= 2)
    {
        // 断言 ② [0]=BuildHouse，[1]=MortgageProperty（后续 Mortgage 仍执行，AC-42②）
        TestEqual(
            TEXT("TC-42b ②-a: LastExecutedActions[0].ActionType 应为 BuildHouse"),
            Sub->GetLastExecutedActions()[0].ActionType, EAIActionType::BuildHouse);
        TestEqual(
            TEXT("TC-42b ②-b: LastExecutedActions[1].ActionType 应为 MortgageProperty（第2建房跳过后续仍执行，AC-42②）"),
            Sub->GetLastExecutedActions()[1].ActionType, EAIActionType::MortgageProperty);
    }

    // 断言 ③ IsActionFeasibleCallCount == 3（三动作均重校验）
    TestEqual(
        TEXT("TC-42b ③: IsActionFeasibleCallCount 应 == 3（三动作均经重校验）"),
        ValSpy->IsActionFeasibleCallCount, 3);

    // 断言 ④ EndTurn 移交
    TestNotEqual(
        TEXT("TC-42b ④: EndTurn 后 GetCurrentActivePlayerId 应已移交"),
        Sub->GetCurrentActivePlayerId(), ActiveBefore);

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-45（AC-10 / AC-45，Alpha）— ResolveAuctionBid 纯函数 sentinel 值域
//
// 无 World，表驱动用例，纯静态函数调用。
//
// 覆盖场景（共 7 例）：
//   负数 → 放弃；INDEX_NONE(-1) → 放弃；0 → 放弃；
//   合法出价 → 返回原值；≤当前最高价 → 放弃；>Cash → 放弃；<最低加价步长 → 放弃
//
// 非 vacuous：每例期望值互异且含合法/各类非法分量，合法例 expected != 0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC45_ResolveAuctionBid,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC45_ResolveAuctionBid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC45_ResolveAuctionBid::RunTest(const FString& Parameters)
{
    // 表驱动用例（GDD AC-45，纯函数无 World 依赖）
    // 列：RawBid / CurrentHighest / MinIncrement / Cash / Expected / 说明

    // 用例1：负数 → 放弃（0）
    TestEqual(
        TEXT("TC-45 [1]: 负数 RawBid=-5 → 放弃（0）"),
        UPlayerTurnSubsystem::ResolveAuctionBid(-5, 50, 10, 200), 0);

    // 用例2：INDEX_NONE(-1) 哨兵 → 放弃（0）
    TestEqual(
        TEXT("TC-45 [2]: INDEX_NONE(-1) 哨兵 → 放弃（0）"),
        UPlayerTurnSubsystem::ResolveAuctionBid(INDEX_NONE, 50, 10, 200), 0);

    // 用例3：0 非法 → 放弃（0）
    TestEqual(
        TEXT("TC-45 [3]: RawBid=0 非法（不作出价0解）→ 放弃（0）"),
        UPlayerTurnSubsystem::ResolveAuctionBid(0, 50, 10, 200), 0);

    // 用例4：合法出价（100 > 50 且 ≤200 且 ≥10）→ 返回 100
    TestEqual(
        TEXT("TC-45 [4]: 合法出价 RawBid=100（>50 且 ≤200 且 ≥10）→ 100"),
        UPlayerTurnSubsystem::ResolveAuctionBid(100, 50, 10, 200), 100);

    // 用例5：≤当前最高价（40 ≤ 50）→ 放弃（0）
    TestEqual(
        TEXT("TC-45 [5]: RawBid=40 ≤ CurrentHighest=50 → 放弃（0）"),
        UPlayerTurnSubsystem::ResolveAuctionBid(40, 50, 10, 200), 0);

    // 用例6：>Cash（300 > 200）→ 放弃（0）
    TestEqual(
        TEXT("TC-45 [6]: RawBid=300 > Cash=200 → 放弃（0）"),
        UPlayerTurnSubsystem::ResolveAuctionBid(300, 50, 10, 200), 0);

    // 用例7：< 最低加价步长（5 < 10）→ 放弃（0）
    TestEqual(
        TEXT("TC-45 [7]: RawBid=5 < MinIncrement=10 → 放弃（0）"),
        UPlayerTurnSubsystem::ResolveAuctionBid(5, 0, 10, 200), 0);

    return true;
}

// =============================================================================
// TC-2（AC-2）— AssembleSnapshot：全盘装配、PreaggregatedNlv、Rent_top1/top2、值语义
//
// GIVEN：注入 3 spy；BoardToReturn = 4 格：
//   tile0：AI 自有，未抵押，MortgageValue=100，HouseCount=2，BuildingCost=100
//   tile1：对手 ownerB，未抵押（Rent_top 候选）
//   tile2：对手 ownerB，已抵押（Rent_top 不计）
//   tile3：对手 ownerB，未抵押（Rent_top 候选）
//   RentSequence={30,50}（tile1→30，tile3→50，tile2 抵押跳过）
// WHEN：AssembleSnapshot(AiId)
// THEN：
//   ① S.Tiles.Num()==4
//   ② tile0.PreaggregatedNlv == 2*(100/2)+100 == 200（AI 自有未抵押）
//   ③ S.Rent_top1==50 && S.Rent_top2==30（降序前两高）
//   ④ GetBoardOwnershipCallCount==1（一次性装配）
//   ⑤ 值语义冻结：拷贝 Copy=S，改 Copy.Rent_top1=999，S.Rent_top1 仍==50
//
// 非 vacuous：
//   ② 若 PreaggregatedNlv 未装配（留 0）→ FAIL
//   ③ 若 Rent_top 未降序取前二 → FAIL
//   ④ 若逐格 round-trip 而非一次 GetBoardOwnership → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC2_AssembleSnapshot,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC2_AssembleSnapshot",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC2_AssembleSnapshot::RunTest(const FString& Parameters)
{
    // ---- 创建 World + 初始化（AssembleSnapshot 是 const 方法，需要 FindPlayerById 有 PlayerState）----
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC2_World"));
    if (!TestNotNull(TEXT("TC-2: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-2: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 2 人对局，AiId = TurnOrderIndex==0 的玩家
    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-2: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const int32 AiId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-2: 应找到先手 AiId"), AiId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // ---- 构造 spy + 配置 BoardToReturn（4 格）----
    const int32 OpponentId = 99; // 对手 ID（不是 AiId）

    // tile0：AI 自有，未抵押，MortgageValue=100，HouseCount=2，BuildingCost=100
    FOwnershipSnapshot OS0;
    OS0.TileIndex     = 0;
    OS0.OwnerId       = AiId;
    OS0.bIsMortgaged  = false;
    OS0.bIsMonopoly   = false;
    OS0.MortgageValue = 100;
    OS0.PurchasePrice = 200;

    // tile1：对手未抵押（Rent_top 候选1）
    FOwnershipSnapshot OS1;
    OS1.TileIndex    = 1;
    OS1.OwnerId      = OpponentId;
    OS1.bIsMortgaged = false;

    // tile2：对手已抵押（Rent_top 不计）
    FOwnershipSnapshot OS2;
    OS2.TileIndex    = 2;
    OS2.OwnerId      = OpponentId;
    OS2.bIsMortgaged = true;

    // tile3：对手未抵押（Rent_top 候选2）
    FOwnershipSnapshot OS3;
    OS3.TileIndex    = 3;
    OS3.OwnerId      = OpponentId;
    OS3.bIsMortgaged = false;

    TSharedPtr<FOwnershipProviderSpy> OwnerSpy = MakeShared<FOwnershipProviderSpy>();
    OwnerSpy->BoardToReturn.Add(OS0);
    OwnerSpy->BoardToReturn.Add(OS1);
    OwnerSpy->BoardToReturn.Add(OS2);
    OwnerSpy->BoardToReturn.Add(OS3);

    // BuildingProvider：tile0 HouseCount=2, BuildingCost=100；其余缺省 0
    TSharedPtr<FBuildingProviderSpy> BuildSpy = MakeShared<FBuildingProviderSpy>();
    BuildSpy->HouseCountByTile.Add(0, 2);
    BuildSpy->BuildingCostByTile.Add(0, 100);

    // EconomyResolver：RentSequence={30, 50}（tile1 → 30，tile3 → 50；tile2 抵押跳过）
    TSharedPtr<FEconomyResolverSpy> EconSpy = MakeShared<FEconomyResolverSpy>();
    EconSpy->RentSequence.Add(30);  // 第1次 CalculateRent → tile1 → 30
    EconSpy->RentSequence.Add(50);  // 第2次 CalculateRent → tile3 → 50

    Sub->SetOwnershipProvider(OwnerSpy);
    Sub->SetBuildingProvider(BuildSpy);
    Sub->SetEconomyResolver(EconSpy);

    // ---- 执行 ----
    FGameStateSnapshot S = Sub->AssembleSnapshot(AiId);

    // ==== 断言 ====

    // ① Tiles.Num()==4（全盘 4 格）
    TestEqual(TEXT("TC-2 ①: S.Tiles.Num() 应==4"), S.Tiles.Num(), 4);

    // ② tile0 的 PreaggregatedNlv == 2*(100/2)+100 == 200
    //    公式：AI 自有未抵押 = HouseCount*(BuildingCost/2) + MortgageValue = 2*50+100 = 200
    bool bFoundTile0 = false;
    for (const FTileSnapshotEntry& E : S.Tiles)
    {
        if (E.TileIndex == 0)
        {
            bFoundTile0 = true;
            TestEqual(
                TEXT("TC-2 ②: tile0.PreaggregatedNlv 应==200（AI 自有 HouseCount=2 BuildingCost=100 MV=100）"),
                E.PreaggregatedNlv, 200);
            break;
        }
    }
    TestTrue(TEXT("TC-2 ②: 应在 Tiles 中找到 tile0"), bFoundTile0);

    // ③ Rent_top1==50，Rent_top2==30（降序前两高）
    TestEqual(TEXT("TC-2 ③-a: S.Rent_top1 应==50"), S.Rent_top1, 50);
    TestEqual(TEXT("TC-2 ③-b: S.Rent_top2 应==30"), S.Rent_top2, 30);

    // ④ GetBoardOwnershipCallCount==1（一次性全盘装配，不逐格 round-trip）
    TestEqual(
        TEXT("TC-2 ④: GetBoardOwnershipCallCount 应==1（一次性装配，ADR-0006 IG#3）"),
        OwnerSpy->GetBoardOwnershipCallCount, 1);

    // ⑤ 值语义冻结：拷贝 Copy=S，改 Copy.Rent_top1=999，S.Rent_top1 仍==50
    FGameStateSnapshot Copy = S;
    Copy.Rent_top1 = 999;
    TestEqual(
        TEXT("TC-2 ⑤: 改 Copy.Rent_top1=999 后，S.Rent_top1 应仍==50（值语义不受影响）"),
        S.Rent_top1, 50);

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4a（AC-4）— RunAiBuyDecision 路由：true→执行购买 / false→不执行
//
// 场景1（买）：BuyDecisionToReturn=true，ExecutePurchaseResult=true
//   断言：R==true，WasLastPurchaseExecuted==true，DecideBuyPropertyCallCount==1，ExecutePurchaseCallCount==1
// 场景2（不买）：BuyDecisionToReturn=false
//   断言：R==false，ExecutePurchaseCallCount==0，DecideBuyPropertyCallCount==1
//
// 非 vacuous：若框架忽略 DecideBuyProperty 返回值恒执行 → 场景2 ExecutePurchaseCallCount==0 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC4a_BuyDecisionRouting,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC4a_BuyDecisionRouting",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC4a_BuyDecisionRouting::RunTest(const FString& Parameters)
{
    // ---- 场景1（买）----
    {
        UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC4a_Buy_World"));
        if (!TestNotNull(TEXT("TC-4a 买: World 应能创建"), World)) return false;

        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("TC-4a 买: Subsystem 应能取得"), Sub))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
        const int32 AiId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
        if (!TestNotEqual(TEXT("TC-4a 买: 应找到先手"), AiId, -1))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // 注入 3 spy（AssembleSnapshot 用空 Board，只验路由）
        TSharedPtr<FOwnershipProviderSpy> OwnerSpy  = MakeShared<FOwnershipProviderSpy>();
        TSharedPtr<FBuildingProviderSpy>  BuildSpy   = MakeShared<FBuildingProviderSpy>();
        TSharedPtr<FEconomyResolverSpy>   EconSpy    = MakeShared<FEconomyResolverSpy>();
        EconSpy->ExecutePurchaseResult = true; // 执行可行
        Sub->SetOwnershipProvider(OwnerSpy);
        Sub->SetBuildingProvider(BuildSpy);
        Sub->SetEconomyResolver(EconSpy);

        // 注入 AI spy：决定买
        TSharedPtr<FAIDecisionMakerSpy> AiSpy = MakeShared<FAIDecisionMakerSpy>();
        AiSpy->BuyDecisionToReturn = true;
        Sub->SetAIDecisionMaker(AiSpy);

        // 执行
        const bool R = Sub->RunAiBuyDecision(AiId, 5);

        // 断言
        TestTrue(TEXT("TC-4a 买: R 应==true（决定买且执行可行）"), R);
        TestTrue(TEXT("TC-4a 买: WasLastPurchaseExecuted 应==true"), Sub->WasLastPurchaseExecuted());
        TestEqual(TEXT("TC-4a 买: DecideBuyPropertyCallCount 应==1"), AiSpy->DecideBuyPropertyCallCount, 1);
        TestEqual(TEXT("TC-4a 买: ExecutePurchaseCallCount 应==1"), EconSpy->ExecutePurchaseCallCount, 1);

        GssAiHooksTestHelpers::DestroyGameWorld(World);
    }

    // ---- 场景2（不买）----
    {
        UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC4a_NoBuy_World"));
        if (!TestNotNull(TEXT("TC-4a 不买: World 应能创建"), World)) return false;

        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("TC-4a 不买: Subsystem 应能取得"), Sub))
        {
            GssAiHooksTestHelpers::DestroyGameWorld(World);
            return false;
        }

        Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
        const int32 AiId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);

        // 注入 3 spy
        TSharedPtr<FOwnershipProviderSpy> OwnerSpy  = MakeShared<FOwnershipProviderSpy>();
        TSharedPtr<FBuildingProviderSpy>  BuildSpy   = MakeShared<FBuildingProviderSpy>();
        TSharedPtr<FEconomyResolverSpy>   EconSpy    = MakeShared<FEconomyResolverSpy>();
        Sub->SetOwnershipProvider(OwnerSpy);
        Sub->SetBuildingProvider(BuildSpy);
        Sub->SetEconomyResolver(EconSpy);

        // 注入 AI spy：决定不买
        TSharedPtr<FAIDecisionMakerSpy> AiSpy = MakeShared<FAIDecisionMakerSpy>();
        AiSpy->BuyDecisionToReturn = false;
        Sub->SetAIDecisionMaker(AiSpy);

        // 执行
        const bool R = Sub->RunAiBuyDecision(AiId, 5);

        // 断言
        TestFalse(TEXT("TC-4a 不买: R 应==false（决定不买）"), R);
        TestEqual(TEXT("TC-4a 不买: ExecutePurchaseCallCount 应==0（决定不买→不调执行）"),
            EconSpy->ExecutePurchaseCallCount, 0);
        TestEqual(TEXT("TC-4a 不买: DecideBuyPropertyCallCount 应==1"), AiSpy->DecideBuyPropertyCallCount, 1);

        GssAiHooksTestHelpers::DestroyGameWorld(World);
    }

    return true;
}

// =============================================================================
// TC-4b（AC-38b）— 决定买但执行期不可行 → 视同不买
//
// GIVEN：BuyDecisionToReturn=true；EconomyResolverSpy.ExecutePurchaseResult=false（模拟 Cash<Price/已被买）
// WHEN：RunAiBuyDecision(AiId, 5)
// THEN：
//   ① R==false（视同不买）
//   ② WasLastPurchaseExecuted()==false
//   ③ ExecutePurchaseCallCount==1（委派了执行期校验）
//   ④ 回合不崩（执行到此即证）
//
// 非 vacuous：若框架不校验 ExecutePurchase 结果、把"决定买"当"已买" → R==true FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC4b_BuyInfeasibleTreatedAsNoBuy,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC4b_BuyInfeasibleTreatedAsNoBuy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC4b_BuyInfeasibleTreatedAsNoBuy::RunTest(const FString& Parameters)
{
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC4b_World"));
    if (!TestNotNull(TEXT("TC-4b: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-4b: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    const int32 AiId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-4b: 应找到先手"), AiId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 注入 3 provider spy
    TSharedPtr<FOwnershipProviderSpy> OwnerSpy  = MakeShared<FOwnershipProviderSpy>();
    TSharedPtr<FBuildingProviderSpy>  BuildSpy   = MakeShared<FBuildingProviderSpy>();
    TSharedPtr<FEconomyResolverSpy>   EconSpy    = MakeShared<FEconomyResolverSpy>();
    EconSpy->ExecutePurchaseResult = false; // 执行期不可行（模拟 Cash<Price 或已被买）
    Sub->SetOwnershipProvider(OwnerSpy);
    Sub->SetBuildingProvider(BuildSpy);
    Sub->SetEconomyResolver(EconSpy);

    // 注入 AI spy：决定买
    TSharedPtr<FAIDecisionMakerSpy> AiSpy = MakeShared<FAIDecisionMakerSpy>();
    AiSpy->BuyDecisionToReturn = true;
    Sub->SetAIDecisionMaker(AiSpy);

    // 执行
    const bool R = Sub->RunAiBuyDecision(AiId, 5);

    // 断言 ① R==false（视同不买，AC-38b）
    TestFalse(TEXT("TC-4b ①: R 应==false（执行期不可行视同不买，AC-38b）"), R);

    // 断言 ② WasLastPurchaseExecuted==false
    TestFalse(TEXT("TC-4b ②: WasLastPurchaseExecuted 应==false（未购买）"), Sub->WasLastPurchaseExecuted());

    // 断言 ③ ExecutePurchaseCallCount==1（委派了执行期校验）
    TestEqual(
        TEXT("TC-4b ③: ExecutePurchaseCallCount 应==1（委派了执行期校验，AC-38b）"),
        EconSpy->ExecutePurchaseCallCount, 1);

    // 断言 ④ 回合不崩（执行到此即证——若崩则测试框架已 FAIL）
    // （隐式：测试运行到此处说明未崩溃，符合 AC-38b 要求）

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-7（AC-7）— SettleRentOnArrival 算租聚合恰 1 次链 + dice_total PULL
//
// GIVEN：2 人对局，StartTurn(PayerId) 使其为当前行动玩家
//        直接设 PS->CurrentRollContext.Total=8（dice_total PULL 路径）
//        OwnershipProviderSpy.PerTileToReturn={7: {OwnerId=对手, bIsMortgaged=false}}
//        BuildingProviderSpy.HouseCountByTile 空 → GetHouseCount(7) 返回缺省 0（AC-49）
//        EconomyResolverSpy.RentToReturn=75
// WHEN：SettleRentOnArrival(PayerId, 7)
// THEN：
//   ① BuildOwnershipSnapshotCallCount==1 且 LastBuildOwnershipTile==7
//   ② GetHouseCountCallCount>=1（house_count 读取，缺省 0）
//   ③ CalculateRentCallCount==1（算租调用恰 1 次）
//   ④ LastRentInput.HouseCount==0（AC-49 缺省），DiceTotal==8（PULL 正确），TileIndex==7，PayerId==PayerId
//   ⑤ Rent==75 且 GetLastRentCharged()==75
//
// 非 vacuous：
//   ④ 若 dice_total 未 PULL（留 0）→ DiceTotal==8 FAIL
//   ③ 若算租调多次 → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC7_RentAggregation,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC7_RentAggregation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC7_RentAggregation::RunTest(const FString& Parameters)
{
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC7_World"));
    if (!TestNotNull(TEXT("TC-7: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-7: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-7: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 找付租方（先手玩家）
    const int32 PayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-7: 应找到先手 PayerId"), PayerId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // StartTurn 使 PayerId 成为当前行动玩家（CurrentActivePlayerId = PayerId）
    URentoPlayerState* PS = Sub->FindPlayerById(PayerId);
    if (PS) { PS->bIsInJail = false; PS->ConsecutiveDoubles = 0; }
    Sub->StartTurn(PayerId);

    // 直接设 CurrentRollContext.Total=8（dice_total PULL 最简路径，brief TC-7 指定）
    if (PS)
    {
        PS->CurrentRollContext.Total = 8;
    }

    // 确认 PayerId == GetCurrentActivePlayerId（GetCurrentRollTotal PULL 的前提）
    TestEqual(TEXT("TC-7: 前提—CurrentActivePlayerId 应==PayerId"),
        Sub->GetCurrentActivePlayerId(), PayerId);

    // 注入 3 provider spy
    TSharedPtr<FOwnershipProviderSpy> OwnerSpy  = MakeShared<FOwnershipProviderSpy>();
    TSharedPtr<FBuildingProviderSpy>  BuildSpy   = MakeShared<FBuildingProviderSpy>();
    TSharedPtr<FEconomyResolverSpy>   EconSpy    = MakeShared<FEconomyResolverSpy>();

    // OwnershipProviderSpy.PerTileToReturn={7: 对手未抵押}
    const int32 OpponentId = 88;
    FOwnershipSnapshot OS7;
    OS7.TileIndex    = 7;
    OS7.OwnerId      = OpponentId;
    OS7.bIsMortgaged = false;
    OwnerSpy->PerTileToReturn.Add(7, OS7);

    // BuildingProvider：HouseCountByTile 空 → tile7 返回缺省 0（AC-49）
    // （不需要额外设置）

    // EconomyResolver：固定返回 75
    EconSpy->RentToReturn = 75;

    Sub->SetOwnershipProvider(OwnerSpy);
    Sub->SetBuildingProvider(BuildSpy);
    Sub->SetEconomyResolver(EconSpy);

    // 执行
    const int32 Rent = Sub->SettleRentOnArrival(PayerId, 7);

    // ==== 断言 ====

    // ① BuildOwnershipSnapshotCallCount==1 且 LastBuildOwnershipTile==7
    TestEqual(
        TEXT("TC-7 ①-a: BuildOwnershipSnapshotCallCount 应==1（恰 1 次，CR-3.2 ①）"),
        OwnerSpy->BuildOwnershipSnapshotCallCount, 1);
    TestEqual(
        TEXT("TC-7 ①-b: LastBuildOwnershipTile 应==7"),
        OwnerSpy->LastBuildOwnershipTile, 7);

    // ② GetHouseCountCallCount>=1（house_count 读取）
    TestTrue(
        TEXT("TC-7 ②: GetHouseCountCallCount 应>=1（house_count 读取，AC-49 缺省 0）"),
        BuildSpy->GetHouseCountCallCount >= 1);

    // ③ CalculateRentCallCount==1（算租调用恰 1 次，CR-3.2 ③）
    TestEqual(
        TEXT("TC-7 ③: CalculateRentCallCount 应==1（算租恰 1 次，CR-3.2 ③）"),
        EconSpy->CalculateRentCallCount, 1);

    // ④ LastRentInput 参数验证
    TestEqual(
        TEXT("TC-7 ④-a: LastRentInput.HouseCount 应==0（AC-49 缺省）"),
        EconSpy->LastRentInput.HouseCount, 0);
    TestEqual(
        TEXT("TC-7 ④-b: LastRentInput.DiceTotal 应==8（PULL 当前程正确，CR-3.1）"),
        EconSpy->LastRentInput.DiceTotal, 8);
    TestEqual(
        TEXT("TC-7 ④-c: LastRentInput.TileIndex 应==7"),
        EconSpy->LastRentInput.TileIndex, 7);
    TestEqual(
        TEXT("TC-7 ④-d: LastRentInput.PayerId 应==PayerId"),
        EconSpy->LastRentInput.PayerId, PayerId);

    // ⑤ Rent==75 且 GetLastRentCharged()==75
    TestEqual(TEXT("TC-7 ⑤-a: Rent 应==75"), Rent, 75);
    TestEqual(TEXT("TC-7 ⑤-b: GetLastRentCharged() 应==75"), Sub->GetLastRentCharged(), 75);

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC8_ForcedLiquidationOrderEarlyStop（AC-8 / AC-50 / AC-51 偿付路径）
//
// GIVEN：2 人对局，InitializeFromConfig(2)；PayerId=先手；PS->Cash=100。
//   OwnershipProviderSpy.BoardToReturn = 2 格：
//     tile_empty：{TileIndex=10, OwnerId=PayerId, unmortgaged, MV=100}
//     tile_housed：{TileIndex=20, OwnerId=PayerId, unmortgaged, MV=150}
//   BuildingProviderSpy.HouseCountByTile = {10:0, 20:1}（tile_empty 无房、tile_housed 有 1 房）。
//   共享 CallLog 注入 Ownership/Building spy。
//   Cash 抬升：OwnSpy 抵押后 100→200；BldSpy 卖房后 200→300。
// WHEN：RunForcedLiquidation(PayerId, 300)
// THEN：
//   ① R==true（偿付成功）
//   ② MortgageCallCount==1 && LastMortgageTile==10（只抵押空地，带房 tile_housed 未被抵押，AC-50）
//   ③ ForcedSellCallCount==1
//   ④ CallLog==["Mortgage","ForcedSell"]（mortgage-empty-first 顺序，AC-51 ①②）
//   ⑤ PS->Cash==300（早停，AC-51 ③：到 300 即停未继续）
//
// 非 vacuous：
//   若框架不读 GetHouseCount 直接抵押 tile_housed → LastMortgageTile 可能==20 → ② FAIL
//   若顺序颠倒 → ④ FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC8_ForcedLiquidationOrderEarlyStop,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC8_ForcedLiquidationOrderEarlyStop",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC8_ForcedLiquidationOrderEarlyStop::RunTest(const FString& Parameters)
{
    // ---- 创建 World + 初始化 ----
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC8_World"));
    if (!TestNotNull(TEXT("TC-8: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-8: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-8: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 找先手玩家（PayerId）
    const int32 PayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-8: 应找到先手玩家"), PayerId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 设置初始 Cash=100
    URentoPlayerState* PS = Sub->FindPlayerById(PayerId);
    if (!TestNotNull(TEXT("TC-8: 应找到 PlayerState"), PS))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }
    PS->Cash = 100;

    // ---- 构造 spy ----
    TSharedPtr<FOwnershipProviderSpy> OwnSpy = MakeShared<FOwnershipProviderSpy>();
    TSharedPtr<FBuildingProviderSpy>  BldSpy  = MakeShared<FBuildingProviderSpy>();
    TSharedPtr<FEconomyResolverSpy>   EcoSpy  = MakeShared<FEconomyResolverSpy>();

    // OwnershipProviderSpy.BoardToReturn：tile_empty(10) + tile_housed(20)
    FOwnershipSnapshot TileEmpty;
    TileEmpty.TileIndex     = 10;
    TileEmpty.OwnerId       = PayerId;
    TileEmpty.bIsMortgaged  = false;
    TileEmpty.MortgageValue = 100;

    FOwnershipSnapshot TileHoused;
    TileHoused.TileIndex     = 20;
    TileHoused.OwnerId       = PayerId;
    TileHoused.bIsMortgaged  = false;
    TileHoused.MortgageValue = 150;

    OwnSpy->BoardToReturn.Add(TileEmpty);
    OwnSpy->BoardToReturn.Add(TileHoused);

    // BuildingProviderSpy.HouseCountByTile = {10:0, 20:1}
    BldSpy->HouseCountByTile.Add(10, 0); // tile_empty 无房
    BldSpy->HouseCountByTile.Add(20, 1); // tile_housed 有 1 房

    // 共享 CallLog（顺序断言 AC-51）
    TArray<FString> CallLog;
    OwnSpy->CallLog = &CallLog;
    BldSpy->CallLog = &CallLog;

    // 抬 Cash 设置：抵押 tile_empty 后 100→200；卖房后 200→300
    OwnSpy->CashTarget       = PS;
    OwnSpy->MortgageCashGain = 100; // 抵押后 Cash += 100
    BldSpy->CashTarget       = PS;
    BldSpy->SellCashGain     = 100; // 卖房后 Cash += 100

    Sub->SetOwnershipProvider(OwnSpy);
    Sub->SetBuildingProvider(BldSpy);
    Sub->SetEconomyResolver(EcoSpy);

    // ---- 执行 ----
    bool R = Sub->RunForcedLiquidation(PayerId, 300);

    // ==== 断言 ====

    // ① R==true（偿付成功）
    TestTrue(TEXT("TC-8 ①: RunForcedLiquidation 应返回 true（偿付成功）"), R);

    // ② MortgageCallCount==1 && LastMortgageTile==10（只抵押空地，AC-50 带房地不可抵押）
    TestEqual(TEXT("TC-8 ②-a: MortgageCallCount 应==1（只抵押空地一次）"),
        OwnSpy->MortgageCallCount, 1);
    TestEqual(TEXT("TC-8 ②-b: LastMortgageTile 应==10（tile_empty，带房 tile_housed 未被抵押，AC-50）"),
        OwnSpy->LastMortgageTile, 10);

    // ③ ForcedSellCallCount==1
    TestEqual(TEXT("TC-8 ③: ForcedSellCallCount 应==1"),
        BldSpy->ForcedSellCallCount, 1);

    // ④ CallLog==["Mortgage","ForcedSell"]（mortgage-empty-first 顺序，AC-51 ①②）
    if (TestEqual(TEXT("TC-8 ④-a: CallLog.Num() 应==2"), CallLog.Num(), 2))
    {
        TestEqual(TEXT("TC-8 ④-b: CallLog[0] 应==\"Mortgage\"（先抵押）"), CallLog[0], FString(TEXT("Mortgage")));
        TestEqual(TEXT("TC-8 ④-c: CallLog[1] 应==\"ForcedSell\"（后卖房，AC-51 ①②）"), CallLog[1], FString(TEXT("ForcedSell")));
    }

    // ⑤ PS->Cash==300（早停，到 300 即停，AC-51 ③）
    TestEqual(TEXT("TC-8 ⑤: PS->Cash 应==300（早停，Cash>=AmountDue 即停，AC-51 ③）"),
        PS->Cash, 300);

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC8b_LiquidationExhaustedInsolvent（AC-51 有界终止 → 破产）
//
// GIVEN：PS->Cash=50；AmountDue=1000。
//   BoardToReturn = 1 格 tile_empty{10, OwnerId=PayerId, unmortgaged, MV=100}；HouseCountByTile={10:0}。
//   OwnSpy CashTarget=PS, MortgageCashGain=100（50→150）。
//   BuildingProviderSpy.PlayerBuildingsToReturn 空。EconomyResolverSpy.IsInsolventResult=true。
// WHEN：RunForcedLiquidation(PayerId, 1000)
// THEN：
//   ① R==false（破产）
//   ②（econ-009 Design X 移除）破产判定改 economy 清算 spec 结构性耗尽，不再经 EcoSpy->IsInsolvent
//   ③ OwnSpy->MortgageCallCount==1（抵押了一次）
//   ④ 未死循环（执行到此即证 SafetyGuard 未触发）
//
// 非 vacuous：
//   若 SafetyGuard 缺失且 mock 不抬 Cash 会死循环——本测试 mock 抬 Cash 但仍<due，
//   靠"无资产→is_insolvent"自然终止，验证终止逻辑正确。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGssAiHooks_TC8b_LiquidationExhaustedInsolvent,
    "Rento.PlayerTurn.GameStateSnapshotAiHooks.TC8b_LiquidationExhaustedInsolvent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGssAiHooks_TC8b_LiquidationExhaustedInsolvent::RunTest(const FString& Parameters)
{
    // ---- 创建 World + 初始化 ----
    UWorld* World = GssAiHooksTestHelpers::CreateGameWorld(TEXT("TC8b_World"));
    if (!TestNotNull(TEXT("TC-8b: World 应能创建"), World)) return false;

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-8b: Subsystem 应能取得"), Sub))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const bool bInit = Sub->InitializeFromConfig(GssAiHooksTestHelpers::MakeConfig(2));
    if (!TestTrue(TEXT("TC-8b: InitializeFromConfig(P=2) 应成功"), bInit))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 找先手玩家
    const int32 PayerId = GssAiHooksTestHelpers::FindFirstPlayerId(Sub);
    if (!TestNotEqual(TEXT("TC-8b: 应找到先手玩家"), PayerId, -1))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }

    URentoPlayerState* PS = Sub->FindPlayerById(PayerId);
    if (!TestNotNull(TEXT("TC-8b: 应找到 PlayerState"), PS))
    {
        GssAiHooksTestHelpers::DestroyGameWorld(World);
        return false;
    }
    PS->Cash = 50; // 初始 Cash=50，AmountDue=1000

    // ---- 构造 spy ----
    TSharedPtr<FOwnershipProviderSpy> OwnSpy = MakeShared<FOwnershipProviderSpy>();
    TSharedPtr<FBuildingProviderSpy>  BldSpy  = MakeShared<FBuildingProviderSpy>();
    TSharedPtr<FEconomyResolverSpy>   EcoSpy  = MakeShared<FEconomyResolverSpy>();

    // BoardToReturn：1 格 tile_empty
    FOwnershipSnapshot TileEmpty;
    TileEmpty.TileIndex     = 10;
    TileEmpty.OwnerId       = PayerId;
    TileEmpty.bIsMortgaged  = false;
    TileEmpty.MortgageValue = 100;
    OwnSpy->BoardToReturn.Add(TileEmpty);

    // HouseCountByTile={10:0}（空地）
    BldSpy->HouseCountByTile.Add(10, 0);
    // PlayerBuildingsToReturn 默认为空（无建筑，NLV 来自地产 MV）

    // 抬 Cash：抵押后 50→150（但 150 < 1000，仍无法偿付）
    OwnSpy->CashTarget       = PS;
    OwnSpy->MortgageCashGain = 100;

    // IsInsolventResult=true（资产耗尽后判定破产）
    EcoSpy->IsInsolventResult = true;

    Sub->SetOwnershipProvider(OwnSpy);
    Sub->SetBuildingProvider(BldSpy);
    Sub->SetEconomyResolver(EcoSpy);

    // ---- 执行 ----
    bool R = Sub->RunForcedLiquidation(PayerId, 1000);

    // ==== 断言 ====

    // ① R==false（破产）
    TestFalse(TEXT("TC-8b ①: RunForcedLiquidation 应返回 false（破产）"), R);

    // ② （econ-009 Design X）破产判定改为 economy 清算 spec 的结构性资产耗尽（DecideNextLiquidationStep→Insolvent），
    //    不再经 EcoSpy->IsInsolvent；故移除 IsInsolventCallCount 断言。破产语义由 ① R==false + ④ 无死循环验证；
    //    清算顺序/破产 spec 的单测在 econ-009 RaisingFundsBankruptcyTest（DecideNextLiquidationStep 直测+变异坐实）。

    // ③ OwnSpy->MortgageCallCount==1（抵押了一次 tile_empty）
    TestEqual(TEXT("TC-8b ③: MortgageCallCount 应==1（抵押一次后 tile 已标抵押）"),
        OwnSpy->MortgageCallCount, 1);

    // ④ 未死循环（执行到此即证 SafetyGuard 未触发——若死循环则测试超时）

    GssAiHooksTestHelpers::DestroyGameWorld(World);
    return true;
}


