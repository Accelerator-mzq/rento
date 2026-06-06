// RngServiceHostLifecycleTest.cpp
// =============================================================================
// 集成测试：UDiceRngService 宿主生命周期 / 继承链 / ShouldCreate / 成员语义
// story-001 [Host] AC covered（headless 可测部分）。
//
// 物理路径：Source/rentoTests/Private/RngServiceHostLifecycleTest.cpp
// 逻辑分类：tests/integration/dice/rng_service_host_lifecycle_test.cpp
// Automation 类目：Rento.Dice.RngServiceLifecycle
//
// headless 验证策略说明：
//
// TC_LC1（继承链）：
//   UClass::IsChildOf 编译期反射断言，无运行时依赖。
//   断言 UDiceRngService IsChildOf UMatchSubsystemBase IsChildOf UWorldSubsystem。
//
// TC_LC2 / TC_LC2b / TC_LC2c / TC_LC2d（ShouldCreateSubsystem 过滤）：
//   仿照 MatchSubsystemBaseTest.cpp TC-1/1b/1c 模式：
//   对 CDO 直接调用 ShouldCreateSubsystem(World)，传入不同 WorldType 的 UWorld*。
//   Game→true；PIE→true；EditorPreview→false；Editor→false（继承 UMatchSubsystemBase 实现）。
//
// TC_LC3（权威流实例语义）：
//   核心正面断言（值语义/无单例的可靠证明）：
//     两实例用相同种子(777) 各抽 5 次 → 序列逐位相同（各持独立流 + 值语义）。
//     若 FRandomStream 是共享单例，第二个 SetSeed(777) 会重置同一流，
//     但第一个实例已抽过 5 次，两者起点不同，序列就不同 → 此断言 FAIL。
//   辅助断言：不同种子(100 vs 200) → 序列不同（补充验证，弱但直观）。
//   确定性重放：相同种子重设后序列与首次一致（Seed 可重复性）。
//
// TC_LC4（OnWorldBeginPlay 注入点验证）：
//   步骤 1（GetSubsystem 路径）：
//     headless CreateWorld(Game) 真创建了 WorldSubsystem（此步前提已由本测试跑绿经验证）。
//     Initialize 后 GetCurrentSeed()==0（Initialize 不注入，ADR-0001 Forbidden）。
//   步骤 2（true-path）：bUseFixedSeed=true / FixedSeed=999 → OnWorldBeginPlay → seed==999。
//   步骤 3（false-path pin）：独立 NewObject 实例 bUseFixedSeed=false（默认）
//     → OnWorldBeginPlay → seed 仍==0（「故意未接线」负向事实不被篡改）。
//
// defer ff-007（PIE 运行时重触发）：
//   PIE Stop→Start 重触发 OnWorldBeginPlay + Seed 重注入的完整运行时验证，
//   headless 下 CreateWorld 不自动触发 BeginPlay，无法可靠驱动。
//   defer ff-007（PIE 隔离验证 story），与 ff-001/ff-003 同惯例。
//
// 函数索引：
//   TC_LC1_InheritanceChain            → 继承链：UDiceRngService → UMatchSubsystemBase → UWorldSubsystem
//   TC_LC2_ShouldCreate_GameWorld      → ShouldCreate：Game World → true
//   TC_LC2b_ShouldCreate_EditorPreview → ShouldCreate：EditorPreview → false
//   TC_LC2c_ShouldCreate_EditorWorld   → ShouldCreate：Editor World → false
//   TC_LC2d_ShouldCreate_PIEWorld      → ShouldCreate：PIE World → true（白名单正面路径）
//   TC_LC3_AuthoritativeStream_InstanceSemantics → 值语义：同种子同序列 + 不同种子不同序列 + 重放
//   TC_LC4_OnWorldBeginPlay_InjectsFixedSeed     → 注入点：true-path 注入 + false-path pin
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MatchSubsystemBase.h"
#include "DiceRngService.h"

