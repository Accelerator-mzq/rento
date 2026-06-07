// IGameClockDITest.cpp
// =============================================================================
// 集成测试：IGameClock DI 注入骨架（story-002 AC-1~6）
//
// 物理路径：Source/rentoTests/Private/IGameClockDITest.cpp
// 逻辑路径：tests/integration/foundation/igameclock_di_test.cpp
// Automation 类目：Rento.Foundation.IGameClockDI
//
// ─── 测试矩阵 ───────────────────────────────────────────────────────────────
//
// TC-1 (AC-1/AC-2)：
//   FWorldGameClock 接口实现 + 委托一致性 + nullptr-World 兜底。
//
//   【AC-2 证明范围说明】：
//     headless CreateWorld 后 World->GetTimeSeconds() 静止为 0.0（无 Tick 驱动），
//     FWorldGameClock 委托路径返回相同 0.0。两者均为常量，
//     "0.0==0.0" 无法区分「正确委托」与「硬编码返回 0.0 的 mutant」。
//     因此 TC-1 拆为两层：
//       (a) 委托一致性：有效 World 下 NowSeconds()==World->GetTimeSeconds()（验委托非常量）
//       (b) nullptr-World 兜底：FWorldGameClock(nullptr).NowSeconds()==0.0（mutation-testable：
//           若兜底返回非 0 值则 FAIL；若无 nullptr 守卫则崩溃而非返回 0）
//     AC-2「读 LIVE 推进 World 时间」完整证明 defer ff-007（需 PIE Tick 才能推进 World 时间，
//     headless 环境无法可靠推进；live-time 单调递增行为由 ff-007 集成验证覆盖）。
//
// TC-2 (AC-3)：
//   FMockGameClock 受控推进：SetNow(5.0)→5.0；SetNow(10.0)→10.0；Advance(3.0)→13.0。
//   无需 World，纯对象测试。
//
// TC-3 (AC-4/AC-5)：
//   GIVEN：Game World 已 Initialize（UTestMatchSubsystem 由引擎自动创建）
//   WHEN：注入 FMockGameClock（SetGameClock），推进受控时钟 0→1.0→2.0
//   THEN：每次调 GetClockNowSeconds() 返回对应受控值（1.0 / 2.0）
//   ALSO：多次 SetGameClock 替换（MockA→5.0, MockB→7.0），证多次替换正确。
//
//   【非 vacuous 论证】：
//     断言 GetClockNowSeconds()==1.0。
//     headless World 时间静止=0.0（无 Tick）。
//     若实现直读 GetWorld()->GetTimeSeconds()（绕过注入时钟），返回 0.0≠1.0 → FAIL。
//     只有路由到 GameClock->NowSeconds()（mock 返回 1.0），断言才 PASS。
//
// TC-4 (AC-6，路径 A)：
//   (a) 编译期 static_assert：GetGameClock() 返回 TSharedPtr<IGameClock>（非裸指针）
//   (b) 运行期：Initialize 后 TSharedPtr.IsValid()==true
//   (c) 反射：UMatchSubsystemBase 本类 UPROPERTY 总数仍==0（FF-001 TC-4 零成员不变式）
//
// Edge_NullClockSafe：
//   SetGameClock(nullptr) 后调 GetClockNowSeconds()，返回 0.0，不崩。
//   AddExpectedError 吞预期 UE_LOG Error 日志（ASCII 前缀匹配，编码无关）。
//
// Edge_MockCanRewind：
//   FMockGameClock 可设小于当前值（回退场景），Advance 负值正确减少。
//
// ─── headless 确定性保证 ─────────────────────────────────────────────────────
//   - 无随机种子依赖
//   - 无真实时间依赖（全靠 mock 受控推进）
//   - 无外部 I/O
//   - CreateWorld 不触发 Tick，World 时间静止（0.0），不与 mock 值混淆
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "MatchSubsystemBase.h"
#include "IGameClock.h"
#include "FMockGameClock.h"
#include "TestMatchSubsystem.h"

// 用于编译期类型断言（TSharedPtr 非裸指针检查）
#include <type_traits>

