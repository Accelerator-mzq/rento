// RentoPlayerState.h
// =============================================================================
// URentoPlayerState — 中心玩家状态实体（pt-001 / TR-turn-001 / GDD CR-1）
//
// 形态选择：轻量 UObject（非 AActor / APlayerState）
//   理由：
//   1. headless NewObject<> 可构造（测试友好，-nullrhi 可跑）
//   2. TObjectPtr<URentoPlayerState> + UPROPERTY() 防 GC
//   3. GDD 与 AC 使用「PlayerState 列表」引用语义（非 USTRUCT 值拷贝）
//   4. story-005 受控写接口需要持稳定引用实体
//   5. 类名 URentoPlayerState（U 前缀）与引擎 APlayerState（A 前缀）不同名不冲突
//
// ⚠ 命名注意（UHT）：
//   UPlayerState 与引擎 APlayerState 无冲突（前缀不同），但若 UHT 报错则已使用
//   URentoPlayerState 避冲突（project-prefix 策略，ADR-0001 §Naming）。
//
// GDD CR-1 全 11 字段（逐字对齐，不增不减）：
//   PlayerId / DisplayName / TokenColor / bIsAI / AIDifficulty /
//   CurrentTileIndex / Cash / bIsInJail / JailTurnsServed / bIsBankrupt /
//   TurnOrderIndex
//
// 额外字段（AC-1 权威，GDD CR-4/L199 确认，story TC-1 要求）：
//   ConsecutiveDoubles — 连续双点计数，开局为 0，定序不累加（GDD CR-2）
//   （注：GDD CR-1 表格 11 字段不含此项，但 AC-1/TC-1 明确要求 ConsecutiveDoubles=0 默认）
//
// 字段写语义归属（GDD CR-1 边界契约）：
//   本 story 只声明字段容器与默认值，不实现委派字段的改写规则（story-005）。
//   受控写接口面（SetPosition/SetCash/SetJailState/SetBankrupt）留 story-005 实现。
//
// 规范依据：
//   - GDD player-turn.md CR-1（11 字段表，L74-90）
//   - story pt-001 AC-1（字段清单权威）+ TC-1（默认值校验）
//   - ADR-0001（UObject 宿主与生命周期，UPROPERTY+TObjectPtr 防 GC）
//   - ADR-0005（字段对齐 PlayerState/GameState，Full Vision 迁移预留）
//   - control-manifest Foundation Layer §Full Vision 迁移预留
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlayerTurnTypes.h"
#include "RentoPlayerState.generated.h"

/**
 * URentoPlayerState — 中心玩家状态实体
 *
 * 持有 GDD CR-1 全 11 字段 + ConsecutiveDoubles（AC-1 权威要求）。
 * 随局在 UPlayerTurnSubsystem 构建，随 World 销毁。
 *
 * 访问方式（通过 UPlayerTurnSubsystem）：
 * @code
 *   UPlayerTurnSubsystem* Sub = World->GetSubsystem<UPlayerTurnSubsystem>();
 *   const TArray<TObjectPtr<URentoPlayerState>>& States = Sub->GetPlayerStates();
 * @endcode
 */
UCLASS(BlueprintType)
class RENTO_API URentoPlayerState : public UObject
{
    GENERATED_BODY()

public:
    // =========================================================================
    // §本系统(开局)拥有改写规则的字段（GDD CR-1）
    // =========================================================================

    /**
     * 唯一玩家 ID（int32，开局分配后恒定）。
     * 改写规则归：本系统(开局)。
     * 默认 -1 表示未分配（开局后恒为 0..P-1）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    int32 PlayerId = -1;

    /**
     * 玩家显示名（FText，大厅配置传入）。
     * 改写规则归：本系统(大厅20配置)。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    FText DisplayName;

    /**
     * 棋子颜色（EPlayerColor，开局唯一分配后恒定）。
     * 改写规则归：本系统(开局)。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    EPlayerColor TokenColor = EPlayerColor::None;

    /**
     * 是否 AI 控制（bool，开局设置后恒定）。
     * 改写规则归：本系统(开局)。
     * 默认 false（AC-1 字段默认值契约）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    bool bIsAI = false;

    /**
     * AI 难度（EAIDifficulty，非 AI 为 None）。
     * 改写规则归：本系统(开局)。
     * 默认 None（AC-1：bIsAI=false → AIDifficulty=None）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    EAIDifficulty AIDifficulty = EAIDifficulty::None;

    /**
     * 回合环位置（0..P-1，开局定序后恒定）。
     * TurnOrderIndex=0 为先手（点数最高者），改写规则归：本系统(开局定序)。
     * 默认 -1 表示未分配。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    int32 TurnOrderIndex = -1;

    // =========================================================================
    // §委派字段——本系统持有，但改写规则归他系统（GDD CR-1 边界契约）
    // =========================================================================

    /**
     * 当前位置（board TileIndex，int32）。
     * 改写规则归：移动(4)，经受控写接口 SetPosition(index)（story-005 实现）。
     * 默认 0（GO 格起点）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    int32 CurrentTileIndex = 0;

    /**
     * 现金值（int32）。
     * 改写规则归：经济(5)，经受控写接口 SetCash/Debit/Credit（story-005 实现）。
     * 默认 0（开局起始资金由 FGameSetupConfig 或经济系统初始化，本 story Out of Scope）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    int32 Cash = 0;

    /**
     * 是否在狱（bool）。
     * 改写规则归：事件格(7)，经受控写接口 SetJailState(bInJail, reason)（story-005）。
     * 默认 false（AC-1 字段默认值契约）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    bool bIsInJail = false;

    /**
     * 已服刑回合数（int32，用于"满3回合强制交保释"判定，规则归事件格7）。
     * 改写规则归：本系统计数 + 事件格(7)定规则。
     * 默认 0（AC-1 字段默认值契约）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    int32 JailTurnsServed = 0;

    /**
     * 破产标志（bool）。
     * 改写规则归：本系统(回合2)，据破产胜负(9) ResolveBankruptcy 返回 debtorEliminated=true 写入。
     * 默认 false（AC-1 字段默认值契约）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    bool bIsBankrupt = false;

    // =========================================================================
    // §额外字段（AC-1 权威，GDD CR-4/L199 确认）
    // =========================================================================

    /**
     * 连续双点计数（int32，开局为 0，定序不累加——GDD CR-2）。
     *
     * GDD CR-1 表格 11 字段不含此项，但 AC-1/TC-1 明确要求 ConsecutiveDoubles=0 默认。
     * GDD CR-4/L199 确认该字段存于本系统。以 AC/TC 为权威，本字段归属此 UObject。
     *
     * 改写规则归：本系统(回合状态机)，story-002 实现双点链计数逻辑。
     * 定序阶段固定为 0，不因定序掷骰而累加（GDD CR-2 澄清）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    int32 ConsecutiveDoubles = 0;
};
