// MatchSubsystemBase.cpp
// =============================================================================
// per-match 宿主基类实现（ADR-0001 四 override 骨架 + story-002 IGameClock DI）
// 规范依据：ADR-0001、ADR-0008、control-manifest Foundation Layer §宿主与生命周期
//           story-002 AC-4（DI 注入点）、AC-5（headless mock 可注入）、
//                     AC-6（TSharedPtr 非裸指针，路径 A 2026-06-07）
// =============================================================================
#include "MatchSubsystemBase.h"
#include "Engine/World.h"

// ----------------------------------------------------------------------------
// ShouldCreateSubsystem
// ----------------------------------------------------------------------------

bool UMatchSubsystemBase::ShouldCreateSubsystem(UObject* Outer) const
{
	// 第一步：保留基类检查（CDO 检测、Abstract 类过滤等）
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// 第二步：WorldType 白名单过滤（ADR-0001 §1）
	// 引擎基类 DoesSupportWorldType 默认允许 Game / Editor / PIE，
	// 本实现收紧为仅 Game（独立运行）和 PIE（编辑器内运行），
	// 显式排除 Editor / EditorPreview / Inactive / GamePreview / GameRPC。
	//
	// 用 Cast<> 而非 CastChecked<>：对非常规 Outer（如插件热重载/测试注入等边界场景）
	// 返回 false 安全拒绝而不 check() 崩进程（依赖根基类，每 Subsystem 继承，防御优先）
	const UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}
	const EWorldType::Type WorldType = World->WorldType;
	return (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------

void UMatchSubsystemBase::Initialize(FSubsystemCollectionBase& Collection)
{
	// 调用基类（设置引擎内部初始化状态，处理 PostInitialize 补调逻辑）
	Super::Initialize(Collection);

	// story-002 AC-4：建立 IGameClock DI 注入点，默认注入生产时钟（FWorldGameClock）。
	// headless fixture 可在 Initialize 返回后调 SetGameClock 替换为 FMockGameClock。
	//
	// GetWorld() 在 Initialize 时可用（WorldSubsystem 的 Outer 即 World）。
	// 使用 TWeakObjectPtr<UWorld> 持有（FWorldGameClock 内部），不阻止 GC 释放 World。
	GameClock = MakeShared<FWorldGameClock>(GetWorld());

	// DI 注入点骨架：派生类在此声明 Subsystem 依赖并获取引用
	//
	// 约束（ADR-0001）：
	//   - 此时 World 尚未 BeginPlay，禁读玩家态
	//   - Seed 注入时机不在此处（见 OnWorldBeginPlay）
	//   - FStreamableManager::RequestAsyncLoad 在此发起，handle 须在 Deinitialize 中 CancelHandle
	//     （story-003 实现，本基类骨架不含）
}

// ----------------------------------------------------------------------------
// IGameClock DI 注入面（story-002 AC-4 / AC-5 / AC-6）
// ----------------------------------------------------------------------------

void UMatchSubsystemBase::SetGameClock(TSharedPtr<IGameClock> InClock)
{
	// DI 注入：替换时钟实现（headless fixture 注入 FMockGameClock，生产无需调用）
	GameClock = MoveTemp(InClock);
}

TSharedPtr<IGameClock> UMatchSubsystemBase::GetGameClock() const
{
	// 返回 TSharedPtr<IGameClock>——类型本身即 TC-4 AC-6 的静态证据（非裸指针）
	return GameClock;
}

double UMatchSubsystemBase::GetClockNowSeconds() const
{
	// Edge case 兜底：时钟未注入时记 Error 日志并安全返回 0.0（不崩溃）
	// 注意：headless 测试中 AddExpectedError 可吞此 UE_LOG，不影响测试绿灯
	if (!GameClock.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("UMatchSubsystemBase::GetClockNowSeconds: GameClock 未注入，返回 0.0。"
			     "请确认 Initialize 已调用，或 headless fixture 已正确注入时钟。"));
		return 0.0;
	}

	// 经注入时钟读时间——此处是 DI 生效的关键路径。
	// 若实现直读 GetWorld()->GetTimeSeconds() 而绕过此分支，TC-3 会因 mock 值不匹配真实 World 时间而 FAIL。
	return GameClock->NowSeconds();
}

// ----------------------------------------------------------------------------
// OnWorldBeginPlay
// ----------------------------------------------------------------------------

void UMatchSubsystemBase::OnWorldBeginPlay(UWorld& InWorld)
{
	// 调用基类（设置 bHasCalledBeginPlay 标志，防重复触发 ensure）
	Super::OnWorldBeginPlay(InWorld);

	// 确定性起点骨架：派生类在此注入 RNG Seed（ADR-0001 §2 / ADR-0004）
	// 示例（由 story-007 / dice epic 填入）：
	//   DiceService->SetSeed(GameSetupConfig.Seed);
	//
	// 本基类不注入任何具体 Seed 或广播（story-001 Out of Scope）
}

// ----------------------------------------------------------------------------
// Deinitialize
// ----------------------------------------------------------------------------

void UMatchSubsystemBase::Deinitialize()
{
	// 派生类在调用 Super 前应先完成：
	//   1. FStreamableHandle->CancelHandle()（防 PIE Stop 后回调写已 GC 对象 — ADR-0001 §3）
	//      story-003 实现，本骨架不含
	//   2. 解绑所有在 Initialize 中绑定的 delegate（PIE Stop 不留野绑定 — ADR-0001/0003）

	// R-3（ADR-0001 §3 显式清理）：
	// 在 Super 之前 Reset GameClock，明确 Deinitialize 是 GameClock 生命周期终点。
	// TSharedPtr::Reset() 释放本宿主对时钟对象的引用计数。
	// FWorldGameClock 内部持 TWeakObjectPtr<UWorld>（非强引用），World 被 GC 时无悬垂风险；
	// 此处 Reset 是防御性清理 + 可读性：让代码自说明 GameClock 不跨越 Deinitialize 存活。
	GameClock.Reset();

	// 调用基类（完成引擎内部清理）
	Super::Deinitialize();
}
