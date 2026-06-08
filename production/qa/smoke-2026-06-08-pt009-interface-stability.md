# Smoke / Code-Review 记录 — pt-009 AC-4 接口稳定承诺（2026-06-08）

> story-009 AC-4：`PlayerState` 字段只增不改语义；回合阶段枚举 `ETurnPhase` 通过扩展新增、不破坏既有转换。
> 据 GDD player-turn.md L254「接口稳定承诺」+ architecture.md §D.2 事件表 + ADR-0005 append-only 核验。
> 性质同 story-008 AC-43（append-only），强度随 OQ-1 ADR。

## 核验结论：PASS

### 1. `URentoPlayerState` 字段只增不改语义（GDD CR-1 全 11 字段 + 额外字段）

`Source/rento/RentoPlayerState.h` 当前字段集（逐一对照 GDD CR-1）：
PlayerId / DisplayName / TokenColor / bIsAI / AIDifficulty / CurrentTileIndex / Cash /
bIsInJail / JailTurnsServed / bIsBankrupt / TurnOrderIndex（11 字段）
+ ConsecutiveDoubles（AC-1 权威）+ CurrentRollContext（story-005 holder）。

- 无字段被删除或语义重定义（历次 story pt-001/005/006 均为**追加** holder/setter，未改既有字段语义）。
- 受控写接口面（SetPosition/SetCash/SetJailState/SetCurrentRollContext/SetBankrupt）只新增、未改既有签名。
- story-008 序列化容器 `FPlayerStateRecord` 逐字段对齐这 11+2 字段（无吞字段）。

### 2. `ETurnPhase` append-only（整数索引不破坏既有转换）

`Source/rento/PlayerTurnTypes.h`：7 合法值 TurnStart(0)/RollPhase(1)/MovePhase(2)/ResolvePhase(3)/
PostRollAction(4)/TurnEnd(5)/JailTurn(6)，整数索引钉死。

- **story-008 已落 `static_assert` 编译期门**（PlayerTurnTypes.h 尾部）固定全部整数索引——重排即编译失败（机器执行的 append-only 保证，非纯 code-review 软约束）。
- 历史草稿值 `AIPlaybackGating` 已在任何引擎资产使用前删除（不违反 append-only，未分配持久化索引）。
- `EPlayerColor`(0..8) / `EAIDifficulty`(0..3) 同受 static_assert 保护。

### 3. 结构保证（story-009 CI 门）

- **CI 随机硬门**（`tools/ci/check-authoritative-purity.sh` Gate A）：权威模块禁引擎全局 RNG，机器执行。
- **CI 目录级断言**（Gate B1/B2 + in-engine harness `Rento.PlayerTurn.CiAuthorityGate`）：权威逻辑无 BP 派生类，结构保证无 BP 写权威状态 / 无 BP 随机旁路 → 接口稳定承诺有结构兜底，非仅约定。

## 结论

AC-4 接口稳定承诺 **PASS**（无破坏既有语义/转换的改动）。append-only 强度已由 story-008 static_assert 升为**编译期机器门**（超出 code-review 软约束）；CI 目录/随机门提供结构保证。
