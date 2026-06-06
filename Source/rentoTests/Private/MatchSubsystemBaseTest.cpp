// MatchSubsystemBaseTest.cpp
// =============================================================================
// 集成测试：UMatchSubsystemBase per-match 宿主基类与生命周期
// story-001 AC-1~6 covered；
// OnWorldBeginPlay 运行时触发验证 defer FF-007（PIE 实测需完整 BeginPlay 调用链，
// headless 下 CreateWorld 不自动触发 BeginPlay，留 story-007 验证）。
//
// 物理路径：Source/rentoTests/Private/MatchSubsystemBaseTest.cpp
// 逻辑分类：tests/integration/foundation/match_subsystem_base_test.cpp
// Automation 类目：Rento.Foundation.MatchSubsystemBase
//
// 测试机制说明（headless `-nullrhi` 验证策略）：
//
// TC-1/TC-1b/TC-1c（ShouldCreateSubsystem 过滤）：
//   直接对 CDO（Class Default Object）调用 ShouldCreateSubsystem(World)，
//   传入不同 WorldType 的 UWorld* 作为 Outer，验证白名单过滤语义。
//   使用 UWorld::CreateWorld + DestroyWorld 创建临时测试 World。
//   无需 PIE 启动，headless 下完全可跑。
//
// TC-2（继承链断言）：
//   UClass::IsChildOf 编译期反射断言，无运行时依赖。
//   UGameInstanceSubsystem 通过 #include + StaticClass() 硬引用，避免 FindObject 静默跳过。
//
// TC-3 / TC-3b（Initialize/Deinitialize 生命周期计数）：
//   使用 UWorld::CreateWorld(EWorldType::Game) 创建真实 Game World。
//   UWorld::CreateWorld 内部调用 InitializeNewWorld → InitializeSubsystems，
//   引擎自动发现并 Initialize 所有注册的非 Abstract WorldSubsystem（含测试桩）。
//   TC-3 在 DestroyWorld 前读取实例级计数器（InitializeCount）。
//   TC-3b 用静态计数器（GTestInitCount/GTestDeinitCount），DestroyWorld 后仍可读取。
//   DestroyWorld 内部调用 CleanupWorld → SubsystemCollection::Deinitialize。
//
//   断言非 vacuous 保证：
//   - 若 ShouldCreateSubsystem 返回 false → GetSubsystem 返回 nullptr → TestNotNull FAIL
//   - 若 Initialize 未被调用 → 计数 0 → TestEqual(0, 1) FAIL
//   - 若 Deinitialize 未被调用 → 静态计数 0 → TestEqual(0, 1) FAIL
//
// TC-4（Reflection 断言）：
//   TFieldIterator<FProperty> 遍历 UMatchSubsystemBase 本类 UPROPERTY，
//   断言无游戏状态写字段，且本类 UPROPERTY 总数为 0（纯骨架）。
//
// TC-6（AC-6 断言）：
//   静态反射断言 UMatchSubsystemBase 不继承 UActorComponent（ADR-0001 Forbidden）。
//
// Edge_NonGameWorld：
//   EditorPreview World → ShouldCreateSubsystem false → Subsystem 不创建 → GetSubsystem == nullptr。
//
// Edge_TwoConsecutiveMatches（真增量断言）：
//   函数顶部一次性重置；match1 destroy 后静态计数为 (1,1)；
//   match2 create 后静态计数为 (2,1)（InitCount 增量恰 1，非实例复用第一局残留）；
//   match2 destroy 后静态计数为 (2,2)。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"   // TC-2: 硬引用消除 FindObject 静默跳过
#include "Components/ActorComponent.h"           // TC-6: AC-6 断言
#include "MatchSubsystemBase.h"
#include "TestMatchSubsystem.h"   // 测试桩：UTestMatchSubsystem / UTestMatchSubsystemCounter

