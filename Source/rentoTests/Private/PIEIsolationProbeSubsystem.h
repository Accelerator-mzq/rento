// PIEIsolationProbeSubsystem.h
// =============================================================================
// ff-007 PIE 隔离验证 — 插桩 probe 子类
//
// 职责：
//   作为 UMatchSubsystemBase 的具体测试子类（concrete，非 Abstract），
//   在 PIE World 中被引擎自动创建（因基类 ShouldCreateSubsystem PIE-true）。
//   提供三类可观测探针：
//
//   1. 静态计数器（跨实例、PIE Stop 后仍可读）：
//      - GPIEProbeInitCount     : Initialize 被调用次数
//      - GPIEProbeDeinitCount   : Deinitialize 被调用次数
//      - GPIEProbeBeginPlayCount: OnWorldBeginPlay 被调用次数
//
//   2. per-instance dirty marker（证 PIE 隔离无跨局残留）：
//      InstanceDirtyValue 初值 = 0。
//      测试第一局写入非零值；第二局 GetSubsystem 拿到的是全新实例，
//      读到 0 → 证无残留（若引擎错误复用实例则读到上局写入值 → FAIL）。
//
//   3. BeginPlay 重触发标志（证 OnWorldBeginPlay 随 PIE Start 重触发）：
//      bBeginPlayFiredThisInstance 每个实例独立，OnWorldBeginPlay 置 true。
//      与静态 GPIEProbeBeginPlayCount 配合，第二局读到 true 证重触发。
//
// 约束（ADR-0001 + story-007 Out of Scope）：
//   - 不持任何游戏状态写字段（Cash/owner/house_count 等）
//   - 不改生产 Source/rento/ 下任何文件
//   - 静态计数器须在每个测试函数开头 Reset（测试隔离纪律）
//
// 规范依据：
//   - ADR-0001 §Verification Required ①②③
//   - story-007 AC-1~7（ff-007 PIE 隔离验证全部 AC）
//   - control-manifest Foundation Layer §宿主与生命周期 (ADR-0001)
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "PIEIsolationProbeSubsystem.generated.h"

// =============================================================================
// 全局静态计数器（供 PIE Stop 后仍可读的跨实例统计）
// 文件级 extern，定义在 PIEIsolationProbeSubsystem.cpp。
// 每个测试函数开头须 Reset：GPIEProbeInitCount = GPIEProbeDeinitCount = GPIEProbeBeginPlayCount = 0;
// =============================================================================
extern int32 GPIEProbeInitCount;
extern int32 GPIEProbeDeinitCount;
extern int32 GPIEProbeBeginPlayCount;

/**
 * UPIEIsolationProbeSubsystem — ff-007 PIE 隔离插桩 probe
 *
 * per-match UWorldSubsystem，挂载于 Game / PIE World（继承基类 ShouldCreateSubsystem）。
 * 所有计数器与 marker 仅为测试观测，不持任何游戏状态语义。
 *
 * 使用方式：
 *   1. 重置静态计数器（GPIEProbeInitCount = GPIEProbeDeinitCount = GPIEProbeBeginPlayCount = 0）
 *   2. 触发 PIE Start → 引擎自动创建本 Subsystem → 计数器 +1
 *   3. 触发 PIE Stop → Deinitialize 计数器 +1
 *   4. 第二局复用测试：GetSubsystem → 读 InstanceDirtyValue（期望 0，证无跨局残留）
 */
UCLASS()
class UPIEIsolationProbeSubsystem : public UMatchSubsystemBase
{
	GENERATED_BODY()

public:
	// =========================================================================
	// per-instance dirty marker（AC-2 PIE 隔离证明）
	// 测试流程：第一局写入 = 非零值 → 第二局读到 = 0（全新实例，无跨局残留）
	// =========================================================================

	/** per-instance 脏标记值（初值 0）。第一局写入非零；第二局读到 0 证 PIE 隔离。*/
	int32 InstanceDirtyValue = 0;

	// =========================================================================
	// BeginPlay 重触发 flag（AC-3 证明）
	// =========================================================================

	/** 本实例 OnWorldBeginPlay 是否已触发（每局实例独立，初值 false）*/
	bool bBeginPlayFiredThisInstance = false;

	// =========================================================================
	// UMatchSubsystemBase 生命周期 override
	// =========================================================================

	/** Initialize：递增 GPIEProbeInitCount（AC-1 触发计数） */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** OnWorldBeginPlay：递增 GPIEProbeBeginPlayCount，置 bBeginPlayFiredThisInstance（AC-3） */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Deinitialize：递增 GPIEProbeDeinitCount（AC-1 销毁计数） */
	virtual void Deinitialize() override;
};
