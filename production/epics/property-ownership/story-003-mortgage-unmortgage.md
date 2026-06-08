# Story 003: 抵押/赎回事务 Mortgage/Unmortgage (前置前置化)

> **Epic**: 地产所有权 Property Ownership
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/property-ownership.md`（CR-3）
**Requirement**: `TR-prop-009`（方向 6→5）、`TR-prop-001`（bIsMortgaged 状态）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0013（容器/封装，primary）· ADR-0007（6→5）
**ADR Decision Summary**: `Mortgage`/`Unmortgage(tile,player)` 归本系统；前置（owner==player + 状态匹配）先行 → 调 economy `Credit`/`Debit`（6→5）→ 置/清 `bIsMortgaged` + 广播；不校验 house_count（读不到，归决策点）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `bIsMortgaged` 内含于 `FTileOwnershipState`；`-nullrhi` 可测（mock economy）。

**Control Manifest Rules (Core/ADR-0013/0007)**:
- Required: 标记写 → C++；前置校验先行（owner==player + 状态匹配）；Unowned 不变式维持。
- Forbidden: economy 在 Credit 内校验 is_mortgaged/house_count；本系统校验 house_count（读不到，6↔8 环）；无主格置 bIsMortgaged。
- Guardrail: Unowned 恒 bIsMortgaged==false。

---

## Acceptance Criteria

- [ ] **AC-9** 正常抵押：owner==player、bIsMortgaged==false、mock Credit=Success，`Mortgage(tile,player)` → 顺序=前置→Credit→置标记；`IsMortgaged==true` ∧ `OnMortgageChanged(tile,true)` 恰 1 次。（GIVEN 不含 house_count==0：归决策点）。
- [ ] **AC-10** 重复抵押守门：bIsMortgaged==true → 前置即拒绝（不调 Credit）、不变、无广播、dev ensure。
- [ ] **AC-11** 正常赎回：bIsMortgaged==true、mock Debit=Success，`Unmortgage(tile,player)` → `IsMortgaged==false` ∧ `OnMortgageChanged(tile,false)` 恰 1 次。
- [ ] **AC-11b** 赎回原子性 Debit 失败：bIsMortgaged==true、owner==player、mock Debit=Failure → `IsMortgaged` 仍 true、owner 不变、`OnMortgageChanged` 调用==0。
- [ ] **AC-12** 赎回守门——非抵押态：bIsMortgaged==false → 前置即拒绝、不变、无广播、dev ensure。
- [ ] **AC-13** 入口守门——只能 owner 操作：owner==playerA，`Mortgage(tile, playerB)` → 前置拒绝（owner!=playerId）、不变、不调 economy、dev ensure。
- [ ] **AC-13b** [关键 Unowned 不变式] 无主格不可抵押：owner==INDEX_NONE，`Mortgage(tile, anyPlayer)` → 前置拒绝、bIsMortgaged 仍 false、不调 economy、dev ensure。
- [ ] **AC-14** [Advisory·code-review] 6 不校验 house_count：`Mortgage` 实现内无 `house_count>0`/读 8 判断。
- [ ] **AC-14b** 6 不据 house 门控：owner==player、bIsMortgaged==false、mock Credit=Success——**不论是否带房**，`Mortgage` → 照置 bIsMortgaged=true ∧ OnMortgageChanged 恰 1 次，且实现路径无 house_count 读取/分支（spy 对系统#8 调用==0）。

---

## Implementation Notes

*Derived from ADR-0013/0007:*

- **`Mortgage(tileIndex, playerId)` / `Unmortgage(tileIndex, playerId)`**（带 playerId 用于 owner 校验，由回合2/AI 决策点调）。执行顺序：
  1. **前置校验先行**：`owner==playerId`（只能 owner，AC-13）且状态匹配（抵押要求 `bIsMortgaged==false`、赎回要求 `==true`）。违反 → 拒绝、不调 economy、不改标记、dev ensure。
  2. 调 economy `Credit(playerId, MortgageValue)`（抵押）/`Debit(playerId, 赎回费=经济 F-6)`（赎回）（**6→5**）。
  3. 现金返成功 → 自置/清 `TileStates[tile].bIsMortgaged` + 广播 `OnMortgageChanged`；现金失败（赎回付不起）→ 终止、标记不变（AC-11b）。
- **Unowned 不变式维持（AC-13b）**：`Mortgage` 前置要求 `owner==playerId≠NONE`，无主格调 `Mortgage` 被前置拒绝（owner==INDEX_NONE → owner!=playerId）→ 保证 Unowned 恒 bIsMortgaged==false。
- **不校验 house_count（AC-14/14b，防 6↔8 环）**：本系统读不到 `house_count`（归建房8）；带房不可抵押前置由**决策点（回合2/AI10）在调 `Mortgage()` 前**校验。若被错误调用（带房抵押），照置标记但 dev ensure+log 暴露上游 bug。spy 断言实现路径无对系统#8 调用。
- economy 不反调 6（同 story-002 AC-7，bIsMortgaged 由本系统自置，economy 不通知 6）。

---

## Out of Scope

- Story 001：容器/OnMortgageChanged。Story 002：买地。
- economy 的 Credit/Debit + 赎回费 F-6（economy story-006）；决策点 house_count==0 前置（player-turn/AI）；建房8 卖房（building epic）。

---

## QA Test Cases

- **AC-9**：owner==player, bIsMortgaged==false, Credit=Success → 顺序前置→Credit→置；IsMortgaged==true ∧ OnMortgageChanged(tile,true)×1。
- **AC-10**：bIsMortgaged==true → Mortgage 前置拒绝、不调 Credit、ensure。
- **AC-11**：bIsMortgaged==true, Debit=Success → IsMortgaged==false ∧ OnMortgageChanged(tile,false)×1。
- **AC-11b**：Debit=Failure → IsMortgaged 仍 true、OnMortgageChanged==0。
- **AC-12**：bIsMortgaged==false → Unmortgage 前置拒绝、ensure。
- **AC-13**：owner==A, Mortgage(tile,B) → 前置拒绝（owner!=playerId）、ensure。
- **AC-13b**：owner==INDEX_NONE, Mortgage(tile,any) → 前置拒绝、bIsMortgaged 仍 false、ensure。
- **AC-14/14b**：[code-review + spy] Mortgage 无 house_count 读取/分支；带房/不带房均照置标记，对系统#8 调用==0。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/property-ownership/mortgage_unmortgage_test.cpp`（类目 `Rento.Property.MortgageUnmortgage`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（容器/OnMortgageChanged）
- Unlocks: None（与 002/004/005 并行）
