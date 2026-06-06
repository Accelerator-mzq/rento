// BoardDataAssetHost.h
// =============================================================================
// UBoardDataAsset 专用持有者宿主（story-002，ADR-0001/ADR-0002）
//
// 继承关系：
//   UMatchSubsystemBase（story-001，纯骨架）
//     └─ UDataAssetHostSubsystem（story-003，泛型异步加载 + CancelHandle 纪律）
//          └─ UBoardDataAssetHost（本文件，窄化为 UBoardDataAsset）
//
// 职责（最大复用基类，不重造）：
//   1. 提供类型化访问器 GetLoadedBoard()（AC-L1）
//      强引用防 GC 由基类 UPROPERTY LoadedDataAsset 满足（UBoardDataAsset IS-A UPrimaryDataAsset），
//      本类不新增第二个强引用成员，避免双重引用 + 同步负担。
//   2. 加载源 seam BoardToLoad（AC-L2 / fork#3）
//      OnWorldBeginPlay 判断非空则发起异步加载；生产接线 defer ff-004/pt-001。
//   3. 状态机驱动（AC-L2）：
//      override OnDataAssetLoaded → 校验挂载点（story-005 占位，MVP 直通）
//      → MarkAsValidated → ActivateDataAsset。
//   4. 热切换边界（AC-L4）：
//      完全继承基类约束——Active 态无 public DA setter，无 SetLoadedBoard 类接口，
//      换盘须经 RequestReset → Empty → 重新 RequestAsyncLoadDataAsset。
//
// Out of Scope（严守边界）：
//   - UBoardDataAsset / FBoardTileData 类型定义 → story-001（已实现）
//   - 加载期校验规则 / 结构化错误码（BoardTooSmall / Index0NotGo / …）→ story-005/006
//   - GetTileData / GetTilesInGroup / GetBoardId 查询接口 → story-004
//   - RNG Seed 注入 → 骰子 epic（dr-001 已另做）
//   - 基类 CancelHandle / 状态机 / 异步加载逻辑 → 完全继承，不重造
//
// 规范依据：
//   - ADR-0001 §Implementation Guidelines — 异步加载纪律、ShouldCreateSubsystem
//   - ADR-0002 §Decision/§Implementation Guidelines — 强引用防 GC、热切换边界
//   - control-manifest Foundation 层 §宿主与生命周期 / §数据容器
//   - story-002 AC-L1~L5
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "DataAssetHostSubsystem.h"
#include "BoardDataAsset.h"
#include "BoardDataAssetHost.generated.h"

/**
 * UBoardDataAssetHost — UBoardDataAsset 专用持有者宿主
 *
 * per-match 服务，挂 UWorldSubsystem（一局 = World 边界，PIE Stop 即销毁 / Start 即重建）。
 * 所有异步加载纪律（handle 存为成员 / Deinitialize CancelHandle / bIsDeinitializing guard）
 * 均由基类 UDataAssetHostSubsystem 提供，本类只负责：
 *   - 类型化访问 GetLoadedBoard()
 *   - BoardToLoad seam + OnWorldBeginPlay 发起加载
 *   - OnDataAssetLoaded 校验挂载点（story-005 占位）→ MarkAsValidated → ActivateDataAsset
 *
 * 热切换边界（AC-L4）：
 *   完全继承基类约束，本类无任何 public 接口在 Active 态直接替换持有的 DA。
 *   换盘唯一路径：RequestReset() → Empty → BoardToLoad = 新 DA → RequestAsyncLoadDataAsset。
 */
UCLASS()
class RENTO_API UBoardDataAssetHost : public UDataAssetHostSubsystem
{
	GENERATED_BODY()

public:
	// =========================================================================
	// 类型化访问器（AC-L1：强引用防 GC 由基类 LoadedDataAsset 满足，此处只做类型化）
	// =========================================================================

