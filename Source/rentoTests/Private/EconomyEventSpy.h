// EconomyEventSpy.h
// =============================================================================
// UEconomyEventSpy —— 经济事件 spy：计数 + 记录 OnCashChanged / OnRentPaid /
//   OnInsufficientFunds / OnBankruptcyDeclared（econ-002 USTRUCT payload 版）。
// 文件作用域 UCLASS（G3：DYNAMIC delegate spy 必 UCLASS），AddDynamic 绑定。
// 处理函数签名与 EconomyTypes.h 的 4 个 F*Info payload delegate 对齐。
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EconomyTypes.h"            // 4 个 F*Info payload 类型
#include "EconomyEventSpy.generated.h"

UCLASS()
class UEconomyEventSpy : public UObject
{
    GENERATED_BODY()

public:
    // —— OnCashChanged ——
    UPROPERTY() int32 CashChangedCount = 0;
    UPROPERTY() int32 LastPlayerId = -1;
    UPROPERTY() int32 LastOldCash = -1;
    UPROPERTY() int32 LastNewCash = -1;
    UPROPERTY() EChangeReason LastReason = EChangeReason::Salary;

    // —— OnRentPaid ——
    UPROPERTY() int32 RentPaidCount = 0;
    UPROPERTY() int32 LastPayerId = -1;
    UPROPERTY() int32 LastPayeeId = -1;
    UPROPERTY() int32 LastRentAmount = -1;
    UPROPERTY() int32 LastTileIndex = -1;

    // —— OnInsufficientFunds ——
    UPROPERTY() int32 InsufficientCount = 0;
    UPROPERTY() int32 LastDue = -1;
    UPROPERTY() int32 LastShort = -1;

    // —— OnBankruptcyDeclared ——
    UPROPERTY() int32 BankruptcyCount = 0;
    UPROPERTY() int32 LastDebtorId = -1;
    UPROPERTY() int32 LastCreditorId = -1;

    UFUNCTION()
    void HandleCashChanged(const FCashChangedInfo& Info)
    {
        ++CashChangedCount;
        LastPlayerId = Info.PlayerId;
        LastOldCash  = Info.OldCash;
        LastNewCash  = Info.NewCash;
        LastReason   = Info.Reason;
    }

    UFUNCTION()
    void HandleRentPaid(const FRentPaidInfo& Info)
    {
        ++RentPaidCount;
        LastPayerId    = Info.PayerId;
        LastPayeeId    = Info.PayeeId;
        LastRentAmount = Info.Amount;
        LastTileIndex  = Info.TileIndex;
    }

    UFUNCTION()
    void HandleInsufficientFunds(const FInsufficientFundsInfo& Info)
    {
        ++InsufficientCount;
        LastPlayerId = Info.PlayerId;
        LastDue      = Info.AmountDue;
        LastShort    = Info.AmountShort;
    }

    UFUNCTION()
    void HandleBankruptcyDeclared(const FBankruptcyDeclaredInfo& Info)
    {
        ++BankruptcyCount;
        LastDebtorId   = Info.DebtorId;
        LastCreditorId = Info.CreditorId;
    }
};
