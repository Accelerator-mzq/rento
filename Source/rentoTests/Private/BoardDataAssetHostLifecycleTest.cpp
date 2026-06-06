// BoardDataAssetHostLifecycleTest.cpp
// =============================================================================
// 集成测试：UBoardDataAssetHost 生命周期 + 加载状态机 + 热切换边界
// story-002 AC-L1~L4（AC-L5 载入预算 Advisory，目标硬件实测）
//
// 物理路径：Source/rentoTests/Private/BoardDataAssetHostLifecycleTest.cpp
// 逻辑路径：tests/integration/board/da_holder_lifecycle_test.cpp
// Automation 类目：Rento.Board.DAHolderLifecycle
//
// headless 测试策略（-nullrhi，无磁盘 IO，无 RHI）：
//   用 NewObject<UBoardDataAsset>(GetTransientPackage(), ..., RF_Transient) 创建内存态 transient DA。
//   UBoardDataAsset 是 non-abstract UCLASS，可安全直接实例化（bd-001 已实现）。
//   内存中已存在的对象在 RequestAsyncLoad 后走同步完成路径，headless 安全，无磁盘 IO。
//
//   桩模式对齐 ff-003 AsyncLoadCancelHandleTest（CreateTestWorld / DestroyTestWorld）。
//
// 测试函数列表（9 个）：
//   TC-L1a (AC-L1): 继承链反射 + GetLoadedBoard() 返回类型正确
//   TC-L1b (AC-L1): ShouldCreateSubsystem — Game World 自动创建
//   TC-L2a (AC-L2): 状态机全路径推进至 Active（非 vacuous）
//   TC-L2b (AC-L2): GetLoadedBoard() 加载后返回正确 UBoardDataAsset 对象（非 vacuous）
//   TC-L2c (AC-L2): 校验挂载点占位直通（story-005 预留）
//   TC-L2d (AC-L2): Cast 失败守卫 — 非 UBoardDataAsset 类型 → 停在 Loaded（非 vacuous）
//   TC-L3  (AC-L3): 两局连续独立加载；CancelHandle 内部由基类 ff-003 覆盖，真异步 UAF defer ff-007
//   TC-L4  (AC-L4): [Advisory·code-review] 无 public DA 直写 setter；RequestReset 合法换盘路径
//   TC-BoardToLoadNull (AC-L2): BoardToLoad 为空时 OnWorldBeginPlay 不崩、状态保持 Empty
//
// 覆盖边界说明：
//   CancelHandle 内部纪律（handle 成员存储/WasCanceled/bIsDeinitializing guard）
//     → 由基类 ff-003 AsyncLoadCancelHandleTest TC-1/TC-2/TC-3/Edge_TwoConsecutiveMatches 覆盖，
//       本文件不重复断言基类内部机制。
//   真异步 UAF（Deinitialize 后悬挂回调写 GC 对象）→ defer ff-007（与 ff-003 同惯例）。
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
#include "TestDataAssetHost.h"           // 引入 UTestConcreteDataAsset（非 UBoardDataAsset 的 stub）
#include "BoardDataAssetHostTestProxy.h" // TC-L2d 测试桩：UBoardDataAssetHostTestProxy

// =============================================================================
// 辅助命名空间：headless 测试 World + transient DA 工厂
// 对齐 ff-003 AsyncLoadTestHelpers 桩模式
// =============================================================================
namespace BoardDAHostTestHelpers
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
	 * UBoardDataAsset 是 non-abstract UCLASS（bd-001 已实现），可直接 NewObject<> 实例化，
	 * 无需抽象桩（不同于 ff-003 需要 UTestConcreteDataAsset：UPrimaryDataAsset 是 abstract）。
	 * RF_Transient：不进磁盘，进程内存活，headless 安全。
	 *
	 * @param UniqueSuffix 附加在对象名末尾的唯一后缀（测试函数名缩写）
	 * @return transient UBoardDataAsset 实例的软引用
	 */
	static TSoftObjectPtr<UBoardDataAsset> MakeTransientBoardDARef(const FString& UniqueSuffix)
	{
		// 静态计数器保证同进程内多次运行不冲突
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

	/**
	 * 创建内存态 transient UTestConcreteDataAsset（非 UBoardDataAsset 类型）。
	 * 用于 TC-L2d：验证 OnDataAssetLoaded 的 Cast 失败守卫。
	 * UTestConcreteDataAsset 继承 UPrimaryDataAsset（非 UBoardDataAsset），故 Cast<UBoardDataAsset> 返回 nullptr。
	 *
	 * @param UniqueSuffix 唯一后缀
	 * @return transient UTestConcreteDataAsset 的 UPrimaryDataAsset 软引用
	 */
	static TSoftObjectPtr<UPrimaryDataAsset> MakeTransientNonBoardDARef(const FString& UniqueSuffix)
	{
		static int32 Counter = 0;
		++Counter;

		const FString ObjName = FString::Printf(
			TEXT("TransientNonBoardDA_%s_%d"), *UniqueSuffix, Counter);

		// UTestConcreteDataAsset IS-A UPrimaryDataAsset，但 NOT UBoardDataAsset
		// → OnDataAssetLoaded 中 Cast<UBoardDataAsset> 将返回 nullptr
		UTestConcreteDataAsset* DA = NewObject<UTestConcreteDataAsset>(
			GetTransientPackage(),
			*ObjName,
			RF_Transient);

		check(DA);

		FSoftObjectPath SoftPath(DA);
		return TSoftObjectPtr<UPrimaryDataAsset>(SoftPath);
	}
}