// =============================================================================
// 辅助：创建/销毁测试 World（与 MatchSubsystemBaseTest 保持相同惯例）
// =============================================================================
namespace IGameClockDITestHelpers
{
	/** 创建 EWorldType::Game 的最小测试 World（bInformEngineOfWorld=false，headless 安全）。*/
	static UWorld* CreateGameWorld(const FName& WorldName)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, WorldName);
		check(World);
		return World;
	}

	/** 销毁测试 World，释放资源。*/
	static void DestroyWorld(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		}
	}
}

// =============================================================================
// TC-1 (AC-1/AC-2)：FWorldGameClock 接口实现 + 委托一致性 + nullptr-World 兜底
//
// MF-1 降级说明：
//   原版「ClockTime==WorldTime」在 headless 下均为 0.0，无法区分正确实现与 mutant。
//   改为两层验证：
//     (a) 委托一致性（保留）：有效 World → NowSeconds()==World->GetTimeSeconds()
//     (b) nullptr-World 兜底（新增，mutation-testable）：
//         FWorldGameClock(nullptr).NowSeconds()==0.0
//         mutation「返回 1.0」→ TestEqual(1.0, 0.0) FAIL
//         mutation「无 nullptr 守卫直接解引用」→ 崩溃（≠ 0.0 返回路径）
//   AC-2 live-time 完整验证 defer ff-007（headless World 时间恒静止，无法推进）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIGameClockDI_TC1_WorldClockMatchesWorldTime,
	"Rento.Foundation.IGameClockDI.TC1_WorldClock_NowSeconds_MatchesWorldTime",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIGameClockDI_TC1_WorldClockMatchesWorldTime::RunTest(const FString& Parameters)
{
	// ─── (a) 有效 World 委托一致性 ────────────────────────────────────────────
	// GIVEN：一个有效的 Game World
	UWorld* World = IGameClockDITestHelpers::CreateGameWorld(TEXT("TC1_ClockWorld"));
	if (!TestNotNull(TEXT("TC-1: 应能创建 Game World"), World))
	{
		return false;
	}

	// GIVEN：FWorldGameClock 持有该 World
	FWorldGameClock WorldClock(World);

	// WHEN：读取 World 时间基准（headless 下静止，通常为 0.0）
	const double WorldTime = static_cast<double>(World->GetTimeSeconds());

	// WHEN：读取 FWorldGameClock::NowSeconds()
	const double ClockTime = WorldClock.NowSeconds();

	// THEN：FWorldGameClock 报告的时间应与 World->GetTimeSeconds() 一致（AC-2 委托一致性）
	// 注：headless 下两者均为 0.0，本断言仅验「委托路径正确」而非「live 推进值正确」；
	//     AC-2「读 LIVE 推进 World 时间」完整验证 defer ff-007（PIE Tick 驱动才能推进）。
	TestEqual(
		TEXT("TC-1(a): FWorldGameClock::NowSeconds() 应与 World->GetTimeSeconds() 委托一致（AC-2）"),
		ClockTime, WorldTime);

	// THEN：再次读取应单调不减（headless 静止下相等亦满足"不减"）
	const double ClockTime2 = WorldClock.NowSeconds();
	TestTrue(
		TEXT("TC-1(a): 连续两次调用 NowSeconds() 应单调不减（AC-2）"),
		ClockTime2 >= ClockTime);

	// 接口继承由编译期保证：若 FWorldGameClock 不继承 IGameClock，
	// 下方隐式转换赋值无法编译（编译期错误，比运行期 TestNotNull 更强）。
	// 此处仅做类型兼容性的文档注释，不再写栈地址恒非空的 vacuous TestNotNull。
	const IGameClock* AsInterface = &WorldClock;
	(void)AsInterface; // suppress unused-variable warning；接口继承已由赋值本身在编译期证明

	IGameClockDITestHelpers::DestroyWorld(World);

	// ─── (b) nullptr-World 兜底（mutation-testable）────────────────────────────
	// GIVEN：FWorldGameClock 持 nullptr World（World 失效场景）
	FWorldGameClock NullClock(nullptr);

	// WHEN / THEN：NowSeconds() 应安全返回 0.0，不崩溃（AC-2 兜底路径）
	// Mutation-testable：
	//   mutation「return 1.0」→ TestEqual(1.0, 0.0) FAIL
	//   mutation「无 nullptr 守卫直接 World->GetTimeSeconds()」→ 空指针解引用崩溃（≠ 正常返回 0.0）
	const double NullClockTime = NullClock.NowSeconds();
	TestEqual(
		TEXT("TC-1(b): nullptr World 下 FWorldGameClock::NowSeconds() 应安全返回 0.0（AC-2 兜底）"),
		NullClockTime, 0.0);

	return true;
}

