# Epics Index

Last Updated: 2026-06-08
Engine: Unreal Engine 5.7
Manifest Version: 2026-06-06

> 一 epic 一架构模块。按层 dependency-safe 顺序创建（Foundation → Core → Feature → Presentation）。
> 每个 epic 须跑 `/create-stories [epic-slug]` 拆成可实现 story 后方可开发。

## Foundation Layer

| Epic | System | GDD | Governing ADRs | Engine Risk | TR 覆盖 | Stories | Status |
|------|--------|-----|----------------|-------------|---------|---------|--------|
| [foundation-framework](foundation-framework/EPIC.md) | ①宿主+②Event Bus（TD-owned）| 无（architecture §2.1）| ADR-0001/0003 | LOW | 横切（支撑下游）| **7 stories** | Ready |
| [board-data](board-data/EPIC.md) | #1 棋盘数据 + ④Board DA Holder | board-data.md | ADR-0002/0001/0005 | **MEDIUM** | 17/17 | **8 stories** | Ready |
| [dice-rng](dice-rng/EPIC.md) | #3 骰子 + RNG 流 | dice.md | ADR-0004/0003/0007/0001/0005 | **MEDIUM** | 17/17 | **8 stories** | Ready |
| [player-turn](player-turn/EPIC.md) | #2 玩家与回合 + GameStateSnapshot | player-turn.md | ADR-0001/0006/0007/0003/0004/0005 | LOW | 17/17 | **9 stories** | Ready |

> **Foundation 共 32 stories**（7+8+8+9）。建议 Sprint0 开工首步：foundation-framework/story-007（PIE 隔离验证）+ dice-rng/story-007（RandRange/默认种子验证）+ board-data/story-002/007（UAssetManager/CSV 验证）—— 门控全部下游 headless 可测性。

## Core Layer

> 创建 2026-06-08（Foundation 32/32 全闭后）。依赖序：economy(bottleneck)→movement→property→tile-events。

| Epic | System | GDD | Governing ADRs | Engine Risk | TR 覆盖 | Stories | Status |
|------|--------|-----|----------------|-------------|---------|---------|--------|
| [economy-cash](economy-cash/EPIC.md) | #5 经济与现金 ⚠bottleneck | economy-cash.md | ADR-0007/0003/0006/0005/0002/0001 | LOW | 12C/4P/**2 Gap** | Not yet created | Ready |
| [movement](movement/EPIC.md) | #4 移动 | movement.md | ADR-0002/0003/0001/0005/0007 | LOW | 7C/8P/**3 Gap** | Not yet created | Ready |
| [property-ownership](property-ownership/EPIC.md) | #6 地产所有权 | property-ownership.md | ADR-0007/0003/0006/0005/0002/0001 | LOW | 8C/4P/**3 Gap** | Not yet created | Ready |
| [tile-events](tile-events/EPIC.md) | #7 事件格 | tile-events.md | ADR-0004/0003/0005/0001/0002/0007 | LOW | 4C/5P/**2 Gap** | Not yet created | Ready |

> **Core 4 epic 共 62 TR**（econ18+move18+prop15+event11），**10 Gap TR**（无 ADR/不完整 9 条 + prop-001 ADR 不完整）= 架构 review 已知 25 Gap carryover 子集。每个 epic 的 untraced TR 在 `/create-stories` 时其 story 标 **Blocked** 直到补 ADR：
> - econ-014/015（整数确定性/溢出防护）· move-010/017（Utility PULL/溢出告警；move-013=Full Vision defer）· prop-001/002/012（OQ-Prop-1 owner map 生命周期 ADR）· event-009/010（跨档接口：建房聚合/holder PULL）
> - **下一步**：`/create-stories economy-cash`（bottleneck 先行）或先 `/architecture-decision` 补 OQ-Prop-1/econ 数据校验等 ADR。

## Feature Layer

> 尚未创建。

## Presentation Layer

> 尚未创建。

---

## 备注

- **Save 框架（§2.1 ③，ADR-0005）** 留 persistence/Polish epic —— 依赖各状态系统 Serialize 契约，自然后期。
- **gate-check**：Foundation + Core 完成是 Pre-Production → Production 门的要求。
- **Sprint0 引擎验证前置**：board-data（ADR-0002 #6 UAssetManager 签名）/ dice-rng（ADR-0004 #4 RandRange/默认种子）/ foundation-framework（ADR-0001 #7 PIE 隔离）须开工首步验证（见各 epic DoD + control-manifest「Engine Verification Required」）。
- Foundation 全 51 TR（board 17 + dice 17 + turn 17）**零 Gap**，全 Covered/Partial（Partial = 跨 ADR 协同或实现期细化，非缺 ADR）。