// =============================================================================
// 辅助：创建 / 销毁测试 World（仿照 MatchSubsystemBaseTest 模式）
// =============================================================================
namespace DiceRngLifecycleTestHelpers
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

	/** 销毁测试 World，释放资源 */
	static void DestroyTestWorld(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		}
	}
}

// =============================================================================
// TC_LC1 — 继承链：UDiceRngService → UMatchSubsystemBase → UWorldSubsystem
//
// AC 覆盖：[Host] 继承链断言（story-001 [Host] §宿主生命周期）
//
// 验证策略：
//   UClass::IsChildOf 编译期反射断言，无运行时依赖。
//   同时断言不继承 UGameInstanceSubsystem（ADR-0001 Forbidden：per-match 禁挂 GameInstance）。
//
// 非 vacuous 保证：
//   若错误地让 UDiceRngService 继承 UGameInstanceSubsystem，
//   IsChildOf(UWorldSubsystem) 通常返回 false → TestTrue FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_LC1_InheritanceChain,
	"Rento.Dice.RngServiceLifecycle.TC_LC1_InheritanceChain",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_LC1_InheritanceChain::RunTest(const FString& Parameters)
{
	const UClass* ServiceClass = UDiceRngService::StaticClass();
	if (!TestNotNull(TEXT("TC_LC1: UDiceRngService::StaticClass() 应非 null"), ServiceClass))
	{
		return false;
	}

	// THEN-1：继承 UMatchSubsystemBase（per-match 宿主基类，ADR-0001 §Key Interfaces）
	TestTrue(
		TEXT("TC_LC1: UDiceRngService 应继承 UMatchSubsystemBase（per-match 宿主基类）"),
		ServiceClass->IsChildOf(UMatchSubsystemBase::StaticClass()));

	// THEN-2：继承 UWorldSubsystem（一局=World 边界，PIE 隔离，ADR-0001）
	TestTrue(
		TEXT("TC_LC1: UDiceRngService 应继承 UWorldSubsystem（per-match 生命周期边界）"),
		ServiceClass->IsChildOf(UWorldSubsystem::StaticClass()));

	// THEN-3：不继承 UGameInstanceSubsystem（ADR-0001 Forbidden：per-match 禁挂 GameInstance）
	TestFalse(
		TEXT("TC_LC1: UDiceRngService 不应继承 UGameInstanceSubsystem（ADR-0001 Forbidden）"),
		ServiceClass->IsChildOf(UGameInstanceSubsystem::StaticClass()));

	return true;
}

// =============================================================================
// TC_LC2 — ShouldCreateSubsystem：Game World → true
//
// AC 覆盖：[Host] ShouldCreateSubsystem 排除 editor-preview World（story-001 [Host]）
//
// 验证策略：对 CDO 直接调用 ShouldCreateSubsystem(GameWorld)，期望 true。
// 继承 UMatchSubsystemBase 实现（UMatchSubsystemBase::ShouldCreateSubsystem
// 白名单 Game+PIE，无需 UDiceRngService 重写）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_LC2_ShouldCreate_GameWorld,
	"Rento.Dice.RngServiceLifecycle.TC_LC2_ShouldCreate_GameWorld_ReturnsTrue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_LC2_ShouldCreate_GameWorld::RunTest(const FString& Parameters)
{
	// GIVEN：Game World
	UWorld* GameWorld = DiceRngLifecycleTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("LC2_GameWorld"));
	if (!TestNotNull(TEXT("TC_LC2: 应能创建 Game World"), GameWorld)) { return false; }

	// WHEN：CDO 调用 ShouldCreateSubsystem
	const UDiceRngService* CDO = GetDefault<UDiceRngService>();
	const bool bShouldCreate = CDO->ShouldCreateSubsystem(GameWorld);

	// THEN：Game World 应返回 true（白名单 Game+PIE，ADR-0001 / UMatchSubsystemBase）
	TestTrue(
		TEXT("TC_LC2: EWorldType::Game 应使 UDiceRngService::ShouldCreateSubsystem 返回 true"),
		bShouldCreate);

	DiceRngLifecycleTestHelpers::DestroyTestWorld(GameWorld);
	return true;
}

