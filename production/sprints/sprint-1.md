# Sprint 1 — 2026-06-29 to 2026-07-17

> Foundation 完成 + 回合 / 编排层 · Review mode: lean
> Engine: Unreal Engine 5.7 · Generated 2026-06-06 · Updated 2026-06-06（FF-004/BD-004 移至 Sprint0）
> **前置**：依赖 Sprint0 验证的宿主 / RNG / 棋盘数据 / 事件总线基底（FF-001/002/003/004/005/007、DR-001/002/003/004/007、BD-001/002/003/004/005/007、PT-001）。Sprint0 任一 ⚙️ 验证证伪须先回修 ADR。

## Sprint Goal

在 Sprint0 验证的基底上完成 Foundation 剩余基础设施，并实现整个 player-turn 编排层（回合状态机 / 6 回合事件 / GameStateSnapshot 装配冻结 / 中途读档重绑）。本 sprint 完成即 Foundation 层全 32 story 落地，达 Pre-Production → Production 门的 Foundation 要求之半（另需 Core 层）。

## Capacity

- Total days: 15 工作日（3 周）
- Buffer (20%): 3 天
- Available: 12 天 · 计划负荷 ~13.25 天 → **满**（FF-004/BD-004 移至 Sprint0 后负荷下降；player-turn 仍占主体，按 Sprint0 实绩复评）

## Tasks

### Must Have（Critical Path）
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| FF-006 | Event Bus 读档集中解绑 / 重绑工具函数（防 EC-8 双订阅） | unreal-specialist | 1.5 | FF-005 (S0) | 读档 `OnGameLoaded` 后先全量解绑再按权威矩阵重绑；物理遍历各 owner delegate |
| DR-005 | OnDiceRolled 事件契约 + 单线程重入禁止 | unreal-specialist | 1.0 | DR-002 (S0) | `OnDiceRolled(FDiceRollResult)` DYNAMIC_MULTICAST + BlueprintAssignable；订阅者纯呈现不二次抽取 |
| DR-006 | F-5 确定性种子序列 fixture（PR-gate headless） | unreal-specialist | 0.75 | DR-002, DR-003 (S0) | ≥20 抽取逐字段精确相等；headless 通过于 `tests/unit/dice/` |
| PT-002 | ETurnPhase 回合阶段状态机（delegate 推进、禁 Latent、双点链 F-3） | unreal-specialist | 0.75 | PT-001 (S0) | 枚举字段 + delegate 推进；禁 Latent/FTimerHandle；读档 switch 重入 |
| PT-003 | F-1 回合推进 + 破产移出 + OnGameWon 单一事件源（return-only） | unreal-specialist | 0.75 | PT-002 | F-1 推进；破产移出；OnGameWon 据破产9 return-only winnerId 单一事件源 |
| PT-004 | 6 回合事件契约 + USTRUCT payload + AI 行动可观察 hook | unreal-specialist | 0.75 | FF-005 (S0), PT-002 | 6 事件广播；payload 必含 USTRUCT；OnAIActionExecuted 可观察 |
| PT-006 | RollPhase 消费 FDiceRollResult + 当前程 holder + AI 决策 RNG 走骰子流 | unreal-specialist | 0.75 | DR-002 (S0), PT-001 | 消费完整 `FDiceRollResult`；AI 同步决策随机走骰子3 权威流（禁自调引擎 RNG）|
| PT-007 | GameStateSnapshot 装配 / 冻结 + AI 决策 hook + ResolvePhase 聚合编排 | unreal-specialist | 1.5 | PT-002, PT-006 | 值语义 USTRUCT 决策期冻结；`Rent_top1/2`/`PreaggregatedNlv` 预聚合（AI 严禁自算）；三 AI hook 传 `const&` |
| PT-008 | 中途读档序列化 + delegate 重绑拓扑序 + 枚举 append-only + 可存档点查询 | unreal-specialist | 1.5 | FF-006, PT-007 | 序列化 CurrentPhase + 11 字段；读档重绑拓扑序；枚举 append-only；瞬态/定序中拒存 |

