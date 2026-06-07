// BoardCookGateCommandlet.h
// =============================================================================
// Commandlet：cook 前 bIsPlaceholderData 门控检测（Alpha gate）
//
// 用途：
//   在 cook 之前扫描所有 UBoardDataAsset 资产，
//   若任一资产的 bIsPlaceholderData==true，则 log Error + 返回退出码 1（阻断 cook）。
//   若全部资产 bIsPlaceholderData==false，则返回 0（cook 放行）。
//
// 语义（AC-4c Alpha gate）：
//   - MVP/nightly 阶段允许 bIsPlaceholderData==true（CI 不以 exitcode=1 为硬失败）
//   - Alpha 里程碑构建要求全部为 false（CI 在 Alpha 阶段以 exitcode=1 为硬失败）
//   - Commandlet 本身只如实报告，不自己决定"是否阻断"——由 CI pipeline 根据构建阶段解释退出码
//
// 调用命令（在工程根目录执行）：
//   "E:/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
//   "D:/ClaudeProject/rento/Claude-Code-Game-Studios/rento.uproject"
//   -run=BoardCookGate
//
// 参考：
//   - story-007 AC-4c（cook 门控 + Alpha gate 语义）
//   - ADR-0002 §Implementation Guidelines #3（cook 前 commandlet 检测 bIsPlaceholderData）
//   - ADR-0005 §Never（bIsPlaceholderData==true 的棋盘不得进 Alpha 构建）
// =============================================================================
#pragma once

// 仅在 Editor 构建中编译（Shipping 构建剥离）
#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BoardCookGateCommandlet.generated.h"

/**
 * UBoardCookGateCommandlet — cook 前扫描所有 UBoardDataAsset 的 bIsPlaceholderData
 *
 * 继承 UCommandlet，重写 Main() 方法。
 * 调用：UnrealEditor-Cmd rento.uproject -run=BoardCookGate
 *
 * 返回码：
 *   0 = 所有棋盘资产 bIsPlaceholderData==false，cook 可放行
 *   1 = 存在 bIsPlaceholderData==true 的资产，Alpha gate 阻断
 */
UCLASS()
class RENTO_API UBoardCookGateCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    // =========================================================================
    // UCommandlet 接口
    // =========================================================================

    /**
     * Commandlet 入口函数（UCommandlet 虚函数）。
     *
     * 枚举所有 UBoardDataAsset 资产，检测 bIsPlaceholderData 字段。
     * 有任意一个为 true 则报告并返回 1；全为 false 则返回 0。
     *
     * @param Params 命令行参数字符串（本 commandlet 不使用额外参数）
     * @return 0 = 全部资产通过；1 = 存在占位资产（Alpha gate 阻断）
     */
    virtual int32 Main(const FString& Params) override;
};

#endif // WITH_EDITOR
