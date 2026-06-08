# Story 006: 破产移交 (TransferOwnership/ReturnTileToBank) + 批量原子性 + 重入锁

> **Epic**: 地产所有权 Property Ownership
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/property-ownership.md`（CR-1 States 破产级联，economy F-11 触发方=系统#9）
**Requirement**: `TR-prop-011`（逐格广播+重入锁）、`TR-prop-012`（批量移交原子性）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0013（批量原子性/RAII 锁，primary）· ADR-0003（逐格广播）
**ADR Decision Summary**: 批量移交先全量前置校验后逐格写（全或全无）+ 单 `FTileOwnershipState` 赋值（owner 与 bIsMortgaged 同格原子清零）+ RAII scope guard 重入锁无条件解除；逐格广播在锁内。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `Broadcast()` 同步执行监听器；RAII scope guard（`ON_SCOPE_EXIT`）保锁无条件解除。`-nullrhi` 可测（spy 监听器回写拦截）。Integration：触发方=破产9（经回合2 编排），用 spy/mock。

**Control Manifest Rules (Core/Foundation/ADR-0013/0003)**:
- Required: owner+bIsMortgaged 同格原子清零（单记录赋值）；批量全或全无；重入锁 RAII 无条件解除；逐格广播在锁内。
- Forbidden: 部分转移中间态；只清 owner 留 bIsMortgaged（非法 Unowned）；锁挂起（死锁）；监听器级联回写。
- Guardrail: creditor != INDEX_NONE（转债权人合法性）。

---

## Acceptance Criteria

- [ ] **AC-33** 收租破产 in-kind——保留抵押：debtor 持 A(bIsMortgaged==true)+B(==false)，9 调 `TransferOwnership(A,debtor,creditor)`+`TransferOwnership(B,...)` → `GetOwner(A)==creditor` ∧ `IsMortgaged(A)==true`（随地转移不清）∧ `GetOwner(B)==creditor` ∧ `IsMortgaged(B)==false`。
- [ ] **AC-33b** [批量原子性] 全部地全或全无：debtor 持 3 格，批量 in-kind 移交 → 3 格 owner 全改 creditor、无一遗漏（无部分转移中间态）；每格各广播一次 `OnOwnershipChanged`（共 3 次）。
- [ ] **AC-33c** [重入锁] 级联防回写：批量移交进行中（`bIsMidBankruptcyTransfer==true`），监听器在 `OnOwnershipChanged` 回调内尝试 `Buy`/`Mortgage`/`TransferOwnership` 受控写 → 该写被拒绝（dev ensure+log）、owner map 不被级联篡改；移交完成后锁解除、写恢复。
- [ ] **AC-33d** 不可购买格守门：TileType ∈ 非可购买格，`TransferOwnership`/`ReturnTileToBank` → 拒绝、dev ensure。
- [ ] **AC-33e** [关键 Unowned 不变式] creditor 合法性守门：某 mortgaged 格，`TransferOwnership(tile, from, INDEX_NONE)` → 前置拒绝、owner 与 bIsMortgaged 均不变、dev ensure（绝不产生 `owner==INDEX_NONE && bIsMortgaged==true`）。
- [ ] **AC-34** 银行破产回退（单格）：债权方==Bank 的某格 bIsMortgaged==true，`ReturnTileToBank(tile)` → `owner==INDEX_NONE` ∧ `bIsMortgaged==false`（**同格原子清零**）。
- [ ] **AC-34b** [批量原子性] 银行回退守不变式：debtor 持 5 格（含 2 mortgaged），批量 `ReturnTileToBank` → 5 格全 `owner==INDEX_NONE` ∧ 全 `bIsMortgaged==false`（绝不留非法态）。
- [ ] **AC-35** [Advisory·code-review / ADR-0013 升 Logic] 移交由系统#9 触发：spy/检查 `TransferOwnership`/`ReturnTileToBank` 调用方是 9 编排路径，economy 不直接调。

---

## Implementation Notes

*Derived from ADR-0013/0003:*

- **`TransferOwnership(tile, from, to)` / `ReturnTileToBank(tile)`**（供破产9 调，9→6）。批量移交协议（ADR-0013 决策③）：
  1. **先全量前置校验整批**：in-kind 须 `creditor != INDEX_NONE`（AC-33e）、各格可购买（AC-33d）、owner==debtor。**任一格失败 → 整批中止，不写任何一格**（dev ensure+log）。
  2. **逐格写 = 单 `FTileOwnershipState` 赋值**：收租 in-kind `TileStates[t].OwnerPlayerId = creditor`（bIsMortgaged 随地保留，AC-33）；银行回退 `TileStates[t] = FTileOwnershipState{}`（owner 与 bIsMortgaged 同一赋值清零，**类型层不可能只清一半**，AC-34/34b）。
  3. **重入锁 `bIsMidBankruptcyTransfer`，RAII scope guard 无条件解除**：批量全程置 true，锁期内任何外部受控写（Buy/Mortgage/Unmortgage/TransferOwnership/ReturnTileToBank）被拒（dev ensure+log，AC-33c）。锁用 RAII（`ON_SCOPE_EXIT`/栈对象析构）→ 无论全成/中途 ensure/异常，离作用域**无条件复位**，绝不挂起死锁。
  4. **逐格广播在锁内**（AC-33b：N 格 = N 次 `OnOwnershipChanged`）。
- **economy 不直接调（AC-35）**：地产 owner 写归 9→6 链，economy 只管现金侧（economy story-009）。spy 断言调用方=9 编排路径。
- deferred broadcast queue 列 Alpha 替代（ADR-0013），MVP 逐格广播+重入锁。

---

## Out of Scope

- 破产9 的破产宣告/出局裁决 + 编排（bankruptcy epic）；economy 的现金侧 F-11（economy story-009）；建房8 的 LiquidateAllBuildings（building epic）；回合2 编排（player-turn）。
- Story 001–005：容器/事务/谓词/快照。

---

## QA Test Cases

- **AC-33**：debtor 持 A(mortgaged)+B → TransferOwnership 各 → GetOwner(A/B)==creditor ∧ IsMortgaged(A)==true ∧ IsMortgaged(B)==false。
- **AC-33b**：debtor 持 3 格 → 批量 in-kind → 3 格全 creditor、无遗漏、OnOwnershipChanged×3。
- **AC-33c**：移交中监听器回调内 Buy/Mortgage → 被拒（ensure+log）、owner map 不篡改；完成后锁解、写恢复。变异：去 RAII guard → 中途 ensure 后锁挂起 → 后续 Buy 被永久拒（FAIL），还原复绿。
- **AC-33d**：非可购买格 TransferOwnership/ReturnTileToBank → 拒绝、ensure。
- **AC-33e**：mortgaged 格 TransferOwnership(tile,from,INDEX_NONE) → 前置拒绝、owner+bIsMortgaged 均不变、ensure（绝不产生非法 Unowned）。
- **AC-34/34b**：单格/批量 ReturnTileToBank → owner==INDEX_NONE ∧ bIsMortgaged==false（同格原子，5 格全清不留半）。
- **AC-35**：[spy/静态] 调用方=9 编排，economy 不直调。

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/property-ownership/bankruptcy_transfer_test.cpp`（类目 `Rento.Property.BankruptcyTransfer`）— 存在且通过（mock 9 触发 + spy 监听器）。AC-33c 重入锁 + AC-34b 同格原子须变异坐实。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（容器/事件）、Story 002（Buy 受控写被锁拦截）、Story 003（Mortgage 受控写被锁拦截）
- Unlocks: None
