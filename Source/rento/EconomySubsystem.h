// EconomySubsystem.h
// =============================================================================
// UEconomySubsystem —— 经济与现金服务（系统5）
//   econ-001（Cash 受控写/不变式）+ econ-002（事件契约）。
//   TR-econ-001/002/018（受控写）+ TR-econ-004/005/006/007（事件契约），amount≥0 部分 TR-econ-014。
//
// 形态：per-match UWorldSubsystem（继承 UMatchSubsystemBase，一局边界、PIE 隔离、ADR-0001）。
//
// 职责：
//   - 现金受控写：Credit / Debit / TransferCash（CR-8 原子守恒）—— 均带 EChangeReason
//   - 现金只读：GetCash
//   - 起始资金：GiveStartingCash（数据驱动 Tuning Knob）
//   - 事件契约（econ-002，ADR-0003）：OnCashChanged / OnRentPaid / OnInsufficientFunds / OnBankruptcyDeclared
//   - 不变式：Cash≥0（Debit 不足额转 Raising Funds）/ amount≥0 拒绝 / amount==0 早返静默
//
// 单源真相（用户 2026-06-08 裁定 Q1）：
//   本服务【不持任何 cash 容器】。Cash 的 field of record = URentoPlayerState.Cash（player-turn 拥有字段、
//   SetCash 字段级 setter）。Credit/Debit 读 PS->Cash、经 PS->SetCash 写。
//
// 解析（用户裁定 Q2）：
//   playerId → URentoPlayerState* 经 GetWorld()->GetSubsystem<UPlayerTurnSubsystem>()->FindPlayerById(id)。
//   运行时依赖：解析依赖回合2 已 InitializeFromConfig；未就绪时各操作安全 no-op（dev log + return false）。
//
// 命名（用户 2026-06-09 裁定）：UEconomySubsystem（对齐 Foundation EventBusDelegateContract 预期 +
//   系统5「经济与现金」全职责语义；econ-001 旧名 UEconomyCashSubsystem 已重命名）。
//
// 规范依据：
//   - GDD economy-cash.md CR-1（受控写/Cash≥0/amount≥0）、CR-8（原子守恒/无限银行）、
//     Events 节（4 事件 + EChangeReason 方向派生）、AC-1~5/33~39
//   - ADR-0001（UObject 宿主）、ADR-0003（事件总线 owner-held delegate）、ADR-0007（C++/BlueprintCallable 边界）、
//     ADR-0014（金钱运算整数纯净）
// =============================================================================
#pragma once

#include "CoreMinimal.h"
#include "MatchSubsystemBase.h"
#include "EconomyTypes.h"            // EChangeReason + 4 Info USTRUCT + 4 delegate 签名
#include "EconomySubsystem.generated.h"

class URentoPlayerState;

/**
 * ELiquidationAction —— 强制清算下一步动作（econ-009 清算顺序 spec 返回值）。
 *   纯 C++ 枚举（非 UENUM）：DecideNextLiquidationStep 内部判据返回，回合2 据此驱动调 6/8（不经 BP）。
 */
enum class ELiquidationAction : uint8
{
    None,           // Cash≥应付 → 偿付成功，停止清算
    MortgageTile,   // 抵押 OutTargetTile（未抵押无房地中 MV 最小，止损优先）
    SellBuilding,   // 卖一栋建筑（无可抵押空地但有房）
    Insolvent,      // 资产耗尽仍不足 → 破产（结构性判定 ⟺ Cash+nlv<应付）
};

/**
 * UEconomySubsystem —— 经济现金权威服务。
 *
 * 不持 cash 容器；Cash 单源 = URentoPlayerState.Cash，经 SetCash 受控写。
 */
UCLASS()
class RENTO_API UEconomySubsystem : public UMatchSubsystemBase
{
    GENERATED_BODY()

public:
    // =========================================================================
    // 只读
    // =========================================================================

