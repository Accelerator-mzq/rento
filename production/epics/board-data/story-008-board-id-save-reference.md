---
Epic: board-data
Status: Ready
Layer: Foundation
Type: Integration
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 008 — 棋盘 DA 存档引用契约（BoardIdentifier）：存引用不存布局

## Context

- **GDD**: `design/gdd/board-data.md` — Interactions（存档(21) 读 `GetBoardId()`）、States and Transitions（运行时可变状态不属本系统/存档干净序列化前提）、AC-28b（跨会话稳定性，测试责任归存档21）、Open Questions OQ-8（跨系统测试追踪）
- **Requirement TR-IDs**: TR-board-006
- **ADR Governing**:
  - **ADR-0005（primary）— 存档序列化契约**：存 DA 引用不存布局；容器存 `BoardIdentifier: FName`（CR-2）；读档拓扑序第 1 步加载 Board DA（BoardIdentifier）→ 其余重建；EC-3 DA 缺失/改名 → 拒绝读档（接受的代价）。
  - **ADR-0002 — 棋盘数据容器**：`GetBoardId()` 取顶层 `BoardIdentifier` 字段、与 `PrimaryAssetId`/路径解耦（AC-28）——保证存档引用跨会话/跨版本恒定。
  - **ADR-0001 — UObject 宿主与生命周期**：存档框架挂 `UGameInstanceSubsystem`（跨局入口）；读档重建经依赖拓扑序，Board DA 最先加载。
- **Engine**: Unreal Engine 5.7 — Risk: LOW（`USaveGame`/`SaveGameToSlot`/`FName` 均 pre-5.3 稳定 API）
- **Engine Notes**: ADR-0005 Knowledge Risk = LOW（全 pre-5.3 稳定持久化 API）。**Sprint0 引擎验证 #8**：`UPROPERTY(SaveGame)` 嵌套 USTRUCT/`TArray`/`TMap` round-trip 冒烟。`FName` case-insensitive：存档(21)经 `FName` 比较安全；若降级 `FString` 比较须 `ToLower()` 归一化（AC-28 R3 注）。本系统侧（board）只供 `GetBoardId()` 返回稳定标识；序列化与跨会话往返测试归存档(21)。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：序列化字段名须逐字对齐各 owner GDD（save-load CR-3 表是核对锚点）（source: ADR-0005）。
  - **Required**：读档重建拓扑序（CR-5）：DA → (经济/地产/建房/事件格牌堆 互不依赖) → 回合2 → …（DA 最先重建）（source: ADR-0005）。
  - **Required**：`BoardIdentifier: FName` 作顶层 `UPROPERTY` 作者手填，与 `PrimaryAssetId`/路径解耦（AC-28）（source: ADR-0002）。
  - **Forbidden**：Never 序列化全量棋盘布局——静态布局由 DA 重建；存 DA 引用不存布局（source: ADR-0005）。
  - **Guardrail**：本系统为存档提供者侧（enable-not-own）；存档21 不裁棋盘语义，本 story 不实现存档容器/写盘（那归存档 epic）。

## Acceptance Criteria

- [ ] **AC-板存档供给** board 侧提供 `GetBoardId() → FName`（顶层 `BoardIdentifier`），供存档(21) 作为唯一棋盘引用持久化；本系统不暴露任何「序列化全量布局」接口。
- [ ] **AC-28b（GetBoardId 跨会话/跨存档稳定性）** [Advisory/Integration] GIVEN 一局存档后重启会话读档，WHEN 再调 `GetBoardId()`，THEN 返回值与存档时一致（供存档系统 21 可靠引用）。**此跨会话/序列化往返的 BLOCKING 测试归存档系统(21) 的 Integration 测试 + code review 守门**（单元测试无法跨进程）；本系统仅声明契约。
- [ ] **OQ-8 回链登记** 本 story 显式登记：AC-28b 的 BLOCKING 测试须在存档(21) GDD §Acceptance Criteria 出现显式 AC 实现并回链 board AC-28b；`/design-review` 审存档(21) GDD 时核对此回链，缺失则存档 GDD 不得 APPROVED。

