// SaveSerializationDelegateRebindTest.cpp
// =============================================================================
// 集成测试：story-008 读档序列化 round-trip + delegate 重绑拓扑序 + 枚举 append-only + 可存档点
//
// 物理路径：Source/rentoTests/Private/SaveSerializationDelegateRebindTest.cpp
// Automation 类目：Rento.PlayerTurn.SaveSerialization
//
// 覆盖 AC（story-008）：
//   TC1 (AC-1) — round-trip 精确阶段/玩家/CD + 读档后 EndTurn 被接受（FSM 真在 PostRollAction）
//   TC2 (AC-2) — 11 字段 + ConsecutiveDoubles + 阶段 + FDiceRollResult(Die1/Die2) + 锁定阈值 round-trip
//   TC3 (AC-3) — RestoreFromSaveData 静默（restore 期不广播）+ 重绑后 Resume 广播未丢失
//   TC4 (AC-4) — switch(CurrentPhase) 读已还原回合2 字段（拓扑序：restore 先于 resume）
//   TC5 (AC-5) — 枚举整数索引 append-only（运行期见证 static_assert，ETurnPhase 合法值=7）
//   TC6 (AC-6) — CanSaveNow：定序未启动拒存 / 首个 TurnStart 后可存
//
// 机制（headless -nullrhi，G7）：
//   - round-trip 走 UGameplayStatics::SaveGameToMemory/LoadGameFromMemory（in-memory，无磁盘）
//   - UTurnEventSpy（UCLASS，复用 story-004）捕获 OnPhaseChanged 广播序列
//   - 不写"未标 SaveGame 字段被过滤"断言（内置 save 不过滤，G7）
//
// ⚠ 已知坑：
//   G1 EAutomationTestFlags_ApplicationContextMask | ProductFilter
//   G3 DYNAMIC delegate spy 用 UCLASS（UTurnEventSpy 复用）
//   G4 例外：UObject（spy/save 对象）用 AddToRoot/RemoveFromRoot 防 GC
//   G7 SaveGameToMemory 不过滤 SaveGame → 只断言 round-trip 恒等
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Kismet/GameplayStatics.h"

#include "PlayerTurnSubsystem.h"
#include "PlayerTurnSaveData.h"
#include "RentoPlayerState.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"
#include "DiceRngService.h"
#include "TurnEventSpy.h"

namespace SaveSerTestHelpers
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

    /** 把 ActiveId 玩家的 FSM 推进到 PostRollAction，并直构造 fixture（CD + roll context）。 */
    static bool BuildPostRollFixture(UPlayerTurnSubsystem* Sub, int32 ActiveId,
                                     int32 CD, int32 Die1, int32 Die2)
    {
        URentoPlayerState* PS = Sub->FindPlayerById(ActiveId);
        if (!PS) { return false; }
        PS->bIsInJail = false;                          // 确保 StartTurn 路由 RollPhase（非 JailTurn）
        Sub->StartTurn(ActiveId);
        if (Sub->GetCurrentPhase() != ETurnPhase::RollPhase) { return false; }
        bool bSent = false;
        Sub->ProcessRollResult(/*bIsDouble=*/false, bSent);   // → MovePhase（CD 被归零）
        if (Sub->GetCurrentPhase() != ETurnPhase::MovePhase) { return false; }
        Sub->AdvanceFromMovePhase();                          // → ResolvePhase
        Sub->AdvanceFromResolvePhase();                       // → PostRollAction
        if (Sub->GetCurrentPhase() != ETurnPhase::PostRollAction) { return false; }
        // 直构造 fixture 状态（测试直写 public 字段；CD/roll 在到位后写，避免 ProcessRollResult 归零）
        PS->ConsecutiveDoubles = CD;
        FDiceRollResult Roll;
        Roll.Die1 = Die1; Roll.Die2 = Die2; Roll.Total = Die1 + Die2; Roll.bIsDouble = (Die1 == Die2);
        PS->CurrentRollContext = Roll;
        return true;
    }

    static UTurnEventSpy* MakeSpy()
    {
        UTurnEventSpy* Spy = NewObject<UTurnEventSpy>(GetTransientPackage());
        Spy->AddToRoot();   // G4 例外
        return Spy;
    }
    static void ReleaseSpy(UTurnEventSpy* Spy) { if (Spy) { Spy->RemoveFromRoot(); } }

    /** round-trip 一个存档对象：Capture→SaveGameToMemory→LoadGameFromMemory→Cast。 */
    static UPlayerTurnSaveData* RoundTrip(UPlayerTurnSubsystem* SrcSub)
    {
        UPlayerTurnSaveData* Save = SrcSub->CaptureSaveData();
        if (!Save) { return nullptr; }
        Save->AddToRoot();   // G4 例外：跨 SaveGameToMemory 防 GC
        TArray<uint8> Bytes;
        const bool bSaved = UGameplayStatics::SaveGameToMemory(Save, Bytes);
        Save->RemoveFromRoot();
        if (!bSaved || Bytes.Num() == 0) { return nullptr; }
        return Cast<UPlayerTurnSaveData>(UGameplayStatics::LoadGameFromMemory(Bytes));
    }
}

