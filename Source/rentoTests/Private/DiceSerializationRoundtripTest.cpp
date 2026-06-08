// DiceSerializationRoundtripTest.cpp
// =============================================================================
// 集成测试：DR-008 骰子侧序列化契约验证（纯验证型，零 src/ 改动）
//
// 物理路径：Source/rentoTests/Private/DiceSerializationRoundtripTest.cpp
// 逻辑分类：tests/integration/dice/dice_serialization_roundtrip_test.cpp
// Automation 类目：Rento.Dice.Serialization
//
// TC↔AC 映射：
//   TC1 (AC-1) — DiceService 无 UPROPERTY(SaveGame) Seed 字段 + FDiceRollResult 完整四字段（反射）
//   TC2 (AC-2) — reseed 非确定值 != 开局种子 + 序列发散；edge: reseed 回 S0 复现
//   TC3 (AC-3) — 读档不重掷：restore+resume 期 dice OnDiceRolled 0 次 + CurrentRollContext 保留
//   TC4 (AC-4) — FDiceRollResult round-trip 逐字段 identity（隔离 dice payload）
//
// 机制：headless -nullrhi，UGameplayStatics::SaveGameToMemory/LoadGameFromMemory（in-memory）
//
// 与 pt-008 的分工：
//   pt-008 SaveSerializationDelegateRebindTest.cpp 验证 subsystem round-trip（phase/player/CD/结构）
//   本文件专攻骰子侧 AC：
//     AC-1 DiceService 反射（无 SaveGame Seed）+ FDiceRollResult 四字段结构
//     AC-2 SetSeed 非确定值语义（不回开局种子 + 序列发散）
//     AC-3 读档不重掷（dice OnDiceRolled 0 次）+ Die1/Die2 VFX 保留
//     AC-4 FDiceRollResult payload 隔离 round-trip identity
//
// 已知坑遵守：
//   G1: EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter（逐字）
//   G4: UObject（spy/save）跨 SaveGameToMemory 或回调注册期 AddToRoot/RemoveFromRoot
//   G7: 内置 SaveGameToMemory 不过滤 SaveGame 字段（全量序列化）— 禁写"过滤"断言，只断 round-trip 恒等
// =============================================================================

#include "Misc/AutomationTest.h"
#include "UObject/Class.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Kismet/GameplayStatics.h"

#include "DiceRngService.h"
#include "PlayerTurnSubsystem.h"
#include "PlayerTurnSaveData.h"
#include "RentoPlayerState.h"
#include "PlayerTurnTypes.h"
#include "GameSetupConfig.h"
#include "DiceRollCountSpy.h"

// =============================================================================
// 文件内部辅助 namespace（TC3 复用 pt-008 helper 范式，独立实现不引用外部 .cpp）
// =============================================================================
namespace DiceSerTestHelpers
{
    /** 创建 headless Game World（-nullrhi 安全）。 */
    static UWorld* CreateGameWorld(const FName& WorldName)
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, WorldName);
        check(World);
        return World;
    }

    /** 销毁 Game World。 */
    static void DestroyGameWorld(UWorld* World)
    {
        if (World) { World->DestroyWorld(/*bInformEngineOfWorld=*/false); }
    }

    /** 构造开局配置（N 个玩家，默认阈值 3，种子 42）。 */
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

    /**
     * 将 ActiveId 玩家的 FSM 推进到 PostRollAction，并直构造 fixture（CD + roll context）。
     * 复用 pt-008 SaveSerTestHelpers::BuildPostRollFixture 同款逻辑。
     */
    static bool BuildPostRollFixture(UPlayerTurnSubsystem* Sub, int32 ActiveId,
                                     int32 CD, int32 Die1, int32 Die2)
    {
        URentoPlayerState* PS = Sub->FindPlayerById(ActiveId);
        if (!PS) { return false; }
        PS->bIsInJail = false;                              // 确保路由 RollPhase（非 JailTurn）
        Sub->StartTurn(ActiveId);
        if (Sub->GetCurrentPhase() != ETurnPhase::RollPhase) { return false; }
        bool bSent = false;
        Sub->ProcessRollResult(/*bIsDouble=*/false, bSent); // → MovePhase（CD 被归零）
        if (Sub->GetCurrentPhase() != ETurnPhase::MovePhase) { return false; }
        Sub->AdvanceFromMovePhase();                        // → ResolvePhase
        Sub->AdvanceFromResolvePhase();                     // → PostRollAction
        if (Sub->GetCurrentPhase() != ETurnPhase::PostRollAction) { return false; }
        // 直构造 fixture 状态（FSM 到位后写，避免 ProcessRollResult 归零 CD）
        PS->ConsecutiveDoubles = CD;
        FDiceRollResult Roll;
        Roll.Die1 = Die1; Roll.Die2 = Die2;
        Roll.Total = Die1 + Die2; Roll.bIsDouble = (Die1 == Die2);
        PS->CurrentRollContext = Roll;
        return true;
    }

    /**
     * round-trip：CaptureSaveData → SaveGameToMemory → LoadGameFromMemory → Cast。
     * UPlayerTurnSaveData 在 SaveGameToMemory 期间 AddToRoot 防 GC（G4）。
     */
    static UPlayerTurnSaveData* RoundTrip(UPlayerTurnSubsystem* SrcSub)
    {
        UPlayerTurnSaveData* Save = SrcSub->CaptureSaveData();
        if (!Save) { return nullptr; }
        Save->AddToRoot();  // G4：跨 SaveGameToMemory 防 GC
        TArray<uint8> Bytes;
        const bool bSaved = UGameplayStatics::SaveGameToMemory(Save, Bytes);
        Save->RemoveFromRoot();
        if (!bSaved || Bytes.Num() == 0) { return nullptr; }
        return Cast<UPlayerTurnSaveData>(UGameplayStatics::LoadGameFromMemory(Bytes));
    }
}

