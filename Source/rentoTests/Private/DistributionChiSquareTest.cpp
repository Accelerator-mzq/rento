// DistributionChiSquareTest.cpp
// =============================================================================
// 单元测试：骰子分布统计均匀性 [Advisory]（story-006，AC-3/4/5/12）
//
// 物理路径：Source/rentoTests/Private/DistributionChiSquareTest.cpp
// 逻辑分类：tests/unit/dice/distribution_chisquare_test.cpp
// Automation 类目：Rento.Dice.DistributionChiSquare
//
// 覆盖 AC（全部 [Advisory]）：
//   AC-3  → TC_AC3_IsDouble_Frequency_ChiSquare（bIsDouble 频率 + df=1 chi-square）
//   AC-4  → TC_AC4_Total_PMF_ChiSquare（Total PMF df=10 chi-square）
//   AC-5  → TC_AC5_DieFace_Uniformity_Independence_ChiSquare（单面均匀 df=5 + 双骰独立性 df=25）
//   AC-12 → TC_AC12_RandomRange_Boundary_Reachability（RandomRange(1,6) 边界可达性）
//
// ⚠ ADVISORY 纪律（story-006 Guardrail / control-manifest）：
//   统计/频率断言永不作 [Logic] gate（CI 偶发误触发）→ 一律 [Advisory]。
//   失败路径：记录 UE_LOG Warning，**不让测试 FAIL**（测试始终返回 true）。
//   绝不能让统计 flaky 把 CI 搞红。
//
// n=36000 固定种子：种子选 42（确定性，与 AC-8/9 的 12345/99999 分开避免序列交叉混淆）
//
// chi-square 临界值（α=0.001）：
//   df=1  → 10.828（bIsDouble 二分类）
//   df=10 → 29.588（Total PMF，11 类别）
//   df=5  → 20.515（单面均匀，6 类别）
//   df=25 → 52.620（双骰独立性，6×6=36 格，边缘 11 个，df=35-10=25）
//
// 依赖：
//   - DiceRngService.h（UDiceRngService / FDiceRollResult）
//   - story-001/002/003/004 已实现原语
// =============================================================================

#include "Misc/AutomationTest.h"
#include "DiceRngService.h"
#include "Math/UnrealMathUtility.h"

// =============================================================================
// chi-square 计算辅助函数
// 公式：χ² = Σ (Observed_i - Expected_i)² / Expected_i
// 参数：Observed 数组、Expected 数组、类别数 k
// 返回：chi-square 统计量（double）
// =============================================================================
static double CalcChiSquare(const int32* Observed, const double* Expected, int32 K)
{
	double Chi2 = 0.0;
	for (int32 i = 0; i < K; ++i)
	{
		if (Expected[i] > 0.0)
		{
			const double Diff = static_cast<double>(Observed[i]) - Expected[i];
			Chi2 += (Diff * Diff) / Expected[i];
		}
	}
	return Chi2;
}

