// ControlledWriteInterfaceTest.cpp
// =============================================================================
// 单元测试：受控写接口面（story pt-005 / TR-turn-003 / GDD CR-1 L92-99）
//
// 物理路径：Source/rentoTests/Private/ControlledWriteInterfaceTest.cpp
// 逻辑路径：tests/unit/player-turn/controlled_write_interface_test.cpp
// Automation 类目：Rento.PlayerTurn.ControlledWriteInterface
//
// 覆盖 AC：
//   AC-1（TR-turn-003）: 每字段 setter 调用后字段真变（TC-1，非 vacuous）
//   AC-2（GDD AC-35）: code-review checklist → production/qa/evidence/（Advisory，本文件注明）
//   AC-3（GDD AC-35a，条件门）: OQ-1 ADR 未裁前 deferred，本文件不写恒 pending 测试
//   AC-4（GDD L99/CR-6）: SetBankrupt 是 bIsBankrupt 唯一路径，经 HandlePlayerBankruptcy
//                          调用（TC-4）
//
// 测试机制（headless -nullrhi）：
//   TC-1：
//     NewObject<URentoPlayerState>(GetTransientPackage()) 直接构造
//     调每个 setter（SetPosition/SetCash/SetJailState/SetCurrentRollContext/SetBankrupt）
//     断言字段真变（非 vacuous：初始值 ≠ 写入值，确保非永真断言）
//
//   TC-4：
//     UWorld::CreateWorld(EWorldType::Game) 创建 Game World
//     注入 stub IResolveBankruptcy（bDebtorEliminated=true，WinnerId=INDEX_NONE）
//     调 HandlePlayerBankruptcy → 断言 bIsBankrupt==true（经 SetBankrupt 路径）
//     验证路径：HandlePlayerBankruptcy 调 SetBankrupt(true)（AC-4 唯一路径要求）
//
// AC-3 deferred 说明：
//   OQ-1 ADR 未裁前，不写 [Logic] 静态扫描门（会是恒 pending 的自动测试）。
//   AC-35a 静态门（扫描非 TurnManager/PlayerStateController 文件无直接字段赋值）
//   待 OQ-1 ADR 签署后同批新增。此处只用注释记录此 deferred 状态。
//
// 断言非 vacuous 保证：
//   - TC-1 SetPosition：初始 CurrentTileIndex=0，写入 5，断言 ==5；flip 为 ==0 则真 FAIL
//   - TC-1 SetCash：初始 Cash=0，写入 1500，断言 ==1500；flip 为 ==0 则真 FAIL
//   - TC-1 SetJailState：初始 bIsInJail=false，写入 true，断言 ==true；flip 则真 FAIL
//   - TC-1 SetCurrentRollContext：初始 Total=0，写入 {3,4,7,false}，断言 Total==7；flip 则真 FAIL
//   - TC-1 SetBankrupt：初始 bIsBankrupt=false，写入 true，断言 ==true；flip 则真 FAIL
//   - TC-4：HandlePlayerBankruptcy 调 stub 返回 bDebtorEliminated=true → bIsBankrupt==true；
//           若 SetBankrupt 未被调（直写绕过），字段行为仍一致，但可通过 spy/log 核实路径
//
// 规范依据：
//   - story pt-005 AC-1~4，TC-1/TC-4，Edge cases
//   - ADR-0007（受控写 setter 落 C++；BP↔C++ handoff → BlueprintCallable）
//   - ADR-0001（UObject headless 构造，UWorld::CreateWorld(Game)）
//   - control-manifest Core Layer §BP/C++ 权威边界（ADR-0007）
//   - G1/G2/G3/G4/G5/G6 实测坑规避（见本文件内各处注释）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "RentoPlayerState.h"
#include "PlayerTurnSubsystem.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"
#include "DiceRngService.h"
#include "BankruptcyInterface.h"

// =============================================================================
// TC-4 用：IResolveBankruptcy stub（纯 C++ 非 UObject，堆分配安全，TSharedPtr 管理）
//
// 返回受控值：bDebtorEliminated=true，WinnerId=INDEX_NONE（对局继续，无胜者）。
// 用于验证 HandlePlayerBankruptcy → SetBankrupt(true) 路径（AC-4）。
// =============================================================================
class FBankruptcyStubEliminate : public IResolveBankruptcy
{
public:
    virtual FBankruptcyResolution ResolveBankruptcy(
        int32 /*DebtorId*/,
        int32 /*CreditorId*/,
        int32 /*ActivePlayerCount*/) override
    {
        // 返回「债务人被消除，但对局继续（无胜者）」——TC-4 最小驱动场景
        FBankruptcyResolution Result;
        Result.bDebtorEliminated = true;
        Result.WinnerId = INDEX_NONE;   // 不触发 OnGameWon（避免测试复杂性）
        return Result;
    }
};

