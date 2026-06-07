// FMockGameClock.h
// =============================================================================
// FMockGameClock — 测试专用受控时钟（story-002 AC-3）
//
// 仅在 rentoTests 模块内使用（test-only，不进 Shipping）。
// 实现 IGameClock 接口，NowSeconds() 返回由 fixture 显式设置/推进的受控值，
// 与真实世界时间、UWorld 状态完全无关——保证测试确定性。
//
// 设计：
//   - SetNow(double)：直接设置当前时间值（用于精确控制断言时刻）
//   - Advance(double)：在当前值基础上前进 Dt（用于模拟时间流逝）
//   - NowSeconds()：返回受控的 SimTime（非 UWorld::GetTimeSeconds()）
//   - 支持"回退"设置（SetNow 可设小于当前值）——用于测重定向场景（Edge case）
//
// 规范依据：story-002 AC-3、TC-2（受控推进）、TC-3（headless DI 验证）
// =============================================================================
#pragma once

#include "IGameClock.h"

/**
 * FMockGameClock — headless 测试用受控时钟
 *
 * fixture 通过 SetNow / Advance 精确控制时间，
 * 被测系统经 GetClockNowSeconds（宿主 DI 读取面）读到对应受控值，
 * 而非真实世界时间。
 */
class FMockGameClock final : public IGameClock
{
public:
    FMockGameClock() = default;

    /**
     * 直接设置当前模拟时间（秒）。
     * 可设小于当前值（支持回退，用于测重定向等场景）。
     * @param InTime 目标时间值（秒）
     */
    void SetNow(double InTime)
    {
        SimTime = InTime;
    }

    /**
     * 在当前时间基础上前进 Dt 秒。
     * 等价于 SetNow(SimTime + Dt)。
     * @param Dt 推进量（秒，正数向前，负数向后）
     */
    void Advance(double Dt)
    {
        SimTime += Dt;
    }

    /**
     * 返回受控的模拟时间（秒）。
     * 与 UWorld 状态、真实流逝时间完全无关——保证测试确定性。
     */
    virtual double NowSeconds() const override
    {
        return SimTime;
    }

private:
    /** 受控的模拟时间（初值 0.0） */
    double SimTime = 0.0;
};
