// PlayerTurnTypes.h
// =============================================================================
// 回合系统公共枚举定义（story pt-001 / TR-turn-001 / TR-turn-014）
//
// 包含：
//   EPlayerColor — 棋子颜色枚举（8 色，开局唯一分配）
//   EAIDifficulty — AI 难度枚举（None/Casual/Normal/Sharp）
//
// 设计约束（ADR-0005 / TR-turn-014）：
//   - 枚举基底必须为 uint8，确保 BP 可见 + append-only 纪律
//   - ordinal 值显式标注，禁止重排（存档序列化依赖整数索引不变）
//   - append-only：只在末尾追加新值，绝不删除/重排已有值（破坏存档）
//   - 含 UMETA(DisplayName) 供编辑器显示
//
// 规范依据：
//   - ADR-0001（player-turn 系统宿主与生命周期）
//   - ADR-0005（枚举 append-only，整数索引不可重排）
//   - TR-turn-014（EPlayerColor/EAIDifficulty 存档序列化约束）
//   - control-manifest Foundation Layer §存档序列化 (ADR-0005)
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "PlayerTurnTypes.generated.h"

// =============================================================================
// EPlayerColor — 棋子颜色枚举（8 色预留 + None 哨兵）
//
// 开局唯一分配，分配后恒定（TR-turn-001 / GDD CR-1 TokenColor 字段）。
// None=0 为哨兵/未分配值，不应出现在对局中（AC-5 唯一性校验门控此值）。
// 实色 1..8 映射到 8 种经典大富翁棋子颜色。
//
// ⚠ append-only 纪律（ADR-0005 / TR-turn-014）：
//   整数索引 0..8 已冻结，新色只能追加 9..N（存档依赖此整数值不变）。
// =============================================================================
UENUM(BlueprintType)
enum class EPlayerColor : uint8
{
    /** 哨兵/未分配（ordinal=0，不出现在对局玩家中） */
    None      = 0  UMETA(DisplayName = "None"),

    /** 红色（ordinal=1） */
    Red       = 1  UMETA(DisplayName = "Red"),

    /** 蓝色（ordinal=2） */
    Blue      = 2  UMETA(DisplayName = "Blue"),

    /** 绿色（ordinal=3） */
    Green     = 3  UMETA(DisplayName = "Green"),

    /** 黄色（ordinal=4） */
    Yellow    = 4  UMETA(DisplayName = "Yellow"),

    /** 紫色（ordinal=5） */
    Purple    = 5  UMETA(DisplayName = "Purple"),

    /** 橙色（ordinal=6） */
    Orange    = 6  UMETA(DisplayName = "Orange"),

    /** 青色/青绿（ordinal=7） */
    Cyan      = 7  UMETA(DisplayName = "Cyan"),

    /** 粉色（ordinal=8） */
    Pink      = 8  UMETA(DisplayName = "Pink"),

    // ⚠ 新颜色只能追加到此处（ordinal >= 9），禁止删除/重排以上值。
};

// =============================================================================
// EAIDifficulty — AI 难度枚举（None/Casual/Normal/Sharp）
//
// 非 AI 玩家固定为 None（GDD CR-1 AIDifficulty 字段语义）。
// AI 系统(10) 消费此字段决策强度差异（GDD CR-1 注释"预留,AI系统10消费"）。
//
// ⚠ append-only 纪律（ADR-0005 / TR-turn-014）：
//   整数索引 0..3 已冻结，新难度只能追加 4..N。
// =============================================================================
UENUM(BlueprintType)
enum class EAIDifficulty : uint8
{
    /** 非 AI 玩家或难度未设置（ordinal=0） */
    None    = 0  UMETA(DisplayName = "None"),

    /** 休闲难度（ordinal=1）：简单策略，适合新手 */
    Casual  = 1  UMETA(DisplayName = "Casual"),

    /** 普通难度（ordinal=2）：标准策略 */
    Normal  = 2  UMETA(DisplayName = "Normal"),

    /** 锐利难度（ordinal=3）：进阶策略，积极买地收租 */
    Sharp   = 3  UMETA(DisplayName = "Sharp"),

    // ⚠ 新难度只能追加到此处（ordinal >= 4），禁止删除/重排以上值。
};

