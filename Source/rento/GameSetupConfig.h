// GameSetupConfig.h
// =============================================================================
// FGameSetupConfig — 新局配置最小 seam（story-004 AC-2 / ADR-0001 §接口名冻结）
//
// ⚠ IMPORTANT — 最小 seam 占位声明
//   本文件定义最小 seam USTRUCT，仅为冻结 StartNewGame(const FGameSetupConfig&) 入口名。
//   完整字段定义与语义 → pt-001 / 回合 epic 填充（Implementation Note 3）。
//   请勿在本文件中扩展字段语义——等待 pt-001 填充后合并到此处，或在 pt-001 中
//   #include 此文件并在同一 USTRUCT 上添加字段（届时删除此注释）。
//
// 设计约束（ADR-0001 §Impl Note 3）：
//   - 接口名 StartNewGame(const FGameSetupConfig&) 在此冻结
//   - pt-001 后续在本 struct 上填充字段，禁止 pt-001 重声明同名 struct
//   - 本 story 只建宿主，不定签名内部语义
//
// 规范依据：
//   - story-004 AC-2（StartNewGame 入口挂跨局宿主）
//   - ADR-0001 §Implementation Note 3（接口名冻结，struct 归属随回合 epic）
//   - control-manifest Foundation Layer §宿主与生命周期（ADR-0001）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameSetupConfig.generated.h"

/**
 * FGameSetupConfig — 新局配置结构体（最小 seam 占位）
 *
 * 当前字段：空（占位）。
 * 完整字段（玩家数量、AI 配置、棋盘选择、起始资金等）由 pt-001 / 回合 epic 填充。
 *
 * 命名规范（control-manifest）：F 前缀 PascalCase USTRUCT，BlueprintType 供 BP 可见。
 */
USTRUCT(BlueprintType)
struct RENTO_API FGameSetupConfig
{
    GENERATED_BODY()

    // =========================================================================
    // 最小 seam 占位注释：
    //
    // 以下字段由 pt-001 / 回合 epic 填充（示例，勿在此取消注释）：
    //
    //   UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameSetup")
    //   int32 PlayerCount = 2;                 // 玩家数量
    //
    //   UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameSetup")
    //   int32 RandomSeed = 0;                  // RNG 种子（ADR-0004）
    //
    //   UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameSetup")
    //   TSoftObjectPtr<UBoardDataAsset> BoardAsset;  // 棋盘 DA 引用（ADR-0002）
    //
    // =========================================================================
};