// =============================================================================
// 辅助：创建 / 销毁测试 World（不通知 Engine，headless 安全）
// =============================================================================
namespace MatchSubsystemTestHelpers
{
	/**
	 * 创建指定 WorldType 的最小测试 World。
	 * bInformEngineOfWorld=false：不依赖 GEngine，headless 环境安全。
	 * 调用方负责 World->DestroyWorld(false) 清理。
	 */
	static UWorld* CreateTestWorld(EWorldType::Type WorldType, const FName& WorldName)
	{
		UWorld* World = UWorld::CreateWorld(WorldType, /*bInformEngineOfWorld=*/false, WorldName);
		check(World);
		return World;
	}

	/** 销毁测试 World，释放资源，防止 PIE 残留 */
	static void DestroyTestWorld(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		}
	}
}

// =============================================================================
// TC-1 — ShouldCreateSubsystem：Game World → true
// AC 覆盖：AC-1（基类及四 override 存在）、AC-2（Game World → true）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC1_GameWorldReturnsTrue,
	"Rento.Foundation.MatchSubsystemBase.TC1_ShouldCreate_GameWorld_ReturnsTrue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC1_GameWorldReturnsTrue::RunTest(const FString& Parameters)
{
	// GIVEN：一个 EWorldType::Game 的测试 World
	UWorld* GameWorld = MatchSubsystemTestHelpers::CreateTestWorld(EWorldType::Game, TEXT("TC1_GameWorld"));
	if (!TestNotNull(TEXT("TC-1: 应能创建 Game World"), GameWorld)) { return false; }

	// WHEN：用 CDO 调用 ShouldCreateSubsystem（与引擎自动创建时的调用路径一致）
	const UTestMatchSubsystem* CDO = GetDefault<UTestMatchSubsystem>();
	const bool bShouldCreate = CDO->ShouldCreateSubsystem(GameWorld);

	// THEN：Game World 应返回 true（ADR-0001 §1 白名单：Game + PIE）
	TestTrue(TEXT("TC-1: EWorldType::Game 应使 ShouldCreateSubsystem 返回 true"), bShouldCreate);

	MatchSubsystemTestHelpers::DestroyTestWorld(GameWorld);
	return true;
}

// =============================================================================
// TC-1b — ShouldCreateSubsystem：EditorPreview World → false
// AC 覆盖：AC-2（editor-preview World → false，ADR-0001 §1 排除）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC1b_EditorPreviewReturnsFalse,
	"Rento.Foundation.MatchSubsystemBase.TC1b_ShouldCreate_EditorPreviewWorld_ReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC1b_EditorPreviewReturnsFalse::RunTest(const FString& Parameters)
{
	// GIVEN：一个 EWorldType::EditorPreview 的测试 World
	UWorld* PreviewWorld = MatchSubsystemTestHelpers::CreateTestWorld(EWorldType::EditorPreview, TEXT("TC1b_PreviewWorld"));
	if (!TestNotNull(TEXT("TC-1b: 应能创建 EditorPreview World"), PreviewWorld)) { return false; }

	// WHEN：查询 ShouldCreateSubsystem
	const UTestMatchSubsystem* CDO = GetDefault<UTestMatchSubsystem>();
	const bool bShouldCreate = CDO->ShouldCreateSubsystem(PreviewWorld);

	// THEN：EditorPreview World 应返回 false
	TestFalse(TEXT("TC-1b: EWorldType::EditorPreview 应使 ShouldCreateSubsystem 返回 false"), bShouldCreate);

	MatchSubsystemTestHelpers::DestroyTestWorld(PreviewWorld);
	return true;
}

