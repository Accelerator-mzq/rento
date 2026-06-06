// BoardDataAsset.h
// =============================================================================
// 棋盘数据资产定义（story-001，ADR-0002 §Key Interfaces 逐字实现）
//
// 内容：
//   - ETileType    — 格子类型枚举（10 项，Property=0，append-only）
//   - EColorGroup  — 色组枚举（None=0 + 8 色组，append-only）
//   - ETileAction  — 格子特殊动作枚举（None=0，4 项，append-only）
//   - EEventDeck   — 牌堆枚举（None=0 + 2 牌堆，append-only）
//   - FBoardTileData  — 每格扁平 struct（GDD CR-3，13 字段，载体无关）
//   - UBoardDataAsset — 顶层 Primary Data Asset 载体（GDD CR-5.1/CR-6，ADR-0002）
//
// Out of Scope（严守边界）：
//   - FBoardAdvanceResult / UBoardMathLibrary / AdvanceIndex / StepsBetween → story-003
//   - 持有者宿主 / 加载 / 释放 / handle → story-002
//   - GetTileData / GetTilesInGroup / GetBoardId 查询接口 → story-004
//   - 加载期校验 / 拒绝逻辑 → story-005 / story-006
//   - DA_Board_Classic40 实例填充 → story-007
//
// 规范依据：
//   - ADR-0002（primary）— 棋盘数据容器，§Decision / §Key Interfaces / §Implementation Guidelines
//   - ADR-0001 — UObject 宿主与生命周期（持有者由 story-002 实现）
//   - ADR-0003 / ADR-0005 — 枚举 append-only 纪律，字段只增不改语义
//   - control-manifest Foundation Layer §数据容器 (ADR-0002) 第 40-50 行
//   - story-001 AC-4 / AC-4b / AC-7b / 结构定义 / 顶层载体
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"   // 提供 UPrimaryDataAsset
#include "BoardDataAsset.generated.h"

// =============================================================================
// ETileType — 格子类型
//
// ⚠ APPEND-ONLY 纪律（ADR-0003/0005）：
//   枚举整数值一旦发布绝不重排，新增类型只能追加到末尾（GoToJail 之后）。
//   存档兼容性依赖整数值稳定（ADR-0005 §Never 重排枚举整数索引）。
// =============================================================================
UENUM(BlueprintType)
enum class ETileType : uint8
{
    /** 地产格（可购买，有租金/色组/建筑）。值 0，append-only 基准 */
    Property         = 0  UMETA(DisplayName = "Property"),

    /** 铁路格（可购买，租金按持有数量倍增） */
    Railroad         = 1  UMETA(DisplayName = "Railroad"),

    /** 公用事业格（可购买，租金按骰点倍率计算） */
    Utility          = 2  UMETA(DisplayName = "Utility"),

    /** 机会格（抽机会牌堆） */
    Chance           = 3  UMETA(DisplayName = "Chance"),

    /** 公共基金格（抽公共基金牌堆） */
    CommunityChest   = 4  UMETA(DisplayName = "Community Chest"),

    /** 税务格（支付固定税额 TaxAmount） */
    Tax              = 5  UMETA(DisplayName = "Tax"),

    /** 起点格（经过/到达时收取薪资 SalaryAmount） */
    Go               = 6  UMETA(DisplayName = "Go"),

    /** 监狱格（探监/服刑两种子状态，由移动/所有权 epic 处理） */
    Jail             = 7  UMETA(DisplayName = "Jail"),

    /** 免费停车格（无收付效果，规则选项由 HouseRule 处理） */
    FreeParking      = 8  UMETA(DisplayName = "Free Parking"),

    /** 前往监狱格（触发玩家入狱，SpecialAction=GoToJail） */
    GoToJail         = 9  UMETA(DisplayName = "Go To Jail"),

    // ⚠ 新增类型追加于此行之后，不改已有值
};

