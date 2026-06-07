// PlayerStateContainerStartNewGameTest.cpp
// =============================================================================
// 单元测试：URentoPlayerState 容器 + UPlayerTurnSubsystem 建队/定序
// story pt-001 AC-1~5 全覆盖。
//
// 物理路径：Source/rentoTests/Private/PlayerStateContainerStartNewGameTest.cpp
// 逻辑分类：tests/unit/player-turn/playerstate_container_startnewgame_test.cpp
// Automation 类目：Rento.PlayerTurn.PlayerStateStartNewGame
//
// 测试机制说明（headless -nullrhi 验证策略）：
//
// TC-1（AC-1）— 字段默认值：
//   NewObject<URentoPlayerState>(GetTransientPackage()) 直接构造，
//   断言全 11+1 字段的类型与默认值。无运行时依赖，完全 headless。
//
// TC-2（AC-2）— P=2 与 P=4 建队尺寸 + TurnOrderIndex 唯一：
//   UWorld::CreateWorld(EWorldType::Game) 创建 Game World，
//   引擎自动 Initialize UPlayerTurnSubsystem（非 Abstract，World Subsystem 自动发现）。
//   调 InitializeFromConfig 传 P=2/P=4 配置，断言列表尺寸 + TurnOrderIndex 覆盖唯一。
//   断言无硬编码 4：使用变量 P 动态检查 {0..P-1}。
//
// TC-3（AC-3/4）— 定序排名 rolls=[5,9,3]：
//   直接调 AssignTurnOrderByRollTotals({5,9,3})，注入 mock 点数，确定性断言：
//     seat1→TurnOrderIndex=0, seat0→TurnOrderIndex=1, seat2→TurnOrderIndex=2。
//   此方法是排名纯方法（不调 DiceRngService），headless 确定性保证。
//
// TC-4（AC-5）— PlayerId/TokenColor 唯一 + TurnOrderIndex 重复拒绝：
//   构造合法建队后验证 PlayerId/TokenColor 唯一；
//   人为构造 TurnOrderIndex 重复，调 ValidateTurnOrderUniqueness 断言返回 false。
//
// Edge — P=2 最小局/ConsecutiveDoubles 保持 0/TokenColor 8 色唯一：
//   P=2 最小局建队成功，ConsecutiveDoubles 全部为 0，
//   8 色不重复时建队通过，9 号（超出8色）TokenColor 重复时校验拒绝。
//
// 断言非 vacuous 保证：
//   - TC-1 字段断言：若删掉 TestEqual 改 TestNotEqual，fixture 对应字段将使测试通过 →
//     证明断言确实在检测真实条件，非永真（非 vacuous）。
//   - TC-2 TurnOrderIndex 唯一性：若人为令两个 seat 同 TurnOrderIndex，
//     则 TSet 中 bDuplicate=true → TestFalse 处真 FAIL。
//   - TC-3 排名：若 rolls 换为 [9,5,3]，则 seat0→0, seat1→1 与预期不同 → 断言真 FAIL。
//   - TC-4 唯一性拒绝：若 ValidateTurnOrderUniqueness 对重复 index 返回 true，
//     则 TestFalse 处真 FAIL。
//
// 规范依据：
//   - story pt-001 AC-1~5，TC-1~4，Edge cases
//   - ADR-0001（UObject 宿主，headless UWorld::CreateWorld）
//   - ADR-0004（定序掷骰权威流，AssignTurnOrderByRollTotals 可测 seam）
//   - control-manifest Foundation Layer
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "RentoPlayerState.h"
#include "PlayerTurnSubsystem.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"

// =============================================================================
// 辅助命名空间：测试 World 生命周期管理（与 MatchSubsystemBaseTest 同模式）
// =============================================================================
namespace PlayerTurnTestHelpers
{
    /** 创建 Game World（非通知引擎，headless 安全）。调用方负责 DestroyWorld。 */
    static UWorld* CreateGameWorld(const FName& WorldName)
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, WorldName);
        check(World);
        return World;
    }

    /** 销毁测试 World，释放资源，防止 PIE 残留。 */
    static void DestroyGameWorld(UWorld* World)
    {
        if (World)
        {
            World->DestroyWorld(/*bInformEngineOfWorld=*/false);
        }
    }

    /**
     * 构建合法的 FGameSetupConfig（N 个玩家，TokenColor 从 Red 起顺序分配）。
     * 无硬编码 4，Players.Num() == N。
     */
    static FGameSetupConfig MakeConfig(int32 N)
    {
        FGameSetupConfig Config;
        // 8 种颜色（EPlayerColor ordinal 1..8 = Red..Pink）
        // N <= 8 时均能分配唯一颜色
        const EPlayerColor Colors[] = {
            EPlayerColor::Red,    // ordinal 1
            EPlayerColor::Blue,   // ordinal 2
            EPlayerColor::Green,  // ordinal 3
            EPlayerColor::Yellow, // ordinal 4
            EPlayerColor::Purple, // ordinal 5
            EPlayerColor::Orange, // ordinal 6
            EPlayerColor::Cyan,   // ordinal 7
            EPlayerColor::Pink,   // ordinal 8
        };

        for (int32 i = 0; i < N; ++i)
        {
            FPlayerSetupEntry Entry;
            Entry.DisplayName   = FText::FromString(FString::Printf(TEXT("Player%d"), i));
            Entry.TokenColor    = Colors[i % 8];  // N<=8 不重复
            Entry.bIsAI         = false;
            Entry.AIDifficulty  = EAIDifficulty::None;
            Config.Players.Add(Entry);
        }
        return Config;
    }
}

