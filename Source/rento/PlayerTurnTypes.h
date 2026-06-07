// PlayerTurnTypes.h
// =============================================================================
// 回合系统公共枚举定义（story pt-001 / TR-turn-001 / TR-turn-014）
//
// 包含：
//   EPlayerColor — 棋子颜色枚举（8 色，开局唯一分配）
//   EAIDifficulty — AI 难度枚举（None/Casual/Normal/Sharp）
//
// 设计约束（ADR-0005 / TR-turn-014）：
//   - 枚举基底必须为 uint8，确保 BP 可见 + append-only 纪律
//   - ordinal 值显式标注，禁止重排（存档序列化依赖整数索引不变）
//   - append-only：只在末尾追加新值，绝不删除/重排已有值（破坏存档）
//   - 含 UMETA(DisplayName) 供编辑器显示
//
// 规范依据：
//   - ADR-0001（player-turn 系统宿主与生命周期）
//   - ADR-0005（枚举 append-only，整数索引不可重排）
//   - TR-turn-014（EPlayerColor/EAIDifficulty 存档序列化约束）
//   - control-manifest Foundation Layer §存档序列化 (ADR-0005)
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "PlayerTurnTypes.generated.h"

// =============================================================================
// EPlayerColor — 棋子颜色枚举（8 色预留 + None 哨兵）
//
// 开局唯一分配，分配后恒定（TR-turn-001 / GDD CR-1 TokenColor 字段）。
// None=0 为哨兵/未分配值，不应出现在对局中（AC-5 唯一性校验门控此值）。
// 实色 1..8 映射到 8 种经典大富翁棋子颜色。
//
// ⚠ append-only 纪律（ADR-0005 / TR-turn-014）：
//   整数索引 0..8 已冻结，新色只能追加 9..N（存档依赖此整数值不变）。
// =============================================================================
UENUM(BlueprintType)
enum class EPlayerColor : uint8
{
    /** 哨兵/未分配（ordinal=0，不出现在对局玩家中） */
    None      = 0  UMETA(DisplayName = "None"),

    /** 红色（ordinal=1） */
    Red       = 1  UMETA(DisplayName = "Red"),

    /** 蓝色（ordinal=2） */
    Blue      = 2  UMETA(DisplayName = "Blue"),

    /** 绿色（ordinal=3） */
    Green     = 3  UMETA(DisplayName = "Green"),

    /** 黄色（ordinal=4） */
    Yellow    = 4  UMETA(DisplayName = "Yellow"),

    /** 紫色（ordinal=5） */
    Purple    = 5  UMETA(DisplayName = "Purple"),

    /** 橙色（ordinal=6） */
    Orange    = 6  UMETA(DisplayName = "Orange"),

    /** 青色/青绿（ordinal=7） */
    Cyan      = 7  UMETA(DisplayName = "Cyan"),

    /** 粉色（ordinal=8） */
    Pink      = 8  UMETA(DisplayName = "Pink"),

    // ⚠ 新颜色只能追加到此处（ordinal >= 9），禁止删除/重排以上值。
};

// =============================================================================
// EAIDifficulty — AI 难度枚举（None/Casual/Normal/Sharp）
//
// 非 AI 玩家固定为 None（GDD CR-1 AIDifficulty 字段语义）。
// AI 系统(10) 消费此字段决策强度差异（GDD CR-1 注释"预留,AI系统10消费"）。
//
// ⚠ append-only 纪律（ADR-0005 / TR-turn-014）：
//   整数索引 0..3 已冻结，新难度只能追加 4..N。
// =============================================================================
UENUM(BlueprintType)
enum class EAIDifficulty : uint8
{
    /** 非 AI 玩家或难度未设置（ordinal=0） */
    None    = 0  UMETA(DisplayName = "None"),

    /** 休闲难度（ordinal=1）：简单策略，适合新手 */
    Casual  = 1  UMETA(DisplayName = "Casual"),

    /** 普通难度（ordinal=2）：标准策略 */
    Normal  = 2  UMETA(DisplayName = "Normal"),

    /** 锐利难度（ordinal=3）：进阶策略，积极买地收租 */
    Sharp   = 3  UMETA(DisplayName = "Sharp"),

    // ⚠ 新难度只能追加到此处（ordinal >= 4），禁止删除/重排以上值。
};