// =============================================================================
// TC-2 (AC-3)：FMockGameClock 受控推进
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIGameClockDI_TC2_MockClockControlledAdvance,
	"Rento.Foundation.IGameClockDI.TC2_MockClock_ControlledAdvance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIGameClockDI_TC2_MockClockControlledAdvance::RunTest(const FString& Parameters)
{
	// GIVEN：FMockGameClock 初值 0
	FMockGameClock Mock;
	TestEqual(TEXT("TC-2: 初始值应为 0.0"), Mock.NowSeconds(), 0.0);

	// WHEN：SetNow(5.0) → THEN：读到 5.0（AC-3）
	Mock.SetNow(5.0);
	TestEqual(TEXT("TC-2: SetNow(5.0) 后 NowSeconds() 应返回 5.0（AC-3）"),
		Mock.NowSeconds(), 5.0);

	// WHEN：SetNow(10.0) → THEN：读到 10.0（AC-3 受控推进）
	Mock.SetNow(10.0);
	TestEqual(TEXT("TC-2: SetNow(10.0) 后 NowSeconds() 应返回 10.0（AC-3）"),
		Mock.NowSeconds(), 10.0);

	// WHEN：Advance(3.0) → THEN：读到 13.0（AC-3 递增推进）
	Mock.Advance(3.0);
	TestEqual(TEXT("TC-2: Advance(3.0) 后 NowSeconds() 应返回 13.0（AC-3）"),
		Mock.NowSeconds(), 13.0);

	// 接口继承由编译期保证（同 TC-1 说明）；经接口指针调用验多态分派正确。
	const IGameClock* AsInterface = &Mock;
	TestEqual(TEXT("TC-2: 经 IGameClock* 调用 NowSeconds() 应返回相同受控值 13.0"),
		AsInterface->NowSeconds(), 13.0);

	return true;
}

