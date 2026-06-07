// BoardValidator.h
// =============================================================================
// 棋盘加载期完整性校验（拒绝类 + 警告类）+ 结构化错误/警告码
// story-005（拒绝类）+ story-006（警告类），TR-board-013，ADR-0002 §Implementation Guidelines (3)
//
// 内容：
//   [拒绝类 — story-005]
//   - EBoardValidationError — 拒绝类错误码枚举（BlueprintType，None=0 为成功哨兵）
//   - FBoardValidationError — 结构化错误 struct，含 TileIndex / 长度 / 越界值定位字段
//
//   [警告类 — story-006]
//   - EBoardValidationWarning — 警告类码枚举（BlueprintType，None=0 为成功哨兵）
//   - FBoardValidationWarning — 结构化警告 struct，含 TileIndex / ColorGroup / TileType / Count
//
//   [校验库]
//   - UBoardValidator — 纯校验静态函数库（UBlueprintFunctionLibrary）
//     拒绝类：ValidateTiles / ValidateBoard（fail-fast，返回单个 FBoardValidationError）
//     警告类：ValidateTilesWarnings / ValidateBoardWarnings（全收集，返回 TArray<FBoardValidationWarning>）
//
// 设计原则（ADR-0002 R6 — 载体解耦）：
//   核心函数 ValidateTiles/ValidateTilesWarnings 与载体 UBoardDataAsset 完全解耦，
//   测试直接传 TArray<FBoardTileData> fixture，无需 DA/AssetManager。
//
// 拒绝类校验语义 = Fail-Fast：返回第一个拒绝错误（Code==None 表示通过）。
// 警告类校验语义 = 全收集：遍历完整返回所有警告（空数组=无警告），不阻止加载。
//
// 拒绝类检查顺序（固定，测试 fixture 依赖此序）：
//   板级：BoardTooSmall → 索引覆盖性(Duplicate/Missing) → GoTileCountInvalid → Index0NotGo → NoJailTile
//        （GoTileCountInvalid 须先于 Index0NotGo：否则「0 个 Go」被 Index0NotGo 抢先，AC-23d 不可达）
//   逐格：PropertyMissingColorGroup → ColorGroupOnNonProperty → PurchasePriceInvalid →
//          MortgageExceedsPurchase → RentTable长度 → DiceMult长度 → DiceMult越界 →
//          DiceMultiplierTableNotAllowed → RentTableNotAllowed → RentArrayOnNonPurchasable →
//          EventDeckMissing → SalaryOnNonGoTile → TaxOnNonTaxTile
//
// 警告类收集顺序（确定性，全收集）：
//   逐格：EmptyDisplayName → MortgageRateHigh → RentTableNotMonotonic
//   聚合：CornerTileCountUnusual → BuildingCostMismatchInGroup → ColorGroupMemberCountMismatch
//
// Out of Scope（严守 story-005/006 边界）：
//   - 状态机推进（Loaded→Validated→Active/LoadFailed）→ story-002
//   - DA_Board_Classic40 实例 [Config] 测试 → story-007
//   - 不触碰 BoardDataAssetHost / DataAssetHostSubsystem / EHostLoadState
//
// 规范依据：
//   - ADR-0002（primary）— §Implementation Guidelines (3) / §Key Interfaces
//   - GDD board-data.md §Edge Cases（加载期完整性校验拒绝类 + 警告类）
//   - story-005 AC-18/19a/19b/20/21/22/22b/23/23b-j/32
//   - story-006 AC-24a/24b/24c/24d/35/36
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BoardDataAsset.h"
#include "BoardValidator.generated.h"

// =============================================================================
// EBoardValidationError — 拒绝类错误码枚举（story-005）
//
// ⚠ APPEND-ONLY 纪律：None=0 为成功哨兵，已有值不得重排。
//   story-006 警告类独立定义 EBoardValidationWarning，不在此枚举追加。
// =============================================================================
UENUM(BlueprintType)
enum class EBoardValidationError : uint8
{
    /** 校验通过（无错误）。值 0，作为成功哨兵 */
    None                         = 0  UMETA(DisplayName = "None"),

    /** AC-18：格子总数 N < 4，棋盘过小，环路无意义 */
    BoardTooSmall                = 1  UMETA(DisplayName = "Board Too Small"),

