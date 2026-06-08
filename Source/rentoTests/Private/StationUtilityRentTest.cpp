// StationUtilityRentTest.cpp
// =============================================================================
// 单元测试：车站/公用租金 F-3/F-4（econ-005）—— count guard + dice_total 显式参数
//
// 物理路径：Source/rentoTests/Private/StationUtilityRentTest.cpp
// 逻辑路径：tests/unit/economy-cash/station_utility_rent_test.cpp
// Automation 类目：Rento.Economy.StationUtilityRent
//
// 被测：
//   ComputeRailroadRent(StationCount, RentTable)
//     station≥1 → RentTable[clamp(station-1,0,3)]；station<1 → 0 + dev log（防 [-1]）
//   ComputeUtilityRent(UtilityCount, DiceTotal, DiceMultiplierTable)
//     count≥1 → DiceTotal × DiceMultiplierTable[clamp(count-1,0,1)]；count<1 → 0 + dev log
//     dice_total 显式参数（AC-18：不缓存，两次不同 dice 各按传入算）
//
// Railroad fixture {25,50,100,200}；Utility mult {4,10}。
//
// 断言非 vacuous（flip 致真 FAIL）：
//   - TC2：去 station guard → station=0 查 RentTable[clamp(-1)]=25≠0 → FAIL
//   - TC5：忽略 DiceTotal 参数（硬编码）→ dice=11 仍 28≠44 → FAIL
//
// 规范依据：story econ-005 AC-14~18；GDD economy-cash F-3/F-4/CR-3；ADR-0015（dice_total PULL）/0014/0006。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "EconomySubsystem.h"
#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnTypes.h"

// =============================================================================
// 测试辅助（static 内部链接）
// =============================================================================
namespace StationUtilityRentTestHelpers
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

    /** Railroad RentTable fixture：{25,50,100,200}（1..4 站 → index 0..3）。 */
    static TArray<int32> MakeRailroadTable()
    {
        return TArray<int32>({25, 50, 100, 200});
    }

    /** Utility DiceMultiplierTable fixture：{4,10}（1/2 公用倍率）。 */
    static TArray<int32> MakeUtilityMultTable()
    {
        return TArray<int32>({4, 10});
    }

} // namespace StationUtilityRentTestHelpers

