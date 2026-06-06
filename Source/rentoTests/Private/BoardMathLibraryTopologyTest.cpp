// BoardMathLibraryTopologyTest.cpp
// =============================================================================
// 单元测试：UBoardMathLibrary F-1/F-2/F-3 环路拓扑 + AdvanceIndex 原子返回
// story-003 AC-2/8~17/29/33/34/37a 全量确定性 fixture covered。
//
// 物理路径：Source/rentoTests/Private/BoardMathLibraryTopologyTest.cpp
// 逻辑分类：tests/unit/board/board_math_library_topology_test.cpp
// Automation 类目：Rento.Board.BoardMathLibrary
//
// 测试设计原则（本项目硬约束）：
//   - 确定性：全部 fixture 代码构造，固定输入→固定输出，无随机/时间依赖
//   - 非 vacuous：每条断言期望值精确（如 NewIndex==38 非 >=-1），
//     改实现会真 FAIL（验证：把公式改成 A%N 不取正，AC-10/AC-14 立即 FAIL）
//   - headless -nullrhi 可跑：UBoardMathLibrary 无状态纯静态，无需 World/UObject 实例
//   - AC-34 UE_LOG Error 处理（🟡 logged decision）：
//     循 FF-003 惯例，实现用 UE_LOG(Error) 替代 ensureMsgf，每次恰好产生一行
//     Error 日志（"From 越界" 子串），AddExpectedError(Contains, 2) 精确吞掉两次。
//   - AC-29 Warning 处理：UE_LOG Warning 不触发 Automation FAIL，无需 AddExpectedError
//
// 覆盖映射（共 19 条测试函数：15 条原始 AC + 4 条 code-review 补充）：
//   TC_AC2_RingClosure                → AC-2  环闭合（39+1 → 0）
//   TC_AC8_F1_NormalAdvance           → AC-8  F-1 普通前进（28+7 → 35）
//   TC_AC9_F1_PassGo                  → AC-9  F-1 越 GO（38+5 → 3）
//   TC_AC10_F1_NegativeSteps          → AC-10 F-1 负步数取正（5-7 → 38）
//   TC_AC11_F1_MultiLap               → AC-11 F-1 整圈/多圈（10+40/10+83）
//   TC_AC12_F2_PassedGoOnce           → AC-12 F-2 过 GO 一次（38+5 → PassedGo=1）
//   TC_AC13_F2_NoPassed               → AC-13 F-2 未过 GO（28+7 → PassedGo=0）
//   TC_AC14_F2_NegativePassedGo       → AC-14 F-2 逆向穿越（5-7 → PassedGo=-1）
//   TC_AC15_F2_MultiLapPassedGo       → AC-15 F-2 多圈计数（38+45 → PassedGo=2）
//   TC_AC16_F3_StepsBetween           → AC-16 F-3 前向距离（36→5 = 9）
//   TC_AC17_F3_SameTile               → AC-17 F-3 同格（20→20 = 0）
//   TC_AC29_OverflowSteps_WithWarning → AC-29 超界照算+告警（0+1000 → 0, 25）
//   TC_AC33_StopOnGo_CountsAsPassed   → AC-33 停在 GO 等价经过（38+2 → PassedGo=1）
//   TC_AC34_FromOutOfBounds_LogAndCorrect → AC-34 越界 from UE_LOG Error+正确返回
//   TC_AC37a_AtomicReturn             → AC-37a AdvanceIndex 单次调用返回整 struct
//
// code-review 补充（覆盖洞修复）：
//   TC_NGuard_AdvanceIndex_InvalidN   → N<=0 守卫：N=0/N=-1 均返回安全值，记 Error
//   TC_NGuard_StepsBetween_InvalidN   → N<=0 守卫：StepsBetween 返回 0，记 Error
//   TC_F2_NegativeMultiLap            → 负多圈：(0,-81,40)→{39,-3}（FloorDiv64 负大数）
//   TC_Boundary_Steps_Exactly2N       → |Steps|==2N 精确边界：不触 Warning，结果正确
//
// AC-37b [Advisory] code-review 静态守门：
//   无运行时断言（运行时无法断言函数不存在）；
//   证据文件：production/qa/evidence/BD-003-AC37b-no-PassedGoCount.md
// =============================================================================

