// SampleFormulaSpec.cpp
// =============================================================================
// UE Automation Spec —— 示例测试**模板**（确定性公式测试模式演示）
//
// 状态：模板。.uproject 与游戏模块尚未创建（设计/架构阶段）。
//       首个实现 sprint 建 DiceTycoon + DiceTycoonTests 模块后：
//         ① 取消下方 #if 0 包裹（或删除占位、接入真实模块）
//         ② 把 SampleAddDeterministic 替换为真实公式断言（如 economy F-1 租金、
//            ai F-1 Buffer、dice F-2 RandRange 边界）
//         ③ include 真实游戏模块头文件
//
// 演示要点（coding-standards / ADR-0004 确定性纪律）：
//   - 固定输入 → 断言确定输出（无 RNG、无时间依赖、无 I/O）
//   - headless `-nullrhi` 可跑（纯逻辑，不碰渲染/Slate）
//   - Automation 类目 "DiceTycoon.[System].[Feature]"
//   - EAutomationTestFlags：ApplicationContextMask | ProductFilter（CI 跑）
// =============================================================================

#if 0  // ← 游戏模块建立后取消此包裹（移除 #if 0 / #endif）

#include "Misc/AutomationTest.h"

// 示例被测对象：一个确定性纯函数（实际项目里替换为 UEconomyLibrary::CalculateRent 等）
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
    "DiceTycoon.Sample.Formula",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSampleFormulaSpec)

void FSampleFormulaSpec::Define()
{
    Describe("确定性公式（示例：替换为真实 economy/ai/dice 公式）", [this]()
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
// 简单风格（单断言）——供对比；复杂场景用上面的 Spec
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSampleSmokeTest,
    "DiceTycoon.Sample.Smoke",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSampleSmokeTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("测试框架自身可跑（脚手架冒烟）"), true);
    return true;
}

#endif  // 游戏模块建立后激活
