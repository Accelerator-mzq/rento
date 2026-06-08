# Epic: 事件格 Tile Events

> **Layer**: Core
> **GDD**: design/gdd/tile-events.md
> **Architecture Module**: 机会/命运/税/监狱/起点 + 牌堆（architecture §2.3）
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories tile-events`

## Overview

事件格系统处理机会/命运/税/监狱/起点格的落地解析，独占牌堆状态（model B 有序 `TArray<FCardData>`，抽=取 index0 append 回尾，无独立抽牌指针）与出狱卡持有标记。开局 Fisher-Yates 洗牌经骰子3 的权威 FRandomStream + 种子序列化（禁 FMath::Rand 全局随机，确定性可重放）。监狱*态字段*（bIsInJail/JailTurnsServed）物理存于回合2 中心 PlayerState、由回合2 序列化，本系统经回合2 受控写接口改写（单写路径）；出狱卡持有态为本系统自有状态随存档序列化。程间非重入：卡触发二次 TeleportTo 不在 OnPawnLanded 同步回调内发起。10 类卡牌效果 dispatch 落 C++（避 BP 大 switch 保守性）。MVP 含机会/命运/税/监狱/起点；命运之轮/俄罗斯轮盘/策略卡留 Alpha。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0015: holder PULL | MoveToNearest Utility 额外骰经回合2 受控写 SetCurrentRollContext 注入当前程 holder，单源不串程（供经济 F-4 PULL；机制 pt-005 已实现 setter） | LOW |
| ADR-0004: 确定性 RNG | 开局 Fisher-Yates 经骰子3 FRandomStream 权威流 + 种子序列化，禁 FMath::Rand（确定性可重放） | LOW※ |
| ADR-0003: 事件总线 | OnCardDrawn/OnTileEventResolved/OnPlayerJailed/OnPlayerReleased DYNAMIC_MULTICAST_DELEGATE+BlueprintAssignable，enum uint8 基底 | LOW |
| ADR-0005: 存档契约 | 牌堆数组顺序 + 出狱卡持有标记 + 洗牌种子序列化（FCardData 标 SaveGame）；监狱态归回合2 序列化 | LOW |
| ADR-0001: UObject 宿主 | 事件格 per-match 服务宿主（UWorldSubsystem，RNG 经 DiceService 隔离流注入）；程间非重入机制 | LOW |
| ADR-0002: 棋盘容器 | 牌堆内存容器形态（model B 有序 TArray<FCardData>） | LOW※ |
| ADR-0007: BP-vs-C++ 边界 | ECardEffectType 10 类效果 dispatch 落 C++（避 BP 大 switch 保守性） | LOW |

> ※ ADR-0004（MEDIUM，RandRange/默构种子）与 ADR-0002（MEDIUM，UAssetManager）均已在 dice-rng/board-data Sprint0 验证；事件格仅消费已验证原语，架构 §2.3 标本系统 risk = LOW。

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-event-001 | 事件格 per-match 服务宿主（UWorldSubsystem，RNG 经 DiceService(3) 隔离流注入） | ADR-0001 ✅ |
| TR-event-002 | 程间非重入：卡触发二次 TeleportTo 不在 OnPawnLanded 同步回调内发起（机制 OQ-Event-7① 待 ADR） | ADR-0001 ⚠ Partial |
| TR-event-003 | 牌堆内存容器：有序 TArray<FCardData>(model B，抽=取 index0 append 回尾)；宿主型 OQ-Event-7② 待裁 | ADR-0002/0005 ⚠ Partial |
| TR-event-004 | 开局 Fisher-Yates 经骰子3 FRandomStream 权威流 + 种子序列化，禁 FMath::Rand（确定性可重放） | ADR-0004 ✅ |
| TR-event-005 | 牌堆数组顺序 + 出狱卡持有标记 + 洗牌种子序列化（SaveGame，FCardData 标 UPROPERTY(SaveGame)） | ADR-0005/0004 ✅ |
| TR-event-006 | OnCardDrawn/OnTileEventResolved/OnPlayerJailed/OnPlayerReleased DYNAMIC_MULTICAST_DELEGATE+payload USTRUCT，enum uint8 基底 | ADR-0003 ⚠ Partial |
| TR-event-007 | 监狱态字段 bIsInJail/JailTurnsServed 物理存于 player-turn PlayerState、由 player-turn 序列化；经受控写接口改写 | ADR-0001/0005 ⚠ Partial |
| TR-event-008 | 出狱卡持有态 bHoldsChanceOutCard/bHoldsChestOutCard 为本系统自有状态、随存档序列化 | ADR-0005 ✅ |
| TR-event-009 | RepairFee 读建房(8)全盘聚合 GetTotalHouseCount/GetTotalHotelCount（与 per-tile [0,5] 口径区分） | ADR-0007/0001 ⚠ Partial（spec 完整，待 building8 impl） |
| TR-event-010 | MoveToNearest Utility 额外掷骰 dice_total 经 player-turn 受控写 SetCurrentRollContext 注入当前程 holder | ADR-0015 ✅ |
| TR-event-011 | ECardEffectType 10 类效果 dispatch 落 C++（避 BP 大 switch 保守性） | ADR-0007 ⚠ Partial |

**覆盖**: 5 Covered / 6 Partial / **0 Gap**。

> ✅ **TR-event-010 已解除（ADR-0015 Accepted，2026-06-08）**：MoveToNearest Utility 额外骰经回合2 SetCurrentRollContext 注入 holder（与 move-010 同源 holder PULL 机制的事件注入端，机制 pt-005 已实现）。
> ✅ **TR-event-009 评估结论（2026-06-08）= 非架构 Gap，无需新 ADR**：接口契约已在 building8 F-7（`GetTotalHouseCount`/`GetTotalHotelCount`，per-tile vs 全盘口径焊死）+ tile-events F-3 完整规格化（OQ-Event-3 ✅ RESOLVED 2026-06-04）；架构面 = ADR-0007（跨系统只读 accessor）+ ADR-0001（building8 宿主），pull 读非事件。这是对 **Feature 层 building8（未 epic）实现**的跨层依赖，非架构 Gap。RepairFee 经 **mock** `GetTotalHouseCount`（AC-21）现可开 story、不被 Blocked；真实接线待 building8 epic 实现。重分类 Gap→Partial（ADR-0007/0001）。

## Definition of Done

This epic is complete when:
- All stories are implemented, reviewed, and closed via `/story-done`
- All acceptance criteria from `design/gdd/tile-events.md` are verified（MVP 范围：机会/命运/税/监狱/起点）
- All Logic and Integration stories have passing test files in `tests/`（physical：`Source/rentoTests/Private/`）
- Fisher-Yates 洗牌确定性经测试守护（固定种子→精确牌序，禁全局随机 grep 硬门覆盖）
- 监狱态单写路径经测试守护（仅经回合2 受控写接口）；程间非重入（OQ-Event-7① ADR 后）
- ✅ untraced TR：event-010 解除（ADR-0015 Accepted 2026-06-08）；event-009 评估=非架构 Gap（接口已 GDD 规格化 + ADR-0007/0001），重分类 Partial，经 mock 可开 story、真实接线待 building8 epic

## Next Step

Run `/create-stories tile-events` to break this epic into implementable stories.
