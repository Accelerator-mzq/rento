// BoardDataAssetHost.cpp
// =============================================================================
// UBoardDataAsset 专用持有者宿主实现（story-002）
// 规范依据：ADR-0001 §3 异步加载纪律、ADR-0002 防 GC + 热切换边界
// =============================================================================
#include "BoardDataAssetHost.h"
#include "Engine/World.h"

// =============================================================================
// GetLoadedBoard — 类型化访问器（AC-L1 核心：类型化 + 防 GC 由基类满足）
// =============================================================================
UBoardDataAsset* UBoardDataAssetHost::GetLoadedBoard() const
{
	// Cast<>：将基类持有的 UPrimaryDataAsset 强引用转为 UBoardDataAsset*。
	// 若加载未完成（nullptr）或类型不匹配（理论不应发生，seam 传入的就是 UBoardDataAsset 软引用），
	// Cast 返回 nullptr——调用方须检查 GetLoadState() >= Loaded 再调此函数。
	return Cast<UBoardDataAsset>(GetLoadedDataAsset());
}

// =============================================================================
// OnWorldBeginPlay — 发起棋盘 DA 异步加载（AC-L2 加载触发时机）
//
// ADR-0001 §Implementation Notes #1：
//   OnWorldBeginPlay 是正确的加载发起时机（Initialize 时玩家配置尚未落地）。
// =============================================================================
void UBoardDataAssetHost::OnWorldBeginPlay(UWorld& InWorld)
{
	// 须先调 Super（基类 UMatchSubsystemBase → UWorldSubsystem 可能有扩展点）
	Super::OnWorldBeginPlay(InWorld);

	if (BoardToLoad.IsNull())
	{
		// BoardToLoad 为空：等待 ff-004/pt-001（StartNewGame / GameSetupConfig）接线后注入。
		// 开发期 / 测试期须手动填入 BoardToLoad，或通过外部调用 RequestAsyncLoadDataAsset。
		UE_LOG(LogTemp, Warning,
			TEXT("[UBoardDataAssetHost] OnWorldBeginPlay：BoardToLoad 为空，"
			     "棋盘 DA 未加载。请由 ff-004/pt-001 注入 BoardToLoad 后重试。"));
		return;
	}

	// 将 TSoftObjectPtr<UBoardDataAsset> 转为基类 RequestAsyncLoadDataAsset 所需的
	// TSoftObjectPtr<UPrimaryDataAsset>（通过 ToSoftObjectPath() 构造，保持路径一致）
	// UBoardDataAsset IS-A UPrimaryDataAsset，转换安全。
	const TSoftObjectPtr<UPrimaryDataAsset> AsBase(BoardToLoad.ToSoftObjectPath());

	// 发起异步加载（基类：存 handle 成员 + 设 Loading 态 + 完成后调 OnDataAssetLoaded）
	// CancelHandle 纪律（ADR-0001 §3）由基类 Deinitialize 统一处理，本类无需重复实现。
	const bool bStarted = RequestAsyncLoadDataAsset(AsBase);

	if (!bStarted)
	{
		// RequestAsyncLoadDataAsset 失败的可能原因：
		//   - 当前已在 Loading 态（重复调用）→ 降级 Warning，非致命
		//   - 其他原因（基类已打 Error log）→ Error 升级告警
		// 先检查是否已在 Loading（重复发起是 Warning 级），否则 Error。
		if (GetLoadState() == EHostLoadState::Loading)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[UBoardDataAssetHost] OnWorldBeginPlay：已在 Loading 态，"
				     "忽略重复加载请求（棋盘路径: %s）"),
				*BoardToLoad.ToSoftObjectPath().ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[UBoardDataAssetHost] OnWorldBeginPlay：RequestAsyncLoadDataAsset 失败，"
				     "棋盘路径: %s"),
				*BoardToLoad.ToSoftObjectPath().ToString());
		}
	}
}

