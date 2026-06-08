# Story 004: 地产租金 F-2 (piecewise, ×2 仅无房 base, 抵押短路)

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Complete
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

- [x] **AC-9** 短路抵押先于表查找：is_mortgaged=true（任意 house/monopoly）→ rent==0（不查 RentTable）。
- [x] **AC-10** 翻倍仅 house_count==0：monopoly ∧ house==0 ∧ RentTable[0]=2 → rent==4；monopoly ∧ house==1 → `RentTable[1]`（不翻倍）。
- [x] **AC-11** [关键] 酒店绝不翻倍：monopoly ∧ house==5 → `RentTable[5]`（无 ×2）。
- [x] **AC-12** 非垄断按房数：is_monopoly=false ∧ house=3 → `RentTable[3]`。
- [x] **AC-13** house_count clamp：house=6 → `clamp(0,5)`→`RentTable[5]` 不崩 + ensure；house=−1 → `RentTable[0]` + ensure。

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
**Required evidence**: `tests/unit/economy-cash/property_rent_f2_test.cpp`（物理 `Source/rentoTests/Private/PropertyRentF2Test.cpp`，类目 `Rento.Economy.PropertyRentF2`）— 存在且通过。
**Status**: [x] 5 TC 全通过（TC1 抵押短路/TC2 ×2仅无房+酒店/TC3 非垄断按房数/TC4 clamp+dev信号+决定性/TC5 SettleRent+OnRentPaid）

---

## Completion Notes（2026-06-09，mode-A 主会话亲写）

**实现**（caller-injected，用户裁定 2026-06-09）：
- `UEconomySubsystem::ComputePropertyRent(bIsMortgaged, bIsMonopoly, HouseCount, RentTable)` 纯 F-2 piecewise（RentTable 注入，economy 不直读 board/6/8）。
- `UEconomySubsystem::SettleRent(PayerId, PayeeId, RentAmount, TileIndex)` 共有收租结算（TransferCash + OnRentPaid，econ-005 F-3/F-4 复用）—— **discharge econ-002「OnRentPaid 触发在 004/005」**。
- `MonopolyRentMultiplier=2` data-driven UPROPERTY（GDD「本系统拥有」+ MVP 锁定）；`PROPERTY_HOUSE_MAX=5` 内部常量。

**仕样判断（GDD F-2 厳密）**：×2 用 **raw HouseCount==0** 判定（非 clamp 后）；house=−1+monopoly → raw −1≠0 落 else → RentTable[0]（不 ×2，AC-13）；酒店 house=5 → raw≠0 → RentTable[5]（绝不翻倍，AC-11 关键）。index 双重 clamp（[0,5]∩[0,Num-1]）防 RentTable 短于 6 越界；空表早返 0。house 越界 **UE_LOG(Error) 替 ensure**（同 econ-003 逸脱，G2+决定性+AddExpectedError）。

**🔴 用户裁定逸脱**：`IEconomyResolver::CalculateRent(FRentInput)` 的 board.RentTable 读取 + seam 配线 **defer**（FRentInput 的 is_monopoly/house_count 来自未实现 property6/building8，现 pt-007 仍 mock）。RentTable caller-injected，与 econ-003 一致。

**对抗 review（unreal-specialist）= APPROVED-WITH-CONCERNS**（无 blocker）。采纳：
- **C-2**（spy LastReason 上写仅留末腿）→ EconomyEventSpy 加 `CashChangedReasons` 序列，TC5 两腿独立断言 reason==Rent。
- **C-3/N-1/N-2** 注释（TC4 决定性不触越界 / RentTable[0]×Mult 溢出境界=两因子静态数据非 runtime / SettleRent RentAmount≥0 不变式）。
- **C-1 不采纳为代码，记协调债**（见下）：`IEconomyResolver` production binding 缺失（现 player-turn EconomyResolver 未注入时 safe no-op return 0；defer 妥当，但须追踪防 property6/building8 实现时 AC-7 素通）。N-1 不加 runtime guard（两因子静态数据，非 econ-003 的 runtime passed_go 可滥用）。

**全证（主会话亲跑，铁律）**：
- Build Succeeded；全量 **292/292**（287 基线 + 5 PropertyRentF2，0 Fail/0 notRun/EXIT 0，零回归含 spy 变更，`Saved/TestReport_econ004_hardened`）。
- **变异坐实非 vacuous**：变异A（去抵押短路）→ **仅 TC1 FAIL**（精密杀，`Saved/TestReport_econ004_mutA`）；变异B（垄断 ×2 无条件含酒店）→ TC1/2/4 FAIL（酒店不翻倍 AC-11 坐实，`Saved/TestReport_econ004_mutB`）；均还原复绿。
- 权威纯净 grep：economy 模块零 `FMath::Rand`/全局随机。

**协调债 / propagate**：
- **🔴 C-1（追踪债）**：`UEconomySubsystem` 尚未实现 `IEconomyResolver`，`CalculateRent(FRentInput)` production binding 缺失。当 property6/building8 epic 落地（提供 is_monopoly/house_count）时，须新 wiring story：economy 实现 IEconomyResolver（读 board.GetTileData(TileIndex).RentTable + dispatch ComputePropertyRent/Railroad/Utility）+ 注入 player-turn `SetEconomyResolver`，闭合 pt-007 AC-7。**未配线期 player-turn SettleRentOnArrival 是 safe no-op（return 0）**。
- **econ-005 复用 SettleRent**（F-3/F-4 收租転移+OnRentPaid 同 004）。
- board RentTable 包络 warning（RentTable[1]≥RentTable[0]×2）待 board epic（propagate 债，econ-016）。

---

## Dependencies

- Depends on: Story 001（Debit/Credit）、Story 002（OnRentPaid/OnCashChanged）
- Unlocks: None（与 005 并行）
