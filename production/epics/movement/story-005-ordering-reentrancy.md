# Story 005: 三步有序契约 + 程间非重入 + 接口稳定

> **Epic**: 移动 Movement
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/movement.md`（CR-4 逻辑先于表现 / 程间原子性）
**Requirement**: `TR-move-011`、`TR-move-012`、`TR-move-015`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0001（单帧同步 / 程间非重入，primary）· ADR-0007（UFUNCTION 接口）
**ADR Decision Summary**: 单帧同步三步有序契约（①受控写 < ②发薪 push < ③落地广播）；逻辑先于表现、动画不回授；程间原子性/非重入（Advance/TeleportTo 完整返回后回合方发起下一程，双点链排序归回合2）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: spy/mock 记录操作序断言顺序；`Advance`/`TeleportTo`/`GetTileIndex` UFUNCTION 编译由 CI 构建验证。`-nullrhi` 可测。Integration：跨移动/经济/回合2 编排时序。

**Control Manifest Rules (Foundation/ADR-0001/0007)**:
- Required: 单帧同步结算（逻辑先于表现，动画不回授）；程间原子性（完整返回后回合方发起下一程）；公共接口 UFUNCTION(BlueprintCallable)。
- Forbidden: 状态机用 Latent；先广播后写位置（违三步有序）；在 OnPawnLanded 同步回调内发起下一程（重入）。
- Guardrail: 双点链排序归回合2 编排（非移动自驱）。

---

## Acceptance Criteria

- [ ] **AC-18** `Advance()` 返回后三状态同步可断言且**有序**：位置已写定 ∧ 发薪已 push（若 P-1 成立）∧ 落地已广播——spy 记录三操作实际序，断言 `SetTileIndex_order < push_order < OnPawnLanded_order`。**push-skipped 子例**（P-1 不成立常态）：`push_salary==false` 时断言 `SetTileIndex_order < OnPawnLanded_order` 且无 push 发生。
- [ ] **AC-21** [Logic·CI 构建] **公共编排接口** `Advance`/`TeleportTo`/`GetTileIndex` 均标 `UFUNCTION(BlueprintCallable)`、BP 可调、编译通过（CI 构建）。`SetTileIndex` BP 暴露强度由 OQ-Move-3 ADR 裁（C++-only / BlueprintProtected，非无条件 BlueprintCallable）。
- [ ] **AC-22** [Advisory·code-review] 职责边界（CR-6）：移动实现内**无** `mod(from+steps,N)`/`floor((from+steps)/N)`/`mod(target−from,N)` 公式直实现，只调 board `AdvanceIndex`/`steps_between`/`GetTileCount`。
- [ ] **程间非重入（move-012）**：`Advance`/`TeleportTo` 完整返回后回合方才发起下一程（双点链第二程不在第一程 `OnPawnLanded` 同步回调内发起）。

---

## Implementation Notes

*Derived from ADR-0001/0007:*

- **三步有序契约（CR-4，AC-18）**：`Advance`/`TeleportTo` 单帧内**依次**：① `SetTileIndex`（写定位置）→ ② 发薪 push（若 P-1 成立）→ ③ 广播 `OnPawnLanded`。**逻辑先于表现**——fixture 在 call 返回后直接断言终态 + **执行顺序**（spy/mock CallLog 记录三操作 order，断言 `SetTileIndex < push < OnPawnLanded`；仅测终态会放过"先广播后写位置"的错误）。push-skipped 子例：`push_salary==false` 时无 push、`SetTileIndex < OnPawnLanded`。
- **程间非重入（move-012）**：双点链/事件二次移动的排序归**回合2 编排**（非移动自驱）；移动保证 `Advance`/`TeleportTo` **完整返回后**回合方才发起下一程，第二程不在第一程 `OnPawnLanded` 同步回调内发起（参 player-turn AC-47 程间非重入 trampoline，movement 侧只保证同步完整返回）。
- **接口稳定（AC-21）**：`Advance`/`TeleportTo`/`GetTileIndex` 标 `UFUNCTION(BlueprintCallable)`（CI 构建验证编译）。`SetTileIndex` 暴露强度归 OQ-Move-3 ADR（story-001 已 note：C++-only/BlueprintProtected）。
- **职责边界（AC-22，CR-6）**：移动不复制 board 位置数学（`AdvanceIndex`/`steps_between`/`GetTileCount` 唯一来源），code-review grep 无 `mod`/`floor` 公式直实现。

---

## Out of Scope

- Story 003/004：Advance/TeleportTo 本体（本 story 验其时序契约 + 非重入）。
- 回合2 的双点链编排 / ResolvePhase 程间驱动（player-turn，AC-47）；board 的位置数学（board epic）。

---

## QA Test Cases

- **AC-18**：Given P-1 成立的 Advance；Then spy CallLog 顺序 SetTileIndex < push < OnPawnLanded。push-skipped 子例：push_salary==false → SetTileIndex < OnPawnLanded 且无 push。
- **AC-21**：[CI 构建] Advance/TeleportTo/GetTileIndex UFUNCTION(BlueprintCallable) 编译通过。
- **AC-22**：[code-review] grep 移动实现无 mod/floor 位置公式，只调 board 接口。
- **程间非重入**：第二程不在第一程 OnPawnLanded 回调内发起（spy 断言第一程完整返回后才发起第二程）。

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/movement/move_ordering_reentrancy_test.cpp`（类目 `Rento.Movement.OrderingReentrancy`）— 存在且通过（AC-21 部分 CI 构建）。AC-18 须变异坐实（先广播后写位置 → 顺序断言 FAIL）。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（SetTileIndex）、Story 002（事件）、Story 003（Advance）、Story 004（TeleportTo）
- Unlocks: None（汇总时序契约）