// =============================================================================
// EJailReason — 入狱原因枚举（最小占位，pt-005 / GDD CR-3.1 SetJailState）
//
// 完整 EJailReason 枚举规则归事件格7 epic；本枚举是最小占位定义，
// 供 SetJailState(bInJail, EJailReason) setter 签名使用（story-005）。
//
// ⚠ append-only 纪律（ADR-0005）：
//   None=0 已冻结，事件7 追加值只能从 1 开始。
// =============================================================================
UENUM(BlueprintType)
enum class EJailReason : uint8
{
    /**
     * 无原因 / 占位（ordinal=0）。
     * 用于 SetJailState 中入狱前的清除状态，或事件7 尚未追加完整值时的默认值。
     */
    None    = 0  UMETA(DisplayName = "None"),

    // ⚠ 事件格7 追加完整 EJailReason 值（ordinal >= 1），禁止删除/重排 None。
};

// =============================================================================
// EArrivalContext — 落地上下文枚举（pt-006 / story-006 LOCKED 设计决策 B / AC-4）
//
// 最小占位（story-006）：
//   DiceMove=0 — 正常骰子移动落地（对照组，走正常落地结算分支）
//   SentToJail=1 — 被传送入狱（GoToJail 格/三连双点/入狱牌），抑制全部落地结算
//
// 完整 EArrivalContext 语境（GoToJailTile / EventCard / AdvanceCard 等）归 movement epic；
// 本 story 仅最小占位：DiceMove 对照组 + SentToJail 抑制组（Out of Scope 严守）。
//
// ⚠ append-only 纪律（ADR-0005）：
//   DiceMove=0 / SentToJail=1 已冻结，movement epic 追加值只能从 2 开始。
//   GDD Edge Cases L392 / AC-46 / TR-move-006 引用。
// =============================================================================
UENUM(BlueprintType)
enum class EArrivalContext : uint8
{
    /**
     * 正常骰子移动落地（ordinal=0，对照组）。
     * RollPhase → MovePhase → 正常落地结算（DecideBuyProperty / SettleRent）。
     */
    DiceMove    = 0  UMETA(DisplayName = "Dice Move"),

    /**
     * 被传送入狱（ordinal=1，抑制组）。
     * GoToJail 格 / 三连双点 / 入狱牌 → 直接传送入狱格，跳过全部落地结算分支。
     * GDD Edge Cases L392：被传送入狱非「正常落地」，Jail 格不可购买/无租/无事件。
     */
    SentToJail  = 1  UMETA(DisplayName = "Sent To Jail"),

    // ⚠ movement epic 追加完整上下文值（ordinal >= 2），禁止删除/重排以上值。
    // 待追加：GoToJailTile / EventCard / AdvanceCard / PassGo 等（归 movement epic）。
};

// =============================================================================
// ETurnPhase — 回合阶段状态机枚举（pt-002 / TR-turn-002 / GDD (b) States 表）
//
// 每个活跃回合经历的阶段序列（GDD L224 States and Transitions (b)）：
//   正常路径：TurnStart → RollPhase → MovePhase → ResolvePhase → PostRollAction → TurnEnd
//   在狱路径：TurnStart → JailTurn → TurnEnd
//   双点额外：TurnEnd →（双点且未入狱）→ RollPhase（同玩家继续）
//
// 禁 Latent（ADR-0001 §4）：状态机仅用枚举字段 + delegate 推进，
//   绝不用 FTimerHandle/Blueprint Delay/WaitForEvent，以保证读档 switch(CurrentPhase) 重入。
//
// ⚠ append-only 纪律（ADR-0005 / GDD L277 R7 澄清）：
//   整数索引 0..6 已冻结（首次提交即锁定），新阶段只能追加到末尾（ordinal >= 7）。
//   历史草稿值（AIPlaybackGating）已在任何引擎资产使用前删除，不违反 append-only。
//   7 个合法值覆盖全部回合阶段，结构（阶段序列）不可变。
// =============================================================================
UENUM(BlueprintType)
enum class ETurnPhase : uint8
{
    /** 回合开始（ordinal=0）：行动权移交至本玩家，路由正常/在狱分支 */
    TurnStart       = 0  UMETA(DisplayName = "Turn Start"),

    /** 掷骰阶段（ordinal=1）：等待完整 FDiceRollResult（Die1/Die2/Total/bIsDouble） */
    RollPhase       = 1  UMETA(DisplayName = "Roll Phase"),

    /** 移动阶段（ordinal=2）：委派移动(4)，等待落点回报 */
    MovePhase       = 2  UMETA(DisplayName = "Move Phase"),

    /** 结算阶段（ordinal=3）：落地结算（买地/收租/事件），委派经济(5)/事件格(7) */
    ResolvePhase    = 3  UMETA(DisplayName = "Resolve Phase"),

    /** 回合后行动（ordinal=4）：人类 UI 发「结束回合」/ AI 同步返回动作列表 */
    PostRollAction  = 4  UMETA(DisplayName = "Post Roll Action"),