// =============================================================================
// TC1（AC-1）— round-trip 精确阶段/玩家/CD + 读档后 EndTurn 被接受
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveSer_TC1_RoundTripResumeEndTurn,
    "Rento.PlayerTurn.SaveSerialization.TC1_RoundTripResumeEndTurn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSaveSer_TC1_RoundTripResumeEndTurn::RunTest(const FString& Parameters)
{
    using namespace SaveSerTestHelpers;
    // ── 源局：player1 在 PostRollAction，CD=2，roll={1,3} ──
    UWorld* W1 = CreateGameWorld(TEXT("TC1_Src"));
    if (!TestNotNull(TEXT("TC-1: W1"), W1)) return false;
    UPlayerTurnSubsystem* Sub1 = W1->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-1: Sub1"), Sub1)) { DestroyGameWorld(W1); return false; }
    Sub1->InitializeFromConfig(MakeConfig(2));
    if (!TestTrue(TEXT("TC-1: fixture"), BuildPostRollFixture(Sub1, /*ActiveId=*/1, /*CD=*/2, 1, 3)))
    { DestroyGameWorld(W1); return false; }

    UPlayerTurnSaveData* Loaded = RoundTrip(Sub1);
    DestroyGameWorld(W1);
    if (!TestNotNull(TEXT("TC-1: Loaded round-trip"), Loaded)) return false;

    // ── 目标局：新 subsystem 还原 ──
    UWorld* W2 = CreateGameWorld(TEXT("TC1_Dst"));
    UPlayerTurnSubsystem* Sub2 = W2->GetSubsystem<UPlayerTurnSubsystem>();
    Sub2->RestoreFromSaveData(Loaded);

    TestEqual(TEXT("TC-1: 精确阶段 PostRollAction"),
        static_cast<int32>(Sub2->GetCurrentPhase()), static_cast<int32>(ETurnPhase::PostRollAction));
    TestEqual(TEXT("TC-1: 活跃玩家 ==1"), Sub2->GetCurrentActivePlayerId(), 1);
    URentoPlayerState* P1 = Sub2->FindPlayerById(1);
    if (TestNotNull(TEXT("TC-1: 还原玩家1"), P1))
    {
        TestEqual(TEXT("TC-1: ConsecutiveDoubles==2"), P1->ConsecutiveDoubles, 2);
    }

    // ── 读档后 EndTurn 被接受（FSM 真在 PostRollAction，否则非法转移被拒）──
    UTurnEventSpy* Spy = MakeSpy();
    Sub2->OnPhaseChanged.AddDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);
    Sub2->EndTurn(false);                       // PostRollAction → TurnEnd（再移交下一玩家）
    Sub2->OnPhaseChanged.RemoveDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);

    bool bSawTurnEnd = false;
    for (const FPhaseChangedInfo& I : Spy->PhaseChanges)
    { if (I.NewPhase == ETurnPhase::TurnEnd) { bSawTurnEnd = true; break; } }
    TestTrue(TEXT("TC-1: EndTurn 被接受 → 广播 TurnEnd（FSM 真在 PostRollAction）"), bSawTurnEnd);

    ReleaseSpy(Spy);
    DestroyGameWorld(W2);
    return true;
}