#include "Misc/AutomationTest.h"
#include "BoardMathLibrary.h"

// =============================================================================
// 测试公共常量：标准 40 格棋盘（经典大富翁，N=40）
// =============================================================================
namespace BoardMathTestConst
{
    // 所有测试默认棋盘大小（N=40），与 story-003 QA Test Cases 表一致
    static constexpr int32 N = 40;
}

// =============================================================================
// TC_AC2 — AC-2（CR-1 环闭合）
//
// GIVEN：N=40，from=39，steps=1
// WHEN： AdvanceIndex(39, 1, 40)
// THEN： NewIndex==0（最后一格 +1 = 起点，首尾相接）
//
// 非 vacuous：若把 F-1 改成 (From+Steps) % N（C++ 截断），39+1=40，40%40=0，
//   本 case 恰好正确——但 AC-10 负数 case 会 FAIL，证组合式不可或缺。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC2_RingClosure,
    "Rento.Board.BoardMathLibrary.TC_AC2_RingClosure",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC2_RingClosure::RunTest(const FString& Parameters)
{
    // GIVEN + WHEN
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(39, 1, BoardMathTestConst::N);

    // THEN：NewIndex 精确 == 0（环闭合）
    TestEqual(TEXT("AC-2: AdvanceIndex(39,1,40).NewIndex 应为 0（首尾相接）"),
        Result.NewIndex, 0);

    return true;
}

// =============================================================================
// TC_AC8 — AC-8（F-1 普通前进）
//
// GIVEN：N=40，from=28，steps=7
// WHEN： AdvanceIndex(28, 7, 40)
// THEN： NewIndex==35（28+7=35，无越界）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC8_F1_NormalAdvance,
    "Rento.Board.BoardMathLibrary.TC_AC8_F1_NormalAdvance",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC8_F1_NormalAdvance::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(28, 7, BoardMathTestConst::N);

    TestEqual(TEXT("AC-8: AdvanceIndex(28,7,40).NewIndex 应为 35（普通前进）"),
        Result.NewIndex, 35);

    return true;
}

// =============================================================================
// TC_AC9 — AC-9（F-1 越 GO）
//
// GIVEN：N=40，from=38，steps=5
// WHEN： AdvanceIndex(38, 5, 40)
// THEN： NewIndex==3（38+5=43，43 mod 40=3，越过 GO 落在 3）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC9_F1_PassGo,
    "Rento.Board.BoardMathLibrary.TC_AC9_F1_PassGo",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC9_F1_PassGo::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(38, 5, BoardMathTestConst::N);

    TestEqual(TEXT("AC-9: AdvanceIndex(38,5,40).NewIndex 应为 3（越 GO 落 3）"),
        Result.NewIndex, 3);

    return true;
}

// =============================================================================
// TC_AC10 — AC-10（F-1 负步数取正）
//
// GIVEN：N=40，from=5，steps=-7
// WHEN： AdvanceIndex(5, -7, 40)
// THEN： NewIndex==38（5-7=-2，数学 mod(-2,40)=38，非 C++ -2%40=-2）
//
// 非 vacuous 验证核心：C++ 单个 % 会返回 -2（错），组合式返回 38（对）。
// 若把 MathMod64 改成 A%N，本断言精确期望 38，会真 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC10_F1_NegativeSteps,
    "Rento.Board.BoardMathLibrary.TC_AC10_F1_NegativeSteps",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC10_F1_NegativeSteps::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(5, -7, BoardMathTestConst::N);

    // 精确期望 38（数学取余），非 -2（C++ % 截断）
    TestEqual(TEXT("AC-10: AdvanceIndex(5,-7,40).NewIndex 应为 38（数学取余，非 C++ 负余数）"),
        Result.NewIndex, 38);

    return true;
}

