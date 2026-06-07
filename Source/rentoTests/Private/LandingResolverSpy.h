// LandingResolverSpy.h
// =============================================================================
// FLandingResolverSpy — ILandingResolver 测试 spy（story pt-006 AC-4 TC-4）
//
// 用途：
//   TC-4 要求断言「SentToJail 时 DecideBuyProperty 调用次数==0，
//   DiceMove 落无主地产时==1」——须捕获 ILandingResolver 调用次数。
//
// 机制：
//   纯 C++ struct（非 UObject）——ILandingResolver 是纯 C++ 接口，
//   不需要 UHT/UCLASS；直接继承 ILandingResolver 即可。
//   TSharedPtr<ILandingResolver> 注入 UPlayerTurnSubsystem（DI 接缝）。
//
// 仅测试用：物理落 Source/rentoTests/Private/，生产代码不引用。
//
// 规范依据：
//   - story pt-006 AC-4（SentToJail 抑制 + DiceMove 买地 seam）
//   - story pt-006 TC-4（spy 计数 DecideBuyProperty）
//   - BankruptcyInterface.h（相同纯 C++ DI 接缝模式，参照）
//   - NextActivePlayerBankruptcyOnGameWonTest.cpp（FBankruptcyResolverStub 范本）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "LandingResolverInterface.h"

// =============================================================================
// FLandingResolverSpy — 计数 DecideBuyProperty / SettleRent 调用次数（测试专用）
//
// 用法：
// @code
//   TSharedPtr<FLandingResolverSpy> Spy = MakeShared<FLandingResolverSpy>();
//   Sub->SetLandingResolver(Spy);
//   // 驱动 HandlePawnLanded(DiceMove) ...
//   TestEqual(TEXT("DecideBuyProperty 恰 1 次"), Spy->DecideBuyPropertyCallCount, 1);
// @endcode
// =============================================================================
struct FLandingResolverSpy final : public ILandingResolver
{
    /** DecideBuyProperty 被调用次数（TC-4 断言） */
    int32 DecideBuyPropertyCallCount = 0;

    /** SettleRent 被调用次数（TC-4 扩展断言，本 story 不主动调用） */
    int32 SettleRentCallCount = 0;

    /** DecideBuyProperty 受控返回值（默认 false = 不买） */
    bool bControlledBuyDecision = false;

    /**
     * 买地决策 spy（记录调用次数，返回受控值）。
     *
     * @param PlayerId  当前行动玩家 PlayerId
     * @param TileIndex 落点 tile 索引
     * @return bControlledBuyDecision（受控返回值）
     */
    virtual bool DecideBuyProperty(int32 /*PlayerId*/, int32 /*TileIndex*/) override
    {
        ++DecideBuyPropertyCallCount;
        return bControlledBuyDecision;
    }

    /**
     * 收租结算 spy（记录调用次数，MVP 阶段无实现）。
     *
     * @param PlayerId  当前行动玩家 PlayerId
     * @param TileIndex 落点 tile 索引
     */
    virtual void SettleRent(int32 /*PlayerId*/, int32 /*TileIndex*/) override
    {
        ++SettleRentCallCount;
    }
};
