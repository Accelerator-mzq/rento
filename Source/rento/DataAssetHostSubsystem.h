// DataAssetHostSubsystem.h
// =============================================================================
// DA 持有者宿主中间层（story-003，ADR-0001 §3 异步加载纪律 + ADR-0002 防 GC 强引用）
//
// 继承关系：
//   UMatchSubsystemBase（story-001，纯骨架）
//     └─ UDataAssetHostSubsystem（本文件，Abstract，泛型 UPrimaryDataAsset 层）
//          └─ （未来）UBoardDataAssetHost（board epic，窄化为 UBoardDataAsset）
//
// 规范依据：
//   - ADR-0001 §Implementation Guidelines #3 — 异步加载纪律
//   - ADR-0002 §Decision — 运行时由宿主以 UPROPERTY 强引用持有 DA 防 GC
//   - control-manifest line 34/97/100/364/374
//   - story-003 AC-1~5
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "Engine/StreamableManager.h"
#include "Engine/DataAsset.h"   // 提供 UPrimaryDataAsset（UDataAsset 子类）
#include "DataAssetHostSubsystem.generated.h"

// =============================================================================
// 宿主加载状态机枚举（ADR-0002 §热切换边界）
// 换盘须走 Empty→Loading→Loaded→Validated→Active 完整路径，
// 禁止在 Active 对局中直接热替换 DA（AC-5）
// =============================================================================
UENUM(BlueprintType)
enum class EHostLoadState : uint8
{
	/** 初始态 / Deinitialize 后重置态：无 DA 持有，handle 已 Release */
	Empty      UMETA(DisplayName = "Empty"),

	/** FStreamableManager::RequestAsyncLoad 发起后，尚未收到完成回调 */
	Loading    UMETA(DisplayName = "Loading"),

	/** 完成回调已写入 LoadedDataAsset，DA 在内存中（防 GC 强引用已激活） */
	Loaded     UMETA(DisplayName = "Loaded"),

	/** DA 校验通过（校验逻辑归 board epic，本层不实现）——占位，供派生类推进 */
	Validated  UMETA(DisplayName = "Validated"),

	/** 对局激活态：DA 正在被对局系统使用；Active 态禁止直接热替换 DA */
	Active     UMETA(DisplayName = "Active"),
};

/**
 * UDataAssetHostSubsystem — DA 持有者宿主中间层（Abstract）
 *
 * 职责：
 *   1. 发起 UPrimaryDataAsset 的异步加载（FStreamableManager::RequestAsyncLoad），
 *      将 TSharedPtr<FStreamableHandle> 存为成员（非局部变量丢弃）—— AC-1。
 *   2. 加载完成回调中将解析出的 DA 写入 UPROPERTY() TObjectPtr<> 强引用，
 *      防止 GC 在对局期间释放 DA —— AC-2。
 *   3. Deinitialize() 显式调用 CancelHandle() + ReleaseHandle()，
 *      随后完成回调 guard 检查 bIsDeinitializing，不再写宿主已 GC 字段 —— AC-3/AC-4。
 *   4. 禁热替换：Active 态无 public DA setter；
 *      重初始化入口在 Active 态拒绝直接替换，须经 Empty→Loading→Loaded→Validated→Active —— AC-5。
 *
 * 约束（Out of Scope / 归 board epic）：
 *   - DA 校验逻辑与结构化错误码（BoardTooSmall/Index0NotGo/…）
 *   - GetTileData / GetTilesInGroup 接口
 *   - 窄化为 UBoardDataAsset 的派生类
 *
 * 使用示例（派生类）：
 * @code
 *   UCLASS()
 *   class UBoardDataAssetHost : public UDataAssetHostSubsystem
 *   {
 *       GENERATED_BODY()
 *   protected:
 *       virtual void Initialize(FSubsystemCollectionBase& Collection) override
 *       {
 *           Super::Initialize(Collection);
 *           TSoftObjectPtr<UPrimaryDataAsset> BoardRef = ...;
 *           RequestAsyncLoadDataAsset(BoardRef);
 *       }
 *   };
 * @endcode
 */
