// NextActivePlayerBankruptcyOnGameWonTest.cpp
// =============================================================================
// 单元测试：F-1 NextActivePlayer + F-2 ActivePlayerCount + 破产移出 + OnGameWon
// story pt-003 AC-1~8 + Edge 全覆盖
//
// 物理路径：Source/rentoTests/Private/NextActivePlayerBankruptcyOnGameWonTest.cpp
// 逻辑路径：tests/unit/player-turn/nextactiveplayer_bankruptcy_ongamewon_test.cpp
// Automation 类目：Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon
//
// 测试机制（headless -nullrhi）：
//   - F-1 / F-2 静态纯函数：直接调用，无需 World，完全 headless 确定性
//   - HandlePlayerBankruptcy：UWorld::CreateWorld(EWorldType::Game) + InitializeFromConfig
//     + SetBankruptcyResolver(stub) 注入受控返回值
//   - OnGameWon spy：文件作用域 UCLASS（UOnGameWonSpy）+ AddDynamic 绑定
//     （DYNAMIC_MULTICAST_DELEGATE 只能由文件作用域 UCLASS 的 UFUNCTION 绑定，
//      禁 namespace 内——UHT 约束，参 TurnPhaseSpy.h 同模式）
//   - AddExpectedError 吞预期 ensure/Error（|A|=0、APC=0 路径）
//
// 断言非 vacuous 保证（每条 flip 均可致真 FAIL）：
//   - TC-1/2/3：F-1 输入 → 预期输出 TestEqual；若枚举错则 FAIL
//   - TC-4：F-2 计数 TestEqual；若计数逻辑错则 FAIL
//   - TC-6：OnGameWon spy 计数 TestEqual(1)；若不广播则 FAIL
//   - TC-7：边沿幂等计数 TestEqual(1)；若重复广播则 FAIL
//   - TC-8：AC-8 封堵计数 TestEqual(0)；若意外广播则 FAIL
//
// 规范依据：
//   - story pt-003 AC-1~8，TC-1~8，Edge
//   - ADR-0003（OnGameWon 单一事件源：回合2 触发，9 return-only）
//   - ADR-0007（[Logic] BLOCKING AC 被测逻辑落 C++，-nullrhi 可测）
//   - control-manifest Foundation Layer §事件总线（ADR-0003）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "PlayerTurnSubsystem.h"
#include "RentoPlayerState.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"
#include "BankruptcyInterface.h"

// OnGameWon spy 头文件（文件作用域 UCLASS，在本文件下方定义后生成）
#include "OnGameWonSpy.h"

// =============================================================================
// IResolveBankruptcy Stub 实现（纯 C++，headless 确定性）
//
// 不继承 UObject，不需要 UHT——纯 C++ 接口 stub。
// 每次调 ResolveBankruptcy 返回预设的受控值，绝不回调任何系统（AC-6 return-only 保证）。
// =============================================================================
struct FBankruptcyResolverStub : public IResolveBankruptcy
{
    /** 受控返回值（测试前设定） */
    FBankruptcyResolution ControlledResult;

    /** 调用计数（验证 ResolveBankruptcy 确实被调用） */
    int32 CallCount = 0;

    /**
     * 构造时设置受控返回值。
     * @param bEliminated 债务人是否被消除
     * @param InWinnerId  获胜玩家 PlayerId（INDEX_NONE=无胜者）
     */
    FBankruptcyResolverStub(bool bEliminated, int32 InWinnerId)
    {
        ControlledResult.bDebtorEliminated = bEliminated;
        ControlledResult.WinnerId = InWinnerId;
    }

    /** return-only 实现（绝不回调，AC-6 无环安全阀） */
    virtual FBankruptcyResolution ResolveBankruptcy(
        int32 /*DebtorId*/,
        int32 /*CreditorId*/,
        int32 /*ActivePlayerCount*/) override
    {
        ++CallCount;
        return ControlledResult;
    }
};

