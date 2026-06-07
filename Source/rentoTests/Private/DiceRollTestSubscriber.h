// DiceRollTestSubscriber.h
// =============================================================================
// story-002 测试专用订阅桩：UDiceRollTestSubscriber
//
// 职责：
//   AC-16c（广播==返回同源）需要一个 UObject 订阅者，以 AddDynamic 挂接
//   OnDiceRolled（DYNAMIC_MULTICAST_DELEGATE 要求回调方法为 UFUNCTION + UObject 宿主）。
//   本类仅捕获最近一次广播的 FDiceRollResult payload，供断言对比。
//
// 设计决策：
//   - 独立 .h 文件（UHT 需要 .generated.h 配对，内联 UCLASS 到 .cpp 不被 UHT 支持）
//   - 遵循 TestMatchSubsystem.h / TestDataAssetHost.h 的桩惯例
//   - 不进 Shipping：rentoTests 模块独立，Editor/Development only
//
// 非 vacuous 保证：
//   - 若 RollDice 实现「广播后重读流返回」，返回值多消耗 Seed → 四字段与 payload 不等
//     → AC-16c TestEqual 立即 FAIL（设计上能检测出实现 bug）
//
// 物理路径：Source/rentoTests/Private/DiceRollTestSubscriber.h
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DiceRngService.h"          // FDiceRollResult、FOnDiceRolled
#include "DiceRollTestSubscriber.generated.h"

/**
 * UDiceRollTestSubscriber — OnDiceRolled 最小订阅桩（AC-16c 专用）
 *
 * 使用示例（测试函数内）：
 * @code
 *   UDiceRollTestSubscriber* Sub = NewObject<UDiceRollTestSubscriber>(GetTransientPackage());
 *   Service->OnDiceRolled.AddDynamic(Sub, &UDiceRollTestSubscriber::OnRollReceived);
 *   FDiceRollResult RetVal = Service->RollDice();
 *   // 断言 RetVal 四字段 == Sub->CapturedResult 四字段
 * @endcode
 */
UCLASS()
class UDiceRollTestSubscriber : public UObject
{
	GENERATED_BODY()

public:
	/** 最近一次广播捕获的 payload（AC-16c 断言对象） */
	FDiceRollResult CapturedResult;

	/** 是否已收到至少一次广播（防 vacuous：未触发则 bReceived==false，AC-16c 可提早检测） */
	bool bReceived = false;

	/**
	 * OnDiceRolled 回调（UFUNCTION 必须，AddDynamic 要求）。
	 * 捕获广播 payload 到成员，供测试断言读取。
	 *
	 * ADR-0004 Guideline 5 要求：回调内禁同步调任何抽取 API（重入污染权威序列）。
	 * 本桩仅做赋值，满足此约束。
	 *
	 * @param Result 广播的掷骰结果 payload
	 */
	UFUNCTION()
	void OnRollReceived(FDiceRollResult Result)
	{
		// 仅赋值，不调任何 RNG API（ADR-0004 重入禁止，story-005 加完整门控）
		CapturedResult = Result;
		bReceived = true;
	}
};
