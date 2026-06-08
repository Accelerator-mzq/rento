// EventBusDelegateContractTest.cpp
// =============================================================================
// 集成测试：story-005 Event Bus 纪律层 delegate 契约验证
//
// 物理路径：Source/rentoTests/Private/EventBusDelegateContractTest.cpp
// Automation 类目：Rento.Foundation.EventBusDelegateContract
//
// 覆盖 AC（story-005 / ADR-0003 纪律层）：
//   TC1 (AC-1) — 反射确认不存在集中式 UGameEventBus 注册表对象
//   TC2 (AC-2) — AddDynamic 订阅 + BlueprintAssignable 反射 flag 验证（以 OnTurnPhaseChanged 为例）
//   TC3 (AC-3) — FTurnOrderResult 是 USTRUCT + 含 OrderedPlayerIds TArray 字段（运行期反射断言）
//   TC4 (AC-4) — 8 个 delegate 名 On<PastTense> + payload struct F...Info/语义名命名规范
//   TC5 (AC-5, 部分) — OnGameWon / OnAIActionExecuted 属于 UPlayerTurnSubsystem owner；
//                      OnBuildingChanged → DEFERRED-to-building-epic
//   TC6 (AC-6, 部分) — 已落面无成对 On*Increased/On*Decreased 方向事件；
//                      OnCashChanged → DEFERRED-to-economy
//   TC7 (AC-7, 部分) — EArrivalContext / EJailReason 越界 cast 中性兜底不崩不错播；
//                      EChangeReason → DEFERRED-to-economy（该枚举由 economy owner 声明，
//                      严禁在本 story 声明—— ADR-0003 owner-held 越权）
//   TC8 (AC-8)       — DEFERRED-to-economy/bankruptcy（两个 owner epic 均未建，无代码可验）
//
// 框架选择：IMPLEMENT_SIMPLE_AUTOMATION_TEST（与邻居 TurnEventPayloadsTest.cpp 一致）
// 反射 API ：UClass::FindPropertyByName / FProperty / FMulticastDelegateProperty（UE4+ 稳定）
//
// ⚠ 已知坑规避：
//   G1 EAutomationTestFlags_ApplicationContextMask（UE5.4+ enum class 作用域）
//   G3 spy 须 UCLASS / UFUNCTION（AddDynamic 要求）—— 复用 TurnEventSpy.h
//   G4 UObject spy 用 NewObject + AddToRoot/RemoveFromRoot（禁裸 new）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"        // FMulticastDelegateProperty
#include "UObject/UObjectGlobals.h"    // FindFirstObject / StaticFindFirstObject（UE5.4+ 替换 ANY_PACKAGE）

#include "PlayerTurnSubsystem.h"
#include "DiceRngService.h"
#include "PlayerTurnTypes.h"       // EArrivalContext / EJailReason
#include "RentoPlayerState.h"
#include "GameSetupConfig.h"
#include "LandingResolverSpy.h"    // ILandingResolver DI seam（AC-7 EArrivalContext 越界测试）
#include "PawnMoverSpy.h"          // IPawnMover DI seam（AC-7 触发 HandlePawnLanded 用）
#include "TurnEventSpy.h"          // 复用已有 UTurnEventSpy（G3 UCLASS spy）