    /** AC-19a：TileIndex 序列存在缺号（某个 [0,N-1] 值未被任一格使用） */
    MissingTileIndex             = 2  UMETA(DisplayName = "Missing Tile Index"),

    /** AC-19b：TileIndex 序列存在重号（两格持有相同 TileIndex） */
    DuplicateTileIndex           = 3  UMETA(DisplayName = "Duplicate Tile Index"),

    /** AC-20：TileIndex==0 的格子 TileType != Go，出发格固定必须是 Go */
    Index0NotGo                  = 4  UMETA(DisplayName = "Index 0 Not Go"),

    /** AC-21：Property 格 ColorGroup == None，地产必须归属某色组 */
    PropertyMissingColorGroup    = 5  UMETA(DisplayName = "Property Missing Color Group"),

    /** AC-32：非 Property 格 ColorGroup != None，仅地产格可有色组（反向校验） */
    ColorGroupOnNonProperty      = 6  UMETA(DisplayName = "Color Group On Non-Property"),

    /**
     * AC-22/22b：RentTable 长度与格子类型不匹配
     * （Property 须 6，Railroad 须 4；错长下游越界读取）
     */
    RentTableLengthInvalid       = 7  UMETA(DisplayName = "Rent Table Length Invalid"),

    /** AC-23：可购买格（Property/Railroad/Utility）PurchasePrice <= 0 */
    PurchasePriceInvalid         = 8  UMETA(DisplayName = "Purchase Price Invalid"),

    /** AC-23b：Chance/CommunityChest 格 EventDeck == None，事件格必须绑定牌堆 */
    EventDeckMissing             = 9  UMETA(DisplayName = "Event Deck Missing"),

    /** AC-23c：存在 GoToJail 格但全盘无 Jail 格，GoToJail 目标未定义 */
    NoJailTile                   = 10 UMETA(DisplayName = "No Jail Tile"),

    /** AC-23d：Go 类型格数量 != 1（缺失或多于一个） */
    GoTileCountInvalid           = 11 UMETA(DisplayName = "Go Tile Count Invalid"),

    /** AC-23e：可购买格 MortgageValue > PurchasePrice（抵押所得不应超过买价） */
    MortgageExceedsPurchase      = 12 UMETA(DisplayName = "Mortgage Exceeds Purchase"),

    /**
     * AC-23f：Utility 格 DiceMultiplierTable.Num() != 2
     * （Utility 倍率数组固定 2 项：[单持倍率, 双持倍率]）
     */
    DiceMultiplierLengthInvalid  = 13 UMETA(DisplayName = "Dice Multiplier Length Invalid"),

    /** AC-23g-a：Property/Railroad 格带非空 DiceMultiplierTable（字段误填，反向校验） */
    DiceMultiplierTableNotAllowed= 14 UMETA(DisplayName = "Dice Multiplier Table Not Allowed"),

    /** AC-23g-b：Utility 格带非空 RentTable（字段误填，反向校验） */
    RentTableNotAllowed          = 15 UMETA(DisplayName = "Rent Table Not Allowed"),

    /**
     * AC-23g-c：非可购买格（Chance/CommunityChest/Tax/Go/Jail/FreeParking/GoToJail）
     * 带非空 RentTable 或非空 DiceMultiplierTable（完全不可购买格不应携带任何租金/倍率数组）
     */
    RentArrayOnNonPurchasable    = 16 UMETA(DisplayName = "Rent Array On Non-Purchasable"),

    /** AC-23h：非 Go 格 SalaryAmount != 0（SalaryAmount 仅 Go 格适用，反向校验） */
    SalaryOnNonGoTile            = 17 UMETA(DisplayName = "Salary On Non-Go Tile"),

    /** AC-23i：非 Tax 格 TaxAmount != 0（TaxAmount 仅 Tax 格适用，反向校验） */
    TaxOnNonTaxTile              = 18 UMETA(DisplayName = "Tax On Non-Tax Tile"),

    /**
     * AC-23j：Utility 格 DiceMultiplierTable 某元素 < 1 或 > DICE_MULT_MAX
     * 倍率须为正整数；上界防经济(5) Utility 租「骰点(≤12) × mult」int32 溢出
     */
    DiceMultiplierOutOfRange     = 19 UMETA(DisplayName = "Dice Multiplier Out Of Range"),