// =============================================================================
// TC2（AC-2）— 11 字段 + CD + 阶段 + FDiceRollResult + 锁定阈值 round-trip
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveSer_TC2_FullFieldRoundTrip,
    "Rento.PlayerTurn.SaveSerialization.TC2_FullFieldRoundTrip",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSaveSer_TC2_FullFieldRoundTrip::RunTest(const FString& Parameters)
{
    using namespace SaveSerTestHelpers;
    UWorld* W1 = CreateGameWorld(TEXT("TC2_Src"));
    if (!TestNotNull(TEXT("TC-2: W1"), W1)) return false;
    UPlayerTurnSubsystem* Sub1 = W1->GetSubsystem<UPlayerTurnSubsystem>();
    // 非默认阈值 2（fresh Sub2 默认 3）→ 证 DoublesJailThreshold round-trip 非 vacuous
    Sub1->InitializeFromConfig(MakeConfig(2, /*DoublesThreshold=*/2));
    if (!TestTrue(TEXT("TC-2: fixture"), BuildPostRollFixture(Sub1, /*ActiveId=*/1, /*CD=*/2, 1, 3)))
    { DestroyGameWorld(W1); return false; }

    // 在玩家1 上设 11 字段的可辨别值（FSM 已到位后写，bIsInJail 不影响路由）
    URentoPlayerState* Src1 = Sub1->FindPlayerById(1);
    if (!TestNotNull(TEXT("TC-2: Src1"), Src1)) { DestroyGameWorld(W1); return false; }
    Src1->Cash             = 1234;
    Src1->CurrentTileIndex = 17;
    Src1->bIsInJail        = true;
    Src1->JailTurnsServed  = 2;
    // TokenColor=Blue / AIDifficulty=None / TurnOrderIndex=定序值 / DisplayName="P1" / bIsBankrupt=false 由 Init 设
    const int32       ExpTurnOrder  = Src1->TurnOrderIndex;
    const EPlayerColor ExpColor      = Src1->TokenColor;
    const FString     ExpName        = Src1->DisplayName.ToString();

    UPlayerTurnSaveData* Loaded = RoundTrip(Sub1);
    DestroyGameWorld(W1);
    if (!TestNotNull(TEXT("TC-2: Loaded"), Loaded)) return false;

    UWorld* W2 = CreateGameWorld(TEXT("TC2_Dst"));
    UPlayerTurnSubsystem* Sub2 = W2->GetSubsystem<UPlayerTurnSubsystem>();
    Sub2->RestoreFromSaveData(Loaded);

    // 阶段不回退 TurnStart（AC-2）
    TestEqual(TEXT("TC-2: 阶段 PostRollAction（不回退 TurnStart）"),
        static_cast<int32>(Sub2->GetCurrentPhase()), static_cast<int32>(ETurnPhase::PostRollAction));
    // 锁定阈值 round-trip（非 vacuous：fresh 默认 3，存档 2）
    TestEqual(TEXT("TC-2: DoublesJailThreshold==2"), Sub2->DoublesJailThreshold, 2);

    URentoPlayerState* Dst1 = Sub2->FindPlayerById(1);
    if (TestNotNull(TEXT("TC-2: 还原玩家1"), Dst1))
    {
        TestEqual(TEXT("TC-2: PlayerId==1"),         Dst1->PlayerId, 1);
        TestEqual(TEXT("TC-2: DisplayName"),         Dst1->DisplayName.ToString(), ExpName);
        TestEqual(TEXT("TC-2: TokenColor"),          static_cast<int32>(Dst1->TokenColor), static_cast<int32>(ExpColor));
        TestFalse(TEXT("TC-2: bIsAI==false"),        Dst1->bIsAI);
        TestEqual(TEXT("TC-2: AIDifficulty==None"),  static_cast<int32>(Dst1->AIDifficulty), static_cast<int32>(EAIDifficulty::None));
        TestEqual(TEXT("TC-2: CurrentTileIndex==17"),Dst1->CurrentTileIndex, 17);
        TestEqual(TEXT("TC-2: Cash==1234"),          Dst1->Cash, 1234);
        TestTrue (TEXT("TC-2: bIsInJail==true"),     Dst1->bIsInJail);
        TestEqual(TEXT("TC-2: JailTurnsServed==2"),  Dst1->JailTurnsServed, 2);
        TestFalse(TEXT("TC-2: bIsBankrupt==false"),  Dst1->bIsBankrupt);
        TestEqual(TEXT("TC-2: TurnOrderIndex"),      Dst1->TurnOrderIndex, ExpTurnOrder);
        TestEqual(TEXT("TC-2: ConsecutiveDoubles==2"), Dst1->ConsecutiveDoubles, 2);
        // FDiceRollResult Die1/Die2 区分 1+3 vs 2+2（仅存 Total=4 不可判）
        const FDiceRollResult& Roll = Dst1->CurrentRollContext;
        TestEqual(TEXT("TC-2: Die1==1"),  Roll.Die1, 1);
        TestEqual(TEXT("TC-2: Die2==3"),  Roll.Die2, 3);
        TestEqual(TEXT("TC-2: Total==4"), Roll.Total, 4);
        TestFalse(TEXT("TC-2: bIsDouble==false"), Roll.bIsDouble);
    }
    DestroyGameWorld(W2);
    return true;
}

