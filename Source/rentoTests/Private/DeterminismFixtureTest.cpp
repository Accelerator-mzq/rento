// DeterminismFixtureTest.cpp
// =============================================================================
// 单元测试：确定性种子序列 fixture（story-006，AC-8/9）— [Logic] PR-gate
//
// 物理路径：Source/rentoTests/Private/DeterminismFixtureTest.cpp
// 逻辑分类：tests/unit/dice/determinism_fixture_test.cpp
// Automation 类目：Rento.Dice.DeterminismFixture
//
// 覆盖 AC：
//   AC-8  → TC_AC8_FixedSeed_TwoColdStartMatches（[Logic] 核心 PR gate）
//   AC-9  → TC_AC9_CrossSeed_TwoGroupsMatchFixedConstants（[Logic] 跨种子固化）
//
// ⚠ expected[] 常量构建规程（story-006 Implementation Notes）：
//   Phase-1：CAPTURE 宏定义时，测试只打印真实值不做断言，用于首次捕获。
//   Phase-2：关闭 CAPTURE 宏，填入打印的真实值为 expected[]，测试变为 PR gate。
//
// 当前状态：Phase-2 PR gate 模式。expected[] 常量已从引擎真实运行捕获（2026-06-08）。
//
// 调用序列说明（≥20 次 RNG 抽取）：
//   RollDice × 5   → 5 次调用 × 2 次抽取/次 = 10 次 RNG 抽取
//   RandomRange(0,5) × 3   → 3 次抽取
//   RandomFloat01 × 2      → 2 次抽取
//   RandomRange(1,6) × 5   → 5 次抽取
//   合计 = 20 次 RNG 抽取（满足 ≥20 要求）
//
// 注意：RandomRange 的 Min/Max 全部用编译期常量（退化值 fixture 稳定性，ADR-0004 F-4④）
//
// 依赖：
//   - DiceRngService.h（UDiceRngService / FDiceRollResult）
//   - story-001/002/003/004 已实现原语（本 story 只组合调用）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "DiceRngService.h"

// =============================================================================
// CAPTURE_MODE：1=打印真实值（Phase-1 捕获），0=PR gate 精确断言（Phase-2）
// 2026-06-08 Phase-1 捕获完成，切换到 Phase-2。
// ⚠ 必须显式 #define 为 0（非注释删除）——UE 开 /we4668：`#if` 引用未定义宏=编译错。
// =============================================================================
#define CAPTURE_MODE 0   // Phase-1 已完成；改回 1 可重新进入捕获模式

// =============================================================================
// 辅助结构：存储单次 RollDice 结果的四字段（用于 fixture 比较）
// =============================================================================
struct FFixtureDiceEntry
{
	int32 Die1;
	int32 Die2;
	int32 Total;
	bool  bIsDouble;
};

