// RandomRangeDegenerateBoundaryTest.cpp
// =============================================================================
// 单元测试：UDiceRngService::RandomRange 退化与边界契约（story-003）
//
// 物理路径：Source/rentoTests/Private/RandomRangeDegenerateBoundaryTest.cpp
// 逻辑分类：tests/unit/dice/randomrange_degenerate_boundary_test.cpp
// Automation 类目：Rento.Dice.RandomRangeDegenerateBoundary
//
// 覆盖 AC：
//   AC-10  → TC_AC10_MinEqualsMax_DoesNotAdvanceCursor
//   AC-11  → TC_AC11_MinGreaterMax_ReturnsMin_NoSwap
//   AC-12b → TC_AC12b_ResultAlwaysInRange
//
// 测试设计原则：
//   - 确定性：全部使用固定种子，无时间 / IO / 随机依赖
//   - NewObject<UDiceRngService>(GetTransientPackage()) —— headless 安全
//   - 每条断言均有「非 vacuous」注释（解释该断言能 FAIL 的具体条件）
//   - Out of Scope 严守：不测 story-001/002 已有逻辑（SetSeed/RollDice/RandomFloat01），
//     不测确定性长序列 fixture（story-006），不测触发次数/重入禁止（story-005）
//
// 测试框架惯例（本项目既定，DR-001/DR-002/BD-003/BD-004 一脉）：
//   - headless 验证路径用 UE_LOG(LogTemp, Error, ...) 替 ensure()
//   - AddExpectedError(Contains) 捕获 Error 行，保证 headless Automation 计数稳定
//   - EAutomationTestFlags_ApplicationContextMask（5.4+ enum class，本项目既定）
//
// 规范依据：
//   - ADR-0004（primary）— 退化与边界契约逐字保真
//   - ADR-0007            — 封装层退化硬门落 C++
//   - control-manifest Foundation Layer §确定性 RNG (ADR-0004)
//   - story-003 AC-10/11/12b QA Test Cases
// =============================================================================

#include "Misc/AutomationTest.h"
#include "DiceRngService.h"

