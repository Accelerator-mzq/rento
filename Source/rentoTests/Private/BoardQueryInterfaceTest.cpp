// BoardQueryInterfaceTest.cpp
// =============================================================================
// 单元测试：UBoardDataAssetHost 只读查询接口集
// story-004 AC-1/26/27/28/30（全部 [Logic] BLOCKING）
//
// 物理路径：Source/rentoTests/Private/BoardQueryInterfaceTest.cpp
// 逻辑路径：tests/unit/board/board_query_interface_test.cpp
// Automation 类目：Rento.Board.QueryInterface
//
// headless 策略（-nullrhi，无磁盘 IO）：
//   复用 bd-002 加载模式：
//     1. CreateTestWorld(EWorldType::Game) → GetSubsystem<UBoardDataAssetHost>
//     2. NewObject<UBoardDataAsset>(RF_Transient) 构造带已知字段的 fixture
//     3. 通过 BoardToLoad seam + OnWorldBeginPlay + FlushAsyncLoading 推进至 Active
//     4. 直接对 Host 调用查询接口，断言结果
//   全部 fixture 代码构造，无随机 / 文件 IO，确定性。
//
// 测试函数列表（7 个）：
//   TC_AC1_GetTileCount_40_And_Index0IsGo      → AC-1  (GetTileCount==40, index0=Go,
//                                                         GAP-4: GetTileData(39).TileIndex==39 边界 pin)
//   TC_AC26_GetTilesInGroup_None_Empty         → AC-26 (Group==None → 空数组，不崩)
//                                                BLK-2: null-board + LightBlue → 空（真验 null-guard）
//   TC_AC27_GetTileData_OutOfBounds            → AC-27 (越界 → Error + 默认空 struct)
//   TC_AC28_GetBoardId_Stable                  → AC-28 (同进程多次调用，结果一致)
//   TC_AC30_GetTilesInGroup_SortedAscending    → AC-30 (录入顺序 [9,6,8] → 返回 [6,8,9])
//                                                GAP-2: Green 0 成员 → 空数组（Sort 对空安全）
//   TC_NullBoard_SafeDefaults                  → GAP-1 (null-board 兜底全家桶)
//                                                GetTileCount==0 / GetTileData(0)==默认 struct
//
// AC-27 logged decision（循 FF-003/bd-003 AC-34 惯例）：
//   实现用 UE_LOG(LogTemp, Error) 替代 ensure(false)，避免 headless 下
//   callstack dump 致 AddExpectedError 计数不可靠。
//   测试用 AddExpectedError(Contains "越界", 2) 精确吞掉上界+下界两次 Error。
//
// AC-30 非 vacuous 设计：
//   LightBlue 格以 TileIndex 字段值 9/6/8 的顺序录入 Tiles 数组。
//   若实现不 Sort 直接返回录入顺序，得 [9,6,8]，断言 [6,8,9] → 真 FAIL。
//   有中间值 8 防止"恰好反转"等价通过。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "BoardDataAssetHost.h"
#include "BoardDataAsset.h"

