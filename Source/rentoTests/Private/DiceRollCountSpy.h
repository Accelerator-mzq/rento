// DiceRollCountSpy.h
// =============================================================================
// story-005 测试专用计数 spy：UDiceRollCountSpy
//
// 职责：
//   AC-15（OnDiceRolled 触发次数精确 == N）需要一个 UObject 订阅者只累加计数，
//   不捕获 payload（与 UDiceRollTestSubscriber 互补）。
//
// 设计决策：
//   - 独立 .h 文件：UHT 要求 UCLASS 须在独立头文件配对 .generated.h
//   - 回调内仅累加 ReceiveCount，绝不调任何 RNG API（ADR-0004 Guideline 5 重入禁止）
//   - 不进 Shipping：rentoTests 模块独立，Editor/Development only
//
// 非 vacuous 保证：
//   - 若 RollDice 省略广播步骤③，ReceiveCount==0 < N → TestEqual FAIL（漏触发）
//   - 若 RollDice 意外双播，ReceiveCount > N → TestEqual FAIL（多触发）
//
// 物理路径：Source/rentoTests/Private/DiceRollCountSpy.h
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DiceRngService.h"         // FDiceRollResult
#include "DiceRollCountSpy.generated.h"

/**
 * UDiceRollCountSpy — OnDiceRolled 触发次数计数桩（AC-15 专用）
 *
 * 回调内仅累加 ReceiveCount，不调任何抽取 API（ADR-0004 Guideline 5）。
 *
 * 使用示例（测试函数内）：
 * @code
 *   UDiceRollCountSpy* Spy = NewObject<UDiceRollCountSpy>(GetTransientPackage());
 *   Service->OnDiceRolled.AddDynamic(Spy, &UDiceRollCountSpy::OnRollReceived);
 *   // ...连续 N 次 RollDice()
 *   TestEqual("触发次数", Spy->ReceiveCount, N);
 * @endcode
 */
UCLASS()
class UDiceRollCountSpy : public UObject
{
    GENERATED_BODY()

public:
    /** 累计触发次数（AC-15 断言对象） */
    int32 ReceiveCount = 0;

    /**
     * OnDiceRolled 回调（UFUNCTION 必须，AddDynamic 要求）。
     * 仅累加 ReceiveCount，绝不调任何 RNG API（ADR-0004 Guideline 5 重入禁止）。
     *
     * @param Result 广播的掷骰结果 payload（本桩不使用，仅计数）
     */
    UFUNCTION()
    void OnRollReceived(FDiceRollResult Result)
    {
        // 仅计数，不调任何 RNG API（ADR-0004 重入禁止）
        ++ReceiveCount;
    }
};
