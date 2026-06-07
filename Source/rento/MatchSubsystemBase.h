// MatchSubsystemBase.h
// =============================================================================
// per-match 宿主基类（ADR-0001 §Key Interfaces 伪代码实现）
//
// 所有对局期服务（回合状态机 / 经济 / 地产 / RNG / 棋盘 DA 持有者）
// 须继承本基类或直接继承 UWorldSubsystem，生命周期边界 = 一局（World 边界）。
//
// 规范依据：
//   - ADR-0001（primary）— UObject 宿主与生命周期边界
//   - ADR-0008（IGameClock DI 注入点）— story-002 扩展
//   - control-manifest Foundation Layer §宿主与生命周期 (ADR-0001)
//   - story-001 AC-1/AC-2/AC-5/AC-6
//   - story-002 AC-4（IGameClock DI 注入点）、AC-5（headless 注入 mock）、
//              AC-6（TSharedPtr 非裸指针，路径 A 裁定 2026-06-07）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "IGameClock.h"
#include "MatchSubsystemBase.generated.h"

/**
 * UMatchSubsystemBase — per-match 宿主基类
 *
 * 固化 ADR-0001 规定的四个 override 骨架，作为全部对局期 World Subsystem 的公共基类：
 *
 *   1. ShouldCreateSubsystem —— 仅游戏局 World（Game / PIE）返回 true，
 *                               排除 Editor / EditorPreview / Inactive 等非对局 World，
 *                               防止编辑器误建对局态（ADR-0001 §1）
 *   2. Initialize            —— 取得依赖 Subsystem、建立 IGameClock DI 注入点骨架；
 *                               此时 World 尚未 BeginPlay，禁读玩家态（ADR-0001 §Initialize 注释）
 *   3. OnWorldBeginPlay      —— 确定性起点钩子；
 *                               Seed 注入由派生类 / story-002/007 在此时机完成，
 *                               本基类骨架不注入任何具体 Seed 或广播（Out of Scope story-001）
 *   4. Deinitialize          —— 解绑本宿主持有的 delegate、取消异步加载 handle；
 *                               具体 FStreamableHandle::CancelHandle 实现归 story-003（ADR-0001 §3）
 *
 * 约束（ADR-0001 + control-manifest 硬规则）：
 *   - 不声明任何游戏状态写字段（Cash / house_count / owner map 等）
 *   - 不使用 UActorComponent 形态（ADR-0001 §6 Forbidden）
 *   - UObject 引用成员须用 TObjectPtr<T> + UPROPERTY() 防 GC（本基类暂无服务引用占位，派生类遵守）
 *   - Full Vision 联网迁移预留：派生类状态字段用普通 UPROPERTY，命名对齐 PlayerState / GameState（ADR-0001 §6）
 *
 * 解锁：story-002（IGameClock DI）、story-003（异步加载纪律）、
 *       story-005（Event Bus 订阅锚点）、story-007（PIE 隔离验证）
 */