// =============================================================================
// 辅助命名空间：headless World + fixture 工厂（复用 bd-002 模式）
// =============================================================================
namespace BoardQueryTestHelpers
{
	/**
	 * 创建最小 Game World（不通知 Engine，headless 安全）。
	 * 对齐 BoardDAHostTestHelpers::CreateTestWorld 模式。
	 */
	static UWorld* CreateTestWorld(const FName& WorldName)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false, WorldName);
		check(World);
		return World;
	}

	/** 销毁测试 World */
	static void DestroyTestWorld(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		}
	}

	/**
	 * 构造带已知 Tiles 的 transient UBoardDataAsset fixture。
	 *
	 * 由调用方传入 SetupFn 对 DA 字段赋值，保持工厂通用。
	 * RF_Transient：不落磁盘，进程内存活，headless 安全。
	 *
	 * @param UniqueSuffix 唯一后缀，防同进程内多次创建命名冲突
	 * @param SetupFn      回调，对 DA 赋值（Tiles / BoardIdentifier 等）
	 * @return transient UBoardDataAsset 的软引用（用于 BoardToLoad seam）
	 */
	static TSoftObjectPtr<UBoardDataAsset> MakeFixtureDA(
		const FString& UniqueSuffix,
		TFunctionRef<void(UBoardDataAsset*)> SetupFn)
	{
		// 静态计数器保证同进程内多次运行不冲突
		static int32 Counter = 0;
		++Counter;

		const FString ObjName = FString::Printf(
			TEXT("BoardQueryFixture_%s_%d"), *UniqueSuffix, Counter);

		UBoardDataAsset* DA = NewObject<UBoardDataAsset>(
			GetTransientPackage(),
			*ObjName,
			RF_Transient);
		check(DA);

		// 调用方负责填充 DA 字段
		SetupFn(DA);

		FSoftObjectPath SoftPath(DA);
		return TSoftObjectPtr<UBoardDataAsset>(SoftPath);
	}

	/**
	 * 通过 BoardToLoad seam 将 fixture DA 加载到 Host，推进至 Active 态。
	 * 对齐 bd-002 TC-L2a/TC-L2b 的加载模式。
	 *
	 * @param Host        UBoardDataAssetHost 实例（来自 World Subsystem）
	 * @param World       所属 World（OnWorldBeginPlay 需要）
	 * @param BoardRef    fixture DA 软引用
	 * @return true = 推进至 Active；false = 加载失败（测试内部早退）
	 */
	static bool LoadFixtureIntoHost(
		UBoardDataAssetHost* Host,
		UWorld* World,
		TSoftObjectPtr<UBoardDataAsset> BoardRef)
	{
		Host->BoardToLoad = BoardRef;
		Host->OnWorldBeginPlay(*World);
		FlushAsyncLoading();
		return Host->GetLoadState() == EHostLoadState::Active;
	}
}

