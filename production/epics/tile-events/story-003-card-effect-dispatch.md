# Story 003: 牌效果执行 dispatch (Bank*/Move*/GoToJailCard/GetOutOfJailFree)

> **Epic**: 事件格 Tile Events
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/tile-events.md`（CR-4 牌效果类型）
**Requirement**: `TR-event-011`（ECardEffectType dispatch 落 C++）、`TR-event-002`（Move* 程间非重入）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0007（C++ dispatch，primary）· ADR-0003
**ADR Decision Summary**: `ECardEffectType` 10 类效果 dispatch 落 C++（避 BP 大 switch 保守性）；金钱效果一律经经济 `Credit`/`Debit`；Move* 二次移动程间非重入。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: C++ dispatch；`-nullrhi` 可测（mock economy/movement）。Move* 二次移动 deferred 队列防同栈重入（unreal Issue-1）。

**Control Manifest Rules (Core/ADR-0007)**:
- Required: 写权威/dispatch 落 C++；金钱效果经经济（本系统不持现金）；二次移动非重入（不在 OnPawnLanded 回调内发起）。
- Forbidden: 绕过经济直接操作现金；牌附加发薪（发薪只走 paysGo→F-1）。
- Guardrail: 破产触发（付不起）归经济/破产9，本系统只发起 Debit。

---

## Acceptance Criteria

- [ ] **AC-15** BankCredit：抽到 BankCredit(50) → mock Credit(playerId,50) 恰 1 次 ∧ 无其他金钱调用。
- [ ] **AC-16** BankDebit：抽到 BankDebit(150) → mock Debit(playerId,150) 恰 1 次。
- [ ] **AC-17** MoveToTile + 双重发薪防御：MoveToTile(target=0,paysGo=true) → mock TeleportTo(playerId,0,paysGo=true,AdvanceCard) 恰 1 次 ∧ **无额外 Credit**（发薪只走 paysGo→F-1）。
- [ ] **AC-18** MoveRelative 后退不过 GO：MoveRelative(-3) → mock Advance(playerId,-3) ∧ paysGo==false ∧ 无 Credit。
- [ ] **AC-19** GoToJailCard：抽到 → mock TeleportTo(JailIndex,paysGo=false,SentToJail) 恰 1 次 ∧ bIsInJail==true ∧ JailTurnsServed==0。
- [ ] **AC-20** GetOutOfJailFree：从 Chance 堆抽到（抽前 bHoldsChanceOutCard==false）→ bHoldsChanceOutCard==true ∧ 无 Credit/Debit/TeleportTo（只置持有）。
- [ ] **AC-47** Go 格双重发薪防御：MoveToTile(GO_INDEX,paysGo=true) → TeleportTo(GO_INDEX,paysGo=true) ∧ **无额外 Credit(playerId,SalaryAmount)**。

---

## Implementation Notes

*Derived from ADR-0007:*

- **`ECardEffectType` dispatch（C++，AC dispatch 落 C++ 避 BP 大 switch）**：每张牌 = `ECardEffectType` + 参数。本 story 实现：
  - `BankCredit`/`BankDebit` → 经济 `Credit`/`Debit`（AC-15/16）。
  - `MoveToTile` → 移动 `TeleportTo(target, paysGo, AdvanceCard)`（AC-17，paysGo=牌面）。
  - `MoveRelative` → 移动 `Advance`（过 GO 计 passedGo）/后退直达（AC-18，后退 paysGo=false 不发薪）。
  - `GoToJailCard` → `SendToJail`（story-006，AC-19，对称 CR-7）。
  - `GetOutOfJailFree` → 置 `bHoldsChanceOutCard`/`bHoldsChestOutCard`（按堆，story-001，AC-20，只置持有无金钱/移动）。
  - （`MoveToNearest`/`RepairFee`/`CollectFromEach`/`PayToEach` 在 story-004/005）。
- **金钱效果经经济（防双重发薪 AC-17/47）**：所有 Bank*/现金转移经经济 `Credit`/`Debit`（本系统不持现金）；牌**不附加发薪**（发薪只走 paysGo→F-1，移动层）。
- **Move* 程间非重入（event-002，unreal Issue-1）**：MoveToTile/MoveRelative 触发的二次移动**不在 `OnPawnLanded` 回调内直接发起**（同步 Broadcast 下同栈重入），入 deferred 队列待前程调用栈返回后处理（或 `bIsResolvingTileEvent` 守门）。卡触发二次移动落地后由该次落地 ResolvePhase 正常处理。
- 破产触发（付不起 Debit）归经济/破产9，本系统只发起 Debit、不裁决破产。

---

## Out of Scope

- Story 002：路由（本 story 执行路由后抽到的牌效果）。Story 004：CollectFromEach/PayToEach（零和）。Story 005：MoveToNearest/RepairFee。Story 006：SendToJail 入狱转换本体（本 story GoToJailCard→调 SendToJail）。
- 经济 Credit/Debit（economy epic）；移动 TeleportTo/Advance（movement epic）。

---

## QA Test Cases

- **AC-15/16**：BankCredit(50)→Credit(p,50)×1；BankDebit(150)→Debit(p,150)×1。
- **AC-17/47**：MoveToTile(0,paysGo=true)→TeleportTo(p,0,true,AdvanceCard)×1 ∧ 无额外 Credit（GO 不双发）。
- **AC-18**：MoveRelative(-3)→Advance(p,-3) ∧ paysGo==false ∧ 无 Credit。
- **AC-19**：GoToJailCard→TeleportTo(JailIndex,false,SentToJail)×1 ∧ bIsInJail==true ∧ JailTurnsServed==0。
- **AC-20**：GetOutOfJailFree(Chance)→bHoldsChanceOutCard==true ∧ 无金钱/移动。
- 非重入：二次移动不在 OnPawnLanded 回调内发起（deferred 队列）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/tile-events/card_effect_dispatch_test.cpp`（类目 `Rento.TileEvents.CardEffectDispatch`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（抽牌/出狱卡持有）、Story 002（路由）
- Unlocks: None（与 004/005 并行；GoToJailCard 依赖 006 SendToJail）
