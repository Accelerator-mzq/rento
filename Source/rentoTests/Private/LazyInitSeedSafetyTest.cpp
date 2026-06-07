// LazyInitSeedSafetyTest.cpp
// =============================================================================
// 单元测试：UDiceRngService lazy-init 兜底种子安全（story-004）
//
// 物理路径：Source/rentoTests/Private/LazyInitSeedSafetyTest.cpp
// 逻辑分类：tests/unit/dice/lazy_init_seed_safety_test.cpp
// Automation 类目：Rento.Dice.LazyInitSeedSafety
//
// 覆盖 AC：
//   AC-17  → TC_AC17_NoSetSeed_RollDice_DoesNotCrash
//   AC-17b → TC_AC17b_LazyInit_InitialSeed_NotZero
//
// 测试设计原则：
//   - 确定性：AC-17b 断言 InitialSeed != 0 是确定性断言（Cycles()|1u 恒非 0），
//     禁用「两次冷启动序列不同」概率断言（AC-9 缺陷模式，story-004 Implementation Notes）
//   - NewObject<UDiceRngService>(GetTransientPackage()) —— headless 安全
//   - 不调 SetSeed（故意触发 lazy-init 兜底路径）
//   - AddExpectedError 次数 = 1（每测试函数触发一次 lazy-init）：
//     RollDice() → RandomRange(1,6) 首次调用触发 EnsureSeedInitialized（Error 日志）；
//     第二次 RandomRange(1,6) 时 bSeedInitialized 已 true，不再触发。
//     GetInitialSeed() 不抽流，不触发 EnsureSeedInitialized。
//     总计：每次 RollDice() 一次 lazy-init，期望计数 = 1。
//
// Out of Scope（严守 story-004 边界）：
//   - 正常 SetSeed 路径 → story-001（已覆盖）
//   - RollDice 字段不变式本体（AC-1）→ story-002（本条复用其约束作兜底验证）
//   - Full Vision 重放 fail-fast（OQ-3）→ 不在 MVP
//   - RandomRange 三道门逻辑 → story-003（已覆盖，不重测）
//   - 确定性长序列 fixture（AC-8/9）→ story-006
//
// 规范依据：
//   - ADR-0004（primary）— lazy-init 兜底 Guideline 3
//   - control-manifest Foundation Layer §确定性 RNG Forbidden §Never 默构 0
//   - story-004 AC-17/17b QA Test Cases
//   - 项目既定惯例：UE_LOG(Error) 替 ensure()，AddExpectedError(Contains) 捕获
// =============================================================================

#include "Misc/AutomationTest.h"
#include "DiceRngService.h"

