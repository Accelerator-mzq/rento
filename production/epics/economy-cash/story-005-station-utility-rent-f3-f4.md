# Story 005: 车站/公用租金 F-3/F-4 (count guards + dice_total PULL)

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/economy-cash.md`（F-3 / F-4 / CR-3）
**Requirement**: `TR-econ-014`（确定性）、`TR-econ-015`（DiceMult 上界 board）、`TR-econ-017`（读 RentTable/DiceMultiplierTable）；消费 holder PULL（ADR-0015）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0015（dice_total holder PULL，primary）· ADR-0014（确定性/DiceMult 溢出）· ADR-0006
**ADR Decision Summary**: Utility 租 `dice_total` 由经济在 `ResolvePhase` 从回合2 holder `GetCurrentRollTotal()` **PULL**（移动不 push/不缓存；事件额外骰经 `SetCurrentRollContext` 注入）；`DiceMultiplierTable[i] ≤ DICE_MULT_MAX=1,000,000` 加载期 fatal（board 执行，已落 AC-23j）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: holder 机制已实现（pt-005/006/007，`GetCurrentRollTotal()` 可调）；整数路径；count guard。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0015)**:
- Required: Utility `dice_total` 经回合2 holder PULL（不缓存）；数组守 `count≥1` 否则 0；金钱整数纯净。
- Forbidden: 经济缓存 dice_total / 反向读移动；用 float；查 `RentTable[−1]`。
- Guardrail: `12 × DICE_MULT_MAX ≤ INT32_MAX`（board 校验）。

---

## Acceptance Criteria

- [ ] **AC-14** `rent=RentTable[station_count−1]`：station=2 → `RentTable[1]`；station=4 → `RentTable[3]`。
- [ ] **AC-15** [关键 guard] station=0（所有权6 竞态）→ rent==0、不查 `[−1]`、不崩 + dev ensure（`AddExpectedError`）。
- [ ] **AC-16** `rent=dice_total × DiceMultiplierTable[utility_count−1]`：count=1, dice=7, mult=4 → 28。
- [ ] **AC-17** [关键 guard] utility_count=0 → rent==0、不查 `[−1]`、不崩 + ensure。
- [ ] **AC-18** dice_total 是显式参数非缓存：两次传不同 dice_total（7/11，同 count）→ rent 各按传入算（28/44），证明不读缓存骰点（"前进到最近公用"额外骰正确）。

---

## Implementation Notes

*Derived from ADR-0015/0014:*

- **F-3 车站**：`rent = RentTable[clamp(station_count−1, 0, 3)]`，**守 `station_count≥1` 否则 rent=0**（防 `RentTable[−1]` 崩；count 来自地产6，不假设其正确，AC-15）。
- **F-4 公用**：`rent = dice_total × DiceMultiplierTable[clamp(utility_count−1, 0, 1)]`，守 `utility_count≥1` 否则 0（AC-17）。
- **`dice_total` 经 holder PULL（ADR-0015）**：F-4 在 `ResolvePhase` 算租时经回合2 `GetCurrentRollTotal()` PULL 当前程骰点（**不缓存于经济成员、不反读移动**）。**显式参数传入 F-4**（AC-18 验两次不同 dice_total 各按传入算，证不读缓存）。"前进到最近公用"额外骰经事件7 `SetCurrentRollContext` 注入 holder（event-010），F-4 PULL 取注入值（不串程）。
- `station_count`/`utility_count` 来自地产6 快照（经回合2 聚合，ADR-0006）。
- **求值顺序**：count guard（≥1）先于表查找；`DiceMultiplierTable` 上界由 board 加载校验（已落 AC-23j，本 story 不重复）。
- 收租转移 + 广播同 story-004（原子 Debit+Credit + OnRentPaid + 2×OnCashChanged reason=Rent）。

---

## Out of Scope

- Story 004：地产租金 F-2。
- 回合2 的 holder 实现（pt-005/006/007 已 done）；事件7 的 SetCurrentRollContext 注入（tile-events epic，机制已实现）；board 的 DiceMult 上界校验（已落 AC-23j）。

---

## QA Test Cases

- **AC-14**：Given station_count=2；Then rent==RentTable[1]。station=4 → RentTable[3]。
- **AC-15**：Given station_count=0；Then rent==0 ∧ 不查 `[−1]` ∧ ensure（AddExpectedError）。
- **AC-16**：Given utility_count=1, dice_total=7, mult=[4,10]；Then rent==28。
- **AC-17**：Given utility_count=0；Then rent==0 ∧ 不查 `[−1]` ∧ ensure。
- **AC-18**：Given 同 utility_count=1, mult=[4,10]；When dice_total=7 → rent==28；dice_total=11 → rent==44；Then 各按传入算（mock holder 返回不同 Total），证不读缓存。Edge：holder 经 GetCurrentRollTotal() PULL（mock 注入），economy 无 dice_total 成员缓存（code-review 负断言）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/station_utility_rent_test.cpp`（类目 `Rento.Economy.StationUtilityRent`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（Debit/Credit）、Story 002（OnRentPaid）；引用 ADR-0015 holder（pt-005/006/007 已 done）
- Unlocks: None（与 004 并行）