    // ⚠ 新增拒绝类错误码追加于此行之后，不改已有值
};

// =============================================================================
// FBoardValidationError — 结构化错误（拒绝类，story-005）
//
// 所有字段标 UPROPERTY(BlueprintReadOnly)，供地图编辑器(26) UI 后续消费。
// 板级错误（BoardTooSmall / NoJailTile / GoTileCountInvalid）TileIndex=-1（无定位格）。
// 长度错误（RentTableLengthInvalid / DiceMultiplierLengthInvalid）填 Expected/ActualLength。
// 越界元素错误（DiceMultiplierOutOfRange）填 OffendingValue。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FBoardValidationError
{
    GENERATED_BODY()

    /** 错误码；None 表示校验通过 */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    EBoardValidationError Code = EBoardValidationError::None;

    /**
     * 定位：触发错误的格子 TileIndex。
     * 板级错误（BoardTooSmall / GoTileCountInvalid / NoJailTile）填 -1（无单格定位）。
     * MissingTileIndex 填缺失的索引值本身；DuplicateTileIndex 填重号的索引值本身。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    int32 TileIndex = -1;

    /**
     * 期望长度（RentTableLengthInvalid / DiceMultiplierLengthInvalid 时有效）。
     * AC-22：Property 应为 6；AC-22b：Railroad 应为 4；AC-23f：Utility 应为 2。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    int32 ExpectedLength = -1;

    /**
     * 实际长度（RentTableLengthInvalid / DiceMultiplierLengthInvalid 时有效）。
     * 与 ExpectedLength 对照，便于编辑器提示「期望 6，实际 4」。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    int32 ActualLength = -1;

    /**
     * 越界元素值（DiceMultiplierOutOfRange 时有效）。
     * 填触发越界的那个元素值，便于编辑器提示具体违规值。
     * 合法区间为 [1, UBoardValidator::DICE_MULT_MAX]。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    int32 OffendingValue = 0;

    /**
     * 便利方法：返回 true 表示校验通过（Code == None）。
     * 调用方可直接 if (Result.IsValid()) 判断。
     */
    bool IsValid() const { return Code == EBoardValidationError::None; }
};

// =============================================================================
// EBoardValidationWarning — 警告类码枚举（story-006）
//
// ⚠ APPEND-ONLY 纪律：None=0 为成功哨兵，已有值不得重排。
//   与拒绝类 EBoardValidationError 独立，两类型语义不同：
//     - 拒绝类：加载失败（fail-fast，返回单个 FBoardValidationError）
//     - 警告类：加载成功（全收集，返回 TArray<FBoardValidationWarning>）
// =============================================================================
UENUM(BlueprintType)
enum class EBoardValidationWarning : uint8
{
    /** 无警告（成功哨兵）。值 0，append-only 基准 */
    None                           = 0  UMETA(DisplayName = "None"),

    /** AC-24a：格子 DisplayName 为空（便于编辑器草稿态，回退显示 "Tile {index}"） */
    EmptyDisplayName               = 1  UMETA(DisplayName = "Empty Display Name"),

    /** AC-24b：某色组成员格数与调用方声明（ExpectedGroupSizes）不符 */
    ColorGroupMemberCountMismatch  = 2  UMETA(DisplayName = "Color Group Member Count Mismatch"),

    /** AC-24c：可购买格 MortgageValue > floor(PurchasePrice × 0.6)（超 60% 抵押率） */
    MortgageRateHigh               = 3  UMETA(DisplayName = "Mortgage Rate High"),

    /** AC-24d：Jail/FreeParking/GoToJail 任一类型格数 ≠ 1（自定义盘允许但经典盘异常） */
    CornerTileCountUnusual         = 4  UMETA(DisplayName = "Corner Tile Count Unusual"),

    /** AC-35：Property 格 RentTable 存在 RentTable[i] > RentTable[i+1]（非单调不减） */
    RentTableNotMonotonic          = 5  UMETA(DisplayName = "Rent Table Not Monotonic"),

    /** AC-36：同一色组（非 None）内各格 BuildingCost 不完全相同 */
    BuildingCostMismatchInGroup    = 6  UMETA(DisplayName = "Building Cost Mismatch In Group"),

    // ⚠ 新增警告码追加于此行之后，不改已有值
};

