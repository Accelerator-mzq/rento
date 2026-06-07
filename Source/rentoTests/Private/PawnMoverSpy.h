// PawnMoverSpy.h
// =============================================================================
// FPawnMoverSpy — IPawnMover 测试 spy（story pt-006 AC-3 / AC-47 TC-3）
//
// 用途：
//   AC-47 要求断言「本系统发起第二程 Advance 时 bInLandedCallback==false」——
//   即多程位移严格串行、无同步嵌套重入。本 spy 同时扮演：
//     ① 移动 seam（IPawnMover::Advance/TeleportTo）；
//     ② 落地回调监听者（在 Advance 内同步进入「落地回调窗口」，
//        置 bInLandedCallback=true→调 Sub->HandlePawnLanded→出口置 false）。
//
// 非重入验证机制：
//   - Advance 入口先捕获当前 bInLandedCallback 到 FlagAtAdvanceIssue（发起时刻 flag）；
//   - 然后模拟落地回调窗口：置 bInLandedCallback=true，调 Sub->HandlePawnLanded，
//     若 bRequestSecondLeg 且为第一程，则在回调窗口内调 Sub->OrchestrateMove 请求第二程
//     （应被回合2 的 bInOrchestration guard 排队，不在本回调栈内递归发起）；
//   - 出口置 bInLandedCallback=false。
//   回合2 蹦床在第一程 Advance 完全返回后才发起第二程 Advance，
//   此时 bInLandedCallback 已为 false → FlagAtAdvanceIssue[1]==false（AC-47）。
//
//   变异自检：若回合2 在回调内同步递归发起第二程，第二程 Advance 发起时
//   bInLandedCallback==true → FlagAtAdvanceIssue[1]==true → TC-3 断言 FAIL（非 vacuous）。
//
// 机制：
//   纯 C++ struct（非 UObject）——IPawnMover 是纯 C++ 接口，不需要 UHT/UCLASS。
//   TSharedPtr<IPawnMover> 注入 UPlayerTurnSubsystem（DI 接缝）。
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
//
// 规范依据：
//   - story pt-006 AC-3 / AC-47（程间非重入）
//   - PawnMovementInterface.h（IPawnMover 接缝）
//   - LandingResolverSpy.h（相同纯 C++ DI spy 模式，参照）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "PawnMovementInterface.h"
#include "PlayerTurnTypes.h"        // EArrivalContext
#include "PlayerTurnSubsystem.h"    // UPlayerTurnSubsystem::HandlePawnLanded / OrchestrateMove

// =============================================================================
// FPawnMoverSpy — 模拟移动 + 落地回调窗口（测试专用）
//
// 用法（TC-3 程间非重入）：
// @code
//   TSharedPtr<FPawnMoverSpy> Mover = MakeShared<FPawnMoverSpy>();
//   Mover->Sub = Sub;
//   Mover->bRequestSecondLeg = true;  // 第一程落地回调内请求第二程
//   Mover->SecondLegSteps = 3;
//   Sub->SetPawnMover(Mover);
//   Sub->OrchestrateMove(7);          // 启动第一程
//   // 断言：Mover->AdvanceCount==2；Mover->FlagAtAdvanceIssue[1]==false（第二程在回调外发起）
// @endcode
// =============================================================================
struct FPawnMoverSpy final : public IPawnMover
{
    /** 注入的回合2 子系统（裸指针，测试期 World 内存活，仿 FBankruptcyResolverStub 惯例）。 */
    UPlayerTurnSubsystem* Sub = nullptr;

    /**
     * 落地回调窗口标志：true = 当前正处于 OnPawnLanded（HandlePawnLanded）同步回调内。
     * Advance 在调 Sub->HandlePawnLanded 前置 true、之后置 false。
     */
    bool bInLandedCallback = false;

    /** Advance 被调用次数（含蹦床发起的后续程）。 */
    int32 AdvanceCount = 0;

    /** TeleportTo 被调用次数。 */
    int32 TeleportCount = 0;

    /**
     * 每次 Advance 发起时刻捕获的 bInLandedCallback 值（按发起先后）。
     * AC-47 核心断言：第二程（索引 1）应为 false（蹦床在回调外发起）。
     */
    TArray<bool> FlagAtAdvanceIssue;

    /** 本程落地上报的上下文（默认 DiceMove；TC-4 可设 SentToJail）。 */
    EArrivalContext ArrivalToReport = EArrivalContext::DiceMove;

    /** 是否在第一程落地回调内请求第二程（TC-3 非重入场景开关）。 */
    bool bRequestSecondLeg = false;

    /** 第二程步数（bRequestSecondLeg=true 时使用）。 */
    int32 SecondLegSteps = 0;

    /**
     * 模拟前进 Steps 步并同步触发落地回调窗口。
     *
     * @param PlayerId 当前行动玩家 PlayerId（spy 不使用，仅满足接口）
     * @param Steps    前进步数
     */
    virtual void Advance(int32 /*PlayerId*/, int32 Steps) override
    {
        ++AdvanceCount;

        // ① 捕获发起时刻 flag（非重入则恒 false——蹦床只在回调外发起 Advance）
        FlagAtAdvanceIssue.Add(bInLandedCallback);

        // ② 进入落地回调窗口
        bInLandedCallback = true;

        if (Sub)
        {
            Sub->HandlePawnLanded(ArrivalToReport);
        }

        // ③ 在回调窗口内请求第二程（仅第一程）：应被回合2 排队，不在此栈内递归发起
        if (bRequestSecondLeg && AdvanceCount == 1 && Sub)
        {
            Sub->OrchestrateMove(SecondLegSteps);
        }

        // ④ 退出落地回调窗口
        bInLandedCallback = false;
    }

    /**
     * 模拟传送到指定格并同步触发落地回调窗口（最小实现）。
     *
     * @param PlayerId  当前行动玩家 PlayerId（spy 不使用）
     * @param TileIndex 目标格（spy 不使用）
     */
    virtual void TeleportTo(int32 /*PlayerId*/, int32 /*TileIndex*/) override
    {
        ++TeleportCount;

        bInLandedCallback = true;
        if (Sub)
        {
            Sub->HandlePawnLanded(ArrivalToReport);
        }
        bInLandedCallback = false;
    }
};