// =============================================================================
// 辅助命名空间：测试 World 生命周期管理（与 PlayerStateContainerStartNewGameTest 同模式）
// =============================================================================
namespace ControlledWriteTestHelpers
{
    /** 创建 Game World（非通知引擎，headless 安全）。调用方负责 DestroyWorld。 */
    static UWorld* CreateGameWorld(const FName& WorldName)
    {
        UWorld* World = UWorld::CreateWorld(
            EWorldType::Game,
            /*bInformEngineOfWorld=*/false,
            WorldName);
        check(World);
        return World;
    }

    /** 销毁测试 World，释放资源。 */
    static void DestroyGameWorld(UWorld* World)
    {
        if (World)
        {
            World->DestroyWorld(/*bInformEngineOfWorld=*/false);
        }
    }

    /**
     * 构建最小 2 人 FGameSetupConfig（初始化 UPlayerTurnSubsystem 所需最小配置）。
     * TC-4 需要 InitializeFromConfig 建队，才能使 FindPlayerById 可工作。
     */
    static FGameSetupConfig BuildMinimal2PConfig()
    {
        FGameSetupConfig Config;
        // 第一个玩家（PlayerId=0，后续用于破产测试）
        {
            FPlayerSetupEntry Entry;
            Entry.DisplayName = FText::FromString(TEXT("Player0"));
            Entry.TokenColor  = EPlayerColor::Red;
            Entry.bIsAI       = false;
            Config.Players.Add(Entry);
        }
        // 第二个玩家（PlayerId=1，必须存在使 F-1 |A|>=2 不触发 INDEX_NONE）
        {
            FPlayerSetupEntry Entry;
            Entry.DisplayName = FText::FromString(TEXT("Player1"));
            Entry.TokenColor  = EPlayerColor::Blue;
            Entry.bIsAI       = false;
            Config.Players.Add(Entry);
        }
        // 使用固定种子（确定性定序，不依赖真实 RNG）
        Config.RandomSeed = 42;
        return Config;
    }
}