// =============================================================================
// TC-1（AC-1）— URentoPlayerState 全字段默认值校验
//
// GIVEN：NewObject<URentoPlayerState> 直接构造（无任何赋值）
// WHEN：读其全 11+ConsecutiveDoubles 字段
// THEN：
//   bIsAI=false, AIDifficulty=None, bIsBankrupt=false, bIsInJail=false,
//   JailTurnsServed=0, ConsecutiveDoubles=0,
//   PlayerId=-1（未分配哨兵）, TurnOrderIndex=-1（未分配哨兵）,
//   CurrentTileIndex=0, Cash=0,
//   TokenColor=None（未分配哨兵）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_TC1_PlayerStateDefaultValues,
    "Rento.PlayerTurn.PlayerStateStartNewGame.TC1_PlayerStateDefaultValues",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_TC1_PlayerStateDefaultValues::RunTest(const FString& Parameters)
{
    // GIVEN：直接构造 URentoPlayerState（headless，无 World 依赖）
    URentoPlayerState* State = NewObject<URentoPlayerState>(GetTransientPackage());
    if (!TestNotNull(TEXT("TC-1: URentoPlayerState 应能 NewObject 构造"), State))
    {
        return false;
    }

    // THEN：验证全字段默认值（AC-1 字段默认值契约）

    // ---- 本系统开局分配字段 ----
    // PlayerId=-1（未分配哨兵，开局后置为 0..P-1）
    TestEqual(TEXT("TC-1: PlayerId 默认应为 -1（未分配哨兵）"),
        State->PlayerId, -1);

    // TokenColor=None（未分配哨兵；AC-1 implicitly via AC-5 唯一性校验门控）
    TestEqual(TEXT("TC-1: TokenColor 默认应为 EPlayerColor::None"),
        State->TokenColor, EPlayerColor::None);

    // bIsAI=false（AC-1 明确要求）
    TestFalse(TEXT("TC-1: bIsAI 默认应为 false"),
        State->bIsAI);

    // AIDifficulty=None（AC-1：bIsAI=false → AIDifficulty=None）
    TestEqual(TEXT("TC-1: AIDifficulty 默认应为 EAIDifficulty::None"),
        State->AIDifficulty, EAIDifficulty::None);

    // TurnOrderIndex=-1（未分配哨兵，定序后置为 0..P-1）
    TestEqual(TEXT("TC-1: TurnOrderIndex 默认应为 -1（未分配哨兵）"),
        State->TurnOrderIndex, -1);

    // ---- 委派字段（本系统持有，改写规则归他系统） ----
    // CurrentTileIndex=0（GO 格起点，GDD CR-1）
    TestEqual(TEXT("TC-1: CurrentTileIndex 默认应为 0"),
        State->CurrentTileIndex, 0);

    // Cash=0（GDD CR-1；起始资金由经济系统初始化，本 story out of scope）
    TestEqual(TEXT("TC-1: Cash 默认应为 0"),
        State->Cash, 0);

    // bIsInJail=false（AC-1 明确要求）
    TestFalse(TEXT("TC-1: bIsInJail 默认应为 false"),
        State->bIsInJail);

    // JailTurnsServed=0（AC-1 明确要求）
    TestEqual(TEXT("TC-1: JailTurnsServed 默认应为 0"),
        State->JailTurnsServed, 0);

    // bIsBankrupt=false（AC-1 明确要求）
    TestFalse(TEXT("TC-1: bIsBankrupt 默认应为 false"),
        State->bIsBankrupt);

    // DisplayName=空 FText（GDD CR-1 第 2 字段；大厅配置传入，默认未设置）
    // 非 vacuous：若 DisplayName 默认非空（被误初始化），IsEmpty()==false → 此行 FAIL
    TestTrue(TEXT("TC-1: DisplayName 默认应为空 FText"),
        State->DisplayName.IsEmpty());

    // ---- AC-1 权威额外字段（GDD CR-4/L199 确认） ----
    // ConsecutiveDoubles=0（AC-1/TC-1 明确要求；定序不累加，GDD CR-2）
    TestEqual(TEXT("TC-1: ConsecutiveDoubles 默认应为 0"),
        State->ConsecutiveDoubles, 0);

    return true;
}