// =============================================================================
// TC_AC11 — AC-11（F-1 整圈/多圈）
//
// GIVEN：N=40
//   case-a: from=10, steps=40 → 整圈，落回原格
//   case-b: from=10, steps=83 → 多圈，83=2*40+3，落在 13
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC11_F1_MultiLap,
    "Rento.Board.BoardMathLibrary.TC_AC11_F1_MultiLap",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC11_F1_MultiLap::RunTest(const FString& Parameters)
{
    // case-a: 整圈（steps=N），落回原格 10
    {
        const FBoardAdvanceResult R = UBoardMathLibrary::AdvanceIndex(10, 40, BoardMathTestConst::N);
        TestEqual(TEXT("AC-11a: AdvanceIndex(10,40,40).NewIndex 应为 10（整圈落回原格）"),
            R.NewIndex, 10);
    }

    // case-b: 多圈（steps=83=2*40+3），落在 13
    {
        const FBoardAdvanceResult R = UBoardMathLibrary::AdvanceIndex(10, 83, BoardMathTestConst::N);
        TestEqual(TEXT("AC-11b: AdvanceIndex(10,83,40).NewIndex 应为 13（多圈，83=2×40+3）"),
            R.NewIndex, 13);
    }

    return true;
}

// =============================================================================
// TC_AC12 — AC-12（F-2 过 GO 一次）
//
// GIVEN：N=40，from=38，steps=5
// WHEN： AdvanceIndex(38, 5, 40).PassedGo
// THEN： PassedGo==1
// 推导：floor((38+5)/40) - floor(38/40) = floor(43/40) - floor(38/40) = 1 - 0 = 1
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC12_F2_PassedGoOnce,
    "Rento.Board.BoardMathLibrary.TC_AC12_F2_PassedGoOnce",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC12_F2_PassedGoOnce::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(38, 5, BoardMathTestConst::N);

    TestEqual(TEXT("AC-12: AdvanceIndex(38,5,40).PassedGo 应为 1（过 GO 一次）"),
        Result.PassedGo, 1);

    return true;
}

// =============================================================================
// TC_AC13 — AC-13（F-2 未过 GO）
//
// GIVEN：N=40，from=28，steps=7
// WHEN： AdvanceIndex(28, 7, 40).PassedGo
// THEN： PassedGo==0
// 推导：floor(35/40) - floor(28/40) = 0 - 0 = 0
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC13_F2_NoPassed,
    "Rento.Board.BoardMathLibrary.TC_AC13_F2_NoPassed",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC13_F2_NoPassed::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(28, 7, BoardMathTestConst::N);

    TestEqual(TEXT("AC-13: AdvanceIndex(28,7,40).PassedGo 应为 0（未过 GO）"),
        Result.PassedGo, 0);

    return true;
}

// =============================================================================
// TC_AC14 — AC-14（F-2 逆向穿越 GO）
//
// GIVEN：N=40，from=5，steps=-7
// WHEN： AdvanceIndex(5, -7, 40).PassedGo
// THEN： PassedGo==-1（如实输出有符号值）
//
// 推导：floor((5-7)/40) - floor(5/40) = floor(-2/40) - floor(5/40)
//        = FloorDiv64(-2, 40) - FloorDiv64(5, 40) = -1 - 0 = -1
//
// ⚠ 非 vacuous 核心：C++ -2/40==0（截断朝零），FloorDiv64(-2,40)==-1（向负无穷）。
//   若用 C++ 整数除法，PassedGo == 0，断言精确期望 -1 会真 FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC14_F2_NegativePassedGo,
    "Rento.Board.BoardMathLibrary.TC_AC14_F2_NegativePassedGo",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC14_F2_NegativePassedGo::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(5, -7, BoardMathTestConst::N);

    // 精确期望 -1（逆行穿越 GO），非 0（C++ 截断朝零的错误结果）
    TestEqual(TEXT("AC-14: AdvanceIndex(5,-7,40).PassedGo 应为 -1（逆向穿越 GO，有符号）"),
        Result.PassedGo, -1);

    return true;
}