// =============================================================================
// TC-1c — ShouldCreateSubsystem：Editor World → false
// AC 覆盖：AC-2（引擎基类默认允许 Editor，本重写收紧排除，ADR-0001 §1）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC1c_EditorWorldReturnsFalse,
	"Rento.Foundation.MatchSubsystemBase.TC1c_ShouldCreate_EditorWorld_ReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC1c_EditorWorldReturnsFalse::RunTest(const FString& Parameters)
{
	// GIVEN：一个 EWorldType::Editor 的测试 World
	UWorld* EditorWorld = MatchSubsystemTestHelpers::CreateTestWorld(EWorldType::Editor, TEXT("TC1c_EditorWorld"));
	if (!TestNotNull(TEXT("TC-1c: 应能创建 Editor World"), EditorWorld)) { return false; }

	// WHEN：查询 ShouldCreateSubsystem
	const UTestMatchSubsystem* CDO = GetDefault<UTestMatchSubsystem>();
	const bool bShouldCreate = CDO->ShouldCreateSubsystem(EditorWorld);

	// THEN：Editor World 应返回 false
	// （引擎基类 DoesSupportWorldType 默认允许 Editor，
	//  UMatchSubsystemBase::ShouldCreateSubsystem 白名单收紧为 Game+PIE，排除 Editor）
	TestFalse(TEXT("TC-1c: EWorldType::Editor 应使 ShouldCreateSubsystem 返回 false（ADR-0001 §1 收紧）"), bShouldCreate);

	MatchSubsystemTestHelpers::DestroyTestWorld(EditorWorld);
	return true;
}

// =============================================================================
// TC-2 — 继承链：UMatchSubsystemBase 继承 UWorldSubsystem，不继承 UGameInstanceSubsystem
// AC 覆盖：AC-3（per-match 服务继承 UWorldSubsystem，ADR-0001 Forbidden 不挂 GameInstanceSubsystem）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC2_InheritsWorldSubsystem,
	"Rento.Foundation.MatchSubsystemBase.TC2_ClassHierarchy_InheritsUWorldSubsystem",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC2_InheritsWorldSubsystem::RunTest(const FString& Parameters)
{
	const UClass* BaseClass = UMatchSubsystemBase::StaticClass();

	// THEN-1：继承链包含 UWorldSubsystem（AC-3）
	TestTrue(
		TEXT("TC-2: UMatchSubsystemBase 应继承 UWorldSubsystem（AC-3）"),
		BaseClass->IsChildOf(UWorldSubsystem::StaticClass()));

	// THEN-2：不继承 UGameInstanceSubsystem（ADR-0001 Forbidden）
	// 使用编译期 UGameInstanceSubsystem::StaticClass() 硬引用，
	// 消除原来 FindObject 返回 null 时的静默跳过假绿问题。
	TestFalse(
		TEXT("TC-2: UMatchSubsystemBase 不应继承 UGameInstanceSubsystem（ADR-0001 Forbidden）"),
		BaseClass->IsChildOf(UGameInstanceSubsystem::StaticClass()));

	return true;
}

// =============================================================================
// TC-3 — Initialize 计数（实例级，在 DestroyWorld 前读取）
// AC 覆盖：AC-4（Game World 触发 Initialize 恰 1 次）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC3_InitializeCalledOnce,
	"Rento.Foundation.MatchSubsystemBase.TC3_Lifecycle_InitializeCalledOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC3_InitializeCalledOnce::RunTest(const FString& Parameters)
{
	// GIVEN：创建 Game World
	// CreateWorld 内部调用 InitializeNewWorld → InitializeSubsystems，
	// 引擎自动创建并 Initialize 所有注册的非 Abstract WorldSubsystem（含 UTestMatchSubsystem）
	UWorld* GameWorld = UWorld::CreateWorld(
		EWorldType::Game,
		/*bInformEngineOfWorld=*/false,
		TEXT("TC3_InitWorld"));

	if (!TestNotNull(TEXT("TC-3: 应能创建 Game World"), GameWorld))
	{
		return false;
	}

	// WHEN：获取 Subsystem 实例（存在 → ShouldCreateSubsystem 已返回 true + Initialize 已调用）
	UTestMatchSubsystem* Subsystem = GameWorld->GetSubsystem<UTestMatchSubsystem>();
	if (!TestNotNull(TEXT("TC-3: Game World 应自动创建 UTestMatchSubsystem（ShouldCreateSubsystem=true）"), Subsystem))
	{
		MatchSubsystemTestHelpers::DestroyTestWorld(GameWorld);
		return false;
	}

	// THEN：Initialize 被调用恰 1 次（AC-4）
	// 若 Initialize 未被调用，InitializeCount == 0，此处 FAIL（非 vacuous）
	TestEqual(TEXT("TC-3: Initialize 应被调用恰 1 次（AC-4）"), Subsystem->InitializeCount, 1);

	MatchSubsystemTestHelpers::DestroyTestWorld(GameWorld);
	return true;
}