// =============================================================================
// OnDataAssetLoaded — 状态机驱动（AC-L2 校验挂载点 + 状态推进）
//
// 基类在 DA 就绪（Loaded 态）后调用此钩子。
// 本函数负责：校验挂载点（story-005 占位）→ MarkAsValidated → ActivateDataAsset。
// =============================================================================
void UBoardDataAssetHost::OnDataAssetLoaded(UPrimaryDataAsset* LoadedAsset)
{
	// 须先调 Super（基类实现为空，但遵守 Super 调用纪律，防未来基类扩展）
	Super::OnDataAssetLoaded(LoadedAsset);

	// -------------------------------------------------------------------------
	// TODO(story-005)：在此处插入 UBoardDataAsset 加载期校验逻辑
	//
	// story-005 将在此位置实现完整校验（GDD §Edge Cases 全部拒绝/警告规则）：
	//   - Index 0 须为 Go 格（Index0NotGo）
	//   - 格子数不少于最小值（BoardTooSmall）
	//   - TileIndex 与数组下标一致（IndexMismatch）
	//   - 其他结构化错误码（story-005/006 定义）
	//
	// 校验失败时的处理（story-005 设计决策点）：
	//   本 story 校验挂载点 MVP 占位：总通过（直接 MarkAsValidated）。
	//   story-005 实现后，校验失败路径须决定：
	//     方案 A — 新增 EHostLoadState::LoadFailed 枚举值（须修改基类 DataAssetHostSubsystem.h），
	//               失败时不调 MarkAsValidated / ActivateDataAsset，而是推进到 LoadFailed 态。
	//     方案 B — 沿用 Empty 态作为失败态（基类当前解析失败路径回退 Empty），
	//               同时由 story-005 定义结构化错误码，通过事件/返回值通知调用方。
	//   AC-L2 提到「校验失败进 LoadFailed」是最终态语义，具体枚举值由 story-005 决定。
	//   当前基类 EHostLoadState 只有 Empty/Loading/Loaded/Validated/Active，无 LoadFailed。
	//
	// ⚠ 当前 MVP 占位：校验直通，无实际校验逻辑。
	// -------------------------------------------------------------------------

	// 类型安全守卫：Cast 到 UBoardDataAsset（本 story 已实现的边界，非 story-005 校验逻辑）。
	//
	// 此 Cast 失败代表调用方传入了非 UBoardDataAsset 软引用（seam 配置错误或引擎内部异常），
	// 与 story-005 的棋盘内容校验（Index0NotGo/BoardTooSmall 等）性质不同——
	// 内容校验发生在「确认类型正确之后」，类型守卫在「进入内容校验之前」。
	//
	// Cast 失败时的已知限制（待 story-005 处理）：
	//   宿主停在 Loaded 态，无法自动恢复：RequestReset 需要 Active 态，
	//   RequestAsyncLoadDataAsset 在非 Empty 态被拒，调用方须手动干预。
	//   story-005 引入 LoadFailed 态或 Empty 回退 + 错误码时将修正此限制。
	UBoardDataAsset* BoardAsset = Cast<UBoardDataAsset>(LoadedAsset);
	if (!BoardAsset)
	{
		// 类型不匹配：seam（BoardToLoad）传入了非 UBoardDataAsset 类型，不推进状态机。
		// 宿主停在 Loaded 态（已知限制，story-005 将在此提供 LoadFailed 恢复路径）。
		UE_LOG(LogTemp, Error,
			TEXT("[UBoardDataAssetHost] OnDataAssetLoaded：加载的资产不是 UBoardDataAsset，"
			     "无法推进状态机。类型: %s。宿主停在 Loaded 态（story-005 将处理恢复路径）。"),
			LoadedAsset ? *LoadedAsset->GetClass()->GetName() : TEXT("nullptr"));
		return;
	}

	// 校验通过（MVP 占位，story-005 将在此前插入真实校验，失败则不执行以下两行）

	// 推进至 Validated 态（声明「校验已通过」）
	const bool bValidated = MarkAsValidated();
	if (!bValidated)
	{
		// MarkAsValidated 仅在 Loaded 态有效；若已不在 Loaded 态（竞态 / 重复调用），静默跳过
		UE_LOG(LogTemp, Warning,
			TEXT("[UBoardDataAssetHost] OnDataAssetLoaded：MarkAsValidated 失败（当前态 %d），"
			     "状态机未推进至 Validated。"),
			static_cast<int32>(GetLoadState()));
		return;
	}

	// 推进至 Active 态（棋盘 DA 进入对局可用状态）
	const bool bActivated = ActivateDataAsset();
	if (!bActivated)
	{
		// ActivateDataAsset 仅在 Validated 态有效；理论上不应失败（上一步刚到 Validated）
		UE_LOG(LogTemp, Warning,
			TEXT("[UBoardDataAssetHost] OnDataAssetLoaded：ActivateDataAsset 失败（当前态 %d），"
			     "状态机未推进至 Active。"),
			static_cast<int32>(GetLoadState()));
	}
}