// =============================================================================
// TC-3 (AC-4/AC-5)：Game World 宿主注入 FMockGameClock，依赖方读受控时间
//
// 非 vacuous 论证：
//   断言 GetClockNowSeconds()==1.0。
//   headless World 时间静止=0.0（无 Tick）。
//   若实现直读 GetWorld()->GetTimeSeconds()，则返回 0.0≠1.0 → TestEqual FAIL。
//   只有路由到 GameClock->NowSeconds()（mock 返回 1.0），断言才 PASS。
//
// R-2 补充：末尾加多次替换 mini-case（MockA→5.0，MockB→7.0），
//           证 SetGameClock 可多次替换且每次生效正确。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIGameClockDI_TC3_HostInjectsMockClockDIWorks,
	"Rento.Foundation.IGameClockDI.TC3_Host_InjectMockClock_DIWorks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIGameClockDI_TC3_HostInjectsMockClockDIWorks::RunTest(const FString& Parameters)
{
	// GIVEN：一个 Game World，引擎自动 Initialize UTestMatchSubsystem（非 Abstract 子类）
	UWorld* World = IGameClockDITestHelpers::CreateGameWorld(TEXT("TC3_DIWorld"));
	if (!TestNotNull(TEXT("TC-3: 应能创建 Game World"), World))
	{
		return false;
	}

	// GIVEN：取得宿主实例（引擎在 CreateWorld 时自动创建所有注册的 WorldSubsystem）
	UTestMatchSubsystem* Host = World->GetSubsystem<UTestMatchSubsystem>();
	if (!TestNotNull(TEXT("TC-3: UTestMatchSubsystem 应在 Game World 中自动创建（AC-4）"), Host))
	{
		IGameClockDITestHelpers::DestroyWorld(World);
		return false;
	}

	// 基准验证：宿主已被 Initialize（初始时钟为 FWorldGameClock）
	// headless 下 World 时间静止，FWorldGameClock::NowSeconds() 应返回 0.0
	const double WorldTimeBaseline = static_cast<double>(World->GetTimeSeconds());
	TestEqual(TEXT("TC-3: Initialize 后默认时钟（FWorldGameClock）应读到 World 时间基准"),
		Host->GetClockNowSeconds(), WorldTimeBaseline);

	// ─── 关键 DI 替换 ───────────────────────────────────────────────────────
	// WHEN：注入 FMockGameClock，替换默认 FWorldGameClock（AC-4 DI 注入面）
	//
	// SetGameClock 参数是值传递（TSharedPtr<IGameClock> InClock），
	// 内部 MoveTemp 移动的是该参数的局部拷贝，不影响调用方持有的 MockClock 原件。
	// 即：调用后 MockClock 在调用方仍有效（引用计数 >= 1），可继续 SetNow/Advance。
	TSharedPtr<FMockGameClock> MockClock = MakeShared<FMockGameClock>();
	Host->SetGameClock(MockClock);

	// ─── 推进 0 → 1.0 ────────────────────────────────────────────────────────
	// WHEN：推进受控时钟到 1.0
	MockClock->SetNow(1.0);

	// THEN：依赖方（GetClockNowSeconds）读到 1.0（非 World 时间 0.0）
	//
	// 【非 vacuous 断言】：
	//   headless World 时间静止=0.0（无 Tick 驱动）。
	//   若 GetClockNowSeconds() 直读 GetWorld()->GetTimeSeconds()，返回 0.0≠1.0 → FAIL。
	//   只有经注入时钟路由，mock 返回 1.0，断言才 PASS。
	TestEqual(
		TEXT("TC-3: 注入 MockClock 后 GetClockNowSeconds() 应返回受控值 1.0，非 World 时间（AC-5 DI 生效）"),
		Host->GetClockNowSeconds(), 1.0);

	// ─── 推进 1.0 → 2.0 ──────────────────────────────────────────────────────
	// WHEN：继续推进到 2.0
	MockClock->Advance(1.0);   // 1.0 + 1.0 = 2.0

	// THEN：读到 2.0（AC-5 连续推进）
	TestEqual(
		TEXT("TC-3: Advance(1.0) 后 GetClockNowSeconds() 应返回 2.0（AC-5 连续推进）"),
		Host->GetClockNowSeconds(), 2.0);

	// ─── 确认 World 时间仍为静止基准（证明读取未绕道真实 World 时间）───────────
	// headless 无 Tick，World 时间不应改变
	const double WorldTimeAfter = static_cast<double>(World->GetTimeSeconds());
	TestEqual(TEXT("TC-3: headless 下 World 时间应仍为初始值（未被 Tick 推进）"),
		WorldTimeAfter, WorldTimeBaseline);
	// 证明：mock 返回 2.0，World 时间仍为 0.0；若实现读 World 则上面断言早 FAIL。
	TestNotEqual(TEXT("TC-3: mock 受控值（2.0）与 World 静止时间（0.0）应不同（证 DI 路径非 World 直读）"),
		Host->GetClockNowSeconds(), WorldTimeAfter);

	// ─── R-2：多次替换 mini-case ─────────────────────────────────────────────
	// 验证 SetGameClock 可多次调用，每次替换后依赖方立即读到新时钟的值。
	//
	// SetGameClock 参数值传递（TSharedPtr<IGameClock> InClock），
	// MoveTemp 移动参数局部拷贝，不影响 MockA/MockB 在调用方的 TSharedPtr 原件，
	// 故注入后调用方仍可通过 MockA/MockB 操控时钟值（排除 moved-from 陷阱歧义）。

	// MockA：注入后设 5.0，断言读到 5.0
	TSharedPtr<FMockGameClock> MockA = MakeShared<FMockGameClock>();
	Host->SetGameClock(MockA);       // 第二次 SetGameClock
	MockA->SetNow(5.0);
	TestEqual(
		TEXT("TC-3(R-2): 注入 MockA 后 GetClockNowSeconds() 应返回 5.0"),
		Host->GetClockNowSeconds(), 5.0);

	// MockB：再次替换，设 7.0，断言读到 7.0（而非 MockA 的 5.0）
	TSharedPtr<FMockGameClock> MockB = MakeShared<FMockGameClock>();
	Host->SetGameClock(MockB);       // 第三次 SetGameClock
	MockB->SetNow(7.0);
	TestEqual(
		TEXT("TC-3(R-2): 注入 MockB 后 GetClockNowSeconds() 应返回 7.0（非 MockA 残留 5.0）"),
		Host->GetClockNowSeconds(), 7.0);

	// 确认 MockA 原件未被 moved-from 破坏：MockA 仍持有值 5.0（SetGameClock 值传递不影响原件）
	TestEqual(
		TEXT("TC-3(R-2): SetGameClock 值传递后 MockA 原件应仍有效，NowSeconds()==5.0"),
		MockA->NowSeconds(), 5.0);

	IGameClockDITestHelpers::DestroyWorld(World);
	return true;
}

