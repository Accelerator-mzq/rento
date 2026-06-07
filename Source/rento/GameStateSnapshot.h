// GameStateSnapshot.h
// =============================================================================
// FGameStateSnapshot + FTileSnapshotEntry — AI 只读聚合视图（story pt-007 / TR-turn-007/008）
//
// 架构约束（ADR-0006 裁定一 + AC-1 值语义不变式）：
//   ⚠ 此结构体 **仅含 POD/容器成员**，绝不内嵌任何 UObject 裸指针或可写引用。
//      这是 AC-1 / ADR-0006 IG#5 的架构红线：值语义天然可注入 mock 字面量
//      （headless 测试直接构造 FGameStateSnapshot 填字段，无需 mock 任何系统接口）。
//      若违反（内嵌 UObject*），GC 不追踪 USTRUCT 成员裸指针，
//      且深拷贝变浅拷贝——TC-1 值语义测试真 FAIL。
//
// 装配方（ADR-0006 裁定二 / ADR-0001）：
//   snapshot 由回合2（UPlayerTurnSubsystem / UWorldSubsystem 宿主）
//   在 AI 决策阶段开头一次性装配（AssembleSnapshot(int32 AiPlayerId)），
//   装配后决策期内冻结不变。AI 严禁自建 snapshot（ADR-0006 Forbidden）。
//
// 传参约定（ADR-0006 裁定三 / R5）：
//   所有 AI hook 经 const FGameStateSnapshot& 传入（零深拷贝）。
//
// 字段清单来源：ADR-0006 Key Interfaces 逐字照抄，勿增删字段。
//
// 规范依据：
//   - story pt-007 AC-1（值语义 USTRUCT，仅 POD/容器）
//   - ADR-0006（GameStateSnapshot 聚合契约）
//   - ADR-0001（宿主与生命周期）
//   - ADR-0007（AI 决策核心落 C++）
//   - control-manifest Core Layer §Required（值语义只读 USTRUCT）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameStateSnapshot.generated.h"

// =============================================================================
// FTileSnapshotEntry — 全盘 per-tile 明细条目
//
// 供需要逐格计算的 AI 决策：BlockingValue、均衡建造、owner map 遍历。
// 字段全部为 POD + 无 UObject 指针（AC-1 / ADR-0006 IG#5 值语义约束）。
//
// ⚠ append-only 纪律（ADR-0005）：仅在末尾追加新字段，禁止删除/重排已有字段。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FTileSnapshotEntry
{
    GENERATED_BODY()

    /** 格索引（对应棋盘布局；-1=未填充哨兵，对应 INDEX_NONE） */
    UPROPERTY() int32 TileIndex      = INDEX_NONE;

    /** 当前拥有者 PlayerId（-1=无主；全盘 owner map 供 AI 算 BoardDensity / BlockingValue） */
    UPROPERTY() int32 OwnerId        = INDEX_NONE;

    /** 当前房屋数量 [0,5]（建房8 口径；5=宾馆） */
    UPROPERTY() int32 HouseCount     = 0;

    /** 是否已抵押（抵押状态下不收租、不可建房） */
    UPROPERTY() bool  bIsMortgaged   = false;

    /** 该格所属组是否已被同一玩家垄断（group monopoly 标志；AI 算垄断租金倍率） */
    UPROPERTY() bool  bIsMonopoly    = false;

    /** 颜色分组 ID（-1=非可购地产，如公共事业/车站/特殊格） */
    UPROPERTY() int32 ColorGroup     = INDEX_NONE;

    /** 购买价（棋盘1 静态底数，经 Board DA；0=不可购买） */
    UPROPERTY() int32 PurchasePrice  = 0;

    /** 抵押价（MortgageValue = PurchasePrice / 2，经济5 口径） */
    UPROPERTY() int32 MortgageValue  = 0;

    /** 建房费用（BuildingCost，每栋房屋的购建价） */
    UPROPERTY() int32 BuildingCost   = 0;

    /**
     * 解押费用（UnmortgageCost = MortgageValue + ceil(MortgageValue/10)，经济5 口径）
     * 约为 MortgageValue × 1.1
     */
    UPROPERTY() int32 UnmortgageCost = 0;

    /**
     * 净可变现值（PreaggregatedNlv）— 回合2 装配期聚合 6/8（AI 不可自算）
     *
     * ⚠ ADR-0006 IG#3 / economy Risk-3：AI 严禁自算此字段（防 5→8 反向环）。
     *    装配方（回合2）聚合 Σ house_count × floor(BuildingCost/2) + MortgageValue(未抵押)。
     */
    UPROPERTY() int32 PreaggregatedNlv = 0;
};