// =============================================================================
// TC-1（AC-14）—— 车站租金 = RentTable[station_count−1]
//   station=2 → RentTable[1]==50；station=4 → RentTable[3]==200。station=5 → clamp→200。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStationUtil_TC1_RailroadFormula,
    "Rento.Economy.StationUtilityRent.TC1_RailroadFormula",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStationUtil_TC1_RailroadFormula::RunTest(const FString& Parameters)
{
    using namespace StationUtilityRentTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-1: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("StaUtil_TC1_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> RT = MakeRailroadTable();

    TestEqual(TEXT("TC-1: station=2 → RentTable[1]==50"), Econ->ComputeRailroadRent(2, RT), 50);
    TestEqual(TEXT("TC-1: station=3 → RentTable[2]==100（中间 index 到达性，C-1）"), Econ->ComputeRailroadRent(3, RT), 100);
    TestEqual(TEXT("TC-1: station=4 → RentTable[3]==200"), Econ->ComputeRailroadRent(4, RT), 200);
    TestEqual(TEXT("TC-1: station=1 → RentTable[0]==25"), Econ->ComputeRailroadRent(1, RT), 25);
    // station=5（地产6 竞态超界）→ clamp(4,0,3)→RentTable[3]==200（静默兜底，不崩）。
    TestEqual(TEXT("TC-1: station=5 → clamp→RentTable[3]==200"), Econ->ComputeRailroadRent(5, RT), 200);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-2（AC-15）—— 车站 guard：station=0 → 0、不查 [−1]、不崩 + dev 信号
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStationUtil_TC2_RailroadZeroGuard,
    "Rento.Economy.StationUtilityRent.TC2_RailroadZeroGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStationUtil_TC2_RailroadZeroGuard::RunTest(const FString& Parameters)
{
    using namespace StationUtilityRentTestHelpers;

    // station=0 + station=−1（负值，C-2）各触发 1 条 "station_count" Error（共 2 条）。
    AddExpectedError(TEXT("station_count"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-2: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("StaUtil_TC2_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> RT = MakeRailroadTable();

    // station=0（所有权6 竞态）→ 0（不查 RentTable[−1]，不崩）。
    TestEqual(TEXT("TC-2: station=0 → rent==0（守门，不查 [−1]）"), Econ->ComputeRailroadRent(0, RT), 0);
    // station=−1（负值竞态，C-2）→ 0（同守门分支 <1）。
    TestEqual(TEXT("TC-2: station=−1 → rent==0（负值守门）"), Econ->ComputeRailroadRent(-1, RT), 0);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-3（AC-16）—— 公用租金 = dice_total × DiceMultiplierTable[utility_count−1]
//   count=1,dice=7,mult{4,10} → 7×4==28；count=2,dice=7 → 7×10==70。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStationUtil_TC3_UtilityFormula,
    "Rento.Economy.StationUtilityRent.TC3_UtilityFormula",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStationUtil_TC3_UtilityFormula::RunTest(const FString& Parameters)
{
    using namespace StationUtilityRentTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-3: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("StaUtil_TC3_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> MT = MakeUtilityMultTable();

    // count=1, dice=7 → 7 × mult[0](4) == 28（AC-16）。
    TestEqual(TEXT("TC-3: count=1,dice=7 → 7×4==28"), Econ->ComputeUtilityRent(1, 7, MT), 28);
    // count=2, dice=7 → 7 × mult[1](10) == 70。
    TestEqual(TEXT("TC-3: count=2,dice=7 → 7×10==70"), Econ->ComputeUtilityRent(2, 7, MT), 70);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-4（AC-17）—— 公用 guard：utility_count=0 → 0、不查 [−1]、不崩 + dev 信号
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStationUtil_TC4_UtilityZeroGuard,
    "Rento.Economy.StationUtilityRent.TC4_UtilityZeroGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStationUtil_TC4_UtilityZeroGuard::RunTest(const FString& Parameters)
{
    using namespace StationUtilityRentTestHelpers;

    // utility_count=0 + utility_count=−1（负值，C-2）各触发 1 条 "utility_count" Error（共 2 条）。
    AddExpectedError(TEXT("utility_count"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-4: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("StaUtil_TC4_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> MT = MakeUtilityMultTable();

    // count=0 → 0（不查 [−1]，不崩）；dice=7 仍 0（守门先于乘法）。
    TestEqual(TEXT("TC-4: utility_count=0 → rent==0（守门，不查 [−1]）"), Econ->ComputeUtilityRent(0, 7, MT), 0);
    // count=−1（负值竞态，C-2）→ 0（同守门分支 <1）。
    TestEqual(TEXT("TC-4: utility_count=−1 → rent==0（负值守门）"), Econ->ComputeUtilityRent(-1, 7, MT), 0);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-6（对抗 review CONCERN-3）—— dice_total≤0 economy 侧独立防线
//   dice_total≤0（holder/上游异常）→ rent==0、不返负值 + dev log（ADR-0015 holder 应保 [2,12]）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStationUtil_TC6_DiceTotalGuard,
    "Rento.Economy.StationUtilityRent.TC6_DiceTotalGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStationUtil_TC6_DiceTotalGuard::RunTest(const FString& Parameters)
{
    using namespace StationUtilityRentTestHelpers;

    // dice=0 + dice=−1 各触发 1 条 "dice_total" Error（共 2 条）。
    AddExpectedError(TEXT("dice_total"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-6: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("StaUtil_TC6_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> MT = MakeUtilityMultTable();

    // dice_total=0（holder/上游异常）→ 0（不返负/零额误算）。
    TestEqual(TEXT("TC-6: dice_total=0 → rent==0（独立防线）"), Econ->ComputeUtilityRent(1, 0, MT), 0);
    // dice_total=−1 → 0（不返负值 −4，防 SettleRent 上游误传）。
    TestEqual(TEXT("TC-6: dice_total=−1 → rent==0（不返负值）"), Econ->ComputeUtilityRent(1, -1, MT), 0);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-5（AC-18）—— dice_total 是显式参数非缓存
//   同 count=1,mult{4,10}：dice=7 → 28；dice=11 → 44（各按传入算，证不读缓存）。
//   "前进到最近公用"额外骰由调用方经 holder PULL 传入，economy 无 dice_total 成员缓存。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStationUtil_TC5_DiceTotalExplicitNotCached,
    "Rento.Economy.StationUtilityRent.TC5_DiceTotalExplicitNotCached",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStationUtil_TC5_DiceTotalExplicitNotCached::RunTest(const FString& Parameters)
{
    using namespace StationUtilityRentTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC-5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("StaUtil_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }
    const TArray<int32> MT = MakeUtilityMultTable();

    // 同 count=1, mult[0]=4，两次不同 dice_total → 各按传入算（证不缓存骰点）。
    const int32 RentDice7  = Econ->ComputeUtilityRent(1, 7, MT);
    const int32 RentDice11 = Econ->ComputeUtilityRent(1, 11, MT);
    TestEqual(TEXT("TC-5: dice=7 → 7×4==28"), RentDice7, 28);
    TestEqual(TEXT("TC-5: dice=11 → 11×4==44（额外骰按传入算，非缓存 7）"), RentDice11, 44);
    TestNotEqual(TEXT("TC-5: 两次 dice 结果不同（证显式参数）"), RentDice7, RentDice11);

    // 决定性：同输入两次一致。
    TestEqual(TEXT("TC-5: 决定性 同输入两次一致"),
        Econ->ComputeUtilityRent(1, 7, MT), Econ->ComputeUtilityRent(1, 7, MT));

    DestroyGameWorld(World);
    return true;
}
