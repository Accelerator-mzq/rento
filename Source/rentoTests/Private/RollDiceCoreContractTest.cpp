// RollDiceCoreContractTest.cpp
// =============================================================================
// 单元测试：RollDice 核心契约 + 执行序铁律（story-002）
//
// 物理路径：Source/rentoTests/Private/RollDiceCoreContractTest.cpp
// 逻辑分类：tests/unit/dice/rolldice_core_contract_test.cpp
// Automation 类目：Rento.Dice.RollDiceCoreContract
//
// 覆盖 AC：
//   AC-1   → TC_AC1_SingleRoll_FieldIntegrityAndInvariant
//   AC-2   → TC_AC2_TwentyRolls_InvariantAlwaysHolds
//   AC-6   → TC_AC6_TwoIndependentDraws_NotTotalSplit
//   AC-7   → TC_AC7_DieRange_AlwaysInBounds（复用 AC-2 序列）
//   AC-16c → TC_AC16c_BroadcastAndReturn_SameSource
//
// 测试设计原则：
//   - 确定性：全部使用固定种子，无时间 / IO / 随机依赖
//   - NewObject<UDiceRngService>(GetTransientPackage()) —— headless 安全
//   - 每条断言均有对应的「非 vacuous」注释（解释该断言能 FAIL 的具体条件）
//   - Out of Scope 严守：不测 story-001 原语（SetSeed/RandomRange/RandomFloat01）、
//     不测退化契约（story-003）、不测触发次数/重入禁止（story-005）、
//     不测确定性长序列 fixture（story-006）
//
// 依赖：
//   - DiceRngService.h（FDiceRollResult、UDiceRngService、OnDiceRolled）
//   - DiceRollTestSubscriber.h（AC-16c 订阅桩）
//   - Math/RandomStream.h（AC-6 参考流构造）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "DiceRngService.h"
#include "DiceRollTestSubscriber.h"

// =============================================================================
// TC_AC1 — AC-1：单次掷骰字段完整性 + payload 不变式
//
// Given:  SetSeed(12345)
// When:   RollDice() 一次
// Then:   Die1∈[1,6] ∧ Die2∈[1,6] ∧ Total==Die1+Die2 ∧ bIsDouble==(Die1==Die2)
//
// Edge：任一字段越界或不变式破裂 = FAIL（封装 bug）
//
// 非 vacuous 保证：
//   - Die1/Die2∈[1,6]：若 RandRange(1,6) 返回 0 或 7，TestTrue 立即 FAIL
//   - Total==Die1+Die2：若实现里 Total = Die1 * Die2（乘法 bug），FAIL
//   - bIsDouble==(Die1==Die2)：若实现里 bIsDouble 恒 false，当 Die1==Die2 时 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRollDice_TC_AC1_SingleRoll_FieldIntegrityAndInvariant,
	"Rento.Dice.RollDiceCoreContract.TC_AC1_SingleRoll_FieldIntegrityAndInvariant",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRollDice_TC_AC1_SingleRoll_FieldIntegrityAndInvariant::RunTest(const FString& Parameters)
{
	// GIVEN：固定种子 12345（ADR-0004 确定性，任意固定值即可）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-1: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(12345);

	// WHEN：RollDice() 一次
	const FDiceRollResult Result = Service->RollDice();

	// THEN-1：Die1 ∈ [1, 6]
	// 非 vacuous：若 RandRange(1,6) 实现有 bug 返回 0 或 7，TestTrue 立即 FAIL
	TestTrue(
		*FString::Printf(TEXT("AC-1: Die1=%d 应 >= 1"), Result.Die1),
		Result.Die1 >= 1);
	TestTrue(
		*FString::Printf(TEXT("AC-1: Die1=%d 应 <= 6"), Result.Die1),
		Result.Die1 <= 6);

	// THEN-2：Die2 ∈ [1, 6]
	TestTrue(
		*FString::Printf(TEXT("AC-1: Die2=%d 应 >= 1"), Result.Die2),
		Result.Die2 >= 1);
	TestTrue(
		*FString::Printf(TEXT("AC-1: Die2=%d 应 <= 6"), Result.Die2),
		Result.Die2 <= 6);

	// THEN-3：Total == Die1 + Die2（构造时不变式）
	// 非 vacuous：若实现里 Total = Die1 * Die2，或与 Die1/Die2 不同步，此处 FAIL
	const int32 ExpectedTotal = Result.Die1 + Result.Die2;
	TestEqual(
		*FString::Printf(TEXT("AC-1: Total=%d 应 == Die1+Die2=%d（payload 不变式）"),
			Result.Total, ExpectedTotal),
		Result.Total, ExpectedTotal);

	// THEN-4：bIsDouble == (Die1 == Die2)（构造时不变式）
	// 非 vacuous：若实现里 bIsDouble 恒 false，当 Die1==Die2 时此处 FAIL
	const bool bExpectedDouble = (Result.Die1 == Result.Die2);
	TestEqual(
		*FString::Printf(TEXT("AC-1: bIsDouble=%d 应 == (Die1==Die2)=%d（不变式）"),
			Result.bIsDouble ? 1 : 0, bExpectedDouble ? 1 : 0),
		Result.bIsDouble, bExpectedDouble);

	return true;
}

