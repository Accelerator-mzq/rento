---
Epic: board-data
Status: Ready
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 003 — UBoardMathLibrary：F-1/F-2/F-3 环路拓扑 + AdvanceIndex 原子返回

## Context

- **GDD**: `design/gdd/board-data.md` — Formulas F-1（环路位移）、F-2（经过 GO 计数）、F-3（前向距离）、Interactions（`AdvanceIndex` 原子性契约）、运行时位移边界（Edge Cases）
- **Requirement TR-IDs**: TR-board-011、TR-board-012
- **ADR Governing**:
  - **ADR-0002（primary）— 棋盘数据容器**：F-1/F-2 封装为 `UBoardMathLibrary` 的 `BlueprintPure` 静态函数；`AdvanceIndex` 返回 `FBoardAdvanceResult` struct（非 out-param），强制原子性（AC-37a/b）；无独立公开 `PassedGoCount` 接口。
  - **ADR-0001 — UObject 宿主与生命周期**：纯函数库可在 `-nullrhi` headless 实例化测试（DI/可测前提）；本库无状态，不持服务实例。
- **Engine**: Unreal Engine 5.7 — Risk: LOW（2026-06-06 spike 已核验：`Floor`/`Floor to Integer64` 入参均 double、差在节点名+返回类型；用 int32 `Floor` 节点棋盘 32 安全）
- **Engine Notes**: ADR-0002 §决策③ 关联实现层指针 + **Sprint0 引擎验证 #6（2026-06-06 spike 完成）**：**机制更正**——5.7 `Floor`(→int32) 与 `Floor to Integer64`(→int64) **入参均 double**，差在 DisplayName + 返回类型（**非** float/double 两条入参推导路径）；风险是误选 int32 的 `Floor` 处理需 int64 的值 → 在 `static_cast<int32>` 截断（`KismetMathLibrary.inl` L709）。棋盘 32 格用 int32 `Floor` 节点本就安全。`UBoardMathLibrary` 为 `UBlueprintFunctionLibrary`（无状态静态），单测直测 C++ 函数无需 Blueprint 运行环境——载体与逻辑解耦。
- **Control Manifest Rules（Foundation 层）**:
  - **Required**：F-1/F-2 封装为 `UBoardMathLibrary` 的 `BlueprintPure` 静态函数（source: ADR-0002）。
  - **Required**：`AdvanceIndex` 返回 `FBoardAdvanceResult` struct（非 out-param）（source: ADR-0002）。
  - **Required（ADR-0007 权威边界）**：是 `[Logic] BLOCKING` AC 的被测逻辑 → C++ 独立 `UObject`/纯函数，须 `-nullrhi` 可实例化（source: ADR-0007）。
  - **Forbidden**：不暴露独立公开 `PassedGoCount(from, steps)` 接口（F-2 仅为 `AdvanceIndex` 返回元组 `passed_go` 分量）（source: ADR-0002 / GDD AC-37b）。

## Acceptance Criteria

