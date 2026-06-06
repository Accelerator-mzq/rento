// TestDataAssetHost.h
// =============================================================================
// story-003 测试专用桩：UTestConcreteDataAsset + UTestDataAssetHost
//
// 职责：
//   - UPrimaryDataAsset / UDataAsset 均为 Abstract，不能直接 NewObject<> 实例化；
//     UTestConcreteDataAsset 提供非 Abstract 的 concrete 测试替身，供 MakeTransientDARef 使用
//   - UDataAssetHostSubsystem 是 Abstract UCLASS，不能直接实例化，须具体子类
//   - 暴露 protected 加载入口（CallRequestAsyncLoad）供测试直接驱动
//   - 提供 spy 计数器（CompletionCallbackWriteCount）和 bCancelObserved 观察点，
//     使测试能断言「回调写字段」次数与「CancelHandle 是否被调」
//
// 为何断言非 vacuous：
//   - 若 RequestAsyncLoadDataAsset 未写 AsyncLoadHandle，TC-1 通过 bHandleNonNullAfterRequest spy FAIL
//   - 若完成回调未写 LoadedDataAsset，TC-1b FAIL（TestNotNull）
//   - 若 Deinitialize 未调 CancelHandle，handle->WasCanceled() == false → TC-2 GDAHostLastCancelObserved FAIL
//   - 若 guard 失效（竞态回调写了 deinit 后的宿主），CompletionCallbackWriteCount 增 → TC-3 FAIL
//
// 物理路径：Source/rentoTests/Private/TestDataAssetHost.h
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataAssetHostSubsystem.h"
#include "TestDataAssetHost.generated.h"

// =============================================================================
// UTestConcreteDataAsset — 非 Abstract 的测试 DA 替身
//
// UDataAsset / UPrimaryDataAsset 均被 UCLASS(abstract) 标记，
// 不能直接用 NewObject<> 实例化（引擎 ensure 报错）。
// 本类继承 UPrimaryDataAsset，去掉 abstract 限制，
// 供 MakeTransientDARef() 在内存中创建 transient DA 实例。
// =============================================================================
UCLASS()
class UTestConcreteDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
};

// =============================================================================
// 全局静态计数器（供跨 DestroyWorld 场景的断言使用）
// 须在每个测试函数头部重置：GDAHostInitCount = 0; GDAHostDeinitCount = 0;
// =============================================================================
extern int32 GDAHostInitCount;    // Initialize 静态计数
extern int32 GDAHostDeinitCount;  // Deinitialize 静态计数

/**
 * F2 测试 seam：CancelHandle 路径断言镜像（static global，DestroyWorld 后仍可读）。
 *
 * 写入时序：
 *   1. DestroyWorld() 触发 Subsystem::Deinitialize()
 *   2. UTestDataAssetHost::Deinitialize() → Super::Deinitialize() → CancelAndReleaseHandle()
 *      → bLastHandleWasCanceled = handle->WasCanceled()
 *   3. Super::Deinitialize() 返回后
 *   4. UTestDataAssetHost::Deinitialize() 写 GDAHostLastCancelObserved = DidLastHandleCancel()
 *   5. TC-2 在 DestroyWorld() 之后断言 GDAHostLastCancelObserved == true
 *
 * 须在 TC-2 函数头部重置：GDAHostLastCancelObserved = false;
 */
extern bool GDAHostLastCancelObserved;

/**
 * UTestDataAssetHost — story-003 测试桩（具体子类，打开 protected 加载入口）
 *
 * 使用示例（测试函数内）：
 * @code
 *   UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TestWorld"));
 *   UTestDataAssetHost* Host = World->GetSubsystem<UTestDataAssetHost>();
 *   TSoftObjectPtr<UPrimaryDataAsset> Ref = MakeTransientDARef();
 *   Host->CallRequestAsyncLoad(Ref);         // 驱动异步加载
 *   // ... 同步完成检测 ...
 *   TestTrue("handle 非空", Host->bHandleNonNullAfterRequest);
 * @endcode
 */
UCLASS()
class UTestDataAssetHost : public UDataAssetHostSubsystem
{
	GENERATED_BODY()

public:
	// =========================================================================
	// spy 观察点（测试专用，由钩子函数写入，测试断言读取）
	// =========================================================================