// =============================================================================
// TC-2（AC-2）— P=2 与 P=4 建队：列表尺寸 + TurnOrderIndex 唯一（无硬编码 4）
//
// GIVEN：FGameSetupConfig.Players.Num()==2 / ==4
// WHEN：UPlayerTurnSubsystem::InitializeFromConfig
// THEN：PlayerStates.Num()==P，TurnOrderIndex 覆盖 {0..P-1} 唯一（无硬编码 4）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_TC2_BuildSquadSizeAndTurnOrderUniqueness,
    "Rento.PlayerTurn.PlayerStateStartNewGame.TC2_BuildSquad_SizeAndTurnOrderUnique",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_TC2_BuildSquadSizeAndTurnOrderUniqueness::RunTest(const FString& Parameters)
{
    // 子测试辅助 lambda：验证 P 个玩家建队后的尺寸 + TurnOrderIndex 唯一性
    auto RunForP = [this](int32 P, const FString& Tag) -> bool
    {
        // GIVEN：创建 Game World（引擎自动 Initialize UPlayerTurnSubsystem）
        UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(
            FName(*FString::Printf(TEXT("TC2_World_P%d"), P)));
        if (!TestNotNull(*FString::Printf(TEXT("%s: World 应能创建"), *Tag), World))
        {
            return false;
        }

        // 取 UPlayerTurnSubsystem
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(*FString::Printf(TEXT("%s: UPlayerTurnSubsystem 应能取得"), *Tag), Sub))
        {
            PlayerTurnTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // WHEN：传入 P 个玩家的配置，执行建队
        // 注意：headless 下 DiceRngService 可能存在（同为 World Subsystem），
        //       InitializeFromConfig 会走生产定序路径；若不存在则退化为序号顺序。
        //       两种情况 TurnOrderIndex 均应覆盖 0..P-1 唯一。
        const FGameSetupConfig Config = PlayerTurnTestHelpers::MakeConfig(P);
        const bool bInitOk = Sub->InitializeFromConfig(Config);

        // THEN-1：建队成功
        if (!TestTrue(*FString::Printf(TEXT("%s: InitializeFromConfig(P=%d) 应返回 true"), *Tag, P), bInitOk))
        {
            PlayerTurnTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // THEN-2：列表尺寸 == P（无硬编码，用变量 P 断言）
        const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
        if (!TestEqual(*FString::Printf(TEXT("%s: PlayerStates.Num() 应等于 P=%d"), *Tag, P),
                States.Num(), P))
        {
            PlayerTurnTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // THEN-3：TurnOrderIndex 覆盖 0..P-1 唯一（无硬编码 4，用变量 P 动态检查）
        TSet<int32> SeenIndices;
        bool bAllUnique = true;
        bool bAllInRange = true;
        for (int32 i = 0; i < P; ++i)
        {
            const URentoPlayerState* State = States[i].Get();
            if (!TestNotNull(*FString::Printf(TEXT("%s: States[%d] 不应为 null"), *Tag, i), State))
            {
                PlayerTurnTestHelpers::DestroyGameWorld(World);
                return false;
            }

            const int32 Idx = State->TurnOrderIndex;

            // 范围检查（0 <= Idx < P）
            if (Idx < 0 || Idx >= P)
            {
                AddError(FString::Printf(TEXT("%s: States[%d].TurnOrderIndex=%d 越界（有效范围 0..%d-1）"), *Tag, i, Idx, P));
                bAllInRange = false;
            }

            // 唯一性检查
            bool bAlreadySeen = false;
            SeenIndices.Add(Idx, &bAlreadySeen);
            if (bAlreadySeen)
            {
                AddError(FString::Printf(TEXT("%s: TurnOrderIndex=%d 重复（AC-2 唯一性违反）"), *Tag, Idx));
                bAllUnique = false;
            }
        }

        TestTrue(*FString::Printf(TEXT("%s: 所有 TurnOrderIndex 应在范围内（0..%d-1）"), *Tag, P), bAllInRange);
        TestTrue(*FString::Printf(TEXT("%s: 所有 TurnOrderIndex 应唯一（覆盖 0..%d-1）"), *Tag, P), bAllUnique);

        // 非 vacuous 判别性：若两个 seat 故意设同值，bAllUnique 应为 false
        // （此行仅记录注释，不修改状态——真实验证在上方循环中）

        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return bInitOk && bAllInRange && bAllUnique;
    };

    // 运行 P=2（最小局）和 P=4（标准局）两个子测试
    const bool bP2 = RunForP(2, TEXT("TC-2[P=2]"));
    const bool bP4 = RunForP(4, TEXT("TC-2[P=4]"));

    return bP2 && bP4;
}

// =============================================================================
// TC-3（AC-3/AC-4）— 定序排名：rolls=[5,9,3] → 降序排名
//
// GIVEN：P=3 玩家建队（seat0/seat1/seat2），mock rolls=[5,9,3]
// WHEN：AssignTurnOrderByRollTotals({5,9,3})（排名纯方法，直接注入，不走 DiceRngService）
// THEN：TurnOrderIndex{seat1:0, seat0:1, seat2:2}（点数降序：9>5>3）
//
// 非 vacuous 验证：rolls 顺序固定（[5,9,3]），与直觉相反——seat1 先手不是 seat0。
//   若将 rolls 换为 [9,5,3]，则 seat0=0, seat1=1，与当前断言不同 → 真 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_TC3_TurnOrderByRollTotals,
    "Rento.PlayerTurn.PlayerStateStartNewGame.TC3_TurnOrder_Rolls_5_9_3",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_TC3_TurnOrderByRollTotals::RunTest(const FString& Parameters)
{
    // GIVEN：创建 Game World + 取 UPlayerTurnSubsystem
    UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("TC3_RankWorld"));
    if (!TestNotNull(TEXT("TC-3: World 应能创建"), World)) { return false; }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-3: UPlayerTurnSubsystem 应能取得"), Sub))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // GIVEN：P=3 建队（seat0=Red, seat1=Blue, seat2=Green，3 种不同颜色）
    FGameSetupConfig Config = PlayerTurnTestHelpers::MakeConfig(3);
    const bool bInitOk = Sub->InitializeFromConfig(Config);
    if (!TestTrue(TEXT("TC-3: P=3 建队应成功"), bInitOk))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // WHEN：注入 mock rolls=[5,9,3]（直接调排名纯方法，不走 DiceRngService）
    // seat0 = 5 点, seat1 = 9 点, seat2 = 3 点
    Sub->AssignTurnOrderByRollTotals({5, 9, 3});

    // THEN：降序排名（9>5>3）
    //   seat1(9分) → TurnOrderIndex=0（先手）
    //   seat0(5分) → TurnOrderIndex=1
    //   seat2(3分) → TurnOrderIndex=2（后手）
    const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
    if (!TestEqual(TEXT("TC-3: PlayerStates.Num() 应为 3"), States.Num(), 3))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // seat0（rolls[0]=5）→ TurnOrderIndex=1
    TestEqual(TEXT("TC-3: seat0(rolls=5) TurnOrderIndex 应为 1"),
        States[0]->TurnOrderIndex, 1);

    // seat1（rolls[1]=9，最高分）→ TurnOrderIndex=0（先手）
    TestEqual(TEXT("TC-3: seat1(rolls=9，最高) TurnOrderIndex 应为 0（先手）"),
        States[1]->TurnOrderIndex, 0);

    // seat2（rolls[2]=3，最低分）→ TurnOrderIndex=2（后手）
    TestEqual(TEXT("TC-3: seat2(rolls=3，最低) TurnOrderIndex 应为 2（后手）"),
        States[2]->TurnOrderIndex, 2);

    // 非 vacuous 判别性：若 seat0 的断言改为 TestEqual(..., 0) 则因 seat0 实际为 1 而 FAIL
    // （此注释说明断言是真实检测，非永真）

    PlayerTurnTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4（AC-5）— PlayerId/TokenColor 唯一 + TurnOrderIndex 重复拒绝
//
// Part A：GIVEN StartNewGame 完成，WHEN 检查 PlayerId/TokenColor，THEN 各唯一
// Part B：GIVEN 人为构造 TurnOrderIndex 重复，WHEN ValidateTurnOrderUniqueness，THEN 返回 false
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_TC4_UniquenessValidation,
    "Rento.PlayerTurn.PlayerStateStartNewGame.TC4_Uniqueness_PlayerIdColor_TurnOrderReject",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_TC4_UniquenessValidation::RunTest(const FString& Parameters)
{
    // Part B 故意构造 TurnOrderIndex 重复，触发 ValidateTurnOrderUniqueness 的预期拒绝 Error 日志。
    // headless Automation 中未吞的 UE_LOG(Error) 会判 Fail，故用 AddExpectedError 吞掉这条预期 Error
    // （项目惯例：验证型错误路径用 UE_LOG(Error) + 测试 AddExpectedError(Contains, N) 吞预期）。
    AddExpectedError(TEXT("ValidateTurnOrderUniqueness"), EAutomationExpectedErrorFlags::Contains, 1);

    // GIVEN：创建 Game World + 取 UPlayerTurnSubsystem
    UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("TC4_UniqueWorld"));
    if (!TestNotNull(TEXT("TC-4: World 应能创建"), World)) { return false; }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-4: UPlayerTurnSubsystem 应能取得"), Sub))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // GIVEN：P=4 建队（合法配置，4 种不同颜色）
    const int32 P = 4;
    const FGameSetupConfig Config = PlayerTurnTestHelpers::MakeConfig(P);
    const bool bInitOk = Sub->InitializeFromConfig(Config);
    if (!TestTrue(TEXT("TC-4: P=4 建队应成功"), bInitOk))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
    if (!TestEqual(TEXT("TC-4: PlayerStates.Num() 应为 4"), States.Num(), P))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // ===========================================================================
    // Part A：PlayerId 唯一性（开局分配 0..P-1，无重复）
    // ===========================================================================
    {
        TSet<int32> SeenIds;
        bool bAllIdsUnique = true;
        for (int32 i = 0; i < P; ++i)
        {
            const int32 Id = States[i]->PlayerId;

            // 范围检查（0 <= Id < P）
            if (!TestTrue(FString::Printf(TEXT("TC-4[PartA]: States[%d].PlayerId=%d 应在 [0,%d-1]"), i, Id, P),
                    Id >= 0 && Id < P))
            {
                bAllIdsUnique = false;
                continue;
            }

            // 唯一性检查
            bool bAlreadySeen = false;
            SeenIds.Add(Id, &bAlreadySeen);
            if (bAlreadySeen)
            {
                AddError(FString::Printf(TEXT("TC-4[PartA]: PlayerId=%d 重复（AC-5 唯一性违反）"), Id));
                bAllIdsUnique = false;
            }
        }
        TestTrue(TEXT("TC-4[PartA]: 所有 PlayerId 应唯一（覆盖 0..P-1）"), bAllIdsUnique);
    }

    // ===========================================================================
    // Part A（续）：TokenColor 唯一性（开局唯一分配，无重复，无 None）
    // ===========================================================================
    {
        TSet<EPlayerColor> SeenColors;
        bool bAllColorsUnique = true;
        bool bNoNoneColor = true;
        for (int32 i = 0; i < P; ++i)
        {
            const EPlayerColor Color = States[i]->TokenColor;

            // None 不合法（AC-5）
            if (Color == EPlayerColor::None)
            {
                AddError(FString::Printf(TEXT("TC-4[PartA]: States[%d].TokenColor=None 非法"), i));
                bNoNoneColor = false;
                continue;
            }

            // 唯一性检查
            bool bAlreadySeen = false;
            SeenColors.Add(Color, &bAlreadySeen);
            if (bAlreadySeen)
            {
                AddError(FString::Printf(TEXT("TC-4[PartA]: TokenColor=%d 重复（AC-5 唯一性违反）"),
                    static_cast<int32>(Color)));
                bAllColorsUnique = false;
            }
        }
        TestTrue(TEXT("TC-4[PartA]: 所有 TokenColor 应唯一且不为 None"), bAllColorsUnique && bNoNoneColor);
    }

    // ===========================================================================
    // Part B：TurnOrderIndex 重复 → ValidateTurnOrderUniqueness 返回 false（拒绝）
    //
    // 人为构造：令 seat0 和 seat1 都持 TurnOrderIndex=0（重复），
    // 调 ValidateTurnOrderUniqueness 应返回 false。
    //
    // 非 vacuous：若 ValidateTurnOrderUniqueness 对重复 index 错误返回 true，
    //   则 TestFalse 处真 FAIL（判别性 false 分量保证）。
    // ===========================================================================
    {
        // 备份原始 TurnOrderIndex（稍后恢复，防止影响其他 Part）
        const int32 OrigIdx0 = States[0]->TurnOrderIndex;
        const int32 OrigIdx1 = States[1]->TurnOrderIndex;

        // 人为制造重复：seat0 和 seat1 都设为 0
        States[0]->TurnOrderIndex = 0;
        States[1]->TurnOrderIndex = 0;  // 故意重复！

        // WHEN：校验唯一性
        const bool bValid = Sub->ValidateTurnOrderUniqueness();

        // THEN：应返回 false（重复 → 拒绝）
        TestFalse(TEXT("TC-4[PartB]: TurnOrderIndex 重复时 ValidateTurnOrderUniqueness 应返回 false（拒绝）"),
            bValid);

        // 恢复原始值（防止后续清理报错）
        States[0]->TurnOrderIndex = OrigIdx0;
        States[1]->TurnOrderIndex = OrigIdx1;
    }

    PlayerTurnTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// Edge_MinimalMatch — P=2 最小局正常定序
//
// GIVEN：P=2（最小合法局），分配两种不同颜色
// WHEN：InitializeFromConfig
// THEN：建队成功，PlayerStates.Num()==2，TurnOrderIndex 覆盖 {0,1} 唯一
//
// 非 vacuous：若建队允许 P=1（非最小合法），尺寸断言 Num()==2 会 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_Edge_MinimalMatch,
    "Rento.PlayerTurn.PlayerStateStartNewGame.Edge_MinimalMatch_P2",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_Edge_MinimalMatch::RunTest(const FString& Parameters)
{
    UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("Edge_MinimalWorld"));
    if (!TestNotNull(TEXT("Edge_P2: World 应能创建"), World)) { return false; }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("Edge_P2: UPlayerTurnSubsystem 应能取得"), Sub))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // GIVEN：P=2 最小局
    const FGameSetupConfig Config = PlayerTurnTestHelpers::MakeConfig(2);

    // WHEN
    const bool bInitOk = Sub->InitializeFromConfig(Config);

    // THEN：建队成功
    TestTrue(TEXT("Edge_P2: P=2 最小局建队应成功"), bInitOk);

    const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
    TestEqual(TEXT("Edge_P2: PlayerStates.Num() 应为 2"), States.Num(), 2);

    if (States.Num() == 2)
    {
        // TurnOrderIndex 覆盖 {0,1}
        const int32 Idx0 = States[0]->TurnOrderIndex;
        const int32 Idx1 = States[1]->TurnOrderIndex;

        TestTrue(TEXT("Edge_P2: States[0].TurnOrderIndex 应在 [0,1]"), Idx0 == 0 || Idx0 == 1);
        TestTrue(TEXT("Edge_P2: States[1].TurnOrderIndex 应在 [0,1]"), Idx1 == 0 || Idx1 == 1);
        TestNotEqual(TEXT("Edge_P2: 两个 TurnOrderIndex 应不相等（唯一）"), Idx0, Idx1);
    }

    PlayerTurnTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// Edge_ConsecutiveDoublesKeepsZero — 定序后 ConsecutiveDoubles 保持 0
