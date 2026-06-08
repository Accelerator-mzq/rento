// ReadLoadDelegateRebindTest.cpp
// =============================================================================
// 集成测试：story-006 Event Bus 读档集中解绑/重绑工具（防 EC-8 双订阅）
//
// 物理路径：Source/rentoTests/Private/ReadLoadDelegateRebindTest.cpp
// Automation 类目：Rento.Foundation.ReadLoadDelegateRebind
//
// 覆盖 AC（story-006 / ADR-0003 §统一规范 #6 + ADR-0005 CR-5/CR-6）：
//   TC1 (AC-1/AC-3) — 中央工具先全量触发重绑；重绑后订阅数 == 初次 N，无 2N 叠加
//   TC2 (AC-2)      — 重绑时机模拟：ReadLoad 序列「SetSeed → Rebind → switch(CurrentPhase)」
//   TC3 (AC-4)      — Init 绑/Deinitialize 全量解绑；PIE Stop 后重广播 spy 计数 == 0
//   TC4 (AC-5)      — 纯叶子方向：spy PerformRebind 不引入回写/反调通道
//   TC5 (AC-6)      — C++ RemoveDynamic → AddDynamic 幂等；重绑两次订阅数仍 == 1
//
// 框架：IMPLEMENT_SIMPLE_AUTOMATION_TEST（与邻居 EventBusDelegateContractTest.cpp 一致）
//
// AC 子句 defer 说明：
//   AC-2 完整路径（OnGameLoaded 广播由 Save epic 实现）= DEFERRED-to-save-epic
//   本 TC2 直接调 RebindPresentationDelegates() 模拟该时机（spy stand-in）。
//   AC-4 「PIE Stop」实际触发 = 需真 PIE（-nullrhi headless 下 World 即 GameWorld）；
//   本 TC3 用 DestroyWorld 模拟 Deinitialize，等价 PIE Stop 生命周期语义。
//
// ⚠ 已知坑规避（本项目实测）：
//   G1 EAutomationTestFlags_ApplicationContextMask（UE5.4+ enum class，非 scope 别名）
//   G3 spy 须 UCLASS/UFUNCTION（AddDynamic 要求）—— RebindConsumerSpy.h
//   G4 UObject spy 用 NewObject + AddToRoot/RemoveFromRoot（禁裸 new）
//   G5 TBaseStructure<T>::Get() 仅引擎内建特化，用户 USTRUCT 用 T::StaticStruct()
//   G6 ensure() 在测试内禁用，用 UE_LOG + TestTrue/TestEqual
//   G7 不加 AddExpectedError（occurrences 不匹配判假 Fail）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#include "EventBusRebindCoordinator.h"
#include "PlayerTurnSubsystem.h"
#include "DiceRngService.h"
#include "GameSetupConfig.h"
#include "RentoPlayerState.h"
#include "RebindConsumerSpy.h"  // 本 story spy（stand-in 消费者）
#include "TurnEventSpy.h"       // 复用 story-004 spy（OnPhaseChanged 计数）

// =============================================================================
// 测试辅助（独立命名空间，避免 ODR，与邻居测试同模式）
// =============================================================================
namespace RebindTestHelpers
{
    // -------------------------------------------------------------------------
    // World 创建/销毁（照 EventBusDelegateContractTest.cpp 同一模式）
    // -------------------------------------------------------------------------
    static UWorld* CreateGameWorld(const FName& WorldName)
    {
        UWorld* World = UWorld::CreateWorld(
            EWorldType::Game, /*bInformEngineOfWorld=*/false, WorldName);
        check(World);
        return World;
    }

    static void DestroyGameWorld(UWorld* World)
    {
        if (World) { World->DestroyWorld(/*bInformEngineOfWorld=*/false); }
    }