// =============================================================================
// EColorGroup — 地产色组
//
// ⚠ APPEND-ONLY 纪律：None=0 固定，已有色组整数值不得重排。
//   ColorKeyTable（TMap<EColorGroup, FColor>）以此枚举为 Key，重排破坏色键表映射。
// =============================================================================
UENUM(BlueprintType)
enum class EColorGroup : uint8
{
    /** 无色组（非地产类格子，或未设置色组）。值 0，append-only 基准 */
    None         = 0  UMETA(DisplayName = "None"),

    /** 棕色组（经典大富翁最低价值地产） */
    Brown        = 1  UMETA(DisplayName = "Brown"),

    /** 淡蓝色组 */
    LightBlue    = 2  UMETA(DisplayName = "Light Blue"),

    /** 品红/粉色组 */
    Magenta      = 3  UMETA(DisplayName = "Magenta"),

    /** 橙色组 */
    Orange       = 4  UMETA(DisplayName = "Orange"),

    /** 红色组 */
    Red          = 5  UMETA(DisplayName = "Red"),

    /** 黄色组 */
    Yellow       = 6  UMETA(DisplayName = "Yellow"),

    /** 绿色组 */
    Green        = 7  UMETA(DisplayName = "Green"),

    /** 深蓝色组（经典大富翁最高价值地产） */
    DarkBlue     = 8  UMETA(DisplayName = "Dark Blue"),

    // ⚠ 新增色组追加于此行之后，不改已有值
};

// =============================================================================
// ETileAction — 格子特殊动作
//
// ⚠ APPEND-ONLY 纪律：None=0 固定，已有动作整数值不得重排。
//   SpecialAction 字段存档后，整数值须与枚举定义永久对应（ADR-0005）。
// =============================================================================
UENUM(BlueprintType)
enum class ETileAction : uint8
{
    /** 无特殊动作（默认值）。值 0，append-only 基准 */
    None                    = 0  UMETA(DisplayName = "None"),

    /** 经过/到达起点时收取薪资（仅 Go 格使用，配合 SalaryAmount） */
    CollectSalary           = 1  UMETA(DisplayName = "Collect Salary"),

    /** 触发玩家移动至监狱（仅 GoToJail 格使用） */
    GoToJail                = 2  UMETA(DisplayName = "Go To Jail"),

    /** 触发房屋规则检查（HouseRule 选项，如免费停车彩池等） */
    TriggerHouseRuleCheck   = 3  UMETA(DisplayName = "Trigger House Rule Check"),

    // ⚠ 新增动作追加于此行之后，不改已有值
};

// =============================================================================
// EEventDeck — 事件牌堆类型
//
// ⚠ APPEND-ONLY 纪律：None=0 固定，已有牌堆整数值不得重排。
//   EventDeck 字段标识本格触发的牌堆，下游事件格 epic 依赖此枚举路由抽牌逻辑。
// =============================================================================
UENUM(BlueprintType)
enum class EEventDeck : uint8
{
    /** 不触发牌堆（默认值，非事件格）。值 0，append-only 基准 */
    None            = 0  UMETA(DisplayName = "None"),

    /** 机会牌堆（Chance cards） */
    Chance          = 1  UMETA(DisplayName = "Chance"),

    /** 公共基金牌堆（Community Chest cards） */
    CommunityChest  = 2  UMETA(DisplayName = "Community Chest"),

    // ⚠ 新增牌堆追加于此行之后，不改已有值
};

