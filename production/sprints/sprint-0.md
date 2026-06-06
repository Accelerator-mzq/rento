# Sprint 0 — 2026-06-08 to 2026-06-26

> Foundation 层首批 sprint · 引擎验证优先 · Review mode: lean
> Engine: Unreal Engine 5.7 · Generated 2026-06-06 · Updated 2026-06-06（拉入 FF-004/BD-004 消除跨 sprint 依赖）

## Sprint Goal

验证 3 个 post-cutoff UE 5.7 API 假设（ADR-0001 PIE 隔离 / ADR-0002 UAssetManager 加载 / ADR-0004 RNG 退化与默认种子），并立起四个零依赖 Foundation 模块的宿主 / RNG / 棋盘数据 / 事件总线基底。**任一假设证伪须在 Sprint1 开工前回修对应 ADR + propagate。**

## Capacity

- Total days: 15 工作日（3 周，solo msc + Claude）
- Buffer (20%): 3 天
- Available: 12 天 · 计划负荷 ~15.75 天 → **超**（拉入 FF-004/BD-004 后负荷自 Sprint1 部分挪回；消除开发期阻塞滑动的代价）
- 速度置信度: **LOW**（首批 sprint，无历史速度；Sprint0 收尾按实绩回校 Sprint1）

## Tasks

### Must Have（Critical Path — ⚙️ = post-cutoff 5.7 API 验证门）
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| FF-001 | UMatchSubsystemBase per-match 宿主基类 + 生命周期 | unreal-specialist | 1.0 | — | `-nullrhi` 下 `NewObject` 实例化；`ShouldCreateSubsystem` 排除 editor-preview World |
| FF-003 | 异步加载纪律 + Deinitialize CancelHandle（防 PIE 空棋盘） | unreal-specialist | 1.0 | FF-001 | PIE Stop 后无悬挂回调；CancelHandle headless 断言 |
| FF-007 ⚙️ | **PIE 隔离验证** + OnWorldBeginPlay 重触发 + CancelHandle | unreal-specialist | 1.0 | FF-001, FF-003, FF-002 | ADR-0001 #7：Init/Deinit 随 PIE World 触发、OnWorldBeginPlay 重触发 headless 断言 |
| DR-001 ⚙️ | RNG 服务宿主 + 种子注入 + 通用原语封装 | unreal-specialist | 1.0 | FF-001 | 唯一权威 `FRandomStream` 持有；`SetSeed` 确定性 |
| DR-003 ⚙️ | **RandomRange 退化/边界契约** | unreal-specialist | 0.75 | DR-001, DR-002 | ADR-0004 #4：Min==Max 早退；`RandRange` 浮点中介行为核实 |
| DR-004 ⚙️ | **lazy-init 兜底种子安全（禁默构 0）** | unreal-specialist | 0.75 | DR-001, DR-002 | ADR-0004 #4：默认构造种子是否恒 0 二次确认；兜底取 `FPlatformTime::Cycles` |
| DR-007 ⚙️ | **流隔离 + CI 禁全局 RNG 静态扫描硬门** | unreal-specialist | 1.0 | DR-001 | grep 硬门生效；白名单 `UDiceRngService` 内 `FRandomStream` |
| BD-001 | FBoardTileData 结构 + UBoardDataAsset 载体定义 | unreal-specialist | 1.0 | FF-001 | `TArray<int32>` 列编译通过；`UPrimaryDataAsset` 载体 |
| BD-002 ⚙️ | **DA 持有者宿主：加载 / 生命周期 / 热切换边界** | unreal-specialist | 1.0 | BD-001, FF-001 | ADR-0002 #6：`UAssetManager` 5.7 加载签名核实；`UPROPERTY` 强引用防 GC |
| BD-003 ⚙️ | UBoardMathLibrary 拓扑 + AdvanceIndex 原子返回 | unreal-specialist | 0.75 | BD-001 | ADR-0002 #6：`Floor(float)→int32` 重载推导；headless fixture |
| BD-004 | 只读查询接口集：GetTileCount/GetTileData/GetTilesInGroup/GetBoardId | unreal-specialist | 0.75 | BD-001, BD-002 | 只读 O(1)；GetTilesInGroup 升序（解锁 BD-007 AC-6） |
| BD-007 ⚙️ | DA_Board_Classic40 temp-fill + [Config] 校验 harness + cook 门控 | unreal-specialist | 0.5 | BD-001, BD-004, BD-005 | ADR-0002 #6：CSV `TArray` 列报错二次确认；`bIsPlaceholderData` cook 前可检测 |