// =============================================================================
// TC_AC1 — AC-1（GetTileCount + index 0 = Go）
//
// GIVEN fixture 棋盘 N=40，Tiles[0].TileType==Go（其余 39 格占位 Property）
// WHEN  GetTileCount()
// THEN  返回 40
// AND   GetTileData(0).TileType == ETileType::Go
//
// 非 vacuous：
//   若 Tiles 构造数量错误（如 39 格），TestEqual(40) → FAIL
//   若 TileType 字段构造错误（如仍为默认 Property），TestEqual(Go) → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardQuery_TC_AC1_GetTileCount_40_And_Index0IsGo,
	"Rento.Board.QueryInterface.TC_AC1_GetTileCount_40_And_Index0IsGo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardQuery_TC_AC1_GetTileCount_40_And_Index0IsGo::RunTest(const FString& Parameters)
{
	UWorld* World = BoardQueryTestHelpers::CreateTestWorld(TEXT("TC_AC1_World"));
	if (!TestNotNull(TEXT("TC-AC1: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-AC1: Host 应创建成功"), Host))
	{
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// GIVEN：构造 40 格 fixture（Tile[0]=Go，其余 Property 占位）
	TSoftObjectPtr<UBoardDataAsset> BoardRef = BoardQueryTestHelpers::MakeFixtureDA(
		TEXT("TC_AC1"),
		[](UBoardDataAsset* DA)
		{
			DA->BoardIdentifier = FName("Classic40");

			// 构造 40 格：index 0 = Go（AC-1 要求），其余 Property 占位
			DA->Tiles.SetNum(40);
			for (int32 i = 0; i < 40; ++i)
			{
				DA->Tiles[i].TileIndex = i;
				// 默认 TileType = Property，index 0 覆盖为 Go
				DA->Tiles[i].TileType = (i == 0) ? ETileType::Go : ETileType::Property;
			}
		});

	// WHEN：加载到 Active 态
	if (!BoardQueryTestHelpers::LoadFixtureIntoHost(Host, World, BoardRef))
	{
		AddError(TEXT("TC-AC1: 加载失败，状态未到 Active，无法继续查询断言"));
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// THEN-1：GetTileCount() == 40（AC-1 核心，非 vacuous）
	TestEqual(
		TEXT("TC-AC1: GetTileCount() 应返回 40（N=40 经典盘，AC-1）"),
		Host->GetTileCount(), 40);

	// THEN-2：GetTileData(0).TileType == Go（AC-1 核心，非 vacuous）
	const FBoardTileData Tile0 = Host->GetTileData(0);
	TestEqual(
		TEXT("TC-AC1: GetTileData(0).TileType 应为 Go（AC-1 Index 0 = Go 格）"),
		Tile0.TileType, ETileType::Go);

	// THEN-3（GAP-4）：GetTileData(39) 合法末位访问 → 返回 Tiles[39]（非越界默认）
	// pin 实现用 `Index >= Count`（严格排除 39）而非 `Index > Count`（会放行 39 越界读 Tiles[40]）。
	// fixture 中 Tiles[39].TileIndex==39（循环填充），与越界返回的默认 TileIndex==0 判然不同。
	// 非 vacuous：若边界条件写 `Index > Count`，GetTileData(39) 正确但 GetTileData(40) 会越界
	//             读脏数据（TC_AC27 那条测试才会捕捉）；此处只确认合法末位不被误拒。
	const FBoardTileData Tile39 = Host->GetTileData(39);
	TestEqual(
		TEXT("TC-AC1 GAP-4: GetTileData(39) 应返回 Tiles[39].TileIndex==39（合法末位，非越界默认 0）"),
		Tile39.TileIndex, 39);
	TestEqual(
		TEXT("TC-AC1 GAP-4: GetTileData(39) TileType 应为 Property（fixture 第 39 格为 Property）"),
		Tile39.TileType, ETileType::Property);

	BoardQueryTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC_AC26 — AC-26（GetTilesInGroup(None) → 空数组，不崩）
//          + BLK-2 修复（null-board 调 LightBlue 验真 null-guard）
//
// 路径 A（BLK-2）：LoadedBoard==null + Group=LightBlue（非 None）→ 空数组
//   ⚠ 原版用 GetTilesInGroup(None)，被实现的 None-guard 短路，
//     无法区分「null-board 兜底」和「None-guard 返回空」两条代码路径。
//   改为传 LightBlue：`if(!Board || Group==None)` 中 Board==null 分支生效，
//   与 None-guard 分支解耦，真正 pin null-board 兜底代码路径。
//
// 路径 B（AC-26 核心）：已加载棋盘 + Group=None → 空数组
//   fixture 所有格 ColorGroup==None；若遍历未早退则 Num()==4 → TestEqual(0) FAIL
//
// 非 vacuous：
//   路径 A：若移除 null-board 检查，Board==null 时 `Board->Tiles` 空指针崩溃 → FAIL
//   路径 B：若 None-guard 遍历了 None 格，Num()>0 → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardQuery_TC_AC26_GetTilesInGroup_None_Empty,
	"Rento.Board.QueryInterface.TC_AC26_GetTilesInGroup_None_Empty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardQuery_TC_AC26_GetTilesInGroup_None_Empty::RunTest(const FString& Parameters)
{
	// --- 路径 A（BLK-2）：LoadedBoard==null + Group=LightBlue → 空数组（真验 null-guard）---
	// 直接 NewObject Host（不走 World Subsystem，未调加载，LoadedBoard==null）
	UBoardDataAssetHost* UnloadedHost = NewObject<UBoardDataAssetHost>(
		GetTransientPackage(), TEXT("TC_AC26_UnloadedHost"), RF_Transient);
	if (!TestNotNull(TEXT("TC-AC26: UnloadedHost 应创建成功"), UnloadedHost)) { return false; }

	// ⚠ 用 LightBlue（非 None）：绕过 None-guard，单独验 null-board 兜底分支。
	// 若实现移除 null-board 检查，Board==null 时 `Board->Tiles` 崩溃 → FAIL（最强 FAIL）
	TArray<int32> ResultNull = UnloadedHost->GetTilesInGroup(EColorGroup::LightBlue);
	TestEqual(
		TEXT("TC-AC26: LoadedBoard==null 时 GetTilesInGroup(LightBlue) 应返回空数组"
		     "（null-board 兜底，BLK-2 修复：用非 None 组隔离 null-guard 路径）"),
		ResultNull.Num(), 0);

	// --- 路径 B：已加载棋盘，Group==None 仍返回空 ---
	UWorld* World = BoardQueryTestHelpers::CreateTestWorld(TEXT("TC_AC26_World"));
	if (!TestNotNull(TEXT("TC-AC26: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-AC26: Host 应创建成功"), Host))
	{
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// 构造含若干 Property 格（ColorGroup 默认 None）的 fixture
	TSoftObjectPtr<UBoardDataAsset> BoardRef = BoardQueryTestHelpers::MakeFixtureDA(
		TEXT("TC_AC26"),
		[](UBoardDataAsset* DA)
		{
			DA->BoardIdentifier = FName("AC26Board");
			DA->Tiles.SetNum(4);
			for (int32 i = 0; i < 4; ++i)
			{
				DA->Tiles[i].TileIndex  = i;
				DA->Tiles[i].TileType   = ETileType::Property;
				// ColorGroup 默认 EColorGroup::None（FBoardTileData 默认值）
			}
		});

	if (!BoardQueryTestHelpers::LoadFixtureIntoHost(Host, World, BoardRef))
	{
		AddError(TEXT("TC-AC26: 加载失败，无法继续断言"));
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// WHEN + THEN：GetTilesInGroup(None) → 空数组（AC-26）
	// 非 vacuous：fixture 中所有格 ColorGroup==None，若遍历未早退则 Num()==4 → FAIL
	TArray<int32> Result = Host->GetTilesInGroup(EColorGroup::None);
	TestEqual(
		TEXT("TC-AC26: GetTilesInGroup(EColorGroup::None) 应返回空数组（AC-26，不遍历 None）"),
		Result.Num(), 0);

	BoardQueryTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC_AC27 — AC-27（越界访问保护）
//
// GIVEN N=40 已加载棋盘
// WHEN  GetTileData(40)（上界越界）和 GetTileData(-1)（下界越界）
// THEN  各返回默认空 FBoardTileData{}（TileIndex==0, TileType==Property 默认）
//        各打一条 UE_LOG(LogTemp, Error) 含 "越界" 子串
//        AddExpectedError(Contains "越界", 2) 精确吞掉两次
//
// 非 vacuous：
//   - 返回的是默认 struct，而非 Tiles 中的脏数据（断言 TileType==Property==默认值，
//     而 Tile[39] 也是 Property——所以加断言 TileIndex==0 而非 39 更精确）
//   - 若实现不做越界检查直接访问 Tiles[40]，出现越界读/崩溃 → Test FAIL（最强 FAIL）
//   - 若返回 Tiles[0] 而非默认空 struct，TileIndex==0+TileType==Go（fixture 设 Go）→ FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardQuery_TC_AC27_GetTileData_OutOfBounds,
	"Rento.Board.QueryInterface.TC_AC27_GetTileData_OutOfBounds_ErrorLog",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardQuery_TC_AC27_GetTileData_OutOfBounds::RunTest(const FString& Parameters)
{
	UWorld* World = BoardQueryTestHelpers::CreateTestWorld(TEXT("TC_AC27_World"));
	if (!TestNotNull(TEXT("TC-AC27: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-AC27: Host 应创建成功"), Host))
	{
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// GIVEN：N=40 fixture（index 0 = Go，其余 Property）
	TSoftObjectPtr<UBoardDataAsset> BoardRef = BoardQueryTestHelpers::MakeFixtureDA(
		TEXT("TC_AC27"),
		[](UBoardDataAsset* DA)
		{
			DA->BoardIdentifier = FName("AC27Board");
			DA->Tiles.SetNum(40);
			for (int32 i = 0; i < 40; ++i)
			{
				DA->Tiles[i].TileIndex = i;
				DA->Tiles[i].TileType  = (i == 0) ? ETileType::Go : ETileType::Property;
			}
		});

	if (!BoardQueryTestHelpers::LoadFixtureIntoHost(Host, World, BoardRef))
	{
		AddError(TEXT("TC-AC27: 加载失败，无法继续断言"));
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// 前提确认：有效范围 [0,40)，GetTileCount()==40
	if (!TestEqual(TEXT("TC-AC27 前提: GetTileCount 应为 40"), Host->GetTileCount(), 40))
	{
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// 声明预期 Error log（每次越界恰好打一条含 "越界" 子串的 Error）
	// AddExpectedError(Contains, 2) 精确吞掉上界 + 下界两次（AC-27 logged decision）
	AddExpectedError(TEXT("越界"), EAutomationExpectedErrorFlags::Contains, 2);

	// WHEN-1：上界越界 GetTileData(40)（Index == N，恰好超出）
	const FBoardTileData ResultOver = Host->GetTileData(40);

	// THEN-1：返回默认空 FBoardTileData{}
	// 默认值：TileIndex==0, TileType==Property（FBoardTileData 默认构造）
	// 精确断言 TileType==Property（而非 Go），区分「返回默认空」和「返回 Tiles[0]」：
	//   Tiles[0].TileType==Go；默认构造 TileType==Property → 若返回 Tiles[0] 则 FAIL
	TestEqual(
		TEXT("TC-AC27: GetTileData(40) 应返回默认 TileType==Property（非 Tiles[0].Go，AC-27）"),
		ResultOver.TileType, ETileType::Property);
	TestEqual(
		TEXT("TC-AC27: GetTileData(40) 应返回默认 TileIndex==0（默认空 struct，AC-27）"),
		ResultOver.TileIndex, 0);

	// WHEN-2：下界越界 GetTileData(-1)
	const FBoardTileData ResultUnder = Host->GetTileData(-1);

	// THEN-2：同样返回默认空 FBoardTileData{}
	TestEqual(
		TEXT("TC-AC27: GetTileData(-1) 应返回默认 TileType==Property（默认空 struct，AC-27）"),
		ResultUnder.TileType, ETileType::Property);
	TestEqual(
		TEXT("TC-AC27: GetTileData(-1) 应返回默认 TileIndex==0（默认空 struct，AC-27）"),
		ResultUnder.TileIndex, 0);

	BoardQueryTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC_AC28 — AC-28（GetBoardId 同进程稳定性 + 与路径解耦）
//
// GIVEN fixture DA 设 BoardIdentifier=FName("Classic40")
// WHEN  同进程内多次调 GetBoardId()
// THEN  每次均返回 FName("Classic40")（非空，与路径解耦，完全一致）
//
// 非 vacuous：
//   若实现从资产路径派生 BoardId（如 SoftPath.GetAssetName()），
//   transient 路径名为 "BoardQueryFixture_TC_AC28_N"，而非 "Classic40" → FAIL
//   若未加载时返回 NAME_None，TestNotEqual(NAME_None) → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardQuery_TC_AC28_GetBoardId_Stable,
	"Rento.Board.QueryInterface.TC_AC28_GetBoardId_Stable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardQuery_TC_AC28_GetBoardId_Stable::RunTest(const FString& Parameters)
{
	UWorld* World = BoardQueryTestHelpers::CreateTestWorld(TEXT("TC_AC28_World"));
	if (!TestNotNull(TEXT("TC-AC28: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-AC28: Host 应创建成功"), Host))
	{
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// 验证未加载时返回 NAME_None（安全兜底）
	TestEqual(
		TEXT("TC-AC28: 未加载时 GetBoardId() 应返回 NAME_None（安全兜底）"),
		Host->GetBoardId(), NAME_None);

	// GIVEN：fixture DA 显式设 BoardIdentifier="Classic40"
	const FName ExpectedId = FName("Classic40");
	TSoftObjectPtr<UBoardDataAsset> BoardRef = BoardQueryTestHelpers::MakeFixtureDA(
		TEXT("TC_AC28"),
		[ExpectedId](UBoardDataAsset* DA)
		{
			// 作者手填的逻辑 ID，与资产路径（TransientBoardQuery_TC_AC28_N）完全不同
			DA->BoardIdentifier = ExpectedId;
			// 最小化格子（查询接口测试不需要大量格子）
			DA->Tiles.SetNum(1);
			DA->Tiles[0].TileIndex = 0;
			DA->Tiles[0].TileType  = ETileType::Go;
		});

	if (!BoardQueryTestHelpers::LoadFixtureIntoHost(Host, World, BoardRef))
	{
		AddError(TEXT("TC-AC28: 加载失败，无法继续断言"));
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// WHEN + THEN：多次调 GetBoardId()，结果完全一致且与 BoardIdentifier 字段对齐
	// FName 比较是 case-insensitive（ADR-0002 R3 注）；"Classic40" 与 "classic40" 相等
	const FName Id1 = Host->GetBoardId();
	const FName Id2 = Host->GetBoardId();
	const FName Id3 = Host->GetBoardId();

	// 非空（AC-28：返回非空 FName）
	TestNotEqual(
		TEXT("TC-AC28: GetBoardId() 应非 NAME_None（AC-28）"),
		Id1, NAME_None);

	// 等于 DA 内显式 BoardIdentifier 字段（AC-28 解耦：transient 资产路径名截然不同）
	// 非 vacuous：若从路径派生，得到的是 transient 对象名而非 "Classic40" → FAIL
	TestEqual(
		TEXT("TC-AC28: GetBoardId() 应等于 DA.BoardIdentifier='Classic40'（AC-28 解耦）"),
		Id1, ExpectedId);

	// 同进程多次调用完全一致（AC-28 稳定性）
	TestEqual(
		TEXT("TC-AC28: 第 1/2 次 GetBoardId() 应完全一致（AC-28 稳定性）"),
		Id1, Id2);
	TestEqual(
		TEXT("TC-AC28: 第 2/3 次 GetBoardId() 应完全一致（AC-28 稳定性）"),
		Id2, Id3);

	BoardQueryTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC_AC30 — AC-30（GetTilesInGroup 升序契约，非 vacuous）
//
// GIVEN fixture 棋盘，LightBlue 成员以 TileIndex 字段值 9/6/8 的录入顺序写入 Tiles
//        （即 Tiles[0].TileIndex=9, Tiles[1].TileIndex=6, Tiles[2].TileIndex=8，
//          且三格 ColorGroup==LightBlue）
// WHEN  GetTilesInGroup(EColorGroup::LightBlue)
// THEN  返回 [6, 8, 9]（严格 TileIndex 字段值升序）
//
// 非 vacuous 设计（关键）：
//   - 录入顺序 9,6,8 != 升序 6,8,9 != 降序 9,8,6
//   - 含中间值 8，防止"恰好反转"等价通过（[6,9] 场景中 Sort vs 反转结果相同）
//   - 若实现不 Sort 直接返回录入顺序，得 [9,6,8]，断言 [6,8,9] → 真 FAIL
//   - 若实现 Sort 但收集的是数组下标而非 TileIndex 字段值，得 [0,1,2]，断言 [6,8,9] → FAIL
//
// 同时验证 GetTilesInGroup 对其他色组（非 LightBlue）正确过滤（Brown 格不进结果）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardQuery_TC_AC30_GetTilesInGroup_SortedAscending,
	"Rento.Board.QueryInterface.TC_AC30_GetTilesInGroup_SortedAscending",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardQuery_TC_AC30_GetTilesInGroup_SortedAscending::RunTest(const FString& Parameters)
{
	UWorld* World = BoardQueryTestHelpers::CreateTestWorld(TEXT("TC_AC30_World"));
	if (!TestNotNull(TEXT("TC-AC30: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-AC30: Host 应创建成功"), Host))
	{
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// GIVEN：构造 fixture 棋盘
	// Tiles 共 5 格，录入顺序如下（TileIndex 字段值即棋盘位置，非数组下标）：
	//   Tiles[0]: TileIndex=9,  ColorGroup=LightBlue  ← 录入第一，但 TileIndex 最大
	//   Tiles[1]: TileIndex=6,  ColorGroup=LightBlue  ← 录入第二，但 TileIndex 最小
	//   Tiles[2]: TileIndex=8,  ColorGroup=LightBlue  ← 录入第三，TileIndex 中间值
	//   Tiles[3]: TileIndex=1,  ColorGroup=Brown       ← 干扰格，不应进入 LightBlue 结果
	//   Tiles[4]: TileIndex=0,  ColorGroup=None        ← Go 格，ColorGroup=None
	//
	// 升序 Sort 后期望：LightBlue → [6, 8, 9]（TileIndex 字段值升序，非录入顺序 [9,6,8]）
	TSoftObjectPtr<UBoardDataAsset> BoardRef = BoardQueryTestHelpers::MakeFixtureDA(
		TEXT("TC_AC30"),
		[](UBoardDataAsset* DA)
		{
			DA->BoardIdentifier = FName("AC30Board");
			DA->Tiles.SetNum(5);

			// 以非升序录入 LightBlue 格（TileIndex 字段值 9 > 6 > 8，非升序）
			// 非 vacuous：录入顺序与期望升序完全不同，含中间值 8
			DA->Tiles[0].TileIndex  = 9;                        // 棋盘位置 9
			DA->Tiles[0].TileType   = ETileType::Property;
			DA->Tiles[0].ColorGroup = EColorGroup::LightBlue;

			DA->Tiles[1].TileIndex  = 6;                        // 棋盘位置 6（最小，录入第二）
			DA->Tiles[1].TileType   = ETileType::Property;
			DA->Tiles[1].ColorGroup = EColorGroup::LightBlue;

			DA->Tiles[2].TileIndex  = 8;                        // 棋盘位置 8（中间，录入第三）
			DA->Tiles[2].TileType   = ETileType::Property;
			DA->Tiles[2].ColorGroup = EColorGroup::LightBlue;

			// 干扰格：Brown 色组，不应混入 LightBlue 结果
			DA->Tiles[3].TileIndex  = 1;
			DA->Tiles[3].TileType   = ETileType::Property;
			DA->Tiles[3].ColorGroup = EColorGroup::Brown;

			// Go 格，ColorGroup=None，不应混入任何色组结果
			DA->Tiles[4].TileIndex  = 0;
			DA->Tiles[4].TileType   = ETileType::Go;
			DA->Tiles[4].ColorGroup = EColorGroup::None;
		});

	if (!BoardQueryTestHelpers::LoadFixtureIntoHost(Host, World, BoardRef))
	{
		AddError(TEXT("TC-AC30: 加载失败，无法继续断言"));
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// WHEN：GetTilesInGroup(LightBlue)
	const TArray<int32> Result = Host->GetTilesInGroup(EColorGroup::LightBlue);

	// THEN-1：成员数量正确（3 格 LightBlue，干扰格已过滤）
	if (!TestEqual(TEXT("TC-AC30: LightBlue 应有 3 个成员（Brown/None 已过滤，AC-30）"),
		Result.Num(), 3))
	{
		// 数量错误则后续索引断言无意义，早退
		BoardQueryTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// THEN-2：返回 [6, 8, 9]（TileIndex 字段值严格升序，AC-30 核心断言，非 vacuous）
	// 若不 Sort → [9, 6, 8]（录入顺序），下面三个断言全部 FAIL
	TestEqual(
		TEXT("TC-AC30: Result[0] 应为 6（升序首位，非录入首位 9，AC-30 非 vacuous）"),
		Result[0], 6);
	TestEqual(
		TEXT("TC-AC30: Result[1] 应为 8（中间值，AC-30 防恰好反转）"),
		Result[1], 8);
	TestEqual(
		TEXT("TC-AC30: Result[2] 应为 9（升序末位，非录入末位 8，AC-30 非 vacuous）"),
		Result[2], 9);

	// THEN-3（GAP-2）：未出现在 fixture 中的合法色组 → 空数组，Sort 对空安全，不崩
	// fixture 含 LightBlue(3格)/Brown(1格)/None(1格)，Green 在 fixture 中 0 成员。
	// 验证实现对空结果 Sort 不崩溃（TArray::Sort 对空数组为空操作）。
	// 非 vacuous：若实现误把其他色组混入，Num()>0 → TestEqual(0) FAIL
	const TArray<int32> GreenResult = Host->GetTilesInGroup(EColorGroup::Green);
	TestEqual(
		TEXT("TC-AC30 GAP-2: GetTilesInGroup(Green) fixture 中 0 成员，应返回空数组"
		     "（Sort 对空安全，不崩，GAP-2）"),
		GreenResult.Num(), 0);

	BoardQueryTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC_NullBoard_SafeDefaults — GAP-1（null-board 全函数兜底 pin）
//
// GIVEN UBoardDataAssetHost 未加载（LoadedBoard==null，GetLoadState()==Empty）
// WHEN  调用所有查询接口
// THEN  GetTileCount() == 0
//        GetTileData(0) 返回默认空 FBoardTileData（TileType==Property，非崩溃）
//        （GetBoardId→NAME_None 已在 TC_AC28 覆盖；
//          GetTilesInGroup(LightBlue)→空 已在 TC_AC26 路径 A 覆盖，不重复）
//
// 为何独立测试函数（而非合并进其他 TC）：
//   null-board 全家桶属于「接口安全契约」，独立函数意图最清晰，
//   且与 AC-1/AC-26/AC-27/AC-28/AC-30 的「已加载」语境完全隔离。
//
// 非 vacuous：
//   GetTileCount==0：若实现 null 时返回任意正数 → FAIL
//   GetTileData(0) TileType==Property：若实现 null 时崩溃 → FAIL（最强 FAIL）；
//     若意外返回 TileType==Go → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardQuery_TC_NullBoard_SafeDefaults,
	"Rento.Board.QueryInterface.TC_NullBoard_SafeDefaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardQuery_TC_NullBoard_SafeDefaults::RunTest(const FString& Parameters)
{
	// GIVEN：直接 NewObject，不走 World Subsystem，不调加载
	// → LoadedBoard==null，GetLoadState()==Empty（C++ 默认初始化）
	UBoardDataAssetHost* Host = NewObject<UBoardDataAssetHost>(
		GetTransientPackage(), TEXT("TC_NullBoard_Host"), RF_Transient);
	if (!TestNotNull(TEXT("TC-NullBoard: Host 应创建成功"), Host)) { return false; }

	// 前提确认：确实未加载
	TestEqual(
		TEXT("TC-NullBoard 前提: GetLoadState 应为 Empty"),
		Host->GetLoadState(), EHostLoadState::Empty);
	TestNull(
		TEXT("TC-NullBoard 前提: GetLoadedBoard 应为 nullptr"),
		Host->GetLoadedBoard());

	// THEN-1：GetTileCount() == 0（null-board 兜底，AC-1 语义，GAP-1）
	// 非 vacuous：若实现 null 时返回 -1 或任意正数 → FAIL
	TestEqual(
		TEXT("TC-NullBoard GAP-1: GetTileCount() 应返回 0（LoadedBoard==null 兜底）"),
		Host->GetTileCount(), 0);

	// THEN-2：GetTileData(0) 返回默认空 FBoardTileData（AC-27 null 分支，GAP-1）
	// Index=0 在「有效范围」概念上合理，但 Count==0 时属于越界（0 >= 0），
	// 实现走 null-board/Count==0 → 越界分支 → UE_LOG(Error) + 返回默认空 struct。
	// TileType==Property（默认构造），非 Go（无法是 Go，没有 DA）。
	// 非 vacuous：若实现 null 时崩溃 → FAIL（最强 FAIL）；TileType!=Property → FAIL
	AddExpectedError(TEXT("越界"), EAutomationExpectedErrorFlags::Contains, 1);
	const FBoardTileData DefaultTile = Host->GetTileData(0);
	TestEqual(
		TEXT("TC-NullBoard GAP-1: GetTileData(0) TileType 应为默认 Property（null-board 走越界分支，GAP-1）"),
		DefaultTile.TileType, ETileType::Property);
	TestEqual(
		TEXT("TC-NullBoard GAP-1: GetTileData(0) TileIndex 应为默认 0（默认空 struct）"),
		DefaultTile.TileIndex, 0);

	return true;
}