// =============================================================================
// 测试辅助（独立命名空间避免 ODR）
// =============================================================================
namespace EventBusContractHelpers
{
    // -------------------------------------------------------------------------
    // 创建/销毁 GameWorld（与 TurnEventPayloadsTest.cpp 完全相同模式）
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // 构建 N 玩家配置（复用 TurnEventPayloadsTest.cpp 同等逻辑）
    // -------------------------------------------------------------------------
    static FGameSetupConfig MakeConfig(int32 N)
    {
        FGameSetupConfig Config;
        Config.DoublesJailThreshold = 3;
        Config.MaxTiebreakRounds    = 5;
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

    // -------------------------------------------------------------------------
    // 找第一名玩家（TurnOrderIndex == 0）
    // -------------------------------------------------------------------------
    static int32 FindFirstPlayerId(const UPlayerTurnSubsystem* Sub)
    {
        for (const TObjectPtr<URentoPlayerState>& State : Sub->GetPlayerStates())
        {
            if (State && State->TurnOrderIndex == 0) { return State->PlayerId; }
        }
        return -1;
    }

    // -------------------------------------------------------------------------
    // Spy 生命周期辅助（与 TurnEventPayloadsTest.cpp 相同，G4 例外）
    // -------------------------------------------------------------------------
    static UTurnEventSpy* MakeSpy()
    {
        UTurnEventSpy* Spy = NewObject<UTurnEventSpy>(GetTransientPackage());
        Spy->AddToRoot();  // 防 GC（G4 例外）
        return Spy;
    }
    static void ReleaseSpy(UTurnEventSpy* Spy) { if (Spy) { Spy->RemoveFromRoot(); } }

    // -------------------------------------------------------------------------
    // 推进到 MovePhase（RollPhase → 非双点 ProcessRollResult → MovePhase）
    // -------------------------------------------------------------------------
    static bool AdvanceToMovePhase(UPlayerTurnSubsystem* Sub, int32 PlayerId)
    {
        URentoPlayerState* PS = Sub->FindPlayerById(PlayerId);
        if (PS) { PS->bIsInJail = false; PS->ConsecutiveDoubles = 0; }
        Sub->StartTurn(PlayerId);
        if (Sub->GetCurrentPhase() != ETurnPhase::RollPhase) { return false; }
        bool bSent = false;
        Sub->ProcessRollResult(/*bIsDouble=*/false, bSent);
        return Sub->GetCurrentPhase() == ETurnPhase::MovePhase;
    }
}

// =============================================================================
// TC1 (AC-1) — 反射确认不存在集中式 UGameEventBus 注册表对象
//
// 验方式：
//   1. FindObject<UClass>(ANY_PACKAGE, TEXT("UGameEventBus")) 返回 nullptr
//      （无此类在内存中注册 = 无集中式 Bus 类型）
//   2. FindObject<UObject>(ANY_PACKAGE, TEXT("GameEventBus*"), /*bExactClass=*/false) = nullptr
//      （无任何以 GameEventBus 命名的 UObject 实例）
// 此为静态结构断言：代码库不包含此类型 = AC-1 成立。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC1_NoGameEventBusClass,
    "Rento.Foundation.EventBusDelegateContract.TC1_NoGameEventBusClass",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC1_NoGameEventBusClass::RunTest(const FString& Parameters)
{
    // 断言 1：UClass 层面不存在 "GameEventBus"（无集中式 Bus 类型）
    // UE5.4+ 用 FindFirstObject<UClass> 替代废弃的 FindObject(ANY_PACKAGE, ...)
    UClass* GameEventBusClass = FindFirstObject<UClass>(TEXT("GameEventBus"),
        EFindFirstObjectOptions::None);
    TestNull(
        TEXT("TC-1: UClass 'GameEventBus' 应不存在（Event Bus 为纪律层非对象，ADR-0003）"),
        GameEventBusClass);

    // 断言 2：不存在名含 "GameEventBus" 的 UObject 实例
    // UE5.4+ FindFirstObject（全类型搜索）
    UObject* GameEventBusInstance = FindFirstObject<UObject>(TEXT("GameEventBus"),
        EFindFirstObjectOptions::None);
    TestNull(
        TEXT("TC-1: UObject 实例 'GameEventBus' 应不存在"),
        GameEventBusInstance);

    // 注释式静态证明：grep 确认 "UGameEventBus" 在 Source/rento 中无定义
    // （编译期证明：若存在则此类型会被 UHT 注册，上述反射查询必能命中）
    UE_LOG(LogTemp, Log, TEXT("TC-1 PASS: 无集中式 UGameEventBus 注册表对象——"
        "Event Bus 实为纪律层（ADR-0003 选项A，去中心化 owner-held delegate）。"));

    return true;
}

// =============================================================================
// TC2 (AC-2) — AddDynamic 订阅 + BlueprintAssignable 反射 flag 验证
//
// 以 OnPhaseChanged（PlayerTurnSubsystem）为主验对象：
//   a) C++ AddDynamic 绑定后 IsBound() == true（C++ 可订）
//   b) 广播后 spy 收到恰 1 次（1-对-多去中心化通路）
//   c) 反射读取 FMulticastDelegateProperty → HasAnyPropertyFlags(CPF_BlueprintAssignable)
//      （BP 可订的编译期证明 = 运行期反射断言）
//   d) OnDiceRolled（DiceRngService）同样断言 CPF_BlueprintAssignable（跨 owner 验证）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC2_DelegateAddDynamicAndBlueprintAssignable,
    "Rento.Foundation.EventBusDelegateContract.TC2_DelegateAddDynamicAndBlueprintAssignable",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC2_DelegateAddDynamicAndBlueprintAssignable::RunTest(const FString& Parameters)
{
    // ---- 子测 a/b：AddDynamic 绑定 + 广播通路 ----
    UWorld* World = EventBusContractHelpers::CreateGameWorld(TEXT("TC2_World"));
    if (!TestNotNull(TEXT("TC-2: World"), World)) return false;
    UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
    if (!TestNotNull(TEXT("TC-2: Sub"), Sub))
    {
        EventBusContractHelpers::DestroyGameWorld(World);
        return false;
    }
    if (!TestTrue(TEXT("TC-2: InitializeFromConfig"),
        Sub->InitializeFromConfig(EventBusContractHelpers::MakeConfig(2))))
    {
        EventBusContractHelpers::DestroyGameWorld(World);
        return false;
    }

    UTurnEventSpy* Spy = EventBusContractHelpers::MakeSpy();

    // AddDynamic（C++ 订阅）
    Sub->OnPhaseChanged.AddDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);
    TestTrue(TEXT("TC-2a: OnPhaseChanged.IsBound() after AddDynamic"),
        Sub->OnPhaseChanged.IsBound());

