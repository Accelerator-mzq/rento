// BoardIdSaveReferenceTest.cpp
// =============================================================================
// 集成测试：棋盘 DA 存档引用契约（BoardIdentifier）— 存引用不存布局
// story-008 AC-板存档供给 / AC-28b（Advisory→存档21）/ OQ-8 回链登记
//
// 物理路径：Source/rentoTests/Private/BoardIdSaveReferenceTest.cpp
// 逻辑路径：tests/integration/board/board_id_save_reference_test.cpp（story-008 §Test Evidence）
// Automation 类目：Rento.Board.SaveReference
//
// TC ↔ AC 映射：
//   TC1 (TC1_GetBoardIdReturnsTopLevelIdentifier)  → AC-板存档供给 / AC-28
//   TC2 (TC2_GetBoardIdNoneWhenNoBoard)             → AC-板存档供给（null 契约）
//   TC3 (TC3_NoLayoutSerializationOnBoardSide)      → AC-板存档供给 / ADR-0005 存DA引用不存布局
//
// 测试机制：headless -nullrhi，无磁盘 IO，无 RHI。
//   NewObject<UBoardDataAsset>(GetTransientPackage(), ..., RF_Transient) 创建内存态 transient DA，
//   内存中已存在的对象在 RequestAsyncLoad 后走同步完成路径，headless 安全。
//   harness 范式复用 BoardDataAssetHostLifecycleTest (BoardDAHostTestHelpers)。
//
// 与 save-load epic 21 的分工（ADR-0005 + story-008 §Implementation Notes §5）：
//   本文件 — 验板侧契约：GetBoardId() 返回正确顶层标识符 + 无布局序列化接口。
//   save-load epic 21 — 跨会话 round-trip BLOCKING 测试（存档写/读/重启，
//                        板侧无法跨进程验证，归 tests/integration/save-load/）。
// =============================================================================

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/DataAsset.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "Async/AsyncWork.h"
#include "BoardDataAssetHost.h"
#include "BoardDataAsset.h"
#include "DataAssetHostSubsystem.h"

// =============================================================================
// 辅助命名空间：headless 测试 World + transient DA 工厂
// 范式对齐 BoardDataAssetHostLifecycleTest (BoardDAHostTestHelpers)
// =============================================================================
namespace BoardIdSaveRefHelpers
{
	/**
	 * 创建指定 WorldType 的最小测试 World（不通知 Engine，headless 安全）。
	 * 调用方负责 World->DestroyWorld(false) 清理。
	 */
	static UWorld* CreateTestWorld(EWorldType::Type WorldType, const FName& WorldName)
	{
		UWorld* World = UWorld::CreateWorld(WorldType, /*bInformEngineOfWorld=*/false, WorldName);
		check(World);
		return World;
	}

	/** 销毁测试 World，释放资源 */
	static void DestroyTestWorld(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		}
	}

	/**
	 * 创建内存态 transient UBoardDataAsset（免磁盘 IO，headless 安全）。
	 *
	 * RF_Transient：不进磁盘，进程内存活，headless 安全。
	 * 静态计数器保证同进程内多次运行对象名不冲突。
	 *
	 * @param UniqueSuffix 附加在对象名末尾的唯一后缀（测试函数名缩写）
	 * @return transient UBoardDataAsset 实例的软引用
	 */
	static TSoftObjectPtr<UBoardDataAsset> MakeTransientBoardDARef(const FString& UniqueSuffix)
	{
		// 静态计数器：同进程内多次运行不冲突
		static int32 Counter = 0;
		++Counter;

		const FString ObjName = FString::Printf(
			TEXT("TransientBoardDA_%s_%d"), *UniqueSuffix, Counter);

		UBoardDataAsset* DA = NewObject<UBoardDataAsset>(
			GetTransientPackage(),
			*ObjName,
			RF_Transient);

		check(DA);

		// 内存对象路径形如 /Engine/Transient.ObjName
		FSoftObjectPath SoftPath(DA);
		return TSoftObjectPtr<UBoardDataAsset>(SoftPath);
	}
}

