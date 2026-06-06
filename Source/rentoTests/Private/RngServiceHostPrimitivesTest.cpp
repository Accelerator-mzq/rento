// RngServiceHostPrimitivesTest.cpp
// =============================================================================
// 单元测试：UDiceRngService 4 原语封装 + BP-callable 接口验证
// story-001 AC-23 / AC-18 / AC-13 / AC-12c covered。
//
// 物理路径：Source/rentoTests/Private/RngServiceHostPrimitivesTest.cpp
// 逻辑分类：tests/unit/dice/rng_service_host_primitives_test.cpp
// Automation 类目：Rento.Dice.RngServicePrimitives
//
// 测试设计原则：
//   - 确定性：全部使用代码构造 fixture + 固定种子，无时间 / IO 依赖
//   - NewObject<UDiceRngService>(GetTransientPackage()) —— headless 安全
//   - 非 vacuous：AC-12c floor 不越界、AC-13 <1.0 断言在理论上能 FAIL
//     （若 FRand() 实现崩坏返回 1.0，TestTrue(f < 1.0f) 立即 FAIL）
//   - OQ 登记：AC-12c 只覆盖 MVP N 范围（2/6/12/100），N>=2^24 浮点越界登记 OQ-4
//
// 函数索引：
//   TC_AC23_BlueprintCallable_FourFunctions  → AC-23（4 函数 BP-callable，Stream 不暴露 BP）
//   TC_AC18_SetSeed_Zero_RandomRange         → AC-18（seed=0 合法，RandomRange 返回∈[1,6]）
//   TC_AC18_Edge_SetSeed_Negative            → AC-18 Edge（负数种子合法，返回∈[1,6]）
//   TC_AC13_RandomFloat01_SemiOpenInterval   → AC-13（固定种子 ×100，所有值 [0,1)）
//   TC_AC12c_Floor_NeverReturnsN             → AC-12c（MVP N∈{2,6,12,100}，floor 不越界）
// =============================================================================

#include "Misc/AutomationTest.h"
#include "UObject/Class.h"         // UClass::FindFunctionByName, TFieldIterator
#include "UObject/Script.h"        // EFunctionFlags（FUNC_BlueprintCallable 枚举，去魔法数字）
#include "Math/RandomStream.h"
#include "DiceRngService.h"