- [ ] **AC-2（CR-1 环闭合）** [Logic] N=40，`AdvanceIndex(39, 1)` → NewIndex=0。
- [ ] **AC-8（F-1）** [Logic] N=40，`AdvanceIndex(28,7)` → NewIndex=35。
- [ ] **AC-9（F-1 越 GO）** [Logic] `AdvanceIndex(38,5)` → NewIndex=3。
- [ ] **AC-10（F-1 负步数）** [Logic] `AdvanceIndex(5,-7)` → NewIndex=38（数学取余，非负）。
- [ ] **AC-11（F-1 多圈）** [Logic] `AdvanceIndex(10,40)` → 10；`AdvanceIndex(10,83)` → 13。
- [ ] **AC-12（F-2 过 GO 一次）** [Logic] `AdvanceIndex(38,5).PassedGo` → 1。
- [ ] **AC-13（F-2 未过 GO）** [Logic] `AdvanceIndex(28,7).PassedGo` → 0。
- [ ] **AC-14（F-2 逆向穿越 GO）** [Logic] `AdvanceIndex(5,-7).PassedGo` → -1（如实输出有符号值）。
- [ ] **AC-15（F-2 多圈计数）** [Logic] `AdvanceIndex(38,45).PassedGo` → 2。
- [ ] **AC-16（F-3 前向距离）** [Logic] `StepsBetween(36,5)` → 9。
- [ ] **AC-17（F-3 同格）** [Logic] `StepsBetween(20,20)` → 0。
- [ ] **AC-29（步数超界照算+告警）** [Logic] N=40，`AdvanceIndex(0,1000)` → NewIndex=0、PassedGo=25，记「步数超界」告警日志；不 clamp、不中断。
- [ ] **AC-33（停在 GO 即经过）** [Logic] N=40，`AdvanceIndex(38,2).PassedGo` → 1（停在索引 0 等价经过一次）。
- [ ] **AC-34（from 越界 ensure + 通用式仍正确返回）** [Logic] N=40，`AdvanceIndex(-1,5)` → 触发 `ensure()`（报错+继续，非崩溃）+ 记 error 日志，且返回 `(NewIndex=4, PassedGo=1)`；`AdvanceIndex(40,1)` → 触发 ensure + 返回 `(NewIndex=1, PassedGo=0)`。
- [ ] **AC-37a（AdvanceIndex 原子返回）** [Logic] 对同一 `(from=38, steps=5)`，`AdvanceIndex` 返回单个 `FBoardAdvanceResult{NewIndex=3, PassedGo=1}`（两值由同一次调用返回）。
- [ ] **AC-37b（无独立 PassedGoCount 公开接口）** [Advisory] code-review + 编译期守门：代码库不存在独立公开 `PassedGoCount(from, steps)` 可被分离调用。

## Implementation Notes

（逐字取自 ADR-0002 §决策③ + GDD §Formulas Blueprint 实现约束，语义不改写）

1. F-1/F-2 **封装为 `UBoardMathLibrary` 的 `BlueprintPure` 静态函数**；`AdvanceIndex` 返回 **`FBoardAdvanceResult` struct**（非 out-param）。Blueprint 用 Break Struct 拆 `NewIndex`/`PassedGo`，两值由单一节点单次返回，强制原子性（AC-37a），天然满足「不存在独立 PassedGoCount 可分离调用」契约（AC-37b）。
2. **F-1**：`new_index = mod(from + steps, N)`，`mod` 为数学取余（结果恒 ≥ 0，Python `%` 语义）。`UBoardMathLibrary` 内 F-1 用组合式 `((A % N) + N) % N`，**禁止**直接用单个 Blueprint `%` 节点（C 风格对负数返回负值，AC-10/AC-14 必失败）。
3. **F-2**：`passed_go = floor((from + steps) / N) - floor(from / N)`（通用式，恒用此式，不用简化式）。`floor` 为向负无穷取整。对通用式的两个 `floor` 项各自先转 float 再 Floor，且显式使用 `Floor(float) → int32` 重载（UE5.x 有 float/double 两条路径，误用 double 重载返回 int64 会隐式截断）。两个易错点：① 忘记转 float 时 Blueprint 整数 `/` 节点会直接截断（朝零）；② `from + steps` 的整数加法本身可能溢出 int32（发生在 float 转换前，与精度无关）。
4. **F-3**：`steps_forward = mod(target - from, N)`，与 F-1 是完全相同的数学取余，`target - from` 同样可为负；**必须**用组合式 `((A % N) + N) % N`，禁止单个 `%` 节点（否则 `StepsBetween(36,5)` 返回 -31 而非 9，AC-16 必失败）。`from == target` 返回 0（语义=已在目标格）；调用方负责解释 0=原地不动还是整圈。
5. **越界与防护各司其职**：通用式保证计算结果对越界 `from` 仍正确（鲁棒自修正）；与 AC-34 的 `ensure()` 不矛盾——通用式负责「算对」，`ensure()` 负责开发期暴露上游状态损坏（报错+继续，非 `check()` 硬崩溃）。
6. **超界照算**：`|steps| > 2*N` 仍照常计算、绝不 clamp（clamp 破坏落点），但记一条「步数超界」告警日志供排查上游 bug（AC-29）。
7. 无独立公开 `PassedGoCount` 接口——F-2 仅为 `AdvanceIndex` 返回元组 `passed_go` 分量的文档/单测命名（AC-37b：运行时单测无法断言函数不存在，由 code-review/编译期静态检查守门）。