UCLASS(Abstract)
class RENTO_API UDataAssetHostSubsystem : public UMatchSubsystemBase
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 公开只读状态查询（无 public setter，无热替换入口）
	// =========================================================================

	/**
	 * 查询当前宿主加载状态。
	 * 只读，无 public setter；状态变更由内部逻辑驱动（RequestAsyncLoadDataAsset / 完成回调 / Deinitialize）。
	 *
	 * @return 当前 EHostLoadState 枚举值
	 */
	UFUNCTION(BlueprintPure, Category = "DataAssetHost")
	EHostLoadState GetLoadState() const { return LoadState; }

	/**
	 * 查询当前持有的 DA（可能为 nullptr，须先检查 LoadState >= Loaded）。
	 * 只读，无 public setter（AC-5 禁热替换）。
	 *
	 * @return 当前强引用持有的 UPrimaryDataAsset，若尚未加载完成则为 nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "DataAssetHost")
	UPrimaryDataAsset* GetLoadedDataAsset() const { return LoadedDataAsset; }

	/**
	 * 尝试将宿主推进至 Active 态（须处于 Validated 态才允许）。
	 * Active 态后，禁止直接热替换 DA —— 须先 RequestReset() 走完整重初始化路径。
	 *
	 * @return true 表示成功推进至 Active；false 表示当前态不满足前提（已 Active 或未 Validated）
	 */
	UFUNCTION(BlueprintCallable, Category = "DataAssetHost")
	bool ActivateDataAsset();

	/**
	 * 重初始化入口：在 Active 态请求重置（回到 Empty 态），以便重新走完整加载流程。
	 * Active 态禁止直接替换 DA，必须经此入口 → Empty → 再次 RequestAsyncLoadDataAsset（AC-5）。
	 *
	 * @return true 表示已成功重置回 Empty；false 表示当前并非 Active 态（无需重置）
	 */
	UFUNCTION(BlueprintCallable, Category = "DataAssetHost")
	bool RequestReset();

	// =========================================================================
	// UWorldSubsystem 生命周期 override
	// =========================================================================

	/** 宿主销毁：CancelHandle + ReleaseHandle，置 bIsDeinitializing，阻止悬挂回调写字段（AC-3） */
	virtual void Deinitialize() override;

protected:
	// =========================================================================
	// 供派生类调用的异步加载入口（protected，无 public 热替换入口）
	// =========================================================================

	/**
	 * 发起 DA 异步加载（ADR-0001 §3 异步加载纪律）。
	 *
	 * 将 FStreamableManager::RequestAsyncLoad 返回的 TSharedPtr<FStreamableHandle>
	 * 存为宿主成员 AsyncLoadHandle（非局部变量丢弃）——AC-1。
	 *
	 * 约束：
	 *   - 只能在 Empty 态调用；Active 态调用将被拒绝（UE_LOG Warning + return false），禁热替换（AC-5）
	 *   - SoftAssetRef 应为有效软引用（派生类负责从 GameSetupConfig 取得）
	 *
	 * @param SoftAssetRef 要加载的 UPrimaryDataAsset 软引用
	 * @return true 表示已成功发起加载；false 表示当前态不允许（Active 态或已在 Loading 中）
	 */
	bool RequestAsyncLoadDataAsset(const TSoftObjectPtr<UPrimaryDataAsset>& SoftAssetRef);

	/**
	 * 将 LoadedDataAsset 推进至 Validated 态（DA 校验通过后由派生类调用）。
	 * 本层不实现校验逻辑，仅提供状态推进接口（校验归 board epic）。
	 *
	 * @return true 表示成功推进至 Validated；false 表示当前态非 Loaded（前提不满足）
	 */
	bool MarkAsValidated();

	/**
	 * 异步加载完成时触发。
	 * 派生类可 override 此函数（须先调 Super），以在 DA 就绪后执行额外初始化。
	 * 仅当 !bIsDeinitializing 时才被调用（guard 已在基类内部过滤）。
	 *
	 * @param LoadedAsset 已加载完毕的 UPrimaryDataAsset 指针（非 nullptr）
	 */
	virtual void OnDataAssetLoaded(UPrimaryDataAsset* LoadedAsset) {}

	/**
	 * 查询最近一次 CancelAndReleaseHandle 调用时 handle 是否确实被 Cancel（F2 测试 seam）。
	 *
	 * 用途：测试桩在 Deinitialize 后（Super 调完 CancelAndReleaseHandle 之后）读取，
	 * 以断言 Deinit 路径真正调用了 CancelHandle 而非静默跳过。
	 * 生产代码不使用此 accessor。
	 *
	 * @return true 表示上次 CancelAndReleaseHandle 时 handle 非空且 WasCanceled()==true
	 */
	bool DidLastHandleCancel() const { return bLastHandleWasCanceled; }

	/**
	 * 测试 seam（F11）：强制进入 Loading 态并以给定路径驱动 ResolveAndStoreAsset，
	 * 用于确定性验证「解析失败 → LoadState 回退 Empty」分支（ResolveAndStoreAsset 内部）。
	 *
	 * 为何需要：该失败分支在生产环境由真异步加载完成回调触发（GetLoadedAsset/ResolveObject
	 * 均取不到对象时回退 Empty 防死锁）；但 headless `-nullrhi` 无 tick、deferred delegate
	 * 不被 FlushAsyncLoading pump，且磁盘不存在路径的 handle 不同步完成，无法可靠驱动该分支。
	 * 本 seam 直接以「Loading 态 + 不可解析路径」驱动分支，确定性、无磁盘 IO。
	 * 生产代码不使用此方法（仅测试桩调用）。
	 *
	 * @param AssetPath 要解析的路径（测试传入不可解析路径以触发失败分支）
	 */
	void DriveResolveForTesting(const FSoftObjectPath& AssetPath)
	{
		LoadState = EHostLoadState::Loading;
		ResolveAndStoreAsset(AssetPath);
	}