    /**
     * 查询玩家现金。
     * @param PlayerId 玩家 ID（0..P-1）
     * @return 玩家当前 Cash；玩家不存在返回 0（dev Warning）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    int32 GetCash(int32 PlayerId) const;

    // =========================================================================
    // 受控写
    // =========================================================================

    /**
     * 入账（Cash += Amount）。
     *
     * amount<0 非法（CR-1 R1 blocker#2）→ dev log + 拒绝、不改 Cash、不广播。
     * amount==0 早返静默（AC-5）→ 不改 Cash、不广播。
     * 成功 → PS->SetCash(Old+Amount) + 广播 OnCashChanged（携 Reason）。
     *
     * @param Reason 变动类型语境（EChangeReason，无方向位；方向由消费方派生，AC-33）。
     * @return true=已入账并广播；false=非法/零额/玩家不存在（未改 Cash）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool Credit(int32 PlayerId, int32 Amount, EChangeReason Reason);

    /**
     * 出账（Cash -= Amount），守 Cash≥0 不变式。
     *
     * amount<0 非法 → dev log + 拒绝。 amount==0 早返静默。
     * Amount > Cash（不足额，AC-1）→ 不扣、广播 OnInsufficientFunds、不广播 OnCashChanged（转 Raising Funds，story-009）。
     * 成功 → PS->SetCash(Old-Amount) + 广播 OnCashChanged（携 Reason）。
     *
     * @return true=已出账并广播；false=非法/零额/不足额/玩家不存在（未改 Cash）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool Debit(int32 PlayerId, int32 Amount, EChangeReason Reason);

    /**
     * 原子转移（CR-8 守恒）：付方扣减额 == 收方入账额，无中间态、无造币/丢币。
     *
     * 用【单一 Amount 局部变量】同时驱动 Debit(Payer,Amount,Reason)+Credit(Payee,Amount,Reason)，绝不两次重算（AC-2）。
     * 先验付方充足；不足则不发生任一腿、广播 OnInsufficientFunds（防造币）。
     * 收租用 Reason=Rent → 两腿 OnCashChanged 均 reason=Rent（AC-37）；OnRentPaid 由收租路径另发（story-004/005）。
     *
     * @return true=转移完成（payer/payee 各 1 次 OnCashChanged）；false=非法/零额/付方不足/任一玩家不存在。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool TransferCash(int32 PayerId, int32 PayeeId, int32 Amount, EChangeReason Reason);

    /**
     * 发薪（F-1，CR-2）：salary = clamp(max(passed_go, 0), 0, PASSED_GO_SAFE_MAX) × SalaryAmount。
     *
     * gate 在 passed_go（防双重发薪，CR-2）：仅 passed_go>0 才 Credit(player, salary, Salary)；
     *   passed_go≤0（未过/逆向穿越）一律不发、不广播、early-return false。
     * 真运行时 clamp（Shipping 亦生效，非仅 dev）防 int32 溢出；passed_go>12（异常大，
     *   后退牌/传送循环 abuse）经 UE_LOG(Error) 暴露上游 bug（dev 信号，不阻断、不替代 clamp）。
     * SalaryAmount 由调用方（移动4 经棋盘 GetTileData(0).SalaryAmount base）注入（caller-injected）；
     *   非正（≤0）或超溢出上界（>2,000,000，economy 侧独立防线 CONCERN-1）→ dev log + 不发。
     * CollectSalary SpecialAction 仅 UI 标记，不经本接口构成第二次发薪（gate 唯一在 passed_go）。
     *
     * @param PlayerId     收薪玩家 ID
     * @param PassedGo     本次移动过 GO 次数（移动4 经 AdvanceIndex 算出；正常 -2..+2）
     * @param SalaryAmount 棋盘 Go 格 base 薪额（经典 200；调用方注入）
     * @return true=已发薪并广播 OnCashChanged(reason=Salary)；false=passed_go≤0/SalaryAmount≤0/玩家不存在。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool PaySalary(int32 PlayerId, int32 PassedGo, int32 SalaryAmount);

    // =========================================================================
    // 租金（econ-004 F-2 / econ-005 F-3/F-4，ADR-0014 整数纯净 / ADR-0006 快照输入）
    // =========================================================================

    /**
     * 地产租金 F-2 piecewise（纯函数，RentTable 由调用方注入；CR-3）：
     *   is_mortgaged                      → 0（短路先于表查找，AC-9）
     *   is_monopoly ∧ house_count==0      → RentTable[0] × MonopolyRentMultiplier（仅无房 base，AC-10）
     *   else                              → RentTable[clamp(house_count,0,5)]（AC-12；酒店 house==5 绝不翻倍 AC-11）
     * ×2 用 raw house_count==0 判定（越界 raw≠0 落 else，AC-13：house=−1→RentTable[0]、house=6→RentTable[5]）。
     * house_count 越界（<0 ∨ >5，建房8 bug）→ UE_LOG(Error) 暴露 + clamp 兜底（不崩，AC-13）。
     * is_mortgaged/is_monopoly 来自地产6 快照、house_count 来自建房8，经回合2 ResolvePhase 聚合传入
     *   （economy 不直读 6/8，防环 ADR-0006）。
     *
     * @return 应付租金 ≥0（抵押/空表→0）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Rent")
    int32 ComputePropertyRent(bool bIsMortgaged, bool bIsMonopoly, int32 HouseCount, const TArray<int32>& RentTable) const;

    /**
     * 收租结算（共有，F-2/F-3/F-4；CR-3/CR-8）：原子转移 payer→payee + 广播 OnRentPaid。
     *   RentAmount≤0（抵押/自有/无主）→ 不转移、不广播、return false（AC-5/37）。
     *   RentAmount>0 → TransferCash(payer,payee,rent,Rent)（两腿 OnCashChanged=Rent）；
     *     付方不足额 → TransferCash 内广播 OnInsufficientFunds + return false → 本函数不广播 OnRentPaid
     *       （未真收租，转 Raising Funds story-009）。成功 → OnRentPaid.Broadcast（AC-34）。
     *
     * @param TileIndex 地产格序号（OnRentPaid payload；非 Payee 所在格）。
     * @return true=收租完成并广播 OnRentPaid；false=零额/付方不足/玩家不存在。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Rent")
    bool SettleRent(int32 PayerId, int32 PayeeId, int32 RentAmount, int32 TileIndex);

    /**
     * 车站租金 F-3（纯函数；CR-3）：rent = RentTable[clamp(station_count−1, 0, 3)]。
     *   守 station_count≥1 否则 0（防 RentTable[−1]，AC-15；count 来自地产6 不假设其正确）。
     *   station_count 由地产6 快照经回合2 聚合传入（economy 不直读 6，防环 ADR-0006）。
     * @return 应付租金 ≥0（无站/空表→0）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Rent")
    int32 ComputeRailroadRent(int32 StationCount, const TArray<int32>& RentTable) const;

    /**
     * 公用租金 F-4（纯函数；CR-3 / ADR-0015）：rent = dice_total × DiceMultiplierTable[clamp(utility_count−1, 0, 1)]。
     *   守 utility_count≥1 否则 0（防 [−1]，AC-17）；dice_total≤0（holder/上游异常）亦置 0（economy 独立防线，不返负值，CONCERN-3）。
     *   **dice_total 显式参数**（非 economy 缓存，AC-18）：调用方经回合2 holder GetCurrentRollTotal() PULL 当前程骰点后传入
     *     （"前进到最近公用"额外骰经事件7 SetCurrentRollContext 注入 holder，调用方 PULL 取注入值）。economy 不反读移动、不缓存 dice。
     *   溢出境界：dice_total∈[2,12]（dice 有界）× DiceMultiplierTable≤1e6（board 加载 fatal AC-23j）= 1.2e7 < INT32_MAX。
     * @return 应付租金 ≥0（无公用/空表→0）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Rent")
    int32 ComputeUtilityRent(int32 UtilityCount, int32 DiceTotal, const TArray<int32>& DiceMultiplierTable) const;

    // =========================================================================
    // 抵押/赎回现金腿（econ-006 F-5/F-6，被地产6 事务调用 6→5，ADR-0014 整数 ceil）
    // =========================================================================

    /**
     * 赎回价（F-6 纯函数，赎回价口径单一来源；CR-5）：unmortgage_cost = MV + ceil(MV×fee_num/fee_den)。
     *   整数 ceil（ADR-0014，零 float）；fee 恒≥1（MV≥1，堵免费赎回退化）。
     *   **纯函数无副作用**：不改状态、不触事件、不读归属、不反调6（地产卡 UI #17 调此显示赎回价，不自算 ceil）。
     *   MV≤0（board 应保 1..PurchasePrice）→ 0 + dev log（防负 cost）。
     * @return unmortgage_cost > MortgageValue（MV>0 时）；MV≤0 → 0。
     */
    UFUNCTION(BlueprintPure, Category="Economy|Mortgage")
    int32 GetUnmortgageCostForDisplay(int32 MortgageValue) const;