// =============================================================================
// TC_AC2 — AC-2：20 次连续掷骰，不变式覆盖整序列
//
// Given:  SetSeed(12345)
// When:   连续 RollDice() 20 次
// Then:   每次 Total==Die1+Die2 ∧ bIsDouble==(Die1==Die2)，无一例外
// Edge：  序列中至少出现一次 bIsDouble==true + 多次 false（覆盖双点分支，非 vacuous）
//
// 非 vacuous 保证：
//   - 不变式：若任意一次 Total != Die1+Die2，TestEqual 立即 FAIL
//   - bIsDouble Edge：固定种子 12345 下 20 次掷骰，统计上极大概率含双点；
//     若实现里 bIsDouble 恒 false（即使 Die1==Die2 时），bFoundDouble 永远 false → FAIL
//   - bFoundNonDouble：若实现里 bIsDouble 恒 true（bug），TestTrue(bFoundNonDouble) FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRollDice_TC_AC2_TwentyRolls_InvariantAlwaysHolds,
	"Rento.Dice.RollDiceCoreContract.TC_AC2_TwentyRolls_InvariantAlwaysHolds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRollDice_TC_AC2_TwentyRolls_InvariantAlwaysHolds::RunTest(const FString& Parameters)
{
	// GIVEN：固定种子，保证确定性序列
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-2: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(12345);

	constexpr int32 RollCount = 20;
	bool bFoundDouble    = false; // 至少一次 bIsDouble==true（覆盖双点分支）
	bool bFoundNonDouble = false; // 至少一次 bIsDouble==false（覆盖非双点分支）

	// WHEN：连续掷骰 20 次，逐次断言不变式
	for (int32 i = 0; i < RollCount; ++i)
	{
		const FDiceRollResult R = Service->RollDice();

		// THEN-1：Total == Die1 + Die2（不变式逐次验证）
		// 非 vacuous：若任意一次 Total != Die1+Die2，立即 FAIL
		const int32 ExpectedTotal = R.Die1 + R.Die2;
		TestEqual(
			*FString::Printf(TEXT("AC-2: 第 %d 次 Total=%d 应 == Die1(%d)+Die2(%d)=%d"),
				i + 1, R.Total, R.Die1, R.Die2, ExpectedTotal),
			R.Total, ExpectedTotal);

		// THEN-2：bIsDouble == (Die1 == Die2)（不变式逐次验证）
		const bool bExpectedDouble = (R.Die1 == R.Die2);
		TestEqual(
			*FString::Printf(TEXT("AC-2: 第 %d 次 bIsDouble=%d 应 == (Die1==Die2)=%d"),
				i + 1, R.bIsDouble ? 1 : 0, bExpectedDouble ? 1 : 0),
			R.bIsDouble, bExpectedDouble);

		// 收集双点/非双点出现情况（Edge 断言用）
		if (R.bIsDouble)
		{
			bFoundDouble = true;
		}
		else
		{
			bFoundNonDouble = true;
		}
	}

	// THEN-3 Edge：序列中至少出现一次 bIsDouble==true（覆盖双点分支）
	// 非 vacuous：若 bIsDouble 实现恒 false（即使 Die1==Die2），此处 FAIL
	// 说明：种子 12345 的 20 次 d6×d6 序列，从概率上（6/36≈16.7% 双点）几乎必然含双点；
	// 若测试环境恰好不含双点（极罕见），可换种子——但固定 12345 已经验证为包含双点。
	TestTrue(
		TEXT("AC-2 Edge: 20 次掷骰序列中应至少出现一次 bIsDouble==true（验双点分支覆盖）"),
		bFoundDouble);

	// THEN-4 Edge：序列中至少出现多次 bIsDouble==false
	// 非 vacuous：若 bIsDouble 实现恒 true（bug），此处 FAIL
	TestTrue(
		TEXT("AC-2 Edge: 20 次掷骰序列中应至少出现一次 bIsDouble==false（验非双点分支覆盖）"),
		bFoundNonDouble);

	return true;
}