// =============================================================================
// TC3（AC-3）— RestoreFromSaveData 静默 + 重绑后 Resume 广播未丢失
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveSer_TC3_RestoreSilentResumeBroadcast,
    "Rento.PlayerTurn.SaveSerialization.TC3_RestoreSilentResumeBroadcast",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSaveSer_TC3_RestoreSilentResumeBroadcast::RunTest(const FString& Parameters)
{
    using namespace SaveSerTestHelpers;
    UWorld* W1 = CreateGameWorld(TEXT("TC3_Src"));
    if (!TestNotNull(TEXT("TC-3: W1"), W1)) return false;
    UPlayerTurnSubsystem* Sub1 = W1->GetSubsystem<UPlayerTurnSubsystem>();
    Sub1->InitializeFromConfig(MakeConfig(2));
    if (!TestTrue(TEXT("TC-3: fixture"), BuildPostRollFixture(Sub1, 1, 2, 1, 3)))
    { DestroyGameWorld(W1); return false; }
    UPlayerTurnSaveData* Loaded = RoundTrip(Sub1);
    DestroyGameWorld(W1);
    if (!TestNotNull(TEXT("TC-3: Loaded"), Loaded)) return false;

    UWorld* W2 = CreateGameWorld(TEXT("TC3_Dst"));
    UPlayerTurnSubsystem* Sub2 = W2->GetSubsystem<UPlayerTurnSubsystem>();

    // spyBefore 在 restore 前绑（若 restore 误广播，bound-before 会收到 → 证 hazard）
    UTurnEventSpy* SpyBefore = MakeSpy();
    Sub2->OnPhaseChanged.AddDynamic(SpyBefore, &UTurnEventSpy::HandlePhaseChanged);

    Sub2->RestoreFromSaveData(Loaded);
    TestEqual(TEXT("TC-3: restore 静默（restore 期不广播，否则未重绑监听者丢失）"),
        SpyBefore->PhaseChanges.Num(), 0);

    // spyAfter 在 restore 后绑（模拟下游重绑 ②）
    UTurnEventSpy* SpyAfter = MakeSpy();
    Sub2->OnPhaseChanged.AddDynamic(SpyAfter, &UTurnEventSpy::HandlePhaseChanged);

    Sub2->ResumeFromLoadedState();              // ③ 广播
    if (TestEqual(TEXT("TC-3: 重绑后 Resume 广播 1 次（未丢失）"), SpyAfter->PhaseChanges.Num(), 1))
    {
        TestEqual(TEXT("TC-3: Resume 广播阶段==PostRollAction"),
            static_cast<int32>(SpyAfter->PhaseChanges[0].NewPhase), static_cast<int32>(ETurnPhase::PostRollAction));
    }

    Sub2->OnPhaseChanged.RemoveDynamic(SpyBefore, &UTurnEventSpy::HandlePhaseChanged);
    Sub2->OnPhaseChanged.RemoveDynamic(SpyAfter,  &UTurnEventSpy::HandlePhaseChanged);
    ReleaseSpy(SpyBefore); ReleaseSpy(SpyAfter);
    DestroyGameWorld(W2);
    return true;
}