// =============================================================================
// TC-3b — Deinitialize 计数（静态计数器，DestroyWorld 后仍可读）
// AC 覆盖：AC-4（Game World 销毁触发 Deinitialize 恰 1 次）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC3b_DeinitializeCalledOnce,
	"Rento.Foundation.MatchSubsystemBase.TC3b_Lifecycle_DeinitializeCalledOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC3b_DeinitializeCalledOnce::RunTest(const FString& Parameters)
{
	// 重置静态计数器（测试隔离）
	GTestInitCount   = 0;
	GTestDeinitCount = 0;

	// GIVEN：创建 Game World（触发 UTestMatchSubsystemCounter::Initialize → GTestInitCount++）
	UWorld* GameWorld = UWorld::CreateWorld(
		EWorldType::Game,
		/*bInformEngineOfWorld=*/false,
		TEXT("TC3b_DeinitWorld"));

	if (!TestNotNull(TEXT("TC-3b: 应能创建 Game World"), GameWorld))
	{
		return false;
	}

	// 确认 Subsystem 已创建（ShouldCreateSubsystem 通过）
	const UTestMatchSubsystemCounter* Subsystem = GameWorld->GetSubsystem<UTestMatchSubsystemCounter>();
	if (!TestNotNull(TEXT("TC-3b: Game World 应自动创建 UTestMatchSubsystemCounter"), Subsystem))
	{
		MatchSubsystemTestHelpers::DestroyTestWorld(GameWorld);
		return false;
	}

	// THEN（销毁前）：Initialize 已调用 1 次，Deinitialize 尚未调用
	TestEqual(TEXT("TC-3b: 销毁前 Initialize 静态计数应为 1"), GTestInitCount, 1);
	TestEqual(TEXT("TC-3b: 销毁前 Deinitialize 静态计数应为 0"), GTestDeinitCount, 0);

	// WHEN：销毁 World → CleanupWorld → SubsystemCollection::Deinitialize
	GameWorld->DestroyWorld(/*bInformEngineOfWorld=*/false);
	GameWorld = nullptr;
	// 注意：Subsystem 指针在 DestroyWorld 后失效，不再访问（通过静态计数器读结果）

	// THEN（销毁后）：Deinitialize 被调用恰 1 次（AC-4）
	// 若 Deinitialize 未被调用，GTestDeinitCount == 0，此处 FAIL（非 vacuous）
	TestEqual(TEXT("TC-3b: Deinitialize 应被调用恰 1 次（AC-4）"), GTestDeinitCount, 1);
	TestEqual(TEXT("TC-3b: Initialize 计数应仍为 1（无多余调用）"), GTestInitCount, 1);

	return true;
}