// =============================================================================
// TC_AC23 — AC-23：4 函数标 BlueprintCallable，编译通过，反射可见；
//           Edge：FRandomStream 成员不暴露 BP（无对应 UPROPERTY BlueprintReadWrite）
//
// 验证策略：
//   1. UClass::FindFunctionByName —— 找到函数即反射元数据生成成功
//   2. 检查 FUNC_BlueprintCallable flag（0x04000000）—— 证 UFUNCTION 宏正确标注
//   3. TFieldIterator<FProperty> 遍历 UDiceRngService 本类 UPROPERTY，
//      断言无名为 "AuthoritativeStream" 的 BlueprintReadWrite 属性
//
// 非 vacuous 保证：
//   - 若函数未加 UFUNCTION(BlueprintCallable)，FindFunctionByName 返回 nullptr → TestNotNull FAIL
//   - 若 FRandomStream 被误加 UPROPERTY(BlueprintReadWrite)，Property 遍历找到 → TestFalse FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_AC23_BlueprintCallable_FourFunctions,
	"Rento.Dice.RngServicePrimitives.TC_AC23_BlueprintCallable_FourFunctions",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_AC23_BlueprintCallable_FourFunctions::RunTest(const FString& Parameters)
{
	const UClass* ServiceClass = UDiceRngService::StaticClass();
	if (!TestNotNull(TEXT("AC-23: UDiceRngService::StaticClass() 应非 null"), ServiceClass))
	{
		return false;
	}

	// BlueprintCallable flag：使用引擎枚举 EFunctionFlags::FUNC_BlueprintCallable（Script.h）
	// 不硬编码 0x04000000u——引擎升级时枚举值若变动，编译期即报错，魔法数字则静默错过。
	const uint32 BPCallableFlag = static_cast<uint32>(FUNC_BlueprintCallable);

	// ---- 验证 4 函数反射可见 + BlueprintCallable flag ----
	const TArray<FName> ExpectedFunctions = {
		FName(TEXT("RandomRange")),
		FName(TEXT("RandomFloat01")),
		FName(TEXT("SetSeed")),
		FName(TEXT("GetCurrentSeed")),
	};

	for (const FName& FuncName : ExpectedFunctions)
	{
		// FindFunctionByName 会向上搜索继承链，含本类即返回
		const UFunction* Func = ServiceClass->FindFunctionByName(FuncName);

		TestNotNull(
			*FString::Printf(TEXT("AC-23: 函数 '%s' 应在反射元数据中可见（FindFunctionByName 命中）"), *FuncName.ToString()),
			Func);

		if (Func)
		{
			// 检查 BlueprintCallable flag（使用引擎枚举转换值，非魔法数字）
			const bool bIsBPCallable = (Func->FunctionFlags & BPCallableFlag) != 0u;
			TestTrue(
				*FString::Printf(TEXT("AC-23: 函数 '%s' 应标 UFUNCTION(BlueprintCallable)（EFunctionFlags::FUNC_BlueprintCallable 应置位）"), *FuncName.ToString()),
				bIsBPCallable);
		}
	}

	// ---- Edge：FRandomStream 成员不暴露 BP ----
	// 遍历 UDiceRngService 本类 UPROPERTY（EFieldIterationFlags::None = 不含父类）
	// 断言：无名为 "AuthoritativeStream" 且带 BlueprintReadWrite/BlueprintReadOnly 的属性
	bool bFoundStreamBPExposed = false;
	for (TFieldIterator<FProperty> PropIt(ServiceClass, EFieldIterationFlags::None); PropIt; ++PropIt)
	{
		const FProperty* Prop = *PropIt;
		const bool bIsBPVisible =
			Prop->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly);

		if (Prop->GetName().Contains(TEXT("AuthoritativeStream")) && bIsBPVisible)
		{
			bFoundStreamBPExposed = true;
		}
	}

	TestFalse(
		TEXT("AC-23 Edge: FRandomStream 成员（AuthoritativeStream）不应暴露给 BP（无 BlueprintReadWrite/ReadOnly）"),
		bFoundStreamBPExposed);

	return true;
}

// =============================================================================
// TC_AC18 — AC-18：seed=0 经 SetSeed(0) 初始化后 RandomRange(1,6) 返回∈[1,6]
//
// 验证策略：
//   NewObject<UDiceRngService> + SetSeed(0) + 多次 RandomRange(1,6)
//   断言每次返回值 ∈ [1,6]（下界 ≥ 1，上界 ≤ 6）
//
// 非 vacuous 保证：
//   若 RandomRange 实现有 bug 返回 0 或 7，TestTrue(v >= 1) 或 TestTrue(v <= 6) 立即 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_AC18_SetSeed_Zero_RandomRange,
	"Rento.Dice.RngServicePrimitives.TC_AC18_SetSeed_Zero_RandomRange_InRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_AC18_SetSeed_Zero_RandomRange::RunTest(const FString& Parameters)
{
	// GIVEN：NewObject 创建服务实例，SetSeed(0)（0 是合法种子，非哨兵，ADR-0004）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-18: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(0);

	// WHEN + THEN：调用 30 次 RandomRange(1,6)，每次结果应 ∈ [1,6]
	constexpr int32 IterCount = 30;
	for (int32 i = 0; i < IterCount; ++i)
	{
		const int32 Value = Service->RandomRange(1, 6);

		TestTrue(
			*FString::Printf(TEXT("AC-18: RandomRange(1,6) 第 %d 次返回值应 >= 1（实际 %d）"), i + 1, Value),
			Value >= 1);

		TestTrue(
			*FString::Printf(TEXT("AC-18: RandomRange(1,6) 第 %d 次返回值应 <= 6（实际 %d）"), i + 1, Value),
			Value <= 6);
	}

	return true;
}

