// NlvInsolvencyTest.cpp
// =============================================================================
// 单元测试：NLV F-9 + F-10 is_insolvent + F-8 建筑卖回（econ-008）
//   🔴 最高风险数值契约：逐栋 floor + 严格 < 边界 + 单一枚举入口
//
// 物理路径：Source/rentoTests/Private/NlvInsolvencyTest.cpp
// 逻辑路径：tests/unit/economy-cash/nlv_insolvency_test.cpp
// Automation 类目：Rento.Economy.NlvInsolvency
//
// 被测：
//   ComputeBuildingSellback(BC)         → floor(BC×1/2)（F-8，纯函数，整数 floor）
//   ComputeNetLiquidationValue(Assets)  → Σ MV[未抵押] + Σ house×sellback（F-9，单一枚举入口）
//   IsInsolvent(player, due, nlv)       → (Cash+nlv) < due 严格 <（F-10，纯谓词）
//
// 断言非 vacuous（flip 致真 FAIL）：
//   - TC1：ceil 路径 BC=75→37 非 38（整数截断==floor）
//   - TC5：变体C BC=75,house=3→111 非 112（逐栋 floor vs 合并 floor 坐实点）
//   - TC6：边界两侧 false/true（严格 <）
//
// 规范依据：story econ-008 AC-24~29/AC-32；ADR-0014（逐栋 floor）；ADR-0006（防 5→8 环）。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "EconomySubsystem.h"
#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "PlayerTurnTypes.h"
#include "EconomyTypes.h"

// =============================================================================
// 测试辅助（static 内部链接）
// =============================================================================
namespace NlvInsolvencyTestHelpers
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

    /** 构造 FNlvAssetEntry 辅助（减少 TC 内冗余）。 */
    static FNlvAssetEntry MakeEntry(int32 MV, bool bMort, int32 House, int32 BC)
    {
        FNlvAssetEntry E;
        E.MortgageValue = MV;
        E.bIsMortgaged  = bMort;
        E.HouseCount    = House;
        E.BuildingCost  = BC;
        return E;
    }

} // namespace NlvInsolvencyTestHelpers

// =============================================================================
// TC1（AC-24）—— F-8 建筑卖回整数 floor + 纯函数无副作用 + 决定性
//   BC=100→50；BC=75→floor(37.5)==37（非 38）；BC=1→floor(0.5)==0；BC=2→1。
//   纯函数不改 Cash、不触事件。两次同输入位级一致。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC1_BuildingSellbackFloor,
    "Rento.Economy.NlvInsolvency.TC1_BuildingSellbackFloor",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC1_BuildingSellbackFloor::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC1: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC1_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    // 核心 floor 验证：整数截断==floor（操作数≥0，ADR-0014 零 float）。
    TestEqual(TEXT("TC1: BC=100 → sellback==50"), Econ->ComputeBuildingSellback(100), 50);
    // 🔴 关键：BC=75 → floor(37.5)==37（非 38）。此值若改 ceil 则==38→FAIL，变异坐实。
    TestEqual(TEXT("TC1: BC=75 → floor(37.5)==37（非 38）"), Econ->ComputeBuildingSellback(75), 37);
    // BC=1 → floor(0.5)==0（截断）。
    TestEqual(TEXT("TC1: BC=1 → floor(0.5)==0"), Econ->ComputeBuildingSellback(1), 0);
    // BC=2 → floor(1.0)==1。
    TestEqual(TEXT("TC1: BC=2 → floor(1.0)==1"), Econ->ComputeBuildingSellback(2), 1);

    // 决定性：同 BC 两次一致（无 float，位级确定，AC-32）。
    TestEqual(TEXT("TC1: 决定性 BC=75 两次一致"),
        Econ->ComputeBuildingSellback(75), Econ->ComputeBuildingSellback(75));

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC2（AC-25）—— NLV 核心求和不变式
//   Assets=[MV100未抵押house0BC0, MV60未抵押house0BC0, MV0未抵押house=3 BC=100]
//   → nlv = 100 + 60 + 3×50 = 310。
//   （第 3 格 MortgageValue=0、house=3 BC=100 卖回 50/栋→贡献 150）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC2_NlvSumInvariant,
    "Rento.Economy.NlvInsolvency.TC2_NlvSumInvariant",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC2_NlvSumInvariant::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC2: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC2_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeEntry(/*MV=*/100, /*bMort=*/false, /*House=*/0, /*BC=*/0));  // MV 侧 100（house=0 不触 BC 防御）
    Assets.Add(MakeEntry(/*MV=*/60,  /*bMort=*/false, /*House=*/0, /*BC=*/0));  // MV 侧 60
    Assets.Add(MakeEntry(/*MV=*/0,   /*bMort=*/false, /*House=*/3, /*BC=*/100)); // 卖回侧 3×50=150

    // nlv = 100 + 60 + 3×floor(100/2) = 100 + 60 + 150 = 310（AC-25）。
    TestEqual(TEXT("TC2: 2 未抵押 MV + 3 栋 BC100 → nlv==310"), Econ->ComputeNetLiquidationValue(Assets), 310);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC3（AC-26）—— 已抵押地 MV 侧贡献 0