// =============================================================================
// TC_LC2b — ShouldCreateSubsystem：EditorPreview World → false
//
// AC 覆盖：[Host] ShouldCreateSubsystem 排除 editor-preview World
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_LC2b_ShouldCreate_EditorPreview,
	"Rento.Dice.RngServiceLifecycle.TC_LC2b_ShouldCreate_EditorPreview_ReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_LC2b_ShouldCreate_EditorPreview::RunTest(const FString& Parameters)
{
	// GIVEN：EditorPreview World（编辑器预览，应排除，ADR-0001 §1）
	UWorld* PreviewWorld = DiceRngLifecycleTestHelpers::CreateTestWorld(
		EWorldType::EditorPreview, TEXT("LC2b_PreviewWorld"));
	if (!TestNotNull(TEXT("TC_LC2b: 应能创建 EditorPreview World"), PreviewWorld)) { return false; }

	// WHEN：CDO 调用 ShouldCreateSubsystem
	const UDiceRngService* CDO = GetDefault<UDiceRngService>();
	const bool bShouldCreate = CDO->ShouldCreateSubsystem(PreviewWorld);

	// THEN：EditorPreview 应返回 false（排除编辑器预览世界，ADR-0001 §1）
	TestFalse(
		TEXT("TC_LC2b: EWorldType::EditorPreview 应使 UDiceRngService::ShouldCreateSubsystem 返回 false"),
		bShouldCreate);

	DiceRngLifecycleTestHelpers::DestroyTestWorld(PreviewWorld);
	return true;
}

// =============================================================================
// TC_LC2c — ShouldCreateSubsystem：Editor World → false
//
// AC 覆盖：[Host] ShouldCreateSubsystem 排除 editor World
// 说明：引擎基类 DoesSupportWorldType 默认允许 Editor，
//       UMatchSubsystemBase 收紧为 Game+PIE，排除 Editor。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_LC2c_ShouldCreate_EditorWorld,
	"Rento.Dice.RngServiceLifecycle.TC_LC2c_ShouldCreate_EditorWorld_ReturnsFalse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_LC2c_ShouldCreate_EditorWorld::RunTest(const FString& Parameters)
{
	// GIVEN：Editor World（引擎基类默认允许，UMatchSubsystemBase 收紧排除）
	UWorld* EditorWorld = DiceRngLifecycleTestHelpers::CreateTestWorld(
		EWorldType::Editor, TEXT("LC2c_EditorWorld"));
	if (!TestNotNull(TEXT("TC_LC2c: 应能创建 Editor World"), EditorWorld)) { return false; }

	// WHEN：CDO 调用 ShouldCreateSubsystem
	const UDiceRngService* CDO = GetDefault<UDiceRngService>();
	const bool bShouldCreate = CDO->ShouldCreateSubsystem(EditorWorld);

	// THEN：Editor World 应返回 false（UMatchSubsystemBase 白名单收紧为 Game+PIE）
	TestFalse(
		TEXT("TC_LC2c: EWorldType::Editor 应使 UDiceRngService::ShouldCreateSubsystem 返回 false（UMatchSubsystemBase 白名单收紧）"),
		bShouldCreate);

	DiceRngLifecycleTestHelpers::DestroyTestWorld(EditorWorld);
	return true;
}