// =============================================================================
// TC_AC8 — AC-8：固定种子 12345，两次冷启动，逐字段精确相等 [Logic] PR-gate
//
// 调用序列（总 ≥20 次 RNG 抽取）：
//   [RollDice×5, RandomRange(0,5)×3, RandomFloat01×2, RandomRange(1,6)×5]
//
// Phase-1 (CAPTURE_MODE)：打印真实值，不做断言，用于捕获 expected[]。
// Phase-2 (no CAPTURE_MODE)：断言两次冷启动结果 == expected[]。
//
// 非 vacuous 保证：
//   - 若 FRandomStream API 内部实现变更（如 5.7+ 改整数路径），序列漂移 → FAIL
//   - 若 SetSeed 实现有 bug（不真正重置流），两次冷启动结果不同，且与 expected 不等 → FAIL
//   - float bit-exact 比较：RandomFloat01 用 == 直接比较（同种子同实现 bit 完全一致）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeterminismFixture_TC_AC8_FixedSeed_TwoColdStartMatches,
	"Rento.Dice.DeterminismFixture.TC_AC8_FixedSeed_TwoColdStartMatches",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeterminismFixture_TC_AC8_FixedSeed_TwoColdStartMatches::RunTest(const FString& Parameters)
{
	// ---- Phase-2 expected[] 常量（Phase-1 CAPTURE 后填入）----
	// 格式：{Die1, Die2, Total, bIsDouble}，共 5 条对应 RollDice×5
	// RandomRange 结果（int32），共 8 条：3×RandomRange(0,5) + 5×RandomRange(1,6)
	// RandomFloat01 结果（float），共 2 条
	//
	// 真实捕获值（2026-06-08，seed=12345，UE5.7 Win64 Development，引擎运行输出）：
	// RollDice×5 序列（每次消耗 2 次 RNG 抽取）：
	//   [0]={Die1=3, Die2=4, Total=7,  bIsDouble=false}
	//   [1]={Die1=4, Die2=6, Total=10, bIsDouble=false}
	//   [2]={Die1=1, Die2=4, Total=5,  bIsDouble=false}
	//   [3]={Die1=4, Die2=4, Total=8,  bIsDouble=true }
	//   [4]={Die1=2, Die2=2, Total=4,  bIsDouble=true }
	// RandomRange(0,5)×3：{0, 1, 3}
	// RandomFloat01×2：{0.1792080402 (0x3E378250), 0.6919496059 (0x3F31239C)}
	// RandomRange(1,6)×5：{6, 4, 1, 5, 1}
	// 合计：10+3+2+5 = 20 次 RNG 抽取，满足 ≥20 要求
#if !CAPTURE_MODE
	// RollDice×5 期望序列（Die1, Die2, Total, bIsDouble）
	static constexpr FFixtureDiceEntry kExpectedRolls[5] = {
		{ 3, 4,  7, false },  // Roll[0]：Die1=3, Die2=4（10th,11th 抽取前各1次）
		{ 4, 6, 10, false },  // Roll[1]
		{ 1, 4,  5, false },  // Roll[2]
		{ 4, 4,  8, true  },  // Roll[3]：bIsDouble=true（Die1==Die2==4）
		{ 2, 2,  4, true  },  // Roll[4]：bIsDouble=true（Die1==Die2==2）
	};
	// RandomRange(0,5)×3 期望序列（Min=0, Max=5，编译期常量保证退化稳定）
	static constexpr int32 kExpectedRange05[3] = { 0, 1, 3 };
	// RandomFloat01×2 期望序列（bit-exact float，从引擎 hex 位捕获）
	// 0x3E378250 = 0.1792080402, 0x3F31239C = 0.6919496059
	static constexpr float kExpectedFloat01[2] = {
		0.1792080402f,  // hex bits 0x3E378250，确定性 bit-exact 比较
		0.6919496059f,  // hex bits 0x3F31239C
	};
	// RandomRange(1,6)×5 期望序列（Min=1, Max=6，编译期常量）
	static constexpr int32 kExpectedRange16[5] = { 6, 4, 1, 5, 1 };
#endif

	// =========================================================================
	// ---- Run-1：第一次冷启动（新 NewObject 实例，SetSeed 重置到确定性起点）----
	// =========================================================================
	UDiceRngService* Service1 = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-8 Run-1: UDiceRngService 应能通过 NewObject<> 构造"), Service1))
	{
		return false;
	}
	Service1->SetSeed(12345);

	// 抽取序列（编译期常量 Min/Max，退化值 fixture 稳定，ADR-0004 F-4④）
	FFixtureDiceEntry Run1Rolls[5];
	for (int32 i = 0; i < 5; ++i)
	{
		const FDiceRollResult R = Service1->RollDice();
		Run1Rolls[i] = { R.Die1, R.Die2, R.Total, R.bIsDouble };
	}

	constexpr int32 kRange05Min = 0;
	constexpr int32 kRange05Max = 5;
	int32 Run1Range05[3];
	Run1Range05[0] = Service1->RandomRange(kRange05Min, kRange05Max);
	Run1Range05[1] = Service1->RandomRange(kRange05Min, kRange05Max);
	Run1Range05[2] = Service1->RandomRange(kRange05Min, kRange05Max);

	float Run1Float01[2];
	Run1Float01[0] = Service1->RandomFloat01();
	Run1Float01[1] = Service1->RandomFloat01();

	constexpr int32 kRange16Min = 1;
	constexpr int32 kRange16Max = 6;
	int32 Run1Range16[5];
	Run1Range16[0] = Service1->RandomRange(kRange16Min, kRange16Max);
	Run1Range16[1] = Service1->RandomRange(kRange16Min, kRange16Max);
	Run1Range16[2] = Service1->RandomRange(kRange16Min, kRange16Max);
	Run1Range16[3] = Service1->RandomRange(kRange16Min, kRange16Max);
	Run1Range16[4] = Service1->RandomRange(kRange16Min, kRange16Max);

	// =========================================================================
	// ---- Run-2：第二次冷启动（全新 NewObject 实例，同 SetSeed(12345)）----
	// =========================================================================
	UDiceRngService* Service2 = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-8 Run-2: UDiceRngService 第二实例应能通过 NewObject<> 构造"), Service2))
	{
		return false;
	}
	Service2->SetSeed(12345);

	FFixtureDiceEntry Run2Rolls[5];
	for (int32 i = 0; i < 5; ++i)
	{
		const FDiceRollResult R = Service2->RollDice();
		Run2Rolls[i] = { R.Die1, R.Die2, R.Total, R.bIsDouble };
	}

	int32 Run2Range05[3];
	Run2Range05[0] = Service2->RandomRange(kRange05Min, kRange05Max);
	Run2Range05[1] = Service2->RandomRange(kRange05Min, kRange05Max);
	Run2Range05[2] = Service2->RandomRange(kRange05Min, kRange05Max);

	float Run2Float01[2];
	Run2Float01[0] = Service2->RandomFloat01();
	Run2Float01[1] = Service2->RandomFloat01();

	int32 Run2Range16[5];
	Run2Range16[0] = Service2->RandomRange(kRange16Min, kRange16Max);
	Run2Range16[1] = Service2->RandomRange(kRange16Min, kRange16Max);
	Run2Range16[2] = Service2->RandomRange(kRange16Min, kRange16Max);
	Run2Range16[3] = Service2->RandomRange(kRange16Min, kRange16Max);
	Run2Range16[4] = Service2->RandomRange(kRange16Min, kRange16Max);

	// =========================================================================
	// ---- CAPTURE_MODE：打印真实值（Phase-1 捕获用）----
	// =========================================================================
#if CAPTURE_MODE
	UE_LOG(LogTemp, Warning,
		TEXT("=== AC-8 CAPTURE MODE: seed=12345 真实序列（复制到 expected[] 常量）==="));

	// 打印 RollDice×5
	for (int32 i = 0; i < 5; ++i)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("  kExpectedRolls[%d] = { %d, %d, %d, %s }"),
			i,
			Run1Rolls[i].Die1,
			Run1Rolls[i].Die2,
			Run1Rolls[i].Total,
			Run1Rolls[i].bIsDouble ? TEXT("true") : TEXT("false"));
	}

	// 打印 RandomRange(0,5)×3
	UE_LOG(LogTemp, Warning,
		TEXT("  kExpectedRange05 = { %d, %d, %d }"),
		Run1Range05[0], Run1Range05[1], Run1Range05[2]);

	// 打印 RandomFloat01×2（用十六进制表示 bit-exact 值，同时打印十进制供参考）
	// bit-exact 捕获：通过 uint32 reinterpret 获取浮点位模式
	uint32 F0Bits, F1Bits;
	FMemory::Memcpy(&F0Bits, &Run1Float01[0], sizeof(float));
	FMemory::Memcpy(&F1Bits, &Run1Float01[1], sizeof(float));
	UE_LOG(LogTemp, Warning,
		TEXT("  kExpectedFloat01[0] = %.10f (hex bits = 0x%08X)"),
		Run1Float01[0], F0Bits);
	UE_LOG(LogTemp, Warning,
		TEXT("  kExpectedFloat01[1] = %.10f (hex bits = 0x%08X)"),
		Run1Float01[1], F1Bits);

	// 打印 RandomRange(1,6)×5
	UE_LOG(LogTemp, Warning,
		TEXT("  kExpectedRange16 = { %d, %d, %d, %d, %d }"),
		Run1Range16[0], Run1Range16[1], Run1Range16[2], Run1Range16[3], Run1Range16[4]);

	// 验证两次冷启动完全一致（逻辑正确性的信心检查，capture 模式也验一致性）
	UE_LOG(LogTemp, Warning,
		TEXT("=== AC-8 CAPTURE: Run-1 vs Run-2 一致性检查 ==="));
	bool bRun1Run2Match = true;
	for (int32 i = 0; i < 5; ++i)
	{
		if (Run1Rolls[i].Die1 != Run2Rolls[i].Die1 ||
			Run1Rolls[i].Die2 != Run2Rolls[i].Die2 ||
			Run1Rolls[i].Total != Run2Rolls[i].Total ||
			Run1Rolls[i].bIsDouble != Run2Rolls[i].bIsDouble)
		{
			bRun1Run2Match = false;
			UE_LOG(LogTemp, Error,
				TEXT("  MISMATCH 第 %d 次 Roll: Run1=(%d,%d,%d,%d) Run2=(%d,%d,%d,%d)"),
				i,
				Run1Rolls[i].Die1, Run1Rolls[i].Die2, Run1Rolls[i].Total, Run1Rolls[i].bIsDouble,
				Run2Rolls[i].Die1, Run2Rolls[i].Die2, Run2Rolls[i].Total, Run2Rolls[i].bIsDouble);
		}
	}
	for (int32 i = 0; i < 3; ++i)
	{
		if (Run1Range05[i] != Run2Range05[i])
		{
			bRun1Run2Match = false;
			UE_LOG(LogTemp, Error,
				TEXT("  MISMATCH Range05[%d]: Run1=%d Run2=%d"), i, Run1Range05[i], Run2Range05[i]);
		}
	}
	if (Run1Float01[0] != Run2Float01[0] || Run1Float01[1] != Run2Float01[1])
	{
		bRun1Run2Match = false;
		UE_LOG(LogTemp, Error,
			TEXT("  MISMATCH Float01: Run1=(%.10f,%.10f) Run2=(%.10f,%.10f)"),
			Run1Float01[0], Run1Float01[1], Run2Float01[0], Run2Float01[1]);
	}
	for (int32 i = 0; i < 5; ++i)
	{
		if (Run1Range16[i] != Run2Range16[i])
		{
			bRun1Run2Match = false;
			UE_LOG(LogTemp, Error,
				TEXT("  MISMATCH Range16[%d]: Run1=%d Run2=%d"), i, Run1Range16[i], Run2Range16[i]);
		}
	}

	if (bRun1Run2Match)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Run-1 == Run-2 PASS：两次冷启动序列完全一致。"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  Run-1 != Run-2 FAIL：两次冷启动序列不一致！确定性实现有问题。"));
		return false;
	}

	// CAPTURE_MODE 下返回 true（不做精确断言，仅打印 + 一致性检查）
	return true;