// =============================================================================
// TC-4 (AC-6，路径 A)：
//   (a) GetGameClock() 返回 TSharedPtr<IGameClock>，非裸指针（编译期+运行期双验证）
//   (b) UMatchSubsystemBase 本类 UPROPERTY 总数仍 == 0（复用 FF-001 TC-4 反射手法）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIGameClockDI_TC4_SharedPtrAndZeroUPROPERTY,
	"Rento.Foundation.IGameClockDI.TC4_SharedPtrCarrier_And_ZeroUPROPERTY",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIGameClockDI_TC4_SharedPtrAndZeroUPROPERTY::RunTest(const FString& Parameters)
{
	// ─── (a) 编译期类型断言：GetGameClock() 返回 TSharedPtr<IGameClock>（非裸指针）────
	//
	// 如果返回类型是裸 IGameClock*，下方 std::is_same 静态断言在编译期就失败，
	// 测试文件无法通过编译——这是比运行期断言更强的证明形式。
	//
	// decltype 获取方法真实返回类型，无需实例化对象。
	using GetGameClockReturnType = decltype(std::declval<UMatchSubsystemBase>().GetGameClock());

	// 编译期断言：返回类型必须是 TSharedPtr<IGameClock>（路径 A 裁定）
	static_assert(
		std::is_same<GetGameClockReturnType, TSharedPtr<IGameClock>>::value,
		"AC-6(路径 A): GetGameClock() 返回类型必须是 TSharedPtr<IGameClock>，非裸指针。"
		"若此 static_assert 触发，说明实现偏离了路径 A 裁定。");

	// 运行期验证：Game World 中实际取到的时钟实例 TSharedPtr 应有效（IsValid）
	UWorld* World = IGameClockDITestHelpers::CreateGameWorld(TEXT("TC4_SharedPtrWorld"));
	if (!TestNotNull(TEXT("TC-4: 应能创建 Game World"), World))
	{
		return false;
	}

	UTestMatchSubsystem* Host = World->GetSubsystem<UTestMatchSubsystem>();
	if (!TestNotNull(TEXT("TC-4: 应能取得 UTestMatchSubsystem"), Host))
	{
		IGameClockDITestHelpers::DestroyWorld(World);
		return false;
	}

	// Initialize 后 GetGameClock() 应返回有效的 TSharedPtr（非空）
	TSharedPtr<IGameClock> ClockPtr = Host->GetGameClock();
	TestTrue(
		TEXT("TC-4: Initialize 后 GetGameClock() 返回的 TSharedPtr<IGameClock> 应有效（非空）（AC-6）"),
		ClockPtr.IsValid());

	// ─── (b) 反射断言：UMatchSubsystemBase 本类 UPROPERTY 总数仍 == 0 ──────────
	//
	// 复用 FF-001 TC-4（MatchSubsystemBaseTest.cpp）的手法：
	// TFieldIterator<FProperty>(BaseClass, EFieldIterationFlags::None) 仅遍历本类声明的 UPROPERTY，
	// 不含父类（EFieldIterationFlags::None = 不向上继承）。
	//
	// GameClock 成员（TSharedPtr<IGameClock>）故意不加 UPROPERTY：
	//   1. IGameClock 是纯 C++ 接口，非 UObject，GC 不需要追踪
	//   2. 不加 UPROPERTY 保证本断言不回归（路径 A 设计约束）
	//
	// 若有人误将 GameClock 改为 UPROPERTY，此断言立即 FAIL，防止 FF-001 TC-4 回归。
	const UClass* BaseClass = UMatchSubsystemBase::StaticClass();
	int32 OwnPropertyCount = 0;
	for (TFieldIterator<FProperty> PropIt(BaseClass, EFieldIterationFlags::None); PropIt; ++PropIt)
	{
		++OwnPropertyCount;
	}

	TestEqual(
		TEXT("TC-4: UMatchSubsystemBase 本类 UPROPERTY 总数应仍为 0"
		     "（GameClock 成员非 UPROPERTY，FF-001 TC-4 零成员不变式未回归）（AC-6）"),
		OwnPropertyCount, 0);

	IGameClockDITestHelpers::DestroyWorld(World);
	return true;
}