// =============================================================================
// TC4（AC-4）— switch(CurrentPhase) 读已还原回合2 字段（拓扑序 restore 先于 resume）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveSer_TC4_ResumeReadsRestoredFields,
    "Rento.PlayerTurn.SaveSerialization.TC4_ResumeReadsRestoredFields",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSaveSer_TC4_ResumeReadsRestoredFields::RunTest(const FString& Parameters)
{
    using namespace SaveSerTestHelpers;
    UWorld* W1 = CreateGameWorld(TEXT("TC4_Src"));
    if (!TestNotNull(TEXT("TC-4: W1"), W1)) return false;
    UPlayerTurnSubsystem* Sub1 = W1->GetSubsystem<UPlayerTurnSubsystem>();
    Sub1->InitializeFromConfig(MakeConfig(2));
    if (!TestTrue(TEXT("TC-4: fixture"), BuildPostRollFixture(Sub1, 1, 2, 1, 3)))
    { DestroyGameWorld(W1); return false; }
    UPlayerTurnSaveData* Loaded = RoundTrip(Sub1);
    DestroyGameWorld(W1);
    if (!TestNotNull(TEXT("TC-4: Loaded"), Loaded)) return false;

    UWorld* W2 = CreateGameWorld(TEXT("TC4_Dst"));
    UPlayerTurnSubsystem* Sub2 = W2->GetSubsystem<UPlayerTurnSubsystem>();
    // fresh：CurrentActivePlayerId==-1, CurrentPhase==TurnStart, PlayerStates 空
    UTurnEventSpy* Spy = MakeSpy();
    Sub2->OnPhaseChanged.AddDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);

    Sub2->RestoreFromSaveData(Loaded);          // ① 回合2 字段写回（静默）
    Sub2->ResumeFromLoadedState();              // ③ switch(CurrentPhase) 重广播

    // 拓扑序证据：Resume 的 switch 读到的是【已还原】的回合2 字段
    //   若 resume 先于 restore（错序），会广播 {TurnStart, CD=0} → FAIL
    if (TestEqual(TEXT("TC-4: Resume 广播 1 次"), Spy->PhaseChanges.Num(), 1))
    {
        TestEqual(TEXT("TC-4: switch 读还原阶段 PostRollAction"),
            static_cast<int32>(Spy->PhaseChanges[0].NewPhase), static_cast<int32>(ETurnPhase::PostRollAction));
        TestEqual(TEXT("TC-4: switch 读还原活跃玩家 CD==2（PlayerStates+活跃id 已重建）"),
            Spy->PhaseChanges[0].ConsecutiveDoubles, 2);
    }
    TestEqual(TEXT("TC-4: 回合2 腿先于 switch（活跃玩家已还原）"),
        Sub2->GetCurrentActivePlayerId(), 1);

    Sub2->OnPhaseChanged.RemoveDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);
    ReleaseSpy(Spy);
    DestroyGameWorld(W2);
    return true;
}