// =============================================================================
// TC_AC6 — AC-6：两次独立顺序抽取，非「先取 Total 再拆解」
//
// Given:  固定种子 S，新建参考流 Ref(S) 直接展开两次 RandRange(1,6)
//         得 expected_d1 / expected_d2
//         相同 S 重建服务实例（同 S、同 0 起点、抽 Die1 前零额外抽取）调 RollDice()
// When:   RollDice() 返回
// Then:   Die1==expected_d1 ∧ Die2==expected_d2
//
// Edge（D-2 前提）：
//   两路径须从同一 S 同一起点出发、RollDice 抽 Die1 前零额外抽取——
//   fixture 构建时显式保证（新建流、不复用已用过的流），否则游标错位使比对失效。
//
// 非 vacuous 保证（核心）：
//   若实现是「先取 Total（一次 RandRange(2,12)）再拆解」：
//     RandRange(2,12) 消耗的流状态与 RandRange(1,6)×2 不同 → Die1/Die2 不等 expected → FAIL
//   若实现「先抽 Die2 再抽 Die1」（顺序颠倒）：
//     Die1 != expected_d1（expected_d1 是流的第一次输出） → FAIL
//   若实现在 RollDice 内部抽 Die1 前额外消耗了流（如校验抽取）：
//     游标偏移 → Die1 != expected_d1 → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRollDice_TC_AC6_TwoIndependentDraws_NotTotalSplit,
	"Rento.Dice.RollDiceCoreContract.TC_AC6_TwoIndependentDraws_NotTotalSplit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRollDice_TC_AC6_TwoIndependentDraws_NotTotalSplit::RunTest(const FString& Parameters)
{
	// 固定种子 S（任意值，须与服务 SetSeed 使用同一值）
	constexpr int32 S = 99999;

	// ---- 参考路径：新建 FRandomStream(S)，直接展开两次 RandRange(1,6) ----
	// 游标对齐前提：同 S、同 0 起点（Initialize(S) 重置游标到 0）、零额外抽取。
	// FRandomStream::Initialize(S) 等价于 SetSeed(S)，重置到确定性起点。
	// RandRange(1,6) 闭区间，与 UDiceRngService::RandomRange(1,6) 底层同调用。
	// （引擎事实①：spike CONFIRMED，RandRange 走 FRand() 浮点中介，pre-5.3 稳定）
	FRandomStream RefStream;
	RefStream.Initialize(S); // 同 UDiceRngService::SetSeed(S) → AuthoritativeStream.Initialize(S)
	const int32 ExpectedD1 = RefStream.RandRange(1, 6); // 流第一次输出
	const int32 ExpectedD2 = RefStream.RandRange(1, 6); // 流第二次输出

	// ---- 服务路径：新建服务实例，SetSeed(S)，RollDice() ----
	// 关键：SetSeed(S) 后到 RollDice() 内抽 Die1 之间，零额外抽取。
	// story-001 实现中 SetSeed 调 Initialize(S) 不做任何抽取，满足此前提。
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-6: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(S); // 与 RefStream.Initialize(S) 完全等价
	const FDiceRollResult Result = Service->RollDice();

	// THEN：Die1 == expected_d1（参考流第一次输出）
	// 非 vacuous：若实现顺序颠倒（先抽 Die2 再抽 Die1），Die1 != expected_d1 → FAIL
	TestEqual(
		*FString::Printf(TEXT("AC-6: Die1=%d 应 == 参考流第一次 RandRange(1,6)=%d（游标对齐前提：同 S=%d、零额外抽取）"),
			Result.Die1, ExpectedD1, S),
		Result.Die1, ExpectedD1);

	// THEN：Die2 == expected_d2（参考流第二次输出）
	// 非 vacuous：若实现是「先取 Total 再拆解」，Die2 != expected_d2 → FAIL
	TestEqual(
		*FString::Printf(TEXT("AC-6: Die2=%d 应 == 参考流第二次 RandRange(1,6)=%d（两次独立顺序抽取证明）"),
			Result.Die2, ExpectedD2, S),
		Result.Die2, ExpectedD2);

	return true;
}

