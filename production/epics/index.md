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
| [economy-cash](economy-cash/EPIC.md) | #5 经济与现金 ⚠bottleneck | economy-cash.md | ADR-0014/0007/0003/0006/0005/0002/0001 | LOW | 14C/4P/**0 Gap** | **10 stories** | Ready |
| [movement](movement/EPIC.md) | #4 移动 | movement.md | ADR-0015/0002/0003/0001/0005/0007 | LOW | 9C/8P/**0 Gap** (move-013 defer) | **5 stories** | Ready |
| [property-ownership](property-ownership/EPIC.md) | #6 地产所有权 | property-ownership.md | ADR-0013/0007/0003/0006/0005/0002/0001 | LOW | 13C/2P/**0 Gap** | **7 stories** | Ready |
| [tile-events](tile-events/EPIC.md) | #7 事件格 | tile-events.md | ADR-0015/0004/0003/0005/0001/0002/0007 | LOW | 5C/6P/**0 Gap** (event-009 待 building8) | **7 stories** | Ready |

> **Core 4 epic 共 62 TR**（econ18+move18+prop15+event11）。**10 Gap TR 已处置（ADR-0013/0014/0015 Accepted 2026-06-08）**：7 ADR-Covered（econ-014/015=0014 · move-010/017+event-010=0015 · prop-001/002/012=0013）+ 1 评估 Partial（event-009 待 building8 Feature epic）+ 1 Full Vision defer（move-013，不开 story）。详见 `docs/architecture/ADR-0013/0014/0015`。
> - ✅ **全 4 Core epic 已拆 story（2026-06-08）**：economy-cash 10 + movement 5 + property-ownership 7 + tile-events 7 = **29 story**（全 Ready，无 Blocked）。**下一步**：`/sprint-plan new`（规划 Core 实现 sprint）或 `/story-readiness economy-cash/story-001` → `/dev-story`（bottleneck 先行实现）。building8（#8 Feature 层）epic 待创建后闭合 event-009 真实接线 + SalaryAmount propagate 债。

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