// =============================================================================
// TC_AC17 — AC-17：不调 SetSeed 直接 RollDice()，shipping 不崩，返回值满足 AC-1
//
// Given:  NewObject<UDiceRngService>，不调 SetSeed。
//         AddExpectedError("lazy-init", Contains, 1) 预注册捕获 lazy-init Error 日志。
// When:   直接 RollDice()（未初始化，触发 EnsureSeedInitialized lazy-init 兜底）。
// Then:   不崩溃（shipping 安全）；
//         返回值满足 AC-1 约束：
//           Die1 ∈ [1,6]，Die2 ∈ [1,6]，Total == Die1+Die2，bIsDouble == (Die1==Die2)。
//
// AddExpectedError 计数分析（确认为 1）：
//   RollDice → RandomRange(1,6)【第一次】→ EnsureSeedInitialized：
//     bSeedInitialized==false → UE_LOG Error（1 行，含「lazy-init」）→ bSeedInitialized=true
//   RollDice → RandomRange(1,6)【第二次】→ EnsureSeedInitialized：
//     bSeedInitialized==true → 直返，不记 Error
//   合计：1 行 Error，期望计数 = 1。
//
// 非 vacuous 保证：
//   若 lazy-init 实现错误退化为 FRandomStream 默构种子 0（AuthoritativeStream 从未 Initialize），
//   则抽取序列确定（每次从 0 出发），但值仍可能落 [1,6]——范围断言不能发现此 bug。
//   此 bug 由 AC-17b（InitialSeed != 0）发现。
//   本条的核心价值：验证 shipping 不崩（无未定义行为、无空指针、无越界返回值）。
//   AC-1 约束断言非 vacuous：若 lazy-init 实现有 bug 返回 [0,0] 或 [0,6]，Die1>=1 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_AC17_NoSetSeed_RollDice_DoesNotCrash,
	"Rento.Dice.LazyInitSeedSafety.TC_AC17_NoSetSeed_RollDice_DoesNotCrash",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_AC17_NoSetSeed_RollDice_DoesNotCrash::RunTest(const FString& Parameters)
{
	// 预注册：捕获 EnsureSeedInitialized 触发的 lazy-init Error 日志（期望 1 次）
	// 子串「lazy-init」与 DiceRngService.cpp EnsureSeedInitialized 中 UE_LOG Error 文本对应。
	// 若实现未记 Error（lazy-init 路径未走），期望计数 1 未满足 → Automation 框架报 FAIL。
	// 若 lazy-init 触发多于 1 次（实现有 bug 重复进入），期望计数 1 未匹配 → FAIL。
	AddExpectedError(TEXT("lazy-init"), EAutomationExpectedErrorFlags::Contains, 1);

	// GIVEN：NewObject 创建服务实例，故意不调 SetSeed（触发 lazy-init 路径）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-17: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	// WHEN：直接 RollDice()（未初始化，触发 EnsureSeedInitialized lazy-init 兜底）
	// 若实现 shipping 会崩，此行 crash → 测试进程崩溃（AC-17 FAIL）
	const FDiceRollResult Result = Service->RollDice();

	// THEN：返回值满足 AC-1 约束（Die1∈[1,6] 等，验 lazy-init 后抽流正常）
	// 非 vacuous：若 lazy-init 实现有 bug 使抽流返回越界值（如 0 或 7），下界/上界断言 FAIL

	// Die1 ∈ [1, 6]
	TestTrue(
		*FString::Printf(TEXT("AC-17: lazy-init 后 RollDice Die1=%d 应 >= 1"), Result.Die1),
		Result.Die1 >= 1);
	TestTrue(
		*FString::Printf(TEXT("AC-17: lazy-init 后 RollDice Die1=%d 应 <= 6"), Result.Die1),
		Result.Die1 <= 6);

	// Die2 ∈ [1, 6]
	TestTrue(
		*FString::Printf(TEXT("AC-17: lazy-init 后 RollDice Die2=%d 应 >= 1"), Result.Die2),
		Result.Die2 >= 1);
	TestTrue(
		*FString::Printf(TEXT("AC-17: lazy-init 后 RollDice Die2=%d 应 <= 6"), Result.Die2),
		Result.Die2 <= 6);

	// Total == Die1 + Die2（构造时不变式）
	// 非 vacuous：若 Result.Total 被误算（如固化后重算），此断言 FAIL
	TestEqual(
		*FString::Printf(TEXT("AC-17: lazy-init 后 Total=%d 应 == Die1(%d)+Die2(%d)=%d"),
			Result.Total, Result.Die1, Result.Die2, Result.Die1 + Result.Die2),
		Result.Total, Result.Die1 + Result.Die2);

	// bIsDouble == (Die1 == Die2)（构造时不变式）
	// 非 vacuous：若 bIsDouble 逻辑有 bug（如恒 false），当 Die1==Die2 时此断言 FAIL
	const bool bExpectedIsDouble = (Result.Die1 == Result.Die2);
	TestEqual(
		*FString::Printf(TEXT("AC-17: lazy-init 后 bIsDouble=%d 应 == (Die1==%d == Die2==%d)=%d"),
			Result.bIsDouble ? 1 : 0,
			Result.Die1, Result.Die2,
			bExpectedIsDouble ? 1 : 0),
		Result.bIsDouble, bExpectedIsDouble);

	return true;
}