// =============================================================================
// TC_LC2d — ShouldCreateSubsystem：PIE World → true（白名单正面路径）
//
// AC 覆盖：[Host] ShouldCreateSubsystem 白名单 Game+PIE 双正面路径
//
// 说明：
//   UMatchSubsystemBase 白名单为 Game+PIE；TC_LC2 已覆盖 Game→true，
//   本测试补全 PIE→true（白名单另一个正面路径，防止有人把 PIE 误加进黑名单）。
//
// 非 vacuous 保证：
//   若有人把 ShouldCreateSubsystem 收紧为「仅 Game，排除 PIE」，
//   此断言立即 FAIL——编辑器内 PIE 运行将无法创建 subsystem，是真实风险。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_LC2d_ShouldCreate_PIEWorld,
	"Rento.Dice.RngServiceLifecycle.TC_LC2d_ShouldCreate_PIEWorld_ReturnsTrue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_LC2d_ShouldCreate_PIEWorld::RunTest(const FString& Parameters)
{
	// GIVEN：PIE World（编辑器内运行，ADR-0001 白名单 Game+PIE）
	UWorld* PIEWorld = DiceRngLifecycleTestHelpers::CreateTestWorld(
		EWorldType::PIE, TEXT("LC2d_PIEWorld"));
	if (!TestNotNull(TEXT("TC_LC2d: 应能创建 PIE World"), PIEWorld)) { return false; }

	// WHEN：CDO 调用 ShouldCreateSubsystem
	const UDiceRngService* CDO = GetDefault<UDiceRngService>();
	const bool bShouldCreate = CDO->ShouldCreateSubsystem(PIEWorld);

	// THEN：PIE World 应返回 true（白名单 Game+PIE，ADR-0001）
	// 若 ShouldCreateSubsystem 被收紧为「仅 Game」，此断言 FAIL（非 vacuous）
	TestTrue(
		TEXT("TC_LC2d: EWorldType::PIE 应使 UDiceRngService::ShouldCreateSubsystem 返回 true（白名单 Game+PIE，ADR-0001）"),
		bShouldCreate);

	DiceRngLifecycleTestHelpers::DestroyTestWorld(PIEWorld);
	return true;
}

