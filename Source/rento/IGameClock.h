// IGameClock.h
// =============================================================================
// IGameClock — 游戏时钟 DI 抽象接口（ADR-0001 §Key Interfaces / ADR-0008 §Key Interfaces）
//
// 设计决策（路径 A，2026-06-07 msc 裁定）：
//   - 纯 C++ 接口，非 UObject / 非 UINTERFACE —— 不污染 GC，不挂 UPROPERTY
//   - 宿主以 TSharedPtr<IGameClock> 持有（AC-6 生命周期安全；非裸指针）
//   - 不声明 UFUNCTION / BlueprintCallable —— 内部基础设施，不做 BP 暴露
//   - FWorldGameClock 与 IGameClock 同头文件（避免头文件碎片化，接口+生产实现一对一）
//
// 规范依据：
//   - ADR-0001 §Key Interfaces L177（IGameClock 伪码）
//   - ADR-0008 §Key Interfaces L209-235（IGameClock/FWorldGameClock 设计）
//   - story-002 AC-1（NowSeconds 返回 double）、AC-2（FWorldGameClock 读 World time）、
//     AC-6（非裸指针，生命周期安全）
// =============================================================================
#pragma once

#include "CoreMinimal.h"

// =============================================================================
// IGameClock — 时钟 DI 接口（AC-1）
// =============================================================================

/**
 * IGameClock — 游戏时钟抽象接口
 *
 * 提供 DI 注入点：生产时用 FWorldGameClock，headless 测试时用 FMockGameClock。
 * 设计为纯 C++ 接口（非 UObject），由 TSharedPtr<IGameClock> 持有以保证生命周期安全。
 *
 * NowSeconds() 语义：返回游戏运行时间（秒）。
 * 生产实现委托给 UWorld::GetTimeSeconds()；测试实现返回受控的可设置值。
 */
class IGameClock
{
public:
    virtual ~IGameClock() = default;

    /**
     * 返回当前游戏时间（秒）。
     * 生产实现：读取 UWorld::GetTimeSeconds()（随游戏进程单调递增）。
     * 测试实现：返回由 fixture 显式推进的受控值，与真实世界时间无关。
     *
     * @return 当前时间（秒，double 精度）
     */
    virtual double NowSeconds() const = 0;
};

// =============================================================================
// FWorldGameClock — 生产时钟实现（AC-2）
// =============================================================================

/**
 * FWorldGameClock — 读取 UWorld 权威时间的生产实现
 *
 * 持 TWeakObjectPtr<UWorld> 以避免裸 UObject 指针悬挂（ADR-0008 §Implementation Notes）。
 * World 失效时安全兜底返回 0.0，不崩溃。
 *
 * 生命周期：由 UMatchSubsystemBase::Initialize 以 MakeShared<FWorldGameClock> 构建，
 * 存入 TSharedPtr<IGameClock> GameClock，随宿主 Deinitialize 自动释放（shared ownership）。
 */
class FWorldGameClock final : public IGameClock
{
public:
    /**
     * 构造函数：接受 World 弱引用
     * @param InWorld 当前对局 UWorld（Initialize 时传入，不拥有所有权）
     */
    explicit FWorldGameClock(UWorld* InWorld)
        : World(InWorld)
    {
    }

    /**
     * 返回 UWorld::GetTimeSeconds()（游戏时钟，随游戏进程单调递增）。
     * World 无效时返回 0.0（安全兜底）。
     */
    virtual double NowSeconds() const override
    {
        // 解引用弱指针：IsValid() + Get() 是 TWeakObjectPtr 的标准用法
        const UWorld* PinnedWorld = World.Get();
        return PinnedWorld ? static_cast<double>(PinnedWorld->GetTimeSeconds()) : 0.0;
    }

private:
    /** World 弱引用：避免持有 UObject 强引用（防止 GC 无法释放 World） */
    TWeakObjectPtr<UWorld> World;
};