// =============================================================================
// TC-4 — Reflection 断言：基类无游戏状态写字段
// AC 覆盖：AC-5（基类不声明 Cash/house_count/owner map 等游戏状态写字段）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC4_NoGameStateFields,
	"Rento.Foundation.MatchSubsystemBase.TC4_Reflection_NoGameStateWriteFields",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC4_NoGameStateFields::RunTest(const FString& Parameters)
{
	const UClass* BaseClass = UMatchSubsystemBase::StaticClass();

	// 游戏状态写字段黑名单（AC-5 明确禁止；大小写不敏感匹配）
	// 这些是 GDD 中经济 / 地产 / owner 系统的权威写字段
	const TArray<FString> ForbiddenFieldNames = {
		TEXT("Cash"),
		TEXT("house_count"),
		TEXT("HouseCount"),
		TEXT("OwnerMap"),
		TEXT("owner_map"),
		TEXT("PlayerCash"),
		TEXT("TileOwner"),
		TEXT("OwnerId"),
	};

	// 遍历 UMatchSubsystemBase 本类声明的 UPROPERTY（EFieldIterationFlags::None = 不含父类）
	for (TFieldIterator<FProperty> PropIt(BaseClass, EFieldIterationFlags::None); PropIt; ++PropIt)
	{
		const FProperty* Prop = *PropIt;
		const FString PropName = Prop->GetName();

		for (const FString& Forbidden : ForbiddenFieldNames)
		{
			TestFalse(
				FString::Printf(TEXT("AC-5: 基类不应有游戏状态写字段 '%s'（发现了 '%s'）"),
					*Forbidden, *PropName),
				PropName.Equals(Forbidden, ESearchCase::IgnoreCase));
		}
	}

	// 补充断言：基类本类 UPROPERTY 总数应为 0（纯骨架基类，无任何数据成员）
	int32 OwnPropertyCount = 0;
	for (TFieldIterator<FProperty> PropIt(BaseClass, EFieldIterationFlags::None); PropIt; ++PropIt)
	{
		++OwnPropertyCount;
	}
	TestEqual(
		TEXT("AC-5: UMatchSubsystemBase 应无任何 UPROPERTY 数据成员（纯骨架，AC-5 结构断言）"),
		OwnPropertyCount, 0);

	return true;
}

// =============================================================================
// Edge — 非 Game World（EditorPreview）不触发 Initialize
// AC 覆盖：AC-2 边界（ShouldCreateSubsystem=false → Subsystem 不存在）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_Edge_NonGameWorldNoSubsystem,
	"Rento.Foundation.MatchSubsystemBase.Edge_NonGameWorld_SubsystemNotCreated",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_Edge_NonGameWorldNoSubsystem::RunTest(const FString& Parameters)
{
	// GIVEN：EditorPreview World（ShouldCreateSubsystem 应返回 false）
	UWorld* PreviewWorld = UWorld::CreateWorld(
		EWorldType::EditorPreview,
		/*bInformEngineOfWorld=*/false,
		TEXT("Edge_PreviewWorld"));

	if (!TestNotNull(TEXT("Edge: 应能创建 EditorPreview World"), PreviewWorld)) { return false; }

	// WHEN：InitializeSubsystems 已在 CreateWorld 内部调用
	// THEN：ShouldCreateSubsystem=false → 引擎不创建 UTestMatchSubsystem
	const UTestMatchSubsystem* Subsystem = PreviewWorld->GetSubsystem<UTestMatchSubsystem>();
	TestNull(
		TEXT("Edge: EditorPreview World 中 UTestMatchSubsystem 不应被创建（ShouldCreateSubsystem=false）"),
		Subsystem);

	MatchSubsystemTestHelpers::DestroyTestWorld(PreviewWorld);
	return true;
}

