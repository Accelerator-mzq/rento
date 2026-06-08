// RuleProviderInterfaces.h
// =============================================================================
// 规则系统注入接缝（所有权6/建房8/经济5）—— story pt-007 簇C1（AC-2/4/7）
//
// 架构约束（ADR-0006/0007 + GDD CR-3.2/CR-8）：
//   - 纯 C++ 接口（非 UInterface）：headless 可 stub/spy 注入，无需 UHT（仿 IResolveBankruptcy）
//   - TSharedPtr 持有；回合2 ResolvePhase 经这些接缝拉取归属/建房/算租事实做聚合编排
//   - 回合2 只编排聚合与传递，不持有/不裁决归属或租金公式（归 6/8/5）
//   - 无 economy→8 / economy→6 反向边（CR-3.3/3.4；C2 spy 验证 zero 5→8 calls）
//
// 值语义 struct：全 POD，值拷贝跨接缝（property AC-30b：BuildOwnershipSnapshot 值拷贝）。
// C1 方法集（C2 将追加 Mortgage/ForcedSellNextBuilding/is_insolvent）：
//   IOwnershipProvider: BuildOwnershipSnapshot / GetBoardOwnership
//   IBuildingProvider:  GetHouseCount / GetPlayerBuildings / GetBuildingCost
//   IEconomyResolver:   CalculateRent / ExecutePurchase
// =============================================================================
#pragma once
#include "CoreMinimal.h"

// 所有权6 per-tile 归属快照（值拷贝，property AC-30b / GDD CR-3.2 ①）
struct RENTO_API FOwnershipSnapshot
{
    int32 TileIndex     = INDEX_NONE;
    int32 OwnerId       = INDEX_NONE;   // INDEX_NONE = 无主
    bool  bIsMortgaged  = false;
    bool  bIsMonopoly   = false;        // 该格所属组是否垄断
    int32 StationCount  = 0;
    int32 UtilityCount  = 0;
    int32 ColorGroup    = INDEX_NONE;
    int32 PurchasePrice = 0;            // 棋盘1 静态底数
    int32 MortgageValue = 0;            // 经济 F-9 口径
};

// 建房8 玩家建筑枚举条目（GDD CR-3.4 ① GetPlayerBuildings 返回）
struct RENTO_API FPlayerBuilding
{
    int32 TileIndex  = INDEX_NONE;
    int32 HouseCount = 0;
};

// 算租入口聚合参数（回合2 ResolvePhase 拼装传经济5，GDD CR-3.2 ③）
struct RENTO_API FRentInput
{
    int32              PayerId    = INDEX_NONE;
    int32              TileIndex  = INDEX_NONE;
    FOwnershipSnapshot Ownership;                  // 归属快照（owner/monopoly/mortgage/station/utility）
    int32              HouseCount = 0;             // 建房8 house_count（8 未实现注入 0）
    int32              DiceTotal  = 0;             // 当前程 PULL（CR-3.1，供 F-4 Utility 租）
};

// 所有权6 注入接缝
class RENTO_API IOwnershipProvider
{
public:
    virtual ~IOwnershipProvider() = default;
    /** 单格归属快照（值拷贝）。AC-7 收租路径用。 */
    virtual FOwnershipSnapshot BuildOwnershipSnapshot(int32 ViewerId, int32 TileIndex) = 0;
    /** 全盘归属（一次性，AssembleSnapshot 用，避免逐格 round-trip）。 */
    virtual TArray<FOwnershipSnapshot> GetBoardOwnership(int32 ViewerId) = 0;
};

// 建房8 注入接缝
class RENTO_API IBuildingProvider
{
public:
    virtual ~IBuildingProvider() = default;
    /** 某格房屋数（8 未实现 mock 返回缺省 0，GDD AC-49）。 */
    virtual int32 GetHouseCount(int32 TileIndex) = 0;
    /** 玩家全组合建筑枚举（NLV/AssembleSnapshot 用）。 */
    virtual TArray<FPlayerBuilding> GetPlayerBuildings(int32 PlayerId) = 0;
    /** 某格建筑成本（NLV 口径 floor(BuildingCost/2)）。 */
    virtual int32 GetBuildingCost(int32 TileIndex) = 0;
};

// 经济5 注入接缝
class RENTO_API IEconomyResolver
{
public:
    virtual ~IEconomyResolver() = default;
    /** 算租入口（回合2 PULL dice_total 后 push 聚合参数）。返回应付租金。 */
    virtual int32 CalculateRent(const FRentInput& Input) = 0;
    /** 执行购买（经济5 扣款 + 所有权6 转移的执行期校验+执行）。
     *  @return true=已购买；false=不可行（Cash<Price / 地产已被买）→ 框架视同不买（AC-38b）。 */
    virtual bool ExecutePurchase(int32 PlayerId, int32 TileIndex) = 0;
};
