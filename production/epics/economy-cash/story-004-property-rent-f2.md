# Story 004: 地产租金 F-2 (piecewise, ×2 仅无房 base, 抵押短路)

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/economy-cash.md`（F-2 / CR-3）
**Requirement**: `TR-econ-014`（确定性）、`TR-econ-017`（读 RentTable base）、`TR-econ-016`（数据包络 note）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0014（整数确定性，primary）· ADR-0006（snapshot 输入 is_monopoly/house_count）· ADR-0007
**ADR Decision Summary**: 金钱整数纯净；`is_monopoly`/`is_mortgaged` 来自地产6 快照、`house_count` 来自建房8，经回合2 `ResolvePhase` 聚合传入（防 6↔8 环）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 整数路径；数组查找守 clamp。`-nullrhi` 可测（mock 输入快照）。

**Control Manifest Rules (Core)**:
- Required: `is_mortgaged` 短路置 0 **先于**表查找；数组查找守 `clamp`；金钱整数纯净。
- Forbidden: 经济直读建房8 `house_count`（5→8 环）；用 float。
- Guardrail: 垄断翻倍 `monopoly_rent_multiplier=2`（本系统拥有），只作用无房 base。

---

## Acceptance Criteria

- [ ] **AC-9** 短路抵押先于表查找：is_mortgaged=true（任意 house/monopoly）→ rent==0（不查 RentTable）。
- [ ] **AC-10** 翻倍仅 house_count==0：monopoly ∧ house==0 ∧ RentTable[0]=2 → rent==4；monopoly ∧ house==1 → `RentTable[1]`（不翻倍）。
- [ ] **AC-11** [关键] 酒店绝不翻倍：monopoly ∧ house==5 → `RentTable[5]`（无 ×2）。
- [ ] **AC-12** 非垄断按房数：is_monopoly=false ∧ house=3 → `RentTable[3]`。
- [ ] **AC-13** house_count clamp：house=6 → `clamp(0,5)`→`RentTable[5]` 不崩 + ensure；house=−1 → `RentTable[0]` + ensure。

---

## Implementation Notes

*Derived from ADR-0014/0006:*

- F-2 piecewise（GDD line 150-152）：
  ```
  if is_mortgaged:                        rent = 0          // 短路先于表查找
  elif is_monopoly and house_count == 0:  rent = RentTable[0] × monopoly_rent_multiplier
  else:                                   rent = RentTable[clamp(house_count, 0, 5)]
  ```
- `monopoly_rent_multiplier = 2`（本系统拥有的锁定系数）；**垄断翻倍只作用无房 base（index 0）**，酒店（house==5）绝不翻倍（AC-11）。
- `is_mortgaged`/`is_monopoly` 由地产6 快照提供，`house_count` 由建房8 提供，二者经回合2 `ResolvePhase` 聚合后作参数传入本系统算租（**economy 不直读 6/8**，防环；ADR-0006）。
- `house_count` 越界（>5 或 <0，建房8 bug）→ `clamp(0,5)` 兜底 + dev `ensure`（AC-13）。
- 收租转移用 story-001 的原子 Debit+Credit（单一 R），广播 `OnRentPaid` + 2×`OnCashChanged`（reason=Rent，story-002 契约）。`rent==0`（抵押/自有/无主）→ 不转移、不广播（AC-5/37）。
- **数据包络（econ-016，board propagate 债）**：F-2 假设 `RentTable[1] ≥ RentTable[0]×2`（防建首房降租）由 board 加载期 warning 校验（ADR-0014 确认 warning、board 执行未落）；本 story 不校验包络，按式忠实照算。

---

## Out of Scope

- Story 005：车站/公用租金 F-3/F-4。
- 地产6 的 is_monopoly/is_mortgaged 判定（property epic）；建房8 的 house_count（building epic）；回合2 的快照聚合（player-turn）。
- board 的 RentTable 包络 warning 校验（board epic，propagate 债）。

---

## QA Test Cases

- **AC-9**：Given is_mortgaged=true, monopoly=true, house=3；Then rent==0（不查 RentTable，spy 断言无表访问）。
- **AC-10**：Given monopoly, house=0, RentTable[0]=2；Then rent==4。Given monopoly, house=1；Then rent==RentTable[1]（不翻倍）。
- **AC-11**：Given monopoly, house=5；Then rent==RentTable[5]（无 ×2）。
- **AC-12**：Given monopoly=false, house=3；Then rent==RentTable[3]。
- **AC-13**：Given house=6；Then clamp→RentTable[5] 不崩 + ensure（AddExpectedError）。Given house=−1；Then RentTable[0] + ensure。
- 确定性：固定输入两次算 rent 位级一致（无 float）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/property_rent_f2_test.cpp`（类目 `Rento.Economy.PropertyRentF2`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（Debit/Credit）、Story 002（OnRentPaid/OnCashChanged）
- Unlocks: None（与 005 并行）