// =============================================================================
// TC_AC10 — AC-10：Min==Max 退化调用不推进权威流游标
//
// Given:  固定种子 S=77777，构造两条独立同种子流实例。
// When:   路径 A：SetSeed(S) → RollDice() → 记 seqA（四字段）。
//         路径 B：SetSeed(S) → RandomRange(5,5)（退化）→ RollDice() → 记 seqB（四字段）。
// Then:   seqA 四字段 == seqB 四字段（证明 RandomRange(5,5) 未推进游标）。
//
// 非 vacuous 保证（核心）：
//   若 RandomRange(5,5) 实现误调流推进 Seed（如直通 RandRange(5,5)），
//   则 B 的 RollDice 消耗的是流的第二个位置（A 的 RollDice 位置的下一位）。
//   两条流从同 S 出发：
//     路径 A 的流状态：[初始] → RollDice 消耗 2 次（Die1/Die2）→ 结束
//     路径 B 的流状态：[初始] → RandomRange(5,5) 误消耗 1 次 → RollDice 消耗 2 次
//   B 的 Die1 = A 的 Die2（偏移 1 位），四字段必不全等 → TestEqual FAIL。
//   故本断言是真能 FAIL 的，非同义反复。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomRange_TC_AC10_MinEqualsMax_DoesNotAdvanceCursor,
	"Rento.Dice.RandomRangeDegenerateBoundary.TC_AC10_MinEqualsMax_DoesNotAdvanceCursor",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRandomRange_TC_AC10_MinEqualsMax_DoesNotAdvanceCursor::RunTest(const FString& Parameters)
{
	// 固定种子 S（任意固定值，须与两路径保持一致）
	constexpr int32 S = 77777;

	// ── 路径 A：SetSeed(S) → RollDice() → 记 seqA ──────────────────────────
	UDiceRngService* ServiceA = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-10: 路径 A 应能创建 UDiceRngService"), ServiceA))
	{
		return false;
	}

	ServiceA->SetSeed(S);
	const FDiceRollResult SeqA = ServiceA->RollDice();

	// ── 路径 B：SetSeed(S) → RandomRange(5,5)（退化）→ RollDice() → 记 seqB ─
	UDiceRngService* ServiceB = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-10: 路径 B 应能创建 UDiceRngService"), ServiceB))
	{
		return false;
	}

	ServiceB->SetSeed(S);
	// 退化调用：Min==Max==5，应早返 5，不推进 Seed
	const int32 DegenerateResult = ServiceB->RandomRange(5, 5);
	const FDiceRollResult SeqB = ServiceB->RollDice();

	// THEN-0：退化返回值本身应 == Min（== Max == 5）
	// （顺带验证 AC-10 前提：退化确实返回了正确值）
	TestEqual(
		*FString::Printf(TEXT("AC-10: RandomRange(5,5) 退化返回值应 == 5（实际 %d）"), DegenerateResult),
		DegenerateResult, 5);

	// THEN-1：Die1 字段相等
	// 非 vacuous：若退化误推进流，B.Die1 = A 流的第二次抽取（即 A.Die2），而非 A.Die1 → FAIL
	TestEqual(
		*FString::Printf(TEXT("AC-10: 路径 B 的 RollDice.Die1=%d 应 == 路径 A 的 RollDice.Die1=%d"
		                     "（证 RandomRange(5,5) 未推进游标；若误推进则 B.Die1 落 A 的下一位 → 不等 → FAIL）"),
			SeqB.Die1, SeqA.Die1),
		SeqB.Die1, SeqA.Die1);

	// THEN-2：Die2 字段相等
	TestEqual(
		*FString::Printf(TEXT("AC-10: 路径 B 的 RollDice.Die2=%d 应 == 路径 A 的 RollDice.Die2=%d"),
			SeqB.Die2, SeqA.Die2),
		SeqB.Die2, SeqA.Die2);

	// THEN-3：Total 字段相等（Total==Die1+Die2 不变式保证此断言不 vacuous）
	TestEqual(
		*FString::Printf(TEXT("AC-10: 路径 B 的 RollDice.Total=%d 应 == 路径 A 的 RollDice.Total=%d"),
			SeqB.Total, SeqA.Total),
		SeqB.Total, SeqA.Total);

	// THEN-4：bIsDouble 字段相等
	TestEqual(
		*FString::Printf(TEXT("AC-10: 路径 B 的 RollDice.bIsDouble=%d 应 == 路径 A 的 RollDice.bIsDouble=%d"),
			SeqB.bIsDouble ? 1 : 0, SeqA.bIsDouble ? 1 : 0),
		SeqB.bIsDouble, SeqA.bIsDouble);

	return true;
}

