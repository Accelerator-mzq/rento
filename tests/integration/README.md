# Integration Tests（规划/清单）

跨系统 + 存读档 round-trip 测试。**断言代码**落 `Source/Tests/`；本目录持规划/清单。

每个跨系统接缝一个子目录：`tests/integration/[seam]/`，重点覆盖**继承义务表**（systems-index）登记的跨系统测试：
- `tests/integration/save-load/` — 中途还原精确阶段 + ConsecutiveDoubles + 完整 `FDiceRollResult`(Die1/Die2) + 读档重绑 delegate（ADR-0005；dice OQ-T-Dice-5）
- `tests/integration/turn-bankruptcy/` — 回合2 调 `ResolveBankruptcy` return-only 不回调（2↔9 无环）
- `tests/integration/ai-turn/` — PostRollAction AI 同步决策 + 回合顺畅移交（N=1）
- `tests/integration/economy-property/` — 收租 push 快照 / 抵押-垄断口径（6→5 单向）

> 继承义务表（systems-index "Inherited Test Obligations"）+ tr-registry `[Integration]` TR 是清单来源。
> 部分集成项是 **CI-stub**（待依赖系统接入后转真实断言，见各 ADR CI-stub 关闭条件）。
