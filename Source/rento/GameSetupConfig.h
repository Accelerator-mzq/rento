// GameSetupConfig.h
// =============================================================================
// FGameSetupConfig / FPlayerSetupEntry — 新局配置结构体（pt-001 填充完成）
//
// 历史：
//   FF-004 首建最小 seam（冻结接口名）；
//   pt-001 在此 struct 上填充完整字段（story Note 5/6），删除占位注释。
//
// 设计约束（ADR-0001 §Impl Note 3 + TR-turn-015）：
//   - 接口名 StartNewGame(const FGameSetupConfig&) 不变（已冻结）
//   - 本 struct 归属回合2（owner=player-turn epic），禁止重声明同名 struct
//   - BoardAsset 字段不做——棋盘选择归 board epic（story Out of Scope）
//   - 无硬编码玩家上限——Players.Num() 即 P，动态数组（AC-2 无硬编码 4）
//
// 规范依据：
//   - story pt-001 Implementation Note 5/6（FGameSetupConfig/FPlayerSetupEntry 字段）
//   - ADR-0001（宿主与生命周期）
//   - ADR-0005（枚举 append-only，struct 字段只增不改语义）
//   - TR-turn-015（开局入口结构体归属）
//   - control-manifest Foundation Layer §宿主与生命周期（ADR-0001）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "PlayerTurnTypes.h"  // EPlayerColor / EAIDifficulty 枚举
#include "GameSetupConfig.generated.h"

// =============================================================================
// FPlayerSetupEntry — 单个玩家的开局配置项（pt-001 story Note 6）
//
// 大厅/主菜单(20)装配后传入，开局时逐条映射为 URentoPlayerState 实体。
// USTRUCT(BlueprintType) 满足 BP 边界可见（裸 TArray 不可作 BP 边界传参）。
//
// 字段约定（GDD CR-1 + story Note 6）：
//   DisplayName   — 与 PlayerState.DisplayName:FText 口径一致
//   TokenColor    — 枚举唯一性由 UPlayerTurnSubsystem 开局时校验（AC-5）
//   bIsAI         — false 时 AIDifficulty 固定为 None
//   AIDifficulty  — 非 AI 时置 None（AC-1 默认值契约）
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FPlayerSetupEntry
{
    GENERATED_BODY()

    /** 玩家显示名（与 PlayerState.DisplayName:FText 口径一致，大厅配置传入） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup")
    FText DisplayName;

    /** 棋子颜色（开局唯一分配；EPlayerColor::None 视为未设置，开局校验拒绝） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup")
    EPlayerColor TokenColor = EPlayerColor::None;

    /** 是否为 AI 控制（false = 本地人类玩家） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup")
    bool bIsAI = false;

    /** AI 难度（非 AI 时固定为 None；AC-1 默认值契约） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup")
    EAIDifficulty AIDifficulty = EAIDifficulty::None;
};

// =============================================================================
// FGameSetupConfig — 新局完整配置（pt-001 填充，TR-turn-015 USTRUCT 归属）
//
// 大厅装配后由 UPersistentServicesSubsystem::StartNewGame 单次传入。
// 动态玩家数 Players.Num() 即 P，无硬编码上限——2/4 均由测试直接传（AC-2）。
//
// USTRUCT(BlueprintType) 满足 BP 边界传参（裸 TArray 不可作 BP 边界传参）。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FGameSetupConfig
{
    GENERATED_BODY()

    /**
     * 参与本局的玩家列表（尺寸即 P，无硬编码上限）。
     *
     * story Note 5：Players.Num() 即 P（AC-2 无硬编码 4）。
     * 开局拒绝路径（P<2/P>4）由 story-002 AC-27 实现，本 story 不防守。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup")
    TArray<FPlayerSetupEntry> Players;

    /**
     * 双点入狱阈值（连续双点 N 次入狱，开局锁定，缺省=3）。
     *
     * story Note 5：DoublesJailThreshold 开局锁定，缺省取本系统默认值（3）。
     * <=0 时由 UPlayerTurnSubsystem 使用系统默认值 3（ADR-0001 数据驱动）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup",
        meta=(ClampMin="1", ClampMax="10", ToolTip="连续双点几次入狱，0=使用系统默认(3)"))
    int32 DoublesJailThreshold = 3;

    /**
     * 定序平手最大重掷轮数（缺省=5）。
     *
     * story Note 5：MaxTiebreakRounds 开局锁定（平手重掷由 story-002 实现）。
     * <=0 时由 UPlayerTurnSubsystem 使用系统默认值 5。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup",
        meta=(ClampMin="1", ClampMax="20", ToolTip="定序平手最大重掷轮数，0=使用系统默认(5)"))
    int32 MaxTiebreakRounds = 5;

    /**
     * 本局权威 RNG 种子（per-match 单种子，ADR-0004 单一权威 FRandomStream）。
     *
     * 接线契约（DiceRngService.h L257）：生产每局种子真实来源为 StartNewGame 玩家配置，
     *   由 pt-001 在 OnWorldBeginPlay 之后（InitializeFromConfig 定序前）调用
     *   DiceService->SetSeed(RandomSeed) 传入——使开局定序掷骰可重放（ADR-0004 重放意图）。
     * 0 是合法种子（非哨兵，dice story-004），确定性测试/调试可复现；
     *   生产可由大厅生成非 0 随机值写入。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerTurn|Setup",
        meta=(ToolTip="本局权威 RNG 种子，定序前注入 DiceService；0 为合法种子"))
    int32 RandomSeed = 0;
};
