// GenerateClassic40Commandlet.h
// =============================================================================
// Commandlet：程序化生成 DA_Board_Classic40 资产
//
// 用途：
//   在 Editor 环境中程序化构造经典大富翁 40 格棋盘的 UBoardDataAsset 实例，
//   按 GDD CR-6 布局 + 公开参考数值 temp-fill 全 40 格，
//   并保存为 /Game/Boards/DA_Board_Classic40.uasset。
//
// 调用命令（在工程根目录执行）：
//   "E:/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
//   "D:/ClaudeProject/rento/Claude-Code-Game-Studios/rento.uproject"
//   -run=GenerateClassic40
//
// 参考：
//   - story-007 AC-3/4-asset/4b/4c/5/6/7/7b
//   - ADR-0002 §Migration Plan Step 4（temp-fill + bIsPlaceholderData=true）
//   - GDD CR-6 经典 40 格布局 + temp-fill 公开参考值
//
// 注意：
//   此头文件被 #if WITH_EDITOR 包裹——Shipping 构建完全不编译此类。
//   Commandlet 在 Editor target 下由引擎自动发现（-run=ClassName 无需 "Commandlet" 后缀）。
// =============================================================================
#pragma once

// 仅在 Editor 构建中编译（Shipping 构建剥离）
#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BoardDataAsset.h"   // 方法签名用 FBoardTileData / ETileType / EColorGroup / ETileAction / EEventDeck
#include "GenerateClassic40Commandlet.generated.h"

/**
 * UGenerateClassic40Commandlet — 程序化生成 DA_Board_Classic40 棋盘资产
 *
 * 继承 UCommandlet，重写 Main() 方法。
 * 调用：UnrealEditor-Cmd rento.uproject -run=GenerateClassic40
 *
 * 成功返回 0，失败返回 1（可作为 CI/构建门控的退出码）。
 */
UCLASS()
class RENTO_API UGenerateClassic40Commandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    // =========================================================================
    // UCommandlet 接口
    // =========================================================================

    /**
     * Commandlet 入口函数（UCommandlet 虚函数）。
     *
     * 程序化构造 UBoardDataAsset，填入 GDD CR-6 布局与公开参考数值，
     * 保存到 /Game/Boards/DA_Board_Classic40。
     *
     * @param Params 命令行参数字符串（本 commandlet 不使用额外参数）
     * @return 0 = 成功；1 = 失败（资产创建或保存出错）
     */
    virtual int32 Main(const FString& Params) override;

private:
    // =========================================================================
    // 内部辅助：构造各类格子数据
    // =========================================================================

    /**
     * 构造单个 Property 格数据（地产格）。
     *
     * @param Index      格子序号（0-based）
     * @param Name       IP 安全占位显示名
     * @param Group      色组枚举
     * @param Price      购买价格
     * @param RentValues 租金表（6 项：无房/1-4房/酒店）
     * @param BuildCost  每栋建筑成本
     * @param Mortgage   抵押价值
     * @return 填好的 FBoardTileData
     */
    static FBoardTileData MakePropertyTile(
        int32 Index,
        const FText& Name,
        EColorGroup Group,
        int32 Price,
        TArray<int32> RentValues,
        int32 BuildCost,
        int32 Mortgage);

    /**
     * 构造单个 Railroad 格数据（铁路格）。
     *
     * @param Index    格子序号
     * @param Name     IP 安全占位显示名
     * @return 填好的 FBoardTileData（RentTable 4 项，标准 25/50/100/200）
     */
    static FBoardTileData MakeRailroadTile(int32 Index, const FText& Name);

    /**
     * 构造单个 Utility 格数据（公用事业格）。
     *
     * @param Index 格子序号
     * @param Name  IP 安全占位显示名
     * @return 填好的 FBoardTileData（DiceMultiplierTable={4,10}，RentTable 空）
     */
    static FBoardTileData MakeUtilityTile(int32 Index, const FText& Name);

    /**
     * 构造角格/事件格/税格/Go 格等特殊格。
     *
     * @param Index        格子序号
     * @param Type         格子类型（Go/Jail/FreeParking/GoToJail/Tax/Chance/CommunityChest）
     * @param Name         IP 安全占位显示名
     * @param TaxAmt       税额（Tax 格用）
     * @param SalaryAmt    薪资（Go 格用）
     * @param Action       特殊动作
     * @param Deck         事件牌堆（Chance/CommunityChest 格用）
     * @return 填好的 FBoardTileData
     */
    static FBoardTileData MakeSpecialTile(
        int32 Index,
        ETileType Type,
        const FText& Name,
        int32 TaxAmt = 0,
        int32 SalaryAmt = 0,
        ETileAction Action = ETileAction::None,
        EEventDeck Deck = EEventDeck::None);
};

#endif // WITH_EDITOR