// =============================================================================
// TC1 (AC-板存档供给 / AC-28)
// GetBoardId() 返回顶层 BoardIdentifier 字段，与资产名/路径解耦
//
// GIVEN transient UBoardDataAsset，BoardIdentifier="Classic40"（逻辑 ID，作者手填）
// WHEN  OnWorldBeginPlay + FlushAsyncLoading → Active
// THEN  GetBoardId() == FName("Classic40")（取顶层字段，非从路径/PrimaryAssetId 派生）
//        GetBoardId() != DA->GetFName()（解耦守卫：资产对象名形如 TransientBoardDA_TC1_N，≠ "Classic40"）
//
// 非 vacuous：若从路径/PrimaryAssetId 派生 ID → GetBoardId() == AssetObjectName → TestNotEqual FAIL
//             这正是 AC-28 要防的 bug（资产改名后旧存档静默损坏）
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardIdSaveRef_TC1_GetBoardIdReturnsTopLevelIdentifier,
	"Rento.Board.SaveReference.TC1_GetBoardIdReturnsTopLevelIdentifier",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardIdSaveRef_TC1_GetBoardIdReturnsTopLevelIdentifier::RunTest(const FString& Parameters)
{
	// GIVEN：创建 Game World（触发 Subsystem 初始化）
	UWorld* World = BoardIdSaveRefHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC1_SaveRef_World"));
	if (!TestNotNull(TEXT("TC-1: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-1: UBoardDataAssetHost 应在 Game World 自动创建"), Host))
	{
		BoardIdSaveRefHelpers::DestroyTestWorld(World);
		return false;
	}

	// GIVEN：创建 transient DA，手填逻辑 ID（与资产对象名故意不同）
	TSoftObjectPtr<UBoardDataAsset> Ref = BoardIdSaveRefHelpers::MakeTransientBoardDARef(TEXT("TC1"));
	UBoardDataAsset* DA = Ref.Get();
	if (!TestNotNull(TEXT("TC-1: transient UBoardDataAsset 应非空"), DA))
	{
		BoardIdSaveRefHelpers::DestroyTestWorld(World);
		return false;
	}

	// 作者手填逻辑 ID（"Classic40"）；资产对象名形如 "TransientBoardDA_TC1_1"
	DA->BoardIdentifier = FName(TEXT("Classic40"));
	// 记录资产对象名，用于解耦守卫断言（非 vacuous）
	const FName AssetObjectName = DA->GetFName();

	// WHEN：通过 BoardToLoad seam 发起加载
	Host->BoardToLoad = Ref;
	Host->OnWorldBeginPlay(*World);
	FlushAsyncLoading();

	// 前提：状态应为 Active（加载链完整）
	if (!TestEqual(TEXT("TC-1 前提: 状态应为 Active"),
		Host->GetLoadState(), EHostLoadState::Active))
	{
		BoardIdSaveRefHelpers::DestroyTestWorld(World);
		return false;
	}

	// THEN-1：GetBoardId() 返回顶层 BoardIdentifier 字段（存档唯一棋盘引用）
	TestEqual(
		TEXT("TC-1: GetBoardId()==BoardIdentifier(\"Classic40\")（存档引用取顶层字段）"),
		Host->GetBoardId(), FName(TEXT("Classic40")));

	// THEN-2：解耦守卫（非 vacuous）——GetBoardId() != 资产对象名
	// 若从路径/PrimaryAssetId 派生 ID → 返回 AssetObjectName → TestNotEqual FAIL
	// 正是 AC-28 要防的 bug：资产改名/移动后旧存档静默损坏
	TestNotEqual(
		TEXT("TC-1: GetBoardId() != 资产对象名（与路径/PrimaryAssetId 解耦，跨会话稳定，AC-28）"),
		Host->GetBoardId(), AssetObjectName);

	BoardIdSaveRefHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC2 (AC-板存档供给)
// 无板时 GetBoardId() == NAME_None（存档引用 null 契约）
//
// GIVEN UBoardDataAssetHost fresh 实例，未加载任何 board
// WHEN  直接查询 GetBoardId()（不触发加载）
// THEN  GetLoadedBoard() == nullptr（前提：未加载）
//        GetBoardId() == NAME_None（无 id 可存；host header L146 契约）
//
// 非 vacuous：若 GetBoardId() 在无板时返回任意非 None 值（如 Default/Empty string→FName）
//             → TestEqual NAME_None FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardIdSaveRef_TC2_GetBoardIdNoneWhenNoBoard,
	"Rento.Board.SaveReference.TC2_GetBoardIdNoneWhenNoBoard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardIdSaveRef_TC2_GetBoardIdNoneWhenNoBoard::RunTest(const FString& Parameters)
{
	// GIVEN：创建 Game World（触发 Subsystem 初始化）
	UWorld* World = BoardIdSaveRefHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC2_SaveRef_World"));
	if (!TestNotNull(TEXT("TC-2: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-2: UBoardDataAssetHost 应在 Game World 自动创建"), Host))
	{
		BoardIdSaveRefHelpers::DestroyTestWorld(World);
		return false;
	}

	// 前提：fresh host 未加载任何 board（未调 OnWorldBeginPlay+BoardToLoad）
	TestNull(
		TEXT("TC-2 前提: GetLoadedBoard()==nullptr（未加载，board host 初始态）"),
		Host->GetLoadedBoard());

	// 契约断言：无板时 GetBoardId() == NAME_None
	// BoardDataAssetHost.h L146 注释明确：LoadedBoard==null 时返回 NAME_None
	TestEqual(
		TEXT("TC-2: 无板时 GetBoardId()==NAME_None（存档引用 null 行为，host header L146）"),
		Host->GetBoardId(), FName(NAME_None));

	BoardIdSaveRefHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC3 (AC-板存档供给 / ADR-0005 存DA引用不存布局)
// UBoardDataAsset 无 SaveGame 字段 + board host 侧无全量布局序列化接口（反射）
//
// GIVEN UBoardDataAsset::StaticClass() 反射 + UBoardDataAssetHost::StaticClass() 反射
// WHEN  字段扫描（CPF_SaveGame）+ UFUNCTION 黑名单扫描
// THEN  (a) UBoardDataAsset 本类 UPROPERTY(SaveGame) 字段数 == 0
//             （board 静态数据不序列化；存档只存 BoardIdentifier 引用，由 save-load 21 持有）
//        (b) BoardIdentifier 字段存在且是 FNameProperty
//             （非 vacuous 守卫：锚点确实存在，证 (a) 非"类空"空过）
//        (c) UBoardDataAssetHost 无「序列化全量布局」public UFUNCTION（黑名单扫描）
//
// 非 vacuous：
//   (a) 若未来错误加 UPROPERTY(SaveGame) → SaveGameCount > 0 → TestEqual 0 FAIL
//   (b) 若 BoardIdentifier 被删/改名 → FindPropertyByName 返回 nullptr → TestNotNull FAIL
//   (c) 若错误暴露 SerializeLayout 等接口 → AddError + TestFalse FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardIdSaveRef_TC3_NoLayoutSerializationOnBoardSide,
	"Rento.Board.SaveReference.TC3_NoLayoutSerializationOnBoardSide",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardIdSaveRef_TC3_NoLayoutSerializationOnBoardSide::RunTest(const FString& Parameters)
{
	// (a) UBoardDataAsset 本类无任何 UPROPERTY(SaveGame) 字段
	// EFieldIterationFlags::None：仅本类，不含父类（UPrimaryDataAsset/UObject 等基类字段不计入）
	const UClass* DAClass = UBoardDataAsset::StaticClass();
	if (!TestNotNull(TEXT("TC-3: UBoardDataAsset::StaticClass() 应非空"), DAClass))
	{
		return false;
	}

	int32 SaveGameCount = 0;
	for (TFieldIterator<FProperty> It(DAClass, EFieldIterationFlags::None); It; ++It)
	{
		if ((*It)->HasAnyPropertyFlags(CPF_SaveGame))
		{
			++SaveGameCount;
		}
	}

	// 核心断言：board 静态数据不序列化；存档只存 BoardIdentifier 引用（ADR-0005）
	TestEqual(
		TEXT("TC-3: UBoardDataAsset 本类 UPROPERTY(SaveGame) 字段数==0（存DA引用不存布局，board 非存档容器）"),
		SaveGameCount, 0);

	// (b) 非 vacuous 守卫：BoardIdentifier 字段存在且是 FNameProperty
	// 证明 (a) 非因"类空/无字段"而平凡通过——存档引用锚点确实存在，只是未标 SaveGame
	const FProperty* IdProp = DAClass->FindPropertyByName(FName(TEXT("BoardIdentifier")));
	TestNotNull(
		TEXT("TC-3: BoardIdentifier 字段反射可见（存档引用锚点存在，SaveGame 检查非 vacuous）"),
		IdProp);

	if (IdProp)
	{
		// CastField<FNameProperty>：确认是 FName 类型（存档 FName 引用的类型安全）
		TestNotNull(
			TEXT("TC-3: BoardIdentifier 是 FNameProperty（存档引用类型正确）"),
			CastField<FNameProperty>(IdProp));
	}

	// (c) UBoardDataAssetHost 无「序列化全量布局」public UFUNCTION（黑名单扫描，仿 TC-L4）
	// 禁止列表：任何语义为"把全量布局写成可序列化 blob"的接口
	// ADR-0005 Forbidden：Never 序列化全量棋盘布局——静态布局由 DA 重建；存 DA 引用不存布局
	bool bFoundLayoutSerializer = false;
	const TArray<FString> Forbidden = {
		TEXT("SerializeLayout"),
		TEXT("SaveLayout"),
		TEXT("SerializeBoard"),
		TEXT("ExportLayout"),
		TEXT("SerializeFullLayout"),
		TEXT("GetLayoutBlob"),
		TEXT("ToSaveData")
	};

	UClass* HostClass = UBoardDataAssetHost::StaticClass();
	for (TFieldIterator<UFunction> F(HostClass); F; ++F)
	{
		if (*F
			&& ((*F)->FunctionFlags & FUNC_Public)
			&& !((*F)->FunctionFlags & FUNC_Static))
		{
			for (const FString& Bad : Forbidden)
			{
				if ((*F)->GetName().Contains(Bad, ESearchCase::IgnoreCase))
				{
					AddError(FString::Printf(
						TEXT("TC-3: 发现禁止的全量布局序列化接口 %s"),
						*(*F)->GetName()));
					bFoundLayoutSerializer = true;
				}
			}
		}
	}

	// 最终断言：board host 只暴露 GetBoardId 引用，无全量布局序列化入口
	TestFalse(
		TEXT("TC-3: board host 无「序列化全量布局」public 接口（只暴露 GetBoardId 引用，存DA引用不存布局，ADR-0005）"),
		bFoundLayoutSerializer);

	return true;
}
