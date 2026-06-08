# Story 002: 买地事务 Buy (前置前置化, 6→5)

> **Epic**: 地产所有权 Property Ownership
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/property-ownership.md`（CR-2）
**Requirement**: `TR-prop-009`（买地方向 6→5）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0013（容器写/封装，primary）· ADR-0007（5↔6 方向）
**ADR Decision Summary**: 买地事务 `Buy(tile,player)` 归本系统（6）；前置校验先行（owner==INDEX_NONE）→ 调 economy `Debit`（6→5）→ 置 owner + 广播；economy 不反调 6（5↔6 无环）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 单线程同步，前置已验后归属写不可能失败（无 2PC）。`-nullrhi` 可测（mock economy.Debit）。

**Control Manifest Rules (Core/ADR-0013/0007)**:
- Required: 写权威状态（owner）→ C++；事务方向 6→5（本系统调 economy Debit）；前置校验先行。
- Forbidden: economy 反调 6（登记归属）；5↔6 环。
- Guardrail: 单线程前置前置化消 2PC。

---

## Acceptance Criteria

- [ ] **AC-5** 正常买地全路径：tile 属 Property、owner==INDEX_NONE、mock economy.Debit=Success，`Buy(tile,player)` → 内部顺序 = 先验前置 → 调 economy.Debit(恰 1 次) → 置 owner；`GetOwner(tile)==player` ∧ `IsMortgaged==false` ∧ `OnOwnershipChanged(tile,INDEX_NONE,player)` 恰 1 次。
- [ ] **AC-6** 前置守门——只能买无主格：owner!=INDEX_NONE → **前置即拒绝（不调 economy.Debit）**、owner 不变、无广播、dev ensure（断言 mock Debit 调用==0）。
- [ ] **AC-7** [Advisory·code-review / ADR-0013 升 Logic] economy 不反向调 6：economy 实现内对本系统买地/归属写接口（`Buy`/`TransferOwnership`/owner-map 写）调用==0（防 5↔6 环，静态扫描）。
- [ ] **AC-8** 原子性两失败半：① 前置通过 + Debit=Success → `GetOwner==buyer`；② Debit=Failure（现金不足）→ 事务在 Debit 后终止，`GetOwner` 仍 `INDEX_NONE`、无广播。两子例独立 fixture。
- [ ] **AC-8b** 单线程无 2PC 残态：owner==INDEX_NONE 前置已过 + Debit Success → 归属写不可能失败（无"Debit 成功但 owner 未改"中间态）。

---

## Implementation Notes

*Derived from ADR-0013/0007:*

- **`Buy(tileIndex, playerId)`**（唯一公开买地入口，由回合2 ResolvePhase 决策点调）。执行顺序（前置前置化，消 2PC）：
  1. **前置校验先行**：该格可购买（Property/Railroad/Utility）且 `owner==INDEX_NONE`。违反 → 拒绝整个事务（不调 economy、不改 owner、dev ensure），返失败（AC-6）。
  2. 前置通过 → 调 economy `Debit(playerId, PurchasePrice)`（**6→5，economy 只执行不回调本系统**）。
  3. Debit 返成功 → 置 `TileStates[tile].OwnerPlayerId = playerId` + 广播 `OnOwnershipChanged(tile, INDEX_NONE, playerId)`（AC-5）。
- **原子性（AC-8/8b）**：前置（owner==NONE）在第 1 步已验，单线程下第 3 步归属写不可能失败（无并发改 owner），不需 2PC 回滚。Debit 失败 → 第 2 步终止 owner 保 INDEX_NONE；前置失败 → 第 1 步终止不扣款。
- **economy 不反调 6（AC-7，5↔6 无环）**：economy `Debit` 内部不调本系统；归属 map 由本系统在 Debit 成功后自登记（economy 不通知 6）。code-review/静态扫描 economy 无对本系统写接口调用（ADR-0013 升 Logic）。
- `PurchasePrice` 读棋盘（base，经济侧已知）；本系统不持价格、不执行扣款逻辑（归 economy）、不持买/不买决策点（归回合2）。

---

## Out of Scope

- Story 001：容器+事件契约（本 story 用其 OnOwnershipChanged）。
- economy 的 Debit 实现（economy story-007 买地腿）；回合2 的买地决策点（player-turn）。
- 抵押/赎回（story-003）；破产移交（story-006）。

---

## QA Test Cases

- **AC-5**：Given Property, owner==NONE, mock Debit=Success；When Buy(tile,player)；Then 顺序=前置→Debit(×1)→置owner；GetOwner==player ∧ IsMortgaged==false ∧ OnOwnershipChanged(tile,NONE,player)×1。
- **AC-6**：Given owner!=NONE；When Buy；Then 前置拒绝、mock Debit 调用==0、owner 不变、无广播、ensure。
- **AC-7**：[静态扫描] economy 无对本系统 Buy/TransferOwnership/owner-map 写调用==0。
- **AC-8**：① Debit=Success → GetOwner==buyer；② Debit=Failure → GetOwner 仍 INDEX_NONE、无广播。独立 fixture。
- **AC-8b**：owner==NONE 前置过 + Debit Success → 无"Debit 成功 owner 未改"中间态。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/property-ownership/buy_transaction_test.cpp`（类目 `Rento.Property.BuyTransaction`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（容器/OnOwnershipChanged）
- Unlocks: None（与 003 并行）