// =============================================================================
// FBoardTileData — 每格扁平数据 struct（GDD CR-3）
//
// 设计原则（ADR-0002 §Implementation Guidelines）：
//   1. 载体无关——struct 本身不依赖 UBoardDataAsset，未来迁移载体 struct 不变。
//   2. 字段只增不改语义（ADR-0003/0005）：已发布字段不得删除或改变语义，
//      新字段追加到末尾并设合理默认值。
//   3. 全部字段标 EditDefaultsOnly，在 DA Details 面板可视化编辑（无 CSV 限制）。
//
// 字段说明（逐字对齐 ADR-0002 §Key Interfaces）：
//   - TileIndex        : 格子序号（0-based），Board 数组下标（O(1) 访问 AC-R5）
//   - TileType         : 格子类型，决定租金计算路由（RentTable vs DiceMultiplierTable vs TaxAmount）
//   - DisplayName      : 本地化显示名（FText，MVP 单语言 inline，OQ-7 指针见 ADR-0002）
//   - ColorGroup       : 色组归属（非地产格填 None）
//   - PurchasePrice    : 购买价格（非可购买格填 0）
//   - RentTable        : 租金表（Property=6 级[无房×1/有房×1-4/全色]/Railroad=4 级，Utility 不用）
//   - DiceMultiplierTable : 骰点倍率表（Utility=2 项[单持/双持]，Property/Railroad 不用）
//   - BuildingCost     : 每栋建筑成本（仅 Property 使用）
//   - MortgageValue    : 抵押价值（购买价 ÷ 2，仅可购买格使用）
//   - TaxAmount        : 固定税额（仅 Tax 格使用，如奢侈税 / 所得税）
//   - SalaryAmount     : 薪资金额（仅 Go 格使用，经过/到达时收取）
//   - EventDeck        : 触发的牌堆（仅 Chance / CommunityChest 格填非 None）
//   - SpecialAction    : 格子特殊动作（GoToJail 格填 ETileAction::GoToJail，其余填 None）
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FBoardTileData
{
    GENERATED_BODY()

    /** 格子序号（0-based），对应 UBoardDataAsset::Tiles 数组下标。默认 0（起点格） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Tile")
    int32 TileIndex = 0;

    /** 格子类型，决定运行时行为路由（租金/税/牌堆/特殊动作）。默认 Property */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Tile")
    ETileType TileType = ETileType::Property;

    /** 本地化显示名（HUD / 地图 UI 用）。MVP 可 inline FText，OQ-7 指针见 ADR-0002 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Tile")
    FText DisplayName;

    /**
     * 色组归属（仅 Property 格有效，其余填 None）。
     * 同色组全部持有时触发垄断状态（由所有权 epic 计算）。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Property")
    EColorGroup ColorGroup = EColorGroup::None;

    /** 购买价格（Property/Railroad/Utility 格填正值，其余填 0） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Property")
    int32 PurchasePrice = 0;

    /**
     * 租金表（Property=6 项：[无房, 1房, 2房, 3房, 4房, 酒店]；Railroad=4 项：[1条,2条,3条,4条]）。
     * Utility 格不使用本字段（改用 DiceMultiplierTable），保持空 TArray。
     * 字段只增不改语义：追加新租金级别时在末尾添加，不移位已有索引。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Property")
    TArray<int32> RentTable;

    /**
     * 骰点倍率表（仅 Utility 格使用，2 项：[单持倍率, 双持倍率]）。
     * 运行时租金 = 骰点之和 × 倍率。
     * Property / Railroad 格不使用本字段，保持空 TArray。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Property")
    TArray<int32> DiceMultiplierTable;

    /** 每栋建筑成本（仅 Property 格使用；Railroad/Utility 不可建房，填 0） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Property")
    int32 BuildingCost = 0;

    /** 抵押价值（= PurchasePrice ÷ 2，仅可购买格有效；非可购买格填 0） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Property")
    int32 MortgageValue = 0;

    /** 固定税额（仅 Tax 格使用，如奢侈税 100 / 所得税 200；其余格填 0） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Tax")
    int32 TaxAmount = 0;

    /**
     * 薪资金额（仅 Go 格使用，经过或到达时玩家收取此金额）。
     * 默认 0；Go 格须填正值，配合 SpecialAction=CollectSalary 使用（AC-7b）。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Special")
    int32 SalaryAmount = 0;

    /** 触发的事件牌堆（仅 Chance/CommunityChest 格填非 None；其余格填 None） */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Event")
    EEventDeck EventDeck = EEventDeck::None;

    /**
     * 格子特殊动作（GoToJail 格填 ETileAction::GoToJail；Go 格填 CollectSalary；其余填 None）。
     * 运行时移动 epic 读取此字段决定是否触发特殊逻辑。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Special")
    ETileAction SpecialAction = ETileAction::None;
};

// =============================================================================
// UBoardDataAsset — 棋盘顶层 Primary Data Asset 载体
//
// 设计原则（ADR-0002 §Decision）：
//   1. 继承 UPrimaryDataAsset，由 UAssetManager 管理加载生命周期。
//   2. 顶层持元数据字段（BoardIdentifier / ColorKeyTable / bIsPlaceholderData）
//      + 一个 TArray<FBoardTileData> Tiles。
//   3. 运行时由 ADR-0001 裁定的持有者宿主以 UPROPERTY 强引用持有，防 GC。
//      （持有者实现见 story-002；本类自身不实现加载/释放逻辑）
//   4. 接口隔离（ADR-0002 R7）：下游不得各自硬引用 UBoardDataAsset，
//      须经持有者的 GetTileData/GetTilesInGroup 访问（查询接口见 story-004）。
//
// 编辑器使用：
//   在 Content Browser 中右键 → Miscellaneous → Data Asset → 选 UBoardDataAsset，
//   以 DA_ 前缀命名（如 DA_Board_Classic40）。
//   Tiles 数组在 Details 面板可 "+" 添加格子，RentTable/DiceMultiplierTable 可直接填多值。
// =============================================================================
UCLASS(BlueprintType)
class RENTO_API UBoardDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 棋盘级元数据（顶层字段，ADR-0002 §Decision 三件套）
    // =========================================================================

    /**
     * 棋盘标识符（作者手填的逻辑 ID，与资产路径 / PrimaryAssetId / 文件名解耦）。
     *
     * 用途（AC-28）：存档时存此值，而非资产路径；资产改名/移动后 BoardIdentifier 不变，
     * 旧存档仍可正确关联同一棋盘布局。
     * 取值示例：FName("Classic40") / FName("Custom_6Players")。
     * 具体 GetBoardId() 查询接口由 story-004 实现。
     *
     * ⚠ FName 大小写不敏感（ADR-0002 R3 注）：存档读取时须用 FName::FastEqual 或显式约定大小写。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Metadata")
    FName BoardIdentifier;

    /**
     * 色键表：色组枚举 → 显示颜色的映射（GDD CR-5.1）。
     *
     * HUD / 地图 UI 通过此表获取各色组的代表色（如 Brown→FColor(...)）。
     * Key=EColorGroup（枚举），Value=FColor（RGBA）。
     * 编辑器在 Details 面板可直接添加 Key-Value 对。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Metadata")
    TMap<EColorGroup, FColor> ColorKeyTable;

    // =========================================================================
    // 棋盘格子数据
    // =========================================================================

    /**
     * 全部格子数据数组（GDD CR-3，顺序 = 棋盘物理顺序，Index 从 0 开始）。
     *
     * 规则：
     *   - Index 0 须为 Go 格（加载期校验由 story-005 负责）
     *   - 数组顺序与 FBoardTileData::TileIndex 须一致（校验见 story-006）
     *   - 运行时 GetTileData(Index) 为 O(1) 数组访问（ADR-0002 R5）
     *
     * 编辑器使用：Details 面板 "+" 追加，填写各字段；
     * TArray<int32> 子字段（RentTable/DiceMultiplierTable）同样支持 "+" 添加元素。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Board|Tiles")
    TArray<FBoardTileData> Tiles;

    // =========================================================================
    // 编辑器专用元数据（Shipping 构建剥离）
    // =========================================================================

#if WITH_EDITORONLY_DATA
    /**
     * 占位数据标志（GDD CR-6，cook 门控）。
     *
     * 语义：true = 本资产尚未填入真实设计数据，仅用于开发期占位；
     *       false = 资产已通过设计评审，可进入 Alpha/Beta 构建。
     *
     * cook 前 commandlet（story-007/gate-check）扫描所有 UBoardDataAsset 实例，
     * 若发现 bIsPlaceholderData=true 则阻断 cook（CI/构建门控语义）。
     * Shipping 构建剥离：WITH_EDITORONLY_DATA 包裹，运行时不占内存，不可在 Shipping 读取。
     *
     * 默认值 true（新建资产默认占位，需作者手动置 false 后通过 gate-check）。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Board|Metadata|Editor")
    bool bIsPlaceholderData = true;
#endif // WITH_EDITORONLY_DATA
};