    /**
     * 抵押放款现金腿（F-5；被地产6 Mortgage() 事务调用，6→5）：payout = MortgageValue（Credit，reason=Mortgage）。
     *   economy 只执行 Credit，**不标记/不通知6**（bIsMortgaged 由6自置，保无环）；
     *   **前置（未抵押/无房）由6/决策点保证，非此处校验**（AC-20；Credit 通用入账读不到抵押标记/房数）。
     * @return Credit 结果（玩家存在且 MV≥0 时 true）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Mortgage")
    bool MortgagePayout(int32 PlayerId, int32 MortgageValue);

    /**
     * 赎回现金腿（F-6；被地产6 Unmortgage() 事务调用，6→5）：Debit(owner, unmortgage_cost, Unmortgage)。
     *   赎回是**自愿行为**：现金不足 → 赎回不可用、不 Debit、不广播、return false（AC-22）；
     *     **显式 pre-check（GetCash≥cost）避免 Debit 内 OnInsufficientFunds 误触发 raising-funds**
     *     （raising-funds 仅强制扣款 rent/tax 才入，CR-5/CR-7）。
     *   成功 → Debit 广播 OnCashChanged(reason=Unmortgage)；6 自行解除抵押标记（economy 不通知6）。
     * @return true=已扣赎回价；false=现金不足/无效 MV/玩家不存在（地保持抵押）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Mortgage")
    bool UnmortgagePayment(int32 PlayerId, int32 MortgageValue);

    // =========================================================================
    // NLV/破产/卖回（econ-008 F-8/F-9/F-10，ADR-0014 逐栋 floor / ADR-0006 防 5→8 环）
    // =========================================================================

    /**
     * 建筑卖回价 F-8（纯函数；整数 floor）：sellback = floor(BuildingCost × sell_num/sell_den)。
     *   整数截断==floor（操作数≥0，ADR-0014 零 float）；默认 sell_num/sell_den=1/2。
     *   **纯函数无副作用**（F-9 逐栋调它）。BuildingCost≤0 → 0 + dev log（防负卖回）；sell_den≤0 → 0 + dev log（防除零）。
     * @return sellback ≥0（BC=100→50；BC=75→floor(37.5)==37）。
     */
    UFUNCTION(BlueprintPure, Category="Economy|Nlv")
    int32 ComputeBuildingSellback(int32 BuildingCost) const;