// =============================================================================
// TC2 用：连续掷骰 Total 捕获辅助（文件内 static）
// 调用 N 次 RollDice()，收集 Total 序列，用于序列发散/复现对比。
// =============================================================================
static TArray<int32> CaptureTotals(UDiceRngService* Svc, int32 N)
{
    TArray<int32> Seq;
    Seq.Reserve(N);
    for (int32 i = 0; i < N; ++i)
    {
        Seq.Add(Svc->RollDice().Total);
    }
    return Seq;
}

// =============================================================================
// TC1 (AC-1) — DiceService 无 SaveGame Seed 字段 + FDiceRollResult 完整四字段（反射）
// 类目：Rento.Dice.Serialization.TC1_NoSaveGameSeedField
//
// 验证策略：
//   (a) TFieldIterator<FProperty> 遍历 UDiceRngService 本类，断言无 UPROPERTY(SaveGame) 字段
//   (b) 非 vacuous 守卫：FixedSeed 字段反射可见但 NOT SaveGame（证判别性非"类无任何字段"）
//   (c) FDiceRollResult 完整四字段（Die1/Die2/Total/bIsDouble）反射可见
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceSer_TC1_NoSaveGameSeedField,
    "Rento.Dice.Serialization.TC1_NoSaveGameSeedField",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceSer_TC1_NoSaveGameSeedField::RunTest(const FString& Parameters)
{
    // ---- (a) UDiceRngService 本类无任何 UPROPERTY(SaveGame) 字段（AC-1：权威 Seed 不序列化）----
    const UClass* ServiceClass = UDiceRngService::StaticClass();
    if (!TestNotNull(TEXT("TC-1: UDiceRngService::StaticClass() 应非 null"), ServiceClass))
    {
        return false;
    }

    // EFieldIterationFlags::None = 仅遍历本类，不含父类（与 pt-008 同款 idiom）
    int32 SaveGameCount = 0;
    for (TFieldIterator<FProperty> It(ServiceClass, EFieldIterationFlags::None); It; ++It)
    {
        if ((*It)->HasAnyPropertyFlags(CPF_SaveGame))
        {
            ++SaveGameCount;
        }
    }
    TestEqual(
        TEXT("TC-1: DiceService 本类 UPROPERTY(SaveGame) 字段数==0（权威 Seed 不序列化）"),
        SaveGameCount, 0);

    // ---- (b) 非 vacuous 守卫：FixedSeed 字段反射可见但 NOT SaveGame ----
    // 证明 SaveGame 检查是判别性的，而非"类恰好没有任何字段"导致的 vacuous 通过
    const FProperty* FixedSeedProp = ServiceClass->FindPropertyByName(FName(TEXT("FixedSeed")));
    TestNotNull(
        TEXT("TC-1: FixedSeed 字段反射可见（类确有 seed 相关状态，SaveGame 检查非 vacuous）"),
        FixedSeedProp);
    if (FixedSeedProp)
    {
        // FixedSeed 是 EditDefaultsOnly 调试旋钮，不得入存档（AC-1）
        TestFalse(
            TEXT("TC-1: FixedSeed 非 UPROPERTY(SaveGame)（调试旋钮不入存档）"),
            FixedSeedProp->HasAnyPropertyFlags(CPF_SaveGame));
    }

    // ---- (c) FDiceRollResult 完整四字段（含 Die1/Die2，不可只存 Total/bIsDouble）----
    // B1 契约：VFX 需要 Die1/Die2 分别画两颗骰面，禁从 Total 拆解
    const UScriptStruct* RollStruct = FDiceRollResult::StaticStruct();
    if (!TestNotNull(TEXT("TC-1: FDiceRollResult::StaticStruct() 应非 null"), RollStruct))
    {
        return false;
    }

    // 逐一验证四字段存在（Die1/Die2 尤为关键：Total=4 无法判定是 1+3 还是 2+2）
    const FName RequiredFields[] = {
        FName(TEXT("Die1")),
        FName(TEXT("Die2")),
        FName(TEXT("Total")),
        FName(TEXT("bIsDouble"))
    };
    for (const FName& FieldName : RequiredFields)
    {
        TestNotNull(
            *FString::Printf(TEXT("TC-1: FDiceRollResult 含字段 '%s'"), *FieldName.ToString()),
            RollStruct->FindPropertyByName(FieldName));
    }

    return true;
}