### Should Have
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| DR-008 | 当前骰序列化契约（完整 FDiceRollResult，MVP 不存 Seed） | unreal-specialist | 1.0 | DR-002 (S0) | 序列化完整 `FDiceRollResult` 读档不重掷；MVP 不存 Seed |
| BD-006 | 加载期完整性校验（警告类）：加载成功 + 结构化警告 | unreal-specialist | 0.75 | BD-005 (S0) | 加载成功 + 结构化警告；headless 测试 |
| BD-008 | 棋盘 DA 存档引用契约（BoardIdentifier）：存引用不存布局 | unreal-specialist | 1.0 | BD-001 (S0) | 存 `BoardIdentifier:FName` 引用而非全量布局；`GetBoardId()` 与资产路径解耦 |
| PT-005 | 受控写接口面（SetPosition / SetCash / SetJailState / SetBankrupt）C++ 强封装 | unreal-specialist | 0.75 | PT-001 (S0) | 受控写接口 C++ 强封装；外部禁直写 PlayerState 字段 |
| PT-009 | 回合状态机 / 编排落 C++ 权威模块 + CI 目录无 BP 派生类断言 | unreal-specialist | 0.5 | PT-002 | 回合模块落 C++ 权威；CI 目录级断言无 BP 派生类 |

### Nice to Have
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| — | （无；本 sprint 满载偏超） | — | — | — | — |

## Carryover from Previous Sprint
| Task | Reason | New Estimate |
|------|--------|--------------|
| （待定 — Sprint0 收尾时若 Should-Have 未消化则在此登记） | — | — |

## Risks
| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| **Sprint1 负荷 ~15d > buffer 后可用 12d，player-turn 占 8 story** | High | Medium | 用户已选「照排两 sprint」；按 Sprint0 实绩复评，必要时 PT-007/008（重 Integration 各 1.5d）甩 Sprint2 |
| Sprint0 ⚙️ 验证证伪连累 ADR | Medium | High | Sprint1 开工前确认 Sprint0 DoD「3 假设结论落档」；证伪则先 propagate 再开工 |
| PT-008 读档重绑依赖 FF-006，二者同 sprint | Medium | Medium | sprint 内排序 FF-006 → PT-007 → PT-008 |
| GameStateSnapshot 预聚合（防 5→8 反向环）实现复杂度被低估 | Medium | Medium | PT-007 给 1.5d；ADR-0006 字段构成已定型，按契约实现 |

## Dependencies on External Factors
- Sprint0 全部 ⚙️ 引擎验证完成且结论落档
- UE 5.7 引擎本地可用

## Definition of Done for this Sprint
- [ ] All Must Have tasks completed（10 项）
- [ ] All tasks pass acceptance criteria
- [ ] QA plan exists (`production/qa/qa-plan-sprint-1.md`)
- [ ] All Logic/Integration stories have passing unit/integration tests（`tests/unit/{board-data,dice,player-turn}/` + 读档重绑 Integration）
- [ ] CI 目录级断言：回合模块目录下无 BP 派生类（PT-009）
- [ ] Smoke check passed (`/smoke-check sprint`)
- [ ] QA sign-off report: APPROVED or APPROVED WITH CONDITIONS (`/team-qa sprint`)
- [ ] No S1 or S2 bugs in delivered features
- [ ] Design documents updated for any deviations
- [ ] Code reviewed and merged
- [ ] **Foundation 层 32 story 全闭合** → 可启动 `/create-epics layer:core`

## Feasibility 注

Sprint0 18 / Sprint1 14（2026-06-06 update：FF-004/BD-004 移至 Sprint0 消除跨 sprint 依赖）；Sprint1 计划负荷 ~13.25d，player-turn 集中尾部（含 PT-007 snapshot / PT-008 save 各 1.5d 重 Integration）。**用户 2026-06-06 决策：照排两 sprint（选项1），赌 AI 加速速度 >1 story/天，按 Sprint0 实绩决定是否触发延期或拆 Sprint2。** 退路：将 PT-007/008/009 + BD-008 甩 Sprint2，Sprint1 降至 ~10d 舒适区。

> **Scope check:** 全部 14 story 来自既有 Foundation epic（无越界添加），无需 `/scope-check`。
