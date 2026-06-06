// TestMatchSubsystem.h
// =============================================================================
// 测试专用最小派生类（test-only，仅在 rentoTests 模块内使用，不进 Shipping）
//
// 用途：
//   1. UMatchSubsystemBase 是 Abstract UCLASS，不能直接实例化，须具体子类实例化
//   2. UTestMatchSubsystem 持有实例级计数器，适用于通过 GetSubsystem 读取的场景（TC-3）
//   3. UTestMatchSubsystemCounter 持静态计数器，适用于 DestroyWorld 后仍可读的场景（TC-3b / Edge）
//
// 为何计数器验证非 vacuous（不自证）：
//   - 引擎 UWorld::CreateWorld → InitializeNewWorld → InitializeSubsystems 中，
//     引擎自动发现并创建所有注册的非 Abstract WorldSubsystem
//   - 若 ShouldCreateSubsystem 返回 false，Subsystem 不创建，GetSubsystem 返回 nullptr，
//     测试在 TestNotNull 处 FAIL
//   - 若 Initialize 被调用则计数 +1；初始为 0，若未调用则 TestEqual(count, 1) FAIL
//   - 若 Deinitialize 被调用则计数 +1；DestroyWorld 前为 0，若未调用则 TestEqual(count, 1) FAIL
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "TestMatchSubsystem.generated.h"

// =============================================================================
// UTestMatchSubsystem — 持实例级计数器（TC-3 用）
// 在 DestroyWorld 前通过 GetSubsystem 读取 InitializeCount
// =============================================================================
UCLASS()
class UTestMatchSubsystem : public UMatchSubsystemBase
{
	GENERATED_BODY()

public:
	/** Initialize 被引擎调用次数（初始为 0，期望：Game World 创建后为 1） */
	int32 InitializeCount = 0;

	/** Deinitialize 被引擎调用次数（初始为 0，期望：DestroyWorld 后为 1） */
	int32 DeinitializeCount = 0;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		// 先调基类（确保引擎内部 bHasCalledBeginPlay 等标志正确设置，
		// 否则后续 OnWorldBeginPlay 的 ensureAlwaysMsgf 会失败导致测试崩溃）
		Super::Initialize(Collection);
		++InitializeCount;
	}

	virtual void Deinitialize() override
	{
		// 先递增计数（ADR-0001 约定：子类先做自身清理，再调 Super）
		++DeinitializeCount;
		Super::Deinitialize();
	}
};

// =============================================================================
// 全局静态计数器（供 UTestMatchSubsystemCounter 使用）
// 文件级 static，不跨 Translation Unit 污染
// 需在测试函数开头重置：GTestInitCount = 0; GTestDeinitCount = 0;
// =============================================================================
extern int32 GTestInitCount;
extern int32 GTestDeinitCount;

// =============================================================================
// UTestMatchSubsystemCounter — 持静态计数器（TC-3b / Edge 用）
// 在 DestroyWorld 后仍可通过静态变量读取 DeinitializeCount
// =============================================================================
UCLASS()
class UTestMatchSubsystemCounter : public UMatchSubsystemBase
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
		++GTestInitCount;
	}

	virtual void Deinitialize() override
	{
		++GTestDeinitCount;
		Super::Deinitialize();
	}
};