// =============================================================================
// FBoardValidationWarning — 结构化警告（警告类，story-006）
//
// 所有字段标 UPROPERTY(BlueprintReadOnly)，供地图编辑器(26) UI 后续消费。
// 定位字段语义：
//   - TileIndex：per-tile 警告填该格索引（EmptyDisplayName/MortgageRateHigh/RentTableNotMonotonic）；
//                group/corner 级警告填 -1（ColorGroupMemberCountMismatch/BuildingCostMismatchInGroup/CornerTileCountUnusual）
//   - ColorGroup：色组级警告填涉及色组（ColorGroupMemberCountMismatch/BuildingCostMismatchInGroup）；其余 None
//   - TileType  ：CornerTileCountUnusual 填涉及角格类型；其余保持默认（ETileType::Property，无语义）
//   - Count     ：CornerTileCountUnusual 填该类型实际数量；ColorGroupMemberCountMismatch 填实际成员数；其余 -1
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FBoardValidationWarning
{
    GENERATED_BODY()

    /** 警告码；None 表示无警告（不应出现在结果数组中，仅作默认值哨兵） */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    EBoardValidationWarning Code = EBoardValidationWarning::None;

    /**
     * 定位：触发警告的格子 TileIndex。
     * per-tile 警告（EmptyDisplayName / MortgageRateHigh / RentTableNotMonotonic）填该格 TileIndex。
     * group/corner 级警告（ColorGroupMemberCountMismatch / BuildingCostMismatchInGroup
     *   / CornerTileCountUnusual）填 -1（无单格定位）。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    int32 TileIndex = -1;

    /**
     * 涉及色组（ColorGroupMemberCountMismatch / BuildingCostMismatchInGroup 时有效）。
     * 其余警告类型填 None。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    EColorGroup ColorGroup = EColorGroup::None;

    /**
     * 涉及角格类型（CornerTileCountUnusual 时有效，填 Jail/FreeParking/GoToJail 之一）。
     * 其余警告类型此字段无语义（保持默认值 ETileType::Property）。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    ETileType TileType = ETileType::Property;

    /**
     * 实际数量。
     * CornerTileCountUnusual：填该类型的实际格子数量（期望 1，实际 Count）。
     * ColorGroupMemberCountMismatch：填该色组实际成员数（与 ExpectedGroupSizes 中声明值对比）。
     * 其余警告类型填 -1。
     */
    UPROPERTY(BlueprintReadOnly, Category = "Board|Validation")
    int32 Count = -1;
};

