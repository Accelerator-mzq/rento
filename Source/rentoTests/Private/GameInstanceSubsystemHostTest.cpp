// GameInstanceSubsystemHostTest.cpp
// =============================================================================
// 集成测试：UPersistentServicesSubsystem 跨局宿主框架
// story-004 AC-1~6；headless `-nullrhi` 可测 vs defer 拆分诚实标注
//
// 物理路径：Source/rentoTests/Private/GameInstanceSubsystemHostTest.cpp
// 逻辑分类：tests/integration/foundation/game_instance_subsystem_host_test.cpp
// Automation 类目：Rento.Foundation.GameInstanceSubsystemHost
//
// ─────────────────────────────────────────────────────────────────────────────
// headless 验证策略（诚实拆分）：
//
// ✅ AC-4（solid headless，反射）：
//   TFieldIterator<FProperty>(StaticClass(), EFieldIterationFlags::None)
//   枚举宿主本类 UPROPERTY，断言无 per-match 对局态字段，且本类 UPROPERTY 总数 == 0。
//   复用 FF-001 TC-4 手法，无需实例化，最可靠的 headless 断言。
//   非 vacuous 保证：宿主类声明了若干 UFUNCTION 方法（无 UPROPERTY），若有人误加
//   UPROPERTY 字段（如 Cash/house_count）则计数非零，断言 FAIL。
//
// ✅ AC-1（结构反射，退化路径）：
//   UClass::IsChildOf(UGameInstanceSubsystem::StaticClass()) 断言继承链。
//   FindFunctionByName 断言 StartNewGame / SaveGameToSlot / LoadGameFromSlot 入口存在。
//   注：headless 下 UGameInstance::Init() 不可靠（需完整引擎栈），故退化为反射验证。
//   实例化路径（真实 GetSubsystem 验证）defer ff-007（见下）。
//
// ✅ AC-3（反射，退化路径）：
//   FindFunctionByName 断言 SaveGameToSlot / LoadGameFromSlot 挂载点存在且归属宿主类。
//   同 AC-1 退化原因。
//
// ❌ AC-2（DEFER ff-007）：
//   StartNewGame 入口跨 PIE 局存活（不随单局 World 销毁）——需真实 PIE Start→Stop→Start。
//   headless -nullrhi 不可驱动 PIE 生命周期（同 ff-007/FF-003 AC-4/DR-001 PIE 边界）。
//   不写 false-green headless 断言（项目红线）。
//
// ❌ AC-5（DEFER ff-007 + bd-007）：
//   UAssetManager 缓存 DA_Board 跨局不释放——需真 DA_Board 资产（bd-007 未实现）
//   + 跨局加载（PIE Stop→Start）。不写 false-green 断言。
//
// ❌ AC-6（DEFER ff-007）：
//   PIE Start→Stop→Start 三局宿主实例恒为同一进程单例——需真实 PIE 三局验证。
//   静态反射只能证继承链正确（反射断言在 AC-1/TC-2 中覆盖）。
//
// ─────────────────────────────────────────────────────────────────────────────
// tech-debt（ff-007 解锁后补测）：
//   - story-004 QA TC-1/TC-2/TC-3（PIE 单例性 + 跨局存活）※ 与本文件代码 TC-1~3 不同，
//     代码 TC-1~3 是结构反射断言，story-004 QA TC-1~3 是 PIE 运行时验证
//   - story-004 QA TC-5（DA_Board 跨局缓存，需 bd-007）
//
// 登记：production/epics/foundation-framework/story-004-*.md §tech-debt
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"           // TC-1 反向断言（非 WorldSubsystem）
#include "Subsystems/GameInstanceSubsystem.h"   // 硬引用，消除 FindObject 静默跳过
#include "Components/ActorComponent.h"           // 反向断言（非 ActorComponent 形态）
#include "PersistentServicesSubsystem.h"         // 被测宿主
#include "MatchSubsystemBase.h"                  // Edge 测试：与 per-match 宿主平行性断言