	/**
	 * 获取当前持有的 UBoardDataAsset（类型化访问器）。
	 *
	 * 实现：Cast<UBoardDataAsset>(GetLoadedDataAsset())
	 * 强引用防 GC 由基类 UPROPERTY() TObjectPtr<UPrimaryDataAsset> LoadedDataAsset 满足
	 * （UBoardDataAsset IS-A UPrimaryDataAsset，同一 GC 强引用链，无需第二个强引用成员）。
	 *
	 * 调用前提：GetLoadState() >= EHostLoadState::Loaded，否则返回 nullptr。
	 *
	 * @return 当前强引用持有的 UBoardDataAsset，若尚未加载完成则为 nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "BoardDataAssetHost")
	UBoardDataAsset* GetLoadedBoard() const;

	// =========================================================================
	// 加载源 seam（AC-L2 / fork#3）
	// 生产接线 defer ff-004/pt-001（StartNewGame / GameSetupConfig）
	// =========================================================================

	/**
	 * 要加载的棋盘 DA 软引用（加载源 seam）。
	 *
	 * 默认空；OnWorldBeginPlay 判断非空则发起异步加载。
	 * 生产场景由 ff-004/pt-001（StartNewGame / GameSetupConfig）在 OnWorldBeginPlay 前注入。
	 * 开发期可在 CDO Details 面板直接填入测试棋盘（如 DA_Board_Classic40）以验证加载流程。
	 *
	 * 设计与 dr-001 的 bUseFixedSeed seam 同思路：
	 *   真实加载源 = 外部注入；seam 字段 = 开发期 / 测试期旁路。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BoardDataAssetHost")
	TSoftObjectPtr<UBoardDataAsset> BoardToLoad;

	// =========================================================================
	// UWorldSubsystem 生命周期 override
	// =========================================================================

	/**
	 * OnWorldBeginPlay — 发起棋盘 DA 异步加载（AC-L2 加载触发时机）。
	 *
	 * 须先调 Super::OnWorldBeginPlay（基类可能有扩展点）。
	 * BoardToLoad 非空时调 RequestAsyncLoadDataAsset 发起异步加载；
	 * 空时仅打 Warning log（等待 ff-004/pt-001 接线后注入）。
	 *
	 * ADR-0001 §Implementation Notes #1：
	 *   OnWorldBeginPlay 是 Seed 注入 / 加载发起的正确时机
	 *   （Initialize 时玩家配置尚未由 StartNewGame 落地）。
	 *
	 * @param InWorld 当前 World 引用（由 UWorldSubsystem 框架传入）
	 */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	// =========================================================================
	// 状态机钩子（AC-L2 校验挂载点）
	// =========================================================================

	/**
	 * 异步加载完成回调（由基类在 DA 就绪后调用）。
	 *
	 * 校验挂载点（story-005 预留，MVP 占位直通）：
	 *   须先调 Super::OnDataAssetLoaded（基类实现为空，但遵守 Super 调用纪律）。
	 *   MVP：直接通过校验 → MarkAsValidated → ActivateDataAsset。
	 *
	 * ⚠ story-005 设计决策点（届时须决定）：
	 *   story-005 将在「// TODO(story-005)」注释处插入真实校验逻辑。
	 *   校验失败时，需决定失败态表示方式：
	 *     方案 A — 新增 EHostLoadState::LoadFailed 枚举值（须修改 DataAssetHostSubsystem.h）
	 *     方案 B — 沿用当前 Empty 态（失败回退 Empty）+ 结构化错误码（由 story-005/006 定义）
	 *   当前基类（ff-003）只有 Empty/Loading/Loaded/Validated/Active，无 LoadFailed 态。
	 *   AC-L2 提到「校验失败进 LoadFailed」是指最终态语义，具体枚举值由 story-005 定义。
	 *   本 story 只留挂载点，失败分支尚未实现。
	 *
	 * @param LoadedAsset 已加载完毕的 UPrimaryDataAsset 指针（基类保证非 nullptr）
	 */
	virtual void OnDataAssetLoaded(UPrimaryDataAsset* LoadedAsset) override;
};