// =============================================================================
// 测试辅助（纯 C++ 函数，无 UHT/UCLASS，headless 安全）
// =============================================================================
namespace BankruptcyTestHelpers
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
     * TiebreakRounds=1：快速触发席位裁定（避免 DiceService 依赖）。
     */
    static FGameSetupConfig MakeConfig(int32 N)
    {
        FGameSetupConfig Config;
        Config.DoublesJailThreshold = 3;
        Config.MaxTiebreakRounds    = 1;
        Config.RandomSeed           = 42;

        // EPlayerColor 枚举值：1=Red, 2=Blue, 3=Green, 4=Yellow（4色已足够4人局）
        const EPlayerColor Colors[] = {
            EPlayerColor::Red,
            EPlayerColor::Blue,
            EPlayerColor::Green,
            EPlayerColor::Yellow
        };

        for (int32 i = 0; i < N; ++i)
        {
            FPlayerSetupEntry Entry;
            Entry.DisplayName   = FText::FromString(FString::Printf(TEXT("Player%d"), i));
            Entry.TokenColor    = Colors[i % 4];
            Entry.bIsAI         = false;
            Entry.AIDifficulty  = EAIDifficulty::None;
            Config.Players.Add(Entry);
        }
        return Config;
    }

    /**
     * 按 TurnOrderIndex 查找对应的 PlayerState。
     * O(N)，测试用，N<=4 可接受。
     */
    static URentoPlayerState* FindStateByTurnOrder(
        UPlayerTurnSubsystem* Sub, int32 TurnOrderIndex)
    {
        for (const TObjectPtr<URentoPlayerState>& State : Sub->GetPlayerStates())
        {
            if (State && State->TurnOrderIndex == TurnOrderIndex)
            {
                return State.Get();
            }
        }
        return nullptr;
    }
}

// =============================================================================
// TC-1（AC-1）：F-1 基础寻路三变体
//
// 验证 NextActivePlayer 在 |A|≥2 时的基础枚举逻辑：
//   (2,4,{0,2,3}) → 3（直接找下一个）
//   (3,4,{0,3})   → 0（环绕）
//   (0,4,{0,3})   → 3（跳过1,2）
//
// 非 vacuous 保证：若改 {0,2,3} 为 {0,2}，则预期从 2 往后应得 0，TestEqual(3) 真 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_NextActivePlayer_BasicRouting,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC1_F1_BasicRouting",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_NextActivePlayer_BasicRouting::RunTest(const FString& Parameters)
{
    // 变体 1：(cur=2, P=4, A={0,2,3}) → 3（直接下一个 A 成员）
    {
        TSet<int32> A = {0, 2, 3};
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(2, 4, A);
        TestEqual(TEXT("TC1-变体1: (2,4,{0,2,3})→3"), Result, 3);
    }

    // 变体 2：(cur=3, P=4, A={0,3}) → 0（环绕：3→(3+1)%4=0 ∈ {0,3}）
    {
        TSet<int32> A = {0, 3};
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(3, 4, A);
        TestEqual(TEXT("TC1-变体2: (3,4,{0,3})→0 (环绕)"), Result, 0);
    }

    // 变体 3：(cur=0, P=4, A={0,3}) → 3（跳过1,2）
    {
        TSet<int32> A = {0, 3};
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(0, 4, A);
        TestEqual(TEXT("TC1-变体3: (0,4,{0,3})→3 (跳过1,2)"), Result, 3);
    }

    return true;
}

// =============================================================================
// TC-2（AC-2）：F-1 守卫——|A|=1 返回 INDEX_NONE
//
// 验证守卫判据仅看 |A|，与 cur 是否∈A 无关：
//   (0,4,{0}) → INDEX_NONE（|A|=1，唯一存活者==cur）
//   (0,2,{1}) → INDEX_NONE（|A|=1，唯一存活者≠cur，断言不返回1）
//
// 非 vacuous 保证：若守卫改为「cur∈A→继续枚举」，第二变体返回1，TestEqual(INDEX_NONE) 真 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_NextActivePlayer_Guard_SingleSurvivor,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC2_F1_Guard_SingleSurvivor",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_NextActivePlayer_Guard_SingleSurvivor::RunTest(const FString& Parameters)
{
    // 变体 1：(0,4,{0}) → INDEX_NONE（|A|=1，唯一存活者==cur）
    {
        TSet<int32> A = {0};
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(0, 4, A);
        TestEqual(TEXT("TC2-变体1: (0,4,{0})→INDEX_NONE (|A|=1,存活者==cur)"),
            Result, INDEX_NONE);
    }

    // 变体 2：(0,2,{1}) → INDEX_NONE（|A|=1，唯一存活者≠cur，守卫先于枚举）
    // ⚠ 断言不返回 1——若守卫没触发（错误实现枚举路径），Result=1，TestEqual(INDEX_NONE) 真 FAIL
    {
        TSet<int32> A = {1};
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(0, 2, A);
        TestEqual(TEXT("TC2-变体2: (0,2,{1})→INDEX_NONE (|A|=1,守卫先于枚举,不返回1)"),
            Result, INDEX_NONE);
        // 额外断言：确保不返回1（如果返回1则此断言也 FAIL）
        TestNotEqual(TEXT("TC2-变体2: 结果不应为1（守卫未触发是 bug）"), Result, 1);
    }

    return true;
}

