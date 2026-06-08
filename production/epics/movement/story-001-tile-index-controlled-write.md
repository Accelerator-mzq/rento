# Story 001: CurrentTileIndex 容器 + SetTileIndex 受控写守界

> **Epic**: 移动 Movement
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/movement.md`（CR-1）
**Requirement**: `TR-move-001`、`TR-move-002`、`TR-move-014`、`TR-move-016`、`TR-move-018`（命名收敛 note）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0001（宿主，primary）· ADR-0007（受控写封装）· ADR-0005（CurrentTileIndex 存 PlayerState）
**ADR Decision Summary**: `CurrentTileIndex`（int32∈[0,N−1]）物理存于回合2 中心 PlayerState、唯一持久态；经移动受控写 `SetTileIndex`，入口守界 `ensure(0≤index<N)`。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `ensure` 在 Shipping 编译为空（守界用 ensure+早返语义，非 check 硬停）；`-nullrhi` 可测。`SetTileIndex` 须 C++ `UFUNCTION` 才能让 `private` 封装生效。

**Control Manifest Rules (Core/Foundation)**:
- Required: 写权威状态 `CurrentTileIndex` → C++；`from` 每次调用实时读 `PlayerState.CurrentTileIndex`（不跨帧缓存，防双点第二程读旧起点）。
- Forbidden: 外部系统直写 `PlayerState.CurrentTileIndex`（绕过 `SetTileIndex` 受控写）。
- Guardrail: `SetTileIndex` 入口守界，越界拒写保留旧值。

---

## Acceptance Criteria

- [ ] **AC-1** 落点不变式 `CurrentTileIndex ∈ [0,N−1]`：from=38, steps=5, N=40 → `newIndex==3`。
- [ ] **AC-2** `from` 实时读不缓存：连续两次 `Advance`（from=0,steps=5→5；再 steps=3 从 5 起→8），第二次 `from` 经 `GetTileIndex(player)` 从 PlayerState 实时读取（非硬传），同一测试帧内串行。
- [ ] **AC-3** [Advisory·code-review] 受控写软约束：只经 `SetTileIndex` 写 `CurrentTileIndex`（BP 层 code-review；OQ-Move-3 选 C++ 强封装→升 [Logic] 静态扫描，前置 player-turn OQ-1 ADR 已 Accepted）。
- [ ] **AC-4** [关键 guard] `SetTileIndex` 入口守界：`SetTileIndex(-1)`/`(N)` → `ensure` 拒绝、不写、`CurrentTileIndex` 不变（ensure+早返：报告后继续、保留旧值）。

---

## Implementation Notes

*Derived from ADR-0001/0007/0005:*

- `CurrentTileIndex: int32` 物理存于回合2 的 PlayerState（`URentoPlayerState`，唯一持久态，`UPROPERTY(SaveGame)`；序列化随存档21）。movement 经受控写 `SetTileIndex` 改它（**单写路径**，不直写）。
- `SetTileIndex(int32 Index)`：入口 `ensure(0 <= Index && Index < N)`，越界 → 早返拒写、保留旧值（AC-4，**非 check 硬停**）。`N = GetTileCount()`（board）。
- `GetTileIndex(player)`：每次实时读 PlayerState（不跨帧缓存，AC-2/AC-16），防双点链第二程读旧起点。单线程 BP + 禁 Latent 下竞态理论不可达，AC-2 主防 BP 图人为缓存 bug。
- **封装强度（OQ-Move-3，prop-006 同款）**：`CurrentTileIndex` C++ private + `SetTileIndex` C++ `UFUNCTION`（C++-only 或 `meta=(BlueprintProtected)`，非无条件 `BlueprintCallable`，否则重开受控写漏洞）；据此 AC-3 由 code-review 升 [Logic] 静态扫描。
- **命名收敛（move-018，OQ-Move-3(b)）**：movement 称 `SetTileIndex`、player-turn 称 `SetPosition`——同一受控写接缝两侧异名，须收敛或显式标别名（实现期与 player-turn 协调；本 story 用 `SetTileIndex`，note 待收敛）。

---

## Out of Scope

- Story 003：`Advance` 编排（调 `SetTileIndex` 落点）。
- Story 004：`TeleportTo`（亦经 `SetTileIndex`）。
- 存档21 的 PlayerState 序列化（persistence）；回合2 的 PlayerState 宿主（player-turn，已实现）。

---

## QA Test Cases

- **AC-1**：Given from=38, steps=5, N=40；When Advance；Then CurrentTileIndex==3 ∈[0,39]。
- **AC-2**：Given from=0；When Advance(steps=5)→5，再 Advance(steps=3)（from 经 GetTileIndex 实时读=5）；Then newIndex==8（非 from 缓存 0+3=3）。同帧串行。
- **AC-3**：[code-review] grep 无外部直写 `CurrentTileIndex =`，只经 SetTileIndex。
- **AC-4**：Given CurrentTileIndex=10；When SetTileIndex(-1)/SetTileIndex(N)；Then 不变==10 ∧ ensure（AddExpectedError）。Edge：SetTileIndex(N−1)（合法上界）→ 写成功。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/movement/tile_index_controlled_write_test.cpp`（类目 `Rento.Movement.TileIndexControlledWrite`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None（Foundation 宿主/PlayerState 已 done）
- Unlocks: Story 002–005（全部经 SetTileIndex/GetTileIndex）