## Out of Scope

- `UBoardDataAsset`/`FBoardTileData` 定义 → story-001。
- `GetTileCount()`（N 的来源）由查询接口供 → story-004；本库 `AdvanceIndex(From, Steps, N)` 的 N 由调用方传入。
- 发薪裁决（`passed_go` 如何被经济(5)消费）→ 经济 epic，非本系统（本库只如实输出有符号值）。
- 传送类事件先调 F-3 求步数再调 F-1/F-2 的串联 → 事件格(7) epic（AC-39）。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

确定性 fixture，每 AC 一条（N=40 除非另注）：

| AC | Given (from, steps[, N]) | When | Then (期望) | Edge |
|---|---|---|---|---|
| AC-2 | (39,1) | AdvanceIndex | NewIndex=0 | 首尾相接 |
| AC-8 | (28,7) | AdvanceIndex | NewIndex=35 | 普通前进 |
| AC-9 | (38,5) | AdvanceIndex | NewIndex=3 | 越 GO |
| AC-10 | (5,-7) | AdvanceIndex | NewIndex=38 | 负步数取正 |
| AC-11 | (10,40)/(10,83) | AdvanceIndex | 10 / 13 | 整圈/多圈 |
| AC-12 | (38,5) | .PassedGo | 1 | 过 GO 一次 |
| AC-13 | (28,7) | .PassedGo | 0 | 未过 |
| AC-14 | (5,-7) | .PassedGo | -1 | 逆向穿越（有符号） |
| AC-15 | (38,45) | .PassedGo | 2 | 多圈计数 |
| AC-16 | StepsBetween(36,5) | — | 9 | 负差取正 |
| AC-17 | StepsBetween(20,20) | — | 0 | 同格 |
| AC-29 | (0,1000) | AdvanceIndex | NewIndex=0, PassedGo=25 + 告警日志 | 超界不 clamp |
| AC-33 | (38,2) | .PassedGo | 1 | 停在 GO=经过 |
| AC-34 | (-1,5)/(40,1) | AdvanceIndex | ensure 报警 + (4,1)/(1,0) | 越界 from 双重处理 |
| AC-37a | (38,5) | AdvanceIndex | 单 struct {NewIndex=3, PassedGo=1} | 原子返回 |

- **AC-37b**：[Advisory·code-review] Verify 无独立公开 `PassedGoCount`；记 `production/qa/evidence/`。
- 测试须确定性（固定输入→固定输出，无随机/无时间依赖）。

## Test Evidence

- **Path**: `tests/unit/board/board_math_library_topology_test.cpp`（[Logic] fixture，`-nullrhi` headless，可作 BLOCKING PR gate）
- **Status**: 未创建
- AC-37b 静态守门 → `production/qa/evidence/`（[Advisory]）

## Dependencies

- **Depends on**: story-001（DONE，提供 `FBoardAdvanceResult` struct 定义；若 struct 随本 story 落地则与 story-001 并行可行——以 story-001 为准）。
- **Unlocks**: 移动(4) 实现（调 `AdvanceIndex`）；事件格(7) 传送契约（调 `StepsBetween` + `AdvanceIndex`）；story-004（查询接口提供 N）。
