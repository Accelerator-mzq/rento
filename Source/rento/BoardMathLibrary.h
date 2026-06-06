// BoardMathLibrary.h
// =============================================================================
// 棋盘数学工具库（story-003，ADR-0002 §决策③ 逐字实现）
//
// 内容：
//   - FBoardAdvanceResult — AdvanceIndex 原子返回 struct（ADR-0002 §Key Interfaces）
//   - UBoardMathLibrary   — F-1/F-2/F-3 拓扑公式纯函数库（BlueprintPure 静态）
//
// 公式（GDD §Formulas，story-003 Implementation Notes 逐字）：
//   F-1: new_index     = mod(from + steps, N)        → 数学取余，结果恒 >= 0
//   F-2: passed_go     = floor((from+steps)/N) - floor(from/N)   → 通用式，向负无穷取整
//   F-3: steps_forward = mod(target - from, N)       → 数学取余，From==Target → 0
//
// 实现防护：
//   - int64 中间量防止 int32 加法溢出（棋盘 MVP 不会溢出，防御性）
//   - FloorDiv64 — 向负无穷整数除法（修正 C++ / 截断朝零）
//   - MathMod64  — 数学取余组合式 ((A%N)+N)%N（修正 C++ % 对负数返回负值）
//   - N<=0 → UE_LOG(Error) 防除零，返回安全值（NewIndex=From, PassedGo=0）
//   - From 越界 → UE_LOG(Error) 暴露上游损坏，通用式仍正确计算（AC-34，🟡 logged decision）
//   - |Steps|>2N → UE_LOG Warning 超界告警，绝不 clamp（AC-29）
//
// Out of Scope（严守 story-003 边界）：
//   - UBoardDataAsset / FBoardTileData → story-001
//   - GetTileCount() / N 的来源       → story-004（N 由调用方传入）
//   - 发薪裁决 / passed_go 消费        → 经济 epic
//   - 传送 F-3+F-1 串联               → 事件格 epic（AC-39）
//   - 独立公开 PassedGoCount 接口      → 禁止（AC-37b）
//
// 规范依据：
//   - ADR-0002 §Key Interfaces / §决策③（struct 返回强制原子性）
//   - story-003 AC-2/8~17/29/33/34/37a/37b
//   - control-manifest Foundation 层 §数据容器 (ADR-0002)
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BoardMathLibrary.generated.h"

// 日志分类（BoardMathLibrary 专用，与 LogTemp 分离）
DECLARE_LOG_CATEGORY_EXTERN(LogBoardMath, Log, All);