// =============================================================================
// TC_AC15 — AC-15（F-2 多圈计数）
//
// GIVEN：N=40，from=38，steps=45
// WHEN： AdvanceIndex(38, 45, 40).PassedGo
// THEN： PassedGo==2
// 推导：floor((38+45)/40) - floor(38/40) = floor(83/40) - floor(38/40) = 2 - 0 = 2
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC15_F2_MultiLapPassedGo,
    "Rento.Board.BoardMathLibrary.TC_AC15_F2_MultiLapPassedGo",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC15_F2_MultiLapPassedGo::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(38, 45, BoardMathTestConst::N);

    TestEqual(TEXT("AC-15: AdvanceIndex(38,45,40).PassedGo 应为 2（多圈计数）"),
        Result.PassedGo, 2);

    return true;
}

// =============================================================================
// TC_AC16 — AC-16（F-3 前向距离）
//
// GIVEN：N=40，from=36，target=5
// WHEN： StepsBetween(36, 5, 40)
// THEN： 9（从 36 顺时针走 9 步到 5：36→37→38→39→0→1→2→3→4→5）
//
// 推导：mod(5-36, 40) = mod(-31, 40) = MathMod64(-31,40) = (-31+40)=9 ✓
// 非 vacuous：C++ 单个 % → -31%40==-31（错），组合式 → 9（对）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC16_F3_StepsBetween,
    "Rento.Board.BoardMathLibrary.TC_AC16_F3_StepsBetween",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC16_F3_StepsBetween::RunTest(const FString& Parameters)
{
    const int32 Steps = UBoardMathLibrary::StepsBetween(36, 5, BoardMathTestConst::N);

    // 精确期望 9（MathMod64 修正后的正确结果）
    TestEqual(TEXT("AC-16: StepsBetween(36,5,40) 应为 9（前向距离，mod(-31,40)=9）"),
        Steps, 9);

    return true;
}

// =============================================================================
// TC_AC17 — AC-17（F-3 同格）
//
// GIVEN：N=40，from=20，target=20
// WHEN： StepsBetween(20, 20, 40)
// THEN： 0（mod(0,40)=0，已在目标格）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC17_F3_SameTile,
    "Rento.Board.BoardMathLibrary.TC_AC17_F3_SameTile",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC17_F3_SameTile::RunTest(const FString& Parameters)
{
    const int32 Steps = UBoardMathLibrary::StepsBetween(20, 20, BoardMathTestConst::N);

    TestEqual(TEXT("AC-17: StepsBetween(20,20,40) 应为 0（同格，已在目标位置）"),
        Steps, 0);

    return true;
}

// =============================================================================
// TC_AC29 — AC-29（步数超界照算+告警）
//
// GIVEN：N=40，from=0，steps=1000（|1000| > 2*40=80，触发超界告警）
// WHEN： AdvanceIndex(0, 1000, 40)
// THEN： NewIndex==0（1000 mod 40=0），PassedGo==25（floor(1000/40)-floor(0/40)=25-0=25）
//         且记「步数超界」Warning 日志（UE_LOG Warning 不致 FAIL，无需 AddExpectedError）
//
// 注意：UE Automation 仅将 Error/Fatal 级日志判 FAIL，Warning 不影响测试结果，
//       因此直接断言返回值即可，不需要 AddExpectedError。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC29_OverflowSteps_WithWarning,
    "Rento.Board.BoardMathLibrary.TC_AC29_OverflowSteps_WithWarning",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC29_OverflowSteps_WithWarning::RunTest(const FString& Parameters)
{
    // steps=1000 超过 2*N=80，会触发 UE_LOG Warning（"步数超界"）
    // Warning 不致 FAIL，不需要 AddExpectedError
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(0, 1000, BoardMathTestConst::N);

    // 精确断言返回值（照常计算，不 clamp）
    TestEqual(TEXT("AC-29: AdvanceIndex(0,1000,40).NewIndex 应为 0（1000 mod 40=0）"),
        Result.NewIndex, 0);

    TestEqual(TEXT("AC-29: AdvanceIndex(0,1000,40).PassedGo 应为 25（floor(1000/40)=25）"),
        Result.PassedGo, 25);

    return true;
}

