// LandingResolverInterface.h
// =============================================================================
// ILandingResolver — 落地结算 DI 注入接口（story pt-006 / GDD CR-3 ResolvePhase）
//
// 架构约束（ADR-0003 + ADR-0001 + story-006 LOCKED 设计决策 C）：
//   - 落地结算 hook 用纯 C++ DI 接缝（仿 BankruptcyInterface.h 模式）
//   - 非 UInterface：不依赖 UHT，headless 可 stub 注入
//   - TSharedPtr<ILandingResolver> 持有，生命周期安全
//   - return-only 签名：决策结果从返回值读取，不回调本系统
//
// 接口说明：
//   - DecideBuyProperty：买地决策钩子（在无主地产落地时，回合2 ResolvePhase 调用）
//   - SettleRent：收租结算钩子（算租公式归 economy epic，本 seam 只暴露调用入口）
//   - 本 story 只声明 seam；真实经济/地产逻辑归各自 epic（Out of Scope）
//
// 规范依据：
//   - story pt-006 LOCKED 设计决策 C（纯 C++ DI 接缝，仿 IResolveBankruptcy 模式）
//   - ADR-0001（per-match 宿主注入；生命周期随 World）
//   - ADR-0003（事件总线，结算逻辑不回调本系统）
//   - BankruptcyInterface.h（DI 接缝形态范本）
// =============================================================================
#pragma once

#include "CoreMinimal.h"

// =============================================================================
// ILandingResolver — 落地结算 DI 注入接口（纯 C++，return-only seam）
//
// 实现要求：
//   - DecideBuyProperty / SettleRent 只执行结算并返回，绝不回调 ResolvePhase
//   - MVP stub 注入受控返回值（TC-4 确定性保证）
//   - 生产实现由 economy/property epic 提供
//
// 使用方式（story-006 ResolveArrival）：
// @code
//   if (LandingResolver.IsValid())
//   {
//       bool bWillBuy = LandingResolver->DecideBuyProperty(PlayerId, TileIndex);
//   }
// @endcode
// =============================================================================
class RENTO_API ILandingResolver
{
public:
    /** 虚析构，保证正确 delete 派生类 */
    virtual ~ILandingResolver() = default;

    /**
     * 买地决策钩子（在 arrivalContext==DiceMove 且落无主地产时调用）。
     *
     * 唯一调用方：UPlayerTurnSubsystem::ResolveArrival，在 DiceMove 路径下。
     * SentToJail 路径跳过，不调用本函数（GDD Edge Cases L392 / AC-46）。
     *
     * 真实买地逻辑（扣款、标注归属、AI决策算法）归 economy/property/AI epic；
     * 本 story 只提供 seam 签名（Out of Scope 严守）。
     *
     * @param PlayerId  当前行动玩家 PlayerId
     * @param TileIndex 落点 tile 索引
     * @return true = 决定购买；false = 不购买（AI stub 注入受控值）
     */
    virtual bool DecideBuyProperty(int32 PlayerId, int32 TileIndex) = 0;

    /**
     * 收租结算钩子（在落有主地产时调用）。
     *
     * 算租公式（F-4 Utility 租 PULL dice_total、F-2/F-3 普通/色组租）归 economy epic；
     * 本 story 只暴露调用入口 seam（Out of Scope 严守）。
     *
     * @param PlayerId  当前行动玩家 PlayerId（租方）
     * @param TileIndex 落点 tile 索引
     */
    virtual void SettleRent(int32 PlayerId, int32 TileIndex) = 0;
};
