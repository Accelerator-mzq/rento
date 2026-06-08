# Epic: 移动 Movement

> **Layer**: Core
> **GDD**: design/gdd/movement.md
> **Architecture Module**: 棋子推进（architecture §2.3）
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories movement`

## Overview

移动系统负责棋子在环形棋盘上的推进 —— 唯一持久态 `CurrentTileIndex` 经移动受控写落于回合2 的 PlayerState。它向上游棋盘1 取原子元组接口（`AdvanceIndex(from,steps)→(newIndex,passedGo)`、`StepsBetween`、`GetTileCount`），向下广播 `OnPawnMoveStarted`/`OnPawnLanded(arrivalContext)` 并把落地移交 ResolvePhase；发薪经编排门 push `(passedGo, SalaryAmount)` 给经济5（orchestrated，非反向读，不成环）。三步有序契约（受控写 < 发薪 push < 落地广播）单帧同步结算，逻辑先于表现、动画不回授；程间原子性/非重入由回合2 编排双点链排序。MVP 聚焦骰子移动 + 卡牌位移 + 入狱三种到达上下文。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0015: holder PULL + 越界告警 | Utility dice_total 经回合2 holder（CurrentRollContext）PULL，移动不投递；越界告警不静默吞冒泡回合2；passedGo 数值兜底委托 ADR-0014（机制 pt-005/006/007 已实现） | LOW |
| ADR-0002: 棋盘容器 | 上游 AdvanceIndex/StepsBetween/GetTileData(0).SalaryAmount 原子元组只读查询（落点与发薪门唯一来源） | LOW※ |
| ADR-0003: 事件总线 | EArrivalContext UENUM(BlueprintType)+uint8 append-only；OnPawnMoveStarted/OnPawnLanded 广播契约 + 落地移交 | LOW |
| ADR-0001: UObject 宿主 | 单帧同步三步有序契约；程间原子性/非重入（Advance/TeleportTo 完整返回后回合方发起下一程） | LOW |
| ADR-0005: 存档契约 | CurrentTileIndex（int32∈[0,N−1]）物理存于 PlayerState、经移动受控写、唯一持久态 | LOW |
| ADR-0007: BP-vs-C++ 边界 | SetTileIndex 入口守界 ensure(0≤index<N)+早返；Advance/TeleportTo/GetTileIndex 标 UFUNCTION(BlueprintCallable) | LOW |

> ※ ADR-0002 MEDIUM 风险（UAssetManager 签名）已在 board-data Sprint0 验证；移动仅只读消费查询接口，不新增引擎面风险。架构 §2.3 标本系统 risk = LOW。

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-move-001 | CurrentTileIndex(int32∈[0,N−1]) 物理存于 PlayerState，经移动受控写；唯一持久态 | ADR-0005 ✅ |
| TR-move-002 | 受控写宿主形态 + SetTileIndex 封装强度（决定 AC-3 能否升 [Logic]） | ADR-0007/0001 ⚠ Partial |
| TR-move-003 | 上游 AdvanceIndex(from,steps)→(newIndex,passedGo) 原子元组接口 | ADR-0002 ✅ |
| TR-move-004 | 上游 StepsBetween + GetTileCount() 只读查询（TeleportTo 前向步数） | ADR-0002 ✅ |
| TR-move-005 | 上游 GetTileData(0).SalaryAmount 只读（发薪 push 金额由经济算，移动只传 base） | ADR-0002 ✅ |
| TR-move-006 | EArrivalContext {DiceMove,AdvanceCard,SentToJail}，UENUM(BlueprintType)+uint8 append-only | ADR-0003/0005 ✅ |
| TR-move-007 | OnPawnMoveStarted(Player,From,To,Steps,PassedGo) dynamic multicast 广播契约 | ADR-0003 ✅ |
| TR-move-008 | OnPawnLanded(Player,TileIndex,EArrivalContext) + 落地移交 ResolvePhase | ADR-0003 ✅ |
| TR-move-009 | 发薪 push (passedGo, SalaryAmount) 编排门 → 经济 F-1（orchestrated，非反向读） | ADR-0003 ⚠ Partial |
| TR-move-010 | Utility 租 dice_total 由经济在 ResolvePhase 从回合掷骰上下文 PULL（holder=回合2，移动不缓存） | ADR-0015 ✅ |
| TR-move-011 | 单帧同步结算 + 三步有序契约（①受控写<②发薪 push<③落地广播）；逻辑先于表现 | ADR-0001 ⚠ Partial |
| TR-move-012 | 程间原子性/非重入（Advance/TeleportTo 完整返回后回合方发起下一程） | ADR-0001 ⚠ Partial |
| TR-move-013 | 联网重放迁移路径：本地广播 → OnRep_CurrentTileIndex/NetMulticast（Full Vision 技术债） | ⏸ Full Vision defer（ADR-0015 明确不裁/不开 story） |
| TR-move-014 | SetTileIndex 入口守界 ensure(0≤index<N)+早返拒写（用 ensure 非 assert/check） | ADR-0007 ⚠ Partial |
| TR-move-015 | Advance/TeleportTo/GetTileIndex 标 UFUNCTION(BlueprintCallable) 编排接口（CI 构建验证） | ADR-0007 ⚠ Partial |
| TR-move-016 | from 每次调用实时读 PlayerState.CurrentTileIndex 不跨帧缓存（防双点第二程读旧起点） | ADR-0005/0001 ⚠ Partial |
| TR-move-017 | passedGo 理论无界 + steps 超界冒泡告警上传回合2/结构化日志；int32 溢出兜底依赖数据校验 | ADR-0015/0014 ✅ |
| TR-move-018 | 受控写接口命名收敛：移动 SetTileIndex(对外) ↔ player-turn SetPosition(字段 setter) 待 ADR 裁 | ADR-0007 ⚠ Partial |

**覆盖**: 9 Covered / 8 Partial / **0 MVP Gap（TR-move-013 = Full Vision defer，不计 MVP）**。

> ✅ **move-010/017 已解除（ADR-0015 Accepted，2026-06-08）**：TR-move-010（dice_total 经回合2 holder PULL，移动不投递）/ TR-move-017（越界告警不静默吞冒泡 + 数值兜底委托 ADR-0014）。机制 pt-005/006/007 已实现，ADR-0015 固化契约。
> ⏸ **TR-move-013（联网重放迁移）= Full Vision defer**：ADR-0015 明确不裁、MVP 不开 story（intentional defer 非遗漏，MVP 单线程不触发）。

## Definition of Done

This epic is complete when:
- All stories are implemented, reviewed, and closed via `/story-done`
- All acceptance criteria from `design/gdd/movement.md` are verified（MVP 范围，不含 move-013 联网）
- All Logic and Integration stories have passing test files in `tests/`（physical：`Source/rentoTests/Private/`）
- 三步有序契约 + 程间非重入经测试守护（spy 断言受控写<发薪<落地、第二程读实时起点）
- ✅ untraced TR（move-010/017）ADR 补齐（ADR-0015 Accepted 2026-06-08）解除 story Blocked；move-013 = Full Vision deferred（不开 story）

## Next Step

Run `/create-stories movement` to break this epic into implementable stories.