// =============================================================================
// TC_AC33 — AC-33（停在 GO 等价经过一次）
//
// GIVEN：N=40，from=38，steps=2
// WHEN： AdvanceIndex(38, 2, 40).PassedGo
// THEN： PassedGo==1（38+2=40，落在索引 0=GO 格，经过一次）
//
// 推导：floor((38+2)/40) - floor(38/40) = floor(40/40) - floor(38/40) = 1 - 0 = 1
// NewIndex：MathMod64(40, 40) = 40%40=0 ✓（停在 GO 格）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC33_StopOnGo_CountsAsPassed,
    "Rento.Board.BoardMathLibrary.TC_AC33_StopOnGo_CountsAsPassed",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC33_StopOnGo_CountsAsPassed::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(38, 2, BoardMathTestConst::N);

    TestEqual(TEXT("AC-33: AdvanceIndex(38,2,40).NewIndex 应为 0（停在 GO 格）"),
        Result.NewIndex, 0);

    TestEqual(TEXT("AC-33: AdvanceIndex(38,2,40).PassedGo 应为 1（停在 GO 索引 0=经过一次）"),
        Result.PassedGo, 1);

    return true;
}

// =============================================================================
// TC_AC34 — AC-34（From 越界：UE_LOG Error 报警 + 通用式仍正确返回）
//
// 🟡 logged decision：实现用 UE_LOG(Error) 替代 ensureMsgf（循 FF-003 惯例）。
// headless Automation 中 ensureMsgf 产生不稳定 callstack dump（行数不确定），
// UE_LOG Error 每次调用恰好产生一行 Error 日志，AddExpectedError(Contains, 2) 稳定。
// AC-34 可测语义（不崩溃 + 记 Error + 仍正确返回）由 UE_LOG Error + continue 完全满足。
//
// GIVEN-1：N=40，from=-1，steps=5（From < 0，越界）
// WHEN：    AdvanceIndex(-1, 5, 40)
// THEN：    记 Error 日志（"From 越界"）+ 返回 {NewIndex=4, PassedGo=1}
// 推导-1：Sum = -1+5 = 4
//   NewIndex:  MathMod64(4, 40) = 4
//   PassedGo:  FloorDiv64(4,40) - FloorDiv64(-1,40) = 0 - (-1) = 1
//   ⚠ FloorDiv64(-1,40) == -1（向负无穷），非 0（C++ -1/40 截断朝零）
//
// GIVEN-2：N=40，from=40，steps=1（From >= N，越界）
// WHEN：    AdvanceIndex(40, 1, 40)
// THEN：    记 Error 日志（"From 越界"）+ 返回 {NewIndex=1, PassedGo=0}
// 推导-2：Sum = 40+1 = 41
//   NewIndex:  MathMod64(41, 40) = 1
//   PassedGo:  FloorDiv64(41,40) - FloorDiv64(40,40) = 1 - 1 = 0
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC34_FromOutOfBounds_LogAndCorrect,
    "Rento.Board.BoardMathLibrary.TC_AC34_FromOutOfBounds_LogAndCorrect",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC34_FromOutOfBounds_LogAndCorrect::RunTest(const FString& Parameters)
{
    // 声明预期 Error 日志：两次越界调用各产生一条 UE_LOG(Error) 行，
    // 内容含 "From 越界" 子串（与 BoardMathLibrary.cpp 中 TEXT() 逐字对齐）。
    // UE_LOG(Error) 每次调用恰好一行，两次 = 2 次，计数稳定。
    AddExpectedError(
        TEXT("From 越界"),
        EAutomationExpectedErrorFlags::Contains,
        2);

    // ------------------------------------------------------------------
    // case-1：From=-1（< 0，越界）
    // 期望：NewIndex=4, PassedGo=1
    // 关键推导：FloorDiv64(-1, 40) == -1（向负无穷），非 0（C++ 截断朝零）
    //   PassedGo = FloorDiv64(4,40) - FloorDiv64(-1,40) = 0 - (-1) = 1
    // ------------------------------------------------------------------
    const FBoardAdvanceResult R1 =
        UBoardMathLibrary::AdvanceIndex(-1, 5, BoardMathTestConst::N);

    TestEqual(TEXT("AC-34 case1: AdvanceIndex(-1,5,40).NewIndex 应为 4（通用式正确计算）"),
        R1.NewIndex, 4);

    // 关键：FloorDiv64(-1,40)==-1 是 FloorDiv64 向负无穷修正的核心验证点
    TestEqual(TEXT("AC-34 case1: AdvanceIndex(-1,5,40).PassedGo 应为 1（FloorDiv(-1,40)==-1，向负无穷）"),
        R1.PassedGo, 1);

    // ------------------------------------------------------------------
    // case-2：From=40（>= N，越界）
    // 期望：NewIndex=1, PassedGo=0
    // 推导：Sum=41，MathMod64(41,40)=1；FloorDiv64(41,40)-FloorDiv64(40,40)=1-1=0
    // ------------------------------------------------------------------
    const FBoardAdvanceResult R2 =
        UBoardMathLibrary::AdvanceIndex(40, 1, BoardMathTestConst::N);

    TestEqual(TEXT("AC-34 case2: AdvanceIndex(40,1,40).NewIndex 应为 1（通用式正确计算）"),
        R2.NewIndex, 1);

    TestEqual(TEXT("AC-34 case2: AdvanceIndex(40,1,40).PassedGo 应为 0"),
        R2.PassedGo, 0);

    return true;
}