// =============================================================================
// TC-1（AC-1）— 继承链：IsChildOf(UGameInstanceSubsystem)
// 验证路径：结构反射（退化路径，非实例化路径）
// 理由：headless 下 UGameInstance::Init() 需完整引擎栈，不可靠；
//       静态反射证继承链正确，已足够证宿主框架挂载点分类正确（AC-1 结构要求）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPersistentServicesSubsystem_TC1_InheritsGameInstanceSubsystem,
    "Rento.Foundation.GameInstanceSubsystemHost.TC1_ClassHierarchy_InheritsUGameInstanceSubsystem",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersistentServicesSubsystem_TC1_InheritsGameInstanceSubsystem::RunTest(const FString& Parameters)
{
    const UClass* HostClass = UPersistentServicesSubsystem::StaticClass();

    // THEN-1：继承 UGameInstanceSubsystem（AC-1 跨局宿主分类正确）
    // 使用编译期 UGameInstanceSubsystem::StaticClass() 硬引用，
    // 消除 FindObject 返回 null 时的静默跳过假绿。
    TestTrue(
        TEXT("TC-1: UPersistentServicesSubsystem 应继承 UGameInstanceSubsystem（AC-1 跨局宿主）"),
        HostClass->IsChildOf(UGameInstanceSubsystem::StaticClass()));

    // THEN-2：不继承 UWorldSubsystem（per-match 宿主才继承 UWorldSubsystem，ADR-0001 分层严格）
    TestFalse(
        TEXT("TC-1: UPersistentServicesSubsystem 不应继承 UWorldSubsystem（跨局宿主 vs per-match 宿主，ADR-0001 分层）"),
        HostClass->IsChildOf(UWorldSubsystem::StaticClass()));

    // THEN-3：不继承 UActorComponent（ADR-0001 Forbidden：禁 UActorComponent 形态，见 FF-001 TC-6 手法）
    TestFalse(
        TEXT("TC-1: UPersistentServicesSubsystem 不应继承 UActorComponent（ADR-0001 Forbidden）"),
        HostClass->IsChildOf(UActorComponent::StaticClass()));

    return true;
}

// =============================================================================
// TC-2（AC-1/AC-2/AC-3）— 挂载点方法存在性：反射验证三类跨局服务入口可达
// 验证路径：FindFunctionByName 反射（退化路径，非实例调用路径）
// 理由：方法存在性是 headless 静态可证的结构约束，无需实例化宿主执行入口。
//       实例调用路径（跨局存活）defer ff-007。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPersistentServicesSubsystem_TC2_EntryPointsExist,
    "Rento.Foundation.GameInstanceSubsystemHost.TC2_Reflection_EntryPointsExist",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersistentServicesSubsystem_TC2_EntryPointsExist::RunTest(const FString& Parameters)
{
    const UClass* HostClass = UPersistentServicesSubsystem::StaticClass();

    // §20 Setup 入口：StartNewGame（AC-2 冻结签名）
    // ExcludeSuper 限定本类查找——若函数不存在/改名/仅继承自父类则返回 nullptr，FAIL（非 vacuous）
    const UFunction* StartNewGameFn = HostClass->FindFunctionByName(FName("StartNewGame"), EIncludeSuperFlag::ExcludeSuper);
    TestNotNull(
        TEXT("TC-2: UPersistentServicesSubsystem 应有 StartNewGame 入口（AC-2 §20 Setup 入口冻结签名，归属本类）"),
        StartNewGameFn);

    // §21 Save 框架挂载点：SaveGameToSlot（AC-3）
    // ExcludeSuper 限定本类查找，命中即归属本类（不含父类继承）
    const UFunction* SaveFn = HostClass->FindFunctionByName(FName("SaveGameToSlot"), EIncludeSuperFlag::ExcludeSuper);
    TestNotNull(
        TEXT("TC-2: UPersistentServicesSubsystem 应有 SaveGameToSlot 挂载点（AC-3 §21 Save 框架归属本类）"),
        SaveFn);

    // §21 Save 框架挂载点：LoadGameFromSlot（AC-3）
    // ExcludeSuper 限定本类查找，命中即归属本类（不含父类继承）
    const UFunction* LoadFn = HostClass->FindFunctionByName(FName("LoadGameFromSlot"), EIncludeSuperFlag::ExcludeSuper);
    TestNotNull(
        TEXT("TC-2: UPersistentServicesSubsystem 应有 LoadGameFromSlot 挂载点（AC-3 §21 Save 框架归属本类）"),
        LoadFn);

    // AC-1 §22 Audio 挂载点存在性（非 UFUNCTION，用编译期成员函数指针证存在——
    // 若方法被删/改名则此处取址无法编译，编译期捕获，比运行期 FindFunctionByName 更强）
    void (UPersistentServicesSubsystem::*AudioInitFn)()   = &UPersistentServicesSubsystem::InitializeAudioService;
    void (UPersistentServicesSubsystem::*AudioDeinitFn)() = &UPersistentServicesSubsystem::DeinitializeAudioService;
    TestTrue(
        TEXT("TC-2: Audio 挂载点 InitializeAudioService/DeinitializeAudioService 应存在（AC-1 §22 Audio 第三类，编译期取址）"),
        AudioInitFn != nullptr && AudioDeinitFn != nullptr);

    return true;
}