// =============================================================================
// TC-3（AC-3）：F-1 入口规范化——负数/超界 cur 的终止性
//
// GIVEN A={0,1,2,3}，P=4（全员存活），验证三个负数/超界变体：
//   cur=-4 → cur_safe=0 → next=1
//   cur=-2 → cur_safe=2 → next=3
//   cur=5  → cur_safe=1 → next=2
//
// 断言：next∈A 且 ≠cur_safe，不返回负值，不无命中终止
// 非 vacuous 保证：若改 cur=-4 为 cur=0，则 next=1，TestEqual(1) 仍通过；
//   但若算法不规范化而直接 %4=-4%4=0 (负)，候选 Candidate=(0+1)%4=1，恰巧通过；
//   换 cur=-2 → cur_safe=2，Candidate=3，若不规范化 cur%4=-2 (负)，
//   (-2+1)%4=-1%4 在 C++ 为 -1，负数 candidate，不在 {0,1,2,3}，会找错。
//   关键判据：TestTrue(Result >= 0)
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_NextActivePlayer_NegativeCurNormalization,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC3_F1_NegativeCurNormalization",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_NextActivePlayer_NegativeCurNormalization::RunTest(const FString& Parameters)
{
    // 注：越界 cur 的 dev 诊断已改 UE_LOG(Warning)（handled 情形，规范化兜住）——
    // Warning 不致 Automation FAIL，故本测试无需 AddExpectedError，直接验规范化输出。

    TSet<int32> A = {0, 1, 2, 3};  // 全员存活，P=4

    // 变体 1：cur=-4 → cur_safe = ((-4%4)+4)%4 = (0+4)%4 = 0 → next=1
    {
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(-4, 4, A);
        TestEqual(TEXT("TC3-变体1: cur=-4 → cur_safe=0 → next=1"), Result, 1);
        TestTrue(TEXT("TC3-变体1: 结果不为负"), Result >= 0);
        // cur_safe=0，next 应≠0
        TestNotEqual(TEXT("TC3-变体1: next≠cur_safe(0)"), Result, 0);
    }

    // 变体 2：cur=-2 → cur_safe = ((-2%4)+4)%4 = (-2+4)%4 = 2 → next=3
    {
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(-2, 4, A);
        TestEqual(TEXT("TC3-变体2: cur=-2 → cur_safe=2 → next=3"), Result, 3);
        TestTrue(TEXT("TC3-变体2: 结果不为负"), Result >= 0);
        // cur_safe=2，next 应≠2
        TestNotEqual(TEXT("TC3-变体2: next≠cur_safe(2)"), Result, 2);
    }

    // 变体 3：cur=5 → cur_safe = ((5%4)+4)%4 = (1+4)%4 = 1 → next=2
    {
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(5, 4, A);
        TestEqual(TEXT("TC3-变体3: cur=5 → cur_safe=1 → next=2"), Result, 2);
        TestTrue(TEXT("TC3-变体3: 结果不为负"), Result >= 0);
        // cur_safe=1，next 应≠1
        TestNotEqual(TEXT("TC3-变体3: next≠cur_safe(1)"), Result, 1);
    }

    return true;
}

