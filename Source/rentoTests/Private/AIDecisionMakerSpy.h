// AIDecisionMakerSpy.h
// =============================================================================
// FAIDecisionMakerSpy — IAIDecisionMaker 测试 spy（story pt-007 AC-3 / AC-37d）
//
// 用途：
//   TC-3 / TC-37d 要求断言：
//     ① DecidePostRollActions 被调用次数 == 1
//     ② RunAiPostRollActions 执行完后阶段切 TurnEnd + 移交下一玩家
//   本 spy 控制 DecidePostRollActions 返回值（非空列表 / 空列表[]）并记录调用次数。
//
// 机制（G3 约束适用）：
//   IAIDecisionMaker 是纯 C++ 接口（非 UInterface），spy 为纯 struct 不需 UCLASS/UHT。
//   直接继承 IAIDecisionMaker + 实现所有纯虚函数。
//   TSharedPtr<IAIDecisionMaker> 注入 UPlayerTurnSubsystem（DI 接缝）。
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
//
// 参照范本：
//   - LandingResolverSpy.h（相同纯 C++ DI spy 模式）
//   - PawnMoverSpy.h（纯 C++ struct spy）
//
// 规范依据：
//   - story pt-007 AC-3 / AC-37d（AI PostRollAction hook + 空数组直接 EndTurn）
//   - AIDecisionMakerInterface.h（IAIDecisionMaker 纯 C++ 接口）
//   - G3：纯 C++ 接口 spy 用 struct，不需 UHT/UCLASS
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "AIDecisionMakerInterface.h"
#include "GameStateSnapshot.h"

// =============================================================================
// FAIDecisionMakerSpy — 受控返回值 + 调用计数（测试专用）
//
// 用法（TC-3 非空动作列表场景）：
// @code
//   TSharedPtr<FAIDecisionMakerSpy> Spy = MakeShared<FAIDecisionMakerSpy>();
//   // 注入 2 条非空动作
//   Spy->PostRollActionsToReturn = {
//       FTurnAction{ EAIActionType::BuyProperty, 5 },
//       FTurnAction{ EAIActionType::BuildHouse,  5 },
//   };
//   Sub->SetAIDecisionMaker(Spy);
//   FGameStateSnapshot Snapshot;  // mock 字面量
//   Sub->RunAiPostRollActions(ActiveId, Snapshot);
//   TestEqual(TEXT("DecidePostRollActions 调用次数==1"),
//       Spy->DecidePostRollActionsCallCount, 1);
// @endcode
//
// 用法（TC-37d 空数组场景）：
// @code
//   Spy->PostRollActionsToReturn = {};  // 空数组 []
// @endcode
// =============================================================================
struct FAIDecisionMakerSpy final : public IAIDecisionMaker
{
    // ---- 受控返回值 ----

    /**
     * DecidePostRollActions 受控返回值（默认空数组）。
     * TC-3：设 2 条非空动作；TC-37d：留空数组 []。
     */
    TArray<FTurnAction> PostRollActionsToReturn;

    /** DecideJailAction 受控返回值（默认 RollDouble） */
    EJailAction JailActionToReturn = EJailAction::RollDouble;

    /** DecideBuyProperty 受控返回值（默认 false = 不买） */
    bool BuyDecisionToReturn = false;

    /** DecideAuctionBid 受控返回值（默认 0 = 放弃） */
    int32 AuctionBidToReturn = 0;

    // ---- 调用计数 ----

    /** DecidePostRollActions 被调用次数（TC-3 / TC-37d 断言） */
    int32 DecidePostRollActionsCallCount = 0;

    /** DecideJailAction 被调用次数 */
    int32 DecideJailActionCallCount = 0;

    /** DecideBuyProperty 被调用次数 */
    int32 DecideBuyPropertyCallCount = 0;

    /** DecideAuctionBid 被调用次数 */
    int32 DecideAuctionBidCallCount = 0;

    // ---- IAIDecisionMaker 接口实现 ----

    /**
     * AI 买地决策 spy（记录调用次数，返回受控值）。
     * 简 A 仅声明调用路径，本 spy 实现供后续簇测试使用。
     */
    virtual bool DecideBuyProperty(const FGameStateSnapshot& /*S*/, int32 /*TileIndex*/) override
    {
        ++DecideBuyPropertyCallCount;
        return BuyDecisionToReturn;
    }

    /**
     * AI 回合后行动决策 spy（记录调用次数，返回受控动作列表）。
     *
     * TC-3：PostRollActionsToReturn 含 2 条 → 返回 2 条非空列表
     * TC-37d：PostRollActionsToReturn 为空 → 返回空数组 []
     */
    virtual TArray<FTurnAction> DecidePostRollActions(const FGameStateSnapshot& /*S*/) override
    {
        ++DecidePostRollActionsCallCount;
        return PostRollActionsToReturn;
    }

    /**
     * AI 出狱决策 spy（记录调用次数，返回受控值）。
     * 簇 A 仅声明调用路径，本 spy 实现供后续簇测试使用。
     */
    virtual EJailAction DecideJailAction(const FGameStateSnapshot& /*S*/) override
    {
        ++DecideJailActionCallCount;
        return JailActionToReturn;
    }

    /**
     * AI 拍卖出价 spy（Alpha 占位，记录调用次数，返回受控值）。
     */
    virtual int32 DecideAuctionBid(const FGameStateSnapshot& /*S*/, int32 /*TileIndex*/) override
    {
        ++DecideAuctionBidCallCount;
        return AuctionBidToReturn;
    }
};
