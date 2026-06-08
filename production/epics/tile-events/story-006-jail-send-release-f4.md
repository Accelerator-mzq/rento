# Story 006: 监狱 SendToJail + 出狱裁决 F-4 (JC-1~5) + 状态机不变式 + 非法降级

> **Epic**: 事件格 Tile Events
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/tile-events.md`（CR-7 入狱 / CR-9 出狱裁决，States 监狱状态机，F-4 JC-1~5）
**Requirement**: `TR-event-007`（监狱态字段 player-turn 受控写）、`TR-event-001`（入/出狱规则权威）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0001（宿主/受控写，primary）· ADR-0003（OnPlayerJailed/Released）
**ADR Decision Summary**: 监狱态字段 `bIsInJail`/`JailTurnsServed` 物理存于 player-turn PlayerState；本系统(7)拥有入/出狱规则与转换权威，经 player-turn 受控写改 `bIsInJail`、player-turn 据裁决机械计 `JailTurnsServed`（单源无双写）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 监狱态字段在 player-turn（已实现宿主）；本系统经受控写接口改写。`-nullrhi` 可测（mock player-turn 受控写 + economy/dice）。Integration：跨 player-turn(字段)/economy(保释)/dice(双点)。

**Control Manifest Rules (Core/Foundation/ADR-0001/0003)**:
- Required: 监狱态字段单源（player-turn 存储+计数）；本系统经受控写改 bIsInJail；入狱写路径唯一 SendToJail；非法决策降级留狱。
- Forbidden: 本系统自存监狱态（双源）；本系统自增 JailTurnsServed（计数权威=player-turn）；本系统发双点链增量/额外回合信号；扣成负现金/凭空消卡。
- Guardrail: `JailTurnsServed ∈ [0, MAX_JAIL_TURNS-1=2]`（无 served==3 留狱）。

---

## Acceptance Criteria

- [ ] **AC-45** SendToJail 入口：回合2 第3次双点调 SendToJail(playerId) → 经受控写 bIsInJail==true ∧ JailTurnsServed==0 ∧ TeleportTo(JailIndex,paysGo=false,SentToJail)×1。写路径唯一。
- [ ] **AC-35** JC-1 强制出狱：bIsInJail=true ∧ JailTurnsServed==2 ∧ bIsDouble=false → bForceRelease==true ∧ Debit(playerId,50) ∧ bIsInJail==false ∧ JailTurnsServed 不增。
- [ ] **AC-36** JC-2 出狱卡消费：bIsInJail=true ∧ bHoldsChanceOutCard=true，选"用 Chance 出狱卡" → bHoldsChanceOutCard==false ∧ 卡回 Chance 堆底 ∧ 经受控写 bIsInJail==false ∧ 无 Debit。
- [ ] **AC-37** JC-4 双点出狱（不发双点链）：bIsInJail=true ∧ JailTurnsServed∈{0,1}，mock RollDice bIsDouble=true → 经受控写 bIsInJail==false ∧ TeleportTo 以骰 Total 移动 ∧ **向 player-turn 发"双点链增量/额外回合"接口调用次数==0**。
- [ ] **AC-38** JC-5 留狱递增：bIsInJail=true ∧ JailTurnsServed==1 ∧ bIsDouble=false → JailTurnsServed==2 ∧ bIsInJail==true ∧ 无 Debit/TeleportTo。
- [ ] **AC-39** JC-3 付保释出狱：bIsInJail=true ∧ JailTurnsServed∈{0,1}，选"付保释" → Debit(playerId,50)×1 ∧ 经受控写 bIsInJail==false。
- [ ] **AC-40** JC-1∧JC-4 同时：JailTurnsServed==2 ∧ bIsDouble=true → 双点**免费**出狱（无 Debit）∧ 经受控写 bIsInJail==false ∧ 不发双点链增量。
- [ ] **AC-41** JC-2 优先 JC-1：JailTurnsServed==2 ∧ bHoldsChanceOutCard=true，选"用卡" → bHoldsChanceOutCard==false ∧ bIsInJail==false ∧ **无 Debit**。
- [ ] **AC-42** `JailTurnsServed∈[0,2]` 永不到3：连续在狱3回合非双点 → 0→1→2→第3回合强制出狱（无 served==3）。
- [ ] **AC-43** 出狱后 bIsInJail==false（四路径）：卡/保释/双点/强制各路径出狱后精确 false。
- [ ] **AC-44** 出狱卡持有与 bIsInJail 独立：bIsInJail=false ∧ bHoldsChanceOutCard=true（狱外持卡），落 Tax 格 → 正常缴税 ∧ bHoldsChanceOutCard 不被清除。
- [ ] **AC-62** DecideJailAction 非法 PayBail 降级：bIsInJail=true ∧ mock Cash<50，返回 PayBail → 不 Debit ∧ 7 裁定留狱（player-turn 计 +1）∧ bIsInJail 仍 true ∧ dev ensure+log。
- [ ] **AC-63** DecideJailAction 非法 UseCard 降级：bIsInJail=true ∧ HoldsAnyOutCard==false，返回 UseCard → 两标记不变==false ∧ 7 裁定留狱（+1）∧ bIsInJail 仍 true ∧ dev ensure+log。

---

## Implementation Notes

*Derived from ADR-0001/0003:*

- **入狱 `SendToJail(playerId)`（CR-7，写路径唯一 AC-45）**：三来源（落 GoToJail / 抽 GoToJailCard / 回合2 第3次双点）统一经此入口：调移动 `TeleportTo(JailIndex,paysGo=false,SentToJail)` + 经 player-turn **受控写接口**置 `bIsInJail=true`、player-turn 据裁决置 `JailTurnsServed=0`。**监狱态字段存 player-turn（OQ-Event-1），本系统不自存**。
- **出狱裁决 F-4 JC-1~5（CR-9，AC-35~41）**：玩家三选项——① 用出狱卡（持有任一）→ 裁定出狱、卡回对应堆底、经受控写 bIsInJail=false、无 Debit；② 付保释 `JAIL_BAIL_AMOUNT=50` → 经济 Debit 成功后置 bIsInJail=false；③ 掷骰碰运气 → bIsDouble→免费出狱用该骰移动（**不进双点链、不给额外回合** AC-37/40）/非双点→留狱（**player-turn 计 +1，本系统不自增** AC-38）。第3回合（JailTurnsServed==2）非双点 → 强制付保释（AC-35）。优先级：卡 > 强制付费（AC-41）；双点 > 强制付费（AC-40）。
- **状态机不变式（AC-42/43/44）**：`JailTurnsServed ∈ [0,2]` 永不到3（==2 下次非双点必强制出狱）；出狱（四路径）后 bIsInJail==false；出狱卡持有与 bIsInJail 独立（狱外持卡，AC-44）。
- **非法决策降级（AC-62/63，承接 player-turn CR-8#4/AC-39b）**：`DecideJailAction` 返回 PayBail 但 Cash<50 / UseCard 但无卡 → 本系统校验合法性，不可行则**降级留狱**（player-turn 计 +1）+ dev ensure+log，**不扣成负现金、不凭空消卡**。
- **双点不进双点链（AC-37/40）**：本系统消费 `bIsDouble` 仅判免费出狱，**不向回合2 发 `ConsecutiveDoubles++`/`grantExtraTurn` 信号**（player-turn OQ-T-1 / dice OQ-T-Dice-3）。

---

## Out of Scope

- Story 002：GoToJail 路由（调 SendToJail）。Story 003：GoToJailCard（调 SendToJail）。Story 007：OnPlayerJailed/Released 广播 + AC-50 集成。
- player-turn 的 JailTurnsServed 机械计数 + 受控写接口 + ConsecutiveDoubles（player-turn epic 已实现）；经济保释 Debit（economy）；骰子 bIsDouble（dice 已 done）。

---

## QA Test Cases

- **AC-45**：SendToJail → bIsInJail==true ∧ JailTurnsServed==0 ∧ TeleportTo(JailIndex,false,SentToJail)×1。
- **AC-35~41**：JC-1~5 各路径（强制/卡/双点/留狱/保释/同时/优先级），见 AC 定值。AC-37/40 不发双点链信号（mock player-turn 调用==0）。
- **AC-42**：0→1→2→第3回合强制出狱（无 served==3）。
- **AC-43**：四路径出狱后 bIsInJail==false。
- **AC-44**：狱外持卡落 Tax → 正常缴税 ∧ 持卡不清。
- **AC-62/63**：非法 PayBail(Cash<50)/UseCard(无卡) → 降级留狱(+1) ∧ bIsInJail 仍 true ∧ 不扣负/不消卡 ∧ ensure+log。

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/tile-events/jail_send_release_f4_test.cpp`（类目 `Rento.TileEvents.JailSendReleaseF4`）— 存在且通过（mock player-turn 受控写 + economy/dice）。AC-37 不发双点链 + AC-62/63 降级须坐实。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（出狱卡持有/回堆）、Story 002（GoToJail 路由）
- Unlocks: Story 007（OnPlayerJailed/Released 广播）
