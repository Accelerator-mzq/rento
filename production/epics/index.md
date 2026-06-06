# Epics Index

Last Updated: 2026-06-06
Engine: Unreal Engine 5.7
Manifest Version: 2026-06-06

> 一 epic 一架构模块。按层 dependency-safe 顺序创建（Foundation → Core → Feature → Presentation）。
> 每个 epic 须跑 `/create-stories [epic-slug]` 拆成可实现 story 后方可开发。

## Foundation Layer

| Epic | System | GDD | Governing ADRs | Engine Risk | TR 覆盖 | Stories | Status |
|------|--------|-----|----------------|-------------|---------|---------|--------|
| [foundation-framework](foundation-framework/EPIC.md) | ①宿主+②Event Bus（TD-owned）| 无（architecture §2.1）| ADR-0001/0003 | LOW | 横切（支撑下游）| Not yet created | Ready |
| [board-data](board-data/EPIC.md) | #1 棋盘数据 + ④Board DA Holder | board-data.md | ADR-0002/0001/0005 | **MEDIUM** | 17/17 | Not yet created | Ready |
| [dice-rng](dice-rng/EPIC.md) | #3 骰子 + RNG 流 | dice.md | ADR-0004/0003/0007/0001/0005 | **MEDIUM** | 17/17 | Not yet created | Ready |
| [player-turn](player-turn/EPIC.md) | #2 玩家与回合 + GameStateSnapshot | player-turn.md | ADR-0001/0006/0007/0003/0004/0005 | LOW | 17/17 | Not yet created | Ready |

## Core Layer

> 尚未创建。Foundation 接近完成后运行 `/create-epics layer:core`（依赖设计可能演化，不提前创建）。

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
