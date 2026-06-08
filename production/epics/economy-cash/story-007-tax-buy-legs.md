# Story 007: 缴税 F-7 + 买地现金腿 CR-4

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/economy-cash.md`（F-7 / CR-4 / CR-6）
**Requirement**: `TR-econ-017`（读 TaxAmount/PurchasePrice）；CR-4 买地现金腿（6→5 方向）、CR-6 缴税 sink
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0007（BP-vs-C++ / 5↔6 方向，primary）· ADR-0001
**ADR Decision Summary**: 买地事务归地产6，6 在事务内调 economy `Debit(PurchasePrice)` 执行扣款腿（6→5）；economy 只扣款、不登记归属、不通知6（保 5↔6 无环）。缴税 flat `Debit(TaxAmount)`，款流向银行 sink（蒸发）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 整数 flat debit；读 board base。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0007)**:
- Required: 买地/缴税现金腿被地产6/事件7 调（6→5 / 7→5）；写 Cash 落 C++。
- Forbidden: economy 反调6（登记归属）；economy 写 owner map。
- Guardrail: 税 sink 蒸发（不进任何玩家，CR-6/CR-8 银行模型）。

---

## Acceptance Criteria

- [ ] **AC-23** `debit==TaxAmount`（flat，F-7）：TaxAmount=200 → Debit==200；Cash<200 → 进 CR-7 Raising Funds（同 AC-1 路径）。
- [ ] **买地现金腿（CR-4）**：地产6 `Buy` 事务调 economy `Debit(player, PurchasePrice)`：校验 `Cash ≥ PurchasePrice` → 成立则 Debit 并返成功；现金不足则购买不可用（**不进 Raising Funds** — 买地可选非强制债务）。economy **不登记归属、不通知6**（5↔6 无环）。

---

## Implementation Notes

*Derived from ADR-0007:*

- **F-7 缴税**：玩家落 `Tax` 格，`Debit(player, TaxAmount)`（TaxAmount 读棋盘 `GetTileData(index).TaxAmount`，base）。款流向银行 sink（蒸发，不 Credit 任何玩家，CR-6）。不足额 → 进 CR-7 Raising Funds（强制扣款，同 AC-1，状态机在 story-009）。reason=Tax。
- **CR-4 买地现金腿**：买地事务 `Buy(tileIndex, playerId)` 归地产6（决策点归回合2 `ResolvePhase`）。6 在事务内调 economy `Debit(player, PurchasePrice)`：economy **校验 `Cash ≥ PurchasePrice`**，成立则 `Debit` 返成功，不足则返失败（购买不可用，**非强制债务、不进 Raising Funds**）。economy **只执行扣款**，**不登记归属、不通知6**（归属 map 由6在 Debit 成功后自登记，5↔6 无环，ADR-0013/0007）。reason=Purchase。
- 买地 Debit 与缴税 Debit 区别：税是**强制**（不足进 Raising Funds），买地是**可选**（不足购买不可用，不进债务流程）。
- 广播 `OnCashChanged`（reason=Tax/Purchase，story-002 契约）。

---

## Out of Scope

- 地产6 的 `Buy` 事务 + 归属登记（property epic，ADR-0013）；回合2 的买地决策点（player-turn）；事件7 的税触发（tile-events）。
- Raising Funds 状态机（story-009）。

---

## QA Test Cases

- **AC-23**：Given TaxAmount=200, Cash=500；When 缴税；Then Debit(200) ∧ Cash==300 ∧ 款不进任何玩家（spy 无 Credit）∧ OnCashChanged(reason=Tax)。Given Cash=150；Then 进 Raising Funds（OnInsufficientFunds，同 AC-1）。
- **买地腿**：Given Cash=500, PurchasePrice=300；When 地产6 调 Debit(player,300)；Then Cash==200 ∧ 返成功 ∧ economy 不登记归属（spy 无 owner 写、无通知6）。Given Cash=200, PurchasePrice=300；Then 返失败 ∧ 不 Debit ∧ **不进 Raising Funds**（买地可选）。
- code-review：economy 买地/缴税路径无 owner map 写、无反调6（AC-7/AC-31 静态扫描，ADR-0013 升 Logic）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/tax_buy_legs_test.cpp`（类目 `Rento.Economy.TaxBuyLegs`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（Debit/GetCash）、Story 002（OnCashChanged reason=Tax/Purchase）
- Unlocks: None
