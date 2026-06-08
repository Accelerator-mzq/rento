# Sprint 2 — 2026-06-09 to 2026-06-27

> Core 层实现：economy bottleneck 优先 + movement · Review mode: lean
> Engine: Unreal Engine 5.7 · Generated 2026-06-08
> **前置**：Foundation Sprint 0/1 全 32 story 已闭（FF 7 + dice 8 + board 8 + player-turn 9）；Core Gap ADR-0013/0014/0015 Accepted（解 prop/econ/move/event 10 Gap TR）；Core 4 epic 已拆 29 story（commit 2e71dfe）。

## Sprint Goal

实现经济现金 bottleneck 全 10 story（现金/发薪/租金/抵押/缴税/NLV/破产判据/存档），立起 Core 核心循环的**钱侧权威**；移动编排 5 story 跟进（位移/落地/发薪 push/holder 不投递）。本 sprint 完成即 Core 层 economy+movement 落地，property/tile-events 留 Sprint 3。

## Capacity

- Total days: 15 工作日（3 周）
- Buffer (20%): 3 天
- Available: 12 天 · 计划负荷 ~12.25 天（economy 8.25 + movement 4.0）→ **满载**
- 按 Sprint 0/1 mode-A workflow-assisted 实绩复评（实测加速 >1 story/天，超载 buffer 可吸收）

## Tasks

### Must Have（Critical Path · economy bottleneck — 全下游 6/8/9 + 呈现依赖其 Credit/Debit/算租）
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| econ-001 | Cash 服务 + Credit/Debit 受控写 + 守恒/不变式 | unreal-specialist | 1.0 | — | Cash≥0 不变式 + 原子守恒 + amount≥0 + 受控写封装（AC-1~5） |
| econ-002 | 经济事件契约 (4 delegate + EChangeReason 9值) | unreal-specialist | 0.75 | econ-001 | 4 owner-held DYNAMIC_MULTICAST + BlueprintAssignable + 方向消费方派生 + spy 计数（AC-33~39） |
| econ-003 | 发薪 F-1 (clamp[0,1000] + 溢出 guard + gate) | unreal-specialist | 0.5 | econ-001/002 | 运行时 clamp 非仅 ensure + gate 在 passed_go + GO 不双发（AC-6~8） |
| econ-004 | 地产租金 F-2 (piecewise, ×2 仅无房 base, 抵押短路) | unreal-specialist | 0.75 | econ-001/002 | 短路抵押先于表查 + 翻倍仅 house0 + 酒店不翻 + clamp（AC-9~13） |
| econ-005 | 车站/公用租 F-3/F-4 (count guards + dice_total PULL) | unreal-specialist | 0.75 | econ-001/002 | count≥1 守门 + dice_total 经回合2 holder PULL 不缓存（AC-14~18，ADR-0015） |
| econ-006 | 抵押/赎回现金腿 F-5/F-6 + 显示读接口 + 无套利 | unreal-specialist | 0.75 | econ-001/002 | 整数 ceil + economy 不校验前置 + GetUnmortgageCostForDisplay 纯函数 + 无套利（AC-19~22/42） |
| econ-007 | 缴税 F-7 + 买地现金腿 CR-4 (6→5 方向) | unreal-specialist | 0.5 | econ-001 | 税 flat sink + 买地 Debit 不登记归属/不反调6（AC-23+买地腿） |
| econ-008 | NLV F-9/F-10 is_insolvent + F-8 卖回 🔴最高风险 | unreal-specialist | 1.0 | econ-001/006 | 逐栋 floor 先于求和 + is_insolvent 严格< + 单一枚举入口 + 确定性（AC-24~32，AC-27 变异坐实） |
| econ-009 | 凑钱状态机 CR-7 + 破产现金侧 F-11 (Integration) | unreal-specialist | 1.5 | econ-006/008 | 清算顺序 spec mortgage-empty-first + 有界终止 + 回合2 驱动不直调6/8 + 现金侧 F-11（AC-30/31/35/36/43 主+变体ABC） |
| econ-010 | Cash 存档 round-trip (Integration) | unreal-specialist | 0.75 | econ-001/002 | CashByPlayer SaveGame round-trip 恒等 + 读档经容器写回 OnCashChanged 0次（econ-009/010/011） |

