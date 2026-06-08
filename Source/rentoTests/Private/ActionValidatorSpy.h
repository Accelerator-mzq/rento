// ActionValidatorSpy.h
// =============================================================================
// FActionValidatorSpy — IActionValidator 测试 spy（story pt-007 AC-6 / AC-42）
//
// 用途：
//   TC-42a / TC-42b 要求断言：
//     ① 被校验为不可行的动作不进入 LastExecutedActions（目标状态未变）
//     ② 不可行动作之后的合法动作仍然执行（AC-42② 批处理非中止）
//     ③ IsActionFeasible 调用次数 == 动作列表长度（每动作都重校验）
//   本 spy 按调用顺序消费受控可行性列表，记录调用次数。
//
// 机制（G3 约束适用）：
//   IActionValidator 是纯 C++ 接口（非 UInterface），spy 为纯 struct 不需 UCLASS/UHT。
//   直接继承 IActionValidator + 实现纯虚方法 IsActionFeasible。
//   TSharedPtr<IActionValidator> 注入 UPlayerTurnSubsystem（DI 接缝）。
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
//
// 参照范本：
//   - AIDecisionMakerSpy.h（相同纯 C++ DI spy 模式）
//   - LandingResolverSpy.h（相同纯 C++ struct spy）
//
// 规范依据：
//   - story pt-007 AC-6 / AC-42
//   - ActionValidatorInterface.h（IActionValidator 纯 C++ 接口）
//   - G3：纯 C++ 接口 spy 用 struct，不需 UHT/UCLASS
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "ActionValidatorInterface.h"
#include "AIDecisionMakerInterface.h"

// =============================================================================
// FActionValidatorSpy — 按调用序返回受控可行性（测试专用）
//
// 使用示例（TC-42a：抵押可行、建房不可行）：
// @code
//   TSharedPtr<FActionValidatorSpy> Spy = MakeShared<FActionValidatorSpy>();
//   Spy->FeasibilityByCallOrder = { true, false };  // 第1调用可行，第2调用不可行
//   Sub->SetActionValidator(Spy);
//   Sub->RunAiPostRollActions(AiPlayerId, Snapshot);
//   TestEqual(TEXT("IsActionFeasible 调用次数==2"), Spy->IsActionFeasibleCallCount, 2);
// @endcode
//
// 超出 FeasibilityByCallOrder 索引时默认返回 true（可行），防越界崩溃。
// =============================================================================
struct FActionValidatorSpy final : public IActionValidator
{
    /**
     * 按调用顺序消费的可行性结果列表。
     *
     * 第 N 次调用返回 FeasibilityByCallOrder[N]。
     * 若调用次数超过列表长度，默认返回 true（可行）。
     *
     * 使用惯例：
     *   TC-42a: {true, false}          → 第1可行，第2不可行
     *   TC-42b: {true, false, true}    → 第1可行，第2不可行，第3可行
     */
    TArray<bool> FeasibilityByCallOrder;

    /**
     * IsActionFeasible 被调用次数（断言"每动作执行前都重校验"）。
     *
     * TC-42a 期望 == 2（两条动作各校验一次）
     * TC-42b 期望 == 3（三条动作各校验一次）
     */
    int32 IsActionFeasibleCallCount = 0;

    /**
     * 执行期可行性重校验 spy 实现。
     *
     * 按 IsActionFeasibleCallCount 作索引消费 FeasibilityByCallOrder；
     * 超出范围时返回 true（默认可行，防测试越界崩溃）。
     */
    virtual bool IsActionFeasible(const FTurnAction& /*Action*/) override
    {
        const int32 Idx = IsActionFeasibleCallCount++;
        // 按调用序消费受控可行性；超出列表长度默认 true（向后兼容）
        return FeasibilityByCallOrder.IsValidIndex(Idx) ? FeasibilityByCallOrder[Idx] : true;
    }
};
