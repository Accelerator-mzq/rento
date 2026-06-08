// RuleProviderSpies.h
// =============================================================================
// 规则系统注入接缝 spy（所有权6/建房8/经济5）—— story pt-007 簇C1 测试用
//
// 架构约束（G3/G4）：
//   - 纯 C++ struct，无 UCLASS/UObject 继承（headless 可直接实例化，无需 UHT）
//   - MakeShared<FXxxSpy>() 局部 TSharedPtr 持有，禁 *new/AddToRoot
//   - 受控返回值 + 调用计数 + 记录最后一次参数（供 TC 断言精确验证 N-次 seam 语义）
// =============================================================================
#pragma once
#include "CoreMinimal.h"
#include "RuleProviderInterfaces.h"

// 所有权6 spy
struct FOwnershipProviderSpy final : public IOwnershipProvider
{
    TArray<FOwnershipSnapshot> BoardToReturn;                 // GetBoardOwnership 返回
    TMap<int32, FOwnershipSnapshot> PerTileToReturn;          // BuildOwnershipSnapshot 按 tile 返回
    int32 BuildOwnershipSnapshotCallCount = 0;
    int32 GetBoardOwnershipCallCount = 0;
    int32 LastBuildOwnershipViewer = INDEX_NONE;
    int32 LastBuildOwnershipTile   = INDEX_NONE;

    virtual FOwnershipSnapshot BuildOwnershipSnapshot(int32 ViewerId, int32 TileIndex) override
    {
        ++BuildOwnershipSnapshotCallCount;
        LastBuildOwnershipViewer = ViewerId;
        LastBuildOwnershipTile   = TileIndex;
        if (const FOwnershipSnapshot* Found = PerTileToReturn.Find(TileIndex)) { return *Found; }
        FOwnershipSnapshot Def; Def.TileIndex = TileIndex; return Def;
    }
    virtual TArray<FOwnershipSnapshot> GetBoardOwnership(int32 /*ViewerId*/) override
    {
        ++GetBoardOwnershipCallCount;
        return BoardToReturn;
    }
};

// 建房8 spy
struct FBuildingProviderSpy final : public IBuildingProvider
{
    TMap<int32, int32> HouseCountByTile;        // GetHouseCount 按 tile 返回（缺省 0）
    TMap<int32, int32> BuildingCostByTile;      // GetBuildingCost 按 tile 返回（缺省 0）
    TArray<FPlayerBuilding> PlayerBuildingsToReturn;
    int32 GetHouseCountCallCount = 0;
    int32 GetPlayerBuildingsCallCount = 0;
    int32 GetBuildingCostCallCount = 0;

    virtual int32 GetHouseCount(int32 TileIndex) override
    {
        ++GetHouseCountCallCount;
        const int32* F = HouseCountByTile.Find(TileIndex);
        return F ? *F : 0;
    }
    virtual TArray<FPlayerBuilding> GetPlayerBuildings(int32 /*PlayerId*/) override
    {
        ++GetPlayerBuildingsCallCount;
        return PlayerBuildingsToReturn;
    }
    virtual int32 GetBuildingCost(int32 TileIndex) override
    {
        ++GetBuildingCostCallCount;
        const int32* F = BuildingCostByTile.Find(TileIndex);
        return F ? *F : 0;
    }
};

// 经济5 spy
struct FEconomyResolverSpy final : public IEconomyResolver
{
    int32 RentToReturn = 0;                 // CalculateRent 返回（固定）
    TArray<int32> RentSequence;             // 若非空，按调用序返回（供 Rent_top 多值测试）
    bool  ExecutePurchaseResult = true;     // ExecutePurchase 返回
    int32 CalculateRentCallCount = 0;
    int32 ExecutePurchaseCallCount = 0;
    FRentInput LastRentInput;               // 记录最后一次算租参数

    virtual int32 CalculateRent(const FRentInput& Input) override
    {
        LastRentInput = Input;
        const int32 Idx = CalculateRentCallCount++;
        if (RentSequence.IsValidIndex(Idx)) { return RentSequence[Idx]; }
        return RentToReturn;
    }
    virtual bool ExecutePurchase(int32 /*PlayerId*/, int32 /*TileIndex*/) override
    {
        ++ExecutePurchaseCallCount;
        return ExecutePurchaseResult;
    }
};