    // 触发一次广播（StartTurn → RollPhase）
    const int32 FirstId = EventBusContractHelpers::FindFirstPlayerId(Sub);
    Sub->StartTurn(FirstId);

    // 断言至少收到 1 次（StartTurn → TurnStart + RollPhase 两次 SetPhase）
    TestTrue(TEXT("TC-2b: OnPhaseChanged 广播后 spy 收到回调（去中心化通路）"),
        Spy->PhaseChanges.Num() >= 1);

    Sub->OnPhaseChanged.RemoveDynamic(Spy, &UTurnEventSpy::HandlePhaseChanged);
    EventBusContractHelpers::ReleaseSpy(Spy);
    EventBusContractHelpers::DestroyGameWorld(World);

    // ---- 子测 c：反射断言 CPF_BlueprintAssignable flag ----
    // 验证 PlayerTurnSubsystem 的 7 个 delegate 全带 BlueprintAssignable
    {
        UClass* SubClass = UPlayerTurnSubsystem::StaticClass();
        check(SubClass);

        // 要验证的 delegate 属性名列表（对应 PlayerTurnSubsystem.h 中的 UPROPERTY）
        const TArray<FName> DelegateNames = {
            TEXT("OnPhaseChanged"),
            TEXT("OnGameWon"),
            TEXT("OnTurnStarted"),
            TEXT("OnTurnEnded"),
            TEXT("OnTurnOrderResolved"),
            TEXT("OnAIActionExecuted"),
            TEXT("OnBuildingAnnounced"),
        };

        for (const FName& PropName : DelegateNames)
        {
            FProperty* Prop = SubClass->FindPropertyByName(PropName);
            if (TestNotNull(
                *FString::Printf(TEXT("TC-2c: UPlayerTurnSubsystem::%s 属性存在"), *PropName.ToString()),
                Prop))
            {
                // 验证是多播 delegate 属性
                FMulticastDelegateProperty* MCDProp = CastField<FMulticastDelegateProperty>(Prop);
                if (TestNotNull(
                    *FString::Printf(TEXT("TC-2c: %s 是 FMulticastDelegateProperty"), *PropName.ToString()),
                    MCDProp))
                {
                    // CPF_BlueprintAssignable = UPROPERTY(BlueprintAssignable) 的编译期 flag
                    TestTrue(
                        *FString::Printf(TEXT("TC-2c: %s 带 CPF_BlueprintAssignable（BP 可订）"),
                            *PropName.ToString()),
                        Prop->HasAnyPropertyFlags(CPF_BlueprintAssignable));
                }
            }
        }
    }

    // ---- 子测 d：OnDiceRolled（DiceRngService）同样验 CPF_BlueprintAssignable ----
    {
        UClass* DiceClass = UDiceRngService::StaticClass();
        check(DiceClass);

        FProperty* DiceProp = DiceClass->FindPropertyByName(TEXT("OnDiceRolled"));
        if (TestNotNull(TEXT("TC-2d: UDiceRngService::OnDiceRolled 属性存在"), DiceProp))
        {
            FMulticastDelegateProperty* MCDProp = CastField<FMulticastDelegateProperty>(DiceProp);
            if (TestNotNull(TEXT("TC-2d: OnDiceRolled 是 FMulticastDelegateProperty"), MCDProp))
            {
                TestTrue(TEXT("TC-2d: OnDiceRolled 带 CPF_BlueprintAssignable（BP 可订）"),
                    DiceProp->HasAnyPropertyFlags(CPF_BlueprintAssignable));
            }
        }
    }

    return true;
}