// =============================================================================
// UBoardValidator — 棋盘校验纯函数库（UBlueprintFunctionLibrary）
//
// 核心设计（ADR-0002 R6 — 载体解耦）：
//   ValidateTiles() 接受 TArray<FBoardTileData> 引用，与 UBoardDataAsset 完全解耦。
//   测试直接构造 TArray<FBoardTileData> fixture 调用，无需 DA / AssetManager。
//
// DICE_MULT_MAX 不变式：
//   12 × DICE_MULT_MAX ≤ MAX_int32（2,147,483,647）
//   1,000,000 × 12 = 12,000,000 ≪ 2,147,483,647，裕度约 178 倍。
//   骰点最大值 12（两颗骰子），Utility 租 = 骰点 × DiceMultiplierTable[i]（经济 F-4）。
//   static_assert 在编译期守住此不变式，防止后续调整 DICE_MULT_MAX 时破坏约束。
// =============================================================================
UCLASS()
class RENTO_API UBoardValidator : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 常量：Utility 骰点倍率上界
    // =========================================================================

    /**
     * Utility 格 DiceMultiplierTable 元素合法上界（AC-23j）。
     *
     * 数值依据（ADR-0002 §Implementation Notes 4）：
     *   骰点最大值 = 12（两颗骰子各 6）。
     *   经济(5) Utility 租公式（F-4）= 骰点 × mult，结果须在 int32 范围内。
     *   12 × 1,000,000 = 12,000,000 ≤ INT32_MAX（约 178 倍裕度）。
     *   初值与 SalaryAmount 上界（经济 OQ-Econ-10）并轨裁定，此处先取 1,000,000。
     */
    static constexpr int32 DICE_MULT_MAX = 1000000;

    // =========================================================================
    // 拒绝类校验函数（story-005，fail-fast，载体解耦，ADR-0002 R6）
    // =========================================================================

    /**
     * 校验 TileData 数组是否满足所有拒绝类规则。
     *
     * 语义：Fail-Fast——返回第一个拒绝错误；全部通过则 Code==None。
     * 检查顺序固定（见文件头注释），测试 fixture 依赖此序。
     *
     * @param Tiles 待校验的格子数据数组（由调用方传入，可来自 DA 或测试 fixture）
     * @return FBoardValidationError 结构化错误（IsValid()==true 表示通过）
     */
    static FBoardValidationError ValidateTiles(const TArray<FBoardTileData>& Tiles);

    /**
     * 校验 UBoardDataAsset 是否满足所有拒绝类规则（Blueprint 入口）。
     *
     * 实现：转发 Board->Tiles（Board 为 null 时传空数组，N=0 → BoardTooSmall，合理兜底）。
     * 供地图编辑器(26) / Blueprint 调用；C++ 单元测试优先使用 ValidateTiles()。
     *
     * @param Board 待校验的棋盘数据资产（可为 null，null → BoardTooSmall）
     * @return FBoardValidationError 结构化错误（IsValid()==true 表示通过）
     */
    UFUNCTION(BlueprintCallable, Category = "Board|Validation")
    static FBoardValidationError ValidateBoard(const UBoardDataAsset* Board);

    // =========================================================================
    // 警告类校验函数（story-006，全收集，载体解耦，ADR-0002 R6）
    // =========================================================================

    /**
     * 收集 TileData 数组的全部警告类问题。
     *
     * 语义：全收集（非 fail-fast）——遍历完整个数组，返回所有警告。
     *   空数组返回 = 无任何警告（干净盘）。
     *   一个棋盘可同时产生多条警告（如同时有空名称格 + 抵押率偏高格）。
     *
     * 警告不拒绝加载：此函数不涉及 LoadFailed 状态（状态机推进 → story-002）。
     * 纯函数：无 UE_LOG 副作用，无全局状态修改。
     *
     * @param Tiles              待校验的格子数据数组（由调用方传入）
     * @param ExpectedGroupSizes 调用方声明的各色组期望成员数（AC-24b）。
     *                           空 map → 跳过色组成员数检查（自定义盘允许，GDD 裁定）。
     * @return TArray<FBoardValidationWarning> 所有警告（空数组=无警告）
     */
    static TArray<FBoardValidationWarning> ValidateTilesWarnings(
        const TArray<FBoardTileData>& Tiles,
        const TMap<EColorGroup, int32>& ExpectedGroupSizes = TMap<EColorGroup, int32>());

    /**
     * 收集 UBoardDataAsset 的全部警告类问题（Blueprint 入口）。
     *
     * 实现：转发 Board->Tiles（Board 为 null 时返回空数组，无警告，合理兜底）。
     * 供地图编辑器(26) / Blueprint 调用；C++ 单元测试优先使用 ValidateTilesWarnings()。
     *
     * @param Board              待校验的棋盘数据资产（可为 null，null → 空警告数组）
     * @param ExpectedGroupSizes 调用方声明的各色组期望成员数（AC-24b，空 map 跳过检查）
     * @return TArray<FBoardValidationWarning> 所有警告（空数组=无警告）
     */
    UFUNCTION(BlueprintCallable, Category = "Board|Validation")
    static TArray<FBoardValidationWarning> ValidateBoardWarnings(
        const UBoardDataAsset* Board,
        const TMap<EColorGroup, int32>& ExpectedGroupSizes);
};

// =============================================================================
// 编译期不变式：12 × DICE_MULT_MAX ≤ MAX_int32
// 保证经济(5) Utility 租「骰点(≤12) × mult」不溢出 int32（ADR-0002 §Notes 4）。
// 若后续调整 DICE_MULT_MAX 超出安全范围，此 static_assert 会在编译期报错。
// =============================================================================
static_assert(
    static_cast<int64>(12) * UBoardValidator::DICE_MULT_MAX <= MAX_int32,
    "DICE_MULT_MAX 过大：12 * DICE_MULT_MAX 将溢出 int32，"
    "会破坏经济(5) Utility 租公式「骰点(<=12) x mult」的安全性。"
    "请减小 DICE_MULT_MAX 或升级经济公式为 int64。"
);
