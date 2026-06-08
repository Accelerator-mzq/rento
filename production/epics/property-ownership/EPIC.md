# Epic: 地产所有权 Property Ownership

> **Layer**: Core
> **GDD**: design/gdd/property-ownership.md
> **Architecture Module**: 买地/抵押事务 owner（architecture §2.3）
> **Status**: Ready
> **Stories**: 7 stories created (2026-06-08) — see ## Stories

## Overview

地产所有权系统是买地/抵押事务的 owner —— 独占 owner map、垄断/抵押状态写权威，以及派生量 `is_monopoly`/`station_count`/`utility_count`（实时重算无缓存）。架构裁定消解两个环：house_count 归建房8 不归 6（防 6↔8 环）；买地/抵押事务方向 6→5（本系统调 economy `Debit`/`Credit` 执行现金侧，economy 不反向调 6，防 5↔6 环）。对外经 `BuildOwnershipSnapshot` 供经济算租（值拷贝非活引用，防结算期归属漂移），破产级联逐格广播带重入锁 `bIsMidBankruptcyTransfer`（须无条件解除）。所有运行态字段只经受控写接口落地，外部禁直写；owner map + bIsMortgaged 序列化随存档，派生量读档后重算。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0013: 运行时容器与批量原子性 | owner map=AoS TArray<FTileOwnershipState> 稠密+哨兵；批量移交全或全无+同格原子清零；C++ private 封装升 AC-3/7/31/32/35 为 [Logic]；TArray<bool> bitfield 前提 REFUTED | LOW |
| ADR-0007: BP-vs-C++ 边界 | 买地/抵押事务方向 6→5；受控写封装 owner map/bIsMortgaged 只经受控接口；公共接口标 UFUNCTION | LOW |
| ADR-0003: 事件总线 | OnOwnershipChanged/OnMortgageChanged DYNAMIC_MULTICAST_DELEGATE+BlueprintAssignable；破产级联逐格广播+重入锁 | LOW |
| ADR-0006: GameStateSnapshot | BuildOwnershipSnapshot 值拷贝接口（owner/is_mortgaged/is_monopoly/count，不含 house_count）单向 push 防 5↔6 环 | LOW |
| ADR-0005: 存档契约 | owner map + bIsMortgaged 序列化；派生量不存读档后重算；监听器读档重绑合约（防 dangling delegate） | LOW |
| ADR-0002: 棋盘容器 | owner map 容器形态 + 非可购买格哨兵槽 + TileIndex 连续无洞前提；GetTilesInGroup 算 is_monopoly | LOW※ |
| ADR-0001: UObject 宿主 | owner map/抵押标记运行态服务 UObject 宿主（随局生灭）；PostLoad 派生量重算晚于棋盘1 加载 | LOW |

> ※ ADR-0002 MEDIUM 已在 board-data Sprint0 验证；地产仅消费 GetTilesInGroup/容器布局，架构 §2.3 标本系统 risk = LOW。

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-prop-001 | owner map 容器形态(TArray 稠密 vs TMap) + 非可购买格哨兵槽 + TileIndex 连续无洞前提 | ADR-0013/0002/0001 ✅ |
| TR-prop-002 | 抵押标记容器（TArray<bool> bitfield 前提 REFUTED；AoS 内含 bool 无独立标记数组） | ADR-0013 ✅ |
| TR-prop-003 | owner map/抵押标记运行态服务 UObject 宿主与生命周期（随局生灭） | ADR-0001 ✅ |
| TR-prop-004 | PostLoad 派生量(is_monopoly/count)重算晚于棋盘1 加载完成（GetTilesInGroup 可用） | ADR-0001/0005 ⚠ Partial |
| TR-prop-005 | owner map + bIsMortgaged 序列化随存档21；派生量不存读档后重算 | ADR-0005 ✅ |
| TR-prop-006 | 受控写封装：owner map/bIsMortgaged 只经受控接口，外部禁直写（BlueprintPrivate 强度） | ADR-0007/0013 ✅ |
| TR-prop-007 | BuildOwnershipSnapshot 值拷贝（owner/is_mortgaged/is_monopoly/count，不含 house_count）单向 push 防 5↔6 环 | ADR-0006/0001 ✅ |
| TR-prop-008 | 快照隔离：值拷贝非活引用，防结算期归属漂移 | ADR-0006 ✅ |
| TR-prop-009 | 买地/抵押事务方向 6→5（调 economy Debit/Credit，economy 不反向调 6，防 5↔6 环） | ADR-0007 ⚠ Partial |
| TR-prop-010 | OnOwnershipChanged/OnMortgageChanged DYNAMIC_MULTICAST_DELEGATE + BlueprintAssignable + payload USTRUCT | ADR-0003 ✅ |
| TR-prop-011 | 破产级联逐格广播 + 重入锁 bIsMidBankruptcyTransfer（无条件解除）vs deferred broadcast queue | ADR-0003/0013 ✅ |
| TR-prop-012 | 批量移交原子性（全或全无，owner 与 bIsMortgaged 同格原子清零） | ADR-0013 ✅ |
| TR-prop-013 | 监听器读档重绑合约（OnOwnershipChanged 绑定不随序列化，防 dangling delegate） | ADR-0005/0003 ✅ |
| TR-prop-014 | 派生量(is_monopoly/station_count/utility_count)非存储态、每次实时重算无缓存（确定性） | ADR-0005 ✅ |
| TR-prop-015 | 公共接口标 UFUNCTION(BlueprintCallable)、事件 BlueprintAssignable、headless -nullrhi 编译通过 | ADR-0007/0003 ✅ |

