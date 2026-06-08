// EconomyEventSpy.h
// =============================================================================
// UEconomyEventSpy —— econ-001 事件 spy：计数 OnCashChanged / OnInsufficientFunds。
// 文件作用域 UCLASS（G3：DYNAMIC delegate spy 必 UCLASS），AddDynamic 绑定。
// 占位签名与 EconomyCashSubsystem.h 的 FOnCashChangedSignature/FOnInsufficientFundsSignature 对齐。
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EconomyEventSpy.generated.h"

UCLASS()
class UEconomyEventSpy : public UObject
{
    GENERATED_BODY()

public:
    // —— OnCashChanged ——
    UPROPERTY()
    int32 CashChangedCount = 0;

    UPROPERTY()
    int32 LastPlayerId = -1;

    UPROPERTY()
    int32 LastOldCash = -1;

    UPROPERTY()
    int32 LastNewCash = -1;

    // —— OnInsufficientFunds ——
    UPROPERTY()
    int32 InsufficientCount = 0;

    UPROPERTY()
    int32 LastDue = -1;

    UPROPERTY()
    int32 LastShort = -1;

    UFUNCTION()
    void HandleCashChanged(int32 PlayerId, int32 OldCash, int32 NewCash)
    {
        ++CashChangedCount;
        LastPlayerId = PlayerId;
        LastOldCash  = OldCash;
        LastNewCash  = NewCash;
    }

    UFUNCTION()
    void HandleInsufficientFunds(int32 PlayerId, int32 AmountDue, int32 AmountShort)
    {
        ++InsufficientCount;
        LastPlayerId = PlayerId;
        LastDue      = AmountDue;
        LastShort    = AmountShort;
    }
};