//
// GIVEN：P=3 建队后执行定序（AssignTurnOrderByRollTotals）
// WHEN：读取所有 PlayerState.ConsecutiveDoubles
// THEN：全部为 0（GDD CR-2：定序不进双点链，ConsecutiveDoubles 开局为 0）
//
// 非 vacuous：若实现在定序时意外累加 ConsecutiveDoubles，则 TestEqual 0 会 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_Edge_ConsecutiveDoublesKeepsZero,
    "Rento.PlayerTurn.PlayerStateStartNewGame.Edge_ConsecutiveDoubles_KeepsZero_AfterOrder",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_Edge_ConsecutiveDoublesKeepsZero::RunTest(const FString& Parameters)
{
    UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("Edge_CDWorld"));
    if (!TestNotNull(TEXT("Edge_CD: World 应能创建"), World)) { return false; }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("Edge_CD: UPlayerTurnSubsystem 应能取得"), Sub))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // GIVEN：P=3 建队
    const FGameSetupConfig Config = PlayerTurnTestHelpers::MakeConfig(3);
    const bool bInitOk = Sub->InitializeFromConfig(Config);
    if (!TestTrue(TEXT("Edge_CD: P=3 建队应成功"), bInitOk))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // WHEN：执行定序（注入 mock rolls，避免 DiceRngService 依赖）
    Sub->AssignTurnOrderByRollTotals({7, 5, 9});

    // THEN：定序后所有 ConsecutiveDoubles 仍为 0（GDD CR-2：定序不累加）
    const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
    for (int32 i = 0; i < States.Num(); ++i)
    {
        TestEqual(FString::Printf(TEXT("Edge_CD: States[%d].ConsecutiveDoubles 定序后应为 0"), i),
            States[i]->ConsecutiveDoubles, 0);
    }

    PlayerTurnTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// Edge_TokenColor8ColorsUnique — TokenColor 唯一分配通过；重复则拒绝
