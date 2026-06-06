// SampleFormulaSpec.cpp
// =============================================================================
// UE Automation Spec —— 脚手架冒烟测试 + 确定性公式测试模式演示
//
// 状态：已激活（rento + rentoTests 模块已建立，2026-06-06）。
//       此文件验证 Automation 框架在本工程 headless 可跑。真实公式断言在 story dev 填：
//         - dice-rng/story-006: F-5 确定性种子序列 fixture（≥20 抽取逐字段相等）
//         - board-data/story-003: AdvanceIndex 拓扑（AC-2/8/9/10... 确定性 fixture）
//         - economy 租金 / ai Buffer 等
//       届时新建 [System][Feature]Spec.cpp，include 真实模块头，替换示例纯函数。
//
// 演示要点（coding-standards / ADR-0004 确定性纪律）：
//   - 固定输入 → 断言确定输出（无 RNG、无时间依赖、无 I/O）
//   - headless `-nullrhi` 可跑（纯逻辑，不碰渲染/Slate）
//   - Automation 类目 "Rento.[System].[Feature]"
//   - EAutomationTestFlags：ApplicationContextMask | ProductFilter（CI 跑）
// =============================================================================

#include "Misc/AutomationTest.h"

// 示例被测对象：一个确定性纯函数（story dev 时替换为 UEconomyLibrary::CalculateRent 等）
namespace
{
	int32 SampleAddDeterministic(int32 A, int32 B)
	{
		return A + B;
	}
}

// -----------------------------------------------------------------------------
// Spec 风格（BDD：Describe/It），推荐用于多场景公式
// -----------------------------------------------------------------------------
BEGIN_DEFINE_SPEC(
	FSampleFormulaSpec,
	"Rento.Sample.Formula",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSampleFormulaSpec)

void FSampleFormulaSpec::Define()
{
	Describe("确定性公式（示例：story dev 时替换为真实 economy/ai/dice 公式）", [this]()
	{
		It("固定输入产出确定输出（无 RNG / 无时间依赖）", [this]()
		{
			const int32 Result = SampleAddDeterministic(1500, 750);
			TestEqual(TEXT("1500 + 750 == 2250"), Result, 2250);
		});

		It("边界值（演示 boundary value 测试，coding-standards 例外：数字本身即重点）", [this]()
		{
			TestEqual(TEXT("0 + 0 == 0"), SampleAddDeterministic(0, 0), 0);
		});
	});
}

// -----------------------------------------------------------------------------
// 简单风格（单断言）——脚手架冒烟，验证测试框架自身在本工程可跑
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSampleSmokeTest,
	"Rento.Sample.Smoke",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSampleSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("测试框架自身可跑（脚手架冒烟）"), true);
	return true;
}
