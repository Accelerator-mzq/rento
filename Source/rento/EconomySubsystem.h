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