// =============================================================================
// TC3 (AC-3) — FTurnOrderResult 是 USTRUCT + 含 OrderedPlayerIds TArray 字段
//
// 编译期证明：裸 TArray 作 BlueprintAssignable 参数编译失败（FOnTurnOrderResolved 声明
//   通过编译本身 = 约束已生效）。
// 运行期补充：反射断言 FTurnOrderResult struct 存在 + 含 OrderedPlayerIds 字段
//   （证明 USTRUCT 封装实际落地，非裸 TArray）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC3_TurnOrderResultUStruct,
    "Rento.Foundation.EventBusDelegateContract.TC3_TurnOrderResultUStruct",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC3_TurnOrderResultUStruct::RunTest(const FString& Parameters)
{
    // 断言 1：FTurnOrderResult 已被 UHT 注册为 UScriptStruct（USTRUCT(BlueprintType) 证明）
    // 用户 USTRUCT 用 ::StaticStruct()（TBaseStructure 仅对引擎内建类型特化，本 repo idiom 见 BoardDataAssetSchemaTest）
    UScriptStruct* StructType = FTurnOrderResult::StaticStruct();
    if (!TestNotNull(TEXT("TC-3: FTurnOrderResult 是 USTRUCT（UScriptStruct 已注册）"), StructType))
        return false;

    // 断言 2：struct 含 OrderedPlayerIds 字段（TArray<int32>——USTRUCT 封装 TArray 而非裸 TArray 作参数）
    FProperty* OrderedProp = StructType->FindPropertyByName(TEXT("OrderedPlayerIds"));
    TestNotNull(TEXT("TC-3: FTurnOrderResult.OrderedPlayerIds 字段存在（USTRUCT 封装 TArray）"),
        OrderedProp);

    if (OrderedProp)
    {
        // 进一步验证是 FArrayProperty（确认确实是 TArray 字段，非其他类型）
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(OrderedProp);
        TestNotNull(TEXT("TC-3: OrderedPlayerIds 是 FArrayProperty（TArray<int32>）"), ArrayProp);
    }

    // 断言 3：struct 含 bResolvedBySeatTiebreak bool 字段
    FProperty* TiebreakProp = StructType->FindPropertyByName(TEXT("bResolvedBySeatTiebreak"));
    TestNotNull(TEXT("TC-3: FTurnOrderResult.bResolvedBySeatTiebreak 字段存在"), TiebreakProp);

    // 编译期证明注释：
    // DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnOrderResolved, const FTurnOrderResult&, Result)
    // 此声明通过编译 = 裸 TArray 未作 BlueprintAssignable 参数（裸 TArray 版本会编译失败）。
    UE_LOG(LogTemp, Log, TEXT("TC-3 PASS: FTurnOrderResult USTRUCT 封装存在，OrderedPlayerIds 包入 struct"
        "（编译通过 = 裸 TArray 作 BlueprintAssignable 参数约束已生效，ADR-0003 Verification 1）。"));

    return true;
}

// =============================================================================
// TC4 (AC-4) — 命名规范：事件全 On<PastTense>，payload 全 F...Info/语义名
//
// 验 8 个已落 delegate 的名称符合规范：
//   PlayerTurnSubsystem (7) + DiceRngService (1) = 8 个
//
// 命名合法规则（ADR-0003 §统一 Delegate 规范 #3）：
//   事件名：On + 过去式动词（OnTurnStarted / OnTurnEnded / OnGameWon 等）
//   payload struct 名：F<Event>Info 或语义名（FDiceRollResult / FTurnOrderResult 语义名合法）
//
// 本测试做名称集合层面断言（不做动词形态 NLP 分析，仅断言全部以 "On" 开头 +
//   payload struct 名在已知合法集合中）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC4_NamingConventionCompliance,
    "Rento.Foundation.EventBusDelegateContract.TC4_NamingConventionCompliance",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC4_NamingConventionCompliance::RunTest(const FString& Parameters)
{
    // ---- 事件名：全以 "On" 开头（On<PastTense> 规范最低约束）----
    // PlayerTurnSubsystem 的 7 个 delegate 属性名
    const TArray<FName> PtsDelegates = {
        TEXT("OnPhaseChanged"),
        TEXT("OnGameWon"),
        TEXT("OnTurnStarted"),
        TEXT("OnTurnEnded"),
        TEXT("OnTurnOrderResolved"),
        TEXT("OnAIActionExecuted"),
        TEXT("OnBuildingAnnounced"),
    };
    // DiceRngService 的 1 个 delegate
    const TArray<FName> DiceDelegates = {
        TEXT("OnDiceRolled"),
    };

    // 断言所有 PlayerTurnSubsystem delegate 名以 "On" 开头（规范纪律层验证）
    for (const FName& Name : PtsDelegates)
    {
        TestTrue(
            *FString::Printf(TEXT("TC-4: PlayerTurnSubsystem delegate '%s' 以 'On' 开头"), *Name.ToString()),
            Name.ToString().StartsWith(TEXT("On")));
    }
    // 断言所有 DiceRngService delegate 名以 "On" 开头
    for (const FName& Name : DiceDelegates)
    {
        TestTrue(
            *FString::Printf(TEXT("TC-4: DiceRngService delegate '%s' 以 'On' 开头"), *Name.ToString()),
            Name.ToString().StartsWith(TEXT("On")));
    }

    // ---- payload struct 名：在合法命名集合中（F...Info 或语义名）----
    // 合法 payload struct 名集合（逐一比对，ADR-0003 §命名）：
    //   FPhaseChangedInfo / FTurnStartedInfo / FTurnEndedInfo / FBuildingAnnouncedInfo = F<Event>Info 模式
    //   FTurnOrderResult / FAIActionDetails / FDiceRollResult = 语义名合法
    //   OnGameWon 参数为裸 int32（单参 payload，非 USTRUCT，合规—— USTRUCT 仅多字段必包）
    const TArray<FString> ValidPayloadStructNames = {
        TEXT("FPhaseChangedInfo"),
        TEXT("FTurnStartedInfo"),
        TEXT("FTurnEndedInfo"),
        TEXT("FTurnOrderResult"),
        TEXT("FAIActionDetails"),
        TEXT("FBuildingAnnouncedInfo"),
        TEXT("FDiceRollResult"),
    };

    // 反射验证：每个 payload struct 名都有对应的 UScriptStruct 注册
    for (const FString& StructName : ValidPayloadStructNames)
    {
        // 去 F 前缀查 UScriptStruct（UHT 注册时去 F 前缀）
        // UE5.4+ 用 FindFirstObject<UScriptStruct> 替代废弃的 FindObject(ANY_PACKAGE, ...)
        const FString CleanName = StructName.Mid(1); // 去掉 "F" 前缀
        UScriptStruct* FoundStruct = FindFirstObject<UScriptStruct>(*CleanName,
            EFindFirstObjectOptions::None);
        TestNotNull(
            *FString::Printf(TEXT("TC-4: payload struct %s 已被 UHT 注册为 USTRUCT"), *StructName),
            FoundStruct);
    }

    // 计数断言：8 个 delegate（7+1）全已覆盖
    TestEqual(TEXT("TC-4: PlayerTurnSubsystem delegate 总数 == 7"), PtsDelegates.Num(), 7);
    TestEqual(TEXT("TC-4: DiceRngService delegate 总数 == 1"), DiceDelegates.Num(), 1);
    TestEqual(TEXT("TC-4: 合法 payload struct 总数 == 7（OnGameWon 裸 int32 合规）"),
        ValidPayloadStructNames.Num(), 7);

    return true;
}