// =============================================================================
// TC-4（AC-4）：F-2 ActivePlayerCount 计数三变体
//
// [F,T,F,F] → 3（3 个活跃）
// [F,T,T,T] → 1（1 个活跃）
// [T,T,T,T] → 0 触发 ensureMsgf（正常流程不可达）
//
// 非 vacuous 保证：若改 [F,T,F,F] 预期为 4，TestEqual(3) 真 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_ActivePlayerCount,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC4_F2_ActivePlayerCount",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_ActivePlayerCount::RunTest(const FString& Parameters)
{
    // [F,T,F,F] → 3
    {
        TArray<bool> Flags = {false, true, false, false};
        const int32 APC = UPlayerTurnSubsystem::ActivePlayerCount(Flags);
        TestEqual(TEXT("TC4: [F,T,F,F]→3"), APC, 3);
    }

    // [F,T,T,T] → 1
    {
        TArray<bool> Flags = {false, true, true, true};
        const int32 APC = UPlayerTurnSubsystem::ActivePlayerCount(Flags);
        TestEqual(TEXT("TC4: [F,T,T,T]→1"), APC, 1);
    }

    // [T,T,T,T] → 0，触发 ensureMsgf（APC=0 正常不可达）
    // AddExpectedError 吞 ensure 消息（ASCII 安全子串）
    {
        AddExpectedError(TEXT("All players are bankrupt"), EAutomationExpectedErrorFlags::Contains, 1);
        TArray<bool> Flags = {true, true, true, true};
        const int32 APC = UPlayerTurnSubsystem::ActivePlayerCount(Flags);
        TestEqual(TEXT("TC4: [T,T,T,T]→0 (触发ensure)"), APC, 0);
    }

    return true;
}