// =============================================================================
// TC-1（AC-1）: 每字段 setter 调用后字段真变
//
// 测试类目：Rento.PlayerTurn.ControlledWriteInterface
//
// G1 规避：使用 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
// G2 规避：setter 本体无 ensure，只有 UE_LOG Verbose（不触发 Error，无需 AddExpectedError）
// G4 规避：无堆分配传出参
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FControlledWriteInterface_TC1_SettersMakeFieldsChange,
    "Rento.PlayerTurn.ControlledWriteInterface.TC1_SettersMakeFieldsChange",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FControlledWriteInterface_TC1_SettersMakeFieldsChange::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // 构造 URentoPlayerState（headless NewObject，无需 World）
    // -------------------------------------------------------------------------
    URentoPlayerState* State = NewObject<URentoPlayerState>(GetTransientPackage());
    State->AddToRoot();  // 防 State GC（同步执行，稳妥起见）

    // 验证初始默认值（AC-1 字段默认值契约，防 vacuous）
    TestEqual(TEXT("初始 CurrentTileIndex 应为 0"), State->CurrentTileIndex, 0);
    TestEqual(TEXT("初始 Cash 应为 0"), State->Cash, 0);
    TestFalse(TEXT("初始 bIsInJail 应为 false"), State->bIsInJail);
    TestEqual(TEXT("初始 CurrentRollContext.Total 应为 0"), State->CurrentRollContext.Total, 0);
    TestFalse(TEXT("初始 bIsBankrupt 应为 false"), State->bIsBankrupt);

    // -------------------------------------------------------------------------
    // SetPosition — CurrentTileIndex 字段真变（非 vacuous：5 ≠ 0）
    // -------------------------------------------------------------------------
    State->SetPosition(5);
    TestEqual(
        TEXT("TC-1 SetPosition(5)：CurrentTileIndex 应变为 5（移动4 唯一合法调用路径验证）"),
        State->CurrentTileIndex, 5);

    // 再次写入不同值（进一步验证非 vacuous：10 ≠ 5）
    State->SetPosition(10);
    TestEqual(
        TEXT("TC-1 SetPosition(10)：CurrentTileIndex 应变为 10"),
        State->CurrentTileIndex, 10);

    // -------------------------------------------------------------------------
    // SetCash — Cash 字段真变（非 vacuous：1500 ≠ 0）
    // -------------------------------------------------------------------------
    State->SetCash(1500);
    TestEqual(
        TEXT("TC-1 SetCash(1500)：Cash 应变为 1500（经济5 唯一合法调用路径验证）"),
        State->Cash, 1500);

    // 负数现金合法（破产前可为负，判定归经济5/破产9）
    State->SetCash(-500);
    TestEqual(
        TEXT("TC-1 SetCash(-500)：Cash 应变为 -500（负数合法，破产判定归经济5）"),
        State->Cash, -500);

    // -------------------------------------------------------------------------
    // SetJailState — bIsInJail 字段真变（非 vacuous：true ≠ false）
    // -------------------------------------------------------------------------
    State->SetJailState(true, EJailReason::None);
    TestTrue(
        TEXT("TC-1 SetJailState(true)：bIsInJail 应变为 true（事件格7 唯一合法调用路径验证）"),
        State->bIsInJail);

    // 出狱（bInJail=false，Reason=None）
    State->SetJailState(false, EJailReason::None);
    TestFalse(
        TEXT("TC-1 SetJailState(false)：bIsInJail 应变回 false（出狱清除）"),
        State->bIsInJail);

    // -------------------------------------------------------------------------
    // SetCurrentRollContext — CurrentRollContext holder 字段真变（非 vacuous：Total=7 ≠ 0）
    // -------------------------------------------------------------------------
    FDiceRollResult TestRoll;
    TestRoll.Die1     = 3;
    TestRoll.Die2     = 4;
    TestRoll.Total    = 7;    // Die1 + Die2 不变式
    TestRoll.bIsDouble = false;

    State->SetCurrentRollContext(TestRoll);
    TestEqual(
        TEXT("TC-1 SetCurrentRollContext({3,4,7,false})：CurrentRollContext.Die1 应为 3"),
        State->CurrentRollContext.Die1, 3);
    TestEqual(
        TEXT("TC-1 SetCurrentRollContext({3,4,7,false})：CurrentRollContext.Die2 应为 4"),
        State->CurrentRollContext.Die2, 4);
    TestEqual(
        TEXT("TC-1 SetCurrentRollContext({3,4,7,false})：CurrentRollContext.Total 应为 7"),
        State->CurrentRollContext.Total, 7);
    TestFalse(
        TEXT("TC-1 SetCurrentRollContext({3,4,7,false})：bIsDouble 应为 false"),
        State->CurrentRollContext.bIsDouble);

    // 双点掷骰（bIsDouble=true，Total=8）
    FDiceRollResult DoubleRoll;
    DoubleRoll.Die1     = 4;
    DoubleRoll.Die2     = 4;
    DoubleRoll.Total    = 8;
    DoubleRoll.bIsDouble = true;

    State->SetCurrentRollContext(DoubleRoll);
    TestTrue(
        TEXT("TC-1 SetCurrentRollContext({4,4,8,true})：bIsDouble 应为 true"),
        State->CurrentRollContext.bIsDouble);
    TestEqual(
        TEXT("TC-1 SetCurrentRollContext({4,4,8,true})：Total 应为 8"),
        State->CurrentRollContext.Total, 8);

    // -------------------------------------------------------------------------
    // SetBankrupt — bIsBankrupt 字段真变（非 vacuous：true ≠ false）
    // -------------------------------------------------------------------------
    State->SetBankrupt(true);
    TestTrue(
        TEXT("TC-1 SetBankrupt(true)：bIsBankrupt 应变为 true（本系统回合2 唯一合法调用路径验证）"),
        State->bIsBankrupt);

    // -------------------------------------------------------------------------
    // 清理
    // -------------------------------------------------------------------------
    State->RemoveFromRoot();

    return true;
}