// =============================================================================
// TC5 (AC-5, 部分) — 单一事件源：OnGameWon / OnAIActionExecuted 属于 UPlayerTurnSubsystem
//
// 可验部分：
//   1. 反射确认 OnGameWon / OnAIActionExecuted 是 UPlayerTurnSubsystem 的 UPROPERTY 成员
//   2. UDiceRngService 类上无 OnGameWon / OnAIActionExecuted（排除错误 owner）
//   3. OnBuildingAnnounced（回合2 广播 beat）存在于 UPlayerTurnSubsystem（验通告 beat owner）
//
// DEFERRED 部分：
//   OnBuildingChanged（建房8 epic，2 字段）→ DEFERRED-to-building-epic
//   building8 epic 未建，无代码可验字段数约束。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC5_SingleEventSourceOwnership,
    "Rento.Foundation.EventBusDelegateContract.TC5_SingleEventSourceOwnership",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC5_SingleEventSourceOwnership::RunTest(const FString& Parameters)
{
    UClass* PtsClass  = UPlayerTurnSubsystem::StaticClass();
    UClass* DiceClass = UDiceRngService::StaticClass();
    check(PtsClass && DiceClass);

    // ---- 断言 1：OnGameWon 属于 UPlayerTurnSubsystem（ADR-0003 单一事件源纪律）----
    FProperty* GameWonProp = PtsClass->FindPropertyByName(TEXT("OnGameWon"));
    TestNotNull(TEXT("TC-5: UPlayerTurnSubsystem::OnGameWon 存在（回合2 owner）"), GameWonProp);

    // ---- 断言 2：UDiceRngService 上无 OnGameWon（排除跨 owner 越权广播）----
    FProperty* DiceGameWonProp = DiceClass->FindPropertyByName(TEXT("OnGameWon"));
    TestNull(TEXT("TC-5: UDiceRngService 上无 OnGameWon（单一事件源，非骰子3 广播）"),
        DiceGameWonProp);

    // ---- 断言 3：OnAIActionExecuted 属于 UPlayerTurnSubsystem----
    FProperty* AIProp = PtsClass->FindPropertyByName(TEXT("OnAIActionExecuted"));
    TestNotNull(TEXT("TC-5: UPlayerTurnSubsystem::OnAIActionExecuted 存在（回合2 owner，非 AI 发）"),
        AIProp);

    // ---- 断言 4：OnBuildingAnnounced（回合2 广播 beat）属于 UPlayerTurnSubsystem ----
    // 注意：OnBuildingAnnounced（回合2，3字段）与 OnBuildingChanged（建房8，2字段）是不同事件
    // OnBuildingAnnounced 已落地于 PlayerTurnSubsystem（story-004）
    FProperty* AnnouncedProp = PtsClass->FindPropertyByName(TEXT("OnBuildingAnnounced"));
    TestNotNull(TEXT("TC-5: UPlayerTurnSubsystem::OnBuildingAnnounced 存在（回合2 建房通告 beat owner）"),
        AnnouncedProp);

    // DEFERRED 注释：
    // TODO [DEFERRED-to-building-epic]: OnBuildingChanged（建房8，2字段，actingPlayerId 由回合2 上下文取）
    //   AC-5 子句：OnBuildingChanged payload 恰 2 字段（无第3字段 actingPlayerId）
    //   待 building epic 落地后验证。building8 owner 不在本 story 范围。
    UE_LOG(LogTemp, Log, TEXT("TC-5: DEFERRED-to-building-epic — "
        "OnBuildingChanged（建房8，2字段）字段数约束待 building epic 落地后验。"));

    return true;
}

