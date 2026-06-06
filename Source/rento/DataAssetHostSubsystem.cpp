// DataAssetHostSubsystem.cpp
// =============================================================================
// DA 持有者宿主中间层实现（story-003）
// 规范依据：ADR-0001 §3 异步加载纪律、ADR-0002 防 GC、story-003 AC-1~5
// =============================================================================
#include "DataAssetHostSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

// =============================================================================
// ActivateDataAsset — 推进至 Active 态（AC-5 热切换边界）
// =============================================================================
bool UDataAssetHostSubsystem::ActivateDataAsset()
{
	// 仅 Validated 态允许推进至 Active
	if (LoadState != EHostLoadState::Validated)
	{
		// 已经是 Active，或前提态不满足（Empty/Loading/Loaded），拒绝
		return false;
	}

	LoadState = EHostLoadState::Active;
	return true;
}

// =============================================================================
// RequestReset — Active 态重置入口（AC-5 热替换防线）
// =============================================================================
bool UDataAssetHostSubsystem::RequestReset()
{
	// 仅 Active 态允许经此入口重置
	if (LoadState != EHostLoadState::Active)
	{
		return false;
	}

	// 取消并释放当前 handle（若 Loading 中也应 Cancel）
	CancelAndReleaseHandle();

	// 清空 DA 强引用，回到 Empty 态
	LoadedDataAsset = nullptr;
	LoadState       = EHostLoadState::Empty;

	return true;
}