    /** 回合结束（ordinal=5）：判定额外回合 or 移交下一玩家 TurnStart */
    TurnEnd         = 5  UMETA(DisplayName = "Turn End"),

    /** 入狱回合（ordinal=6）：在狱玩家专属分支，出狱决策点后进 TurnEnd */
    JailTurn        = 6  UMETA(DisplayName = "Jail Turn"),

    // ⚠ 新阶段只能追加到此处（ordinal >= 7），禁止删除/重排以上值（存档序列化依赖整数索引不变）。
};

// =============================================================================
// EActionType — AI 可观察行动类型（HUD16 R-2 propagate 落定 OQ-HUD-12，owner=回合2）
//   append-only（ADR-0003/ADR-0005）：禁删除/重排，尾部追加。
// =============================================================================
UENUM(BlueprintType)
enum class EActionType : uint8
{
    None        = 0 UMETA(DisplayName="None"),
    BuyProperty = 1 UMETA(DisplayName="Buy Property"),
    BuildHouse  = 2 UMETA(DisplayName="Build House"),
    Mortgage    = 3 UMETA(DisplayName="Mortgage"),
    Unmortgage  = 4 UMETA(DisplayName="Unmortgage"),
    Move        = 5 UMETA(DisplayName="Move"),
    PayRent     = 6 UMETA(DisplayName="Pay Rent"),
    Bankrupt    = 7 UMETA(DisplayName="Bankrupt"),
    // ⚠ 新值尾部追加（ordinal>=8）
};

// OnPhaseChanged payload（AC-2/AC-3）
USTRUCT(BlueprintType)
struct RENTO_API FPhaseChangedInfo
{
    GENERATED_BODY()
    /** 旧阶段 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") ETurnPhase OldPhase = ETurnPhase::TurnStart;
    /** 新阶段 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") ETurnPhase NewPhase = ETurnPhase::TurnStart;
    /** 当前行动玩家连续双点次数（无活跃玩家时为 0） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 ConsecutiveDoubles = 0;
};

// OnTurnStarted payload（AC-2）
USTRUCT(BlueprintType)
struct RENTO_API FTurnStartedInfo
{
    GENERATED_BODY()
    /** 行动玩家 PlayerId */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 PlayerId = INDEX_NONE;
    /** 是否为 AI 玩家 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") bool  bIsAI    = false;
};

// OnTurnEnded payload（AC-2/AC-4）
USTRUCT(BlueprintType)
struct RENTO_API FTurnEndedInfo
{
    GENERATED_BODY()
    /** 结束回合的玩家 PlayerId（移交前） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 PlayerId        = INDEX_NONE;
    /** 是否获得额外回合（双点且未入狱且未破产） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") bool  bGrantsExtraTurn = false;
};

// OnTurnOrderResolved payload（AC-2/AC-5；OrderedPlayerIds 包入 struct，禁裸 TArray 作 delegate 参数）
USTRUCT(BlueprintType)
struct RENTO_API FTurnOrderResult
{
    GENERATED_BODY()
    /** 按回合序（TurnOrderIndex）排列的 PlayerId 数组 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") TArray<int32> OrderedPlayerIds;
    /** 是否经席位升序裁定（达 MaxTiebreakRounds 或 DiceService 不可用） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") bool bResolvedBySeatTiebreak = false;
};

// OnAIActionExecuted payload（AC-2/AC-6）
USTRUCT(BlueprintType)
struct RENTO_API FAIActionDetails
{
    GENERATED_BODY()
    /** 本次广播动作在已执行序列中的 0-based 索引 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 ActionIndex     = INDEX_NONE;
    /** 执行动作的 AI 玩家 PlayerId */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 ActingPlayerId  = INDEX_NONE;
    /** 目标格索引 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 TargetTileIndex = INDEX_NONE;
    /** 金额占位（经济5 层未知，本层为 0） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 Amount          = 0;
    /** HUD 可观察行动类型 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") EActionType ActionType = EActionType::None;
};

// OnBuildingAnnounced payload（AC-7）
USTRUCT(BlueprintType)
struct RENTO_API FBuildingAnnouncedInfo
{
    GENERATED_BODY()
    /** 建房格索引 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 TileIndex      = INDEX_NONE;
    /** 新房屋数量 */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 NewHouseCount  = 0;
    /** 执行建房的玩家 PlayerId（取当前回合上下文，方案②） */
    UPROPERTY(BlueprintReadOnly, Category="PlayerTurn|Events") int32 ActingPlayerId = INDEX_NONE;
};