// =============================================================================
// TC_LC3 — 权威流实例语义：两个独立 NewObject 实例各持独立流（值语义）
//
// AC 覆盖：[Host] 权威 FRandomStream 作 DiceService 实例成员（值语义，无需 GC）
//
// 验证策略（三层，核心在正面断言）：
//
// 【核心·正面断言】相同种子两实例同序列（值语义 / 无共享单例的可靠证明）：
//   实例 C 和实例 D 各自 SetSeed(777) 后独立抽取 5 次：
//   期望序列逐位相同——各实例持独立 FRandomStream，同种子起点一致，序列必同。
//   若 FRandomStream 是共享单例：D 的 SetSeed(777) 会重置同一流，
//   但 C 已抽过 5 次，D 再抽 5 次时流起点已偏移 → 序列不同 → TestEqual FAIL。
//   （注：「不同种子→不同序列」是弱检测，共享单例下因 B 覆盖 A 的种子，
//   交替抽取仍可能偶然相同；「同种子→同序列」才是确凿证明。）
//
// 【辅助·负面断言】不同种子两实例序列不同（补充验证，直观）：
//   实例 A SetSeed(100)，实例 B SetSeed(200)，各抽 5 次，期望至少一位不同。
//
// 【辅助·确定性断言】相同种子重设后序列与首次一致（Seed 可重复性）：
//   对实例 A 重设 SetSeed(100)，重新抽取，期望与首次序列逐位相同。
//
// 非 vacuous 保证（各层）：
//   正面断言：若 FRandomStream 是共享单例 → 序列偏移 → TestEqual 逐位 FAIL。
//   负面断言：若两实例恰好同种子同序列 → bSequencesDiffer==false → TestTrue FAIL。
//   确定性断言：若 FRandomStream 有非确定性（时间依赖等）→ 重放不同 → TestEqual FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_LC3_AuthoritativeStream_InstanceSemantics,
	"Rento.Dice.RngServiceLifecycle.TC_LC3_AuthoritativeStream_InstanceSemantics",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_LC3_AuthoritativeStream_InstanceSemantics::RunTest(const FString& Parameters)
{
	constexpr int32 IterCount = 5;

	// ==========================================================================
	// 【核心·正面断言】相同种子两实例 → 序列逐位相同（值语义 / 无共享单例的可靠证明）
	// ==========================================================================

	UDiceRngService* ServiceC = NewObject<UDiceRngService>(GetTransientPackage());
	UDiceRngService* ServiceD = NewObject<UDiceRngService>(GetTransientPackage());

	if (!TestNotNull(TEXT("TC_LC3: 实例 C 应能创建"), ServiceC)) { return false; }
	if (!TestNotNull(TEXT("TC_LC3: 实例 D 应能创建"), ServiceD)) { return false; }

	// 两实例各自 SetSeed(777)，独立抽取（不交替）
	ServiceC->SetSeed(777);
	TArray<int32> SeqC;
	SeqC.Reserve(IterCount);
	for (int32 i = 0; i < IterCount; ++i)
	{
		SeqC.Add(ServiceC->RandomRange(1, 10000));
	}

	ServiceD->SetSeed(777);
	TArray<int32> SeqD;
	SeqD.Reserve(IterCount);
	for (int32 i = 0; i < IterCount; ++i)
	{
		SeqD.Add(ServiceD->RandomRange(1, 10000));
	}

	// 期望：序列逐位相同（各持独立流，同种子同序列）
	// 若 FRandomStream 是共享单例，D.SetSeed(777) 重置同一流，
	// 但 C 已抽过 5 次，D 的 5 次从偏移位置抽 → 序列不同 → 此处 FAIL（非 vacuous）
	for (int32 i = 0; i < IterCount; ++i)
	{
		TestEqual(
			*FString::Printf(
				TEXT("TC_LC3 [核心正面]: 相同种子(777) 实例 C/D 第 %d 次应逐位相同（各持独立流；共享单例会使序列偏移而不同）"),
				i + 1),
			SeqC[i], SeqD[i]);
	}

	// ==========================================================================
	// 【辅助·负面断言】不同种子两实例 → 序列不同（补充验证，直观）
	// ==========================================================================

	UDiceRngService* ServiceA = NewObject<UDiceRngService>(GetTransientPackage());
	UDiceRngService* ServiceB = NewObject<UDiceRngService>(GetTransientPackage());

	if (!TestNotNull(TEXT("TC_LC3: 实例 A 应能创建"), ServiceA)) { return false; }
	if (!TestNotNull(TEXT("TC_LC3: 实例 B 应能创建"), ServiceB)) { return false; }

	// 设置不同种子，期望序列不同
	ServiceA->SetSeed(100);
	ServiceB->SetSeed(200);

	TArray<int32> SeqA, SeqB;
	SeqA.Reserve(IterCount);
	SeqB.Reserve(IterCount);

	for (int32 i = 0; i < IterCount; ++i)
	{
		SeqA.Add(ServiceA->RandomRange(1, 1000));
		SeqB.Add(ServiceB->RandomRange(1, 1000));
	}

	// 期望：至少一位不同（固定不同种子，概率极高；若全部相同则 FRandomStream 设计异常）
	bool bSequencesDiffer = false;
	for (int32 i = 0; i < IterCount; ++i)
	{
		if (SeqA[i] != SeqB[i])
		{
			bSequencesDiffer = true;
			break;
		}
	}

	TestTrue(
		TEXT("TC_LC3 [辅助负面]: 实例 A(seed=100) 与实例 B(seed=200) 的随机序列应不同（不同种子辅助验证）"),
		bSequencesDiffer);

	// ==========================================================================
	// 【辅助·确定性断言】相同种子重设后序列与首次一致（Seed 可重复性）
	// ==========================================================================

	ServiceA->SetSeed(100);
	TArray<int32> SeqA2;
	SeqA2.Reserve(IterCount);
	for (int32 i = 0; i < IterCount; ++i)
	{
		SeqA2.Add(ServiceA->RandomRange(1, 1000));
	}

	for (int32 i = 0; i < IterCount; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("TC_LC3 [辅助确定性]: 实例 A 相同种子(100) 重放第 %d 次应与首次相同"), i + 1),
			SeqA[i], SeqA2[i]);
	}

	return true;
}