// =============================================================================
// TC-3（AC-4）— 反射断言：宿主无 per-match 对局态 UPROPERTY 字段
// 验证路径：TFieldIterator<FProperty>（solid headless，无需实例化）
// 非 vacuous 保证：
//   宿主声明了若干 UFUNCTION 方法（存在真对手）；若误加 UPROPERTY 字段
//   （如 UPROPERTY() int32 Cash）则 OwnPropertyCount != 0，TestEqual FAIL。
//   黑名单扫描（Cash/house_count 等）是双重保险：计数断言已足够兜底，黑名单提升错误诊断可读性。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPersistentServicesSubsystem_TC3_NoPerMatchStateFields,
    "Rento.Foundation.GameInstanceSubsystemHost.TC3_Reflection_NoPerMatchStateFields",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersistentServicesSubsystem_TC3_NoPerMatchStateFields::RunTest(const FString& Parameters)
{
    const UClass* HostClass = UPersistentServicesSubsystem::StaticClass();

    // per-match 对局态字段黑名单（AC-4 明确禁止；story-004 §AC-4 枚举）
    // Cash / house_count / owner map / 回合 phase 等写字段
    const TArray<FString> ForbiddenFieldNames = {
        TEXT("Cash"),
        TEXT("HouseCo"),       // house_count 前缀（大小写不敏感）
        TEXT("house_count"),
        TEXT("HouseCount"),
        TEXT("OwnerMap"),
        TEXT("owner_map"),
        TEXT("PlayerCash"),
        TEXT("TileOwner"),
        TEXT("OwnerId"),
        TEXT("TurnPhase"),
        TEXT("CurrentPhase"),
        TEXT("ETurnPhase"),
        TEXT("ActivePlayerIndex"),
        TEXT("RollResult"),
    };

    // 遍历宿主本类声明的 UPROPERTY（EFieldIterationFlags::None = 不含父类字段）
    int32 OwnPropertyCount = 0;
    for (TFieldIterator<FProperty> PropIt(HostClass, EFieldIterationFlags::None); PropIt; ++PropIt)
    {
        const FProperty* Prop = *PropIt;
        const FString PropName = Prop->GetName();
        ++OwnPropertyCount;

        // 黑名单检查（诊断用，计数断言为主要门禁）
        for (const FString& Forbidden : ForbiddenFieldNames)
        {
            TestFalse(
                FString::Printf(
                    TEXT("TC-3/AC-4: 跨局宿主不应有 per-match 对局态字段 '%s'（发现了 '%s'）"),
                    *Forbidden, *PropName),
                PropName.Equals(Forbidden, ESearchCase::IgnoreCase));
        }
    }

    // 主断言：宿主本类 UPROPERTY 总数应为 0（纯挂载点框架，无数据字段）
    // 这是非 vacuous 的硬门：若误加任何 UPROPERTY 字段则 OwnPropertyCount > 0，FAIL。
    // 注：UFUNCTION 不计入 FProperty 迭代，方法 stub 不影响此断言。
    TestEqual(
        TEXT("TC-3/AC-4: UPersistentServicesSubsystem 本类应无任何 UPROPERTY 数据字段（纯挂载点框架，ADR-0001 Forbidden: 禁 per-match 对局态）"),
        OwnPropertyCount, 0);

    return true;
}

