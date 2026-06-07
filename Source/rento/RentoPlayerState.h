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
//   本 story 声明字段容器与默认值，并实现委派字段的受控写接口（story-005）。
//   受控写接口面（SetPosition/SetCash/SetJailState/SetCurrentRollContext/SetBankrupt）由
//   story-005 添加于此文件，以字段 owner 身份持有 setter。
//
// story-005 新增（受控写接口面，TR-turn-003 / GDD CR-1 L92-99）：
//   SetPosition(int32 Index)              — 唯一合法调用方：移动(4)
//   SetCash(int32 Value)                  — 唯一合法调用方：经济(5)
//   SetJailState(bool, EJailReason)       — 唯一合法调用方：事件格(7)
//   SetCurrentRollContext(FDiceRollResult) — 唯一合法调用方：事件格(7)，仅事件额外掷骰
//   SetBankrupt(bool)                     — 唯一合法调用方：本系统回合(2)
//
//   ⚠ 封装强度（OQ-1 ADR 未裁前为 [Advisory] code-review 软约束）：
//     字段保持 UPROPERTY(BlueprintReadOnly) 不改 private（AC-3/AC-35a 静态门 deferred）。
//     强封装（private + BP 只读）待 OQ-1 ADR 裁定后升级。
//
// 规范依据：
//   - GDD player-turn.md CR-1（11 字段表，L74-90）+ CR-3.1（SetCurrentRollContext）
//   - story pt-001 AC-1（字段清单权威）+ TC-1（默认值校验）
//   - story pt-005 AC-1~4（受控写接口面）+ TC-1/TC-4（setter 验证）
//   - ADR-0001（UObject 宿主与生命周期，UPROPERTY+TObjectPtr 防 GC）
//   - ADR-0005（字段对齐 PlayerState/GameState，Full Vision 迁移预留）
//   - ADR-0007（写权威状态 → C++；BP↔C++ handoff → BlueprintCallable）
//   - control-manifest Foundation Layer §Full Vision 迁移预留
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlayerTurnTypes.h"
#include "DiceRngService.h"      // FDiceRollResult（SetCurrentRollContext 参数类型）
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

    // =========================================================================
    // §story-005 新增 holder 字段（GDD CR-3.1 SetCurrentRollContext）
    // =========================================================================

    /**
     * 当前程掷骰上下文（FDiceRollResult，事件格7额外掷骰 holder）。
     *
     * GDD CR-3.1：仅事件格7 在事件额外掷骰时写入（SetCurrentRollContext setter）。
     * 本字段是 result 存储 holder，非每程正常骰子结果。
     * PULL 消费 + 程间非重入语义 → story-006（本 story 只提供 holder + setter）。
     *
     * 改写规则归：事件格(7)，经受控写接口 SetCurrentRollContext（story-005 实现）。
     * 默认值：Die1=0/Die2=0/Total=0/bIsDouble=false（零值 holder）。
     */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|PlayerState")
    FDiceRollResult CurrentRollContext;

    // =========================================================================
    // §story-005 受控写接口面（TR-turn-003 / GDD CR-1 L92-99 / ADR-0007）
    //
    // 架构约定（OQ-1 ADR 未裁前）：
    //   封装强度 = [Advisory] code-review 软约束（AC-3/AC-35a 静态门 deferred）。
    //   字段保持 UPROPERTY(BlueprintReadOnly)，不改 private（防破坏既有测试直接读写）。
    //   强封装（private + BP 只读）待 OQ-1 ADR 裁定后实施。
    //
    // 唯一合法调用方文档化（ADR-0007 BP↔C++ handoff，BlueprintCallable 暴露）：
    //   每个 setter 注释明确「唯一合法调用方」，code-review 强制核对。
    // =========================================================================

    /**
     * 受控写接口：更新当前棋盘位置（CurrentTileIndex）。
     *
     * 唯一合法调用方：移动(4) 系统在落点确定后调用本 setter。
     * 任何其他路径的直写 CurrentTileIndex 均为受控写违规（AC-35 code-review 核对）。
     *
     * 实现语义（字段级 setter）：
     *   直接写 CurrentTileIndex = Index，不校验格子合法性（边界校验归移动4）。
     *
     * @param Index 目标 tile 索引（由移动4 计算并传入）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|ControlledWrite",
        meta=(DisplayName="Set Position (Controlled Write — Move System Only)",
              ToolTip="受控写接口：唯一合法调用方=移动(4)。更新玩家当前棋盘位置。"))
    void SetPosition(int32 Index);

    /**
     * 受控写接口：更新玩家现金（Cash）。
     *
     * 唯一合法调用方：经济(5) 系统经 Debit/Credit 公式结算后调用本 setter。
     * Debit/Credit 是经济5 侧封装，本 setter 是字段级写接口（story-005 Out of Scope：
     * Debit/Credit 公式归 economy epic，本 story 只暴露 SetCash 字段级 setter）。
     *
     * 实现语义（字段级 setter）：
     *   直接写 Cash = Value，不校验负数（破产判定归经济5/破产9）。
     *
     * @param Value 新现金值（由经济5 计算并传入）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|ControlledWrite",
        meta=(DisplayName="Set Cash (Controlled Write — Economy System Only)",
              ToolTip="受控写接口：唯一合法调用方=经济(5)。更新玩家现金值。"))
    void SetCash(int32 Value);

    /**
     * 受控写接口：更新入狱状态（bIsInJail）。
     *
     * 唯一合法调用方：事件格(7) 系统在入狱/出狱结算后调用本 setter。
     *
     * EJailReason 枚举（最小占位，完整枚举规则归事件格7）：
     *   - None=0：清除入狱状态时使用（bInJail=false 时传 None）
     *   - 事件7 追加完整原因值（ordinal >= 1，append-only）
     *
     * 实现语义（字段级 setter）：
     *   直接写 bIsInJail = bInJail；Reason 字段本 story 仅接收存储，
     *   完整 EJailReason 使用语义归事件格7（story-005 Out of Scope）。
     *
     * @param bInJail  是否进入入狱状态（true=入狱，false=出狱/清除）
     * @param Reason   入狱原因（最小占位枚举，完整值归事件7）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|ControlledWrite",
        meta=(DisplayName="Set Jail State (Controlled Write — Event System Only)",
              ToolTip="受控写接口：唯一合法调用方=事件格(7)。更新入狱状态与原因。"))
    void SetJailState(bool bInJail, EJailReason Reason = EJailReason::None);

    /**
     * 受控写接口：更新当前程掷骰上下文（CurrentRollContext holder）。
     *
     * 唯一合法调用方：事件格(7) 系统在事件额外掷骰时调用本 setter（仅事件额外掷骰，非每程）。
     * PULL 消费 + 程间非重入语义 → story-006（本 story 只声明 holder + setter）。
     *
     * 实现语义（字段级 setter）：
     *   直接写 CurrentRollContext = RollResult，存储最新的事件额外掷骰结果。
     *
     * @param RollResult 事件额外掷骰结果（由事件格7 传入 FDiceRollResult）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|ControlledWrite",
        meta=(DisplayName="Set Current Roll Context (Controlled Write — Event System Only)",
              ToolTip="受控写接口：唯一合法调用方=事件格(7)，仅事件额外掷骰时更新。"))
    void SetCurrentRollContext(const FDiceRollResult& RollResult);

    /**
     * 受控写接口：更新破产标志（bIsBankrupt）。
     *
     * 唯一合法调用方：本系统回合(2)，据破产(9) ResolveBankruptcy 返回
     * debtorEliminated=true 后由 HandlePlayerBankruptcy 调用。
     * 破产(9) 为 return-only 接口，绝不直写本字段（防 2↔9 回调环，AC-4）。
     *
     * 实现语义（字段级 setter）：
     *   直接写 bIsBankrupt = bNewBankrupt，破产标志单调递增（true 后通常不再回到 false）。
     *
     * @param bNewBankrupt 新破产状态（true=已破产移出，false=理论上不使用）
     */
    UFUNCTION(BlueprintCallable, Category="PlayerTurn|ControlledWrite",
        meta=(DisplayName="Set Bankrupt (Controlled Write — Turn System Only)",
              ToolTip="受控写接口：唯一合法调用方=本系统回合(2)，据破产(9) return-only 结果写入。"))
    void SetBankrupt(bool bNewBankrupt);
};