    /**
     * 净清算价值 F-9（纯函数；**单一资产枚举入口** AC-27③/AC-29）：
     *   nlv = Σ MortgageValue[未抵押] + Σ HouseCount × ComputeBuildingSellback(BuildingCost)。
     *   **逐栋 floor 先于求和**（🔴 AC-27/AC-32 变体C）：对每条 entry 调 ComputeBuildingSellback（已 floor）再乘 HouseCount，
     *     **绝不**先求和 BuildingCost 再 floor。已抵押地 MV 侧贡献 0（AC-26）；卖回侧由 HouseCount 驱动（抵押地 house 应为 0）。
     *   资产清单由回合2 聚合6/8 传入（economy 不直读6/8，ADR-0006）。空清单→0。
     *   溢出境界：单玩家全盘 nlv ~10⁵ ≪ INT32_MAX（Guardrail）。
     * @return nlv ≥0。
     */
    UFUNCTION(BlueprintPure, Category="Economy|Nlv")
    int32 ComputeNetLiquidationValue(const TArray<FNlvAssetEntry>& Assets) const;

    /**
     * 破产谓词 F-10（纯谓词；严格 `<`，AC-28）：is_insolvent = (GetCash(player) + PreaggregatedNlv < AmountDue)。
     *   **消费传入的 PreaggregatedNlv（由回合2 经 F-9 算好），不内部枚举/不重算**（AC-29 单源，防 5→8 环）。
     *   严格 `<`：付到恰好 0 算**能付、非破产**（Cash+nlv==due → false，AC-28）。
     * @return true=破产（资产清算后仍不足额）；false=能付（含付到 0）。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Nlv")
    bool IsInsolvent(int32 PlayerId, int32 AmountDue, int32 PreaggregatedNlv) const;

    // =========================================================================
    // 强制清算顺序 spec + 破产现金侧（econ-009 CR-7/F-11，ADR-0001/0003）
    // =========================================================================

    /**
     * 强制清算下一步决策（清算顺序 spec，economy 拥有；纯静态无副作用，AC-43）。
     *   止损优先 mortgage-empty-first（CR-7）：
     *     Cash≥AmountDue                         → None（偿付成功）
     *     存在未抵押∧无房地（HouseCount==0）     → MortgageTile + OutTargetTile=MV 最小者（抵押可赎回，零损）
     *     否则存在建筑（HouseCount>0）            → SellBuilding（永久半价亏损，故后于抵押）
     *     否则（资产耗尽）                        → Insolvent（结构性破产判定 ⟺ Cash+总 nlv<应付）
     *   **执行驱动归回合2**：economy 只出"下一步该干什么"，回合2 据返回调 6.Mortgage/8.ForcedSellNextBuilding
     *   （economy 不直调 6/8，防 5→6/5→8 环，ADR-0006/0001）。Assets 由回合2 聚合自有地传入（owner 已筛）。
     *   顺序不影响最终是否破产（NLV order-independent），只影响够即停后剩余。
     * @param Cash          债务玩家当前现金
     * @param AmountDue     应付金额
     * @param Assets        玩家自有可清算地（回合2 聚合，owner==player）
     * @param OutTargetTile [out] MortgageTile 时=待抵押格序号；否则 INDEX_NONE
     * @return 下一步动作（回合2 据此驱动）
     */
    static ELiquidationAction DecideNextLiquidationStep(
        int32 Cash, int32 AmountDue, const TArray<FNlvAssetEntry>& Assets, int32& OutTargetTile);

