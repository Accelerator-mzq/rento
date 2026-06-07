// AIDecisionMakerInterface.h
// =============================================================================
// IAIDecisionMaker — AI 决策 hook 接口（story pt-007 AC-3 前置 / TR-turn-008）
//
// 架构约束（ADR-0006 裁定一/三 / ADR-0007）：
//   - 所有 hook 经 const FGameStateSnapshot& 传入（零深拷贝，决策期冻结，AI 不写状态）
//   - 纯 C++ 接口（非 UInterface）：headless 可 stub/spy 注入，无需 UHT
//   - TSharedPtr<IAIDecisionMaker> 持有（仿 IResolveBankruptcy / ILandingResolver DI 模式）
//
// 并存关系说明（pt-006 / pt-007 接缝）：
//   pt-006 已有 ILandingResolver::DecideBuyProperty(int32 PlayerId, int32 TileIndex)
//   ——人类/通用 landing 路径，ResolveArrival 调用。
//   本接口 IAIDecisionMaker::DecideBuyProperty(const FGameStateSnapshot&, int32 TileIndex)
//   ——AI 专用决策 hook，AI 决策阶段调用。
//   二者并存，不替代。本接口不改 ILandingResolver，不改 ResolveArrival。
//
// ⚠ WARNING — 偏离 ADR-0006 Key Interfaces（已记录 logged decision）：
//   ADR 原文 DecideBuyProperty 签名：bool DecideBuyProperty(const FGameStateSnapshot& S,
//                                                           const FPropertyData& P)
//   偏离原因：FPropertyData 类型尚不存在（property epic 未实现）。
//   本簇改用：bool DecideBuyProperty(const FGameStateSnapshot& S, int32 TileIndex)
//   AI 经 S.Tiles[TileIndex] 读取地产属性。此偏离不改变接口语义，
//   是 logged decision（由 code-review 阶段核查；FPropertyData 可用时回补签名）。
//
// ⚠ EJailAction / EAIActionType / FTurnAction — append-only 纪律（ADR-0005）：
//   枚举值禁止删除/重排，仅在末尾追加。整数索引已冻结。
//
// 规范依据：
//   - story pt-007 AC-3（AI PostRollAction hook DI 接缝）
//   - ADR-0006（hook 签名 const FGameStateSnapshot& 决策期冻结）
//   - ADR-0007（AI 决策核心落 C++，headless 可注入）
//   - control-manifest Core Layer §Required（AI hook const& 传 snapshot）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "GameStateSnapshot.h"
#include "AIDecisionMakerInterface.generated.h"

// =============================================================================
// EJailAction — AI 出狱决策枚举（story pt-007 / GDD CR-8 DecideJailAction 返回值）
//
// append-only 纪律（ADR-0005）：PayBail=0 / RollDouble=1 / UseCard=2 已冻结，
// 新值只能追加（ordinal >= 3）。
// =============================================================================
UENUM(BlueprintType)
enum class EJailAction : uint8
{
    /** 缴纳保释金出狱（PayBailAmount，扣 JailBailAmount 现金，ordinal=0） */
    PayBail     = 0  UMETA(DisplayName = "Pay Bail"),

    /** 尝试掷双点出狱（不扣现金，若非双点继续留狱，ordinal=1） */
    RollDouble  = 1  UMETA(DisplayName = "Roll Double"),

    /**
     * 使用出狱卡出狱（消耗 bHasJailCard，ordinal=2）。
     * AC-5 非法降级：UseCard 但 bHasJailCard==false → 降级留狱（JailTurnsServed+1）。
     */
    UseCard     = 2  UMETA(DisplayName = "Use Card"),

    // ⚠ 新值只能追加到此处（ordinal >= 3），禁止删除/重排以上值（ADR-0005 存档序列化约束）。
};

// =============================================================================
// EAIActionType — AI 回合后行动类型枚举（story pt-007 / GDD CR-8 DecidePostRollActions）
//
// 最小占位集合（完整动作类型归 ai-opponent epic）。
// append-only 纪律（ADR-0005）：None=0 已冻结，其余 1..4 本簇最小集合，
// ai-opponent epic 追加值只能从 5 开始。
// =============================================================================
UENUM(BlueprintType)
enum class EAIActionType : uint8
{
    /** 无操作（占位/空动作，ordinal=0） */
    None                = 0  UMETA(DisplayName = "None"),

    /** 购买地产（ordinal=1；触发买地意图） */
    BuyProperty         = 1  UMETA(DisplayName = "Buy Property"),

    /** 建造房屋（ordinal=2；触发建房意图） */
    BuildHouse          = 2  UMETA(DisplayName = "Build House"),

    /** 抵押地产（ordinal=3；触发抵押意图） */
    MortgageProperty    = 3  UMETA(DisplayName = "Mortgage Property"),

    /** 解押地产（ordinal=4；触发解押意图） */
    UnmortgageProperty  = 4  UMETA(DisplayName = "Unmortgage Property"),