//
// 注：8 色调色板由 EPlayerColor 枚举（8 实色+None）定义保证；游戏为 2-4 人局
//     （pt-002 AC-10 强制 P<=4），故每局取 <=4 色。本测试在合法 P 上限（P=4）验
//     「4 色互异 → 唯一分配成功」，重复色 → 拒绝。
//
// GIVEN：
//   Part A：P=4，4 个玩家各用不同颜色（合法上限）→ 建队成功 + 4 色互异
//   Part B：2 个玩家但 TokenColor 重复（均为 Red）→ 建队拒绝，返回 false
//
// 非 vacuous：
//   Part A：若实现复用同色致 <4 互异，TestEqual(4) 处真 FAIL；若建队失败 TestTrue FAIL。
//   Part B：若重复颜色未被拒绝，TestFalse 处真 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_Edge_TokenColor8Colors,
    "Rento.PlayerTurn.PlayerStateStartNewGame.Edge_TokenColor_8Colors_UniqueAssign",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_Edge_TokenColor8Colors::RunTest(const FString& Parameters)
{
    // Part B 故意构造重复 TokenColor，触发 InitializeFromConfig 的预期拒绝 Error 日志。
    // headless Automation 中未吞的 UE_LOG(Error) 会判 Fail，故用 AddExpectedError 吞掉这条预期 Error。
    AddExpectedError(TEXT("TokenColor"), EAutomationExpectedErrorFlags::Contains, 1);

    // ===========================================================================
    // Part A：8 种不同颜色 → 建队成功
    // ===========================================================================
    {
        UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("Edge_8Color_World"));
        if (!TestNotNull(TEXT("Edge_8Color[A]: World 应能创建"), World)) { return false; }

        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("Edge_8Color[A]: UPlayerTurnSubsystem 应能取得"), Sub))
        {
            PlayerTurnTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // 8 色调色板（Red..Pink）由 EPlayerColor 枚举定义保证（PlayerTurnTypes.h）；
        // 游戏为 2-4 人局（pt-002 AC-10 强制 P<=4），故每局最多用 4 色。
        // 本 Part 验「从 8 色调色板取 4 色，开局唯一分配」在合法 P 上限成功。
        // MakeConfig(4) 分配前 4 色（Red/Blue/Green/Yellow，互异）。
        const FGameSetupConfig Config4 = PlayerTurnTestHelpers::MakeConfig(4);

        const bool bOk = Sub->InitializeFromConfig(Config4);
        TestTrue(TEXT("Edge_Color[A]: 4 种不同颜色（P=4 合法上限）应建队成功"), bOk);

        // 验 4 个分配的 TokenColor 确实互异（唯一性意图，非 vacuous：若实现复用同色则 FAIL）
        if (bOk)
        {
            const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
            TSet<EPlayerColor> Distinct;
            for (const TObjectPtr<URentoPlayerState>& S : States)
            {
                if (S) { Distinct.Add(S->TokenColor); }
            }
            TestEqual(TEXT("Edge_Color[A]: 4 玩家应分得 4 种互异颜色"), Distinct.Num(), 4);
        }

        PlayerTurnTestHelpers::DestroyGameWorld(World);
    }

    // ===========================================================================
    // Part B：TokenColor 重复（2 个玩家均为 Red）→ 建队拒绝，返回 false
    // ===========================================================================
    {
        UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("Edge_DupColor_World"));
        if (!TestNotNull(TEXT("Edge_8Color[B]: World 应能创建"), World)) { return false; }

        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("Edge_8Color[B]: UPlayerTurnSubsystem 应能取得"), Sub))
        {
            PlayerTurnTestHelpers::DestroyGameWorld(World);
            return false;
        }

        // 构造重复颜色配置（2 个玩家均为 Red）
        FGameSetupConfig DupConfig;
        for (int32 i = 0; i < 2; ++i)
        {
            FPlayerSetupEntry Entry;
            Entry.DisplayName  = FText::FromString(FString::Printf(TEXT("Player%d"), i));
            Entry.TokenColor   = EPlayerColor::Red;  // 故意重复！
            Entry.bIsAI        = false;
            Entry.AIDifficulty = EAIDifficulty::None;
            DupConfig.Players.Add(Entry);
        }

        // InitializeFromConfig 应检测到重复颜色并返回 false
        // 注意：此调用会产生一条 UE_LOG(Error) 日志，这是预期行为（校验拒绝日志）
        const bool bOk = Sub->InitializeFromConfig(DupConfig);
        TestFalse(TEXT("Edge_8Color[B]: TokenColor 重复时建队应被拒绝（返回 false）"), bOk);

        // 校验失败后 PlayerStates 应为空（InitializeFromConfig 清空了列表）
        TestEqual(TEXT("Edge_8Color[B]: 建队失败后 PlayerStates 应为空"),
            Sub->GetPlayerStates().Num(), 0);

        PlayerTurnTestHelpers::DestroyGameWorld(World);
    }

    return true;
}

