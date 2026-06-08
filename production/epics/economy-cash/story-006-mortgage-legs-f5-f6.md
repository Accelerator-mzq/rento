# Story 006: 抵押/赎回现金腿 F-5/F-6 + 显示读接口 + 无套利

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/economy-cash.md`（F-5 / F-6 / CR-5）
**Requirement**: `TR-econ-014`（整数 ceil）、`TR-econ-004`（GetUnmortgageCostForDisplay BlueprintCallable）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0014（整数 ceil 确定性，primary）· ADR-0007
**ADR Decision Summary**: 赎回费整数 ceil `(MV×fee_num + fee_den−1)/fee_den`（零 float）；抵押/赎回事务归地产6，economy 只执行现金腿（6→5），不校验抵押/无房前置。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 整数 ceil；纯函数 `GetUnmortgageCostForDisplay` 无副作用。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0014/0007)**:
- Required: 赎回费整数 ceil（`(x×num+den−1)/den`）；零 float；现金腿被地产6 调（6→5）。
- Forbidden: economy 在 `Credit` 内校验 is_mortgaged/house_count（读不到，前置归6/决策点）；economy 反调6。
- Guardrail: `fee` 恒 ≥1（堵免费赎回退化）。

---

## Acceptance Criteria

- [ ] **AC-19** `payout==MortgageValue`（F-5）：MV=100 → Credit 额==100。
- [ ] **AC-20** [Advisory·code-review] 抵押前置不归 economy：`Credit` 实现内**无** `is_mortgaged`/`house_count` 读取或拒绝分支（前置归地产6 `Mortgage()`/决策点）。
- [ ] **AC-21** [整数 ceil] `unmortgage_cost = MV + ceil(MV×fee_num/fee_den)`：MV=100, fee=1/10 → 110；MV=75 → fee=ceil(7.5)=8 → **83**（非 82.5/82）；MV=1 → fee=1 → **2**（fee 恒≥1）。
- [ ] **AC-22** 赎回现金不足：Cash < unmortgage_cost → 赎回不可用、不 Debit、地保持抵押。
- [ ] **AC-42** [Logic·可数学验证] 抵押无套利：对任意 `MV>0` 与 `fee_rate≥0`，一抵一赎净现金 == `−fee_rate×MV ≤ 0`（恒非正）。MV=100,fee=1/10 → 净 −10；MV=60 → 净 −6。

---

## Implementation Notes

*Derived from ADR-0014:*

- **F-5 抵押放款**：`payout = MortgageValue`（`Credit`，被地产6 `Mortgage()` 事务调用，6→5）。economy 只执行 `Credit`，**不标记/不通知6**（`bIsMortgaged` 由6自置，保无环）。**前置（未抵押/无房）由6/决策点保证，非 economy 在 `Credit` 内校验**（AC-20；`Credit` 是通用入账接口，读不到抵押标记/房数）。
- **F-6 赎回费整数 ceil**：`fee = (MortgageValue × fee_num + fee_den − 1) / fee_den`（fee_num/fee_den=1/10，经典 10%）；`unmortgage_cost = MortgageValue + fee`。被地产6 `Unmortgage()` 调用 `Debit(owner, unmortgage_cost)`。fee 恒 ≥1（AC-21 MV=1→fee=1）。
- **`GetUnmortgageCostForDisplay(int32 MortgageValue) → int32`**（`UFUNCTION(BlueprintCallable/BlueprintPure)`，纯函数）：返 F-6 `unmortgage_cost`，**无副作用、不改状态、不触事件、不读归属/不反调6**。供地产卡 UI(17) 显示赎回价（#17 不自算 ceil，取整口径单源归本系统，CR-5/OQ-PC-8）。
- **无套利（AC-42）**：抵一赎净 = `+MV − (MV + fee) = −fee ≤ 0`，恒非正（数学可证伪，固定 fixture 验）。
- 现金腿成功后广播 `OnCashChanged`（reason=Mortgage/Unmortgage）。现金不足赎回 → 不 Debit、地保持抵押（AC-22）。

---

## Out of Scope

- 地产6 的 `Mortgage()`/`Unmortgage()` 事务 + `bIsMortgaged` 标记（property epic）；决策点的 house_count==0 前置（player-turn/AI）。
- 卖房（建房8）；地产卡 UI(17) 的赎回价呈现（presentation）。

---

## QA Test Cases

- **AC-19**：Given MV=100；When 地产6 调 Credit(owner,100,Mortgage)；Then payout==100 ∧ Cash+=100。
- **AC-20**：[code-review] `Credit` 实现内 grep 无 is_mortgaged/house_count 读取/分支。
- **AC-21**：MV=100 → cost=110；MV=75 → fee=ceil(7.5)=8 → cost=83；MV=1 → fee=1 → cost=2。两次冷启动位级一致（无 float）。
- **AC-22**：Given Cash=50, cost=110；When Unmortgage；Then 不 Debit ∧ 地保持抵押 ∧ OnCashChanged 0 次。
- **AC-42**：MV=100,fee=1/10 → 抵押 +100、赎回 −110、净 −10；MV=60 → 净 −6。断言净恒 ≤0。
- `GetUnmortgageCostForDisplay(75)` → 83（纯函数，无副作用，spy 断言不触事件/不改 Cash）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/mortgage_legs_test.cpp`（类目 `Rento.Economy.MortgageLegs`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（Credit/Debit）、Story 002（OnCashChanged reason=Mortgage/Unmortgage）
- Unlocks: Story 009（凑钱状态机用 Mortgage 现金腿 + 赎回费口径）