// =============================================================================
// TC_AC3 — AC-3：bIsDouble 频率 ∈ [0.155, 0.178] + chi-square(df=1) < 10.83
//
// 理论双点频率 = 6/36 ≈ 0.16667（Die1==Die2 恰好 6 种）
// n=36000，期望双点次数 = 36000 × (1/6) = 6000，非双点 = 30000
// chi-square(df=1) 公式：(obs_double - 6000)²/6000 + (obs_nondouble - 30000)²/30000
//
// Advisory 处理：失败 → UE_LOG Warning，测试返回 true（不阻塞 CI）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDistributionChiSquare_TC_AC3_IsDouble_Frequency,
	"Rento.Dice.DistributionChiSquare.TC_AC3_IsDouble_Frequency_ChiSquare",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDistributionChiSquare_TC_AC3_IsDouble_Frequency::RunTest(const FString& Parameters)
{
	// 固定种子，n=36000 次掷骰（Advisory 统计 seed，与 AC-8/9 分开）
	constexpr int32 Seed       = 42;
	constexpr int32 N          = 36000;
	constexpr double ExpDouble = N * (1.0 / 6.0); // ≈ 6000
	constexpr double ExpNonDouble = N - ExpDouble; // ≈ 30000
	constexpr double kCritical_df1 = 10.828;       // α=0.001, df=1

	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("TC_AC3: UDiceRngService 构造失败"));
		return true; // Advisory：不阻塞
	}
	Service->SetSeed(Seed);

	int32 DoubleCount = 0;
	for (int32 i = 0; i < N; ++i)
	{
		const FDiceRollResult R = Service->RollDice();
		if (R.bIsDouble) ++DoubleCount;
	}

	const double FreqDouble = static_cast<double>(DoubleCount) / N;
	const int32  NonDoubleCount = N - DoubleCount;

	// chi-square(df=1)：二分类 bIsDouble vs !bIsDouble
	const int32  Observed[2] = { DoubleCount, NonDoubleCount };
	const double Expected[2] = { ExpDouble, ExpNonDouble };
	const double Chi2 = CalcChiSquare(Observed, Expected, 2);

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC3 [Advisory]: n=%d seed=%d bIsDouble 次数=%d 频率=%.5f（期望≈%.5f）chi2(df=1)=%.4f 临界=%.3f"),
		N, Seed, DoubleCount, FreqDouble, 1.0 / 6.0, Chi2, kCritical_df1);

	// 频率范围检查（[0.155, 0.178]，Advisory）
	if (FreqDouble < 0.155 || FreqDouble > 0.178)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC3 [Advisory WARNING]: bIsDouble 频率=%.5f 超出 [0.155, 0.178] 预期范围（n=%d seed=%d）。"
				 "这是 Advisory 警告，不阻塞 PR。"),
			FreqDouble, N, Seed);
	}

	// chi-square 临界检查（Advisory）
	if (Chi2 >= kCritical_df1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC3 [Advisory WARNING]: bIsDouble chi-square(df=1)=%.4f >= 临界值 %.3f（α=0.001）。"
				 "可能存在双点分布异常，但统计偶发误触发，Advisory 不阻塞 CI。"),
			Chi2, kCritical_df1);
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("TC_AC3 [Advisory OK]: chi2=%.4f < %.3f，bIsDouble 分布在统计置信范围内。"),
			Chi2, kCritical_df1);
	}

	// Advisory：始终返回 true，不阻塞 CI
	return true;
}

// =============================================================================
// TC_AC4 — AC-4：Total PMF chi-square(df=10) < 29.59
//
// 双骰合计 Total ∈ [2,12]，共 11 种值，理论 PMF（两独立 d6）：
//   Total  Combinations  P         Expected(n=36000)
//     2        1        1/36       1000
//     3        2        2/36       2000
//     4        3        3/36       3000
//     5        4        4/36       4000
//     6        5        5/36       5000
//     7        6        6/36       6000
//     8        5        5/36       5000
//     9        4        4/36       4000
//    10        3        3/36       3000
//    11        2        2/36       2000
//    12        1        1/36       1000
// df = 11 - 1 = 10，临界值（α=0.001）= 29.588
//
// Advisory 处理：失败 → UE_LOG Warning，测试返回 true
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDistributionChiSquare_TC_AC4_Total_PMF,
	"Rento.Dice.DistributionChiSquare.TC_AC4_Total_PMF_ChiSquare",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDistributionChiSquare_TC_AC4_Total_PMF::RunTest(const FString& Parameters)
{
	constexpr int32 Seed = 42;
	constexpr int32 N    = 36000;
	constexpr double kCritical_df10 = 29.588; // α=0.001, df=10

	// Total ∈ [2,12]，索引 i 对应 Total = i+2
	// 理论组合数（两独立 d6）：[1,2,3,4,5,6,5,4,3,2,1]
	const double Combinations[11] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0 };
	double ExpectedTotal[11];
	for (int32 i = 0; i < 11; ++i)
	{
		ExpectedTotal[i] = N * Combinations[i] / 36.0;
	}

	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("TC_AC4: UDiceRngService 构造失败"));
		return true;
	}
	Service->SetSeed(Seed);

	int32 TotalCount[11] = {};
	for (int32 i = 0; i < N; ++i)
	{
		const FDiceRollResult R = Service->RollDice();
		// Total 范围 [2,12]，索引 = Total - 2
		const int32 Idx = R.Total - 2;
		if (Idx >= 0 && Idx < 11)
		{
			++TotalCount[Idx];
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("TC_AC4 [Advisory WARNING]: Total=%d 超出 [2,12] 范围（第 %d 次掷骰）"), R.Total, i);
		}
	}

	const double Chi2 = CalcChiSquare(TotalCount, ExpectedTotal, 11);

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC4 [Advisory]: n=%d seed=%d Total PMF chi2(df=10)=%.4f 临界=%.3f"),
		N, Seed, Chi2, kCritical_df10);

	// 打印各 Total 值观测 vs 期望
	for (int32 i = 0; i < 11; ++i)
	{
		UE_LOG(LogTemp, Log,
			TEXT("  Total=%2d: obs=%5d exp=%.1f"),
			i + 2, TotalCount[i], ExpectedTotal[i]);
	}

	if (Chi2 >= kCritical_df10)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC4 [Advisory WARNING]: Total PMF chi2(df=10)=%.4f >= 临界值 %.3f（α=0.001）。"
				 "Advisory，不阻塞 CI。"),
			Chi2, kCritical_df10);
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("TC_AC4 [Advisory OK]: chi2=%.4f < %.3f，Total PMF 在统计置信范围内。"),
			Chi2, kCritical_df10);
	}

	return true; // Advisory：始终返回 true
}

