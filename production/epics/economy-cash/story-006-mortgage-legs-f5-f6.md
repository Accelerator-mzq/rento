# Story 006: 抵押/赎回现金腿 F-5/F-6 + 显示读接口 + 无套利

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Complete
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

- [x] **AC-19** `payout==MortgageValue`（F-5）：MV=100 → Credit 额==100。
- [x] **AC-20** [Advisory·code-review] 抵押前置不归 economy：`Credit` 实现内**无** `is_mortgaged`/`house_count` 读取或拒绝分支（前置归地产6 `Mortgage()`/决策点）。
- [x] **AC-21** [整数 ceil] `unmortgage_cost = MV + ceil(MV×fee_num/fee_den)`：MV=100, fee=1/10 → 110；MV=75 → fee=ceil(7.5)=8 → **83**（非 82.5/82）；MV=1 → fee=1 → **2**（fee 恒≥1）。
- [x] **AC-22** 赎回现金不足：Cash < unmortgage_cost → 赎回不可用、不 Debit、地保持抵押。
- [x] **AC-42** [Logic·可数学验证] 抵押无套利：对任意 `MV>0` 与 `fee_rate≥0`，一抵一赎净现金 == `−fee_rate×MV ≤ 0`（恒非正）。MV=100,fee=1/10 → 净 −10；MV=60 → 净 −6。

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
**Required evidence**: `tests/unit/economy-cash/mortgage_legs_test.cpp`（物理 `Source/rentoTests/Private/MortgageLegsTest.cpp`，类目 `Rento.Economy.MortgageLegs`）— 存在且通过。
**Status**: [x] 6 TC 全通过（TC1 放款/TC2 ceil赎回价+纯函数/TC3 MV≤0防御/TC4 不足不可用/TC5 无套利+赎回成功/TC6 除零防御）

---

## Completion Notes（2026-06-09，mode-A 主会话亲写）

**实现**（3 方法，承 econ-003~005 caller-injected）：
- `GetUnmortgageCostForDisplay(MV)` BlueprintPure 纯函数 = `MV + ceil(MV×num/den)`（整数 ceil ADR-0014，赎回价**单一口径** #17 显示+赎回腿同源，CR-5/OQ-PC-8）。AC-21。
- `MortgagePayout(player, MV)` F-5 腿 = `Credit(player, MV, Mortgage)`（薄 wrapper，AC-19）。
- `UnmortgagePayment(player, MV)` F-6 腿 = cost 计算 → **pre-check GetCash≥cost** → Debit(Unmortgage)。AC-22。
- `UnmortgageFeeNum/Den=1/10` data-driven UPROPERTY（+ ClampMin/Max meta）。

**🔴 关键语义裁定（主会话）**：**UnmortgagePayment 自愿行为** —— 现金不足时**显式 pre-check** return false（不 Debit、不广播、**不触 OnInsufficientFunds**），区别于 rent/tax 的 Debit 不足走 raising-funds（CR-5/CR-7：raising-funds 仅强制扣款入，赎回是玩家自愿可不做的行为）。变异坐实（去 pre-check→Debit 走 InsufficientFunds→TC4 FAIL）。

**整数 ceil 验算**：`fee=(MV×num+den−1)/den`；MV=100→fee=10→cost=110；MV=75→fee=ceil(7.5)=8→cost=83（非 floor 82）；MV=1→fee=1→cost=2（fee≥1 MV≥1 自动）。**AC-42 无套利**：抵一赎净=+MV−(MV+fee)=−fee≤0 恒非正（MV=100 净−10/MV=60 净−6）。

**对抗 review（unreal-specialist）= APPROVED-WITH-CONCERNS**（无 blocker，逐行验证 ceil/无套利/自愿语义/AC-20）。采纳：
- **CONCERN-2**（UnmortgageFeeDen=0 除零崩溃）→ runtime guard `den≤0→零 fee 退化(cost=MV)+dev log`（meta 仅约束 editor，code/data-asset 仍可置 0 故 runtime 必需）+ TC6。
- **CONCERN-1**（FeeNum×MV 溢出）→ UPROPERTY 加 ClampMin/Max meta + 溢出境界注释（MV≤PurchasePrice board 校验）。
- **CONCERN-3 不采纳**：FeeNum=0 零 fee 是**合法 House Rules 调参**（GDD Tuning unmortgage_fee 范围 0/1–2/10），「fee 恒≥1」仅 default 1/10 下的丸め退化防止，非绝对不变式。
- **AC-20 grep 核实**：Credit 函数体内零 is_mortgaged/house_count/bIsMortgaged 命中（前置归地产6/决策点）。

**全证（主会话亲跑，铁律）**：
- Build Succeeded；全量 **304/304**（298 基线 + 6 MortgageLegs，0 Fail/0 notRun/EXIT 0，零回归，`Saved/TestReport_econ006_hardened`）。
- **变异坐实非 vacuous**：变异A（ceil→floor）→ **仅 TC2 FAIL**（MV=75/1 非整数检出，`mutA`）；变异B（去 UnmortgagePayment pre-check）→ **仅 TC4 FAIL**（自愿语义 OnInsufficientFunds 不变式，`mutB`）；均还原复绿。
- 权威纯净 grep：economy 零随机 + AC-20 Credit 内零抵押/房数读取。

**协调债 / propagate**：
- 地产6 的 Mortgage()/Unmortgage() 事务调本系统 MortgagePayout/UnmortgagePayment（6→5）+ 自置/解除 bIsMortgaged（property epic）；house_count==0 抵押前置归决策点/地产6（economy 读不到，AC-20）。
- 地产卡 UI #17 调 GetUnmortgageCostForDisplay 显示赎回价（presentation，CR-5/OQ-PC-8）。
- **解锁 econ-009**（凑钱状态机用 Mortgage 现金腿 + 赎回费口径）。

---

## Dependencies

- Depends on: Story 001（Credit/Debit）、Story 002（OnCashChanged reason=Mortgage/Unmortgage）
- Unlocks: Story 009（凑钱状态机用 Mortgage 现金腿 + 赎回费口径）