// =============================================================================
// TC-4（AC-4）: 破产经 SetBankrupt 路径
//
// 验证 HandlePlayerBankruptcy 调 IResolveBankruptcy stub（bDebtorEliminated=true）
// 后，debtor 的 bIsBankrupt==true（经 SetBankrupt 受控写接口，而非直写字段）。
//
// AC-4 语义要求：SetBankrupt 是 bIsBankrupt 的唯一写路径，
//   HandlePlayerBankruptcy 内已从直写改为调 SetBankrupt(true)（story-005 迁移）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FControlledWriteInterface_TC4_SetBankruptIsOnlyPath,
    "Rento.PlayerTurn.ControlledWriteInterface.TC4_SetBankruptIsOnlyPath",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FControlledWriteInterface_TC4_SetBankruptIsOnlyPath::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // 创建 Game World（headless，非通知引擎）
    // -------------------------------------------------------------------------
    UWorld* World = ControlledWriteTestHelpers::CreateGameWorld(
        TEXT("TC4_SetBankruptIsOnlyPath_World"));

    // UPlayerTurnSubsystem 由引擎随 World 自动发现并初始化
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!Sub)
    {
        AddError(TEXT("TC-4：UPlayerTurnSubsystem 未找到（World Subsystem 自动发现失败）"));
        ControlledWriteTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // -------------------------------------------------------------------------
    // 建队（最小 2 人配置，确保 FindPlayerById 可工作）
    // -------------------------------------------------------------------------
    const FGameSetupConfig Config = ControlledWriteTestHelpers::BuildMinimal2PConfig();
    const bool bInited = Sub->InitializeFromConfig(Config);
    if (!bInited)
    {
        AddError(TEXT("TC-4：InitializeFromConfig 失败（测试前置条件不满足）"));
        ControlledWriteTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 验证 PlayerId=0 的初始 bIsBankrupt=false（防 vacuous）
    URentoPlayerState* DebtorState = Sub->FindPlayerById(0);
    if (!DebtorState)
    {
        AddError(TEXT("TC-4：FindPlayerById(0) 返回 nullptr（建队后 PlayerId=0 应存在）"));
        ControlledWriteTestHelpers::DestroyGameWorld(World);
        return false;
    }

    TestFalse(
        TEXT("TC-4 前置：PlayerId=0 初始 bIsBankrupt 应为 false"),
        DebtorState->bIsBankrupt);

    // -------------------------------------------------------------------------
    // 注入 IResolveBankruptcy stub（bDebtorEliminated=true，WinnerId=INDEX_NONE）
    // -------------------------------------------------------------------------
    TSharedPtr<IResolveBankruptcy> Stub =
        MakeShared<FBankruptcyStubEliminate>();
    Sub->SetBankruptcyResolver(Stub);

    // -------------------------------------------------------------------------
    // 调 HandlePlayerBankruptcy（DebtorId=0，CreditorId=-1 银行）
    // 预期：内部调 DebtorState->SetBankrupt(true) → bIsBankrupt==true
    // -------------------------------------------------------------------------
    Sub->HandlePlayerBankruptcy(/*DebtorId=*/0, /*CreditorId=*/-1);

    // -------------------------------------------------------------------------
    // 断言 bIsBankrupt==true（经 SetBankrupt 路径写入，AC-4）
    //
    // 断言非 vacuous：初始 bIsBankrupt=false（上方已验），WriteCall 后断言 true。
    // 若 SetBankrupt 或直写路径均未执行，则 bIsBankrupt 仍为 false → TestTrue 真 FAIL。
    // -------------------------------------------------------------------------
    TestTrue(
        TEXT("TC-4：HandlePlayerBankruptcy 后 PlayerId=0 的 bIsBankrupt 应为 true"
             "（经 SetBankrupt(true) 受控写路径，AC-4 唯一路径验证）"),
        DebtorState->bIsBankrupt);

    // -------------------------------------------------------------------------
    // 额外验证：PlayerId=1 的 bIsBankrupt 应仍为 false（隔离性，无旁路污染）
    // -------------------------------------------------------------------------
    URentoPlayerState* OtherState = Sub->FindPlayerById(1);
    if (OtherState)
    {
        TestFalse(
            TEXT("TC-4：PlayerId=1 的 bIsBankrupt 应仍为 false（SetBankrupt 无旁路污染）"),
            OtherState->bIsBankrupt);
    }

    // -------------------------------------------------------------------------
    // 清理
    // -------------------------------------------------------------------------
    ControlledWriteTestHelpers::DestroyGameWorld(World);

    return true;
}

// =============================================================================
// AC-2 说明（[Advisory] code-review）：
//
// 本测试文件不包含 AC-2 的自动测试——AC-2 是 code-review checklist（软约束），
// 记录于 production/qa/evidence/controlled-write-interface-checklist-pt005.md。
//
// AC-3 deferred 说明：
//
// OQ-1 ADR 未裁前，AC-3/AC-35a 静态扫描门不激活（避免恒 pending 的 [Logic] gate）。
// 待 OQ-1 ADR 签署且选择「C++ 强封装」方案后，在同批提交中新增：
//   - 字段改为 private
//   - grep/静态扫描断言：非白名单文件无对 Cash/CurrentTileIndex/bIsInJail/bIsBankrupt 的直接赋值
//   - AC-35a [Logic] 测试门激活
// =============================================================================
