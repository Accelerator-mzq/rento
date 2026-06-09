// EconomyTypes.h
// =============================================================================
// 经济系统(5) 事件契约类型 —— econ-002 / TR-econ-004/005/006/007 / ADR-0003（事件总线）
//
// 经济5 owner-held 的事件契约：EChangeReason 枚举 + 4 个 payload USTRUCT + 4 个 delegate 签名。
// 下游消费方（HUD16/VFX19/audio22）include 本头获取契约（不 include subsystem 实现头）。
//
// ADR-0003 纪律：
//   - 命名 On<PastTense>；payload `F<Event>Info`（全 blittable int32/enum，BlueprintType）。
//   - 去中心化 owner-held DYNAMIC_MULTICAST_DELEGATE + BlueprintAssignable（非集中 Bus）。
//   - 方向由消费方派生：EChangeReason 无方向位；正负向由 `NewCash-OldCash` delta 符号
//     + OnRentPaid 的 Payer/Payee 视角派生（不为收/付各发一事件）。
//   - OnBankruptcyDeclared 是经济5 现金侧信号（2 字段 Debtor/Creditor），与破产9 OnPlayerBankrupt
//     （3 字段）切分、并存不合并（Foundation EventBusDelegateContract TC8 AC-8）。
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "EconomyTypes.generated.h"

/**
 * EChangeReason —— 现金变动【类型】语境（9 值，无方向位）。
 *
 * 方向（正/负）由消费方从 OnCashChanged 的 `NewCash-OldCash` delta 符号派生，
 * 收/付方向由 OnRentPaid 的 Payer/Payee 视角派生（GDD 方向 reconcile 裁定，#22/#19）。
 * **禁** RentReceived/RentPaid/PassGo/JailFine/BuildCost 等方向子值/别名。
 *
 * 序列化纪律：uint8 基底，**append-only**（新 reason 只追加在末尾，既有值不改序）。
 */
UENUM(BlueprintType)
enum class EChangeReason : uint8
{
    Salary     = 0  UMETA(DisplayName = "Salary"),
    Rent       = 1  UMETA(DisplayName = "Rent"),
    Purchase   = 2  UMETA(DisplayName = "Purchase"),
    Mortgage   = 3  UMETA(DisplayName = "Mortgage"),
    Unmortgage = 4  UMETA(DisplayName = "Unmortgage"),
    Tax        = 5  UMETA(DisplayName = "Tax"),
    Build      = 6  UMETA(DisplayName = "Build"),
    Trade      = 7  UMETA(DisplayName = "Trade"),
    Bankruptcy = 8  UMETA(DisplayName = "Bankruptcy"),
};

/**
 * FCashChangedInfo —— OnCashChanged payload（AC-33）。
 * delta = NewCash - OldCash（符号定正负向，消费方派生）。
 */
USTRUCT(BlueprintType)
struct FCashChangedInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 PlayerId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 OldCash = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 NewCash = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    EChangeReason Reason = EChangeReason::Salary;
};

/**
 * FRentPaidInfo —— OnRentPaid payload（AC-34）。
 * Payer→Payee 视角定金币飞溅方向；TileIndex=地产格（非 Payee 所在格）。
 */
USTRUCT(BlueprintType)
struct FRentPaidInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 PayerId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 PayeeId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 Amount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 TileIndex = INDEX_NONE;
};

/**
 * FInsufficientFundsInfo —— OnInsufficientFunds payload（AC-35）。
 * 进入 Raising Funds 瞬态（供 UI 弹"需筹资"、回合阻塞）。
 */
USTRUCT(BlueprintType)
struct FInsufficientFundsInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 PlayerId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 AmountDue = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 AmountShort = 0;
};

/**
 * FBankruptcyDeclaredInfo —— OnBankruptcyDeclared payload（AC-36，2 字段）。
 * 经济5 现金侧转移完成信号；与破产9 OnPlayerBankrupt（3 字段）切分、并存不合并。
 */
USTRUCT(BlueprintType)
struct FBankruptcyDeclaredInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 DebtorId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Economy|Event")
    int32 CreditorId = INDEX_NONE;
};

/**
 * FNlvAssetEntry —— F-9 NLV 资产枚举输入 DTO（econ-008）。
 *   由回合2 ResolvePhase 聚合地产6(MortgageValue/bIsMortgaged)+建房8(HouseCount/BuildingCost)后传入 economy F-9，
 *   economy 不直读6/8（ADR-0006 防 5→8 环）。一条记录=一格可购买地（地产/车站/公用）。
 */
USTRUCT(BlueprintType)
struct FNlvAssetEntry
{
    GENERATED_BODY()

    /** 抵押价（地产6 base；未抵押地计入 NLV 的 MV 侧）。 */
    UPROPERTY(BlueprintReadWrite, Category="Economy|Nlv")
    int32 MortgageValue = 0;

    /** 是否已抵押（已抵押→MV 侧贡献 0，AC-26）。 */
    UPROPERTY(BlueprintReadWrite, Category="Economy|Nlv")
    bool bIsMortgaged = false;

    /** 该格建筑数 0..5（建房8；驱动卖回侧）。 */
    UPROPERTY(BlueprintReadWrite, Category="Economy|Nlv")
    int32 HouseCount = 0;

    /** 该格每栋建造成本（board base；卖回 = HouseCount × floor(BuildingCost/2)）。 */
    UPROPERTY(BlueprintReadWrite, Category="Economy|Nlv")
    int32 BuildingCost = 0;
};

// 4 个 owner-held DYNAMIC_MULTICAST_DELEGATE（单 payload USTRUCT 参，ADR-0003 / pt-004 FPhaseChangedInfo 先例）。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCashChangedSignature, const FCashChangedInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRentPaidSignature, const FRentPaidInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsufficientFundsSignature, const FInsufficientFundsInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBankruptcyDeclaredSignature, const FBankruptcyDeclaredInfo&, Info);
