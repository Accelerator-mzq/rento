---
Epic: foundation-framework
Status: Ready
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 003 — 异步加载纪律 + Deinitialize CancelHandle（防 PIE 空棋盘）

## Context

- **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 模块①——异步加载生命周期纪律 + ④ Board DA Holder 宿主面）
- **Requirement TR-ID**: 框架面支撑 **TR-board-007**（运行时 DA 持有者宿主 + UPROPERTY 强引用防 GC）、**TR-board-008**（热切换边界：换盘走完整 Loaded→Validated→Active，禁 Active 对局热替换 DA；`Deinitialize` `CancelHandle`/`ReleaseHandle` 防 PIE 空棋盘）。本 story 提供这两 TR 的**异步加载 + 生命周期纪律框架**；DA 解析/校验/`GetTileData` 功能 owns 归 board epic（ADR-0002）。
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期边界**：`Initialize` 中 `FStreamableManager::RequestAsyncLoad` 取得的 `TSharedPtr<FStreamableHandle>` 须存为成员，`Deinitialize` 显式 `CancelHandle()`；防 PIE Stop 后回调悬挂写已 GC 对象（本项目登记的具体地雷 OQ-6 / player-turn OQ-1⑩ GC 陷阱）。
  - **ADR-0002（secondary，TR-board-008 列入）— 棋盘数据容器**：运行时由宿主以 `UPROPERTY UBoardDataAsset*` 强引用持有防 GC；`Never` 在 Active 对局中热替换 DA，须走完整 `Loaded→Validated→Active` 重初始化。
- **Engine**: Unreal Engine 5.7（C++ 框架）— **Risk: LOW**（`FStreamableManager`/`FStreamableHandle` pre-5.3 稳定；`UAssetManager` 5.4+ 变化由 board epic / ADR-0002 自核，本 story 仅用 handle 生命周期面）
- **Engine Notes（ADR-0001 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**（本 ADR 范围）。`FStreamableManager` 异步加载本身稳定；`UAssetManager` 同步/异步 Primary DA 的 5.7 API 签名是 ADR-0002 的 Sprint0 验证项⑥，**不在本 story 范围**。
  - Verification Required（ADR-0001 验证③，完整见 story-007）：`Deinitialize()` 中 `CancelHandle` 后无悬挂异步回调写空棋盘。
- **Control Manifest Rules**:
  - **Required（Foundation/ADR-0001）**: 异步加载纪律——`Initialize` 中 `FStreamableManager::RequestAsyncLoad` 取得的 `TSharedPtr<FStreamableHandle>` 须存为成员，`Deinitialize` 显式 `CancelHandle()`。
  - **Required（Foundation/ADR-0002）**: 运行时由宿主以 `UPROPERTY UBoardDataAsset*` 强引用持有，防 GC。
  - **Forbidden（ADR-0001）**: Never 让 `Deinitialize` 后残留悬挂异步加载回调写已 GC 对象（PIE Stop 写空棋盘）。
  - **Forbidden（ADR-0002）**: Never 在 Active 对局中热替换 DA——须走完整 `Loaded→Validated→Active` 重初始化。
  - **Global**: UObject 引用成员用 `TObjectPtr<T>` + `UPROPERTY()` 防 GC，不用裸指针/`TSharedPtr<T>`（注：`FStreamableHandle` 是非-UObject，仍用 `TSharedPtr<FStreamableHandle>`——这是 ADR-0001 明示例外，handle 本身非 UObject）。
  - **Guardrail（ADR-0002）**: Board DA Load Time < 100ms（对局初始化一次性）；Memory < 0.1MB/棋盘。

## Acceptance Criteria

