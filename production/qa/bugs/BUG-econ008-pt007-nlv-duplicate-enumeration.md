# BUG: player-turn 内联 NLV 重复枚举 + 硬编码 /2 绕过 economy F-9

> **ID**: BUG-econ008-pt007-nlv-dup
> **报告日期**: 2026-06-09
> **发现来源**: econ-008 /code-review（qa-tester FINDING-2 → 主会话 fresh-grep 坐实并扩展）
> **严重度**: Medium（潜在 House-Rules 分歧 bug；MVP 默认配置不触发）
> **类型**: 架构契约违反（单一枚举入口）+ 潜在数值分歧
> **状态**: Partially Resolved（:2154 econ-009 已修；:1996/:1991 AI-snapshot 待跟进）
> **建议 Owner**: player-turn（系统2）；propagate 债

---

## 解决进展（2026-06-09 econ-009）

- ✅ **`:2154` CheckInsolvencyWithNlv 内联 NLV — 已解除**：econ-009 用户裁定 option 3 把清算顺序 spec 抽到 economy
  `UEconomySubsystem::DecideNextLiquidationStep`，`RunForcedLiquidation` 改 Design X 结构性破产判定，
  **删除 CheckInsolvencyWithNlv**（连带其内联 `/2` NLV）。破产 NLV 聚合的 canonical 口径统一归 economy F-9
  `ComputeNetLiquidationValue`（逐栋 floor，单一真源）。TC9_BankruptcyNlvAggregation 删（覆盖移至 econ-008 TC2/TC5）。
- ⬜ **`:1996` + `:1991` AssembleSnapshot（AI 快照）— 仍待跟进**（AI-snapshot follow-up，非 econ-009 scope）：
  - `:1996` per-tile NLV 贡献 `E.HouseCount * (E.BuildingCost / 2) + (mortgaged?0:MV)` 硬编 /2（应经 economy 口径）；
  - `:1991` `E.UnmortgageCost = MV + CeilToInt(MV/10.0f)` 用 **float**（违 ADR-0014 整数纪律，应调 economy `GetUnmortgageCostForDisplay`）。
  建议在 AI-snapshot/ai-opponent epic 或 pt-008 follow-up 中统一到 economy 口径。

---

## 概述

econ-008 创建了 economy 的 canonical NLV 公式 `UEconomySubsystem::ComputeNetLiquidationValue`（F-9，逐栋 floor，经 `BuildingSellbackNum/Den` 旋钮）+ `FNlvAssetEntry` 输入 DTO。但 player-turn（pt-007，提交 `6e7f312`，**早于 econ-008**）已自行**内联枚举 NLV** 两处，硬编码 `/2` 卖回率，**不调** economy F-9。如今 canonical F-9 存在，这两处成为重复实现 + 潜在分歧源。

## 重现 / 证据

fresh-grep `Source/rento/*.cpp` 命中 2 处内联 NLV（除 economy 自身 F-9 外）：

1. **`PlayerTurnSubsystem.cpp:2154`**（`CheckInsolvencyWithNlv`，pt-007 簇 C2）：
   ```cpp
   Nlv += B.HouseCount * (BuildingProvider->GetBuildingCost(B.TileIndex) / 2);
   ```
2. **`PlayerTurnSubsystem.cpp:1996`**（同款内联）：
   ```cpp
   ? (E.HouseCount * (E.BuildingCost / 2) + (OS.bIsMortgaged ? 0 : OS.MortgageValue))
   ```

两处均：
- 硬编码 `/2`，**绕过 `BuildingSellbackNum/Den`（默认 1/2）旋钮**
- 不调 `ComputeNetLiquidationValue` / `ComputeBuildingSellback`
- 自行枚举 buildings + ownership 算 nlv，再传给 `EconomyResolver->IsInsolvent`

## 影响

- **潜在 House-Rules 分歧**：若 `BuildingSellbackNum/Den` 调为非默认（如 60% 卖回），economy F-9 返正确值，但 player-turn 两处仍按 50% → 破产判定偏差。**MVP 默认 1/2 时两路重合，故现有测试（含 econ-008 8 TC + `GameStateSnapshotAiHooksTest`）均不触发**。
- **违反单一枚举入口精神**（econ-008 AC-29「nlv 由 F-9 计算非独立重算」在系统层未达；AC-27③ 在 economy 内满足，但系统级存在第二/第三段枚举）。
- **维护风险**：未来改 `ComputeBuildingSellback`（如加 BC 下界 guard）player-turn 不跟进。

## 建议修复（player-turn follow-up story）

把 `PlayerTurnSubsystem.cpp:1996` 与 `:2154` 重构为：
1. 用 6/8 provider 数据装配 `TArray<FNlvAssetEntry>`（MortgageValue/bIsMortgaged/HouseCount/BuildingCost）；
2. 调 `EconomyResolver->ComputeNetLiquidationValue(Assets)` 得 nlv（单一 canonical 口径，自动经旋钮）；
3. 传该 nlv 给 `IsInsolvent`。

消除两处硬编码 `/2` + 重复枚举，统一到 economy F-9（ADR-0006「回合2 聚合 → economy F-9」的最终态）。

## 验证要求（修复时）

- 新增 House-Rules 分歧回归测试：把 `BuildingSellbackNum/Den` 设非 1/2，断言 player-turn NLV == economy `ComputeNetLiquidationValue` 同口径（坐实当前分歧、防回归）。
- `CheckInsolvencyWithNlv` 现有 `GameStateSnapshotAiHooksTest` 覆盖须保持通过。

## 关联

- econ-008 story: `production/epics/economy-cash/story-008-nlv-insolvency-f9-f10.md`（提供 canonical F-9，本身 COMPLETE）
- pt-007 story: PlayerTurnSubsystem C2 簇（`6e7f312`）
- ADR-0006（preaggregated_nlv 回合2 装配）、ADR-0014（逐栋 floor）