    // ⚠ ai-opponent epic 追加完整动作类型（ordinal >= 5），禁止删除/重排以上值（ADR-0005）。
};

// =============================================================================
// FTurnAction — AI 单次行动结构体（story pt-007 / GDD CR-8）
//
// DecidePostRollActions 返回 TArray<FTurnAction>，框架逐条执行。
// 执行前逐动作重校验可行性（AC-6 / ADR-0006 IG#8），不可行则静默跳过。
// =============================================================================
USTRUCT(BlueprintType)
struct RENTO_API FTurnAction
{
    GENERATED_BODY()

    /** 行动类型（EAIActionType，None=无操作） */
    UPROPERTY() EAIActionType ActionType     = EAIActionType::None;

    /** 目标格索引（对应 FGameStateSnapshot::Tiles 数组索引；-1=无目标格） */
    UPROPERTY() int32         TargetTileIndex = INDEX_NONE;
};

// =============================================================================
// IAIDecisionMaker — AI 决策 hook 纯 C++ 接口
//
// 接口形态（仿 IResolveBankruptcy / ILandingResolver 纯 C++ DI 模式）：
//   - 纯 C++ class + 虚析构 + TSharedPtr 持有
//   - 不依赖 UHT，headless spy 直接继承 struct 实现（G3：纯 C++ 接口 spy 不需 UCLASS）
//
// 所有 hook 传参规则（ADR-0006 裁定三 / R5）：
//   - const FGameStateSnapshot& — 决策期冻结，AI 只读消费，零深拷贝
//   - AI 不得经参数写任何游戏状态（类型系统强制）
//
// 并存声明（见文件头注释）：
//   本接口与 ILandingResolver::DecideBuyProperty 并存，不替代。
// =============================================================================
class RENTO_API IAIDecisionMaker
{
public:
    /** 虚析构，保证正确 delete 派生类（TSharedPtr 正确释放） */
    virtual ~IAIDecisionMaker() = default;

    /**
     * AI 买地决策（ResolvePhase 落无主地产路径；簇 A 仅声明，调用路径归簇 B）。
     *
     * ⚠ 偏离 ADR-0006 原签名（logged decision）：
     *   ADR 原文：bool DecideBuyProperty(const FGameStateSnapshot& S, const FPropertyData& P)
     *   本实现：bool DecideBuyProperty(const FGameStateSnapshot& S, int32 TileIndex)
     *   原因：FPropertyData 尚不存在，AI 可经 S.Tiles[TileIndex] 读取地产属性。
     *
     * @param S         只读快照（决策期冻结视图）
     * @param TileIndex 落点格索引（对应 S.Tiles[TileIndex]）
     * @return true = AI 决定购买；false = 放弃
     */
    virtual bool DecideBuyProperty(const FGameStateSnapshot& S, int32 TileIndex) = 0;

    /**
     * AI 回合后行动决策（PostRollAction 阶段同步批处理；AC-3 / GDD CR-8 / AC-37a）。
     *
     * 框架执行语义（ADR-0006 IG#8 / AC-6）：
     *   - 返回 TArray<FTurnAction>；框架逐条执行前重校验可行性
     *   - 不可行动作静默跳过 + UE_LOG dev 诊断，不中断整个回合
     *   - 全列表处理完后 EndTurn
     *   - 返回空数组 [] → 框架直接 EndTurn（AC-37d）
     *
     * @param S 只读快照（决策期冻结，贪心循环影子变量在 AI 内部，不回写 snapshot）
     * @return AI 决策动作列表（按序执行；空=直接 EndTurn）
     */
    virtual TArray<FTurnAction> DecidePostRollActions(const FGameStateSnapshot& S) = 0;

    /**
     * AI 出狱决策（JailTurn 阶段；簇 A 仅声明，调用路径归簇 B）。
     *
     * 失败降级（ADR-0006 IG#7 / AC-5）：
     *   PayBail 但现金不足 / UseCard 但无卡 → 降级留狱（JailTurnsServed+1）、
     *   Cash/持卡数未变、回合不崩。
     *
     * @param S 只读快照
     * @return EJailAction 决策（PayBail / RollDouble / UseCard）
     */
    virtual EJailAction DecideJailAction(const FGameStateSnapshot& S) = 0;

    /**
     * AI 拍卖出价（Alpha 占位；本簇仅声明，调用路径归簇 B / AC-10 / GDD AC-45）。
     *
     * 出价合法性校验归拍卖12（MVP 不触发）。
     * 值域 sentinel（ADR-0006 IG #hook2）：
     *   负数 / INDEX_NONE / 0 → 视同放弃；
     *   > 当前最高价 且 <= Cash 且 >= 最低加价 → 合法。
     *
     * @param S         只读快照
     * @param TileIndex 拍卖格索引
     * @return 出价（放弃=0 或负数）
     */
    virtual int32 DecideAuctionBid(const FGameStateSnapshot& S, int32 TileIndex) = 0;
};