// =============================================================================
// TC_AC37a — AC-37a（AdvanceIndex 原子返回）
//
// GIVEN：N=40，from=38，steps=5
// WHEN： 单次调用 AdvanceIndex(38, 5, 40)
// THEN： 返回单个 FBoardAdvanceResult，同时携带 NewIndex=3 AND PassedGo=1
//         （与 AC-9/AC-12 同参数，交叉验证两值正确）
//
// 原子性验证说明（诚实说明可测边界）：
//   运行时测试只能验证「单次调用返回两个正确值」——两个 TestEqual 各自独立断言。
//   「不存在分离调用路径」属于类型层面保证（struct 返回 + Break Struct 节点），
//   运行时无法证明函数不存在，该约束由 AC-37b code-review 守门。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_AC37a_AtomicReturn,
    "Rento.Board.BoardMathLibrary.TC_AC37a_AtomicReturn",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_AC37a_AtomicReturn::RunTest(const FString& Parameters)
{
    // 单次调用，结果存入一个 struct（原子返回：两值同源于同一次调用）
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(38, 5, BoardMathTestConst::N);

    // 断言 NewIndex 正确（F-1：38+5=43，mod(43,40)=3）
    TestEqual(TEXT("AC-37a: AdvanceIndex(38,5,40).NewIndex 应为 3（F-1 正确）"),
        Result.NewIndex, 3);

    // 断言 PassedGo 正确（F-2：floor(43/40)-floor(38/40)=1-0=1）
    TestEqual(TEXT("AC-37a: AdvanceIndex(38,5,40).PassedGo 应为 1（F-2 正确）"),
        Result.PassedGo, 1);

    // 注：上方两个 TestEqual 已各自独立验证两值，此处不重复 TestTrue 合并断言
    // （合并断言与两条 TestEqual 完全冗余，删去以避免假覆盖）

    return true;
}