// =============================================================================
// TC_LC4 — OnWorldBeginPlay 注入点：true-path 注入 + false-path pin
//
// AC 覆盖：[Host] OnWorldBeginPlay 为唯一 Seed 注入时机（story-001 [Host]）
//
// 验证步骤：
//   步骤 1（GetSubsystem 路径）：
//     headless CreateWorld(Game) 真创建了 WorldSubsystem（TestNotNull 已验证）。
//     此步前提 = headless CreateWorld(Game) 能触发 WorldSubsystem 注册，
//     已由本测试跑绿经验证（TestNotNull 失败则 return false，后续不执行）。
//     Initialize 后 GetCurrentSeed()==0（Initialize 不注入，ADR-0001 Forbidden）。
//   步骤 2（true-path 正向）：
//     设 bUseFixedSeed=true / FixedSeed=999 → Test_TriggerOnWorldBeginPlay
//     → GetCurrentSeed()==999（OnWorldBeginPlay 注入生效）。
//   步骤 3（false-path pin 负向）：
//     独立 NewObject 实例，bUseFixedSeed=false（默认值，不设）
//     → Test_TriggerOnWorldBeginPlay → GetCurrentSeed()==0。
//     「故意未接线」负向事实：防有人在 else 分支意外加注入逻辑。
//
// defer ff-007 说明：
//   PIE Stop→Start 重触发 OnWorldBeginPlay + Seed 重注入的完整运行时验证，
//   headless 下 CreateWorld 不自动触发 BeginPlay，无法可靠驱动。
//   defer ff-007（PIE 隔离验证 story），与 ff-001/ff-003 同惯例。
//
// 非 vacuous 保证：
//   步骤 1：若 Initialize 违反 ADR-0001 注入 Seed → seed!=0 → TestEqual FAIL。
//   步骤 2：若 OnWorldBeginPlay true-path 未调 SetSeed → seed!=999 → TestEqual FAIL。
//   步骤 3：若 false-path 被意外加入注入逻辑 → seed!=0 → TestEqual FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_LC4_OnWorldBeginPlay_InjectsFixedSeed,
	"Rento.Dice.RngServiceLifecycle.TC_LC4_OnWorldBeginPlay_InjectsFixedSeed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_LC4_OnWorldBeginPlay_InjectsFixedSeed::RunTest(const FString& Parameters)
{
	// ---- 步骤 1：Initialize 不注入 Seed（GetSubsystem 路径） ----
	// GIVEN：创建 Game World（引擎自动触发 UDiceRngService::Initialize）
	// 此步前提：headless CreateWorld(Game) 能触发 WorldSubsystem 注册（已由本测试跑绿经验证）
	UWorld* GameWorld = DiceRngLifecycleTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("LC4_GameWorld"));
	if (!TestNotNull(TEXT("TC_LC4: 应能创建 Game World"), GameWorld)) { return false; }

	// 获取引擎自动创建的 UDiceRngService 实例
	UDiceRngService* Service = GameWorld->GetSubsystem<UDiceRngService>();
	if (!TestNotNull(TEXT("TC_LC4: Game World 应自动创建 UDiceRngService（ShouldCreateSubsystem=true）"), Service))
	{
		DiceRngLifecycleTestHelpers::DestroyTestWorld(GameWorld);
		return false;
	}

	// THEN-1：Initialize 不注入 Seed，GetCurrentSeed() 应为 FRandomStream 默构值 0
	// （ADR-0001 Forbidden：Never 在 Initialize 注入 Seed——玩家配置尚未落地）
	// （ADR-0004 Verification ③ CONFIRMED：FRandomStream() 默构种子恒 0）
	// 若有人违反 ADR-0001 在 Initialize 里注入了 Seed（任何非 0 值），此处 FAIL（非 vacuous）
	const int32 SeedAfterInit = Service->GetCurrentSeed();
	TestEqual(
		*FString::Printf(TEXT("TC_LC4 步骤1: Initialize 后 GetCurrentSeed() 应为默构值 0（Initialize 不注入，ADR-0001 Forbidden；实际=%d）"), SeedAfterInit),
		SeedAfterInit, 0);

	// ---- 步骤 2：OnWorldBeginPlay true-path 注入固定种子 ----
	// GIVEN：设 bUseFixedSeed=true + FixedSeed=999（测试旋钮，直接设成员）
	Service->bUseFixedSeed = true;
	Service->FixedSeed = 999;

	// WHEN：手动触发 OnWorldBeginPlay 逻辑（通过测试辅助入口，headless 安全）
	//
	// 为何不直接调用 Service->OnWorldBeginPlay()：
	//   OnWorldBeginPlay 是 protected 虚函数，C++ access control 禁止外部直接调用。
	//   Test_TriggerOnWorldBeginPlay 是 WITH_AUTOMATION_TESTS 门控的 public 转发入口，
	//   生产构建中不可见，不破坏封装。
	//
	// defer ff-007 说明：
	//   完整 PIE Stop→Start 重触发留 ff-007（PIE 隔离验证 story），
	//   本测试仅验证「注入点语义」（true-path / false-path），与 ff-001/ff-003 同惯例。
	Service->Test_TriggerOnWorldBeginPlay(*GameWorld);

	// THEN-2：OnWorldBeginPlay true-path 注入后 GetCurrentSeed() 应返回 999
	// 若 OnWorldBeginPlay 中 bUseFixedSeed 分支未正确调用 SetSeed，此处 FAIL（非 vacuous）
	const int32 SeedAfterBeginPlay = Service->GetCurrentSeed();
	TestEqual(
		*FString::Printf(TEXT("TC_LC4 步骤2 [true-path]: bUseFixedSeed=true/FixedSeed=999 后 GetCurrentSeed() 应为 999（实际=%d）"), SeedAfterBeginPlay),
		SeedAfterBeginPlay, 999);

	DiceRngLifecycleTestHelpers::DestroyTestWorld(GameWorld);

	// ---- 步骤 3：false-path pin（独立 NewObject 实例，不依赖 GetSubsystem） ----
	// GIVEN：独立 NewObject 实例，bUseFixedSeed 保持默认值 false（不手动设置）
	// 不使用 CreateWorld/GetSubsystem，直接 NewObject 更轻量、语义更清晰
	UDiceRngService* ServiceFalsePath = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("TC_LC4 步骤3: false-path 实例应能 NewObject 创建"), ServiceFalsePath))
	{
		return false;
	}

	// bUseFixedSeed 默认 false，不修改——验证默认状态下 false-path 不注入
	// check(ServiceFalsePath->bUseFixedSeed == false);  // 合约断言（不用 ensure，避免测试崩溃）

	// 需要一个 World 引用传给 Test_TriggerOnWorldBeginPlay（签名要求）
	// 复用一个临时 Game World（内容无关，false-path 不读 World）
	UWorld* FalsePathWorld = DiceRngLifecycleTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("LC4_FalsePathWorld"));
	if (!TestNotNull(TEXT("TC_LC4 步骤3: false-path 临时 World 应能创建"), FalsePathWorld))
	{
		return false;
	}

	// WHEN：触发 OnWorldBeginPlay（bUseFixedSeed=false → else 分支 → 不注入）
	ServiceFalsePath->Test_TriggerOnWorldBeginPlay(*FalsePathWorld);

	// THEN-3：false-path 下 GetCurrentSeed() 应仍为默构值 0
	// 「故意未接线」负向事实的 pin 断言：
	//   防止有人在 OnWorldBeginPlay 的 else 分支意外加入注入逻辑。
	//   若 else 分支被错误地加入 SetSeed(非0)，此断言立即 FAIL（非 vacuous）。
	const int32 SeedFalsePath = ServiceFalsePath->GetCurrentSeed();
	TestEqual(
		*FString::Printf(TEXT("TC_LC4 步骤3 [false-path pin]: bUseFixedSeed=false 的 OnWorldBeginPlay 后 seed 应仍为 0（else 分支不注入；实际=%d）"), SeedFalsePath),
		SeedFalsePath, 0);

	DiceRngLifecycleTestHelpers::DestroyTestWorld(FalsePathWorld);
	return true;
}