//   Assets=[MV100已抵押house0, MV60未抵押house0] → nlv==60（已抵押 MV100 贡献 0）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC3_MortgagedContributesZero,
    "Rento.Economy.NlvInsolvency.TC3_MortgagedContributesZero",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC3_MortgagedContributesZero::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC3: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC3_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeEntry(/*MV=*/100, /*bMort=*/true,  /*House=*/0, /*BC=*/0));  // 已抵押 → MV 侧 0
    Assets.Add(MakeEntry(/*MV=*/60,  /*bMort=*/false, /*House=*/0, /*BC=*/0));  // 未抵押 → MV 侧 60

    // 已抵押 MV100 贡献 0，nlv 只计未抵押 MV60（AC-26）。
    TestEqual(TEXT("TC3: 已抵押 MV100 贡献 0 → nlv==60"), Econ->ComputeNetLiquidationValue(Assets), 60);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC4（AC-27①）—— 综合 NLV 数值正确
//   PropA(MV=100 未抵押) + PropB(MV=60 已抵押 house=0) + 2 栋 BC=100（属 PropA）；Cash=30。
//   nlv = 100 + 0 + 2×floor(100/2) = 100 + 0 + 100 = 200。
//   并验证 IsInsolvent 边界：due=231→true；due=230→false（Cash30+nlv200=230）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC4_NlvComprehensive,
    "Rento.Economy.NlvInsolvency.TC4_NlvComprehensive",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC4_NlvComprehensive::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC4: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC4_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    // PropA：MV=100 未抵押，2 栋 BC=100（卖回 2×50=100）；PropB：MV=60 已抵押 house=0。
    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeEntry(/*MV=*/100, /*bMort=*/false, /*House=*/2, /*BC=*/100)); // PropA
    Assets.Add(MakeEntry(/*MV=*/60,  /*bMort=*/true,  /*House=*/0, /*BC=*/0));  // PropB 已抵押

    // nlv = MV_PropA(100) + 0(PropB 抵押) + 2×floor(100/2)(100) = 200（AC-27①）。
    const int32 Nlv = Econ->ComputeNetLiquidationValue(Assets);
    TestEqual(TEXT("TC4: nlv==100+0+2×50==200"), Nlv, 200);

    // Cash=30；Cash+nlv=230。
    Turn->FindPlayerById(0)->SetCash(30);

    // due=231 → 230<231==true（破产）。
    TestTrue(TEXT("TC4: Cash30+nlv200=230 < due=231 → is_insolvent true"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/231, /*PreaggregatedNlv=*/Nlv));
    // due=230 → 230<230==false（能付，付到恰好 0，AC-28）。
    TestFalse(TEXT("TC4: Cash30+nlv200=230 < due=230 → is_insolvent false（付到 0 能付）"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/230, /*PreaggregatedNlv=*/Nlv));

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC5（AC-32 🔴）—— 逐栋 floor 变异坐实 + 决定性
//   变体C：Assets=[house=3 BC=75] → nlv == 3×floor(75/2) == 3×37 == 111（**非 112**）。
//   若实现错用合并 floor：floor(3×75/2)=floor(112.5)=112 → 此断言 FAIL（变异坐实点）。
//   再附决定性：同 Assets 两次 ComputeNetLiquidationValue 一致；ComputeBuildingSellback(75) 两次==37。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC5_PerBuildingFloorMutation,
    "Rento.Economy.NlvInsolvency.TC5_PerBuildingFloorMutation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC5_PerBuildingFloorMutation::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC5: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC5_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeEntry(/*MV=*/0, /*bMort=*/false, /*House=*/3, /*BC=*/75));

    // 🔴 逐栋 floor 坐实：3×floor(75/2)=3×37=111（**非** floor(3×75/2)=floor(112.5)=112）。
    // 若实现误为合并 floor，此断言 FAIL——这是 AC-32 变体C 核心变异点。
    const int32 Nlv = Econ->ComputeNetLiquidationValue(Assets);
    TestEqual(TEXT("TC5: 🔴 变体C house=3 BC=75 → 3×37==111（非 112，逐栋 floor）"), Nlv, 111);

    // 决定性：同 Assets 两次 ComputeNetLiquidationValue 位级一致。
    const int32 Nlv2 = Econ->ComputeNetLiquidationValue(Assets);
    TestEqual(TEXT("TC5: 决定性 两次 ComputeNetLiquidationValue 一致"), Nlv, Nlv2);

    // 决定性：ComputeBuildingSellback(75) 两次==37（无 float，AC-32）。
    TestEqual(TEXT("TC5: 决定性 ComputeBuildingSellback(75)==37"),   Econ->ComputeBuildingSellback(75), 37);
    TestEqual(TEXT("TC5: 决定性 ComputeBuildingSellback(75) 两次一致"),
        Econ->ComputeBuildingSellback(75), Econ->ComputeBuildingSellback(75));

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC6（AC-28 🔴）—— is_insolvent 严格 < 边界
//   SetCash(0号玩家, 50)；nlv=120；Cash+nlv=170。
//   due=170 → 170<170==false（付到 0 算能付，不破产）。
//   due=171 → 170<171==true（破产）。
//   断言边界两侧（false / true），证明严格 < 而非 <=。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC6_InsolvencyStrictLessThan,
    "Rento.Economy.NlvInsolvency.TC6_InsolvencyStrictLessThan",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC6_InsolvencyStrictLessThan::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;
    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC6: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC6_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    Turn->FindPlayerById(0)->SetCash(50);   // Cash=50；Cash+nlv=50+120=170

    // 🔴 严格 < 边界：due==Cash+nlv 时不破产（付到恰好 0 算能付，AC-28）。
    TestFalse(TEXT("TC6: Cash=50 nlv=120 due=170 → 170<170==false（付到 0 能付，不破产）"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/170, /*PreaggregatedNlv=*/120));

    // 严格 < 右侧：due==Cash+nlv+1 时破产。
    TestTrue(TEXT("TC6: Cash=50 nlv=120 due=171 → 170<171==true（破产）"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/171, /*PreaggregatedNlv=*/120));

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC7（AC-27③/AC-29）—— 单一枚举入口语义证 + 防御边界
//   语义证：IsInsolvent 接收 int32 nlv 不枚举——同一 (player,due) 传不同 PreaggregatedNlv 得不同结果，
//     证明 F-10 消费传入值非自算（SetCash(0,50)；IsInsolvent(0,200,100)→150<200 true；
//     IsInsolvent(0,200,160)→210<200 false）。
//   防御：空 Assets→nlv==0；ComputeBuildingSellback(0)==0（触 "BuildingCost" Error，须 AddExpectedError）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC7_SingleEnumerationEntryAndGuard,
    "Rento.Economy.NlvInsolvency.TC7_SingleEnumerationEntryAndGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC7_SingleEnumerationEntryAndGuard::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;

    // BC=0 触发 1 条 "BuildingCost" Error（防御段调用 ComputeBuildingSellback(0)）。
    AddExpectedError(TEXT("BuildingCost"), EAutomationExpectedErrorFlags::Contains, 1);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC7: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC7_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    Turn->FindPlayerById(0)->SetCash(50);   // Cash=50

    // 语义证（AC-27③/AC-29）：IsInsolvent 消费传入的 PreaggregatedNlv，不自算。
    //   同一 (PlayerId=0, AmountDue=200)，传入不同 nlv 得不同结果——证明 F-10 消费传入值。
    //   nlv=100 → Cash+nlv=150 < 200 → true（破产）。
    TestTrue(TEXT("TC7: Cash=50+nlv=100=150 < due=200 → true（消费传入 nlv 非自算）"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/200, /*PreaggregatedNlv=*/100));
    //   nlv=160 → Cash+nlv=210 < 200 → false（不破产）——与上方结论相反，证明 IsInsolvent 消费传入值。
    TestFalse(TEXT("TC7: Cash=50+nlv=160=210 < due=200 → false（不同 nlv 不同结果）"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/200, /*PreaggregatedNlv=*/160));

    // 防御：空 Assets → ComputeNetLiquidationValue({}) == 0。
    TArray<FNlvAssetEntry> EmptyAssets;
    TestEqual(TEXT("TC7: 空 Assets → nlv==0"), Econ->ComputeNetLiquidationValue(EmptyAssets), 0);

    // 防御：BuildingCost=0（无效数据）→ ComputeBuildingSellback 返 0 + dev log（已 AddExpectedError）。
    // 注意：此调用触发 1 条 "BuildingCost" Error，已在 TC7 顶部 AddExpectedError 声明。
    TestEqual(TEXT("TC7: BC=0 → sellback==0（无效数据 guard）"), Econ->ComputeBuildingSellback(0), 0);

    DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC8（review W-1/W-2 防御）—— 负 HouseCount / 负 PreaggregatedNlv dev-log 守门