### Should Have（移动编排 — economy-independent，可与 Must 并行起；超载则甩 Sprint 3）
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| move-001 | CurrentTileIndex 容器 + SetTileIndex 受控写守界 | unreal-specialist | 0.75 | — | 落点∈[0,N-1] + from 实时读不缓存 + SetTileIndex 守界 ensure（AC-1~4） |
| move-002 | 事件契约 (OnPawnMoveStarted/OnPawnLanded + EArrivalContext) + 落地移交 | unreal-specialist | 0.75 | move-001 | EArrivalContext 3 项 uint8 + 两 delegate + arrivalContext 透传（AC-14~17/20） |
| move-003 | Advance 编排 + 发薪 push 门 + Total 不投递 + 越界告警 | unreal-specialist | 1.0 | move-001/002 | 调 board AdvanceIndex + 发薪门 P-1 + Total 不投递 spy==0 + 越界冒泡（AC-5~9/23/24，ADR-0015） |
| move-004 | TeleportTo (paysGo/advanceOnZero/去坐牢 context) | unreal-specialist | 0.75 | move-001/002 | 归约 steps_between + advanceOnZero 整圈/原地 + 去坐牢强制 SentToJail + 守界（AC-10~13） |
| move-005 | 三步有序契约 + 程间非重入 + 接口稳定 (Integration) | unreal-specialist | 0.75 | move-001~004 | SetTileIndex<push<OnPawnLanded 有序 spy + 程间非重入 + UFUNCTION（AC-18/21/22） |

### Nice to Have（economy-independent property 基底，velocity 充裕则拉入）
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| prop-001 | owner map 容器 (AoS) + 受控写封装 + 事件契约 | unreal-specialist | 0.75 | — | AoS TArray<FTileOwnershipState> 稠密+哨兵 + C++ private 封装 + 2 事件（AC-1~4，ADR-0013） |
| prop-004 | 派生谓词 F-1/F-2/F-3 (垄断+计数) + 确定性无缓存 | unreal-specialist | 1.0 | prop-001 | 空集守门 + 全盘遍历不低报 + 派生实时重算无缓存（AC-15~29b/40/40b） |

## Carryover from Previous Sprint
| Task | Reason | New Estimate |
|------|--------|--------------|
| 无 | Foundation Sprint 0/1 全 32 story 已闭，无 carryover | — |

## Risks
| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| economy 满载 8.25d + movement 4d ≈ 12.25d 略超 12d 可用 | High | Medium | movement 为 Should Have（可甩 Sprint 3）；按 Sprint 0/1 mode-A 实绩复评 |
| econ-009 凑钱/破产 Integration 跨 6/8/9（未实现）复杂度低估 | Medium | Medium | 给 1.5d；mock 6/8 接缝（spy CallLog，story 已写明）；ADR-0013/0014 契约已定 |
| econ-008 NLV 🔴最高风险门 AC-27 vacuous 风险 | Medium | High | 变异坐实非空壳（story Test Evidence 已要求）；逐栋 floor ADR-0014 已定 |
| Core→Feature 依赖：econ-009 AC-30/31 资产侧 + event-009 待 building8(未 epic) | Low | Low | 现金侧 mock 测试，资产侧归 9/6/8 story；MVP 经典值不触发 |
| SalaryAmount≤2e6 加载校验 propagate 债（board 未落） | Low | Low | MVP 经典 SalaryAmount=200 不触发；econ-003 运行时 clamp 兜底乘法；待 board story |

## Dependencies on External Factors
- UE 5.7 引擎本地可用
- ADR-0013/0014/0015 Accepted（已，2026-06-08）；Core 4 epic story 已拆（commit 2e71dfe）

## Definition of Done for this Sprint
- [ ] All Must Have tasks completed（economy 10 story）
- [ ] All tasks pass acceptance criteria
- [ ] QA plan exists (`production/qa/qa-plan-sprint-2.md`)
- [ ] All Logic/Integration stories have passing unit/integration tests（`tests/unit/{economy-cash,movement}/` + Integration round-trip/破产）
- [ ] 环依赖纪律经测试守护：6→5 单向（economy 不反调 6）、逐栋 floor、passed_go clamp、dice_total holder PULL
- [ ] Smoke check passed (`/smoke-check sprint`)
- [ ] QA sign-off report: APPROVED or APPROVED WITH CONDITIONS (`/team-qa sprint`)
- [ ] No S1 or S2 bugs in delivered features
- [ ] Design documents updated for any deviations
- [ ] Code reviewed and merged

## Feasibility 注

Sprint 0 (18) / Sprint 1 (14) Foundation 全闭，velocity 经 mode-A workflow-assisted 实测 >1 story/天。Sprint 2 计划负荷 ~12.25d（economy 8.25 must + movement 4.0 should）略超 12d 可用，但 movement 为 Should Have 可甩 Sprint 3，buffer 3d 可吸收 economy 重 Integration（econ-009 1.5d）。**economy bottleneck 优先**：property/movement/tile-events 全依赖 economy Credit/Debit/算租，故 economy 10 story 为 Must Have critical path。PR-SPRINT 反馈门 lean 模式 skip。

> **Scope check:** 全部 17 story（econ 10 + move 5 + prop 2 nice）来自既有 Core epic（无越界添加），无需 `/scope-check`。