// =============================================================================
// Edge — 连续两局：真增量断言证无跨局残留
// AC 覆盖：AC-4 边界（per-match Subsystem 随局生灭，无跨局单例）
//
// 断言策略（真增量，非重置后恒 1）：
//   函数顶部一次性重置（隔离其他测试）；match1/match2 之间不重置。
//   match1 create → GTestInitCount==1
//   match1 destroy → GTestDeinitCount==1
//   match2 create → GTestInitCount==2（增量恰 1，证明无实例复用/单例残留）
//   match2 destroy → GTestDeinitCount==2
//
//   若引擎复用了 match1 的 Subsystem 实例（错误），则 match2 create 时
//   Initialize 不会再被调用，GTestInitCount 仍为 1，TestEqual(GTestInitCount, 2) FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_Edge_TwoConsecutiveMatches,
	"Rento.Foundation.MatchSubsystemBase.Edge_TwoConsecutiveMatches_SecondMatchInitOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_Edge_TwoConsecutiveMatches::RunTest(const FString& Parameters)
{
	// 函数顶部一次性重置（隔离其他测试），match1/match2 之间不重置
	GTestInitCount   = 0;
	GTestDeinitCount = 0;

	// === 第一局 ===
	UWorld* World1 = UWorld::CreateWorld(
		EWorldType::Game,
		/*bInformEngineOfWorld=*/false,
		TEXT("Edge_Match1"));

	if (!TestNotNull(TEXT("Edge: 第一局 World 应创建成功"), World1)) { return false; }

	const UTestMatchSubsystemCounter* Sub1 = World1->GetSubsystem<UTestMatchSubsystemCounter>();
	if (!TestNotNull(TEXT("Edge: 第一局应创建 UTestMatchSubsystemCounter"), Sub1))
	{
		MatchSubsystemTestHelpers::DestroyTestWorld(World1);
		return false;
	}

	// match1 create 后：Init==1，Deinit==0
	TestEqual(TEXT("Edge: match1 create 后 InitCount==1"), GTestInitCount, 1);
	TestEqual(TEXT("Edge: match1 create 后 DeinitCount==0"), GTestDeinitCount, 0);

	World1->DestroyWorld(/*bInformEngineOfWorld=*/false);
	World1 = nullptr;

	// match1 destroy 后：Init==1，Deinit==1
	TestEqual(TEXT("Edge: match1 destroy 后 InitCount==1"), GTestInitCount, 1);
	TestEqual(TEXT("Edge: match1 destroy 后 DeinitCount==1"), GTestDeinitCount, 1);

	// === 第二局（不重置计数器，验证真增量）===
	UWorld* World2 = UWorld::CreateWorld(
		EWorldType::Game,
		/*bInformEngineOfWorld=*/false,
		TEXT("Edge_Match2"));

	if (!TestNotNull(TEXT("Edge: 第二局 World 应创建成功"), World2)) { return false; }

	const UTestMatchSubsystemCounter* Sub2 = World2->GetSubsystem<UTestMatchSubsystemCounter>();
	if (!TestNotNull(TEXT("Edge: 第二局应创建 UTestMatchSubsystemCounter（无跨局单例残留）"), Sub2))
	{
		MatchSubsystemTestHelpers::DestroyTestWorld(World2);
		return false;
	}

	// match2 create 后：Init==2（增量恰 1，证明 match2 获得新实例，非实例复用）
	// 若引擎错误复用 match1 实例，Initialize 不再调用，此处 FAIL（非 vacuous）
	TestEqual(TEXT("Edge: match2 create 后 InitCount==2（增量恰 1，无跨局残留）"), GTestInitCount, 2);
	TestEqual(TEXT("Edge: match2 create 后 DeinitCount==1（match1 残留已清，match2 尚存活）"), GTestDeinitCount, 1);

	World2->DestroyWorld(/*bInformEngineOfWorld=*/false);
	World2 = nullptr;

	// match2 destroy 后：Init==2，Deinit==2
	TestEqual(TEXT("Edge: match2 destroy 后 InitCount==2"), GTestInitCount, 2);
	TestEqual(TEXT("Edge: match2 destroy 后 DeinitCount==2"), GTestDeinitCount, 2);

	return true;
}

// =============================================================================
// TC-6 — AC-6 断言：UMatchSubsystemBase 不使用 UActorComponent 形态
// AC 覆盖：AC-6（ADR-0001 Forbidden: 禁 UActorComponent 形态承载对局服务）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMatchSubsystemBase_TC6_NotActorComponent,
	"Rento.Foundation.MatchSubsystemBase.TC6_ClassHierarchy_NotActorComponent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMatchSubsystemBase_TC6_NotActorComponent::RunTest(const FString& Parameters)
{
	const UClass* BaseClass = UMatchSubsystemBase::StaticClass();

	// THEN：不继承 UActorComponent（ADR-0001 Forbidden：禁 UActorComponent 形态）
	// 使用编译期 UActorComponent::StaticClass() 硬引用，无静默跳过风险。
	TestFalse(
		TEXT("TC-6: UMatchSubsystemBase 不应继承 UActorComponent（AC-6 / ADR-0001 Forbidden）"),
		BaseClass->IsChildOf(UActorComponent::StaticClass()));

	return true;
}