// =============================================================================
// TC_AC11 — AC-11：Min>Max 返回 Min，不 swap，UE_LOG(Error) 触发
//
// Given:  固定种子 42（种子对本条无影响，Min>Max 早返不推进流，随便固定即可）。
//         AddExpectedError("Min>Max", Contains, 1) 预注册捕获 Error 行。
// When:   RandomRange(10, 3)（Min=10 > Max=3）
// Then:   返回值 == 10（= Min，非 3，非 [3,10] 随机）。
//         AddExpectedError 计数满足（UE_LOG Error 行恰被捕获 1 次）。
//
// 非 vacuous 保证（核心）：
//   若实现"好心" swap：实际执行 RandomRange(3, 10)，返回 [3,10] 随机值。
//   固定种子 42 下第一次抽取为确定值，但该值 != 10 的概率为 7/8 > 87%。
//   TestEqual(result, 10) 在 swap 实现下大概率 FAIL（精确值验证，非统计断言）。
//   若实现返回 Max（=3）而非 Min（=10）：TestEqual FAIL。
//   若实现不记 UE_LOG Error：AddExpectedError 未满足 → Automation 框架报 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomRange_TC_AC11_MinGreaterMax_ReturnsMin_NoSwap,
	"Rento.Dice.RandomRangeDegenerateBoundary.TC_AC11_MinGreaterMax_ReturnsMin_NoSwap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRandomRange_TC_AC11_MinGreaterMax_ReturnsMin_NoSwap::RunTest(const FString& Parameters)
{
	// 预注册：捕获 UE_LOG(LogTemp, Error, "...Min>Max...") 的 Error 行
	// AddExpectedError 参数：(子串, 匹配类型, 期望次数)
	// 子串 "Min>Max" 与 DiceRngService.cpp 中 Error 消息逐字对应，Contains 模式匹配。
	// 若实现未记 Error，期望次数 1 未满足 → Automation 框架报 FAIL（防假绿）。
	AddExpectedError(TEXT("Min>Max"), EAutomationExpectedErrorFlags::Contains, 1);

	// GIVEN：构造服务实例，固定种子（Min>Max 早返不推进流，种子对结果无影响）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-11: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(42);

	// WHEN：调用 RandomRange(10, 3)，Min=10 > Max=3，触发第二道门
	const int32 Result = Service->RandomRange(10, 3);

	// THEN：返回值应 == Min（= 10），非 Max（= 3），非 [3,10] 随机
	// 非 vacuous：
	//   若实现 swap → 执行 RandomRange(3, 10) → 返回 [3,10] 随机 ≠ 10（大概率 FAIL）
	//   若实现返回 Max（= 3）→ TestEqual(3, 10) FAIL
	TestEqual(
		*FString::Printf(TEXT("AC-11: RandomRange(10,3) 应返回 Min=10（fail-safe，非 swap 后的随机值，非 Max=3）；实际返回 %d"),
			Result),
		Result, 10);

	return true;
}