// =============================================================================
// TC_AC18_Edge — AC-18 Edge：负数种子（SetSeed(-12345)）同样合法，返回值∈[1,6]
//
// 验证策略：与 TC_AC18 相同结构，仅换用负数种子。
// FRandomStream::Initialize 接受 int32 全范围，负数不应有特殊路径。
//
// 非 vacuous 保证：同 TC_AC18。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_AC18_Edge_SetSeed_Negative,
	"Rento.Dice.RngServicePrimitives.TC_AC18_Edge_SetSeed_Negative_InRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_AC18_Edge_SetSeed_Negative::RunTest(const FString& Parameters)
{
	// GIVEN：NewObject + SetSeed(-12345)（负数种子，ADR-0004 §Key Interfaces 合法）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-18 Edge: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(-12345);

	// WHEN + THEN：调用 30 次 RandomRange(1,6)，每次结果应 ∈ [1,6]
	constexpr int32 IterCount = 30;
	for (int32 i = 0; i < IterCount; ++i)
	{
		const int32 Value = Service->RandomRange(1, 6);

		TestTrue(
			*FString::Printf(TEXT("AC-18 Edge: 负数种子 RandomRange(1,6) 第 %d 次应 >= 1（实际 %d）"), i + 1, Value),
			Value >= 1);

		TestTrue(
			*FString::Printf(TEXT("AC-18 Edge: 负数种子 RandomRange(1,6) 第 %d 次应 <= 6（实际 %d）"), i + 1, Value),
			Value <= 6);
	}

	return true;
}

// =============================================================================
// TC_AC13 — AC-13：固定种子 RandomFloat01() ×100，所有值 [0.0, 1.0)，无一 ==1.0；
//           Edge：至少一次 f 接近 0（验下界可达）
//
// 验证策略：
//   NewObject + SetSeed(42) + 调用 100 次 RandomFloat01()
//   断言每值 >= 0.0f 且严格 < 1.0f（非 <=）
//   Edge：100 次中 min 值应 < 0.1f（宽松阈值，验下界可达但不要求精确值）
//
// 非 vacuous 保证（关键）：
//   TestTrue(f < 1.0f) 是真能 FAIL 的断言——若 FRand() 实现有 bug 返回 1.0，
//   此处立即 FAIL（而非同义反复，FRand 文档保证 <1.0，本测试是运行时验证）。
//   Edge min < 0.1f：固定种子 42 的前 100 次 FRand 序列统计上极大概率含接近 0 的值；
//   若实现有 bug（如全返回 0.5），min 不小于 0.5，TestTrue FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_AC13_RandomFloat01_SemiOpenInterval,
	"Rento.Dice.RngServicePrimitives.TC_AC13_RandomFloat01_SemiOpenInterval",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_AC13_RandomFloat01_SemiOpenInterval::RunTest(const FString& Parameters)
{
	// GIVEN：NewObject + SetSeed(42)（任意固定种子，保证确定性）
	UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
	if (!TestNotNull(TEXT("AC-13: UDiceRngService 应能通过 NewObject<> 构造"), Service))
	{
		return false;
	}

	Service->SetSeed(42);

	// WHEN：收集 100 次 RandomFloat01() 结果
	constexpr int32 IterCount = 100;
	float MinObserved = 1.0f; // 初始为最大可能值，逐步取 min
	bool bAnyEqualOne = false;

	for (int32 i = 0; i < IterCount; ++i)
	{
		const float f = Service->RandomFloat01();

		// THEN-1：每值应 >= 0.0（下界包含）
		TestTrue(
			*FString::Printf(TEXT("AC-13: 第 %d 次 RandomFloat01() 应 >= 0.0（实际 %.8f）"), i + 1, f),
			f >= 0.0f);

		// THEN-2：每值应严格 < 1.0（1.0 排除，半开区间定义）
		// 非 vacuous：若 FRand 返回 1.0，此处立即 FAIL
		TestTrue(
			*FString::Printf(TEXT("AC-13: 第 %d 次 RandomFloat01() 应严格 < 1.0（实际 %.8f，1.0 排除）"), i + 1, f),
			f < 1.0f);

		if (f >= 1.0f)
		{
			bAnyEqualOne = true;
		}

		MinObserved = FMath::Min(MinObserved, f);
	}

	// THEN-3：汇总断言（单条 conclusive message，补充细粒度断言之外的汇总视图）
	TestFalse(TEXT("AC-13: 100 次调用中不应有任何值 >= 1.0"), bAnyEqualOne);

	// THEN-4 Edge：下界可达（min < 0.1f）
	// FRand 在大量抽样中统计上必然覆盖接近 0 的值；
	// 固定种子 42 前 100 次序列经 FRandomStream 确定性保证，min 应远小于 0.1。
	// 若实现 bug 导致全输出偏高（如全 > 0.5），此处 FAIL（非 vacuous）。
	TestTrue(
		*FString::Printf(TEXT("AC-13 Edge: 100 次抽样 min(%.8f) 应 < 0.1f（验下界可达）"), MinObserved),
		MinObserved < 0.1f);

	return true;
}

