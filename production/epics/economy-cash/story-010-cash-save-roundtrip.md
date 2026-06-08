# Story 010: Cash 存档序列化 round-trip

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/economy-cash.md`（States / 现金状态序列化）
**Requirement**: `TR-econ-009`（Cash serialize）、`TR-econ-010`（load 不经 Credit/Debit, OnCashChanged 0次）、`TR-econ-011`（Raising Funds 瞬态不中途存档）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0005（存档契约，primary）· ADR-0003（OnCashChanged 0 次 on load）
**ADR Decision Summary**: 每玩家 `Cash` 经 `UPROPERTY(SaveGame)`（`CashByPlayer TMap<int32,int32>`）序列化；**读档经容器写回、不经 Credit/Debit**（`OnCashChanged` 触发 0 次，避免误播金币 juice）；读档重建拓扑序（DA → 经济/地产/建房/事件格 互不依赖 → 回合2）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: ⚠ **G7 实证**：UE5.7 内置 `SaveGameToMemory`/`SaveGameToSlot` **不按 UPROPERTY(SaveGame) 过滤**（全量序列化，见 ADR-0005 §G7 / memory）。round-trip 走 `SaveGameToMemory`/`LoadGameFromMemory`（in-memory，无磁盘），**只断言恒等不写"过滤"断言**（参 pt-008 方案A）。`-nullrhi` 可测。Integration：save/load round-trip。

**Control Manifest Rules (Core/ADR-0005)**:
- Required: `Cash` 写回经容器、不经 Credit/Debit（`OnCashChanged` 0 次）；读档重建拓扑序；delegate 读档重绑（呈现层先 Unbind 后 Bind）。
- Forbidden: 读档经 Credit/Debit 重放（误播 juice）；序列化 Raising Funds 瞬态债务；重排枚举整数索引（破坏旧档）。
- Guardrail: 单玩家一 Cash 值，体量极小。

---

## Acceptance Criteria

- [ ] **AC-Save-1（econ-009）** `CashByPlayer` round-trip 恒等：GIVEN 4 玩家 Cash {1500, 200, 0, 750}，WHEN `SaveGameToMemory` → `LoadGameFromMemory`，THEN 读回 `GetCash(p)` 逐玩家恒等。
- [ ] **AC-Save-2（econ-010）** 读档不经 Credit/Debit：WHEN 读档写回 Cash，THEN 经容器直写（非 Credit/Debit 路径）∧ `OnCashChanged` 触发 **0 次**（spy 断言，避免误播金币 juice）。
- [ ] **AC-Save-3（econ-011）** Raising Funds 瞬态不中途存档：GIVEN 玩家处于 Raising Funds（强制扣款未决），WHEN 查询可存档点，THEN 不允许在该瞬态落存档（建议契约，与回合2/存档21 协同）——序列化的是 `Cash` 终值，非"欠多少"的瞬态债务。

---

## Implementation Notes

*Derived from ADR-0005:*

- `Cash` 容器 `CashByPlayer: TMap<int32,int32>` 标 `UPROPERTY(SaveGame)`（story-001 已建容器，本 story 加序列化 + round-trip 验证）。
- **round-trip 走 in-memory**（G7 实证，ADR-0005）：`SaveGameToMemory(SaveObj, OutBytes)` → `LoadGameFromMemory(OutBytes)`，**只断言读回恒等**，不写"SaveGame 过滤生效"断言（内置路径全量序列化，见 memory `ue57-savegame-builtin-path-no-savegame-filter`）。
- **读档写回经容器、不经 Credit/Debit（AC-Save-2）**：`RestoreFromSaveData` 直写 `CashByPlayer`，**不调 Credit/Debit**、**不广播 `OnCashChanged`**（0 次，避免读档误播金币 juice，control-manifest ADR-0005）。delegate 重绑由存档21 广播 `OnGameLoaded`、呈现层先 Unbind 后 Bind（非本系统职责，参 ADR-0005 CR-6）。
- **读档重建拓扑序**：经济/地产/建房/事件格牌堆互不依赖，可任意序重建，均晚于 DA、早于回合2（control-manifest ADR-0005）。
- **Raising Funds 不中途存档（AC-Save-3，econ-011）**：建议筹资未决时不落存档点（避免序列化瞬态债务），与回合2/存档21 协同裁定（OQ-Econ-4）。本 story 验"序列化 Cash 终值 + 不在 Raising Funds 中途存档"的契约（可用回合2 可存档点查询 mock）。

---

## Out of Scope

- 存档21 的 USaveGame 容器/四重完整性门/单槽（persistence，ADR-0005，参 pt-008 框架）；回合2 的可存档点裁定（player-turn）；呈现层 delegate 读档重绑（VFX/HUD/audio）。
- Story 009：Raising Funds 状态机本身（本 story 只验"该瞬态不中途存档"）。

---

## QA Test Cases

- **AC-Save-1**：Given 4 玩家 Cash {1500,200,0,750}；When SaveGameToMemory → LoadGameFromMemory；Then 逐玩家 GetCash 恒等。Edge：Cash=0 玩家正确序列化。
- **AC-Save-2**：When 读档写回；Then OnCashChanged spy 计数==0（不经 Credit/Debit）；容器直写路径核验。
- **AC-Save-3**：Given 玩家 Raising Funds 未决；When 查可存档点；Then 不允许中途存档（mock 回合2 可存档点查询返 false）；序列化的是 Cash 终值非瞬态债务。

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/economy-cash/cash_save_roundtrip_test.cpp`（类目 `Rento.Economy.CashSaveRoundtrip`）— 存在且通过（in-memory round-trip）。AC-Save-2 须 spy 坐实 OnCashChanged 0 次（变异：误经 Credit 写回 → 计数!=0 FAIL）。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（Cash 容器/GetCash）、Story 002（OnCashChanged spy）
- Unlocks: None
