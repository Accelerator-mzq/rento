# Story 004: 零和聚合 F-1 (CollectFromEach / PayToEach)

> **Epic**: 事件格 Tile Events
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/tile-events.md`（CR-4，F-1 零和聚合）
**Requirement**: `TR-event-011`（dispatch）；经济金钱效果（economy OQ-T-Econ-3 金钱侧）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0003（事件/经济调用，primary）· ADR-0007
**ADR Decision Summary**: CollectFromEach/PayToEach 零和逐笔经经济 `Credit`/`Debit`（无银行中转）；ActivePlayers 单次快照防途中不平衡。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 整数零和；`-nullrhi` 可测（mock economy + ActivePlayers）。

**Control Manifest Rules (Core)**:
- Required: 零和逐笔经经济（无银行中转）；ActivePlayers 单次快照（不途中重算）；金钱经经济。
- Forbidden: 汇总合并 Debit（须逐笔保 economy F-11 单债权人前提）；银行中转零和。
- Guardrail: other_count==0 退化 total==0 不报错。

---

## Acceptance Criteria

- [ ] **AC-22** CollectFromEach 总额：Active={P1,P2,P3,P4}，acting=P1，amount=50 → other_count==3 ∧ Debit 对 P2/P3/P4 各 50 ∧ Credit(P1, 实收Σ)。
- [ ] **AC-23** PayToEach 零和逐笔：acting=P1，other={P2,P3}，amount=30 → Debit(P1,30) 调**两次**（逐笔非汇总60）∧ Credit(P2,30)+Credit(P3,30) 各一次 ∧ 无银行中转 ∧ 总流出60==总流入60。
- [ ] **AC-24** other_count==0 退化：Active={P1}，amount=50 → total==0 ∧ 无 Debit/Credit ∧ 不报错。
- [ ] **AC-25** 2人局 CollectFromEach：Active={P1,P2}，acting=P1，amount=50 → Debit(P2,50)×1 ∧ Credit(P1,50)×1。
- [ ] **AC-26** ActivePlayers 单次快照：F-1 途中 mock 模拟 P3 破产移除 → 仍按**快照时刻** other_count 执行（不途中重算）。
- [ ] **AC-48** 多债权人 PayToEach 逐笔（OQ-Event-4 防御）：acting=P1，other={P2,P3}，amount=30 → Debit(P1,30) 按回合顺序先 P2 债再 P3 债 ∧ 每笔独立（不合并60）∧ Credit 各独立。
- [ ] **AC-53** 牌金钱效果经经济：BankCredit(50)/BankDebit(150)/CollectFromEach(30) 各执行 → 分别 Credit/Debit；本系统无绕过 economy 的内部现金操作。

---

## Implementation Notes

*Derived from ADR-0003:*

- **`CollectFromEach(acting, amount)`（F-1）**：向其他每位在局玩家各收 amount → `Debit(each other, amount)` + `Credit(acting, Σ)`（AC-22/25）。
- **`PayToEach(acting, amount)`（F-1）**：向其他每位各付 → `Debit(acting, amount)` **逐笔**（每债权人一次，非汇总）+ `Credit(each other, amount)`（AC-23/48）。**逐笔保 economy F-11 单债权人前提**（破产时单债权人，不合并）。**无银行中转**（玩家间直接零和）。
- **ActivePlayers 单次快照（AC-26）**：F-1 开始时取 ActivePlayers 快照，全程按快照 other_count 执行（不途中重算，防两次查询间 P 破产移除致不平衡）。
- **金钱经经济（AC-53）**：所有现金转移经经济 `Credit`/`Debit`，本系统不持现金、无绕过 economy 的内部操作。
- other_count==0（单人局/全破产）退化 total==0、不调金钱、不报错（AC-24）。

---

## Out of Scope

- Story 003：Bank*/Move* 效果（本 story 只零和聚合）。Story 005：MoveToNearest/RepairFee。
- 经济 Credit/Debit（economy epic）；ActivePlayers 真值（回合2/破产9）。

---

## QA Test Cases

- **AC-22**：Active 4 人，P1 收 50 → Debit(P2/P3/P4,50) 各 ∧ Credit(P1, Σ=150)。
- **AC-23**：P1 付 {P2,P3} 各 30 → Debit(P1,30)×2（逐笔）∧ Credit(P2,30)+Credit(P3,30) ∧ 无银行中转 ∧ 60==60。
- **AC-24**：Active={P1} → total==0 ∧ 无金钱 ∧ 不报错。
- **AC-25**：Active 2 人，P1 收 50 → Debit(P2,50)×1 ∧ Credit(P1,50)×1。
- **AC-26**：途中 P3 破产移除 → 按快照 other_count 执行（不重算）。
- **AC-48**：PayToEach 逐笔按回合顺序、各独立（不合并60）。
- **AC-53**：BankCredit/BankDebit/CollectFromEach 经经济，无绕过。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/tile-events/collect_pay_each_f1_test.cpp`（类目 `Rento.TileEvents.CollectPayEachF1`）— 存在且通过。AC-23 逐笔/AC-26 快照须坐实。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（牌堆）、Story 002（路由）
- Unlocks: None（与 003/005 并行）