// =============================================================================
// TC_AC7 — AC-7：Die1/Die2 范围确定性覆盖（复用 AC-2 序列）
//
// Given:  SetSeed(12345)（与 AC-2 相同）
// When:   连续 RollDice() 20 次，遍历每次结果
// Then:   Die1∈[1,6] ∧ Die2∈[1,6] 恒成立
//
// 说明：本条只验范围确定性，「均匀性」属统计层（AC-3~5，归 story-006）。
//
// 非 vacuous 保证：
//   - 若 RandRange(1,6) 实现有 off-by-one（返回 0 或 7），TestTrue 立即 FAIL
//   - 范围断言在 20 次连续掷骰中不退化为空集（确保至少扫到不同值）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRollDice_TC_AC7_DieRange_AlwaysInBounds,
	"Rento.Dice.RollDiceCoreContract.TC_AC7_DieRange_AlwaysInBounds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRollDice_TC_AC7_DieRange_AlwaysInBounds::RunTest(const FString& Parameters)
{
	// GIVEN：固定种子（与 AC-2 同，复用确定性序列）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-7: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(12345);

	constexpr int32 RollCount = 20;

	// WHEN + THEN：遍历 20 次，逐次断言范围
	for (int32 i = 0; i < RollCount; ++i)
	{
		const FDiceRollResult R = Service->RollDice();

		// Die1 ∈ [1, 6]
		// 非 vacuous：若 RandRange(1,6) 返回 0（off-by-one），Die1 >= 1 → FAIL
		TestTrue(
			*FString::Printf(TEXT("AC-7: 第 %d 次 Die1=%d 应 >= 1"), i + 1, R.Die1),
			R.Die1 >= 1);
		TestTrue(
			*FString::Printf(TEXT("AC-7: 第 %d 次 Die1=%d 应 <= 6"), i + 1, R.Die1),
			R.Die1 <= 6);

		// Die2 ∈ [1, 6]
		TestTrue(
			*FString::Printf(TEXT("AC-7: 第 %d 次 Die2=%d 应 >= 1"), i + 1, R.Die2),
			R.Die2 >= 1);
		TestTrue(
			*FString::Printf(TEXT("AC-7: 第 %d 次 Die2=%d 应 <= 6"), i + 1, R.Die2),
			R.Die2 <= 6);
	}

	return true;
}

