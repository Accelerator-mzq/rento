// PlayerTurnSaveData.h
// =============================================================================
// 回合系统读档序列化容器（pt-008 / TR-turn-012/013 / ADR-0005）
//
// 形态（DV-1/DV-2，见 pt-008 brief）：
//   - URentoPlayerState 是 UObject → 序列化用 FPlayerStateRecord 值记录（绕开 UObject 指针）
//   - roll-context holder per-player → FDiceRollResult 序列化进每条记录
//
// 机制（G7，引擎实证）：
//   UE5.7 内置 SaveGameToMemory/SaveGameToSlot 不按 UPROPERTY(SaveGame) 过滤（全量序列化）。
//   字段仍标 SaveGame（产品路径对齐 + 表意），但当前不依赖其过滤。
//   round-trip 走 UGameplayStatics::SaveGameToMemory/LoadGameFromMemory（in-memory）。
//
// Out of Scope（本容器只装回合2 切片）：
//   棋盘 DA 引用 / 经济 Cash map / 地产 owner map / 建房 house_count / 事件格牌堆
//   + magic/version/checksum/manifest 完整性门 + 原子写盘 → save-load epic（ADR-0005 容器全量整合）。
//
// 规范依据：ADR-0005（存档契约）+ GDD CR-1（11 字段）+ AC-33/34（精确阶段 + FDiceRollResult）。
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlayerTurnTypes.h"     // EPlayerColor / EAIDifficulty / ETurnPhase
#include "DiceRngService.h"      // FDiceRollResult
#include "PlayerTurnSaveData.generated.h"

// =============================================================================
// FPlayerStateRecord — 单玩家序列化值记录（GDD CR-1 全 11 字段 + ConsecutiveDoubles + holder）
//   全字段标 UPROPERTY(SaveGame)（G7：产品路径对齐 + 表意；内置 save 不过滤但仍标）。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FPlayerStateRecord
{
    GENERATED_BODY()

    // ── GDD CR-1 全 11 字段（逐一，AC-2 含 JailTurnsServed）──
    UPROPERTY(SaveGame) int32         PlayerId         = -1;
    UPROPERTY(SaveGame) FText         DisplayName;
    UPROPERTY(SaveGame) EPlayerColor  TokenColor       = EPlayerColor::None;
    UPROPERTY(SaveGame) bool          bIsAI            = false;
    UPROPERTY(SaveGame) EAIDifficulty AIDifficulty     = EAIDifficulty::None;
    UPROPERTY(SaveGame) int32         CurrentTileIndex = 0;
    UPROPERTY(SaveGame) int32         Cash             = 0;
    UPROPERTY(SaveGame) bool          bIsInJail        = false;
    UPROPERTY(SaveGame) int32         JailTurnsServed  = 0;
    UPROPERTY(SaveGame) bool          bIsBankrupt      = false;
    UPROPERTY(SaveGame) int32         TurnOrderIndex   = -1;

    // ── 额外字段（AC-1：ConsecutiveDoubles 默认 0；CR-2 定序不累加）──
    UPROPERTY(SaveGame) int32         ConsecutiveDoubles = 0;

    // ── roll-context holder（DV-2 per-player；AC-2 完整 FDiceRollResult Die1/Die2/Total/bIsDouble）──
    UPROPERTY(SaveGame) FDiceRollResult CurrentRollContext;
};

// =============================================================================
// UPlayerTurnSaveData — 回合2 序列化切片容器（USaveGame 子类）
//   全字段标 UPROPERTY(SaveGame)。NewObject<>(GetTransientPackage()) headless 可构造。
// =============================================================================
UCLASS()
class RENTO_API UPlayerTurnSaveData : public USaveGame
{
    GENERATED_BODY()

public:
    /** 全玩家序列化记录（顺序与 PlayerStates 一致）。 */
    UPROPERTY(SaveGame) TArray<FPlayerStateRecord> PlayerRecords;

    /** 当前回合阶段（AC-1/2：精确阶段，switch 重入）。 */
    UPROPERTY(SaveGame) ETurnPhase CurrentPhase = ETurnPhase::TurnStart;

    /** 当前行动玩家 ID（-1=无活跃回合）。 */
    UPROPERTY(SaveGame) int32 CurrentActivePlayerId = -1;

    /** 锁定的双点入狱阈值（AC-2：锁定的 DOUBLES_JAIL_THRESHOLD）。 */
    UPROPERTY(SaveGame) int32 DoublesJailThreshold = 3;
};