// =============================================================================
// TC5（AC-5）— 枚举整数索引 append-only（运行期见证 static_assert）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveSer_TC5_EnumAppendOnly,
    "Rento.PlayerTurn.SaveSerialization.TC5_EnumAppendOnly",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSaveSer_TC5_EnumAppendOnly::RunTest(const FString& Parameters)
{
    // static_assert（PlayerTurnTypes.h 尾部）是 BLOCKING 编译期门——违反即编译失败。
    // 本 TC 是运行期见证：钉死整数索引（重排破坏旧存档）。
    TestEqual(TEXT("TC-5: ETurnPhase::TurnStart==0"),      static_cast<int32>(ETurnPhase::TurnStart),      0);
    TestEqual(TEXT("TC-5: ETurnPhase::RollPhase==1"),      static_cast<int32>(ETurnPhase::RollPhase),      1);
    TestEqual(TEXT("TC-5: ETurnPhase::MovePhase==2"),      static_cast<int32>(ETurnPhase::MovePhase),      2);
    TestEqual(TEXT("TC-5: ETurnPhase::ResolvePhase==3"),   static_cast<int32>(ETurnPhase::ResolvePhase),   3);
    TestEqual(TEXT("TC-5: ETurnPhase::PostRollAction==4"), static_cast<int32>(ETurnPhase::PostRollAction), 4);
    TestEqual(TEXT("TC-5: ETurnPhase::TurnEnd==5"),        static_cast<int32>(ETurnPhase::TurnEnd),        5);
    TestEqual(TEXT("TC-5: ETurnPhase::JailTurn==6（合法值=7 个）"), static_cast<int32>(ETurnPhase::JailTurn), 6);

    TestEqual(TEXT("TC-5: EPlayerColor::None==0"), static_cast<int32>(EPlayerColor::None), 0);
    TestEqual(TEXT("TC-5: EPlayerColor::Pink==8"), static_cast<int32>(EPlayerColor::Pink), 8);

    TestEqual(TEXT("TC-5: EAIDifficulty::None==0"),  static_cast<int32>(EAIDifficulty::None),  0);
    TestEqual(TEXT("TC-5: EAIDifficulty::Sharp==3"), static_cast<int32>(EAIDifficulty::Sharp), 3);
    return true;
}

// =============================================================================
// TC6（AC-6）— CanSaveNow：定序未启动拒存 / 首个 TurnStart 后可存
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveSer_TC6_CanSaveNowGate,
    "Rento.PlayerTurn.SaveSerialization.TC6_CanSaveNowGate",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSaveSer_TC6_CanSaveNowGate::RunTest(const FString& Parameters)
{
    using namespace SaveSerTestHelpers;
    UWorld* World = CreateGameWorld(TEXT("TC6_World"));
    if (!TestNotNull(TEXT("TC-6: World"), World)) return false;
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-6: Sub"), Sub)) { DestroyGameWorld(World); return false; }

    // 未开局（0 玩家）→ 拒存
    TestFalse(TEXT("TC-6: 未开局拒存"), Sub->CanSaveNow());

    // 开局后、首个 TurnStart 未启动（CurrentActivePlayerId==-1）→ 拒存
    Sub->InitializeFromConfig(MakeConfig(2));
    TestFalse(TEXT("TC-6: 定序完成但首回合未启动 → 拒存（瞬态）"), Sub->CanSaveNow());

    // 进入首个 TurnStart 后 → 可存
    URentoPlayerState* First = nullptr;
    for (const TObjectPtr<URentoPlayerState>& S : Sub->GetPlayerStates())
    { if (S && S->TurnOrderIndex == 0) { First = S; break; } }
    if (TestNotNull(TEXT("TC-6: 找到先手"), First))
    {
        Sub->StartTurn(First->PlayerId);
        TestTrue(TEXT("TC-6: 进入首个 TurnStart 后可存"), Sub->CanSaveNow());
    }
    DestroyGameWorld(World);
    return true;
}