// =============================================================================
// TC_AC12c — AC-12c：固定种子，对 MVP N∈{2,6,12,100}，
//            floor(RandomFloat01()*N) ×100 次，结果恒∈[0, N-1]，永不返回 N
//
// 验证策略：
//   对每个 N：NewObject + SetSeed(固定值) + 100 次 floor(f*N) 计算
//   断言每次结果 ∈ [0, N-1]（即 floor_result >= 0 且 floor_result < N）
//
// OQ-4 登记：
//   本条只覆盖 MVP N 范围（2/6/12/100 均 << 2^24）。
//   N >= 2^24 时 FRand() 浮点精度可能使 floor(f*N) 偶发返回 N（越界），
//   该场景在 MVP 不可达，守门登记 OQ-4，不在本条声称已覆盖（避免假安全感）。
//
// 非 vacuous 保证（关键）：
//   floor_result < N 是真能 FAIL 的断言——
//   若 FRand() 有 bug 返回 1.0，则 floor(1.0 * N) = N，TestTrue(N < N) 立即 FAIL。
//   若 FRand() 返回略小于 1.0 的值（如 0.9999999f），对小 N（2/6/12/100）：
//   floor(0.9999999 * 100) = floor(99.99999) = 99 < 100，安全（MVP 范围断言有意义）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDiceRngService_TC_AC12c_Floor_NeverReturnsN,
	"Rento.Dice.RngServicePrimitives.TC_AC12c_Floor_NeverReturnsN",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDiceRngService_TC_AC12c_Floor_NeverReturnsN::RunTest(const FString& Parameters)
{
	// OQ-4 登记注释（代码内声明，供后续 story-003/006 引用）：
	// 本测试只覆盖 MVP N 范围（最大 N=100 << 2^24=16777216）。
	// N >= 2^24 的浮点舍入越界风险属 OQ-4，不在本 story 声称已覆盖。

	// MVP N 范围（ADR-0004 §AC-12c / story-001 QA Test Cases）
	const TArray<int32> NValues = { 2, 6, 12, 100 };
	constexpr int32 IterCount = 100;

	for (const int32 N : NValues)
	{
		// GIVEN：每个 N 独立 NewObject + SetSeed(固定值 = N*7+1，任意固定，保证确定性)
		// 不同 N 用不同种子，避免相同序列掩盖 N 相关的边界问题
		UDiceRngService* Service = NewObject<UDiceRngService>(GetTransientPackage());
		if (!TestNotNull(
			*FString::Printf(TEXT("AC-12c: N=%d 应能创建 UDiceRngService"), N),
			Service))
		{
			return false;
		}

		const int32 Seed = N * 7 + 1; // 固定种子，不同 N 隔离
		Service->SetSeed(Seed);

		// WHEN + THEN：100 次 floor(f*N)，结果恒∈[0, N-1]
		for (int32 i = 0; i < IterCount; ++i)
		{
			const float f = Service->RandomFloat01();

			// FMath::FloorToInt 等价于 C++ (int32)floor(f * N)
			const int32 FloorResult = FMath::FloorToInt(f * static_cast<float>(N));

			// 下界断言：>= 0（FRand 返回 >=0，floor 结果 >=0）
			TestTrue(
				*FString::Printf(
					TEXT("AC-12c: N=%d 第 %d 次 floor(f*N)=%d 应 >= 0"),
					N, i + 1, FloorResult),
				FloorResult >= 0);

			// 上界断言（关键非 vacuous 断言）：严格 < N，永不返回 N
			// 若 FRand 有 bug 返回 1.0，floor(1.0*N)=N，此处立即 FAIL
			TestTrue(
				*FString::Printf(
					TEXT("AC-12c: N=%d 第 %d 次 floor(f*N)=%d 应严格 < N=%d（永不返回 N）"),
					N, i + 1, FloorResult, N),
				FloorResult < N);
		}
	}

	return true;
}