- [ ] AC-1: 宿主在 `Initialize` 中经 `FStreamableManager::RequestAsyncLoad` 发起 DA 异步加载，返回的 `TSharedPtr<FStreamableHandle>` 存为宿主成员（非局部变量丢弃）。
- [ ] AC-2: 加载完成后，解析出的 `UBoardDataAsset*` 以 `UPROPERTY()` 强引用（`TObjectPtr`）持有，防 GC。
- [ ] AC-3: `Deinitialize()` 显式调用所持 handle 的 `CancelHandle()`（与 `ReleaseHandle`），随后无异步回调写宿主已 GC 字段。
- [ ] AC-4: 在 `-nullrhi` headless 下，于异步加载中途触发 PIE Stop（`Deinitialize`），无悬挂回调崩溃（写空棋盘地雷被 `CancelHandle` 消除）。
- [ ] AC-5: 换盘走完整 `Loaded→Validated→Active` 重初始化路径；**禁** Active 对局中直接热替换 DA 引用（无绕过重初始化的热替换入口）。

## Implementation Notes

> 逐字保真自 ADR-0001 Implementation Guidelines #3 + ADR-0002，不改写语义。

1. **异步加载纪律**：`Initialize` 中 `FStreamableManager::RequestAsyncLoad` 取得的 `TSharedPtr<FStreamableHandle>` 须存为成员，`Deinitialize` 显式 `CancelHandle()`。PIE Stop→回调悬挂写已 GC 对象是本项目登记的具体地雷（OQ-6 / player-turn OQ-1⑩ GC 陷阱）。
2. **防 GC**：运行时由宿主以 `UPROPERTY UBoardDataAsset*`（`TObjectPtr<UBoardDataAsset>`）强引用持有（ADR-0002）。
3. **热切换边界**：Never 在 Active 对局中热替换 DA——须走完整 `Loaded→Validated→Active` 重初始化（ADR-0002）。本 story 提供生命周期边界框架；`Loaded→Validated→Active` 内的校验/结构化错误码（`BoardTooSmall`/`Index0NotGo`/…）归 board epic。
4. handle 完成回调内须 guard 宿主仍有效（PIE Stop 后宿主已 `Deinitialize`/GC 时不写）；`CancelHandle` 是首道防线。

## Out of Scope

- DA 解析后的 tile 访问接口（`GetTileData`/`GetTilesInGroup`/`GetTileWorldTransform`/环序 N）→ board epic（ADR-0002）。
- DA 校验逻辑与结构化错误码、`bIsPlaceholderData` 编辑器剥离 → board epic（ADR-0002）。
- `UAssetManager` 同步/异步 Primary DA 的 5.7 API 签名验证 → ADR-0002 Sprint0 验证⑥（board epic）。
- 宿主基类四 override 骨架本身 → **story-001**（本 story 在其 `Initialize`/`Deinitialize` 上挂异步加载纪律）。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

> Integration（异步加载 + 宿主生命周期跨 PIE 边界）；headless `-nullrhi`。

- **TC-1 (AC-1/AC-2)**: GIVEN 宿主 `Initialize` 发起 DA 异步加载，WHEN 检查宿主成员，THEN `TSharedPtr<FStreamableHandle>` 成员非空；加载完成后 `UPROPERTY` DA 引用非空（防 GC）。
- **TC-2 (AC-3)**: GIVEN 加载已完成的宿主，WHEN 调 `Deinitialize`，THEN 所持 handle `CancelHandle()` 被调用；之后无回调写宿主字段（spy 回调计数==0）。
- **TC-3 (AC-4)**: GIVEN `-nullrhi`、异步加载进行中（未完成），WHEN 触发 PIE Stop（`Deinitialize`），THEN 不崩溃、无空棋盘写入（`CancelHandle` 生效）。
- **TC-4 (AC-5)**: GIVEN Active 对局，WHEN 尝试直接替换 DA 引用，THEN 无此入口/被拒，必须经 `Loaded→Validated→Active` 重初始化。
- **Edge cases**: 加载完成回调晚于 `Deinitialize` 到达（竞态）须被 guard 吞掉不写已 GC 宿主；同进程连续两局，第一局 handle 在第二局 `Initialize` 前已 `Cancel`/`Release`（无跨局 handle 泄漏）。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/foundation/async_load_cancel_handle_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-001（异步加载纪律挂宿主基类 `Initialize`/`Deinitialize`）。
- **Unlocks**: board epic 的 DA 解析/校验实现（TR-board-007/008 功能面）；story-007（PIE 隔离验证③ CancelHandle 后无悬挂回调）。