## Implementation Notes

（逐字取自 ADR-0005 §Decision / §Architecture + ADR-0002 §Key Interfaces + GDD §Interactions，语义不改写）

1. 存 DA 引用不存布局：容器存 `BoardIdentifier: FName`（DA 引用，非全量布局，CR-2）；读档拓扑序第 1 步「加载 Board DA(BoardIdentifier)」→ 再重建经济/地产/建房/事件格牌堆 → 回合2。
2. `GetBoardId()` 返回 DA 内显式 `BoardIdentifier: FName` 字段（如 `"Classic40"`），由内容作者手写、与资产路径/文件名/`PrimaryAssetId` 解耦；**禁止**从资产路径或 `PrimaryAssetId` 派生（资产改名/移动会静默损坏所有旧存档）。该字段跨会话/跨版本恒定。
3. EC-3 代价（接受）：DA 删除/改名 → 读档失败（换取存档体积小 + 不冗余）；静态布局可由 DA 完全重建，复制是冗余（违反 DRY，board 静态数据运行时不可变本就该由 DA 重建）。
4. 关键边界：运行时可变状态（谁拥有/建几栋/是否抵押，以 `TileIndex` 为键）由地产(6)/建房(8) 各自持有，**不属本系统**；这是存档(21)能干净序列化的前提——存档只需存「用了哪张棋盘 DA + 各 TileIndex 的可变状态」，无需复制静态布局。
5. **测试责任归属**（OQ-8 追踪）：AC-28b 跨会话稳定性的 BLOCKING 测试归存档(21) Integration（棋盘数据无法在自身单测中验证跨进程往返）；本系统提供 `GetBoardId()` 契约，下游 GDD 承接其 BLOCKING 测试并回链 board AC 编号。

## Out of Scope

- `GetBoardId()` 实现本体 + 同进程稳定性（AC-28）→ story-004（本 story 聚焦存档引用契约 + 跨会话责任移交）。
- `USaveGame` 容器、四重完整性门、写盘原子性、读档重绑 → 存档(21) epic（ADR-0005 实现）。
- 运行时可变状态序列化（owner map / house_count / Cash）→ 各 owner 系统 + 存档 epic。

## QA Test Cases

- **AC-板存档供给**：[Integration/code-review] Verify board 侧仅暴露 `GetBoardId() → FName`，无「序列化全量布局」接口；存档容器对棋盘仅持 `BoardIdentifier`。Pass：review sign-off + 记 `production/qa/evidence/`。
- **AC-28b**：[Advisory→存档21 Integration] GIVEN 存档时 `GetBoardId()=="Classic40"`，WHEN 重启会话读档后再调 `GetBoardId()`，THEN 仍 `=="Classic40"`（与存档时一致）。**本系统视角为 Advisory（契约声明）；实测归存档(21) Integration 测试**——板侧无法跨进程验证。Edge：DA 改名 → 读档失败（EC-3，存档侧测试）；`FName` 大小写不一致 → 须 `ToLower()` 归一化避免读档失配。
- **OQ-8 回链**：[Advisory·design-review] Verify 存档(21) GDD §AC 含显式实现 AC 并注明「实现 board-data AC-28b」；缺失则存档 GDD 不得 APPROVED。

## Test Evidence

- **Path**: `tests/integration/board/board_id_save_reference_test.cpp`（[Integration] 契约侧占位；**跨会话 round-trip BLOCKING 测试落 `tests/integration/save-load/` 由存档21 实现**）
- **Status**: 未创建（板侧契约占位；权威跨会话测试待存档21 epic）
- OQ-8 回链核对 + 存档供给 code-review → `production/qa/evidence/`（[Advisory]）

## Dependencies

- **Depends on**: story-001（DONE，`BoardIdentifier` 字段）、story-004（DONE，`GetBoardId()` 实现）。
- **Unlocks**: 存档(21) 读档拓扑序第 1 步（加载 Board DA）；主菜单20「继续游戏」入口（间接）。
