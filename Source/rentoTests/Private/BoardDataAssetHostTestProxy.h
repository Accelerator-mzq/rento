// BoardDataAssetHostTestProxy.h
// =============================================================================
// story-002 测试专用桩：UBoardDataAssetHostTestProxy
//
// 职责：
//   暴露 UBoardDataAssetHost 的 protected 加载入口（RequestAsyncLoadDataAsset），
//   允许测试直接注入任意 UPrimaryDataAsset 软引用（包括非 UBoardDataAsset 类型），
//   以触发 OnDataAssetLoaded 的 Cast 失败守卫验证（TC-L2d）。
//
// 模式对齐：
//   与 ff-003 的 UTestDataAssetHost（TestDataAssetHost.h）同模式——
//   仅暴露接口，不添加业务逻辑。
//   UCLASS 必须定义在 .h 文件中（UHT 约束）。
//
// 物理路径：Source/rentoTests/Private/BoardDataAssetHostTestProxy.h
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "BoardDataAssetHost.h"
#include "BoardDataAssetHostTestProxy.generated.h"

/**
 * UBoardDataAssetHostTestProxy — story-002 测试桩（具体子类，打开 protected 加载入口）
 *
 * 仅供 TC-L2d（Cast 失败守卫）测试使用。
 * 生产代码中不应引用此类。
 *
 * 使用示例：
 * @code
 *   UBoardDataAssetHostTestProxy* Proxy = NewObject<UBoardDataAssetHostTestProxy>(
 *       GetTransientPackage(), RF_Transient);
 *   TSoftObjectPtr<UPrimaryDataAsset> NonBoardRef = MakeTransientNonBoardDARef("TC_L2d");
 *   Proxy->CallRequestAsyncLoadAny(NonBoardRef);
 *   FlushAsyncLoading();
 *   // 断言 Proxy->GetLoadState() == EHostLoadState::Loaded（Cast 失败，未推进）
 * @endcode
 */
UCLASS()
class UBoardDataAssetHostTestProxy : public UBoardDataAssetHost
{
	GENERATED_BODY()

public:
	/**
	 * 暴露基类 protected 加载入口，允许测试注入任意 UPrimaryDataAsset 软引用。
	 *
	 * 生产代码通过 BoardToLoad seam（TSoftObjectPtr<UBoardDataAsset>）经 OnWorldBeginPlay 发起加载，
	 * 该路径无法注入非 UBoardDataAsset 类型。
	 * 此方法绕过类型限制，仅用于 TC-L2d 的 Cast 失败守卫验证场景。
	 *
	 * @param SoftAssetRef 任意 UPrimaryDataAsset 软引用（包括非 UBoardDataAsset 类型）
	 * @return 同 RequestAsyncLoadDataAsset 返回值（true 表示成功发起加载）
	 */
	bool CallRequestAsyncLoadAny(const TSoftObjectPtr<UPrimaryDataAsset>& SoftAssetRef)
	{
		// 直接调用基类 protected 方法，暴露给测试
		return RequestAsyncLoadDataAsset(SoftAssetRef);
	}
};