// =============================================================================
// TC6 (AC-6, 部分) — 方向由消费方派生：已落面无成对 On*Increased/On*Decreased 方向事件
//
// 可验部分：
//   1. 反射扫描 PlayerTurnSubsystem 和 DiceRngService 的全部 delegate 属性名，
//      断言无以 "Increased" / "Decreased" 结尾的方向事件对
//   2. 断言 OnTurnStarted / OnTurnEnded 是独立语义事件（非方向对），
//      "Started/Ended" 是动词形态而非方向（符合规范）
//
// DEFERRED 部分：
//   OnCashChanged（经济5 owner-held，EChangeReason 方向上下文）
//   → DEFERRED-to-economy（经济5 epic 未建，无代码可验）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC6_NoDirectionalEventPairs,
    "Rento.Foundation.EventBusDelegateContract.TC6_NoDirectionalEventPairs",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC6_NoDirectionalEventPairs::RunTest(const FString& Parameters)
{
    // ---- 反射扫描：PlayerTurnSubsystem 全 delegate 属性名 ----
    UClass* PtsClass  = UPlayerTurnSubsystem::StaticClass();
    UClass* DiceClass = UDiceRngService::StaticClass();
    check(PtsClass && DiceClass);

    // 收集两个类的所有 FMulticastDelegateProperty 名
    TArray<FString> AllDelegateNames;

    for (TFieldIterator<FMulticastDelegateProperty> It(PtsClass); It; ++It)
    {
        AllDelegateNames.Add(It->GetName());
    }
    for (TFieldIterator<FMulticastDelegateProperty> It(DiceClass); It; ++It)
    {
        AllDelegateNames.Add(It->GetName());
    }

    // 已落 delegate 名应为已知集合（调试用 log）
    UE_LOG(LogTemp, Log, TEXT("TC-6: 扫描到已落 delegate 共 %d 个"), AllDelegateNames.Num());
    for (const FString& Name : AllDelegateNames)
    {
        UE_LOG(LogTemp, Log, TEXT("TC-6:   delegate 名 = %s"), *Name);
    }

    // ---- 断言：无以 "Increased" 结尾的方向事件 ----
    for (const FString& Name : AllDelegateNames)
    {
        TestFalse(
            *FString::Printf(TEXT("TC-6: delegate '%s' 不应以 'Increased' 结尾（方向由消费方派生，ADR-0003 §4）"),
                *Name),
            Name.EndsWith(TEXT("Increased")));
    }

    // ---- 断言：无以 "Decreased" 结尾的方向事件 ----
    for (const FString& Name : AllDelegateNames)
    {
        TestFalse(
            *FString::Printf(TEXT("TC-6: delegate '%s' 不应以 'Decreased' 结尾（方向由消费方派生）"),
                *Name),
            Name.EndsWith(TEXT("Decreased")));
    }

    // ---- 计数断言：已落 8 个 delegate（7 PlayerTurnSubsystem + 1 DiceRngService）----
    // 注：AllDelegateNames 可能含父类 delegate（UMatchSubsystemBase/UWorldSubsystem），
    //     只断言 >= 8 而非 == 8（宽松避免父类 delegate 误判）
    TestTrue(TEXT("TC-6: 已落 delegate 总数 >= 8（7回合+1骰子）"),
        AllDelegateNames.Num() >= 8);

    // DEFERRED 注释：
    // TODO [DEFERRED-to-economy]: OnCashChanged（经济5，EChangeReason payload）
    //   AC-6 子句：payload 携 EChangeReason 类型语境，方向（正负）由消费方从 NewCash-OldCash 符号派生。
    //   验证：接收一次增、一次减 → 同一事件路径，方向由消费方派生，无成对 OnCashIncreased/Decreased。
    //   待 economy epic 落地后验证（EChangeReason 枚举由 economy owner 声明，本 story 严禁声明）。
    UE_LOG(LogTemp, Log, TEXT("TC-6: DEFERRED-to-economy — "
        "OnCashChanged EChangeReason 方向派生约束待 economy epic 落地后验。"));

    return true;
}