#else // !CAPTURE_MODE — Phase-2 PR gate 断言

	// =========================================================================
	// ---- Phase-2：逐字段精确断言 == expected[]（两次冷启动各自对照 expected）----
	// =========================================================================

	// ---- Run-1 vs expected[] ----
	for (int32 i = 0; i < 5; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-1: Roll[%d].Die1"), i),
			Run1Rolls[i].Die1, kExpectedRolls[i].Die1);
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-1: Roll[%d].Die2"), i),
			Run1Rolls[i].Die2, kExpectedRolls[i].Die2);
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-1: Roll[%d].Total"), i),
			Run1Rolls[i].Total, kExpectedRolls[i].Total);
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-1: Roll[%d].bIsDouble"), i),
			Run1Rolls[i].bIsDouble, kExpectedRolls[i].bIsDouble);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-1: Range05[%d]"), i),
			Run1Range05[i], kExpectedRange05[i]);
	}
	// float bit-exact 比较（== 操作符，确定性要 bit 级，不用 IsNearlyEqual）
	// 同种子同实现下 bit 完全一致，任何漂移都应被检测到
	TestTrue(
		*FString::Printf(TEXT("AC-8 Run-1: Float01[0]=%.10f bit-exact == expected %.10f"),
			Run1Float01[0], kExpectedFloat01[0]),
		Run1Float01[0] == kExpectedFloat01[0]);
	TestTrue(
		*FString::Printf(TEXT("AC-8 Run-1: Float01[1]=%.10f bit-exact == expected %.10f"),
			Run1Float01[1], kExpectedFloat01[1]),
		Run1Float01[1] == kExpectedFloat01[1]);
	for (int32 i = 0; i < 5; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-1: Range16[%d]"), i),
			Run1Range16[i], kExpectedRange16[i]);
	}

	// ---- Run-2 vs expected[]（第二次冷启动独立重跑，逐字段比对）----
	for (int32 i = 0; i < 5; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-2: Roll[%d].Die1"), i),
			Run2Rolls[i].Die1, kExpectedRolls[i].Die1);
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-2: Roll[%d].Die2"), i),
			Run2Rolls[i].Die2, kExpectedRolls[i].Die2);
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-2: Roll[%d].Total"), i),
			Run2Rolls[i].Total, kExpectedRolls[i].Total);
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-2: Roll[%d].bIsDouble"), i),
			Run2Rolls[i].bIsDouble, kExpectedRolls[i].bIsDouble);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-2: Range05[%d]"), i),
			Run2Range05[i], kExpectedRange05[i]);
	}
	TestTrue(
		*FString::Printf(TEXT("AC-8 Run-2: Float01[0]=%.10f bit-exact == expected %.10f"),
			Run2Float01[0], kExpectedFloat01[0]),
		Run2Float01[0] == kExpectedFloat01[0]);
	TestTrue(
		*FString::Printf(TEXT("AC-8 Run-2: Float01[1]=%.10f bit-exact == expected %.10f"),
			Run2Float01[1], kExpectedFloat01[1]),
		Run2Float01[1] == kExpectedFloat01[1]);
	for (int32 i = 0; i < 5; ++i)
	{
		TestEqual(
			*FString::Printf(TEXT("AC-8 Run-2: Range16[%d]"), i),
			Run2Range16[i], kExpectedRange16[i]);
	}

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC8 PASS: 两次冷启动 seed=12345 序列逐字段与 expected[] 精确相等"
			 "（TR-dice-017，ADR-0004 性质 D，story-006 AC-8 [Logic] PR gate）。"));
	return true;