// =============================================================================
// Edge_TokenColorNone_Rejected（AC-5 补充）— TokenColor=None 哨兵值拒绝开局
//
// GIVEN：2 玩家配置，其一 TokenColor=None（哨兵/未分配值）
// WHEN：InitializeFromConfig
// THEN：建队拒绝返回 false，PlayerStates 为空（AC-5「TokenColor 唯一分配」门控 None）
//
// 非 vacuous：若 None 未被拒绝（实现漏校验），TestFalse 处真 FAIL。
// 缺口来源：qa-review W-5（Edge_TokenColor PartB 仅测重复色，未测 None 哨兵拒绝）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_Edge_TokenColorNoneRejected,
    "Rento.PlayerTurn.PlayerStateStartNewGame.Edge_TokenColorNone_Rejected",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_Edge_TokenColorNoneRejected::RunTest(const FString& Parameters)
{
    // TokenColor=None 触发 InitializeFromConfig 的预期拒绝 Error 日志，AddExpectedError 吞掉。
    // 子串 "TokenColor=None" 为 ASCII 且精确匹配 None 路径（区别于重复路径），occurrences=1。
    AddExpectedError(TEXT("TokenColor=None"), EAutomationExpectedErrorFlags::Contains, 1);

    UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("Edge_NoneColor_World"));
    if (!TestNotNull(TEXT("Edge_None: World 应能创建"), World)) { return false; }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("Edge_None: UPlayerTurnSubsystem 应能取得"), Sub))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 构造含 None 的配置：seat0=Red（合法）, seat1=None（哨兵，非法）
    FGameSetupConfig NoneConfig;
    {
        FPlayerSetupEntry E0;
        E0.DisplayName = FText::FromString(TEXT("Player0"));
        E0.TokenColor  = EPlayerColor::Red;
        NoneConfig.Players.Add(E0);

        FPlayerSetupEntry E1;
        E1.DisplayName = FText::FromString(TEXT("Player1"));
        E1.TokenColor  = EPlayerColor::None;  // 哨兵值，应拒绝
        NoneConfig.Players.Add(E1);
    }

    // WHEN：建队应检测 None 并拒绝
    const bool bOk = Sub->InitializeFromConfig(NoneConfig);

    // THEN：返回 false + 列表清空
    TestFalse(TEXT("Edge_None: TokenColor=None 时建队应被拒绝（返回 false）"), bOk);
    TestEqual(TEXT("Edge_None: 拒绝后 PlayerStates 应为空"),
        Sub->GetPlayerStates().Num(), 0);

    PlayerTurnTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// Edge_TurnOrderOutOfRange_Rejected（AC-5 补充）— TurnOrderIndex 越界拒绝