// =============================================================================
// FBoardAdvanceResult — AdvanceIndex 原子返回 struct
//
// ADR-0002 §Key Interfaces 逐字对齐：
//   USTRUCT(BlueprintType) — Blueprint 可 Break Struct 拆分两值
//   BlueprintReadOnly      — 只读（由 AdvanceIndex 填充，不可外部写入）
//
// 原子性保证（AC-37a）：
//   两值 NewIndex / PassedGo 由 AdvanceIndex 单次调用同时返回，
//   Blueprint 通过 Break Struct 节点拆分，无法单独取其中一值而不调 AdvanceIndex，
//   天然满足「不存在独立 PassedGoCount 可分离调用」契约（AC-37b）。
//
// ⚠ 注意：本 struct 在此定义（story-003 落地），BoardDataAsset.h 注释已 defer 至本 story。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FBoardAdvanceResult
{
    GENERATED_BODY()

    /**
     * 移动后的新格子索引（F-1：数学取余，结果恒 >= 0）。
     * 公式：new_index = mod(from + steps, N) = ((from+steps) % N + N) % N
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Math")
    int32 NewIndex = 0;

    /**
     * 经过 GO（索引 0）的次数（F-2：通用式，有符号）。
     * 公式：passed_go = floor((from+steps)/N) - floor(from/N)
     * 逆行时为负（AC-14: AdvanceIndex(5,-7).PassedGo == -1）。
     * 含义：正=顺时针经过 GO，负=逆时针穿越 GO，0=未经过。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Math")
    int32 PassedGo = 0;
};

// =============================================================================
// UBoardMathLibrary — 棋盘拓扑纯函数库
//
// 继承 UBlueprintFunctionLibrary（无状态静态，ADR-0002 §决策③）。
// 单测直测 C++ 函数，无需 Blueprint 运行环境（ADR-0001 §headless 可实例化前提）。
//
// ⚠ 不提供独立公开 PassedGoCount 接口（AC-37b 禁止）：
//   F-2 仅为 AdvanceIndex 返回元组 passed_go 分量，不可分离调用。
// =============================================================================
UCLASS()
class RENTO_API UBoardMathLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // =========================================================================
    // AdvanceIndex — 环路位移 + GO 计数（F-1 + F-2 原子返回）
    //
    // 计算棋子从 From 移动 Steps 步后的新位置，以及经过 GO（索引 0）的次数。
    //
    // @param From   当前格子索引（0-based，合法范围 [0, N)；越界记 UE_LOG Error，通用式仍正确）
    // @param Steps  移动步数（可为负：逆行；可超界：|Steps|>2N 照算不 clamp，记 Warning）
    // @param N      棋盘格子总数（必须 > 0，否则记 UE_LOG Error 并返回安全值）
    // @return       FBoardAdvanceResult{NewIndex, PassedGo}（原子返回，AC-37a）
    //
    // AC 覆盖：AC-2/8/9/10/11/12/13/14/15/29/33/34/37a
    // =========================================================================
    UFUNCTION(BlueprintPure, Category = "Board|Math",
              meta = (DisplayName = "Advance Index",
                      ToolTip = "环路位移（F-1）+ GO 计数（F-2）原子返回。N 由调用方传入（story-004 提供）。"))
    static FBoardAdvanceResult AdvanceIndex(int32 From, int32 Steps, int32 N);

    // =========================================================================
    // StepsBetween — 前向距离（F-3）
    //
    // 计算从 From 沿正方向到达 Target 所需的最少步数。
    // 等价于 mod(Target - From, N)，结果在 [0, N) 范围内。
    //
    // @param From   当前格子索引（0-based）
    // @param Target 目标格子索引（0-based）
    // @param N      棋盘格子总数（必须 > 0）
    // @return       [0, N) 范围内的前向步数；From == Target 返回 0（AC-17）
    //
    // AC 覆盖：AC-16/17
    // =========================================================================
    UFUNCTION(BlueprintPure, Category = "Board|Math",
              meta = (DisplayName = "Steps Between",
                      ToolTip = "前向距离（F-3）：从 From 顺时针到 Target 的最少步数。"))
    static int32 StepsBetween(int32 From, int32 Target, int32 N);

private:
    // =========================================================================
    // 内部辅助：向负无穷整数除法（floor division）
    //
    // C++ 整数除法 / 截断朝零（非 floor）。本函数修正为向负无穷取整，
    // 使 F-2 通用式 passed_go = FloorDiv64(from+steps, N) - FloorDiv64(from, N) 正确。
    //
    // 关键案例（AC-14 依赖）：
    //   FloorDiv64(-2, 40) == -1（C++ -2/40 == 0，错；floor(-2/40) == -1，正确）
    //   FloorDiv64(-1, 40) == -1（C++ -1/40 == 0，错；floor(-1/40) == -1，正确）
    //   FloorDiv64(43, 40) ==  1（正数无差异）
    //
    // @param A  被除数（int64，容纳 from+steps 不溢出）
    // @param B  除数（int64，即 N64，调用方已保证 > 0）
    // @return   floor(A / B)（向负无穷取整）
    // =========================================================================
    static int64 FloorDiv64(int64 A, int64 B);

    // =========================================================================
    // 内部辅助：数学取余（结果恒 >= 0，等价于 Python %）
    //
    // C++ 整数 % 对负数截断朝零（返回负余数），本函数修正为恒非负。
    // 公式：((A % N) + N) % N
    //
    // 关键案例（AC-10/F-1 依赖）：
    //   MathMod64(-2, 40) == 38（C++ -2%40 == -2，错；数学 mod(-2,40) == 38，正确）
    //   MathMod64(43, 40) ==  3（正数无差异）
    //
    // @param A  被取余数（int64）
    // @param N  模数（int64，调用方已保证 > 0）
    // @return   A mod N，结果 in [0, N)
    // =========================================================================
    static int64 MathMod64(int64 A, int64 N);
};