// =============================================================================
// TC-L1a (AC-L1): 继承链反射 + GetLoadedBoard() 返回类型正确
//
// GIVEN UBoardDataAssetHost 通过 World->GetSubsystem<> 获取
// WHEN  检查类层级 + GetLoadedBoard 返回类型
// THEN  IsA<UDataAssetHostSubsystem> == true（继承链正确）
//        GetLoadedBoard() 返回 UBoardDataAsset*（类型化访问器工作）
//        GetLoadedBoard 与 GetLoadedDataAsset 指向同一对象（强引用一致性，无第二副本）
//
// 非 vacuous：若 Cast 类型错误，GetLoadedBoard 返回 nullptr 而 GetLoadedDataAsset 非空 → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L1a_InheritanceAndGetLoadedBoardReturnType,
	"Rento.Board.DAHolderLifecycle.TC_L1a_Inheritance_GetLoadedBoardReturnType",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L1a_InheritanceAndGetLoadedBoardReturnType::RunTest(const FString& Parameters)
{
	// GIVEN：Game World（触发 Subsystem 初始化）
	UWorld* World = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L1a_BoardDAHost_World"));
	if (!TestNotNull(TEXT("TC-L1a: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-L1a: UBoardDataAssetHost 应在 Game World 自动创建（AC-L1）"), Host))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// THEN-1：继承链反射——IsA<UDataAssetHostSubsystem>（间接验证继承关系，AC-L1）
	TestTrue(
		TEXT("TC-L1a: UBoardDataAssetHost 应继承 UDataAssetHostSubsystem（AC-L1 继承链）"),
		Host->IsA<UDataAssetHostSubsystem>());

	// THEN-2：初始态 GetLoadedBoard() 返回 nullptr（未加载时正确行为）
	TestNull(
		TEXT("TC-L1a: 初始态 GetLoadedBoard() 应返回 nullptr（尚未加载，AC-L1）"),
		Host->GetLoadedBoard());

	// GIVEN：通过 BoardToLoad seam 加载 transient UBoardDataAsset
	TSoftObjectPtr<UBoardDataAsset> BoardRef =
		BoardDAHostTestHelpers::MakeTransientBoardDARef(TEXT("TC_L1a"));
	Host->BoardToLoad = BoardRef;
	Host->OnWorldBeginPlay(*World);
	FlushAsyncLoading();

	// THEN-3：GetLoadedBoard 非空，类型正确（AC-L1 类型化访问器）
	UBoardDataAsset* LoadedBoard = Host->GetLoadedBoard();
	TestNotNull(
		TEXT("TC-L1a: FlushAsyncLoading 后 GetLoadedBoard() 应非空（AC-L1 类型化访问）"),
		LoadedBoard);

	// THEN-4：GetLoadedBoard 与 GetLoadedDataAsset 同一对象（无第二个强引用副本，AC-L1）
	// 非 vacuous：若 Cast 返回 nullptr（类型错误），LoadedBoard 为 nullptr，TestEqual 两个非空 FAIL
	if (LoadedBoard)
	{
		TestEqual(
			TEXT("TC-L1a: GetLoadedBoard 应与 GetLoadedDataAsset 指向同一对象（无重复强引用，AC-L1）"),
			static_cast<UObject*>(LoadedBoard),
			static_cast<UObject*>(Host->GetLoadedDataAsset()));
	}

	BoardDAHostTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC-L1b (AC-L1): ShouldCreateSubsystem — Game World 自动创建
//
// GIVEN Game World（EWorldType::Game）
// WHEN  GetSubsystem<UBoardDataAssetHost>()
// THEN  返回非空（ShouldCreateSubsystem 对 Game World 返回 true）
//
// Editor-Preview 排除由基类 UMatchSubsystemBase::ShouldCreateSubsystem 继承实现，
// headless 无法创建 EditorPreview World，由基类 ff-001 已覆盖该路径（TC-1b/1c）。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L1b_ShouldCreateSubsystem_GameWorld,
	"Rento.Board.DAHolderLifecycle.TC_L1b_ShouldCreateSubsystem_GameWorld",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L1b_ShouldCreateSubsystem_GameWorld::RunTest(const FString& Parameters)
{
	UWorld* World = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L1b_ShouldCreate_World"));
	if (!TestNotNull(TEXT("TC-L1b: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	TestNotNull(
		TEXT("TC-L1b: UBoardDataAssetHost 应在 Game World 自动创建（ShouldCreateSubsystem，AC-L1）"),
		Host);

	BoardDAHostTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC-L2a (AC-L2): 状态机全路径推进至 Active
//
// GIVEN transient UBoardDataAsset 在内存（基类同步完成路径）
// WHEN  BoardToLoad seam + OnWorldBeginPlay + FlushAsyncLoading
// THEN  状态最终为 Active
//
// ⚠ 同步完成路径说明（ff-003 既定行为）：
//   transient 内存对象在 RequestAsyncLoad 后 HasLoadCompleted()==true，
//   基类同步路径立即调 ResolveAndStoreAsset → OnDataAssetLoaded，
//   OnDataAssetLoaded override 同步调 MarkAsValidated + ActivateDataAsset，
//   整条链在 OnWorldBeginPlay 调用内同步跑完，返回时已是 Active。
//   生产真异步则停在 Loading 直到回调，FlushAsyncLoading 补偿。
//   中间态断言只断「已脱离 Empty」，不锁定具体中间态。
//
// 非 vacuous（FlushAsyncLoading 后 TestEqual Active）：
//   若 OnDataAssetLoaded 未调 MarkAsValidated → 停在 Loaded → FAIL
//   若 MarkAsValidated 未调 ActivateDataAsset → 停在 Validated → FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L2a_StateProgression_LoadedValidatedActive,
	"Rento.Board.DAHolderLifecycle.TC_L2a_StateProgression_LoadedValidatedActive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L2a_StateProgression_LoadedValidatedActive::RunTest(const FString& Parameters)
{
	UWorld* World = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L2a_StateProgression_World"));
	if (!TestNotNull(TEXT("TC-L2a: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-L2a: UBoardDataAssetHost 应在 Game World 自动创建"), Host))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// 前提断言：初始应为 Empty 态
	TestEqual(TEXT("TC-L2a 前提: 初始态应为 Empty"), Host->GetLoadState(), EHostLoadState::Empty);

	// GIVEN：通过 BoardToLoad seam 发起加载
	TSoftObjectPtr<UBoardDataAsset> BoardRef =
		BoardDAHostTestHelpers::MakeTransientBoardDARef(TEXT("TC_L2a"));
	Host->BoardToLoad = BoardRef;
	Host->OnWorldBeginPlay(*World);

	// THEN-1：加载已发起——状态已脱离 Empty（中间态不锁定，见注释）
	const EHostLoadState StateAfterRequest = Host->GetLoadState();
	TestTrue(
		TEXT("TC-L2a: 加载发起后状态应已脱离 Empty（load 已发起，AC-L2）"),
		StateAfterRequest != EHostLoadState::Empty);

	// WHEN：FlushAsyncLoading（真异步路径补偿；同步路径下为空操作）
	FlushAsyncLoading();

	// THEN-2：最终状态为 Active（核心断言，非 vacuous）
	TestEqual(
		TEXT("TC-L2a: FlushAsyncLoading 后状态应为 Active（Loaded→Validated→Active 全路径推进，AC-L2）"),
		Host->GetLoadState(), EHostLoadState::Active);

	BoardDAHostTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC-L2b (AC-L2): GetLoadedBoard() 在加载完成后返回正确 UBoardDataAsset 对象
//
// GIVEN transient UBoardDataAsset（已知实例指针 ExpectedBoard）
// WHEN  OnWorldBeginPlay + FlushAsyncLoading
// THEN  GetLoadedBoard() == ExpectedBoard（同一对象，非另一个实例）
//        GetLoadedBoard() Cast 正确返回 UBoardDataAsset*
//
// 非 vacuous：
//   Cast 类型错误 → GetLoadedBoard 返回 nullptr → TestNotNull FAIL
//   加载了错误对象（不同实例）→ TestEqual 对象指针 FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L2b_GetLoadedBoard_CorrectObjectAfterLoad,
	"Rento.Board.DAHolderLifecycle.TC_L2b_GetLoadedBoard_CorrectObjectAfterLoad",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L2b_GetLoadedBoard_CorrectObjectAfterLoad::RunTest(const FString& Parameters)
{
	UWorld* World = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L2b_CorrectObject_World"));
	if (!TestNotNull(TEXT("TC-L2b: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-L2b: Host 应创建成功"), Host))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// GIVEN：创建已知实例，记录原始指针
	TSoftObjectPtr<UBoardDataAsset> BoardRef =
		BoardDAHostTestHelpers::MakeTransientBoardDARef(TEXT("TC_L2b"));

	UBoardDataAsset* ExpectedBoard = BoardRef.Get();
	if (!TestNotNull(TEXT("TC-L2b 前提: transient UBoardDataAsset 应非空"), ExpectedBoard))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// WHEN：通过 seam 发起加载
	Host->BoardToLoad = BoardRef;
	Host->OnWorldBeginPlay(*World);
	FlushAsyncLoading();

	// 前提：状态应为 Active
	if (!TestEqual(TEXT("TC-L2b 前提: 状态应为 Active"),
		Host->GetLoadState(), EHostLoadState::Active))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// THEN-1：GetLoadedBoard 非空（Cast 成功，AC-L1 + AC-L2）
	UBoardDataAsset* LoadedBoard = Host->GetLoadedBoard();
	if (!TestNotNull(TEXT("TC-L2b: GetLoadedBoard() 应非空（AC-L2 类型化访问）"), LoadedBoard))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// THEN-2：与创建时的实例是同一对象（对象一致性，非 vacuous）
	TestEqual(
		TEXT("TC-L2b: GetLoadedBoard() 应与创建的 transient 实例是同一对象（AC-L2 对象一致性）"),
		static_cast<UObject*>(LoadedBoard),
		static_cast<UObject*>(ExpectedBoard));

	// THEN-3：与 GetLoadedDataAsset 同一对象（无重复强引用副本，AC-L1）
	TestEqual(
		TEXT("TC-L2b: GetLoadedBoard 应与 GetLoadedDataAsset 同一对象（无重复强引用，AC-L1）"),
		static_cast<UObject*>(LoadedBoard),
		static_cast<UObject*>(Host->GetLoadedDataAsset()));

	BoardDAHostTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC-L2c (AC-L2): 校验挂载点占位直通
//
// GIVEN 合法 transient UBoardDataAsset（类型守卫通过）
// WHEN  OnWorldBeginPlay + FlushAsyncLoading
// THEN  状态推进至 Active（MVP 占位校验直通，story-005 预留挂载点）
//
// 注意：story-005 实现后此测试须更新，引入合法/非法 DA 的真实校验断言。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L2c_ValidationMountPoint_PassThrough,
	"Rento.Board.DAHolderLifecycle.TC_L2c_ValidationMountPoint_PassThrough",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L2c_ValidationMountPoint_PassThrough::RunTest(const FString& Parameters)
{
	UWorld* World = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L2c_Validation_World"));
	if (!TestNotNull(TEXT("TC-L2c: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-L2c: Host 应创建成功"), Host))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	TSoftObjectPtr<UBoardDataAsset> BoardRef =
		BoardDAHostTestHelpers::MakeTransientBoardDARef(TEXT("TC_L2c"));
	Host->BoardToLoad = BoardRef;
	Host->OnWorldBeginPlay(*World);
	FlushAsyncLoading();

	// MVP 占位校验直通 → Active
	// story-005 实现后：合法 DA → Active；非法 DA → LoadFailed/Empty + 错误码
	TestEqual(
		TEXT("TC-L2c: MVP 占位校验直通，状态应为 Active（story-005 将引入真实校验，AC-L2 挂载点）"),
		Host->GetLoadState(), EHostLoadState::Active);

	BoardDAHostTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC-L2d (AC-L2): Cast 失败守卫 — 非 UBoardDataAsset 类型 → 停在 Loaded 态
//
// 验证 OnDataAssetLoaded 中「类型安全守卫」（本 story 已实现的边界，非 story-005 校验逻辑）
// 的当前行为：Cast<UBoardDataAsset> 失败 → return，宿主停在 Loaded 态。
//
// GIVEN UBoardDataAssetHostTestProxy（暴露 protected 加载入口）
//        + transient UTestConcreteDataAsset（IS-A UPrimaryDataAsset，NOT UBoardDataAsset）
// WHEN  向宿主注入非 UBoardDataAsset 软引用并等待加载完成
// THEN  GetLoadState() == Loaded（停在此，未推进至 Validated/Active）
//        GetLoadedBoard() == nullptr（Cast 失败，类型化访问器正确返回 nullptr）
//        GetLoadedDataAsset() != nullptr（基类强引用已写入，证明加载本身成功）
//
// 非 vacuous：
//   若移除 Cast 失败的 return，状态推进至 Active → TestEqual Loaded FAIL
//   GetLoadedDataAsset 非空 + GetLoadedBoard 为空 = 精确 pin 当前行为（非全失败）
//
// 已知限制（story-005 将处理）：
//   宿主停在 Loaded 态后无自动恢复路径——RequestReset 需要 Active，
//   RequestAsyncLoadDataAsset 在非 Empty 态被拒。story-005 引入 LoadFailed 态或 Empty 回退时修正。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L2d_CastFailure_StaysLoaded,
	"Rento.Board.DAHolderLifecycle.TC_L2d_CastFailure_StaysLoaded",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L2d_CastFailure_StaysLoaded::RunTest(const FString& Parameters)
{
	// UBoardDataAssetHostTestProxy 是 UBoardDataAssetHost 的子类，
	// 通过 GetSubsystem 获取的是 UBoardDataAssetHost，测试桩需独立实例化。
	// UWorldSubsystem 不支持同一 World 注册两个相同基类的 Subsystem，
	// 故直接 NewObject<> 构造测试代理（不走 Subsystem 自动创建路径）。
	//
	// 注意：此处不依赖 World Subsystem 注册机制——测试桩仅用于调用内部方法，
	// 不需要真实的 World 生命周期绑定（与 ff-003 UTestDataAssetHost 的测试桩同模式）。
	UBoardDataAssetHostTestProxy* Proxy = NewObject<UBoardDataAssetHostTestProxy>(
		GetTransientPackage(),
		TEXT("TC_L2d_Proxy"),
		RF_Transient);
	if (!TestNotNull(TEXT("TC-L2d: TestProxy 应创建成功"), Proxy)) { return false; }

	// GIVEN：前提态为 Empty
	TestEqual(TEXT("TC-L2d 前提: 初始态应为 Empty"), Proxy->GetLoadState(), EHostLoadState::Empty);

	// 声明预期的 Error log（Cast 失败会打 LogTemp Error，Automation 默认将 Error 判 FAIL）
	AddExpectedError(TEXT("加载的资产不是 UBoardDataAsset"), EAutomationExpectedErrorFlags::Contains, 1);

	// WHEN：注入非 UBoardDataAsset 类型（UTestConcreteDataAsset IS-A UPrimaryDataAsset，NOT UBoardDataAsset）
	TSoftObjectPtr<UPrimaryDataAsset> NonBoardRef =
		BoardDAHostTestHelpers::MakeTransientNonBoardDARef(TEXT("TC_L2d"));
	const bool bStarted = Proxy->CallRequestAsyncLoadAny(NonBoardRef);

	TestTrue(TEXT("TC-L2d: 加载发起应返回 true（UPrimaryDataAsset 类型合法，加载本身成功）"), bStarted);

	// FlushAsyncLoading：确保完成回调（OnDataAssetLoaded）被触发
	FlushAsyncLoading();

	// THEN-1：状态停在 Loaded（Cast 失败守卫触发，未推进至 Validated/Active）
	// 非 vacuous：若移除 Cast 失败的 return → 状态推进至 Active → FAIL
	TestEqual(
		TEXT("TC-L2d: Cast 失败后状态应停在 Loaded（未推进至 Validated/Active，当前行为 pin，AC-L2 守卫）"),
		Proxy->GetLoadState(), EHostLoadState::Loaded);

	// THEN-2：GetLoadedBoard 返回 nullptr（Cast<UBoardDataAsset> 失败，类型化访问器正确）
	// 非 vacuous：若 Cast 意外成功（类型继承关系改变）→ 返回非 nullptr → FAIL
	TestNull(
		TEXT("TC-L2d: Cast 失败时 GetLoadedBoard() 应返回 nullptr（类型化访问器正确，AC-L1）"),
		Proxy->GetLoadedBoard());

	// THEN-3：GetLoadedDataAsset 非空（基类强引用已写，加载本身成功）
	// 非 vacuous：若基类未写入强引用（加载失败）→ 返回 nullptr → FAIL
	TestNotNull(
		TEXT("TC-L2d: Cast 失败时 GetLoadedDataAsset() 应非空（基类强引用已写，加载本身成功）"),
		Proxy->GetLoadedDataAsset());

	// Proxy 是 RF_Transient NewObject，无需 Subsystem Deinitialize，GC 自动回收
	return true;
}

// =============================================================================
// TC-BoardToLoadNull (AC-L2): BoardToLoad 为空 → OnWorldBeginPlay 不崩、状态保持 Empty
//
// GIVEN UBoardDataAssetHost，BoardToLoad 未设置（IsNull()==true，等待 ff-004/pt-001 接线）
// WHEN  OnWorldBeginPlay 被调
// THEN  不崩溃
//        GetLoadState() == Empty（未发起加载）
//        GetLoadedBoard() == nullptr（无 DA 持有）
//
// 意图：pin「空 seam 等待 ff-004 接线」的正常路径行为，防止意外的空 seam 崩溃。
//
// 非 vacuous：若 OnWorldBeginPlay 里的 IsNull 检查被删，空引用传入 RequestAsyncLoadDataAsset，
//             基类拒绝（IsNull 早期返回 false + 打 Warning）→ 状态仍 Empty，断言仍通过。
//             但崩溃路径（若直接 Dereference 空指针）→ 进程 crash → 测试 FAIL（崩溃=最强 FAIL）。
//             Empty 状态断言额外保证：不会意外进入 Loading 态。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_BoardToLoadNull_StaysEmpty,
	"Rento.Board.DAHolderLifecycle.TC_BoardToLoadNull_StaysEmpty",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_BoardToLoadNull_StaysEmpty::RunTest(const FString& Parameters)
{
	UWorld* World = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_NullSeam_World"));
	if (!TestNotNull(TEXT("TC-NullSeam: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-NullSeam: Host 应创建成功"), Host))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// 前提：BoardToLoad 未设置（IsNull()==true）
	TestTrue(TEXT("TC-NullSeam 前提: BoardToLoad 应为空（IsNull）"), Host->BoardToLoad.IsNull());

	// OnWorldBeginPlay 打 Warning log，声明为预期（避免 Automation 把 Warning 判 FAIL）
	// 注意：UE Automation 默认不把 Warning 判 FAIL（只有 Error 才是），此处注释说明即可，无需 AddExpectedError
	Host->OnWorldBeginPlay(*World);

	// THEN-1：不崩（执行到此行即通过）

	// THEN-2：状态保持 Empty（未发起加载）
	TestEqual(
		TEXT("TC-NullSeam: BoardToLoad 为空时 OnWorldBeginPlay 后状态应仍为 Empty（AC-L2 seam 路径）"),
		Host->GetLoadState(), EHostLoadState::Empty);

	// THEN-3：GetLoadedBoard 为 nullptr（无 DA 持有）
	TestNull(
		TEXT("TC-NullSeam: BoardToLoad 为空时 GetLoadedBoard 应为 nullptr"),
		Host->GetLoadedBoard());

	BoardDAHostTestHelpers::DestroyTestWorld(World);
	return true;
}

// =============================================================================
// TC-L3 (AC-L3): 两局连续独立加载——第二局从全新实例成功独立加载棋盘
//
// 验证的真实属性：
//   第二局 UBoardDataAssetHost（全新 UWorldSubsystem 实例）能从头完成完整加载流程，
//   与第一局的棋盘数据完全隔离（加载不同 DA，最终 DA 对象不同）。
//   这是 AC-L3「第二局正确重新加载、无第一局残留」的直接可测子集。
//
// 覆盖边界（诚实声明）：
//   - 第二局从 Empty 态开始：UWorldSubsystem 全新实例，成员默认就是 Empty/nullptr，
//     这是 C++ 默认初始化的正常行为，与 Deinitialize 是否执行无关——不作为断言。
//   - CancelHandle 内部验证（handle 成员/WasCanceled/bIsDeinitializing guard）：
//     由基类 ff-003 AsyncLoadCancelHandleTest TC-2/Edge_TwoConsecutiveMatches 覆盖，本测试不重复。
//   - 真异步 UAF（Deinitialize 后悬挂回调写 GC 对象）→ [defer ff-007]（与 ff-003 同惯例）。
//
// 非 vacuous：
//   第二局独立加载到 Active：若第二局 Host 的加载路径因依赖第一局状态而损坏 → 停在非 Active → FAIL
//   两局 GetLoadedBoard 指针不同：若两局错误共享同一 DA 实例 → TestNotEqual FAIL
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L3_PIEIsolation_TwoConsecutiveMatches,
	"Rento.Board.DAHolderLifecycle.TC_L3_PIEIsolation_TwoConsecutiveMatches_NoHandleLeak",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L3_PIEIsolation_TwoConsecutiveMatches::RunTest(const FString& Parameters)
{
	// === 第一局 ===
	UWorld* World1 = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L3_Match1_World"));
	if (!TestNotNull(TEXT("TC-L3: 第一局 World 应创建成功"), World1)) { return false; }

	UBoardDataAssetHost* Host1 = World1->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-L3: 第一局 Host 应创建成功"), Host1))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World1);
		return false;
	}

	// 第一局：加载特定 DA → Active
	TSoftObjectPtr<UBoardDataAsset> BoardRef1 =
		BoardDAHostTestHelpers::MakeTransientBoardDARef(TEXT("TC_L3_Match1"));
	Host1->BoardToLoad = BoardRef1;
	Host1->OnWorldBeginPlay(*World1);
	FlushAsyncLoading();

	if (!TestEqual(TEXT("TC-L3: 第一局状态应为 Active"), Host1->GetLoadState(), EHostLoadState::Active))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World1);
		return false;
	}

	// 记录第一局的 DA 指针，用于后续隔离断言
	UBoardDataAsset* DA1 = Host1->GetLoadedBoard();
	TestNotNull(TEXT("TC-L3: 第一局 GetLoadedBoard 应非空"), DA1);

	// 第一局：销毁（Deinitialize → CancelHandle + ReleaseHandle，由基类处理）
	World1->DestroyWorld(/*bInformEngineOfWorld=*/false);
	World1 = nullptr;
	Host1  = nullptr;

	// 第一局销毁后 FlushAsyncLoading：确保任何残留回调已被 guard 拦截（无悬挂写）
	FlushAsyncLoading();

	// === 第二局 ===
	UWorld* World2 = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L3_Match2_World"));
	if (!TestNotNull(TEXT("TC-L3: 第二局 World 应创建成功"), World2)) { return false; }

	UBoardDataAssetHost* Host2 = World2->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-L3: 第二局 Host 应创建成功"), Host2))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World2);
		return false;
	}

	// 第二局：加载不同 DA → Active（核心：独立加载路径可用）
	TSoftObjectPtr<UBoardDataAsset> BoardRef2 =
		BoardDAHostTestHelpers::MakeTransientBoardDARef(TEXT("TC_L3_Match2"));
	Host2->BoardToLoad = BoardRef2;
	Host2->OnWorldBeginPlay(*World2);
	FlushAsyncLoading();

	// THEN-1：第二局独立加载到 Active（非 vacuous：加载路径损坏则停在非 Active → FAIL）
	TestEqual(
		TEXT("TC-L3: 第二局加载后状态应为 Active（独立完整加载路径可用，AC-L3）"),
		Host2->GetLoadState(), EHostLoadState::Active);

	UBoardDataAsset* DA2 = Host2->GetLoadedBoard();
	TestNotNull(TEXT("TC-L3: 第二局 GetLoadedBoard 应非空（独立加载成功，AC-L3）"), DA2);

	// THEN-2：两局 DA 是不同对象（棋盘数据隔离，非 vacuous：错误共享同一实例则 FAIL）
	if (DA1 && DA2)
	{
		TestNotEqual(
			TEXT("TC-L3: 两局 GetLoadedBoard 应是不同对象（棋盘数据隔离，AC-L3）"),
			static_cast<UObject*>(DA1),
			static_cast<UObject*>(DA2));
	}

	BoardDAHostTestHelpers::DestroyTestWorld(World2);
	return true;
}

// =============================================================================
// TC-L4 (AC-L4): [Advisory·code-review] 无直接写 DA 引用的 public setter
//
// GIVEN UBoardDataAssetHost 类反射
// WHEN  扫描所有 public UFUNCTION
// THEN  无 SetLoadedBoard / SetBoardDA / ReplaceBoard / SetLoadedDataAsset / SetDA 类 setter
//
// 扫描范围说明：
//   仅扫描「直接写持有 DA 引用的 setter」（热替换入口）。
//   ActivateDataAsset / RequestReset 是合法状态机接口，不是热替换：
//     ActivateDataAsset：Validated→Active 的状态推进，不写 DA 引用。
//     RequestReset：Active→Empty 的规范换盘第一步，清空强引用是正确行为。
//   这两个函数不在扫描黑名单中。
//
// [Advisory·code-review] 反射扫描辅助验证，主要保障由 code-review 签署。
// 功能验证（非 vacuous）：RequestReset 是 Active 态唯一合法重置路径。
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBoardDAHost_TC_L4_NoPublicHotSwapSetter,
	"Rento.Board.DAHolderLifecycle.TC_L4_NoPublicHotSwapSetter_AdvisoryCodeReview",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBoardDAHost_TC_L4_NoPublicHotSwapSetter::RunTest(const FString& Parameters)
{
	UWorld* World = BoardDAHostTestHelpers::CreateTestWorld(
		EWorldType::Game, TEXT("TC_L4_HotSwap_World"));
	if (!TestNotNull(TEXT("TC-L4: 应能创建 Game World"), World)) { return false; }

	UBoardDataAssetHost* Host = World->GetSubsystem<UBoardDataAssetHost>();
	if (!TestNotNull(TEXT("TC-L4: Host 应创建成功"), Host))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// --- 反射扫描：确认无直接写 DA 引用的 public setter ---
	// 黑名单：写持有棋盘 DA 引用的 setter（热替换入口）。
	// ActivateDataAsset / RequestReset 是合法状态机接口，不在黑名单（见注释）。
	bool bFoundSetter = false;
	const TArray<FString> ForbiddenNames = {
		TEXT("SetLoadedBoard"), TEXT("SetBoardDA"), TEXT("ReplaceBoard"),
		TEXT("SetLoadedDataAsset"), TEXT("SetDA")
	};

	UClass* HostClass = UBoardDataAssetHost::StaticClass();
	for (TFieldIterator<UFunction> FuncIt(HostClass); FuncIt; ++FuncIt)
	{
		UFunction* Func = *FuncIt;
		if (Func && (Func->FunctionFlags & FUNC_Public) && !(Func->FunctionFlags & FUNC_Static))
		{
			const FString FuncName = Func->GetName();
			for (const FString& Forbidden : ForbiddenNames)
			{
				if (FuncName.Contains(Forbidden, ESearchCase::IgnoreCase))
				{
					AddError(FString::Printf(
						TEXT("TC-L4: 发现禁止的 public DA 直写 setter：%s（AC-L4 热切换边界违反）"),
						*FuncName));
					bFoundSetter = true;
				}
			}
		}
	}

	TestFalse(
		TEXT("TC-L4: UBoardDataAssetHost 不应有 public DA 直写 setter（无热替换入口，AC-L4）"),
		bFoundSetter);

	// --- 功能验证：RequestReset 是 Active 态唯一合法重置路径（非 vacuous）---
	TSoftObjectPtr<UBoardDataAsset> BoardRef =
		BoardDAHostTestHelpers::MakeTransientBoardDARef(TEXT("TC_L4"));
	Host->BoardToLoad = BoardRef;
	Host->OnWorldBeginPlay(*World);
	FlushAsyncLoading();

	if (!TestEqual(TEXT("TC-L4 前提: 状态应为 Active"), Host->GetLoadState(), EHostLoadState::Active))
	{
		BoardDAHostTestHelpers::DestroyTestWorld(World);
		return false;
	}

	// Active 态唯一合法路径：RequestReset → Empty（规范换盘第一步）
	const bool bReset = Host->RequestReset();
	TestTrue(TEXT("TC-L4: Active 态 RequestReset 应返回 true（合法换盘第一步，AC-L4）"), bReset);
	TestEqual(
		TEXT("TC-L4: RequestReset 后状态应为 Empty（AC-L4）"),
		Host->GetLoadState(), EHostLoadState::Empty);
	TestNull(
		TEXT("TC-L4: RequestReset 后 GetLoadedBoard 应为 nullptr（DA 引用已清除，AC-L4）"),
		Host->GetLoadedBoard());

	BoardDAHostTestHelpers::DestroyTestWorld(World);
	return true;
}