// =============================================================================
// RequestAsyncLoadDataAsset — 发起异步加载（AC-1 异步加载纪律核心）
// =============================================================================
bool UDataAssetHostSubsystem::RequestAsyncLoadDataAsset(const TSoftObjectPtr<UPrimaryDataAsset>& SoftAssetRef)
{
	// Active 态禁止直接热替换（AC-5）。
	// 禁令由 return false + 不改状态兑现，不用 ensure：
	// 此路径是 AC-5 明确规定的「被测预期拒绝路径」，有 return false 契约，
	// ensure（语义=「不该发生的 bug」）会污染 Automation 错误日志，迫使测试做 AddExpectedError 体操。
	if (LoadState == EHostLoadState::Active)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DataAssetHostSubsystem] Active 对局中禁止直接热替换 DA（AC-5）；"
			     "须先 RequestReset() 回 Empty 态。忽略本次请求。"));
		return false;
	}

	// Loading 态：重复发起视为编程错误，拒绝
	if (LoadState == EHostLoadState::Loading)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DataAssetHostSubsystem] RequestAsyncLoadDataAsset 在 Loading 态被重复调用，忽略。"));
		return false;
	}

	// 软引用为空视为编程错误
	if (SoftAssetRef.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DataAssetHostSubsystem] RequestAsyncLoadDataAsset 传入空软引用，忽略。"));
		return false;
	}

	// 清理旧 handle（Loaded/Validated 重走加载路径时，旧 handle 须先释放）
	CancelAndReleaseHandle();
	LoadedDataAsset = nullptr;

	// 状态推进至 Loading（ADR-0001 §3 §5 — 发起即置 Loading）
	LoadState = EHostLoadState::Loading;

	// -------------------------------------------------------------------------
	// 核心：FStreamableManager::RequestAsyncLoad，handle 存为宿主成员（AC-1）
	//
	// 使用 UAssetManager::Get().GetStreamableManager() 取 FStreamableManager 实例
	// （UAssetManager::Get() 为进程单例，GetStreamableManager() 为其成员引用）
	//
	// 完成回调使用 TWeakObjectPtr 捕获 this，防宿主已 GC 时 Lambda 写悬挂指针；
	// bIsDeinitializing 为第二道防线（AC-3/AC-4）。
	//
	// 注意（headless / transient 对象）：
	//   FStreamableManager 对已在内存的对象会立即标记 handle 为完成，
	//   但 completion delegate 进入引擎的 deferred callback queue（帧延迟），
	//   FlushAsyncLoading() 只 pump 磁盘 IO，不 pump 该 deferred queue。
	//   因此 RequestAsyncLoad 返回后须检查 HasLoadCompleted()，
	//   若已完成则同步调用 ResolveAndStoreAsset（AC-2 核心），
	//   避免在 headless / 单帧测试环境中 Loaded 状态永不到达。
	// -------------------------------------------------------------------------
	FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();

	// 用 TWeakObjectPtr 安全捕获 this（UObject 生命周期不确定）
	TWeakObjectPtr<UDataAssetHostSubsystem> WeakThis(this);

	// 记录路径（Lambda 捕获 by value，避免生命周期问题）
	FSoftObjectPath AssetPath = SoftAssetRef.ToSoftObjectPath();

	// RequestAsyncLoad 返回的 handle 存为成员（AC-1 核心，非局部变量丢弃）
	// Lambda 捕获：WeakThis（宿主生命周期守卫）+ AssetPath（解析已加载对象用）
	AsyncLoadHandle = StreamableManager.RequestAsyncLoad(
		AssetPath,
		FStreamableDelegate::CreateLambda([WeakThis, AssetPath]()
		{
			// 回调触发时，宿主可能已被 GC 或 Deinitialize
			UDataAssetHostSubsystem* Host = WeakThis.Get();
			if (!Host)
			{
				// 宿主已 GC，直接丢弃（AC-3/AC-4 guard 第一道防线）
				return;
			}

			// bIsDeinitializing guard：第二道防线（Deinitialize 已调，不写字段）
			if (Host->bIsDeinitializing)
			{
				return;
			}

			// handle 可能在回调触发前已被 Cancel（CancelHandle → bCanceled == true）
			// 若 handle 已 Cancel，不写宿主字段（AC-3 — CancelHandle 后无回调写字段）
			if (!Host->AsyncLoadHandle.IsValid() || Host->AsyncLoadHandle->WasCanceled())
			{
				return;
			}

			// 解析并写入宿主字段（AC-2 核心，抽为私有方法供同步/异步两条路径复用）
			Host->ResolveAndStoreAsset(AssetPath);
		}),
		FStreamableManager::AsyncLoadHighPriority,
		/*bManageActiveHandle=*/false,
		/*bStartStalled=*/false,
		TEXT("DataAssetHostSubsystem_AsyncLoad")
	);

	// RequestAsyncLoad 应始终返回有效 handle（即使资产已在内存中亦返回已完成 handle）
	if (!AsyncLoadHandle.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[DataAssetHostSubsystem] RequestAsyncLoad 返回无效 handle，路径: %s"),
			*AssetPath.ToString());
		LoadState = EHostLoadState::Empty;
		return false;
	}

	// -------------------------------------------------------------------------
	// 同步完成检测（transient / 内存对象的 deferred delegate 补偿路径）
	//
	// 若 handle 已同步完成（HasLoadCompleted() == true），deferred queue 里的 delegate
	// 可能不会在本帧触发（headless 无 tick，FlushAsyncLoading 不 pump deferred queue）。
	// 此处主动检测并同步调用 ResolveAndStoreAsset，确保 Loaded 状态立即到达。
	// 这不违反 AC-1（handle 已存为成员）也不违反 AC-3（guard 检查保持一致）。
	// -------------------------------------------------------------------------
	if (AsyncLoadHandle->HasLoadCompleted() && !bIsDeinitializing)
	{
		ResolveAndStoreAsset(AssetPath);
	}

	return true;
}

// =============================================================================
// MarkAsValidated — 推进至 Validated 态（供派生类在校验通过后调用）
// =============================================================================
bool UDataAssetHostSubsystem::MarkAsValidated()
{
	if (LoadState != EHostLoadState::Loaded)
	{
		return false;
	}

	LoadState = EHostLoadState::Validated;
	return true;
}

// =============================================================================
// Deinitialize — 取消 handle，阻止悬挂回调（AC-3/AC-4 核心）
// =============================================================================
void UDataAssetHostSubsystem::Deinitialize()
{
	// -------------------------------------------------------------------------
	// 第一步：置 bIsDeinitializing（第二道 guard），阻止任何即将到来的完成回调写字段
	// 注意：须在 CancelHandle 之前置位，防止 CancelHandle 触发 cancel delegate 时
	// 另一个回调线程恰好通过了 WeakThis.Get() 但还没检查 bIsDeinitializing
	// -------------------------------------------------------------------------
	bIsDeinitializing = true;

	// 第二步：Cancel + Release handle（ADR-0001 §3 — 显式 CancelHandle，AC-3 核心）
	CancelAndReleaseHandle();

	// 第三步：清空 DA 强引用（防止 Subsystem 被 GC 时持有悬挂 DA 引用）
	LoadedDataAsset = nullptr;
	LoadState       = EHostLoadState::Empty;

	// 第四步：调基类（UWorldSubsystem 内部清理，须最后调）
	Super::Deinitialize();
}