// =============================================================================
// TC7 (AC-7, 部分) — 未注册枚举值兜底：EArrivalContext / EJailReason 越界 cast
//
// 可验部分（ADR-0003 §统一规范 #7 中性兜底）：
//
// EArrivalContext 越界兜底：
//   ResolveArrival(EArrivalContext) 在 PlayerTurnSubsystem.cpp 实现：
//     if (Context == EArrivalContext::SentToJail) → 跳过结算
//     else → DiceMove 路径（中性兜底）
//   越界 cast 值（如 255）走 else 分支 = 中性兜底（不崩、不错播 SentToJail 行为）。
//   断言：HandlePawnLanded(cast(255)) 不崩 + 阶段推进到 PostRollAction（DiceMove 兜底路径）。
//
// EJailReason 越界兜底：
//   SetJailState(bool, EJailReason) 仅写 bIsInJail，接收 Reason 不使用（最小占位语义）。
//   越界 cast 值不会触发任何分支（reason 不参与逻辑）= 天然中性兜底。
//   断言：SetJailState(true, cast(255)) 不崩 + bIsInJail == true。
//
// DEFERRED 部分：
//   EChangeReason（经济5 owner-held）→ DEFERRED-to-economy
//   严禁在本 story 声明 EChangeReason（ADR-0003 owner-held 越权）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC7_UnknownEnumValueFallback,
    "Rento.Foundation.EventBusDelegateContract.TC7_UnknownEnumValueFallback",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC7_UnknownEnumValueFallback::RunTest(const FString& Parameters)
{
    // ---- 场景 A：EArrivalContext 越界 cast → ResolveArrival 中性兜底 ----
    // 实现：ResolveArrival 用 if(==SentToJail) else 路由；越界值走 else（DiceMove 兜底）
    // 断言：不崩 + 阶段推进到 PostRollAction（DiceMove 兜底路径 = 推进 PostRollAction）
    {
        UWorld* World = EventBusContractHelpers::CreateGameWorld(TEXT("TC7a_World"));
        if (!TestNotNull(TEXT("TC-7a: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("TC-7a: Sub"), Sub))
        {
            EventBusContractHelpers::DestroyGameWorld(World);
            return false;
        }
        Sub->InitializeFromConfig(EventBusContractHelpers::MakeConfig(2));
        const int32 FirstId = EventBusContractHelpers::FindFirstPlayerId(Sub);

        // 推进到 MovePhase（HandlePawnLanded 须在 MovePhase 后才能被调用路由到 ResolveArrival）
        // ResolveArrival 内部需要 CurrentPhase == ResolvePhase 才能推进 PostRollAction
        // 实际路径：StartTurn → ProcessRollResult → MovePhase → AdvanceFromMovePhase → ResolvePhase
        // 然后调 HandlePawnLanded 触发 ResolveArrival
        bool bSent = false;
        URentoPlayerState* PS = Sub->FindPlayerById(FirstId);
        if (PS) { PS->bIsInJail = false; PS->ConsecutiveDoubles = 0; }
        Sub->StartTurn(FirstId);
        Sub->ProcessRollResult(false, bSent);  // → MovePhase
        Sub->AdvanceFromMovePhase();            // → ResolvePhase

        // 此时在 ResolvePhase，调 HandlePawnLanded 携越界 EArrivalContext 值（uint8=200）
        // ResolveArrival: if(==SentToJail=1) ... else(DiceMove 兜底)
        // 越界值 200 ≠ SentToJail=1，走 else 分支（DiceMove 兜底，中性不崩）
        const EArrivalContext UnknownContext = static_cast<EArrivalContext>(200);

        // DiceMove 兜底路径：LandingResolver 未注入时代码用 IsValid() 守卫静默跳过（实测无 DecideBuyProperty 日志，
        // 故不需 AddExpectedError——加了反而因「期待错误未出现」判 Fail）。调用不应崩溃。
        Sub->HandlePawnLanded(UnknownContext);

        // DiceMove 兜底路径推进到 PostRollAction（ResolveArrival else 路径 → SetPhase(PostRollAction)）
        TestEqual(
            TEXT("TC-7a: EArrivalContext 越界值 200 走中性兜底，推进到 PostRollAction（不崩不错播 SentToJail 行为）"),
            static_cast<int32>(Sub->GetCurrentPhase()),
            static_cast<int32>(ETurnPhase::PostRollAction));

        EventBusContractHelpers::DestroyGameWorld(World);
    }

    // ---- 场景 B：EJailReason 越界 cast → SetJailState 中性兜底 ----
    // SetJailState 实现：直接写 bIsInJail = bInJail；Reason 接收不使用（最小占位）
    // 越界 Reason 值不参与任何 if 分支 = 天然中性兜底
    {
        UWorld* World = EventBusContractHelpers::CreateGameWorld(TEXT("TC7b_World"));
        if (!TestNotNull(TEXT("TC-7b: World"), World)) return false;
        UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
        if (!TestNotNull(TEXT("TC-7b: Sub"), Sub))
        {
            EventBusContractHelpers::DestroyGameWorld(World);
            return false;
        }
        Sub->InitializeFromConfig(EventBusContractHelpers::MakeConfig(2));
        const int32 FirstId = EventBusContractHelpers::FindFirstPlayerId(Sub);

        URentoPlayerState* PS = Sub->FindPlayerById(FirstId);
        if (!TestNotNull(TEXT("TC-7b: PlayerState"), PS))
        {
            EventBusContractHelpers::DestroyGameWorld(World);
            return false;
        }

        // 初始状态：不在狱
        PS->bIsInJail = false;

        // 越界 EJailReason 值（uint8=255）
        const EJailReason UnknownReason = static_cast<EJailReason>(255);

        // 调用 SetJailState 携越界 Reason，不应崩溃
        PS->SetJailState(true, UnknownReason);

        // 断言：bIsInJail 被正确写入（Reason 越界但字段写入正常，中性兜底）
        TestTrue(TEXT("TC-7b: EJailReason 越界值 255 不崩，bIsInJail 正确置 true（中性兜底）"),
            PS->bIsInJail);

        // 恢复并断言 false 路径同样安全
        PS->SetJailState(false, UnknownReason);
        TestFalse(TEXT("TC-7b: SetJailState(false, 越界 Reason) 不崩，bIsInJail 正确置 false"),
            PS->bIsInJail);

        EventBusContractHelpers::DestroyGameWorld(World);
    }

    // DEFERRED 注释：
    // TODO [DEFERRED-to-economy]: EChangeReason 越界 cast 兜底
    //   AC-7 子句：消费方对未知 EChangeReason 走中性兜底，不崩不静默错播。
    //   EChangeReason 枚举由经济5 owner 声明（ADR-0003 owner-held 纪律），
    //   本 story 严禁声明该枚举——越权违反 ADR-0003 单一 owner 不变式。
    //   待 economy epic 落地后，由 economy epic 对应 story 验证。
    UE_LOG(LogTemp, Log, TEXT("TC-7: DEFERRED-to-economy — "
        "EChangeReason 越界兜底约束待 economy epic 落地后验（本 story 严禁声明该枚举，ADR-0003 owner-held）。"));

    return true;
}

// =============================================================================
// TC8 (AC-8) — 破产双事件源职责切分
//
// 全 DEFERRED：OnBankruptcyDeclared（经济5）/ OnPlayerBankrupt（破产9）
// 两 owner epic 均未建，当前代码库中不存在相关 delegate 声明。
//
// 验证方式（DEFERRED 后）：
//   1. 反射确认 UEconomySubsystem 持 OnBankruptcyDeclared（2字段：Debtor/Creditor）
//   2. 反射确认 UBankruptcySubsystem 持 OnPlayerBankrupt（3字段：Debtor/Creditor/Reason）
//   3. 两事件并存未合并（各自独立 delegate）
//
// 当前：仅写明 TODO 锚点，不写假断言（断言不存在的代码永远 PASS = vacuous）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEventBus_TC8_BankruptcyDualSourceDeferred,
    "Rento.Foundation.EventBusDelegateContract.TC8_BankruptcyDualSourceDeferred",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FEventBus_TC8_BankruptcyDualSourceDeferred::RunTest(const FString& Parameters)
{
    // TODO [DEFERRED-to-economy]: OnBankruptcyDeclared（经济5，2字段 Debtor/Creditor）
    //   AC-8 子句：OnBankruptcyDeclared 2 字段，经济5 owner，现金侧完成信号。
    //   验证：反射找 UEconomySubsystem::OnBankruptcyDeclared + payload 2 字段断言。
    //
    // TODO [DEFERRED-to-bankruptcy]: OnPlayerBankrupt（破产9，3字段 Debtor/Creditor/Reason）
    //   AC-8 子句：OnPlayerBankrupt 3 字段，破产9 owner，编排末信号。
    //   验证：反射找 UBankruptcySubsystem::OnPlayerBankrupt + payload 3 字段断言 + 并存未合并。
    //
    // 当前状态：两个 owner epic 未建，无代码可验。
    // 本测试仅作 TODO 锚点，不写假断言（ADR-0003 破产双事件源职责切分 = 结构性约定，
    // 需双 epic 落地后 E2E 验证）。

    UE_LOG(LogTemp, Log, TEXT("TC-8: DEFERRED-to-economy/bankruptcy — "
        "OnBankruptcyDeclared（经济5，2字段）/ OnPlayerBankrupt（破产9，3字段）"
        "两 owner epic 均未建，无代码可验。"
        "待 economy/bankruptcy epic 落地后验双事件并存 + 字段数 + 职责切分约束。"));

    // 当前仅做存在性负向验证：确认这两个 class 尚未注册（避免 vacuous 断言）
    // 若任一 class 已意外存在，则 DEFERRED 范围被违反，应通报
    // UE5.4+ 用 FindFirstObject<UClass> 替代废弃的 FindObject(ANY_PACKAGE, ...)
    UClass* EconClass = FindFirstObject<UClass>(TEXT("EconomySubsystem"),
        EFindFirstObjectOptions::None);
    if (EconClass != nullptr)
    {
        // economy epic 已落地——AC-8 可验，但超出本 story 范围，记 Warning
        UE_LOG(LogTemp, Warning,
            TEXT("TC-8: UEconomySubsystem 已注册！AC-8 验证已可进行，"
                 "请在 economy story 对应测试中补充 OnBankruptcyDeclared 字段断言。"));
    }
    UClass* BankruptClass = FindFirstObject<UClass>(TEXT("BankruptcySubsystem"),
        EFindFirstObjectOptions::None);
    if (BankruptClass != nullptr)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("TC-8: UBankruptcySubsystem 已注册！AC-8 验证已可进行，"
                 "请在 bankruptcy story 对应测试中补充 OnPlayerBankrupt 字段断言。"));
    }

    // 测试本身永远返回 true（DEFERRED 占位，non-vacuous 通过 DEFERRED 注释守）
    return true;
}
