// ActionValidatorInterface.h
// =============================================================================
// IActionValidator — PostRollAction 逐动作执行期可行性重校验 DI 接缝（story pt-007 AC-6 / AC-42）
//
// 架构约束（仿 IResolveBankruptcy / BankruptcyInterface.h 范式）：
//   - 纯 C++ 接口（非 UInterface）：headless 可 stub/spy 注入，无需 UHT
//   - TSharedPtr<IActionValidator> 持有，生命周期安全（与 BankruptcyResolver 惯例一致）
//   - 纯查询接口：IsActionFeasible 禁止修改任何游戏状态（查询腿 / 执行腿分离）
//
// 设计意图（GDD CR-8 批处理失败策略 / ADR-0006 IG#8）：
//   一条 PostRollAction 的可行性由其 ActionType 的主责系统裁定：
//     BuyProperty  → 所有权(6)裁定
//     BuildHouse   → 建房(8) 裁定
//     MortgageProperty / UnmortgageProperty → 经济(5)裁定
//   本接缝抽象"委派对应规则系统重校验"这一动作。
//   生产实现归簇C/各规则 epic。本簇仅定义接缝 + FActionValidatorSpy。
//
// 失败策略（GDD CR-8 / AC-42）：
//   IsActionFeasible 返回 false → 框架静默跳过该动作（UE_LOG Warning）+ continue。
//   后续合法动作仍正常执行（不中止整批）。
//
// 规范依据：
//   - story pt-007 AC-6 / AC-42
//   - ADR-0006（GameStateSnapshot 值语义、决策视图）
//   - ADR-0007（回合状态机 C++ 落地，headless 可测）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "AIDecisionMakerInterface.h"   // FTurnAction 完整定义（const& 传参需完整类型）

// =============================================================================
// IActionValidator — PostRollAction 逐动作执行期可行性重校验 DI 接缝
//
// 接口形态（仿 IResolveBankruptcy）：
//   - 纯 C++ 接口，非 UInterface
//   - TSharedPtr<IActionValidator> 持有
//   - 单纯虚方法 IsActionFeasible，= 0，无副作用
//
// 注入场景：
//   - 测试注入 FActionValidatorSpy（按调用顺序消费受控可行性结果）
//   - 生产注入由簇C/规则 epic 提供委派实现
//   - nullptr（未注入）时 RunAiPostRollActions 退化为"全部可行"（向后兼容）
// =============================================================================
class RENTO_API IActionValidator
{
public:
    /** 虚析构，保证正确 delete 派生类（TSharedPtr 正确释放） */
    virtual ~IActionValidator() = default;

    /**
     * 执行期重校验：单条动作在当前实时状态下是否可行。
     *
     * 委派对应规则系统（所有权/建房/经济）裁定可行性。
     * 回合2 据返回值决定是否执行该动作。
     *
     * @param Action 待校验的 PostRollAction 动作
     * @return true  = 可行（框架执行该动作）
     *         false = 不可行（框架静默跳过 + UE_LOG Warning + continue，不中止整批）
     *
     * ⚠ 实现禁止修改任何游戏状态（纯查询；状态变更归动作执行腿，非校验腿）
     */
    virtual bool IsActionFeasible(const FTurnAction& Action) = 0;
};