// =============================================================================
// CancelAndReleaseHandle — 内部辅助，取消并释放 handle（ADR-0001 §3）
// =============================================================================
void UDataAssetHostSubsystem::CancelAndReleaseHandle()
{
	if (AsyncLoadHandle.IsValid())
	{
		// CancelHandle：立即取消加载请求，阻止完成回调触发（AC-3 首道防线）
		// ReleaseHandle：释放引用计数，允许引擎回收 handle 资源
		// 根据 StreamableManager.h 注释：CancelHandle 会阻止 completion delegate 被调，
		// 即使已在 delayed callback queue 中排队
		AsyncLoadHandle->CancelHandle();

		// F2：在 Reset() 之前读取 WasCanceled()（Reset 后指针失效）
		// 记录 bLastHandleWasCanceled 供测试 seam（DidLastHandleCancel()）读取，
		// 以断言 Deinit 路径真正调用了 CancelHandle 而非静默跳过
		bLastHandleWasCanceled = AsyncLoadHandle->WasCanceled();

		AsyncLoadHandle->ReleaseHandle();

		// 释放 TSharedPtr 引用，handle 引用计数清零后由引擎销毁
		AsyncLoadHandle.Reset();
	}
}

// =============================================================================
// ResolveAndStoreAsset — 从路径解析对象并写入宿主字段（AC-2 核心）
//
// 同时被两条路径调用：
//   1. 异步完成 Lambda（调用前已通过 guard 检查）
//   2. RequestAsyncLoad 返回后的同步完成检测（handle->HasLoadCompleted()）
// 调用方须保证：!bIsDeinitializing && handle 有效且未 Cancel。
// =============================================================================
void UDataAssetHostSubsystem::ResolveAndStoreAsset(const FSoftObjectPath& AssetPath)
{
	// 若已是 Loaded 或更高态（可能同步路径和异步路径都触发），跳过重复写入
	if (LoadState != EHostLoadState::Loading)
	{
		return;
	}

	// -------------------------------------------------------------------------
	// F1：优先从 handle->GetLoadedAsset() 取已加载对象（已走 manager 内部 redirector 解析）；
	// 仅当 handle 已 Reset（nullptr）或不含已加载对象时，降级用 AssetPath.ResolveObject() 回退。
	//
	// 原因（redirector gap）：
	//   AssetPath.ResolveObject() 在 GUObjectHashTables 按路径精确匹配，
	//   若资产被 redirector 重定向，实际内存对象的路径与软引用路径不同，
	//   导致 ResolveObject() 返回 nullptr 而 handle 内含有效对象。
	//   FStreamableHandle::GetLoadedAsset() 使用 manager 内部 resolved map，
	//   已将 redirector 转换过的最终对象路径正确映射，与 redirector 安全。
	// -------------------------------------------------------------------------
	UObject* RawAsset = nullptr;
	if (AsyncLoadHandle.IsValid())
	{
		// 主路径：从 handle 取（redirector-safe）
		RawAsset = AsyncLoadHandle->GetLoadedAsset();
	}
	if (!RawAsset)
	{
		// 降级回退路径：handle 已 Reset 或未取到（内存对象同步场景的极少数情况）
		RawAsset = AssetPath.ResolveObject();
	}
	UPrimaryDataAsset* LoadedAsset = Cast<UPrimaryDataAsset>(RawAsset);

	if (!LoadedAsset)
	{
		// 解析失败：路径无效 / 类型不匹配 / 对象未加载到内存
		// F11：解析失败将 LoadState 回到 Empty，允许调用方重试（防死锁在 Loading 态）
		UE_LOG(LogTemp, Error,
			TEXT("[DataAssetHostSubsystem] ResolveAndStoreAsset：未取到 UPrimaryDataAsset，路径: %s"),
			*AssetPath.ToString());
		LoadState = EHostLoadState::Empty;
		return;
	}

	// 写入 UPROPERTY() TObjectPtr 强引用（防 GC，AC-2）
	LoadedDataAsset = LoadedAsset;
	LoadState       = EHostLoadState::Loaded;

	// 通知派生类（board epic 的校验逻辑在此处插入）
	OnDataAssetLoaded(LoadedAsset);
}