    /**
     * 破产现金侧结算（F-11；被破产9 在资产 in-kind 移交后调用）：
     *   CreditorId 为玩家   → `creditor.Cash += debtor.Cash`（收租破产，MVP 不收继承利息，AC-30）；
     *   CreditorId==INDEX_NONE（银行）→ debtor 现金蒸发、不入任何玩家（税/银行破产，AC-31）。
     *   恒广播 OnBankruptcyDeclared(Debtor,Creditor) 恰一次（AC-36，即便 debtor.Cash==0）。
     *   **只结算现金侧**：地产/建筑 in-kind 移交由破产9 经地产6/建房8（所有权 AC-33），本函数不写 owner map、不拆建筑。
     * @param DebtorId   破产玩家
     * @param CreditorId 债权玩家；INDEX_NONE=银行（现金蒸发）
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    void SettleBankruptcy(int32 DebtorId, int32 CreditorId);

    // =========================================================================
    // 缴税/买地现金腿（econ-007 F-7/CR-4，被事件7/地产6 调用 7→5 / 6→5，ADR-0007）
    // =========================================================================

    /**
     * 缴税现金腿（F-7；被事件7 Tax 格触发，7→5）：Debit(player, TaxAmount, Tax)。
     *   税是【强制】扣款：现金不足 → Debit 内广播 OnInsufficientFunds 进 Raising Funds（同 AC-1，story-009 状态机）。
     *   款流向银行 sink【蒸发】：不 Credit 任何玩家（CR-6/CR-8 无限银行模型），仅 1 次 OnCashChanged（reason=Tax）。
     *   TaxAmount 读棋盘 GetTileData(index).TaxAmount base（调用方注入）；<0/==0 由 Debit 通用守门处理。
     * @return true=已扣税并广播；false=不足额（已发 OnInsufficientFunds）/非法额/玩家不存在。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool PayTax(int32 PlayerId, int32 TaxAmount);

    /**
     * 买地现金腿（CR-4；被地产6 Buy 事务调用，6→5）：显式 pre-check GetCash≥PurchasePrice → Debit(price, Purchase)。
     *   买地是【可选】行为：现金不足 → 购买不可用（不 Debit、不广播、return false，**不进 Raising Funds**，
     *     显式 pre-check 避免 Debit 内 OnInsufficientFunds 误触发 raising-funds，仿 UnmortgagePayment）。
     *   economy 只执行扣款：**不登记归属、不通知6**（归属 map 由6在扣款成功后自登记，5↔6 无环，ADR-0013/0007）。
     *   PurchasePrice≤0（board 数据 bug）→ dev log + return false（防无效购买）。
     * @return true=已扣款（reason=Purchase）；false=现金不足（购买不可用）/无效 price/玩家不存在。
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    bool BuyPropertyCashLeg(int32 PlayerId, int32 PurchasePrice);

    /**
     * 发放起始资金（数据驱动 Tuning Knob StartingCash，经 Credit 入账，reason=Salary 入账 faucet）。
     * @param PlayerId 玩家 ID
     */
    UFUNCTION(BlueprintCallable, Category="Economy|Cash")
    void GiveStartingCash(int32 PlayerId);

