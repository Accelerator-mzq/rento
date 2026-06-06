// BoardMathLibrary.cpp
// =============================================================================
// 棋盘数学工具库实现（story-003，ADR-0002 §决策③）
//
// 实现要点：
//   1. FloorDiv64  — 向负无穷取整除法（修正 C++ / 截断朝零）
//      FloorDiv64(-1, 40) == -1（非 0）—— AC-14/AC-34 PassedGo 正确的关键
//   2. MathMod64   — 数学取余 ((A%N)+N)%N（修正 C++ % 负数返回负值）
//      MathMod64(-2, 40) == 38 —— AC-10 NewIndex 正确的关键
//   3. int64 中间量 — 防止 int32 加法溢出（廉价防御性）
//   4. N<=0 → UE_LOG Error + 返回安全值，防除零
//   5. From 越界 → UE_LOG Error + 通用式仍正确计算（AC-34，🟡 logged decision：
//      循 FF-003 惯例用 UE_LOG 替代 ensureMsgf，headless Automation 中 ensure
//      产生不稳定 callstack dump 导致 AddExpectedError 计数不可靠）
//   6. |Steps|>2N → UE_LOG Warning，绝不 clamp（AC-29）
// =============================================================================

#include "BoardMathLibrary.h"

// 定义日志分类（LogBoardMath，DECLARE 在 .h，DEFINE 在 .cpp）
DEFINE_LOG_CATEGORY(LogBoardMath);

// =============================================================================
// FloorDiv64 — 向负无穷整数除法
//
// C++ 整数 / 截断朝零（Truncation Division），不满足向负无穷取整（Floor Division）。
// 本函数修正：当余数非零且 A、B 异号（商为负）时，结果向下再减 1。
//
// 数学原理：
//   floor(A/B) = trunc(A/B) - 1  当 A%B != 0 且 A 与 B 异号
//   floor(A/B) = trunc(A/B)      其余情况
//
// 典型正确性验证（AC-14 依赖）：
//   FloorDiv64(-2, 40):  C++ -2/40==0,  -2%40==-2（非零且异号）→ 0-1 == -1 ✓
//   FloorDiv64(-1, 40):  C++ -1/40==0,  -1%40==-1（非零且异号）→ 0-1 == -1 ✓
//   FloorDiv64(43, 40):  C++ 43/40==1,  43%40==3 （非零但同号）→ 1   ==  1 ✓
//   FloorDiv64(40, 40):  C++ 40/40==1,  40%40==0 （余数零）    → 1   ==  1 ✓
//   FloorDiv64(-40, 40): C++ -40/40==-1,-40%40==0（余数零）    → -1  == -1 ✓
// =============================================================================
int64 UBoardMathLibrary::FloorDiv64(int64 A, int64 B)
{
    // C++ 标准截断朝零的商和余数
    const int64 Q = A / B;
    const int64 R = A % B;

    // 当余数非零且 A、B 异号时，截断商比 floor 商大 1，须减 1
    // (A ^ B) < 0 利用 XOR 最高位检测异号（int64 有符号，最高位为符号位）
    const bool bNeedCorrection = (R != 0) && ((A ^ B) < 0);
    return Q - (bNeedCorrection ? 1 : 0);
}

// =============================================================================
// MathMod64 — 数学取余（结果恒 >= 0，Python % 语义）
//
// C++ 整数 % 遵循截断朝零规则，对负被除数返回负余数（如 -2%40==-2）。
// 本函数用组合式修正为恒非负（数学取余 = Euclidean Modulo）：
//   ((A % N) + N) % N
//
// 分析：
//   若 A%N >= 0（非负余数），加 N 后再 %N，结果不变（N <= 2N-1，%N 后仍原值）。
//   若 A%N <  0（负余数），加 N 后变为正，再 %N 得到正确正余数。
//
// 典型正确性验证（AC-10/F-3 依赖）：
//   MathMod64(-2,  40): C++ -2%40==-2,   (-2+40)%40==38 ✓
//   MathMod64(-31, 40): C++ -31%40==-31, (-31+40)%40==9 ✓ (AC-16 StepsBetween(36,5)=9)
//   MathMod64(43,  40): C++ 43%40==3,    (3+40)%40==3   ✓
//   MathMod64(0,   40): C++ 0%40==0,     (0+40)%40==0   ✓
// =============================================================================
int64 UBoardMathLibrary::MathMod64(int64 A, int64 N)
{
    return ((A % N) + N) % N;
}