UCLASS(Abstract)
class RENTO_API UMatchSubsystemBase : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 仅当 Outer World 为游戏局（EWorldType::Game 独立运行 / EWorldType::PIE 编辑器内运行）
	 * 时返回 true，排除 Editor / EditorPreview / Inactive / GamePreview / GameRPC 等非对局 World。
	 *
	 * 实现策略：先调 Super::ShouldCreateSubsystem（保留 CDO 检查等基类逻辑），
	 * 再对 WorldType 做白名单过滤（只允许 Game + PIE）。
	 * 注意：基类 DoesSupportWorldType 默认允许 Game/Editor/PIE，
	 * 本重写收紧为 Game+PIE（排除 Editor），以满足 ADR-0001 "排除 editor-preview World" 要求。
	 *
	 * @param Outer 当前 UWorld 对象（由引擎传入）
	 * @return true 表示应在该 World 创建本 Subsystem
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * 宿主初始化入口：取得依赖 Subsystem、建立 IGameClock DI 注入点。
	 *
	 * story-002（AC-4）：本方法在此建立 IGameClock 默认生产实现（FWorldGameClock）。
	 * headless fixture 可在 Initialize 调用后，通过 SetGameClock 替换为 FMockGameClock。
	 *
	 * 约束（ADR-0001 §Initialize）：
	 *   - 此时 World 尚未 BeginPlay，禁读玩家态（玩家配置由 StartNewGame 落地在 BeginPlay 后）
	 *   - Seed 注入时机不在此处，见 story-007 / ADR-0004
	 *   - 异步加载（FStreamableManager::RequestAsyncLoad）在此发起，handle 存为成员，
	 *     Deinitialize 须显式 CancelHandle（story-003 实现，本骨架不含）
	 *
	 * @param Collection 用于声明对其他 Subsystem 的依赖（InitializeDependency<T>()）
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * 确定性起点钩子，在 World BeginPlay 后由引擎调用。
	 *
	 * 派生类在此处：
	 *   - 注入 RNG Seed（ADR-0001 §2 / ADR-0004 §Seed 注入唯一时机）
	 *   - 发起首帧逻辑（首个 OnTurnStarted 由回合2 在此后驱动）
	 *
	 * 本基类骨架只调 Super（设置引擎内部 bHasCalledBeginPlay 标志），
	 * 不注入任何具体 Seed / 广播（story-001 Out of Scope）。
	 *
	 * @param InWorld 已开始游戏的 World 引用
	 */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * 宿主销毁入口：解绑本宿主持有的 delegate、取消异步加载 handle。
	 *
	 * 派生类在此处：
	 *   - 调用 FStreamableHandle->CancelHandle()（防 PIE Stop 后回调写已 GC 棋盘 — ADR-0001 §3）
	 *   - 解绑所有在 Initialize 中绑定的 delegate（PIE Stop 不留野绑定 — ADR-0001/0003）
	 *
	 * 具体 CancelHandle 实现归 story-003；本基类骨架先调 Super 再留扩展点。
	 */
	virtual void Deinitialize() override;

	// =========================================================================
	// IGameClock DI 注入面（story-002 AC-4 / AC-5 / AC-6）
	// =========================================================================

	/**
	 * 注入时钟实现（DI 注入面）。
	 *
	 * headless fixture 在 Initialize 后调此方法注入 FMockGameClock，替换默认生产时钟。
	 * 生产代码无需调用（Initialize 已建默认 FWorldGameClock）。
	 *
	 * @param InClock TSharedPtr 持有的时钟实现（非裸指针，AC-6 路径 A）
	 */
	void SetGameClock(TSharedPtr<IGameClock> InClock);

	/**
	 * 读取当前注入的时钟实例（供派生类 / 测试读取注入类型）。
	 *
	 * 返回类型为 TSharedPtr<IGameClock>，静态可证为非裸指针（TC-4 AC-6 验证点）。
	 * 未注入时返回空 TSharedPtr（nullptr 语义）。
	 */
	TSharedPtr<IGameClock> GetGameClock() const;

	/**
	 * 经注入时钟读取当前时间（秒）。
	 *
	 * 依赖方经此函数读取时间，而非直读 GetWorld()->GetTimeSeconds()。
	 * 这保证测试时注入 FMockGameClock 后，依赖方读到受控值而非真实世界时间（AC-5 DI 生效）。
	 *
	 * 安全兜底（Edge case）：时钟未注入时记 Error 日志并返回 0.0，不崩溃。
	 *
	 * @return 当前游戏时间（秒）；未注入时返回 0.0
	 */
	double GetClockNowSeconds() const;

private:
	// =========================================================================
	// 内部成员
	// =========================================================================

	/**
	 * 时钟 DI 引用（story-002 AC-4 / AC-6）。
	 *
	 * 关键设计约束：
	 *   1. 故意不加 UPROPERTY —— 纯 C++ 接口非 UObject，GC 不需要追踪
	 *   2. 不加 UPROPERTY 保证 FF-001 TC-4「UMatchSubsystemBase 本类 UPROPERTY 总数 == 0」
	 *      零成员不变式不被回归打破（ADR-0001 / story-001 AC-5 结构断言）
	 *   3. TSharedPtr（非裸指针）满足 AC-6「生命周期安全」的真意图；
	 *      路径 A 裁定（msc 2026-06-07）：TSharedPtr 替代 TScriptInterface/TWeakObjectPtr
	 */
	TSharedPtr<IGameClock> GameClock;
};