// =============================================================================
// TC-5（AC-5）：当前玩家自回合破产 → TurnEnd 移出 + F-1 推进
//
// GIVEN：4 人局，玩家 TurnOrderIndex=0 的玩家（Player0）开始回合
//   mock HandlePlayerBankruptcy stub 置 bIsBankrupt=true（模拟破产）
// WHEN：EndTurn（正常阶段序列：StartTurn→ProcessRoll→AdvanceMove→AdvanceResolve→EndTurn）
// THEN：F-1 跳过 TurnOrderIndex=0（已破产），移交下一活跃玩家
//
// 双点后落地破产：stub 注入 bDebtorEliminated=true + 无胜者，验证移出无额外回合
//
// 非 vacuous 保证：若不跳过破产玩家，CurrentActivePlayerId 仍为 Player0，FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_HandleBankruptcy_TurnAdvance,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC5_BankruptcyTurnAdvance",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_HandleBankruptcy_TurnAdvance::RunTest(const FString& Parameters)
{
    // 创建 4 人局 World
    UWorld* World = BankruptcyTestHelpers::CreateGameWorld(TEXT("TC5_World"));
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC5: UPlayerTurnSubsystem 不为 null"), Sub))
    {
        BankruptcyTestHelpers::DestroyGameWorld(World);
        return false;
    }

    FGameSetupConfig Config = BankruptcyTestHelpers::MakeConfig(4);
    if (!TestTrue(TEXT("TC5: InitializeFromConfig 应成功"), Sub->InitializeFromConfig(Config)))
    {
        BankruptcyTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 找 TurnOrderIndex=0 的玩家（先手玩家）
    URentoPlayerState* FirstPlayer = BankruptcyTestHelpers::FindStateByTurnOrder(Sub, 0);
    if (!TestNotNull(TEXT("TC5: TurnOrderIndex=0 的玩家应存在"), FirstPlayer))
    {
        BankruptcyTestHelpers::DestroyGameWorld(World);
        return false;
    }
    const int32 FirstPlayerId = FirstPlayer->PlayerId;

    // 注入不触发胜利的 stub（bDebtorEliminated=true, WinnerId=INDEX_NONE → 对局继续）
    auto Stub = MakeShared<FBankruptcyResolverStub>(true, INDEX_NONE);
    Sub->SetBankruptcyResolver(Stub);

    // 开始先手玩家的回合
    Sub->StartTurn(FirstPlayerId);

    // 模拟回合阶段序列（正常路径）：RollPhase → MovePhase → ResolvePhase → PostRollAction
    bool bDummySentToJail = false; Sub->ProcessRollResult(/*bIsDouble=*/false, bDummySentToJail);
    Sub->AdvanceFromMovePhase();
    Sub->AdvanceFromResolvePhase();

    // 触发破产（置 bIsBankrupt=true + 移出轮转）
    Sub->HandlePlayerBankruptcy(FirstPlayerId, -1);
    TestTrue(TEXT("TC5: 破产玩家 bIsBankrupt 应为 true"),
        FirstPlayer->bIsBankrupt);

    // EndTurn → F-1 跳过已破产的 TurnOrderIndex=0，移交下一活跃玩家
    Sub->EndTurn(/*bSentToJailThisTurn=*/false);

    // 验证：当前活跃玩家不再是 FirstPlayerId（已破产被跳过）
    const int32 NewActiveId = Sub->GetCurrentActivePlayerId();
    TestNotEqual(TEXT("TC5: TurnEnd 后应移交到其他玩家（破产者已被跳过）"),
        NewActiveId, FirstPlayerId);

    // 验证新玩家未破产
    URentoPlayerState* NewPlayerState = Sub->FindPlayerById(NewActiveId);
    if (TestNotNull(TEXT("TC5: 新活跃玩家的 PlayerState 应存在"), NewPlayerState))
    {
        TestFalse(TEXT("TC5: 新活跃玩家不应已破产"),
            NewPlayerState->bIsBankrupt);
    }

    BankruptcyTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-6（AC-6）：HandlePlayerBankruptcy + IResolveBankruptcy stub 注入
//
// 分支 A：stub 返回 {bDebtorEliminated=true, WinnerId==Player1.PlayerId}（APC==1）
//   → OnGameWon 计数==1，WinnerId 正确，bIsBankrupt 置 true，移出轮转
//
// 分支 B：stub 返回 {bDebtorEliminated=true, WinnerId==INDEX_NONE}（APC>1）
//   → OnGameWon 计数==0，bIsBankrupt 仍置 true
//
// ⚠ 此条不得降级（return-only 无环架构安全阀的机器证明底线，AC-6）
//
// 非 vacuous 保证：
//   分支 A：若广播 0 次，TestEqual(1) 真 FAIL
//   分支 B：若意外广播 1 次，TestEqual(0) 真 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_HandleBankruptcy_OnGameWon_Trigger,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC6_OnGameWon_Trigger",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_HandleBankruptcy_OnGameWon_Trigger::RunTest(const FString& Parameters)
{
    // === 分支 A：stub 返回有效 WinnerId → OnGameWon 触发 1 次 ===
    {
        UWorld* World = BankruptcyTestHelpers::CreateGameWorld(TEXT("TC6_BranchA_World"));
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        check(Sub);

        FGameSetupConfig Config = BankruptcyTestHelpers::MakeConfig(2);
        Sub->InitializeFromConfig(Config);

        // 找两个玩家
        const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
        check(States.Num() == 2);
        const int32 DebtorId  = States[0]->PlayerId;
        const int32 WinnerId  = States[1]->PlayerId;

        // OnGameWon spy：文件作用域 UCLASS（UOnGameWonSpy），记录 WinnerId 序列
        UOnGameWonSpy* Spy = NewObject<UOnGameWonSpy>(GetTransientPackage());
        Spy->AddToRoot();
        Sub->OnGameWon.AddDynamic(Spy, &UOnGameWonSpy::HandleGameWon);

        // 注入 stub：bDebtorEliminated=true, WinnerId=States[1]->PlayerId（APC==1 路径）
        auto Stub = MakeShared<FBankruptcyResolverStub>(true, WinnerId);
        Sub->SetBankruptcyResolver(Stub);

        // 触发破产
        Sub->HandlePlayerBankruptcy(DebtorId, -1);

        // 断言：OnGameWon 广播恰 1 次
        TestEqual(TEXT("TC6-A: OnGameWon 广播计数应==1"), Spy->BroadcastCount, 1);
        // 断言：WinnerId payload 正确
        if (Spy->RecordedWinnerIds.Num() > 0)
        {
            TestEqual(TEXT("TC6-A: OnGameWon WinnerId payload 正确"),
                Spy->RecordedWinnerIds[0], WinnerId);
        }
        // 断言：debtor bIsBankrupt=true
        TestTrue(TEXT("TC6-A: Debtor bIsBankrupt 应为 true"),
            States[0]->bIsBankrupt);

        Sub->OnGameWon.RemoveDynamic(Spy, &UOnGameWonSpy::HandleGameWon);
        Spy->RemoveFromRoot();
        BankruptcyTestHelpers::DestroyGameWorld(World);
    }

    // === 分支 B：stub 返回 WinnerId==INDEX_NONE → OnGameWon 不触发 ===
    {
        UWorld* World = BankruptcyTestHelpers::CreateGameWorld(TEXT("TC6_BranchB_World"));
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        check(Sub);

        FGameSetupConfig Config = BankruptcyTestHelpers::MakeConfig(3);
        Sub->InitializeFromConfig(Config);

        const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
        check(States.Num() == 3);
        const int32 DebtorId = States[0]->PlayerId;

        UOnGameWonSpy* Spy = NewObject<UOnGameWonSpy>(GetTransientPackage());
        Spy->AddToRoot();
        Sub->OnGameWon.AddDynamic(Spy, &UOnGameWonSpy::HandleGameWon);

        // stub：bDebtorEliminated=true, WinnerId=INDEX_NONE（APC>1，对局继续）
        auto Stub = MakeShared<FBankruptcyResolverStub>(true, INDEX_NONE);
        Sub->SetBankruptcyResolver(Stub);

        Sub->HandlePlayerBankruptcy(DebtorId, -1);

        // 断言：OnGameWon 广播计数==0
        TestEqual(TEXT("TC6-B: WinnerId=INDEX_NONE 时 OnGameWon 不触发，计数应==0"),
            Spy->BroadcastCount, 0);
        // 断言：debtor bIsBankrupt 仍被置 true（bDebtorEliminated=true）
        TestTrue(TEXT("TC6-B: bDebtorEliminated=true 时 bIsBankrupt 应置 true"),
            States[0]->bIsBankrupt);

        Sub->OnGameWon.RemoveDynamic(Spy, &UOnGameWonSpy::HandleGameWon);
        Spy->RemoveFromRoot();
        BankruptcyTestHelpers::DestroyGameWorld(World);
    }

    return true;
}

// =============================================================================
// TC-7（AC-7）：OnGameWon 边沿幂等——连续两次相同 WinnerId，计数精确==1
//
// GIVEN：stub 连续两次返回 {bDebtorEliminated=true, WinnerId==Player1}
// WHEN：两次调 HandlePlayerBankruptcy（第二次已进 Winner 终态）
// THEN：OnGameWon 监听计数精确==1（第二次被 bGameWon 边沿守卫拦截）
//       同时断言 F-1 对已破产局返回 INDEX_NONE（验证移出轮转生效）
//
// 非 vacuous 保证：若边沿守卫不拦截，第二次广播后计数=2，TestEqual(1) 真 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_OnGameWon_EdgeIdempotent,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC7_OnGameWon_EdgeIdempotent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_OnGameWon_EdgeIdempotent::RunTest(const FString& Parameters)
{
    UWorld* World = BankruptcyTestHelpers::CreateGameWorld(TEXT("TC7_World"));
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    check(Sub);

    FGameSetupConfig Config = BankruptcyTestHelpers::MakeConfig(2);
    Sub->InitializeFromConfig(Config);

    const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
    check(States.Num() == 2);
    const int32 DebtorId = States[0]->PlayerId;
    const int32 WinnerIdVal = States[1]->PlayerId;

    // spy
    UOnGameWonSpy* Spy = NewObject<UOnGameWonSpy>(GetTransientPackage());
    Spy->AddToRoot();
    Sub->OnGameWon.AddDynamic(Spy, &UOnGameWonSpy::HandleGameWon);

    // stub：每次调用均返回同一 WinnerId（bDebtorEliminated=true）
    auto Stub = MakeShared<FBankruptcyResolverStub>(true, WinnerIdVal);
    Sub->SetBankruptcyResolver(Stub);

    // 第一次调用 → 触发 OnGameWon，bGameWon=true
    Sub->HandlePlayerBankruptcy(DebtorId, -1);
    TestEqual(TEXT("TC7: 第一次调用后 OnGameWon 计数应==1"),
        Spy->BroadcastCount, 1);
    TestTrue(TEXT("TC7: 第一次调用后 bGameWon 应为 true"),
        Sub->bGameWon);

    // 第二次调用 → bGameWon 边沿守卫拦截，不重发
    Sub->HandlePlayerBankruptcy(DebtorId, -1);
    TestEqual(TEXT("TC7: 第二次调用后 OnGameWon 计数仍应精确==1（边沿守卫拦截）"),
        Spy->BroadcastCount, 1);

    // 验证 F-1 守卫：此时所有活跃玩家只有 Winner（|A|=1），NextActivePlayer 返回 INDEX_NONE
    {
        TSet<int32> ActiveIndices;
        for (const TObjectPtr<URentoPlayerState>& State : Sub->GetPlayerStates())
        {
            if (State && !State->bIsBankrupt)
            {
                ActiveIndices.Add(State->TurnOrderIndex);
            }
        }
        // DebtorId 已 bIsBankrupt=true，活跃集合 |A| 应为 1
        TestEqual(TEXT("TC7: 破产后活跃集合大小应为1"), ActiveIndices.Num(), 1);

        // F-1 守卫：|A|=1 → INDEX_NONE
        const int32 F1Result = UPlayerTurnSubsystem::NextActivePlayer(0, 2, ActiveIndices);
        TestEqual(TEXT("TC7: F-1 对 |A|=1 应返回 INDEX_NONE"), F1Result, INDEX_NONE);
    }

    Sub->OnGameWon.RemoveDynamic(Spy, &UOnGameWonSpy::HandleGameWon);
    Spy->RemoveFromRoot();
    BankruptcyTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// TC-8（AC-8）：封堵 2↔9 双触发
//
// GIVEN：玩家A bIsBankrupt=true（直接置 flag，不经 ResolveBankruptcy）
//        玩家B bIsBankrupt=false
// WHEN：B TurnEnd + F-1 推进（本回合无任何 ResolveBankruptcy 调用）
// THEN：OnGameWon 广播计数==0（正常 TurnEnd 路径绝不因 APC==1 独立触发）
//
// ⚠ 这是 AC-8 封堵旧 2↔9 双触发的机器证明底线
// 非 vacuous 保证：若 EndTurn 内有「if APC==1 → broadcast OnGameWon」，计数=1，FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_AC8_Seal2to9DoubleTrigger,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.TC8_AC8_Seal2to9DoubleTrigger",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_AC8_Seal2to9DoubleTrigger::RunTest(const FString& Parameters)
{
    UWorld* World = BankruptcyTestHelpers::CreateGameWorld(TEXT("TC8_World"));
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    check(Sub);

    // P=2 局
    FGameSetupConfig Config = BankruptcyTestHelpers::MakeConfig(2);
    Sub->InitializeFromConfig(Config);

    const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
    check(States.Num() == 2);

    // 找 TurnOrderIndex=0 和 TurnOrderIndex=1 的玩家
    URentoPlayerState* StateA = BankruptcyTestHelpers::FindStateByTurnOrder(Sub, 0);
    URentoPlayerState* StateB = BankruptcyTestHelpers::FindStateByTurnOrder(Sub, 1);
    check(StateA && StateB);

    // GIVEN：直接置 A 破产（不经9 ResolveBankruptcy）
    StateA->bIsBankrupt = true;

    // spy 订阅 OnGameWon
    UOnGameWonSpy* Spy = NewObject<UOnGameWonSpy>(GetTransientPackage());
    Spy->AddToRoot();
    Sub->OnGameWon.AddDynamic(Spy, &UOnGameWonSpy::HandleGameWon);

    // ⚠ 不注入 BankruptcyResolver（此路径不调 HandlePlayerBankruptcy）

    // WHEN：B 的完整回合（StartTurn → ProcessRoll → AdvanceMove → AdvanceResolve → EndTurn）
    Sub->StartTurn(StateB->PlayerId);
    bool bDummySentToJail = false; Sub->ProcessRollResult(/*bIsDouble=*/false, bDummySentToJail);
    Sub->AdvanceFromMovePhase();
    Sub->AdvanceFromResolvePhase();
    Sub->EndTurn(/*bSentToJailThisTurn=*/false);

    // THEN：OnGameWon 广播计数==0（正常 TurnEnd 路径不因 APC==1 触发）
    TestEqual(TEXT("TC8: 正常 TurnEnd 路径 OnGameWon 计数应==0（AC-8 封堵2↔9双触发）"),
        Spy->BroadcastCount, 0);

    Sub->OnGameWon.RemoveDynamic(Spy, &UOnGameWonSpy::HandleGameWon);
    Spy->RemoveFromRoot();
    BankruptcyTestHelpers::DestroyGameWorld(World);
    return true;
}

// =============================================================================
// Edge Case A：cur∉A 且 |A|≥2（cur 已破产异常入参）仍正确寻下一 A 成员
//
// GIVEN：A={1,3}（TurnOrderIndex 0,2 已破产）, P=4
//   cur=0（0 不在 A，模拟破产玩家异常入参）→ next=1（0 的下一个 ∈ A）
//   cur=2（2 不在 A）→ next=3
//
// 验证防御性：cur 已破产不影响 F-1 找正确下一个 A 成员
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_Edge_CurNotInA_TwoActive,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.Edge_CurNotInA_TwoActive",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_Edge_CurNotInA_TwoActive::RunTest(const FString& Parameters)
{
    TSet<int32> A = {1, 3};  // TurnOrderIndex 1 和 3 活跃

    // cur=0（不在 A）→ 枚举 k=1 候选 (0+1)%4=1 ∈ A → next=1
    {
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(0, 4, A);
        TestEqual(TEXT("Edge_A: cur=0(∉A),A={1,3},P=4 → next=1"), Result, 1);
    }

    // cur=2（不在 A）→ 枚举 k=1 候选 (2+1)%4=3 ∈ A → next=3
    {
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(2, 4, A);
        TestEqual(TEXT("Edge_A: cur=2(∉A),A={1,3},P=4 → next=3"), Result, 3);
    }

    return true;
}

// =============================================================================
// Edge Case B：F-1 守卫 |A|=1 返回 INDEX_NONE，不依赖「唯一存活者==cur」
//
// 验证守卫判据纯看 |A|（AC-2 扩展验证）：
//   A={2}，P=4，cur=0（cur≠2）→ INDEX_NONE
//   A={2}，P=4，cur=2（cur==2）→ INDEX_NONE
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_Edge_F1_Guard_IndependentOfCurMembership,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.Edge_F1_Guard_IndependentOfCurMembership",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_Edge_F1_Guard_IndependentOfCurMembership::RunTest(const FString& Parameters)
{
    TSet<int32> A = {2};  // 唯一存活者 TurnOrderIndex=2

    // cur=0（≠2，不在A）→ 守卫先行，|A|=1 → INDEX_NONE
    {
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(0, 4, A);
        TestEqual(TEXT("Edge_B: |A|=1,cur=0(≠A成员)→INDEX_NONE"), Result, INDEX_NONE);
    }

    // cur=2（==A 唯一成员）→ 守卫先行，|A|=1 → INDEX_NONE
    {
        const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(2, 4, A);
        TestEqual(TEXT("Edge_B: |A|=1,cur=2(==A成员)→INDEX_NONE"), Result, INDEX_NONE);
    }

    return true;
}

// =============================================================================
// Edge Case C：APC=0 退化局（MVP 单线程不可达）只 assert，不触发任何 2→9 反向信号
//
// 验证 F-1 |A|=0 路径触发 ensureMsgf 并返回 INDEX_NONE（而非 crash）。
// AddExpectedError 吞 ensure 消息（ASCII 安全子串）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTest_Edge_APC0_DegenerateCase,
    "Rento.PlayerTurn.NextActivePlayerBankruptcyOnGameWon.Edge_APC0_DegenerateCase",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTest_Edge_APC0_DegenerateCase::RunTest(const FString& Parameters)
{
    // 吞 F-1 |A|=0 的 ensure 消息
    AddExpectedError(TEXT("ActiveIndices is empty"), EAutomationExpectedErrorFlags::Contains, 1);

    TSet<int32> EmptyA;  // |A|=0

    // F-1 应返回 INDEX_NONE（不 crash，不触发任何 2→9 反向信号）
    const int32 Result = UPlayerTurnSubsystem::NextActivePlayer(0, 4, EmptyA);
    TestEqual(TEXT("Edge_C: |A|=0 → INDEX_NONE（不 crash）"), Result, INDEX_NONE);

    return true;
}
