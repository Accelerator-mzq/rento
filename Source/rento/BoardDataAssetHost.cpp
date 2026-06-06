// BoardDataAssetHost.cpp
// =============================================================================
// UBoardDataAsset 专用持有者宿主实现（story-002 + story-004）
// 规范依据：ADR-0001 §3 异步加载纪律、ADR-0002 防 GC + 热切换边界 + 接口隔离
// story-004：只读查询接口 GetTileCount/GetTileData/GetTilesInGroup/GetBoardId
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

// =============================================================================
// story-004 只读查询接口实现（AC-1/26/27/28/30）
//
// 设计约定：
//   - 所有函数通过 GetLoadedBoard() 读取 DA，不持有第二个强引用。
//   - LoadedBoard==null 时各函数安全返回默认值，不崩溃（统一兜底策略）。
//   - GetTileData 越界使用 UE_LOG(Error) 而非 ensure(false)：
//     循 FF-003/bd-003 AC-34 既定 logged decision——headless Automation 中
//     ensure 产生不稳定 callstack dump 致 AddExpectedError 计数不可靠。
// =============================================================================

// =============================================================================
// GetTileCount — 返回棋盘格子总数（AC-1）
// =============================================================================
int32 UBoardDataAssetHost::GetTileCount() const
{
	// LoadedBoard 为空（未加载完成）→ 返回 0，安全兜底
	const UBoardDataAsset* Board = GetLoadedBoard();
	if (!Board)
	{
		return 0;
	}

	// O(1) TArray::Num()（ADR-0002 R5 性能约束）
	return Board->Tiles.Num();
}

// =============================================================================
// GetTileData — 按索引获取格子数据（O(1)，AC-27 越界保护）
// =============================================================================
FBoardTileData UBoardDataAssetHost::GetTileData(int32 Index) const
{
	const UBoardDataAsset* Board = GetLoadedBoard();
	const int32 Count = Board ? Board->Tiles.Num() : 0;

	// 越界检查（LoadedBoard==null 视为 Count==0，统一走越界分支）
	if (!Board || Index < 0 || Index >= Count)
	{
		// AC-27 logged decision：UE_LOG(Error) 替代 ensure(false)
		// 消息含 "越界" 子串供测试 AddExpectedError(Contains) 匹配
		UE_LOG(LogTemp, Error,
			TEXT("[UBoardDataAssetHost] GetTileData：Index=%d 越界（有效范围 [0, %d)），"
			     "返回默认空 FBoardTileData。调用方须先调 GetTileCount() 约束范围。"),
			Index, Count);

		// 返回默认空 struct（不崩溃，不返回脏数据，AC-27）
		return FBoardTileData{};
	}

	// O(1) 直接数组访问（ADR-0002 R5）
	return Board->Tiles[Index];
}

// =============================================================================
// GetTilesInGroup — 按色组返回成员 TileIndex 列表（升序，AC-26/AC-30）
// =============================================================================
TArray<int32> UBoardDataAssetHost::GetTilesInGroup(EColorGroup Group) const
{
	// AC-26：Group==None 直接返回空数组（非地产格无色组，不报错）
	// LoadedBoard==null 同样返回空数组（安全兜底）
	const UBoardDataAsset* Board = GetLoadedBoard();
	if (!Board || Group == EColorGroup::None)
	{
		return TArray<int32>{};
	}

	// 遍历 Tiles，收集 ColorGroup==Group 的成员格子的 TileIndex 字段值
	TArray<int32> Result;
	for (const FBoardTileData& Tile : Board->Tiles)
	{
		if (Tile.ColorGroup == Group)
		{
			// 收集的是 FBoardTileData::TileIndex 字段值（棋盘位置），非数组下标
			Result.Add(Tile.TileIndex);
		}
	}

	// AC-30：显式 Sort 保证升序，不依赖 DA 录入顺序
	// Sort 即使 Result 为空也安全（TArray::Sort 对空数组为空操作）
	Result.Sort();

	return Result;
}

// =============================================================================
// GetBoardId — 获取棋盘逻辑标识符（AC-28）
// =============================================================================
FName UBoardDataAssetHost::GetBoardId() const
{
	const UBoardDataAsset* Board = GetLoadedBoard();

	// LoadedBoard==null → NAME_None（安全兜底，AC-28）
	// 取顶层 BoardIdentifier 字段（作者手填），禁止从路径/PrimaryAssetId 派生
	return Board ? Board->BoardIdentifier : NAME_None;
}