// =============================================================================
// TC2 (AC-2) — reseed 非确定值 != 开局种子 + 序列发散；edge: reseed 回 S0 复现
// 类目：Rento.Dice.Serialization.TC2_ReseedNonDeterministicDiverges
//
// 验证策略：
//   1. S0 开局序列 SeqA（确定性捕获）
//   2. S1（非确定值，!=S0）reseed 后序列 SeqB != SeqA（发散）
//   3. edge：SetSeed(S0) 复现序列 SeqC == SeqA（证 SeqA 确定 + SeqB!=SeqA 非 vacuous）
//      此 edge 同时揭示"读档误 reseed 回开局种子"的 bug 根因
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceSer_TC2_ReseedNonDeterministicDiverges,
    "Rento.Dice.Serialization.TC2_ReseedNonDeterministicDiverges",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceSer_TC2_ReseedNonDeterministicDiverges::RunTest(const FString& Parameters)
{
    // S1 模拟读档时的非确定 reseed 值（!=开局 S0，模拟运行时新非确定种子）
    constexpr int32 S0 = 12345;
    constexpr int32 S1 = 67890;
    constexpr int32 N  = 8;  // 捕获 8 次掷骰 Total，足以统计区分两条序列

    UDiceRngService* Svc = NewObject<UDiceRngService>(GetTransientPackage());
    if (!TestNotNull(TEXT("TC-2: UDiceRngService 应能通过 NewObject<> 构造"), Svc))
    {
        return false;
    }

    // ---- 开局 SetSeed(S0)，捕获序列 SeqA ----
    Svc->SetSeed(S0);
    TestEqual(TEXT("TC-2: 开局 GetInitialSeed()==S0"), Svc->GetInitialSeed(), S0);
    const TArray<int32> SeqA = CaptureTotals(Svc, N);

    // ---- 读档 reseed 非确定值 S1（AC-2：不可重设回开局种子）----
    Svc->SetSeed(S1);
    TestEqual(TEXT("TC-2: reseed GetInitialSeed()==S1"), Svc->GetInitialSeed(), S1);
    TestNotEqual(
        TEXT("TC-2: reseed 种子 != 开局种子 S0（不可重设回开局，否则后续序列可复现重复）"),
        Svc->GetInitialSeed(), S0);

    // 读档后掷骰序列应与开局发散（S1 != S0 → 不同序列）
    const TArray<int32> SeqB = CaptureTotals(Svc, N);
    TestTrue(
        TEXT("TC-2: reseed(S1 非 S0) 后续掷骰序列与开局发散（SeqB != SeqA，不可复现开局序列）"),
        SeqB != SeqA);

    // ---- edge：reseed 回开局 S0 → 序列 C == A（复现，证 SeqA 确定 + SeqB!=SeqA 非 vacuous）----
    // 此 edge 揭示"读档误 reseed 回 S0"的根因：整条后续序列与开局可复现地重复
    Svc->SetSeed(S0);
    const TArray<int32> SeqC = CaptureTotals(Svc, N);
    TestTrue(
        TEXT("TC-2 edge: SetSeed(S0) 复现开局序列 SeqC==SeqA（='不可重设回开局种子'的根因，读档须用非确定值）"),
        SeqC == SeqA);

    return true;
}