**覆盖**: 13 Covered / 2 Partial（TR-prop-004 PostLoad 序、TR-prop-009 6→5 方向）/ **0 Gap**。

> ✅ **3 条 Gap TR 已解除（ADR-0013 Accepted，2026-06-08）**：TR-prop-001（owner map 容器=AoS TArray<FTileOwnershipState> 稠密+哨兵+连续前提）/ TR-prop-002（**TArray<bool> bitfield 前提源码核实 REFUTED**；AoS 内含 bool 无独立标记数组）/ TR-prop-012（批量移交全或全无+同格原子清零）。另升级 TR-prop-006（封装→C++ private+只读访问器，AC-3/7/31/32/35 升 [Logic]）+ TR-prop-011（逐格广播+重入锁 RAII 无条件解除）由 Partial→Covered。详见 `docs/architecture/ADR-0013-property-ownership-runtime-container.md`。

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | owner map 容器 (AoS) + 受控写封装 + 事件契约 | Logic | Ready | ADR-0013/0001/0007/0003 |
| 002 | 买地事务 Buy (前置前置化, 6→5) | Logic | Ready | ADR-0013/0007 |
| 003 | 抵押/赎回事务 Mortgage/Unmortgage | Logic | Ready | ADR-0013/0007 |
| 004 | 派生谓词 F-1/F-2/F-3 (垄断+计数) + 确定性无缓存 | Logic | Ready | ADR-0002/0006/0013 |
| 005 | 归属快照 BuildOwnershipSnapshot (值拷贝, 防漂移) | Logic | Ready | ADR-0006/0001 |
| 006 | 破产移交 + 批量原子性 + 重入锁 | Integration | Ready | ADR-0013/0003 |
| 007 | 职责边界 + 接口稳定 + 序列化/读档重绑 | Logic | Ready | ADR-0005/0003/0007 |

> **依赖序**：001 → {002,003,004} → 005(需004) / 006(需001/002/003)；007 汇总。6 Logic + 1 Integration，全 Ready（无 Blocked）。
> **TR 覆盖**：prop-001..015 全覆盖（prop-001/002/012 经 ADR-0013 解 Gap，prop-006/011 升 Covered）。AC-1..41（44 含 CI-stub→Logic）分布于 7 story。

## Definition of Done

This epic is complete when:
- All stories are implemented, reviewed, and closed via `/story-done`
- All acceptance criteria from `design/gdd/property-ownership.md` are verified
- All Logic and Integration stories have passing test files in `tests/`（physical：`Source/rentoTests/Private/`）
- 环依赖纪律经测试守护：6→5 单向（economy 不反调 6）、house_count 归8（6 不持）、snapshot 值拷贝隔离
- ✅ OQ-Prop-1 owner map 容器/原子性/封装 ADR 补齐（ADR-0013 Accepted 2026-06-08，解除 prop-001/002/012 Gap，story 可解 Blocked）

## Next Step

7 stories created. Run `/story-readiness production/epics/property-ownership/story-001-owner-map-container-events.md` then `/dev-story`. Work in dependency order.