### Should Have
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| FF-002 | IGameClock DI 注入骨架（生产 / 测试时钟） | unreal-specialist | 1.0 | FF-001 | `FWorldGameClock` / `FMockGameClock` 注入点就位（FF-007 AC-6 前提） |
| FF-004 | 跨局 UGameInstanceSubsystem 宿主框架（Save/Audio/Setup 入口） | unreal-specialist | 1.0 | FF-001 | 跨局宿主框架；`StartNewGame` 入口挂载点（解锁 PT-001） |
| FF-005 | Event Bus 纪律层：delegate 规范 + USTRUCT payload 契约 | unreal-specialist | 1.0 | FF-001 | 多字段 payload 必含 USTRUCT；裸 TArray 编译失败 |
| DR-002 | RollDice 核心契约 + 执行序铁律 | unreal-specialist | 0.75 | DR-001 | 流抽→固化→广播→返回同值；返回值 == payload（DR-003/004 前提） |
| BD-005 | 加载期完整性校验（拒绝类）+ 结构化错误码 | unreal-specialist | 0.75 | BD-001 | 失败结构化错误码拒绝进对局（BD-007 资产基线前提） |
| PT-001 | PlayerState 容器 + StartNewGame 开局入口 + 开局定序 | unreal-specialist | 0.75 | FF-001, FF-004 | 11 字段容器；`StartNewGame(FGameSetupConfig)`；开局定序 headless |

### Nice to Have
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|--------------|---------------------|
| — | （无；本 sprint 满载） | — | — | — | — |

## Carryover from Previous Sprint
| Task | Reason | New Estimate |
|------|--------|--------------|
| —（首批 sprint，无 carryover）| — | — |

## Sprint 内排序约束（dependency-safe）
- FF：001 → 002 → 003 → 005 → 007（007 需 001/002/003）
- DR：001 → 002 → 003 / 004（004 需 002 作兜底返回值检查）→ 007
- BD：001 → 002 / 003 / 005 → 004 → 007（007 需 004 的 GetTilesInGroup + 005 拒绝类基线）
- PT：FF-004 → PT-001（StartNewGame 入口宿主先行）

## Risks
| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| post-cutoff 5.7 API 假设证伪（RandRange / UAssetManager / 默认种子） | Medium | High | ⚙️ 6 项全进 Must-Have 关键路径；证伪即触发 ADR 回修 + propagate，**不带病进 Sprint1** |
| 首批 sprint 无历史速度，估算置信度 LOW | High | Medium | Sprint0 收尾按实绩回校 Sprint1（`/retrospective`） |
| 计划负荷 ~15.75d 超 buffer 后 12d 可用 | Medium | Medium | Must 12 项为硬核心；Should 6 项按实际产能消化，可顺延 Sprint1 |
| BD-007 软依赖 BD-006（警告类，留 Sprint1） | Low | Low | BD-006 为 Advisory 警告类，不阻塞 BD-007 可测 AC（靠 BD-005 拒绝类 + 结构校验）；仅「干净基线」说法依赖，可接受 |

## Dependencies on External Factors
- UE 5.7 引擎本地可用（`E:\Epic Games\UE_5.7`，已核实）— 全部 ⚙️ 验证项依赖真实引擎而非文档
- 无外部服务 / 第三方依赖

## Definition of Done for this Sprint
- [ ] All Must Have tasks completed（12 项含全部 6 ⚙️ 引擎验证门）
- [ ] All tasks pass acceptance criteria
- [ ] **3 个 post-cutoff API 假设结论落档**：证实 → ADR 标注「5.7 verified」；证伪 → ADR 回修 + propagate 工单
- [ ] QA plan exists (`production/qa/qa-plan-sprint-0-2026-06-06.md`，含 FF-004/BD-004)
- [ ] All Logic/Integration stories have passing unit/integration tests（`tests/unit/{board,dice,player-turn}/` + `tests/integration/{foundation,board,dice}/`）
- [ ] Smoke check passed (`/smoke-check sprint`)
- [ ] QA sign-off report: APPROVED or APPROVED WITH CONDITIONS (`/team-qa sprint`)
- [ ] No S1 or S2 bugs in delivered features
- [ ] Design documents updated for any deviations
- [ ] Code reviewed and merged

> **Scope check:** 本 sprint 全部 18 story 来自既有 Foundation epic（无越界添加），无需 `/scope-check`。