// =============================================================================
// Edge — 宿主类不为 Abstract（GameInstanceSubsystem 可被引擎自动创建）
// AC 覆盖：AC-1 结构要求（宿主框架存在 + 挂载点可达）
// 补充验证：UGameInstanceSubsystem 不需要 ShouldCreateSubsystem override 就会自动创建，
//           但要求宿主类不为 Abstract（否则引擎跳过创建，GetSubsystem 返回 nullptr）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPersistentServicesSubsystem_Edge_NotAbstract,
    "Rento.Foundation.GameInstanceSubsystemHost.Edge_ClassFlags_NotAbstract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersistentServicesSubsystem_Edge_NotAbstract::RunTest(const FString& Parameters)
{
    const UClass* HostClass = UPersistentServicesSubsystem::StaticClass();

    // THEN：宿主类不为 Abstract（Abstract UCLASS 不能被实例化，引擎自动创建 Subsystem 会跳过）
    // UMatchSubsystemBase 是 Abstract；UPersistentServicesSubsystem 是具体类（非 Abstract）
    const bool bIsAbstract = HostClass->HasAnyClassFlags(CLASS_Abstract);
    TestFalse(
        TEXT("Edge: UPersistentServicesSubsystem 不应为 Abstract UCLASS（否则 GetSubsystem 返回 nullptr）"),
        bIsAbstract);

    return true;
}

// =============================================================================
// Edge — 宿主框架与 per-match 宿主无继承关系（两类宿主平行，ADR-0001 分层）
// AC 覆盖：AC-4（跨局宿主不承 per-match 对局态，继承关系层面验证）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPersistentServicesSubsystem_Edge_NoInheritanceFromPerMatchBase,
    "Rento.Foundation.GameInstanceSubsystemHost.Edge_ClassHierarchy_NoInheritanceFromMatchSubsystemBase",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersistentServicesSubsystem_Edge_NoInheritanceFromPerMatchBase::RunTest(const FString& Parameters)
{
    const UClass* HostClass = UPersistentServicesSubsystem::StaticClass();

    // UMatchSubsystemBase 是 per-match 宿主基类（UWorldSubsystem）
    // UPersistentServicesSubsystem 是跨局宿主（UGameInstanceSubsystem）
    // 两者应无继承关系——继承会破坏生命周期分层（ADR-0001 两类宿主并存约束）
    const UClass* PerMatchBaseClass = UMatchSubsystemBase::StaticClass();
    TestFalse(
        TEXT("Edge: UPersistentServicesSubsystem 不应继承 UMatchSubsystemBase（两类宿主平行，ADR-0001 生命周期分层）"),
        HostClass->IsChildOf(PerMatchBaseClass));

    return true;
}

// =============================================================================
// DEFER 注释块（ff-007 解锁后补测）
// ※ 下方编号为 story-004 QA Test Case 编号，与本文件代码 TC-1~3（结构反射）不同。
//
// 以下 story-004 QA TC 需真实 PIE 生命周期或真 DA_Board 资产（bd-007）：
//
// story-004 QA TC-2（AC-2：StartNewGame 跨局存活）— DEFER ff-007：
//   GIVEN PIE Start→Stop→Start，WHEN 第一局调 StartNewGame(config) 后 Stop 再 Start，
//   THEN 入口仍可达（跨局存活）。
//   需要：UGameInstance::Init() 完整栈 + PIE Stop/Start 生命周期驱动。
//   headless -nullrhi 不可驱动：GEngine/GWorld 状态不完整，GetSubsystem 不可靠。
//
// story-004 QA TC-1/TC-3（AC-1/AC-6：PIE 单例性）— DEFER ff-007：
//   GIVEN PIE Start→Stop→Start 三局，THEN 宿主实例指针三局恒同一（进程单例）。
//   需要：真实 PIE 三局 + UGameInstance 单例性运行时验证。
//
// story-004 QA TC-5（AC-5：DA_Board 跨局缓存）— DEFER ff-007 + bd-007：
//   GIVEN 连续两局加载同一 DA_Board，THEN 底层资产仅加载一次（无第二次磁盘 IO）。
//   需要：真 DA_Board 资产（bd-007 实现后）+ 跨局 PIE 加载验证。
//
// tech-debt 登记：production/epics/foundation-framework/story-004-game-instance-subsystem-host.md
// =============================================================================