	/**
	 * RequestAsyncLoadDataAsset 调用后，handle 是否非空（AC-1 断言点）。
	 * 条件：LoadState == Loading || LoadState == Loaded
	 * （Loading 表示 handle 已建且加载中；Loaded 表示同步完成路径直接到达 Loaded）
	 * 在 CallRequestAsyncLoad 调用后检查此标志。
	 */
	bool bHandleNonNullAfterRequest = false;

	/**
	 * 完成回调已写入 LoadedDataAsset 的次数（AC-2/TC-2 断言点）。
	 * 每次 OnDataAssetLoaded 被调用则 +1。
	 * Deinitialize 后若仍增加，说明 guard 失效（TC-2/TC-3 FAIL 条件）。
	 */
	int32 CompletionCallbackWriteCount = 0;

	/**
	 * 暴露加载入口给测试调用（绕过 Initialize 内部时序，直接驱动加载路径）。
	 *
	 * @param SoftAssetRef 要加载的 UPrimaryDataAsset 软引用
	 * @return 同 RequestAsyncLoadDataAsset 返回值
	 */
	bool CallRequestAsyncLoad(const TSoftObjectPtr<UPrimaryDataAsset>& SoftAssetRef)
	{
		bool bResult = RequestAsyncLoadDataAsset(SoftAssetRef);

		// 加载发起后，读取 handle 是否非空（AC-1 spy）
		// Loading 或 Loaded 都说明 handle 已被建立（同步完成路径会直接到 Loaded）
		bHandleNonNullAfterRequest = (GetLoadState() == EHostLoadState::Loading
		                           || GetLoadState() == EHostLoadState::Loaded);

		return bResult;
	}

	/**
	 * 暴露 MarkAsValidated 给测试调用（完成 Loaded → Validated 推进）。
	 *
	 * @return 同 MarkAsValidated 返回值
	 */
	bool CallMarkAsValidated()
	{
		return MarkAsValidated();
	}

	/**
	 * 透传 DidLastHandleCancel() 给测试（供测试不经 static global 直接读）。
	 * 与 GDAHostLastCancelObserved 的区别：此方法在 Deinitialize 前仍可访问；
	 * GDAHostLastCancelObserved 是 DestroyWorld 后的唯一安全读取点。
	 */
	bool GetDidCancelOnDeinit() const { return DidLastHandleCancel(); }

	/**
	 * 暴露 F11 测试 seam：强制 Loading 态 + 以给定路径驱动 ResolveAndStoreAsset。
	 * 用于确定性触发「解析失败 → 回退 Empty」分支（见基类 DriveResolveForTesting 注释）。
	 *
	 * @param AssetPath 测试传入不可解析路径以触发失败分支
	 */
	void CallDriveResolveForTesting(const FSoftObjectPath& AssetPath)
	{
		DriveResolveForTesting(AssetPath);
	}

	// =========================================================================
	// 生命周期 override（注入静态计数器 + spy 检查）
	// =========================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
		++GDAHostInitCount;
	}

	virtual void Deinitialize() override
	{
		// 递增 Deinit 计数（须在 Super 前，保持与 story-001 一致的「先自身后 Super」顺序）
		++GDAHostDeinitCount;

		// Super::Deinitialize 调 CancelAndReleaseHandle → 设 bLastHandleWasCanceled
		Super::Deinitialize();

		// F2 seam：Super 返回后读取 DidLastHandleCancel()（bLastHandleWasCanceled 已写入）
		// 写入静态镜像供 TC-2 在 DestroyWorld() 之后断言（宿主已被 GC，不能再解引用）
		GDAHostLastCancelObserved = DidLastHandleCancel();
	}

protected:
	/**
	 * 完成回调钩子 override：记录 spy 计数（AC-2 / TC-2 断言点）。
	 * Super::OnDataAssetLoaded 为空实现，无需调用。
	 */
	virtual void OnDataAssetLoaded(UPrimaryDataAsset* LoadedAsset) override
	{
		Super::OnDataAssetLoaded(LoadedAsset);
		// 每次回调写入 LoadedDataAsset 则计数 +1（TC-2 断言：Deinitialize 后此值不再增）
		++CompletionCallbackWriteCount;
	}
};
