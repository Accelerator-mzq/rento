// BankruptcyInterface.h
// =============================================================================
// IResolveBankruptcy — 破产9 return-only DI 注入接口（story pt-003 / TR-turn-006）
//
// 架构约束（ADR-0003 单一事件源 + story-003 LOCKED 设计决策 §4）：
//   - 破产9 为 return-only：ResolveBankruptcy 只计算并返回结果，绝不回调本系统
//   - 回合2 读返回值（FBankruptcyResolution）后单独驱动 bIsBankrupt 写和 OnGameWon 广播
//   - 这是 2↔9 接缝的安全阀：interface 签名强制 return-only，物理阻断回调环
//
// 接口形态（参照 IGameClock 纯 C++ TSharedPtr DI 模式）：
//   - 纯 C++ 接口（非 UInterface）：不依赖 UHT，headless 可 stub 注入
//   - TSharedPtr<IResolveBankruptcy> 持有，生命周期安全（与 IGameClock ADR-0008 惯例一致）
//
// FBankruptcyResolution：
//   - 本 story 到 bankruptcy epic 的 seam 结构体
//   - story-004 的 6 事件 payload 声明不在此（story-004 负责 enrich）
//
// 规范依据：
//   - story pt-003 LOCKED 设计决策 §4
//   - ADR-0003（单一事件源；OnGameWon 由回合2 触发，9 return-only）
//   - ADR-0001（per-match 宿主注入；生命周期随 World）
//   - control-manifest Foundation Layer §事件总线 (ADR-0003)
// =============================================================================
#pragma once

#include "CoreMinimal.h"

// =============================================================================
// FBankruptcyResolution — 破产9 裁决结果（return-only 返回结构）
//
// 回合2 从 ResolveBankruptcy 获取此结构，据此：
//   1. bDebtorEliminated=true → 置 debtor bIsBankrupt=true + 移出轮转
//   2. WinnerId != INDEX_NONE → 广播 OnGameWon(WinnerId) 恰一次
//
// 设计说明（story-003 §4）：
//   WinnerId=INDEX_NONE 表示对局继续（>1 人存活 / 退化局由9 内部处理）。
//   本 struct 是本 story 到 bankruptcy epic 的 seam，非 story-004 的 6 事件 payload。
// =============================================================================
struct RENTO_API FBankruptcyResolution
{
    /** 债务人是否被消除（true = 永久移出轮转，bIsBankrupt 将被置 true） */
    bool bDebtorEliminated = false;

    /**
     * 获胜玩家 PlayerId（INDEX_NONE = 对局继续，无胜者）。
     *
     * 破产9 在 activePlayersSnapshot 上算 APC（显式排除 debtor）：
     *   - APC==1 → 唯一存活者 PlayerId → WinnerId=该 PlayerId
     *   - APC>1  → 对局继续 → WinnerId=INDEX_NONE
     *   - APC==0 → 退化局，由9 内部处理兜底，WinnerId=INDEX_NONE
     *
     * ⚠ 回合2 绝不独立用本系统 F-2 重算「APC==1→胜利」（AC-8 封堵2↔9双触发）
     */
    int32 WinnerId = INDEX_NONE;
};

// =============================================================================
// IResolveBankruptcy — 破产9 注入接口（纯 C++，return-only）
//
// 实现要求：
//   - Implement() 只计算+返回 FBankruptcyResolution，绝不调任何回调
//   - MVP stub 实现注入受控返回值（story-003 TC-6/7/8 确定性保证）
//   - 生产实现由 bankruptcy epic 提供
//
// 使用方式（story-003 HandlePlayerBankruptcy）：
// @code
//   FBankruptcyResolution Result = BankruptcyResolver->ResolveBankruptcy(
//       DebtorId, CreditorId, ActiveIndices);
//   if (Result.bDebtorEliminated) { ... }
//   if (Result.WinnerId != INDEX_NONE && !bGameWon) { ... }
// @endcode
// =============================================================================
class RENTO_API IResolveBankruptcy
{
public:
    /** 虚析构，保证正确 delete 派生类 */
    virtual ~IResolveBankruptcy() = default;

    /**
     * 裁决破产结果（return-only，绝不回调本系统）。
     *
     * @param DebtorId         债务玩家 PlayerId
     * @param CreditorId       债权玩家 PlayerId（-1 表示银行/无债权方）
     * @param ActivePlayerCount 当前活跃玩家数（debtor 排除后的快照值，由调用方传入）
     * @return 裁决结果（bDebtorEliminated + WinnerId）
     *
     * ⚠ 实现禁止调用任何回调（return-only 是2↔9无环安全阀的物理保证）
     */
    virtual FBankruptcyResolution ResolveBankruptcy(
        int32 DebtorId,
        int32 CreditorId,
        int32 ActivePlayerCount) = 0;
};