// =============================================================================
// TC_NGuard_AdvanceIndex_InvalidN — N<=0 守卫：AdvanceIndex 防除零
//
// 实现（BoardMathLibrary.cpp L80-88）：N<=0 时 UE_LOG(Error "N 无效") + 返回安全值
// {NewIndex=From, PassedGo=0}，不进入除法路径。
//
// GIVEN-1：N=0（最典型的除零输入）
// WHEN：    AdvanceIndex(5, 3, 0)
// THEN：    返回安全值 {NewIndex=5, PassedGo=0}，记 Error 日志（"N 无效"）
//
// GIVEN-2：N=-1（负数 N，同样无效）
// WHEN：    AdvanceIndex(5, 3, -1)
// THEN：    返回安全值 {NewIndex=5, PassedGo=0}，记 Error 日志（"N 无效"）
//
// 非 vacuous：若删去 N<=0 守卫直接进除法，N=0 除零崩溃（不返回），测试无法拿到值；
//   N=-1 时 MathMod64(8,-1) 返回 0（所有数 mod -1 == 0），NewIndex≠5，TestEqual FAIL。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_NGuard_AdvanceIndex_InvalidN,
    "Rento.Board.BoardMathLibrary.TC_NGuard_AdvanceIndex_InvalidN",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_NGuard_AdvanceIndex_InvalidN::RunTest(const FString& Parameters)
{
    // 两次 N 无效调用 → 两条 "N 无效" Error 日志
    AddExpectedError(
        TEXT("N 无效"),
        EAutomationExpectedErrorFlags::Contains,
        2);

    // ------------------------------------------------------------------
    // case-1：N=0（除零守卫，最典型非法输入）
    // 期望：安全值原位 {NewIndex=5, PassedGo=0}
    // ------------------------------------------------------------------
    const FBoardAdvanceResult R1 = UBoardMathLibrary::AdvanceIndex(5, 3, 0);

    TestEqual(TEXT("NGuard-Advance case1: N=0 时 NewIndex 应为安全值 5（原位）"),
        R1.NewIndex, 5);
    TestEqual(TEXT("NGuard-Advance case1: N=0 时 PassedGo 应为安全值 0"),
        R1.PassedGo, 0);

    // ------------------------------------------------------------------
    // case-2：N=-1（负数 N，同样无效）
    // 期望：安全值原位 {NewIndex=5, PassedGo=0}
    // 非 vacuous：若跳过守卫，MathMod64(8,-1) 内 (8%-1+(-1))%-1=0，NewIndex==0≠5
    // ------------------------------------------------------------------
    const FBoardAdvanceResult R2 = UBoardMathLibrary::AdvanceIndex(5, 3, -1);

    TestEqual(TEXT("NGuard-Advance case2: N=-1 时 NewIndex 应为安全值 5（原位）"),
        R2.NewIndex, 5);
    TestEqual(TEXT("NGuard-Advance case2: N=-1 时 PassedGo 应为安全值 0"),
        R2.PassedGo, 0);

    return true;
}