// =============================================================================
// TC_AC17b — AC-17b：不调 SetSeed → RollDice 触发 lazy-init → GetInitialSeed() != 0
//
// Given:  NewObject<UDiceRngService>，不调 SetSeed。
//         AddExpectedError("lazy-init", Contains, 1) 预注册捕获 lazy-init Error 日志。
// When:   RollDice()（触发 lazy-init，用 Cycles()|1u 兜底写入 InitialSeed）；
//         随后读 GetInitialSeed()。
// Then:   GetInitialSeed() != 0（确定性断言）——
//         证明兜底种子取自 FPlatformTime::Cycles()|1u（非确定源，强制非 0），
//         未退化为 FRandomStream 默构 0。
//
// 确定性分析（AC-17b 为确定性断言，非概率断言）：
//   实现：FallbackSeed = static_cast<int32>(FPlatformTime::Cycles() | 1u)
//   Cycles()|1u 的最低位恒为 1，故 FallbackSeed 为奇数，static_cast<int32> 后仍非 0。
//   因此 GetInitialSeed() != 0 是 100% 确定为真，零 flaky 风险。
//   此断言能 FAIL 的唯一条件：lazy-init 未调 Initialize（退化默构 0）或用 Cycles()&~1u（恒偶）。
//
// 非 vacuous 保证（AC-17b 的核心价值）：
//   若实现错误退化为 FRandomStream 默构种子 0（未调 Initialize），
//   则 GetInitialSeed() == 0 → TestTrue(..., false) → FAIL。
//   若 Cycles()|1u 误写为 Cycles()（不加 |1u，极小概率 0），
//   则存在 InitialSeed==0 的极小概率路径，但测试会 FAIL（发现实现缺陷）。
//
// AddExpectedError 计数分析（确认为 1）：
//   与 TC_AC17 相同——RollDice 首次 RandomRange(1,6) 触发 1 次 lazy-init；
//   GetInitialSeed() 不抽流，不触发 EnsureSeedInitialized。
//   总计：1 行 Error，期望计数 = 1。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_AC17b_LazyInit_InitialSeed_NotZero,
	"Rento.Dice.LazyInitSeedSafety.TC_AC17b_LazyInit_InitialSeed_NotZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_AC17b_LazyInit_InitialSeed_NotZero::RunTest(const FString& Parameters)
{
	// 预注册：捕获 EnsureSeedInitialized 触发的 lazy-init Error 日志（期望 1 次）
	// 子串「lazy-init」与 DiceRngService.cpp EnsureSeedInitialized 中 UE_LOG Error 文本对应。
	AddExpectedError(TEXT("lazy-init"), EAutomationExpectedErrorFlags::Contains, 1);

	// GIVEN：NewObject 创建服务实例，故意不调 SetSeed
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-17b: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	// WHEN-1：RollDice()——触发 EnsureSeedInitialized lazy-init 兜底
	//   首次 RandomRange(1,6) 时 bSeedInitialized==false → Initialize(Cycles()|1u) → bSeedInitialized=true
	Service->RollDice();

	// WHEN-2：读取 GetInitialSeed()——读 AuthoritativeStream.GetInitialSeed()
	//   返回 Initialize() 时传入的 FallbackSeed（即 Cycles()|1u 的 int32 值）
	//   注意：GetInitialSeed() 不调 EnsureSeedInitialized，不抽流，不触发新 Error
	const int32 InitialSeed = Service->GetInitialSeed();

	// THEN：InitialSeed != 0（确定性断言）
	// 证明：FallbackSeed = static_cast<int32>(Cycles()|1u)，|1u 确保最低位为 1，永不为 0。
	// 非 vacuous：
	//   若实现退化为默构（未调 Initialize）→ InitialSeed == 0（FRandomStream 默构值）→ FAIL
	//   若实现误用 Cycles()（不加 |1u）且恰好 Cycles()==0 → InitialSeed == 0 → FAIL（发现缺陷）
	TestTrue(
		*FString::Printf(TEXT(
			"AC-17b: lazy-init 后 GetInitialSeed()=%d 应 != 0"
			"（证明兜底种子取自 FPlatformTime::Cycles()|1u，非 FRandomStream 默构 0；"
			"FallbackSeed 最低位恒为 1，确定性断言）"),
			InitialSeed),
		InitialSeed != 0);

	return true;
}
