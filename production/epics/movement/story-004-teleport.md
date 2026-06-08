# Story 004: TeleportTo (paysGo/advanceOnZero/去坐牢 context)

> **Epic**: 移动 Movement
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/movement.md`（CR-2，P-2/P-3，M-1/M-2）
**Requirement**: `TR-move-004`、`TR-move-006`（SentToJail 强制）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0002（board StepsBetween，primary）· ADR-0003（EArrivalContext）
**ADR Decision Summary**: `TeleportTo(paysGo=true)` 归约 `steps_between` 走 Advance 发薪路径；`paysGo=false` 直达 `SetTileIndex` + passedGo 硬置 0；去坐牢强制 `arrivalContext==SentToJail`。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 整数；board `StepsBetween`/`GetTileCount` 上游接口（ADR-0002 已 done）。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0002/0003)**:
- Required: TeleportTo 归约 `steps_between`（不复制公式）；入口守界 ensure；去坐牢强制 SentToJail。
- Forbidden: `paysGo=false` 路径调 `AdvanceIndex`（直达不算步）；移动复制位置公式。
- Guardrail: target ∈ [0,N−1]（事件7 保证 + 入口 ensure 守界）。

---

## Acceptance Criteria

- [ ] **AC-10** `paysGo=true` 归约：from=5, target=15 → `steps=steps_between(5,15)=10` → `newIndex==15`。
- [ ] **AC-11a** `target==from`、`advanceOnZero=true` 整圈（M-1）：from=10,target=10,paysGo=true,advanceOnZero=true → `effective_steps=N=40` → newIndex==10, passedGo==1, 发薪 1 次。
- [ ] **AC-11b** `target==from`、`advanceOnZero=false` 原地：from=10,target=10,advanceOnZero=false → effective_steps=0 → newIndex==10, passedGo==0, 不发薪。
- [ ] **AC-12** `paysGo=false` 直达（去坐牢）：`TeleportTo(JailIndex,paysGo=false)` → newIndex==JailIndex、passedGo 强制 0、不 push；**不调 AdvanceIndex**，入口 ensure 守界。**须含跨 GO 向量**：from=38,JailIndex=10,N=40（跨 GO）→ 仍 passedGo==0、不 push。
- [ ] **AC-12b** 去坐牢 context 强制（M-2）：`TeleportTo(JailIndex,paysGo=false)` → `OnPawnLanded.arrivalContext == SentToJail`（移动强制置，非调用方注入）。
- [ ] **AC-13** [关键 guard] `target` 越界：`TeleportTo(-1/N,...)` → `ensure` 拒绝、不写、dev ensure。

---

## Implementation Notes

*Derived from ADR-0002/0003:*

- **`TeleportTo(player, target, paysGo, advanceOnZero=?)`**：
  - **`paysGo=true`**（前进到X类牌）：`steps = steps_between(from, target)`（board，CR-6 不复制公式）→ 走 Advance 路径（发薪门 P-1 适用）。`target==from` 时由调用方注入的 `advanceOnZero` 决定：true→`effective_steps=N`（整圈，AC-11a）/false→`effective_steps=0`（原地，AC-11b）。**movement 只执行注入的 advanceOnZero，不自决**（OQ-Move-1 默认值归事件7）。
  - **`paysGo=false`**（去坐牢）：`SetTileIndex(target)` 直达（**不调 AdvanceIndex**，不算步），`passedGo` 硬置 0、不 push 发薪（经典忠实"去坐牢不过 GO 领薪"，AC-12 含跨 GO 向量验证）。**强制 `arrivalContext=SentToJail`**（移动置，非调用方注入，AC-12b）。
- **入口守界**：`TeleportTo` 入口 `ensure(0<=target<N)`（target 由事件7 保证 ∈[0,N−1]，但守界兜底，AC-13）。
- 广播 `OnPawnMoveStarted`/`OnPawnLanded`（story-002 契约）；`SentToJail` 抑制 ResolvePhase 买地/收租归 player-turn AC-46（cross-ref，移动只透传/强制 context）。

---

## Out of Scope

- Story 003：Advance（TeleportTo paysGo=true 复用其路径）。Story 002：事件契约。
- 事件7 的"前进到X"/去坐牢牌触发 + advanceOnZero 注入（tile-events epic，OQ-T-Move-2）；player-turn 的 SentToJail 抑制（AC-46，已实现）。

---

## QA Test Cases

- **AC-10**：from=5,target=15 → steps=10 → newIndex==15。
- **AC-11a**：from=10,target=10,advanceOnZero=true → effective_steps=40 → (10,1) 发薪 1 次。
- **AC-11b**：from=10,target=10,advanceOnZero=false → effective_steps=0 → (10,0) 不发薪。
- **AC-12**：TeleportTo(JailIndex,paysGo=false)：from=38,JailIndex=10,N=40（跨 GO）→ newIndex==10,passedGo==0,push 0,不调 AdvanceIndex。
- **AC-12b**：TeleportTo(JailIndex,paysGo=false) → OnPawnLanded.arrivalContext==SentToJail（移动强制）。
- **AC-13**：TeleportTo(-1)/TeleportTo(N) → ensure 拒绝、不写（AddExpectedError）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/movement/teleport_test.cpp`（类目 `Rento.Movement.Teleport`）— 存在且通过。AC-12 须含跨 GO 向量。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（SetTileIndex）、Story 002（事件 + EArrivalContext SentToJail）；复用 Story 003 Advance 路径（paysGo=true）
- Unlocks: Story 005（三步有序）