#endif // CAPTURE_MODE
}

// =============================================================================
// TC_AC9 — AC-9：跨种子固化，seed=12345 vs seed=99999 各跑 RollDice×5
//
// 断言各组运行结果逐字段精确等于各自固化常量（不用「两组不同」概率断言，
// 见 story-006 Implementation Notes AC-9 R1 修 D-1 明确禁止）。
//
// 两组常量不同的离线确认：
//   在 CAPTURE 模式下，两组常量均从引擎真实运行捕获。
//   seed=12345 与 seed=99999 是不同种子，FRandomStream LCG 起点不同，
//   产生不同序列。编写者须在捕获后人工核对两组值不完全相同（注释说明）。
//
// 非 vacuous 保证：
//   - 若实现忽略种子（如 lazy-init 退化到固定 0），至少一组对不上 expected → FAIL
//   - 若 SetSeed 不重置流（bug），结果依赖当前流状态，与固化常量不符 → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeterminismFixture_TC_AC9_CrossSeed_TwoGroupsMatchFixedConstants,
	"Rento.Dice.DeterminismFixture.TC_AC9_CrossSeed_TwoGroupsMatchFixedConstants",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeterminismFixture_TC_AC9_CrossSeed_TwoGroupsMatchFixedConstants::RunTest(const FString& Parameters)
{
	// =========================================================================
	// ---- Phase-2 expected 常量（已从引擎真实运行捕获，2026-06-08）----
	//
	// 离线确认（story-006 AC-9 R1 修 D-1 要求）：
	//   两组常量从 UE5.7 Win64 Development 实机运行捕获，确认互不相同：
	//   kExpected_12345[0]：Die1=3, Die2=4
	//   kExpected_99999[0]：Die1=6, Die2=1
	//   第一条即不同（Die1: 3≠6, Die2: 4≠1），两组常量确实不完全相同 ✓
	//   这证明 SetSeed(12345) 和 SetSeed(99999) 产生不同序列，覆盖「忽略种子退化」场景。
	//   运行期断言只做「等于固化常量」，不做「两组不同」概率断言（ADR-0004 设计意图）。
	// =========================================================================
#if !CAPTURE_MODE
	// seed=12345 下 RollDice×5 固化序列（引擎捕获值）
	static constexpr FFixtureDiceEntry kExpected_12345[5] = {
		{ 3, 4,  7, false },  // kExpected_12345[0]
		{ 4, 6, 10, false },  // kExpected_12345[1]
		{ 1, 4,  5, false },  // kExpected_12345[2]
		{ 4, 4,  8, true  },  // kExpected_12345[3]
		{ 2, 2,  4, true  },  // kExpected_12345[4]
	};

	// seed=99999 下 RollDice×5 固化序列（引擎捕获值）
	// 离线确认：与 kExpected_12345[] 不同（第 0 条 Die1: 3≠6, Die2: 4≠1）
	static constexpr FFixtureDiceEntry kExpected_99999[5] = {
		{ 6, 1,  7, false },  // kExpected_99999[0]
		{ 4, 1,  5, false },  // kExpected_99999[1]
		{ 6, 2,  8, false },  // kExpected_99999[2]
		{ 6, 4, 10, false },  // kExpected_99999[3]
		{ 6, 6, 12, true  },  // kExpected_99999[4]
	};
#endif

	// =========================================================================
	// ---- CAPTURE_MODE：打印两组真实值 ----
	// =========================================================================
#if CAPTURE_MODE

	// ---- seed=12345 组 ----
	UDiceRngService* Service12345 = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-9 seed=12345: UDiceRngService 构造"), Service12345)) return false;
	Service12345->SetSeed(12345);

	FFixtureDiceEntry Rolls12345[5];
	for (int32 i = 0; i < 5; ++i)
	{
		const FDiceRollResult R = Service12345->RollDice();
		Rolls12345[i] = { R.Die1, R.Die2, R.Total, R.bIsDouble };
	}

	// ---- seed=99999 组 ----
	UDiceRngService* Service99999 = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-9 seed=99999: UDiceRngService 构造"), Service99999)) return false;
	Service99999->SetSeed(99999);

	FFixtureDiceEntry Rolls99999[5];
	for (int32 i = 0; i < 5; ++i)
	{
		const FDiceRollResult R = Service99999->RollDice();
		Rolls99999[i] = { R.Die1, R.Die2, R.Total, R.bIsDouble };
	}

	// 打印两组捕获值
	UE_LOG(LogTemp, Warning,
		TEXT("=== AC-9 CAPTURE MODE: seed=12345 vs seed=99999 序列 ==="));

	UE_LOG(LogTemp, Warning, TEXT("  --- seed=12345 kExpected_12345[] ---"));
	for (int32 i = 0; i < 5; ++i)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("  kExpected_12345[%d] = { %d, %d, %d, %s }"),
			i,
			Rolls12345[i].Die1, Rolls12345[i].Die2, Rolls12345[i].Total,
			Rolls12345[i].bIsDouble ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogTemp, Warning, TEXT("  --- seed=99999 kExpected_99999[] ---"));
	for (int32 i = 0; i < 5; ++i)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("  kExpected_99999[%d] = { %d, %d, %d, %s }"),
			i,
			Rolls99999[i].Die1, Rolls99999[i].Die2, Rolls99999[i].Total,
			Rolls99999[i].bIsDouble ? TEXT("true") : TEXT("false"));
	}

	// 打印两组第一条是否不同（离线确认辅助）
	bool bGroupsDiffer = false;
	for (int32 i = 0; i < 5; ++i)
	{
		if (Rolls12345[i].Die1 != Rolls99999[i].Die1 ||
			Rolls12345[i].Die2 != Rolls99999[i].Die2)
		{
			bGroupsDiffer = true;
			break;
		}
	}
	if (bGroupsDiffer)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("  离线确认：两组常量不完全相同 ✓（seed 差异产生不同序列，符合预期）"));
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("  ⚠ 离线确认：两组 RollDice×5 序列完全相同——这极不可能，请检查 SetSeed 实现"));
	}

	return true;