// =============================================================================
// TC_AC12b — AC-12b：范围不变式（固定种子，多组 Min/Max，每次结果恒落 [Min,Max]）
//
// Given:  固定种子 S=12345，多组 (Min,Max)：(0,5)/(1,6)/(-3,3)/(10,10)。
// When:   每组 RandomRange(Min,Max) 调用 20 次（>= 20 次，满足 AC 要求）。
// Then:   每次 Min <= result <= Max，无一例外。
//
// Edge 覆盖：
//   (10,10)：Min==Max 退化路径，每次恒返回 10（与 AC-10 退化一致）。
//   (-3,3) ：负区间，验封装正确处理负值 Min/Max（int32 全范围合法）。
//   (0,5)  ：从 0 起的标准区间。
//   (1,6)  ：骰面区间，与 RollDice 内部调用路径相同。
//
// 非 vacuous 保证：
//   若 RandRange 实现有 off-by-one（如返回 Max+1 或 Min-1）：TestTrue FAIL。
//   若 Min==Max 退化路径误返回其他值（如 RandRange(10,10) 意外返回 11）：TestTrue FAIL。
//   若负区间截断错误（如将 -3 截断为 0）：(-3,3) 组 result>=Min 的下界断言对截断 bug
//     不敏感——截断后返回 [0,3]，0>=-3 为 true，下界断言仍 PASS（假绿）。
//     真正捕获截断的断言：循环后验证 bSawNegative==true（至少一次 result<0）；
//     若截断后永返 [0,3]，bSawNegative 恒为 false → TestTrue FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRandomRange_TC_AC12b_ResultAlwaysInRange,
	"Rento.Dice.RandomRangeDegenerateBoundary.TC_AC12b_ResultAlwaysInRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRandomRange_TC_AC12b_ResultAlwaysInRange::RunTest(const FString& Parameters)
{
	// 多组 (Min, Max)（AC-12b 要求覆盖这四组）
	struct FTestCase
	{
		int32 Min;
		int32 Max;
		const TCHAR* Label; // 用于断言消息，方便定位失败组
	};

	const TArray<FTestCase> TestCases = {
		{ 0,   5,  TEXT("(0,5)")  },   // 标准正区间
		{ 1,   6,  TEXT("(1,6)")  },   // 骰面区间
		{ -3,  3,  TEXT("(-3,3)") },   // 负 Min 区间（CaseIdx=2，seed=14345）
		{ 10, 10,  TEXT("(10,10)")},   // Min==Max 退化，恒返回 10
	};

	// 固定基础种子；每组用不同偏移避免相同序列掩盖 range 相关边界问题
	// 种子 = 12345 + 组索引 * 1000（任意固定，保证确定性）
	// (-3,3) 组：CaseIdx=2 → seed=14345
	constexpr int32 BaseSeed = 12345;
	constexpr int32 IterCount = 20; // AC 要求 >= 20 次

	for (int32 CaseIdx = 0; CaseIdx < TestCases.Num(); ++CaseIdx)
	{
		const FTestCase& TC = TestCases[CaseIdx];

		// GIVEN：独立实例 + 固定种子（每组隔离）
		UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
		if (!TestNotNull(
			*FString::Printf(TEXT("AC-12b: 组 %s 应能创建 UDiceRngService"), TC.Label),
			Service))
		{
			return false;
		}

		const int32 Seed = BaseSeed + CaseIdx * 1000;
		Service->SetSeed(Seed);

		// 负 Min 组专用：追踪 20 次中是否出现过 result<0
		// 用途：捕获「将 Min 截断为正」bug——范围断言 result>=Min 对此不敏感
		//   （截断后返回 0，0>=-3 为 true，假绿）；
		//   bSawNegative==true 才是真正验证负 Min 未被截断的断言。
		bool bSawNegative = false;

		// WHEN + THEN：调用 IterCount 次，断言每次结果落 [Min, Max]
		for (int32 i = 0; i < IterCount; ++i)
		{
			const int32 Result = Service->RandomRange(TC.Min, TC.Max);

			// 负 Min 组：累积是否出现过负值（仅对 TC.Min<0 的组有意义）
			if (TC.Min < 0 && Result < 0)
			{
				bSawNegative = true;
			}

			// 下界断言：result >= Min
			// 非 vacuous：若 RandRange 实现有 off-by-one 返回 Min-1，立即 FAIL
			// 注意：对负 Min 截断 bug（如将 -3 截成 0）此断言不敏感（0>=-3 为 true），
			// 截断 bug 由循环后 bSawNegative 断言捕获。
			TestTrue(
				*FString::Printf(TEXT("AC-12b: 组 %s 第 %d 次 result=%d 应 >= Min=%d"),
					TC.Label, i + 1, Result, TC.Min),
				Result >= TC.Min);

			// 上界断言：result <= Max
			// 非 vacuous：若 RandRange 实现有 off-by-one 返回 Max+1，立即 FAIL
			// Edge (10,10)：Max==Min==10，result 必须 == 10，> 10 或 < 10 均 FAIL
			TestTrue(
				*FString::Printf(TEXT("AC-12b: 组 %s 第 %d 次 result=%d 应 <= Max=%d"),
					TC.Label, i + 1, Result, TC.Max),
				Result <= TC.Max);
		}

		// 负 Min 组循环后断言：20 次抽取中应至少出现一次 result<0
		// 非 vacuous：若实现将 Min=-3 截断为 0（即实际执行 RandomRange(0,3)），
		//   则所有 Result in {0,1,2,3}，Result<0 永远为 false，bSawNegative 恒 false
		//   → TestTrue(..., false) → FAIL。
		// 确定性：seed=14345（BaseSeed=12345 + 2*1000）下 [-3,3] 七值含三负，
		//   P(20次全正) = (4/7)^20 ≈ 0.00003%，实践上必然出现负值（实跑确认）。
		if (TC.Min < 0)
		{
			TestTrue(
				*FString::Printf(TEXT(
					"AC-12b: 组 %s 的 20 次抽取应至少出现一次 result<0，"
					"证负 Min 未被截断为正（若实现将 Min 截为 0，永无负数 → FAIL）"),
					TC.Label),
				bSawNegative);
		}
	}

	return true;
}
