# Unit Tests（规划/清单）

隔离单元测试——公式、状态机、纯逻辑。**断言代码**落 `Source/Tests/`（UE Automation C++）；
本目录持**按系统的测试规划**（哪些 `[Logic]` AC 要测、test-spec、fixture 口径）。

每个系统一个子目录：`tests/unit/[system]/`，如：
- `tests/unit/economy/` — 租金/抵押/建房公式（economy-cash.md F-1..F-11）
- `tests/unit/dice/` — 确定性 RNG（dice.md F-2/F-4，ADR-0004 性质 D 重放）
- `tests/unit/ai/` — Buy Score / Buffer / 难度档差（ai-opponent.md F-1..F-7，AC-58/59/60 反幻想底线）
- `tests/unit/turn/` — 回合状态机 / 双点链 / return-only 编排（player-turn.md）

> tr-registry.yaml 的 `[Logic]` TR + 各 GDD Acceptance Criteria 是测试清单来源。
> `/qa-plan sprint` 按 story type 分类、生成本目录的 test-spec。