// =============================================================================
// TC3 (AC-3) — 读档不重掷：restore+resume 期 dice OnDiceRolled 0 次 + CurrentRollContext 保留
// 类目：Rento.Dice.Serialization.TC3_LoadDoesNotRerollDice
//
// 验证策略：
//   1. 源局 BuildPostRollFixture(player1, CD=0, Die1=2, Die2=5)
//   2. RoundTrip 存读 → 目标局
//   3. spy dice OnDiceRolled，跨 RestoreFromSaveData + ResumeFromLoadedState
//   4. 断言 ReceiveCount==0（不重掷）
//   5. 断言 Die1==2 / Die2==5 / Total==7（VFX 保留，不默认 0，不被新值覆盖）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceSer_TC3_LoadDoesNotRerollDice,
    "Rento.Dice.Serialization.TC3_LoadDoesNotRerollDice",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceSer_TC3_LoadDoesNotRerollDice::RunTest(const FString& Parameters)
{
    using namespace DiceSerTestHelpers;

    // ---- 源局：player1 PostRollAction，CD=0，roll={Die1=2, Die2=5}（Total=7，非 double）----
    UWorld* W1 = CreateGameWorld(TEXT("DiceSerTC3_Src"));
    if (!TestNotNull(TEXT("TC-3: W1 源局"), W1)) { return false; }
    UPlayerTurnSubsystem* Sub1 = W1->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-3: Sub1"), Sub1)) { DestroyGameWorld(W1); return false; }
    Sub1->InitializeFromConfig(MakeConfig(2));
    if (!TestTrue(TEXT("TC-3: BuildPostRollFixture(player1, CD=0, Die1=2, Die2=5)"),
        BuildPostRollFixture(Sub1, /*ActiveId=*/1, /*CD=*/0, /*Die1=*/2, /*Die2=*/5)))
    {
        DestroyGameWorld(W1);
        return false;
    }

    // round-trip（存档 + 读档，Die1/Die2 均存活）
    UPlayerTurnSaveData* Loaded = RoundTrip(Sub1);
    DestroyGameWorld(W1);
    if (!TestNotNull(TEXT("TC-3: Loaded round-trip"), Loaded)) { return false; }

    // ---- 目标局：spy dice OnDiceRolled，跨 restore+resume 全程 ----
    UWorld* W2 = CreateGameWorld(TEXT("DiceSerTC3_Dst"));
    if (!TestNotNull(TEXT("TC-3: W2 目标局"), W2)) { return false; }
    UPlayerTurnSubsystem* Sub2 = W2->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-3: Sub2"), Sub2)) { DestroyGameWorld(W2); return false; }
    UDiceRngService* Dice2 = W2->GetSubsystem<UDiceRngService>();
    if (!TestNotNull(TEXT("TC-3: Dice2 subsystem"), Dice2)) { DestroyGameWorld(W2); return false; }

    // spy：G4 AddToRoot 防 GC（跨 restore+resume 回调注册期）
    UDiceRollCountSpy* Spy = NewObject<UDiceRollCountSpy>(GetTransientPackage());
    Spy->AddToRoot();  // G4
    Dice2->OnDiceRolled.AddDynamic(Spy, &UDiceRollCountSpy::OnRollReceived);

    // ① 静默还原回合2 字段（含 CurrentRollContext，不应触发掷骰）
    Sub2->RestoreFromSaveData(Loaded);
    // ③ switch(CurrentPhase) 重广播（不应触发掷骰，仅重广播阶段事件）
    Sub2->ResumeFromLoadedState();

    // 解绑后断言（确保 spy 不再接收后续事件）
    Dice2->OnDiceRolled.RemoveDynamic(Spy, &UDiceRollCountSpy::OnRollReceived);

    // 核心断言：restore+resume 全程 dice OnDiceRolled 触发 0 次（不重掷当前骰）
    TestEqual(
        TEXT("TC-3: 读档(restore+resume)全程 dice OnDiceRolled 触发 0 次（不重掷当前骰，player-turn 序列化保护）"),
        Spy->ReceiveCount, 0);

    // G4：释放 root 引用
    Spy->RemoveFromRoot();

    // ---- 值保留：当前回合骰 == 存档前 {Die1=2, Die2=5}（非默认 0，非重掷新值）----
    // Die1/Die2 均存活 → VFX 可忠实重现骰面（2+5 而非 Total=7 的任意拆解）
    URentoPlayerState* P1 = Sub2->FindPlayerById(1);
    if (TestNotNull(TEXT("TC-3: 还原玩家1"), P1))
    {
        const FDiceRollResult& R = P1->CurrentRollContext;
        TestEqual(TEXT("TC-3: Die1==2（序列化保留，VFX 第一颗骰面）"), R.Die1, 2);
        TestEqual(TEXT("TC-3: Die2==5（序列化保留，VFX 区分 2+5 与其他合=7 组合）"), R.Die2, 5);
        TestEqual(TEXT("TC-3: Total==7（Die1+Die2 一致）"), R.Total, 7);
    }

    DestroyGameWorld(W2);
    return true;
}

