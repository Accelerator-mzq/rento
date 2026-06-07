// PIEIsolationProbeSubsystem.cpp
// =============================================================================
// ff-007 PIE 隔离验证 probe 子类实现
// 规范依据：ADR-0001 §Verification Required ①②③、story-007 AC-1~7
// =============================================================================
#include "PIEIsolationProbeSubsystem.h"

// =============================================================================
// 全局静态计数器定义（extern 声明在头文件）
// 每个 PIE 测试函数开头须重置这三个计数器，保证测试隔离。
// =============================================================================

/** Initialize 触发次数（AC-1 观测点，PIE Start 每局应 +1）*/
int32 GPIEProbeInitCount = 0;

/** Deinitialize 触发次数（AC-1 观测点，PIE Stop 每局应 +1）*/
int32 GPIEProbeDeinitCount = 0;

/** OnWorldBeginPlay 触发次数（AC-3 观测点，PIE 每次 Start 应 +1）*/
int32 GPIEProbeBeginPlayCount = 0;

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------

void UPIEIsolationProbeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 先调基类：建立 IGameClock DI 注入点（FWorldGameClock），设置引擎内部初始化标志
	Super::Initialize(Collection);

	// 递增静态计数器（AC-1：每次 PIE Start 应恰好 +1）
	++GPIEProbeInitCount;

	// per-instance dirty marker 初值保持 0（不在此处写入——
	// 写入由测试主动在 PIE World 内调 GetSubsystem 拿到实例后写入，
	// 以模拟「第一局写状态，第二局验无残留」场景）
}

// ----------------------------------------------------------------------------
// OnWorldBeginPlay
// ----------------------------------------------------------------------------

void UPIEIsolationProbeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	// 先调基类（设置引擎内部 bHasCalledBeginPlay 标志，否则后续 ensure 可能失败）
	Super::OnWorldBeginPlay(InWorld);

	// 递增静态计数器（AC-3：PIE Stop→Start 后第二次触发应使计数再 +1）
	++GPIEProbeBeginPlayCount;

	// per-instance BeginPlay 标志（AC-3：每个实例应在本局 Start 时被触发）
	bBeginPlayFiredThisInstance = true;
}

// ----------------------------------------------------------------------------
// Deinitialize
// ----------------------------------------------------------------------------

void UPIEIsolationProbeSubsystem::Deinitialize()
{
	// 先递增计数器（AC-1：每次 PIE Stop 应恰好 +1）
	// 注：在 Super::Deinitialize 之前递增，与 TestMatchSubsystem.h 保持一致惯例
	++GPIEProbeDeinitCount;

	// 调基类（重置 GameClock TSharedPtr、执行引擎内部清理）
	Super::Deinitialize();
}