// =============================================================================
// FGameStateSnapshot — AI 唯一读取面
//
// 值语义、决策期冻结、按 const& 传入三 hook。
// 无任何系统活指针（类型系统强制 AI 只读，物理阻断 AI 反向触达系统状态）。
//
// ⚠ AC-1 / ADR-0006 裁定一 不变式：
//    仅含 POD/容器成员（int32 / bool / TArray<FTileSnapshotEntry>）。
//    绝不内嵌任何 UObject 裸指针或可写引用。
//    测试 TC-1 验证深拷贝（改 Copy 字段不影响 Original）——若内嵌 UObject* 则浅拷贝致 TC-1 FAIL。
//
// ⚠ append-only 纪律（ADR-0005）：仅在末尾追加新字段，禁止删除/重排已有字段。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FGameStateSnapshot
{
    GENERATED_BODY()

    // ---- 决策主体 AI 自身 ----

    /** AI 自身 PlayerId（-1=未填充哨兵） */
    UPROPERTY() int32 SelfPlayerId    = INDEX_NONE;

    /** AI 当前现金余额 */
    UPROPERTY() int32 SelfCash        = 0;

    // ---- 全盘暴露视图（三 hook 共需，算 F-1 Buffer）----

    /**
     * 全盘对手未抵押地产单次租金最高（piecewise property_rent 口径，装配期预聚合）
     * 垄断×2 仅作用 house=0 的 base；不足两块取现有，无对手地产=0
     */
    UPROPERTY() int32 Rent_top1       = 0;

    /**
     * 全盘对手未抵押地产单次租金次高（同 Rent_top1 口径）
     * 不足两块取现有之和，无对手地产=0
     */
    UPROPERTY() int32 Rent_top2       = 0;

    // ---- 全局常量/底数 ----

    /** 开局起始金额（F-2 LiquidationDiscount 分母；经典规则 1500） */
    UPROPERTY() int32 StartingCash           = 1500;

    /** 经典棋盘格数（F-6 BoardDensity 分母；经典规则 40） */
    UPROPERTY() int32 BoardTileCountClassic  = 40;

    /** 缴保释金额（JAIL_BAIL_AMOUNT；经典规则 50） */
    UPROPERTY() int32 JailBailAmount         = 50;

    /** 在狱最大回合数（MaxJailTurns；超限后强制缴保释金出狱；经典规则 3） */
    UPROPERTY() int32 MaxJailTurns           = 3;

    // ---- DecideJailAction 专属 ----

    /** AI 是否持有出狱卡（Get Out of Jail Free） */
    UPROPERTY() bool  bHasJailCard    = false;

    /** AI 已在狱服刑回合数（JailTurnsServed，从 0 开始计） */
    UPROPERTY() int32 JailTurnsServed = 0;

    // ---- 全盘 per-tile 明细（owner map / 逐格计算）----

    /**
     * 全盘格明细数组（owner map + 逐格 POD 数据）
     *
     * 供 AI 做 BlockingValue/均衡建造等需要逐格数据的决策。
     * HeldInGroup/GroupSize/OppMaxInGroup 由 AI 从 Tiles 现算
     *（本就 per-tile 逐格，无跨系统聚合，符合 ADR-0006 决策点 B1 例外）。
     */
    UPROPERTY() TArray<FTileSnapshotEntry> Tiles;
};