// =============================================================================
// TC4 (AC-4) — FDiceRollResult round-trip 逐字段 identity（隔离 dice payload）
// 类目：Rento.Dice.Serialization.TC4_RollResultRoundTripIdentity
//
// 验证策略：
//   直构造 UPlayerTurnSaveData（不经完整 subsystem），隔离验证 dice payload 序列化。
//   fixture: Die1=1, Die2=3, Total=4, bIsDouble=false（1+3 区别于 2+2，B1 VFX 契约）
//   G7：不写"SaveGame 过滤"断言，只断 round-trip 恒等。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiceSer_TC4_RollResultRoundTripIdentity,
    "Rento.Dice.Serialization.TC4_RollResultRoundTripIdentity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceSer_TC4_RollResultRoundTripIdentity::RunTest(const FString& Parameters)
{
    // G4：存档对象跨 SaveGameToMemory AddToRoot 防 GC
    UPlayerTurnSaveData* Save = NewObject<UPlayerTurnSaveData>(GetTransientPackage());
    if (!TestNotNull(TEXT("TC-4: UPlayerTurnSaveData 构造"), Save)) { return false; }
    Save->AddToRoot();  // G4

    // fixture：Die1=1, Die2=3（1+3 不同于 2+2，Total=4 无法区分，B1 契约非 vacuous）
    FPlayerStateRecord Rec;
    Rec.PlayerId                     = 7;
    Rec.CurrentRollContext.Die1      = 1;
    Rec.CurrentRollContext.Die2      = 3;
    Rec.CurrentRollContext.Total     = 4;
    Rec.CurrentRollContext.bIsDouble = false;
    Save->PlayerRecords.Add(Rec);

    // SaveGameToMemory（G7：全量序列化，不过滤 SaveGame 标注）
    TArray<uint8> Bytes;
    const bool bSaved = UGameplayStatics::SaveGameToMemory(Save, Bytes);
    Save->RemoveFromRoot();  // G4：SaveGameToMemory 完成后释放

    if (!TestTrue(TEXT("TC-4: SaveGameToMemory 成功"), bSaved && Bytes.Num() > 0))
    {
        // save 失败无法继续后续断言
        return false;
    }

    // LoadGameFromMemory
    UPlayerTurnSaveData* Loaded = Cast<UPlayerTurnSaveData>(
        UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("TC-4: LoadGameFromMemory 成功，Cast 非 null"), Loaded))
    {
        return false;
    }

    // 记录数验证（确保至少 1 条，再取第 0 条）
    if (!TestEqual(TEXT("TC-4: 记录数==1"), Loaded->PlayerRecords.Num(), 1))
    {
        return false;
    }

    // 逐字段 identity（固定 fixture + 精确相等，无"调参直到通过"逃逸，AC-4）
    const FDiceRollResult& R = Loaded->PlayerRecords[0].CurrentRollContext;
    TestEqual(TEXT("TC-4: Die1==1（round-trip identity）"), R.Die1, 1);
    TestEqual(TEXT("TC-4: Die2==3（1+3 区别于 2+2，B1 VFX 契约：不可只存 Total=4）"), R.Die2, 3);
    TestEqual(TEXT("TC-4: Total==4（round-trip identity）"), R.Total, 4);
    TestFalse(TEXT("TC-4: bIsDouble==false（round-trip identity）"), R.bIsDouble);

    return true;
}
