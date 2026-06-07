// PawnMovementInterface.h
// =============================================================================
// IPawnMover — 棋子移动 DI 注入接口（story pt-006 / GDD CR-3 MovePhase + CR-3.1 程间非重入）
//
// 架构约束（ADR-0001 + story-006 LOCKED 设计决策 D）：
//   - 移动 seam 用纯 C++ DI（仿 BankruptcyInterface.h 模式，非 UInterface）
//   - TSharedPtr<IPawnMover> 持有，headless 可 stub 注入
//   - Advance/TeleportTo 实现内须同步触发落地回调（HandlePawnLanded）
//   - 回合2（UPlayerTurnSubsystem）经 public HandlePawnLanded(EArrivalContext)
//     接收落地回调，不在 Advance 内部直接耦合
//
// 程间非重入约束（story-006 LOCKED 设计决策 D / AC-47）：
//   - Advance/TeleportTo 须同步完成（含触发 HandlePawnLanded 回调）后才返回
//   - 回合2 发起下一程 Advance 必须在前一程 HandlePawnLanded 同步返回后进行
//   - 移动 spy 在 Advance 内同步调 HandlePawnLanded，以验证非重入 guard 机制
//
// 规范依据：
//   - story pt-006 LOCKED 设计决策 D（程间非重入 + DI seam）
//   - ADR-0001（UObject 宿主与生命周期）
//   - GDD CR-3.1（程间原子性/非重入）
//   - GDD Edge Cases L392（SentToJail 抑制）
//   - BankruptcyInterface.h（DI 接缝形态范本）
// =============================================================================
#pragma once

#include "CoreMinimal.h"

// 前向声明
enum class EArrivalContext : uint8;

// =============================================================================
// IPawnMover — 棋子移动 DI 注入接口（纯 C++ seam）
//
// 实现要求：
//   - Advance 实现：计算目标格，移动棋子，同步触发 HandlePawnLanded 回调
//   - TeleportTo 实现：直接传送到目标格，同步触发 HandlePawnLanded 回调
//   - 真实移动（位置写入、动画）归 movement epic；
//     本 story 只提供 seam（Out of Scope 严守）
//   - MVP stub 在 Advance/TeleportTo 内同步调用注入的 OnLanded 回调（TC-3 确定性）
// =============================================================================
class RENTO_API IPawnMover
{
public:
    /** 虚析构，保证正确 delete 派生类 */
    virtual ~IPawnMover() = default;

    /**
     * 前进 Steps 步（骰子移动路径）。
     *
     * 实现约束（story-006 AC-47 / GDD CR-3.1 程间非重入）：
     *   - 移动完成后**同步**触发落地回调（调用 UPlayerTurnSubsystem::HandlePawnLanded）
     *   - 回调须在本函数返回前完成，不得异步延迟
     *   - 移动4 真实实现（动画、位置计算）归 movement epic；
     *     MVP spy 实现直接同步触发回调
     *
     * @param PlayerId 当前行动玩家 PlayerId
     * @param Steps    前进步数（来自 FDiceRollResult.Total）
     */
    virtual void Advance(int32 PlayerId, int32 Steps) = 0;

    /**
     * 传送到指定格（GoToJail 或卡牌效果）。
     *
     * 实现约束同 Advance：完成后同步触发落地回调。
     * 传送完成后 HandlePawnLanded 携带的 arrivalContext 由调用方传递。
     *
     * @param PlayerId  当前行动玩家 PlayerId
     * @param TileIndex 目标 tile 索引
     */
    virtual void TeleportTo(int32 PlayerId, int32 TileIndex) = 0;
};