    // =========================================================================
    // Tuning Knob（数据驱动，coding-standards：gameplay 值不硬编码）
    // =========================================================================

    /** 每玩家起始现金（GDD Tuning：经典 1500；快速档 750 由上游配置覆盖）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy|Tuning")
    int32 StartingCash = 1500;

    /** 垄断无房地租翻倍系数（GDD：本系统拥有，MVP 锁定 2；跨系统旋钮，改动须协同所有权6/建房8）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy|Tuning")
    int32 MonopolyRentMultiplier = 2;

    /** 赎回费率分子（GDD Tuning unmortgage_fee=1/10 经典 10%；num=0=零 fee 合法 House Rules；
     *  ClampMax 束缚防 num×MV 溢出 int32，对抗 review CONCERN-1）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy|Tuning", meta=(ClampMin="0", ClampMax="100"))
    int32 UnmortgageFeeNum = 1;
    /** 赎回费率分母（必 ≥1 防除零，ClampMin 约束 editor + runtime guard 兜底 code/data-asset）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy|Tuning", meta=(ClampMin="1", ClampMax="1000"))
    int32 UnmortgageFeeDen = 10;

    /** F-8 建筑卖回率分子（GDD Tuning：经典 1/2 半价回收；num=0=零回收合法 House Rules）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy|Tuning", meta=(ClampMin="0", ClampMax="100"))
    int32 BuildingSellbackNum = 1;
    /** F-8 建筑卖回率分母（必 ≥1 防除零，ClampMin 约束 editor + runtime guard 兜底）。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy|Tuning", meta=(ClampMin="1", ClampMax="1000"))
    int32 BuildingSellbackDen = 2;

    // =========================================================================
    // 事件（econ-002，owner-held DYNAMIC_MULTICAST_DELEGATE，payload USTRUCT，ADR-0003）
    // =========================================================================

    /** 任何现金变动后广播（payload FCashChangedInfo，含 EChangeReason，AC-33）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnCashChangedSignature OnCashChanged;

    /** 收租转移完成时广播（payload FRentPaidInfo，AC-34；触发在 story-004/005）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnRentPaidSignature OnRentPaid;

    /** 强制扣款超现金时广播（payload FInsufficientFundsInfo，进 Raising Funds，AC-35）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnInsufficientFundsSignature OnInsufficientFunds;

    /** 破产现金侧转移完成时广播（payload FBankruptcyDeclaredInfo，2 字段，AC-36；触发在 story-009）。 */
    UPROPERTY(BlueprintAssignable, Category="Economy|Events")
    FOnBankruptcyDeclaredSignature OnBankruptcyDeclared;

private:
    /**
     * 解析 playerId → URentoPlayerState*（经回合2 FindPlayerById；用户裁定 Q2）。
     * @return 找到返回指针；World/turn/玩家缺失返回 nullptr。
     */
    URentoPlayerState* ResolvePlayer(int32 PlayerId) const;
};
