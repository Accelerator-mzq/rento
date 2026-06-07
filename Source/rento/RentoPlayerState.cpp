// RentoPlayerState.cpp
// =============================================================================
// URentoPlayerState — 中心玩家状态实体实现（pt-001 / pt-005 / TR-turn-001 / TR-turn-003）
//
// story pt-001 scope：
//   - 字段声明与默认值（全部在头文件 in-class 初始化）
//   - 本文件原为空白骨架，供 story-005 扩展
//
// story pt-005 scope（新增，受控写接口面）：
//   - SetPosition(int32 Index)              — 字段级 setter，CurrentTileIndex
//   - SetCash(int32 Value)                  — 字段级 setter，Cash
//   - SetJailState(bool, EJailReason)       — 字段级 setter，bIsInJail
//   - SetCurrentRollContext(FDiceRollResult) — 字段级 setter，CurrentRollContext holder
//   - SetBankrupt(bool)                     — 字段级 setter，bIsBankrupt
//
//   封装强度（OQ-1 ADR 未裁前）：
//     字段保持 UPROPERTY(BlueprintReadOnly)，不改 private。
//     AC-3/AC-35a 静态门 deferred（待 OQ-1 ADR 裁定后激活）。
//
// 规范依据：
//   - story pt-001（字段容器，GDD CR-1 边界契约）
//   - story pt-005 AC-1~4（受控写接口面，TR-turn-003）
//   - ADR-0001（UObject 宿主，不持状态写语义——宿主不改写规则，setter 是字段级入口）
//   - ADR-0007（写权威状态 → C++；BP↔C++ handoff → BlueprintCallable）
// =============================================================================

#include "RentoPlayerState.h"

// ===========================================================================
// SetPosition — 受控写接口：CurrentTileIndex（唯一合法调用方=移动4）
// ===========================================================================

void URentoPlayerState::SetPosition(int32 Index)
{
    // 字段级 setter：直接写 CurrentTileIndex，不做边界校验（归移动4）
    UE_LOG(LogTemp, Verbose,
        TEXT("URentoPlayerState::SetPosition — PlayerId=%d CurrentTileIndex: %d → %d "
             "（唯一合法调用方=移动4，受控写接口面 story-005 / TR-turn-003）"),
        PlayerId, CurrentTileIndex, Index);

    CurrentTileIndex = Index;
}

// ===========================================================================
// SetCash — 受控写接口：Cash（唯一合法调用方=经济5）
// ===========================================================================

void URentoPlayerState::SetCash(int32 Value)
{
    // 字段级 setter：直接写 Cash，不校验负数（破产判定归经济5/破产9）
    UE_LOG(LogTemp, Verbose,
        TEXT("URentoPlayerState::SetCash — PlayerId=%d Cash: %d → %d "
             "（唯一合法调用方=经济5，受控写接口面 story-005 / TR-turn-003）"),
        PlayerId, Cash, Value);

    Cash = Value;
}

// ===========================================================================
// SetJailState — 受控写接口：bIsInJail（唯一合法调用方=事件格7）
// ===========================================================================

void URentoPlayerState::SetJailState(bool bInJail, EJailReason Reason)
{
    // 字段级 setter：直接写 bIsInJail；Reason 接收存储（完整使用语义归事件格7）
    UE_LOG(LogTemp, Verbose,
        TEXT("URentoPlayerState::SetJailState — PlayerId=%d bIsInJail: %s → %s, "
             "Reason=%d "
             "（唯一合法调用方=事件格7，受控写接口面 story-005 / TR-turn-003）"),
        PlayerId,
        bIsInJail ? TEXT("true") : TEXT("false"),
        bInJail   ? TEXT("true") : TEXT("false"),
        static_cast<int32>(Reason));

    bIsInJail = bInJail;

    // Reason 字段：本 story 仅接收，完整 EJailReason 使用语义归事件格7（Out of Scope）
    // 此处不存储 Reason（EJailReason 占位枚举，holder 字段由事件7 epic 追加）
}

// ===========================================================================
// SetCurrentRollContext — 受控写接口：CurrentRollContext holder（唯一合法调用方=事件格7）
// ===========================================================================

void URentoPlayerState::SetCurrentRollContext(const FDiceRollResult& RollResult)
{
    // 字段级 setter：直接写 CurrentRollContext holder（事件额外掷骰存储）
    UE_LOG(LogTemp, Verbose,
        TEXT("URentoPlayerState::SetCurrentRollContext — PlayerId=%d "
             "CurrentRollContext: Die1=%d, Die2=%d, Total=%d, bIsDouble=%s "
             "（唯一合法调用方=事件格7 额外掷骰，受控写接口面 story-005 / GDD CR-3.1）"),
        PlayerId,
        RollResult.Die1, RollResult.Die2, RollResult.Total,
        RollResult.bIsDouble ? TEXT("true") : TEXT("false"));

    CurrentRollContext = RollResult;
}

// ===========================================================================
// SetBankrupt — 受控写接口：bIsBankrupt（唯一合法调用方=本系统回合2）
//
// AC-4 关键路径：HandlePlayerBankruptcy 经 IResolveBankruptcy 返回
// debtorEliminated=true 后调本 setter，物理阻断 2↔9 直写环路。
// ===========================================================================

void URentoPlayerState::SetBankrupt(bool bNewBankrupt)
{
    // 字段级 setter：直接写 bIsBankrupt
    // 唯一合法调用方=本系统回合2（HandlePlayerBankruptcy），
    // 破产9 为 return-only，绝不直写本字段（AC-4 物理防环）
    UE_LOG(LogTemp, Verbose,
        TEXT("URentoPlayerState::SetBankrupt — PlayerId=%d bIsBankrupt: %s → %s "
             "（唯一合法调用方=本系统回合2，据破产9 return-only 结果写入，"
             "受控写接口面 story-005 / TR-turn-003）"),
        PlayerId,
        bIsBankrupt    ? TEXT("true") : TEXT("false"),
        bNewBankrupt   ? TEXT("true") : TEXT("false"));

    bIsBankrupt = bNewBankrupt;
}