// =============================================================================
// TC_AC16c — AC-16c：广播与返回同源（执行序铁律④的可测证明）
//
// Given:  订阅 OnDiceRolled 的回调捕获 payload 到局部变量；SetSeed(12345)
// When:   RollDice() 返回
// Then:   返回值四字段 == 回调捕获 payload 四字段
//
// Edge（核心）：
//   若实现「广播后重读流返回」（即 ③ 广播后再调一次 RandRange），
//   返回值比 payload 多消耗 Seed → 四字段不等 → TestEqual FAIL。
//   这是验证「执行序铁律④：返回同一固化值（非广播后重读流）」的唯一可测方法。
//
// 订阅者说明：
//   AddDynamic 要求回调方法为 UFUNCTION + UObject 宿主。
//   UDiceRollTestSubscriber（DiceRollTestSubscriber.h）提供最小桩，
//   回调内仅赋值不调任何 RNG API（ADR-0004 重入禁止）。
//
// 非 vacuous 保证：
//   若 bReceived == false（广播未触发）→ 捕获值为默认 0 → Die1/Die2/Total 对比可能偶然相等
//   → 须先断言 bReceived==true（防假绿）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRollDice_TC_AC16c_BroadcastAndReturn_SameSource,
	"Rento.Dice.RollDiceCoreContract.TC_AC16c_BroadcastAndReturn_SameSource",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRollDice_TC_AC16c_BroadcastAndReturn_SameSource::RunTest(const FString& Parameters)
{
	// GIVEN-1：构造服务实例
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-16c: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	// GIVEN-2：构造订阅桩（UObject 宿主，AddDynamic 要求）
	UDiceRollTestSubscriber* Subscriber = NewObject<UDiceRollTestSubscriber>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-16c: UDiceRollTestSubscriber 应能通过 NewObject<> 构造"), Subscriber))
	{
		return false;
	}

	// GIVEN-3：订阅 OnDiceRolled（dynamic multicast 须 AddDynamic）
	Service->OnDiceRolled.AddDynamic(Subscriber, &UDiceRollTestSubscriber::OnRollReceived);

	// GIVEN-4：固定种子
	Service->SetSeed(12345);

	// WHEN：调用 RollDice()
	const FDiceRollResult ReturnedResult = Service->RollDice();

	// THEN-0（防 vacuous）：订阅者必须已收到广播
	// 若广播未触发（实现省略③步骤），bReceived==false，后续字段对比可能偶然假绿
	// → 必须先断言 bReceived 保证后续比较有意义
	if (!TestTrue(TEXT("AC-16c: 订阅者回调应已被触发（OnDiceRolled 广播应在 RollDice 内发生）"),
		Subscriber->bReceived))
	{
		return false;
	}

	// THEN-1：Die1 字段同源
	// 非 vacuous：若实现「广播后重读流」，ReturnedResult.Die1 != Subscriber->CapturedResult.Die1 → FAIL
	TestEqual(
		*FString::Printf(TEXT("AC-16c: 返回值 Die1=%d 应 == 广播 payload Die1=%d（同一固化值，非广播后重读流）"),
			ReturnedResult.Die1, Subscriber->CapturedResult.Die1),
		ReturnedResult.Die1, Subscriber->CapturedResult.Die1);

	// THEN-2：Die2 字段同源
	TestEqual(
		*FString::Printf(TEXT("AC-16c: 返回值 Die2=%d 应 == 广播 payload Die2=%d"),
			ReturnedResult.Die2, Subscriber->CapturedResult.Die2),
		ReturnedResult.Die2, Subscriber->CapturedResult.Die2);

	// THEN-3：Total 字段同源
	TestEqual(
		*FString::Printf(TEXT("AC-16c: 返回值 Total=%d 应 == 广播 payload Total=%d"),
			ReturnedResult.Total, Subscriber->CapturedResult.Total),
		ReturnedResult.Total, Subscriber->CapturedResult.Total);

	// THEN-4：bIsDouble 字段同源
	TestEqual(
		*FString::Printf(TEXT("AC-16c: 返回值 bIsDouble=%d 应 == 广播 payload bIsDouble=%d"),
			ReturnedResult.bIsDouble ? 1 : 0,
			Subscriber->CapturedResult.bIsDouble ? 1 : 0),
		ReturnedResult.bIsDouble, Subscriber->CapturedResult.bIsDouble);

	// 测试结束后解绑（防野绑定，ADR-0001 / ADR-0003 Deinit 纪律；headless 测试内显式清理）
	Service->OnDiceRolled.RemoveDynamic(Subscriber, &UDiceRollTestSubscriber::OnRollReceived);

	return true;
}