//   W-1：ComputeNetLiquidationValue 负 HouseCount → 卖回侧跳过（贡献 0）+ Error log；MV 侧仍计。
//   W-2：IsInsolvent 负 PreaggregatedNlv → 保守按 0 + Error log（不让负 nlv 污染破产判定）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FNlvInsolvency_TC8_NegativeInputGuards,
    "Rento.Economy.NlvInsolvency.TC8_NegativeInputGuards",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNlvInsolvency_TC8_NegativeInputGuards::RunTest(const FString& Parameters)
{
    using namespace NlvInsolvencyTestHelpers;

    // 负 HouseCount 触 1 条 "HouseCount" Error；2 次负 nlv IsInsolvent 触 2 条 "PreaggregatedNlv" Error。
    AddExpectedError(TEXT("HouseCount"), EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedError(TEXT("PreaggregatedNlv"), EAutomationExpectedErrorFlags::Contains, 2);

    UWorld* World = nullptr; UEconomySubsystem* Econ = nullptr; UPlayerTurnSubsystem* Turn = nullptr;
    if (!TestTrue(TEXT("TC8: SetupMatch"), SetupMatch(World, Econ, Turn, TEXT("Nlv_TC8_World"), 2)))
    {
        DestroyGameWorld(World); return false;
    }

    // W-1：负 HouseCount entry（未抵押 MV=100，house=-3 BC=75）→ MV 侧 100，卖回侧跳过（不算 -3×37）。
    TArray<FNlvAssetEntry> Assets;
    Assets.Add(MakeEntry(/*MV=*/100, /*bMort=*/false, /*House=*/-3, /*BC=*/75));
    TestEqual(TEXT("TC8: 负 HouseCount → 卖回侧跳过，仅 MV 侧 100"),
        Econ->ComputeNetLiquidationValue(Assets), 100);

    // W-2：负 PreaggregatedNlv → 保守按 0（GetCash < AmountDue），不加负值。
    Turn->FindPlayerById(0)->SetCash(50);   // Cash=50
    //   due=60：50<60==true（按 nlv=0，破产）。
    TestTrue(TEXT("TC8: 负 nlv 保守按 0 → Cash50<due60==true"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/60, /*PreaggregatedNlv=*/-999));
    //   due=40：50<40==false（按 nlv=0，能付）。
    TestFalse(TEXT("TC8: 负 nlv 保守按 0 → Cash50<due40==false"),
        Econ->IsInsolvent(/*PlayerId=*/0, /*AmountDue=*/40, /*PreaggregatedNlv=*/-999));

    DestroyGameWorld(World);
    return true;
}