// =============================================================================
// AdvanceIndex — 环路位移 + GO 计数（F-1 + F-2 原子返回）
// =============================================================================
FBoardAdvanceResult UBoardMathLibrary::AdvanceIndex(int32 From, int32 Steps, int32 N)
{
    // ------------------------------------------------------------------
    // N 防护：N <= 0 会导致除零，必须最先检查
    // 用 UE_LOG Error（非 ensure）：循 FF-003 惯例，headless 测试中 ensure
    // 产生不稳定 callstack dump，UE_LOG Error 每次调用恰好一行，更易管理
    // ------------------------------------------------------------------
    if (N <= 0)
    {
        UE_LOG(LogBoardMath, Error,
            TEXT("UBoardMathLibrary::AdvanceIndex — N 无效: N=%d（必须 > 0），返回安全值"),
            N);
        // 返回安全值（原位，PassedGo=0），不计算（无法除 N）
        FBoardAdvanceResult Safe;
        Safe.NewIndex = From;
        Safe.PassedGo = 0;
        return Safe;
    }

    // ------------------------------------------------------------------
    // AC-34：From 越界检查
    // 🟡 logged decision（循 FF-003 惯例）：用 UE_LOG Error 替代 ensureMsgf。
    // headless Automation 中 ensureMsgf 产生不稳定 callstack dump 导致
    // AddExpectedError 计数不可靠；UE_LOG Error 每次调用恰好一行，可精确吞掉。
    // 可测语义：不崩溃 + 记 Error 日志 + 通用式仍正确返回（AC-34 全满足）。
    // ------------------------------------------------------------------
    if (From < 0 || From >= N)
    {
        UE_LOG(LogBoardMath, Error,
            TEXT("UBoardMathLibrary::AdvanceIndex — From 越界: From=%d, N=%d，通用式仍计算"),
            From, N);
        // 不 return，通用式鲁棒自修正，照样计算（AC-34 明确要求「仍正确返回」）
    }

    // ------------------------------------------------------------------
    // AC-29：超界步数告警（|Steps| > 2*N）
    // 照常计算，绝不 clamp（clamp 破坏落点）；记 Warning 供排查上游 bug
    // UE Automation 不因 Warning 判 FAIL（只有 Error 才判 FAIL）
    // ------------------------------------------------------------------
    if (FMath::Abs(Steps) > 2 * N)
    {
        UE_LOG(LogBoardMath, Warning,
            TEXT("AdvanceIndex: 步数超界，Steps=%d 超过 2*N=%d，照常计算不 clamp"),
            Steps, 2 * N);
    }

    // ------------------------------------------------------------------
    // 计算：用 int64 中间量防止 int32 加法溢出
    // ------------------------------------------------------------------
    const int64 From64  = static_cast<int64>(From);
    const int64 Steps64 = static_cast<int64>(Steps);
    const int64 N64     = static_cast<int64>(N);

    // F-1: new_index = mod(from + steps, N)
    // MathMod64 保证结果恒 >= 0（数学取余，非 C++ % 截断）
    const int64 Sum64      = From64 + Steps64;
    const int64 NewIndex64 = MathMod64(Sum64, N64);

    // F-2: passed_go = floor((from+steps)/N) - floor(from/N)
    // FloorDiv64 保证向负无穷取整（修正 C++ / 截断朝零）
    // 通用式，任何 from/steps 组合均可正确处理（含逆行、多圈、越界 from）
    const int64 PassedGo64 = FloorDiv64(Sum64, N64) - FloorDiv64(From64, N64);

    // 棋盘 N 最大数十格，PassedGo 不超过 Steps/N 量级，int32 完全容纳
    FBoardAdvanceResult Result;
    Result.NewIndex = static_cast<int32>(NewIndex64);
    Result.PassedGo = static_cast<int32>(PassedGo64);
    return Result;
}

// =============================================================================
// StepsBetween — 前向距离（F-3）
// =============================================================================
int32 UBoardMathLibrary::StepsBetween(int32 From, int32 Target, int32 N)
{
    // ------------------------------------------------------------------
    // N 防护：N <= 0 无法取余，防除零
    // ------------------------------------------------------------------
    if (N <= 0)
    {
        UE_LOG(LogBoardMath, Error,
            TEXT("UBoardMathLibrary::StepsBetween — N 无效: N=%d（必须 > 0），返回 0"),
            N);
        return 0;
    }

    const int64 From64   = static_cast<int64>(From);
    const int64 Target64 = static_cast<int64>(Target);
    const int64 N64      = static_cast<int64>(N);

    // F-3: steps_forward = mod(target - from, N)
    // MathMod64 保证结果恒 >= 0（数学取余）
    // From == Target → mod(0, N) == 0（AC-17）
    // StepsBetween(36, 5, 40) → mod(5-36, 40) = mod(-31, 40) = 9（AC-16）
    return static_cast<int32>(MathMod64(Target64 - From64, N64));
}