// =============================================================================
// TC_NGuard_StepsBetween_InvalidN — N<=0 守卫：StepsBetween 防除零
//
// 实现（BoardMathLibrary.cpp L149-155）：N<=0 时 UE_LOG(Error "N 无效") + return 0。
//
// GIVEN：N=0
// WHEN：  StepsBetween(5, 10, 0)
// THEN：  返回 0（安全值），记 Error 日志（"N 无效"）
//
// 非 vacuous：若删去守卫直接 MathMod64(5,0)，内部 10%0 除零崩溃；返回值不为 0。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_NGuard_StepsBetween_InvalidN,
    "Rento.Board.BoardMathLibrary.TC_NGuard_StepsBetween_InvalidN",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_NGuard_StepsBetween_InvalidN::RunTest(const FString& Parameters)
{
    // 一次 N 无效调用 → 一条 "N 无效" Error 日志
    AddExpectedError(
        TEXT("N 无效"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    const int32 Result = UBoardMathLibrary::StepsBetween(5, 10, 0);

    TestEqual(TEXT("NGuard-Between: N=0 时 StepsBetween 应返回安全值 0"),
        Result, 0);

    return true;
}

// =============================================================================
// TC_F2_NegativeMultiLap — F-2 负多圈（FloorDiv64 大负数验证）
//
// GIVEN：N=40，from=0，steps=-81
// WHEN： AdvanceIndex(0, -81, 40)
// THEN： NewIndex=39，PassedGo=-3
//
// 精确推导（自行手算，非来自提示）：
//   Sum = 0 + (-81) = -81
//   F-1: MathMod64(-81, 40)
//        -81 % 40 = -1（C++ 截断朝零）
//        (-1 + 40) % 40 = 39 ✓ → NewIndex = 39
//   F-2: FloorDiv64(-81, 40) - FloorDiv64(0, 40)
//        -81 / 40 = -2（C++ 截断朝零），-81 % 40 = -1（非零且异号）→ -2 - 1 = -3
//        0 / 40 = 0
//        PassedGo = -3 - 0 = -3 ✓
//
// 非 vacuous（两个关键验证）：
//   NewIndex：C++ (-81 % 40) = -1（错），组合式 = 39（对）—— AC-10 同类修正的大数验证
//   PassedGo：C++ (-81/40) = -2（错），FloorDiv64 = -3（对）—— 负大数 floor 修正验证
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_F2_NegativeMultiLap,
    "Rento.Board.BoardMathLibrary.TC_F2_NegativeMultiLap",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_F2_NegativeMultiLap::RunTest(const FString& Parameters)
{
    const FBoardAdvanceResult Result =
        UBoardMathLibrary::AdvanceIndex(0, -81, BoardMathTestConst::N);

    // F-1：MathMod64(-81,40)=39（修正 C++ -81%40=-1）
    TestEqual(TEXT("F2-NegMultiLap: AdvanceIndex(0,-81,40).NewIndex 应为 39（数学取余）"),
        Result.NewIndex, 39);

    // F-2：FloorDiv64(-81,40)-FloorDiv64(0,40) = -3-0 = -3（修正 C++ -81/40=-2）
    TestEqual(TEXT("F2-NegMultiLap: AdvanceIndex(0,-81,40).PassedGo 应为 -3（三圈逆行，FloorDiv64 大负数）"),
        Result.PassedGo, -3);

    return true;
}

// =============================================================================
// TC_Boundary_Steps_Exactly2N — |Steps|==2N 精确边界：不触 Warning + 结果正确
//                              + |Steps|==2N+1 触 Warning（Warning 不致 FAIL）
//
// 验证 Warning 触发条件为严格 >（非 >=）：
//   实现：FMath::Abs(Steps) > 2 * N
//   |Steps|==2N   → 80 > 80 == false → 不触 Warning
//   |Steps|==2N+1 → 81 > 80 == true  → 触 Warning（不致 FAIL）
//
// case-a：Steps=80（|Steps|==2N，精确边界，不触 Warning）
//   Sum = 0 + 80 = 80
//   F-1: 80 % 40 = 0 → NewIndex = 0
//   F-2: FloorDiv64(80,40) - FloorDiv64(0,40) = 2 - 0 = 2 → PassedGo = 2
//
// case-b：Steps=81（|Steps|==2N+1，触 Warning，Warning 不致 FAIL）
//   Sum = 0 + 81 = 81
//   F-1: 81 % 40 = 1 → NewIndex = 1
//   F-2: FloorDiv64(81,40) - 0 = 2 - 0 = 2 → PassedGo = 2
//   （81/40=2 余 1，同号不修正，与 FloorDiv64 一致）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBoardMathLibrary_TC_Boundary_Steps_Exactly2N,
    "Rento.Board.BoardMathLibrary.TC_Boundary_Steps_Exactly2N",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardMathLibrary_TC_Boundary_Steps_Exactly2N::RunTest(const FString& Parameters)
{
    // ------------------------------------------------------------------
    // case-a: Steps=80（|Steps|==2N=80，精确边界）
    // 断言：不触 Warning（不会有 Warning 日志），结果 {0, 2} 正确
    // 不需要 AddExpectedError（无 Error/Warning 产生）
    // ------------------------------------------------------------------
    {
        const FBoardAdvanceResult R =
            UBoardMathLibrary::AdvanceIndex(0, 80, BoardMathTestConst::N);

        TestEqual(TEXT("Boundary 2N: AdvanceIndex(0,80,40).NewIndex 应为 0（80 mod 40=0）"),
            R.NewIndex, 0);
        TestEqual(TEXT("Boundary 2N: AdvanceIndex(0,80,40).PassedGo 应为 2（两整圈）"),
            R.PassedGo, 2);
    }

    // ------------------------------------------------------------------
    // case-b: Steps=81（|Steps|==2N+1=81，超出边界，触 Warning）
    // Warning 不致 FAIL，不需要 AddExpectedError，直接断言结果
    // ------------------------------------------------------------------
    {
        const FBoardAdvanceResult R =
            UBoardMathLibrary::AdvanceIndex(0, 81, BoardMathTestConst::N);

        TestEqual(TEXT("Boundary 2N+1: AdvanceIndex(0,81,40).NewIndex 应为 1（81 mod 40=1）"),
            R.NewIndex, 1);
        TestEqual(TEXT("Boundary 2N+1: AdvanceIndex(0,81,40).PassedGo 应为 2（两整圈+1步）"),
            R.PassedGo, 2);
    }

    return true;
}