private:
	// =========================================================================
	// 内部状态（私有，全部通过受控入口修改，无公开 setter）
	// =========================================================================

	/**
	 * 异步加载 handle（非 UPROPERTY，FStreamableHandle 是非-UObject，ADR-0001 明示例外）。
	 * handle 生命周期：RequestAsyncLoad 后非空；Deinitialize 中 CancelHandle + ReleaseHandle 后置空。
	 * 使用 TSharedPtr 而非裸指针（control-manifest line 364/374 — TSharedPtr for 非-UObject）。
	 */
	TSharedPtr<FStreamableHandle> AsyncLoadHandle;

	/**
	 * 运行时 DA 强引用（UPROPERTY + TObjectPtr 防 GC，control-manifest line 374）。
	 * 加载完成回调写入；Deinitialize 重置为 nullptr。
	 * 无 public setter（AC-5 禁热替换）。
	 */
	UPROPERTY()
	TObjectPtr<UPrimaryDataAsset> LoadedDataAsset = nullptr;

	/** 当前加载状态机状态（初始为 Empty） */
	EHostLoadState LoadState = EHostLoadState::Empty;

	/**
	 * Deinitialize 中设为 true，阻止悬挂回调写已 GC 宿主字段（AC-3/AC-4 guard）。
	 * 一旦置 true，完成回调将跳过对 LoadedDataAsset 的写入。
	 */
	bool bIsDeinitializing = false;

	/**
	 * CancelAndReleaseHandle 执行后，记录 handle->WasCanceled() 的值（F2 测试 seam）。
	 * 读取窗口：handle Reset() 之前、WasCanceled() 之后——此时 bCanceled 已由 CancelHandle 置位。
	 * 测试桩在桩的 Deinitialize override 里（Super::Deinitialize 之后）读取 DidLastHandleCancel()。
	 */
	bool bLastHandleWasCanceled = false;

	// =========================================================================
	// 内部辅助
	// =========================================================================

	/**
	 * 内部：取消并释放当前 handle（ADR-0001 §3）。
	 * 在 Deinitialize 中调用，也用于 RequestReset 路径。
	 * 执行后将 handle->WasCanceled() 写入 bLastHandleWasCanceled（测试 seam，F2）。
	 */
	void CancelAndReleaseHandle();

	/**
	 * 内部：解析已加载对象并写入宿主字段（AC-2 核心）。
	 *
	 * 同时被两条路径调用：
	 *   1. 异步完成 Lambda 回调（真异步路径，调用前已通过 guard 检查）
	 *   2. RequestAsyncLoad 返回后若 handle->HasLoadCompleted() 立即同步调用
	 *      （transient / 内存中已存在对象的 deferred delegate 不经 FlushAsyncLoading pump，
	 *       headless 单帧测试环境下须同步路径补偿）
	 *
	 * 优先从 handle->GetLoadedAsset() 取对象（已走 manager 内部 redirector 解析），
	 * handle 已 Reset 时降级用 AssetPath.ResolveObject() 回退（F1）。
	 *
	 * 若已是 Loaded 或更高态，函数幂等返回（防双路径重复写入）。
	 * 解析失败时将 LoadState 回到 Empty，允许调用方重试（F11）。
	 *
	 * @param AssetPath 要解析的软引用路径（handle 已 Reset 时降级用）
	 */
	void ResolveAndStoreAsset(const FSoftObjectPath& AssetPath);
};