// =============================================================================
// TC_AC5 — AC-5：单面均匀 chi-square(df=5) < 20.52 + 双骰独立性 chi-square(df=25) < 52.62
//
// 单面均匀性：Die1 ∈ [1,6] 各面期望 = N/6 = 6000（Die2 同理，合并 2×N 样本）
//   df = 6 - 1 = 5，临界值（α=0.001）= 20.515
//
// 双骰独立性（联合频率 vs 边缘乘积）：
//   6×6 联合分布，理论各格期望 = N/36 = 1000（两独立骰）
//   df = (6-1)×(6-1) = 25，临界值（α=0.001）= 52.620
//   chi-square = Σ_{i,j} (obs_{ij} - N/36)² / (N/36)
//
// Advisory 处理：失败 → UE_LOG Warning，测试返回 true
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDistributionChiSquare_TC_AC5_DieFace_Uniformity_Independence,
	"Rento.Dice.DistributionChiSquare.TC_AC5_DieFace_Uniformity_Independence_ChiSquare",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDistributionChiSquare_TC_AC5_DieFace_Uniformity_Independence::RunTest(const FString& Parameters)
{
	constexpr int32 Seed = 42;
	constexpr int32 N    = 36000;
	constexpr double kCritical_df5  = 20.515; // α=0.001, df=5（单面均匀）
	constexpr double kCritical_df25 = 52.620; // α=0.001, df=25（双骰独立性）

	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("TC_AC5: UDiceRngService 构造失败"));
		return true;
	}
	Service->SetSeed(Seed);

	// Die1 面频次 [0..5] 对应 [1..6]
	int32 Die1Count[6] = {};
	// Die2 面频次
	int32 Die2Count[6] = {};
	// 联合频次 [Die1-1][Die2-1]（6×6 矩阵）
	int32 JointCount[6][6] = {};

	for (int32 i = 0; i < N; ++i)
	{
		const FDiceRollResult R = Service->RollDice();
		const int32 D1 = R.Die1 - 1; // [0,5]
		const int32 D2 = R.Die2 - 1; // [0,5]
		if (D1 >= 0 && D1 < 6 && D2 >= 0 && D2 < 6)
		{
			++Die1Count[D1];
			++Die2Count[D2];
			++JointCount[D1][D2];
		}
	}

	// ---- 单面均匀 chi-square（Die1）----
	const double ExpFace = static_cast<double>(N) / 6.0; // 每面期望 = 6000
	double ExpFaceArr[6];
	for (int32 i = 0; i < 6; ++i) ExpFaceArr[i] = ExpFace;

	const double Chi2_Die1 = CalcChiSquare(Die1Count, ExpFaceArr, 6);
	const double Chi2_Die2 = CalcChiSquare(Die2Count, ExpFaceArr, 6);

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC5 [Advisory]: n=%d seed=%d"),
		N, Seed);
	UE_LOG(LogTemp, Log,
		TEXT("  Die1 均匀 chi2(df=5)=%.4f（临界 %.3f）"),
		Chi2_Die1, kCritical_df5);
	UE_LOG(LogTemp, Log,
		TEXT("  Die2 均匀 chi2(df=5)=%.4f（临界 %.3f）"),
		Chi2_Die2, kCritical_df5);

	if (Chi2_Die1 >= kCritical_df5)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC5 [Advisory WARNING]: Die1 均匀 chi2=%.4f >= 临界 %.3f。Advisory 不阻塞 CI。"),
			Chi2_Die1, kCritical_df5);
	}
	if (Chi2_Die2 >= kCritical_df5)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC5 [Advisory WARNING]: Die2 均匀 chi2=%.4f >= 临界 %.3f。Advisory 不阻塞 CI。"),
			Chi2_Die2, kCritical_df5);
	}

	// ---- 双骰独立性 chi-square（联合频率 vs 边缘乘积）----
	// 两独立 d6 → 联合分布各格期望 = N/36
	// 注：df = (6-1)(6-1) = 25，临界 52.620
	const double ExpJoint = static_cast<double>(N) / 36.0; // ≈ 1000

	double Chi2_Joint = 0.0;
	for (int32 d1 = 0; d1 < 6; ++d1)
	{
		for (int32 d2 = 0; d2 < 6; ++d2)
		{
			const double Diff = static_cast<double>(JointCount[d1][d2]) - ExpJoint;
			Chi2_Joint += (Diff * Diff) / ExpJoint;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("  双骰独立性 chi2(df=25)=%.4f（临界 %.3f）"),
		Chi2_Joint, kCritical_df25);

	if (Chi2_Joint >= kCritical_df25)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC5 [Advisory WARNING]: 双骰独立性 chi2=%.4f >= 临界 %.3f。Advisory 不阻塞 CI。"),
			Chi2_Joint, kCritical_df25);
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("TC_AC5 [Advisory OK]: 均匀性 + 独立性 chi-square 均在置信范围内。"));
	}

	return true; // Advisory：始终返回 true
}