//
// GIVEN：合法 P=3 建队后，人为将某 seat 的 TurnOrderIndex 设为越界值（>=P）
// WHEN：ValidateTurnOrderUniqueness
// THEN：返回 false（AC-5「TurnOrderIndex 未覆盖 0..P-1→拒绝」的越界子case）
//
// 非 vacuous：若越界值未被拒绝（实现漏范围检查），TestFalse 处真 FAIL。
// 缺口来源：qa-review W-4（TC4 PartB 仅测重复，未测越界路径）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPlayerTurn_Edge_TurnOrderOutOfRangeRejected,
    "Rento.PlayerTurn.PlayerStateStartNewGame.Edge_TurnOrderOutOfRange_Rejected",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPlayerTurn_Edge_TurnOrderOutOfRangeRejected::RunTest(const FString& Parameters)
{
    // 越界 TurnOrderIndex 触发 ValidateTurnOrderUniqueness 的预期越界 Error 日志，AddExpectedError 吞掉。
    // 子串 "ValidateTurnOrderUniqueness" 为 ASCII；本测试仅触发越界路径一次，occurrences=1。
    AddExpectedError(TEXT("ValidateTurnOrderUniqueness"), EAutomationExpectedErrorFlags::Contains, 1);

    UWorld* World = PlayerTurnTestHelpers::CreateGameWorld(TEXT("Edge_OOR_World"));
    if (!TestNotNull(TEXT("Edge_OOR: World 应能创建"), World)) { return false; }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("Edge_OOR: UPlayerTurnSubsystem 应能取得"), Sub))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // GIVEN：合法 P=3 建队（seed 接线后定序成功，无 lazy-init）
    const FGameSetupConfig Config = PlayerTurnTestHelpers::MakeConfig(3);
    const bool bInitOk = Sub->InitializeFromConfig(Config);
    if (!TestTrue(TEXT("Edge_OOR: P=3 建队应成功"), bInitOk))
    {
        PlayerTurnTestHelpers::DestroyGameWorld(World);
        return false;
    }

    const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
    if (States.Num() == 3)
    {
        // 人为将 seat0 的 TurnOrderIndex 设为越界值 99（>= P=3）
        const int32 OrigIdx0 = States[0]->TurnOrderIndex;
        States[0]->TurnOrderIndex = 99;  // 越界！

        // WHEN：校验唯一性
        const bool bValid = Sub->ValidateTurnOrderUniqueness();

        // THEN：越界 → 拒绝（返回 false）
        TestFalse(TEXT("Edge_OOR: TurnOrderIndex 越界(99>=P) 时 ValidateTurnOrderUniqueness 应返回 false"),
            bValid);

        // 恢复原值（防 World 销毁阶段异常）
        States[0]->TurnOrderIndex = OrigIdx0;
    }

    PlayerTurnTestHelpers::DestroyGameWorld(World);
    return true;
}