#else // !CAPTURE_MODE — Phase-2 PR gate 断言

	// =========================================================================
	// ---- seed=12345 组：运行并逐字段比对 kExpected_12345[] ----
	// =========================================================================
	UDiceRngService* Service12345 = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-9 seed=12345: UDiceRngService 构造"), Service12345)) return false;
	Service12345->SetSeed(12345);

	for (int32 i = 0; i < 5; ++i)
	{
		const FDiceRollResult R = Service12345->RollDice();

		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=12345: Roll[%d].Die1"), i),
			R.Die1, kExpected_12345[i].Die1);
		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=12345: Roll[%d].Die2"), i),
			R.Die2, kExpected_12345[i].Die2);
		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=12345: Roll[%d].Total"), i),
			R.Total, kExpected_12345[i].Total);
		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=12345: Roll[%d].bIsDouble"), i),
			R.bIsDouble, kExpected_12345[i].bIsDouble);
	}

	// =========================================================================
	// ---- seed=99999 组：运行并逐字段比对 kExpected_99999[] ----
	// =========================================================================
	UDiceRngService* Service99999 = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-9 seed=99999: UDiceRngService 构造"), Service99999)) return false;
	Service99999->SetSeed(99999);

	for (int32 i = 0; i < 5; ++i)
	{
		const FDiceRollResult R = Service99999->RollDice();

		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=99999: Roll[%d].Die1"), i),
			R.Die1, kExpected_99999[i].Die1);
		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=99999: Roll[%d].Die2"), i),
			R.Die2, kExpected_99999[i].Die2);
		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=99999: Roll[%d].Total"), i),
			R.Total, kExpected_99999[i].Total);
		TestEqual(
			*FString::Printf(TEXT("AC-9 seed=99999: Roll[%d].bIsDouble"), i),
			R.bIsDouble, kExpected_99999[i].bIsDouble);
	}

	// 离线确认注释（Phase-2 静态文档）：
	// 根据 CAPTURE_MODE 阶段打印的日志，两组常量在第一条即不同（seed 差异产生不同 LCG 起点）。
	// seed=12345 第[0]条与 seed=99999 第[0]条的 Die1/Die2 至少一项不同。
	// 运行期断言只做「等于固化常量」，不做「两组不同」概率断言（story-006 AC-9 R1 修 D-1 明确禁止）。

	UE_LOG(LogTemp, Log,
		TEXT("TC_AC9 PASS: seed=12345 和 seed=99999 各组 RollDice×5 逐字段与各自固化常量精确相等"
			 "（TR-dice-002，ADR-0004 性质 D，story-006 AC-9 [Logic]）。"));
	return true;
#endif // CAPTURE_MODE
}