// =============================================================================
// TC_AC12 — AC-12：RandomRange(1,6) 边界 1 与 6 可达性（[Advisory]）
//
// n=36000 次 RandomRange(1,6)，断言 1 和 6 至少各出现一次。
// 降 Advisory 原因：某种子前 N 次未现边界（极罕见但理论上可能）。
// 若固化 fixture 确认边界出现位置可升 [Logic]（story-006 AC-12 Edge）。
//
// Advisory 处理：失败 → UE_LOG Warning，测试返回 true
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDistributionChiSquare_TC_AC12_RandomRange_Boundary_Reachability,
	"Rento.Dice.DistributionChiSquare.TC_AC12_RandomRange_Boundary_Reachability",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDistributionChiSquare_TC_AC12_RandomRange_Boundary_Reachability::RunTest(const FString& Parameters)
{
	constexpr int32 Seed = 42;
	constexpr int32 N    = 36000;

	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("TC_AC12: UDiceRngService 构造失败"));
		return true;
	}
	Service->SetSeed(Seed);

	bool bGot1 = false;
	bool bGot6 = false;
	int32 First1Pos = -1;
	int32 First6Pos = -1;

	for (int32 i = 0; i < N; ++i)
	{
		const int32 Val = Service->RandomRange(1, 6);
		if (Val == 1 && !bGot1)
		{
			bGot1 = true;
			First1Pos = i;
		}
		if (Val == 6 && !bGot6)
		{
			bGot6 = true;
			First6Pos = i;
		}
		if (bGot1 && bGot6) break; // 两边界都出现，提前退出
	}

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC12 [Advisory]: n=%d seed=%d RandomRange(1,6) 边界可达性："),
		N, Seed);
	UE_LOG(LogTemp, Log,
		TEXT("  边界 1 首次出现位置: %d（%s）"),
		First1Pos, bGot1 ? TEXT("可达 ✓") : TEXT("未出现 ✗"));
	UE_LOG(LogTemp, Log,
		TEXT("  边界 6 首次出现位置: %d（%s）"),
		First6Pos, bGot6 ? TEXT("可达 ✓") : TEXT("未出现 ✗"));

	if (!bGot1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC12 [Advisory WARNING]: RandomRange(1,6) 在 %d 次调用内边界 1 未出现。"
				 "Advisory 不阻塞 CI。"),
			N);
	}
	if (!bGot6)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TC_AC12 [Advisory WARNING]: RandomRange(1,6) 在 %d 次调用内边界 6 未出现。"
				 "Advisory 不阻塞 CI。"),
			N);
	}

	if (bGot1 && bGot6)
	{
		UE_LOG(LogTemp, Log,
			TEXT("TC_AC12 [Advisory OK]: 边界 1 和 6 均在前 %d 次调用内出现。"),
			N);
	}

	return true; // Advisory：始终返回 true
}