// =============================================================================
// Edge_NullClockSafe：SetGameClock(nullptr) 后调 GetClockNowSeconds() 安全返回 0.0
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIGameClockDI_Edge_NullClockSafe,
	"Rento.Foundation.IGameClockDI.Edge_NullClock_GetClockNowSeconds_ReturnsSafeDefault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIGameClockDI_Edge_NullClockSafe::RunTest(const FString& Parameters)
{
	// GIVEN：Game World，宿主已 Initialize（持默认 FWorldGameClock）
	UWorld* World = IGameClockDITestHelpers::CreateGameWorld(TEXT("Edge_NullClockWorld"));
	if (!TestNotNull(TEXT("Edge_NullClock: 应能创建 Game World"), World))
	{
		return false;
	}

	UTestMatchSubsystem* Host = World->GetSubsystem<UTestMatchSubsystem>();
	if (!TestNotNull(TEXT("Edge_NullClock: 应能取得 UTestMatchSubsystem"), Host))
	{
		IGameClockDITestHelpers::DestroyWorld(World);
		return false;
	}

	// WHEN：注入空指针（模拟时钟被显式清除的异常场景）
	Host->SetGameClock(nullptr);

	// THEN：GetClockNowSeconds() 应触发 UE_LOG Error（预期日志，需 AddExpectedError 吞掉）
	// R-1：使用纯 ASCII 前缀匹配，避免含中文的匹配串在 GBK/UTF-8 编码差异下不稳定。
	// UE_LOG 消息以 "UMatchSubsystemBase::GetClockNowSeconds" 开头（ASCII），Contains 稳定匹配。
	AddExpectedError(
		TEXT("UMatchSubsystemBase::GetClockNowSeconds"),
		EAutomationExpectedMessageFlags::Contains,
		1);

	// THEN：返回 0.0，不崩溃（Edge case 安全兜底）
	const double SafeDefault = Host->GetClockNowSeconds();
	TestEqual(
		TEXT("Edge_NullClock: 未注入时钟时 GetClockNowSeconds() 应安全返回 0.0（不崩溃）"),
		SafeDefault, 0.0);

	IGameClockDITestHelpers::DestroyWorld(World);
	return true;
}

// =============================================================================
// Edge_MockCanRewind：FMockGameClock 支持回退设置（SetNow 可设小于当前值）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIGameClockDI_Edge_MockCanRewind,
	"Rento.Foundation.IGameClockDI.Edge_MockClock_CanRewind",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FIGameClockDI_Edge_MockCanRewind::RunTest(const FString& Parameters)
{
	// GIVEN：FMockGameClock 设为 10.0
	FMockGameClock Mock;
	Mock.SetNow(10.0);
	TestEqual(TEXT("Edge_Rewind: 设为 10.0 后应读到 10.0"), Mock.NowSeconds(), 10.0);

	// WHEN：SetNow(3.0)（小于当前值，模拟回退）
	Mock.SetNow(3.0);

	// THEN：读到 3.0（mock 支持回退，不强制单调）
	TestEqual(TEXT("Edge_Rewind: SetNow(3.0) 回退后应读到 3.0"),
		Mock.NowSeconds(), 3.0);

	// WHEN：Advance(-1.0)（负推进）
	Mock.Advance(-1.0);

	// THEN：读到 2.0
	TestEqual(TEXT("Edge_Rewind: Advance(-1.0) 后应读到 2.0"),
		Mock.NowSeconds(), 2.0);

	return true;
}
