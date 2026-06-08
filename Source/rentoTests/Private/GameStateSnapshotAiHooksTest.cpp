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