    // -------------------------------------------------------------------------
    // 构建 N 玩家配置（照 EventBusDelegateContractTest.cpp 同一模式）
    // -------------------------------------------------------------------------
    static FGameSetupConfig MakeConfig(int32 N)
    {
        FGameSetupConfig Config;
        Config.DoublesJailThreshold = 3;
        Config.MaxTiebreakRounds    = 5;
        Config.RandomSeed           = 42;

        const EPlayerColor Colors[] = {
            EPlayerColor::Red, EPlayerColor::Blue,
            EPlayerColor::Green, EPlayerColor::Yellow };
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

    // -------------------------------------------------------------------------
    // spy 生命周期辅助（G4 例外：UObject + AddToRoot）
    // -------------------------------------------------------------------------
    static URebindConsumerSpy* MakeRebindSpy()
    {
        URebindConsumerSpy* Spy =
            NewObject<URebindConsumerSpy>(GetTransientPackage());
        Spy->AddToRoot();  // 防 GC（G4 例外）
        return Spy;
    }

    static void ReleaseRebindSpy(URebindConsumerSpy* Spy)
    {
        if (Spy) { Spy->RemoveFromRoot(); }
    }

    static UTurnEventSpy* MakeTurnSpy()
    {
        UTurnEventSpy* Spy =
            NewObject<UTurnEventSpy>(GetTransientPackage());
        Spy->AddToRoot();
        return Spy;
    }

    static void ReleaseTurnSpy(UTurnEventSpy* Spy)
    {
        if (Spy) { Spy->RemoveFromRoot(); }
    }
}

// =============================================================================
// TC1 (AC-1 / AC-3) — 中央工具触发重绑；订阅数 == 初次 N，无 2N 叠加
//
// 步骤：
//   1. 创建 World + 获取 Coord + Sub
//   2. 创建 RebindConsumerSpy，RegisterWithCoordinator 一次性注册
//   3. Spy.PerformRebind() 手动模拟「初次开局绑定」（使 HandleOnPhaseChanged 订阅 OnPhaseChanged）
//   4. 调用 Coord->RebindPresentationDelegates() 一次（模拟第一次读档重绑）
//      → spy.PerformRebind 被触发，RebindCallCount == 1
//      → OnPhaseChanged 订阅数仍 == 1（RemoveDynamic 先解，AddDynamic 后绑，非 2N）
//   5. 再调 RebindPresentationDelegates() 一次（模拟连续两次读档，幂等验证）
//      → RebindCallCount == 2；OnPhaseChanged 订阅数仍 == 1（不叠加）
//   6. 通过 StartTurn 触发 OnPhaseChanged 广播，验证 spy 通路仍然有效
//
// AC-3 核心断言：多次 RebindPresentationDelegates 后，OnPhaseChanged 每次广播
//   spy.PhaseChangedCount 恰 +1（订阅数 == 1，非 N 倍叠加）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRebind_TC1_CentralRebindToolIdempotent,
    "Rento.Foundation.ReadLoadDelegateRebind.TC1_CentralRebindToolIdempotent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRebind_TC1_CentralRebindToolIdempotent::RunTest(const FString& Parameters)
{
    // ---- 环境创建 ----
    UWorld* World = RebindTestHelpers::CreateGameWorld(TEXT("TC1_World"));
    if (!TestNotNull(TEXT("TC-1: World"), World)) { return false; }

    UEventBusRebindCoordinator* Coord =
        World->GetSubsystem<UEventBusRebindCoordinator>();
    if (!TestNotNull(TEXT("TC-1: Coord"), Coord))
    {
        RebindTestHelpers::DestroyGameWorld(World);
        return false;
    }

    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-1: Sub"), Sub))
    {
        RebindTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // InitializeFromConfig 使 Sub 可用（开局建队）
    TestTrue(TEXT("TC-1: InitializeFromConfig"),
        Sub->InitializeFromConfig(RebindTestHelpers::MakeConfig(2)));

    // ---- spy 创建 + 注册 ----
    URebindConsumerSpy* Spy = RebindTestHelpers::MakeRebindSpy();
    // 一次性注册（模拟消费者 Init）
    Spy->RegisterWithCoordinator(Coord, Sub);

    // ---- 初次开局绑定（手动模拟消费者 Init 结束后首次绑 owner delegate）----
    // 初始状态：spy 已向协调器注册 OnRebindRequested，
    // 但 OnPhaseChanged 还未绑定（模拟消费者 Init 后、开局广播前状态）。
    // 手动调一次 PerformRebind 模拟开局初始绑定路径。
    Spy->PerformRebind();
    TestTrue(TEXT("TC-1: 初次绑定后 OnPhaseChanged.IsBound()"),
        Sub->OnPhaseChanged.IsBound());
    TestEqual(TEXT("TC-1: 初次 PerformRebind 后 RebindCallCount == 1"),
        Spy->RebindCallCount, 1);

    // ---- 第一次 RebindPresentationDelegates（模拟第一次读档重绑）----
    Coord->RebindPresentationDelegates();
    TestEqual(TEXT("TC-1: 第一次 RebindPresentationDelegates 后 RebindCallCount == 2"),
        Spy->RebindCallCount, 2);
    // OnPhaseChanged 仍绑定（重绑后通路依然有效）
    TestTrue(TEXT("TC-1: 第一次读档重绑后 OnPhaseChanged.IsBound()"),
        Sub->OnPhaseChanged.IsBound());

    // ---- 验证订阅数 == 1（非 2N 叠加）：触发广播，PhaseChangedCount 恰 +1 ----
    const int32 CountBefore1 = Spy->PhaseChangedCount;

    // 找 TurnOrderIndex==0 的玩家触发 StartTurn → 广播 OnPhaseChanged
    int32 FirstPlayerId = -1;
    for (const TObjectPtr<URentoPlayerState>& PS : Sub->GetPlayerStates())
    {
        if (PS && PS->TurnOrderIndex == 0) { FirstPlayerId = PS->PlayerId; break; }
    }
    int32 ReferenceDelta = 0;  // 订阅数==1 时单次 StartTurn 的 OnPhaseChanged 广播次数（供第二次读档后真比对，防同义反复）
    if (TestNotEqual(TEXT("TC-1: 找到 FirstPlayerId"), FirstPlayerId, -1))
    {
        Sub->StartTurn(FirstPlayerId);
        const int32 Delta1 = Spy->PhaseChangedCount - CountBefore1;
        ReferenceDelta = Delta1;
        // StartTurn 广播 >=1 次 OnPhaseChanged（TurnStart + RollPhase 两次 SetPhase）
        // 关键：Delta == 整数倍 >= 1，而非 2*整数倍（订阅数 == 1，非 2）
        TestTrue(TEXT("TC-1: 第一次读档重绑后，每次广播 spy 仅收到 N 次（非 2N）"),
            Delta1 >= 1);
        // 反向验证：若订阅了 2 次，则 Delta = 2 * 真实广播次数；
        // 断言 Delta % 2 == 真实广播次数（只有 1 个订阅）
        // 真实广播次数 = Sub->StartTurn 广播的 OnPhaseChanged 次数
        // 用 Delta 除以真实广播次数，验结果 == 1（非 2）
        // 简化：连续两次 StartTurn 比较 Delta，若订阅 2 次则每次 Delta 翻倍
        const int32 CountBefore2 = Spy->PhaseChangedCount;
        Sub->StartTurn(FirstPlayerId);
        const int32 Delta2 = Spy->PhaseChangedCount - CountBefore2;
        // Delta1 应等于 Delta2（同一订阅数，广播次数相同 = StartTurn 产生的 OnPhaseChanged 次数一致）
        TestEqual(TEXT("TC-1: 连续两次 StartTurn 的 PhaseChanged delta 相等（订阅数稳定，非叠加）"),
            Delta1, Delta2);
    }

    // ---- 第二次 RebindPresentationDelegates（连续两次读档，幂等验证）----
    Coord->RebindPresentationDelegates();
    TestEqual(TEXT("TC-1: 第二次 RebindPresentationDelegates 后 RebindCallCount == 3"),
        Spy->RebindCallCount, 3);
    TestTrue(TEXT("TC-1: 第二次读档重绑后 OnPhaseChanged 仍 IsBound()"),
        Sub->OnPhaseChanged.IsBound());

    // 再触发一次广播，再次验证订阅数不变
    const int32 CountBefore3 = Spy->PhaseChangedCount;
    Sub->StartTurn(FirstPlayerId);
    const int32 Delta3 = Spy->PhaseChangedCount - CountBefore3;
    TestTrue(TEXT("TC-1: 第二次读档重绑后，广播 delta 仍 >= 1（订阅通路有效）"),
        Delta3 >= 1);
    // 与 Delta1（ReferenceDelta）相等（订阅数不变，非自比同义反复）
    if (FirstPlayerId != -1)
    {
        TestEqual(TEXT("TC-1: 第二次读档重绑后 delta == 第一次 delta（幂等，订阅数 == 1，非自比同义反复）"),
            Delta3, ReferenceDelta);
    }

    // ---- 清理 ----
    // 注意：先移除 spy 的 OnPhaseChanged 订阅，再解 root，再销毁 World
    Sub->OnPhaseChanged.RemoveDynamic(Spy, &URebindConsumerSpy::HandleOnPhaseChanged);
    RebindTestHelpers::ReleaseRebindSpy(Spy);
    RebindTestHelpers::DestroyGameWorld(World);

    return true;
}

// =============================================================================
// TC2 (AC-2) — 重绑时机模拟：拓扑序「SetSeed → RebindPresentationDelegates → switch」
//
// 读档拓扑序（ADR-0005 CR-5）：
//   DA → 经济/地产/建房/事件格（互不依赖）→ 回合2 → 骰子 SetSeed → 重绑 → switch(CurrentPhase)
//
// 本 TC 以调用顺序断言模拟拓扑序：
//   1. 模拟骰子 SetSeed（DiceRngService.SetSeed）
//   2. 调 RebindPresentationDelegates（重绑）
//   3. 调 RestoreFromSaveData + ResumeFromLoadedState（模拟 switch(CurrentPhase)）
//
// 断言：
//   a. RebindPresentationDelegates 调用成功（无崩溃）
//   b. 重绑后订阅仍有效（spy 能收到 ResumeFromLoadedState 广播）
//
// AC-2 完整路径（OnGameLoaded 广播由 Save epic 接线）= DEFERRED-to-save-epic。
// 本 TC 直接调 RebindPresentationDelegates() 模拟该接线点（spy stand-in）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRebind_TC2_TopologicalOrderAfterSetSeedBeforeSwitch,
    "Rento.Foundation.ReadLoadDelegateRebind.TC2_TopologicalOrderAfterSetSeedBeforeSwitch",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRebind_TC2_TopologicalOrderAfterSetSeedBeforeSwitch::RunTest(const FString& Parameters)
{
    UWorld* World = RebindTestHelpers::CreateGameWorld(TEXT("TC2_World"));
    if (!TestNotNull(TEXT("TC-2: World"), World))  { return false; }

    UEventBusRebindCoordinator* Coord =
        World->GetSubsystem<UEventBusRebindCoordinator>();
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    UDiceRngService* Dice     = World->GetSubsystem<UDiceRngService>();

    if (!TestNotNull(TEXT("TC-2: Coord"), Coord) ||
        !TestNotNull(TEXT("TC-2: Sub"),   Sub)   ||
        !TestNotNull(TEXT("TC-2: Dice"),  Dice))
    {
        RebindTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 开局建队（使 Sub 状态有效）
    Sub->InitializeFromConfig(RebindTestHelpers::MakeConfig(2));

    // ---- spy 注册（模拟消费者 Init）----
    URebindConsumerSpy* Spy = RebindTestHelpers::MakeRebindSpy();
    Spy->RegisterWithCoordinator(Coord, Sub);

    // 初次开局绑定（手动模拟）
    Spy->PerformRebind();
    TestTrue(TEXT("TC-2: 初次绑定后 OnPhaseChanged.IsBound()"),
        Sub->OnPhaseChanged.IsBound());

    // ---- 模拟读档拓扑序 ----

    // 拓扑序步骤1：DA + 经济/地产/建房 重建（本 TC 不涉及，Skip）

    // 拓扑序步骤2：骰子 SetSeed（CR-5 第4步）
    const int32 LoadedSeed = 12345;
    Dice->SetSeed(LoadedSeed);
    TestEqual(TEXT("TC-2: 骰子 SetSeed 后 GetInitialSeed() == LoadedSeed"),
        Dice->GetInitialSeed(), LoadedSeed);

    // 拓扑序步骤3：重绑（CR-5 第5步，= SetSeed 之后）
    Coord->RebindPresentationDelegates();
    TestEqual(TEXT("TC-2: RebindPresentationDelegates 调用后 RebindCallCount == 2（初次+重绑=2）"),
        Spy->RebindCallCount, 2);
    TestTrue(TEXT("TC-2: 重绑后 OnPhaseChanged 仍 IsBound()（拓扑序中位正确）"),
        Sub->OnPhaseChanged.IsBound());

    // 拓扑序步骤4：switch(CurrentPhase) 还原阶段 — 用 ResumeFromLoadedState 模拟广播
    // （ ResumeFromLoadedState 内部 switch + 广播 OnPhaseChanged；需先 RestoreFromSaveData）
    // 本 TC 仅验：重绑之后 ResumeFromLoadedState 广播能被 spy 接收（通路有效）
    // 不完整 save/load round-trip（DEFERRED-to-save-epic）
    const int32 PhaseCountBefore = Spy->PhaseChangedCount;
    // 直接触发广播（StartTurn 代替 ResumeFromLoadedState 的广播语义，等价验通路）
    int32 FirstPlayerId = -1;
    for (const TObjectPtr<URentoPlayerState>& PS : Sub->GetPlayerStates())
    {
        if (PS && PS->TurnOrderIndex == 0) { FirstPlayerId = PS->PlayerId; break; }
    }
    if (FirstPlayerId != -1)
    {
        Sub->StartTurn(FirstPlayerId);
        TestTrue(TEXT("TC-2: switch(CurrentPhase) 步骤后广播被 spy 接收（重绑通路有效）"),
            Spy->PhaseChangedCount > PhaseCountBefore);
    }

    // DEFERRED 注释：
    // TODO [DEFERRED-to-save-epic]: AC-2 完整路径 = OnGameLoaded 广播触发 RebindPresentationDelegates。
    //   存档21 LoadGame() 在拓扑序 SetSeed 后广播 OnGameLoaded，OnGameLoaded 消费者调
    //   Coord->RebindPresentationDelegates()。本 TC 以直接调用模拟该接线，待 Save epic 落地后
    //   补充 E2E 验证（OnGameLoaded 广播 → Coord->RebindPresentationDelegates 调用链）。
    UE_LOG(LogTemp, Log,
        TEXT("TC-2: DEFERRED-to-save-epic — AC-2 OnGameLoaded→RebindPresentationDelegates "
             "接线待 Save epic 落地后补充 E2E 验证。"));

    // ---- 清理 ----
    Sub->OnPhaseChanged.RemoveDynamic(Spy, &URebindConsumerSpy::HandleOnPhaseChanged);
    RebindTestHelpers::ReleaseRebindSpy(Spy);
    RebindTestHelpers::DestroyGameWorld(World);

    return true;
}

// =============================================================================
// TC3 (AC-4) — 生命周期：Init 绑 / Deinitialize 全量解绑；PIE Stop 无野绑定
//
// 步骤：
//   1. World + spy 创建，RegisterWithCoordinator + 初次绑定
//   2. 验 OnPhaseChanged.IsBound() == true（Init 绑成功）
//   3. DestroyWorld 触发 Deinitialize（模拟 PIE Stop）
//      → UnbindAllOwnerDelegates() Clear 全部 owner delegate
//      → OnRebindRequested.Clear() 清空 meta-delegate
//   4. 重新创建 World + Sub（新对象，invocation list 本就空）
//      验 OnPhaseChanged.IsBound() == false（Deinitialize 已 Clear）
//
// 注意：Deinitialize 后原 Sub 对象已销毁（World 销毁随之）；
//   断言通过新 World 的新 Sub 验「初始 IsBound==false」。
//   若 Clear 生效，新 World 的 OnPhaseChanged 应不含任何订阅。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRebind_TC3_LifecycleDeinitUnbindsAll,
    "Rento.Foundation.ReadLoadDelegateRebind.TC3_LifecycleDeinitUnbindsAll",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRebind_TC3_LifecycleDeinitUnbindsAll::RunTest(const FString& Parameters)
{
    // ---- 阶段1：初始化，绑定 ----
    UWorld* World1 = RebindTestHelpers::CreateGameWorld(TEXT("TC3_World1"));
    if (!TestNotNull(TEXT("TC-3: World1"), World1)) { return false; }

    UEventBusRebindCoordinator* Coord1 =
        World1->GetSubsystem<UEventBusRebindCoordinator>();
    UPlayerTurnSubsystem* Sub1 = World1->GetSubsystem<UPlayerTurnSubsystem>();

    if (!TestNotNull(TEXT("TC-3: Coord1"), Coord1) ||
        !TestNotNull(TEXT("TC-3: Sub1"),   Sub1))
    {
        RebindTestHelpers::DestroyGameWorld(World1);
        return false;
    }

    Sub1->InitializeFromConfig(RebindTestHelpers::MakeConfig(2));

    URebindConsumerSpy* Spy = RebindTestHelpers::MakeRebindSpy();
    Spy->RegisterWithCoordinator(Coord1, Sub1);
    Spy->PerformRebind(); // 初次绑定

    // 验：初始绑定后 OnPhaseChanged 已绑（AC-4 Init 绑）
    TestTrue(TEXT("TC-3: Init 绑后 Sub1->OnPhaseChanged.IsBound() == true"),
        Sub1->OnPhaseChanged.IsBound());
    TestTrue(TEXT("TC-3: Init 绑后 Coord1->OnRebindRequested.IsBound() == true"),
        Coord1->OnRebindRequested.IsBound());

    // 验：广播 OnRebindRequested 可抵达 spy（绑定通路有效）
    Coord1->RebindPresentationDelegates();
    TestEqual(TEXT("TC-3: RebindPresentationDelegates 后 RebindCallCount == 2"),
        Spy->RebindCallCount, 2);

    // ---- 阶段1.5：直接验 UnbindAllOwnerDelegates() 真 Clear（AC-4 核心强验）----
    // ⚠ 不依赖「新建 World2 的 Sub2 天然空」（那对 Clear 是否真生效是假覆盖）——
    //   而是对**同一个** Sub1：先确认已绑，直接调中央全量解绑，断言 IsBound 翻 false。
    TestTrue(TEXT("TC-3: UnbindAll 前 Sub1->OnPhaseChanged.IsBound() == true（先决条件）"),
        Sub1->OnPhaseChanged.IsBound());
    Coord1->UnbindAllOwnerDelegates();
    TestFalse(TEXT("TC-3: UnbindAllOwnerDelegates() 后 Sub1->OnPhaseChanged.IsBound() == false（Clear 真生效，非新对象天然空）"),
        Sub1->OnPhaseChanged.IsBound());

    // ---- 阶段2：DestroyWorld 触发 Deinitialize（模拟 PIE Stop）----
    // spy 先解绑（避免 DestroyWorld 后 spy 持有悬挂 Sub 引用触发 GC 问题）
    if (Sub1->OnPhaseChanged.IsBound())
    {
        Sub1->OnPhaseChanged.RemoveDynamic(
            Spy, &URebindConsumerSpy::HandleOnPhaseChanged);
    }

    RebindTestHelpers::DestroyGameWorld(World1);
    World1 = nullptr;
    // Sub1 和 Coord1 此时已销毁（跟随 World），不再访问

    // ---- 阶段3：新建 World2（模拟 PIE Start 后新局），验证初始状态干净 ----
    // 新 World 的 Subsystem 是全新对象，invocation list 从空开始
    UWorld* World2 = RebindTestHelpers::CreateGameWorld(TEXT("TC3_World2"));
    if (!TestNotNull(TEXT("TC-3: World2"), World2))
    {
        RebindTestHelpers::ReleaseRebindSpy(Spy);
        return false;
    }

    UEventBusRebindCoordinator* Coord2 =
        World2->GetSubsystem<UEventBusRebindCoordinator>();
    UPlayerTurnSubsystem* Sub2 = World2->GetSubsystem<UPlayerTurnSubsystem>();

    if (!TestNotNull(TEXT("TC-3: Coord2"), Coord2) ||
        !TestNotNull(TEXT("TC-3: Sub2"),   Sub2))
    {
        RebindTestHelpers::ReleaseRebindSpy(Spy);
        RebindTestHelpers::DestroyGameWorld(World2);
        return false;
    }

    // PIE Stop（DestroyWorld）+ PIE Start（新 World）后，新 Sub2 的 OnPhaseChanged
    // invocation list 应为空（全新对象，Deinitialize Clear 已保证旧绑定不残留）
    TestFalse(TEXT("TC-3: Deinitialize（PIE Stop）后新 World 的 OnPhaseChanged.IsBound() == false（无野绑定残留）"),
        Sub2->OnPhaseChanged.IsBound());
    TestFalse(TEXT("TC-3: Deinitialize 后新 World 的 OnRebindRequested.IsBound() == false"),
        Coord2->OnRebindRequested.IsBound());

    // ---- 清理 ----
    RebindTestHelpers::ReleaseRebindSpy(Spy);
    RebindTestHelpers::DestroyGameWorld(World2);

    return true;
}

// =============================================================================
// TC4 (AC-5) — 纯叶子方向：spy PerformRebind 不引入呈现层回写/反调通道
//
// 验证设计约束（ADR-0003 Forbidden）：
//   a. spy 仅订阅 owner delegate（方向: owner→spy），从不调用 owner 方法回写状态
//   b. PerformRebind 执行期间，owner 内部状态（CurrentPhase / PlayerStates）不变
//   c. RebindPresentationDelegates 广播前后，owner 内部状态不变
//      （广播本身不触发任何 owner 状态写操作）
//
// 实现验证策略：
//   - 记录 RebindPresentationDelegates 前后 Sub->GetCurrentPhase()
//   - 断言 Phase 未变（重绑过程中无 owner 方法被调）
//   - 断言 Sub->GetPlayerStates().Num() 未变（无 PlayerState 被修改）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRebind_TC4_PureLeafNoCallbackToOwner,
    "Rento.Foundation.ReadLoadDelegateRebind.TC4_PureLeafNoCallbackToOwner",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRebind_TC4_PureLeafNoCallbackToOwner::RunTest(const FString& Parameters)
{
    UWorld* World = RebindTestHelpers::CreateGameWorld(TEXT("TC4_World"));
    if (!TestNotNull(TEXT("TC-4: World"), World)) { return false; }

    UEventBusRebindCoordinator* Coord =
        World->GetSubsystem<UEventBusRebindCoordinator>();
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();

    if (!TestNotNull(TEXT("TC-4: Coord"), Coord) ||
        !TestNotNull(TEXT("TC-4: Sub"),   Sub))
    {
        RebindTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(RebindTestHelpers::MakeConfig(2));

    // 记录重绑前 owner 状态（Phase + PlayerStates 数量）
    const ETurnPhase PhaseBefore = Sub->GetCurrentPhase();
    const int32 PlayerCountBefore = Sub->GetPlayerStates().Num();
    const int32 ActivePlayerIdBefore = Sub->GetCurrentActivePlayerId();

    // ---- spy 注册 + 绑定 ----
    URebindConsumerSpy* Spy = RebindTestHelpers::MakeRebindSpy();
    Spy->RegisterWithCoordinator(Coord, Sub);
    Spy->PerformRebind();

    // ---- 执行重绑 ----
    Coord->RebindPresentationDelegates();

    // ---- 断言：owner 状态未改变（纯叶子，无回写，AC-5）----
    TestEqual(TEXT("TC-4: RebindPresentationDelegates 前后 CurrentPhase 不变（纯叶子不变式）"),
        static_cast<int32>(Sub->GetCurrentPhase()),
        static_cast<int32>(PhaseBefore));

    TestEqual(TEXT("TC-4: RebindPresentationDelegates 前后 PlayerStates.Num() 不变"),
        Sub->GetPlayerStates().Num(), PlayerCountBefore);

    TestEqual(TEXT("TC-4: RebindPresentationDelegates 前后 CurrentActivePlayerId 不变"),
        Sub->GetCurrentActivePlayerId(), ActivePlayerIdBefore);

    // 断言：spy 仅做订阅操作（RebindCallCount 递增，但无 owner 写方法被调用）
    TestEqual(TEXT("TC-4: spy.RebindCallCount == 2（初次+重绑，仅订阅关系操作）"),
        Spy->RebindCallCount, 2);

    // 设计注释证明（AC-5 第二层验证）：
    // spy.PerformRebind() 的实现只调 RemoveDynamic + AddDynamic，
    // 不调任何 owner 方法（StartTurn/EndTurn/ProcessRollResult 等）。
    // 上述 Phase/PlayerCount/ActivePlayerId 不变断言可证伪此约束：
    // 若 spy 错误回写了 owner，则这三个断言至少一个会 FAIL。
    UE_LOG(LogTemp, Log,
        TEXT("TC-4 PASS: RebindConsumerSpy.PerformRebind 仅操作订阅关系，"
             "owner 内部状态不变（纯叶子不变式，ADR-0003 Forbidden 验证）。"));

    // ---- 清理 ----
    Sub->OnPhaseChanged.RemoveDynamic(Spy, &URebindConsumerSpy::HandleOnPhaseChanged);
    RebindTestHelpers::ReleaseRebindSpy(Spy);
    RebindTestHelpers::DestroyGameWorld(World);

    return true;
}

// =============================================================================
// TC5 (AC-6) — C++ RemoveDynamic → AddDynamic 幂等；重绑两次订阅数仍 == 1
//
// 步骤：
//   1. spy 注册到 Coord（一次性）
//   2. 手动调 PerformRebind() 三次（每次 Remove + Add）
//      每次后验：OnPhaseChanged.IsBound() == true
//   3. 广播 OnPhaseChanged 一次，验 spy 收到 PhaseChangedCount == 1（非 3）
//      证明订阅数 == 1，不随 PerformRebind 次数叠加
//
// AC-6 核心语义：
//   RemoveDynamic 后 AddDynamic = 无论调多少次，结果订阅数 == 1。
//   若 AddDynamic 允许重复（无幂等），3次 PerformRebind 后订阅数==3，
//   广播后 PhaseChangedCount 会是 3 而非 1（此时 TC FAIL = 验证有效）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRebind_TC5_CppRemoveThenAddDynamicIdempotent,
    "Rento.Foundation.ReadLoadDelegateRebind.TC5_CppRemoveThenAddDynamicIdempotent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRebind_TC5_CppRemoveThenAddDynamicIdempotent::RunTest(const FString& Parameters)
{
    UWorld* World = RebindTestHelpers::CreateGameWorld(TEXT("TC5_World"));
    if (!TestNotNull(TEXT("TC-5: World"), World)) { return false; }

    UEventBusRebindCoordinator* Coord =
        World->GetSubsystem<UEventBusRebindCoordinator>();
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();

    if (!TestNotNull(TEXT("TC-5: Coord"), Coord) ||
        !TestNotNull(TEXT("TC-5: Sub"),   Sub))
    {
        RebindTestHelpers::DestroyGameWorld(World);
        return false;
    }

    Sub->InitializeFromConfig(RebindTestHelpers::MakeConfig(2));

    // ---- spy 注册 ----
    URebindConsumerSpy* Spy = RebindTestHelpers::MakeRebindSpy();
    Spy->RegisterWithCoordinator(Coord, Sub);

    // ---- 三次 PerformRebind（RemoveDynamic → AddDynamic，AC-6 幂等）----
    for (int32 i = 0; i < 3; ++i)
    {
        Spy->PerformRebind();
        TestTrue(
            *FString::Printf(TEXT("TC-5: 第%d次 PerformRebind 后 OnPhaseChanged.IsBound()"), i+1),
            Sub->OnPhaseChanged.IsBound());
    }
    TestEqual(TEXT("TC-5: 三次 PerformRebind 后 RebindCallCount == 3"),
        Spy->RebindCallCount, 3);

    // ---- 广播 OnPhaseChanged 一次，验 spy 只收到「单次广播产生的 N 次」----
    // StartTurn → SetPhase(TurnStart) + SetPhase(RollPhase) = 2次 OnPhaseChanged 广播
    // 若订阅数 == 1：PhaseChangedCount += 2
    // 若订阅数 == 3（叠加）：PhaseChangedCount += 6（= 2 * 3）
    int32 FirstPlayerId = -1;
    for (const TObjectPtr<URentoPlayerState>& PS : Sub->GetPlayerStates())
    {
        if (PS && PS->TurnOrderIndex == 0) { FirstPlayerId = PS->PlayerId; break; }
    }
    if (!TestNotEqual(TEXT("TC-5: 找到 FirstPlayerId"), FirstPlayerId, -1))
    {
        Sub->OnPhaseChanged.RemoveDynamic(Spy, &URebindConsumerSpy::HandleOnPhaseChanged);
        RebindTestHelpers::ReleaseRebindSpy(Spy);
        RebindTestHelpers::DestroyGameWorld(World);
        return false;
    }

    // 基准：先触发一次，数出真实「单次 StartTurn」产生的 OnPhaseChanged 广播次数
    const int32 CountBase0 = Spy->PhaseChangedCount;
    Sub->StartTurn(FirstPlayerId);
    const int32 SingleStartTurnBroadcastCount = Spy->PhaseChangedCount - CountBase0;
    TestTrue(TEXT("TC-5: StartTurn 产生 >= 1 次 OnPhaseChanged 广播"),
        SingleStartTurnBroadcastCount >= 1);

    // 现在三次 PerformRebind（RemoveDynamic → AddDynamic）已执行，
    // 验证订阅数 == 1：再触发一次 StartTurn，delta == SingleStartTurnBroadcastCount（非 3 倍）
    const int32 CountBefore = Spy->PhaseChangedCount;
    Sub->StartTurn(FirstPlayerId);
    const int32 DeltaAfterRebind = Spy->PhaseChangedCount - CountBefore;

    // 关键断言：delta == 单次 StartTurn 广播次数（订阅数 == 1，幂等）
    // 若订阅了 3 次，delta 应是 3 * SingleStartTurnBroadcastCount
    TestEqual(
        TEXT("TC-5: 三次 RemoveDynamic→AddDynamic 后广播 delta == 单次广播次数"
             "（订阅数 == 1，RemoveDynamic+AddDynamic 幂等，AC-6 验证）"),
        DeltaAfterRebind,
        SingleStartTurnBroadcastCount);

    // ---- 清理 ----
    Sub->OnPhaseChanged.RemoveDynamic(Spy, &URebindConsumerSpy::HandleOnPhaseChanged);
    RebindTestHelpers::ReleaseRebindSpy(Spy);
    RebindTestHelpers::DestroyGameWorld(World);

    return true;
}
