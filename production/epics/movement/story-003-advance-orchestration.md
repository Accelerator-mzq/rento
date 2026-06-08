# Story 003: Advance 编排 + 发薪 push 门 + Total 不投递 + 越界告警

> **Epic**: 移动 Movement
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/movement.md`（CR-2/CR-3/CR-3.1，P-1，F-1/F-2）
**Requirement**: `TR-move-003`、`TR-move-005`、`TR-move-009`、`TR-move-010`、`TR-move-017`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0002（board AdvanceIndex 原子元组，primary）· ADR-0015（dice_total Total 不投递 + 越界告警）· ADR-0007
**ADR Decision Summary**: `Advance` 调 board `AdvanceIndex(from,steps)→(newIndex,passedGo)`（board 拥有位置数学）；移动只消费 `Total` 作 `steps` 不缓存/不向经济 push（ADR-0015）；越界告警不静默吞冒泡。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 整数索引；board `AdvanceIndex` 是上游接口（ADR-0002，已 done）。`-nullrhi` 可测（mock board + spy 经济 push）。

**Control Manifest Rules (Core/ADR-0002/0015)**:
- Required: 调 board `AdvanceIndex`（不复制 board 位置数学）；发薪仅 `passed_go>0` 才 push；Total 仅作 steps 不投递；越界告警不静默吞。
- Forbidden: 移动复制 `mod`/`floor` 位置公式（CR-6）；移动缓存/push `Total` 给经济（ADR-0015）；移动 clamp passedGo（保环路落点）。
- Guardrail: passedGo 数值兜底委托 ADR-0014（economy clamp[0,1000]）。

---

## Acceptance Criteria

- [ ] **AC-5** 基础位移：from=5, steps=7 → `newIndex==12`, `passedGo==0`。
- [ ] **AC-6** 过 GO：from=38, steps=5, N=40 → `newIndex==3`, `passedGo==1`。
- [ ] **AC-7** [关键] 发薪 push 门 P-1 四值边界（验 `>` 非 `≥`）：passedGo=0→不 push；=1→push (1,SalaryAmount) 恰 1 次；=2→push (2,...)；=−1→不 push。
- [ ] **AC-7b** CR-3.1 `Total` 仅作位移、不投递：`Advance(steps=Total, context=DiceMove)` → 移动不对经济发起任何 `Total` push/cache（spy 经济接口投递次数==0）；`Total` 仅当 `steps`。
- [ ] **AC-8** 后退位移：from=2, steps=−5, N=40 → `newIndex==37`, `passedGo==−1` → 不发薪。
- [ ] **AC-9a** `steps=0` 早返：`Advance(0)` → 原地、passedGo=0、不 push、`OnPawnLanded` 0 次。
- [ ] **AC-9b** [Advisory·code-review] 掷骰路径 `ensure(steps≥2)` 存在（Dev 暴露异常牌）。
- [ ] **AC-9c** [关键] GO 格落地不双发：from=35, steps=5, N=40 → newIndex==0 ∧ passedGo==1 ∧ 发薪 push **恰 1 次**（CollectSalary 标记不触发第二次）。
- [ ] **AC-23** `from` 越界鲁棒：`Advance(from=−1,steps=5,N=40)` → board 公式数学鲁棒不崩、落点按 board 定义 + dev ensure（不抑制）。
- [ ] **AC-24** [Advisory·code-review] steps 超界冒泡（M-4）：board `|steps|>2N` 告警经移动**不抑制地**冒泡（回合2/结构化日志）。

---

## Implementation Notes

*Derived from ADR-0002/0015:*

- `Advance(player, steps, paysGo=true, context=DiceMove)`：调 board `AdvanceIndex(from, steps) → (newIndex, passedGo)`（**board 拥有公式，移动不复制 `mod`/`floor`**，CR-6/AC-22）；`steps` 可负（后退）、可超 N（多圈）。落点经 `SetTileIndex(newIndex)`（story-001）。
- **发薪 push 门 P-1**：仅 `paysGo==true ∧ passedGo>0` 才 push `(passedGo, SalaryAmount)` 给经济（CR-3，经济 F-1 算 `passedGo×SalaryAmount`）。`passedGo≤0`（未过/逆向）或 `paysGo==false` 不 push（AC-7 验 `>` 非 `≥`）。**移动不拥有金额、不拥有 passed_go 算法**，只拥有"何时 push"。`SalaryAmount` 读 board `GetTileData(0).SalaryAmount`。
- **Total 不投递（AC-7b，ADR-0015）**：掷骰移动只把骰 `Total` 当 `steps` 消费算落点，**绝不缓存/不向经济 push `Total`**。Utility 租 `dice_total` 由经济在 ResolvePhase 从回合2 holder PULL（economy story-005），非移动投递。spy 断言经济投递次数==0。
- **GO 格不双发（AC-9c）**：GO 格 `SpecialAction=CollectSalary` 仅 UI 标记，发薪唯一由 `passedGo>0` 授权，不触发第二次 push。
- **steps=0 早返（AC-9a）**：原地、不重触发落地结算（`OnPawnLanded` 0 次）。`ensure(steps≥2)` 仅 Dev 暴露异常牌（AC-9b）。
- **越界鲁棒/告警（ADR-0015）**：`from` 越界信任 board 公式 `((A%N)+N)%N` 兜底（board AC-34）不崩 + dev ensure 不抑制（AC-23）；`|steps|>2N` board 告警经移动**不静默吞**冒泡回合2/日志（AC-24）。移动**不 clamp** passedGo（保环路落点），数值兜底委托 ADR-0014。

---

## Out of Scope

- Story 002：事件契约（本 story 触发 OnPawnMoveStarted/OnPawnLanded，契约在 002）。
- Story 004：TeleportTo。Story 005：三步有序契约（本 story 的写/push/广播顺序断言在 005）。
- board 的 AdvanceIndex 实现（board epic，ADR-0002 已 done）；经济 F-1 发薪消费（economy story-003）；经济 dice_total PULL（economy story-005）。

---

## QA Test Cases

- **AC-5/6**：from=5,steps=7→(12,0)；from=38,steps=5,N=40→(3,1)。
- **AC-7**：passedGo=0→push 0 次；=1→push(1,Salary)×1；=2→push(2,...)；=−1→push 0 次。
- **AC-7b**：Advance(steps=Total,DiceMove) → 经济 Total 投递 spy==0；Total 仅作 steps。
- **AC-8**：from=2,steps=−5,N=40→(37,−1) 不发薪。
- **AC-9a**：Advance(0)→原地,passedGo=0,push 0,OnPawnLanded 0 次。
- **AC-9c**：from=35,steps=5,N=40→(0,1) ∧ 发薪 push 恰 1 次（CollectSalary 不双发）。
- **AC-23**：Advance(from=−1,steps=5)→board 鲁棒不崩 + ensure（AddExpectedError）。
- **AC-24**：[code-review] |steps|>2N 告警冒泡不静默吞。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/movement/advance_orchestration_test.cpp`（类目 `Rento.Movement.AdvanceOrchestration`）— 存在且通过。AC-7b 须 spy 坐实经济投递==0。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（SetTileIndex）、Story 002（OnPawnMoveStarted/OnPawnLanded 契约）
- Unlocks: Story 005（三步有序）
