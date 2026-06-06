---
Epic: foundation-framework
Status: Ready
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 007 — Sprint0 引擎验证：PIE 隔离 + OnWorldBeginPlay 重触发 + CancelHandle（ADR-0001 #7）

## Context

- **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 模块①——Sprint0 post-cutoff 宿主生命周期实测）
- **Requirement TR-ID**: 框架面验证支撑 **TR-turn-001/002**（World Subsystem 宿主 + 随局生灭 + 状态机随档重入）、**TR-dice-008**（DiceService 宿主 + Seed 注入点）、**TR-board-007/008**（DA 持有者宿主 + Deinitialize CancelHandle 防空棋盘）。本 story 是 **EPIC.md DoD 显式列入的 Sprint0 引擎验证故事**——验证 ADR-0001 的 post-cutoff（LLM 知识 ~5.3）待实测行为，门控全部下游 headless 可测性。
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期边界**：Verification Required ①②③ + Validation Criteria（PIE 隔离、`OnWorldBeginPlay` 重触发、`CancelHandle` 后无悬挂回调、mock `IGameClock` 受控推进）。
  - **ADR-0004（secondary）— 确定性 RNG**（Seed 注入点验证连带）：Seed 注入唯一时机 = `OnWorldBeginPlay`；默认种子恒 0、禁 lazy-init；固定 Seed → 固定序列 `-nullrhi` 跑绿。
- **Engine**: Unreal Engine 5.7（C++ 框架，headless `-nullrhi`）— **Risk: LOW（门控下游 headless 可测性；本身是 Sprint0 验证 #7 LOW 风险项）**
- **Engine Notes（ADR-0001 Engine Compatibility — 本 story 即验证项本体）**:
  - Post-Cutoff APIs Used: **None**（Subsystem 框架 pre-5.3 稳定），但行为须 5.7 实测确认。
  - **Verification Required（control-manifest Sprint0 #7，本 story 直接执行）**: `UWorldSubsystem::Initialize/Deinitialize` 随 PIE World 正确触发；`OnWorldBeginPlay` 在 PIE Stop→Start 重触发；`Deinitialize` `CancelHandle` 后无悬挂回调。
  - **失败处理**：若任一行为与 ADR-0001 假设不符，须修订 ADR-0001 并重 Accept（Sprint0 验证铁律）。
- **Control Manifest Rules**:
  - **Required（ADR-0001）**: `ShouldCreateSubsystem` 排除 editor-preview World；Seed 注入唯一时机 = `OnWorldBeginPlay`（不在 `Initialize`）；异步加载 `Deinitialize` 显式 `CancelHandle()`。
  - **Required（ADR-0004）**: RNG 默认种子恒 0、禁 lazy-init；固定 Seed → 固定序列可重放。
  - **Forbidden（ADR-0001）**: Never 在 `Initialize` 注入 Seed；Never 让 `Deinitialize` 后残留悬挂异步加载回调写已 GC 对象。
  - **Determinism（测试标准）**: 测试须产生每次相同结果——无随机 seed、无时间依赖断言；固定 Seed 下零方差。
  - **Guardrail**: 本验证非帧热路径；CI `-nullrhi` headless 运行（source: ADR-0001）。

## Acceptance Criteria

- [ ] AC-1: `-nullrhi` headless 下，对游戏 World 触发 PIE Start，per-match `UWorldSubsystem` 派生子类 `Initialize` 恰触发 1 次；PIE Stop 时 `Deinitialize` 恰触发 1 次（ADR-0001 验证①）。
- [ ] AC-2: PIE Play→Stop→Play 三轮，第二局无第一局残留状态（PIE 隔离，ADR-0001 Validation Criteria）。
- [ ] AC-3: `OnWorldBeginPlay` 在 PIE Stop→Start 之间确实**重新触发**（Seed 重注入正确性，ADR-0001 验证②）。
- [ ] AC-4: PIE Stop 时异步加载中途，无悬挂回调崩溃（`CancelHandle` 生效，ADR-0001 验证③）。
- [ ] AC-5: 固定 Seed 经 `OnWorldBeginPlay` 注入 → 产出固定 RNG 序列，在 `-nullrhi` headless 跑绿（证明 World Subsystem + Seed 注入可测；ADR-0001 Validation + ADR-0004）。
- [ ] AC-6: HUD 状态机注入 mock `FMockGameClock`，`-nullrhi` 下相位按受控时钟推进（ADR-0001 Validation，story-002 DI 落地验证）。
- [ ] AC-7: 默认种子恒 0、未注入 Seed 时不 lazy-init（ADR-0004），且 Seed 不在 `Initialize` 注入（仅 `OnWorldBeginPlay`）。

## Implementation Notes

> 逐字保真自 ADR-0001 Verification Required / Validation Criteria / Migration Plan + ADR-0004 Seed 注入纪律，不改写语义。

1. **PIE 隔离 + Initialize/Deinitialize 触发**（验证①）：`-nullrhi` 下 PIE Start/Stop 触发 `Initialize`/`Deinitialize` 各一次。验证基类 `UMatchSubsystemBase`（story-001）落地行为。
2. **OnWorldBeginPlay 重触发**（验证②）：`OnWorldBeginPlay` 在 PIE Stop→Start 之间确实重新触发——这是 Seed 重注入正确性的前提；若不重触发则确定性破裂（Risk 表登记，高影响）。
3. **CancelHandle 防空棋盘**（验证③）：PIE Stop 时异步加载中途，`Deinitialize` 的 `CancelHandle` 后无悬挂异步回调写已 GC 对象。验证 story-003 落地行为。
4. **Seed 注入纪律**（ADR-0004 + ADR-0001 IG#2）：Seed 注入唯一时机 = `OnWorldBeginPlay`（不在 `Initialize`——此时玩家配置尚未由 `StartNewGame` 落地）；RNG 默认种子恒 0、禁 lazy-init。固定 Seed → 固定序列 `-nullrhi` 跑绿（dice 确定性 fixture）。
5. **mock IGameClock 推进**（验证④）：HUD 状态机 fixture 注入 mock 时钟，`-nullrhi` 推进相位（验证 story-002 DI 注入面）。
6. **失败处理铁律**：任一验证项与 ADR 假设不符 → 修订对应 ADR 并重 Accept（不静默通过）。
7. **Determinism**：固定 Seed 下零方差；测试无随机 seed、无时间依赖断言（用 `FMockGameClock` 受控时间，不读真实流逝）。

## Out of Scope

- 宿主基类四 override 骨架实现本身 → **story-001**（本 story 验证其行为）。
- `IGameClock` 接口/实现本身 → **story-002**（本 story 验证 mock 注入推进）。
- 异步加载 `CancelHandle` 实现本身 → **story-003**（本 story 验证无悬挂回调）。
- RNG 流抽取/`RollDice` 执行序铁律实现 → dice epic（ADR-0004）；本 story 仅验证 Seed 注入时机 + 固定序列可重放。
- 其余 Sprint0 验证项（CommonUI/NativeTick/Enhanced Input/Audio/Substrate 等 #1–6/8–10）→ 各对应 epic 的 Sprint0 验证 story。

## QA Test Cases

> Integration（headless 引擎行为验证）；`-nullrhi`，固定 Seed + mock 时钟，零方差确定性。

- **TC-1 (AC-1)**: GIVEN `-nullrhi` 游戏 World，WHEN PIE Start 然后 Stop，THEN 派生子类 `Initialize` 计数==1、`Deinitialize` 计数==1。
- **TC-2 (AC-2)**: GIVEN 第一局写入若干服务状态，WHEN Stop 后第二局 Start，THEN 第二局服务为全新实例、无第一局残留（PIE 隔离）。
- **TC-3 (AC-3)**: GIVEN PIE Stop→Start，WHEN 监听 `OnWorldBeginPlay`，THEN 第二局重新触发一次（非仅首局触发）。
- **TC-4 (AC-4)**: GIVEN 异步加载进行中，WHEN 触发 PIE Stop，THEN 无悬挂回调崩溃、无写空棋盘（`CancelHandle` 生效）。
- **TC-5 (AC-5/AC-7)**: GIVEN 固定 Seed S 经 `OnWorldBeginPlay` 注入，WHEN 抽取固定次数 RNG，THEN 产出序列与冻结期望序列逐元素相等（零方差）；GIVEN 未注入 Seed，WHEN 检查，THEN 默认种子==0、无 lazy-init、`Initialize` 未注入 Seed。
- **TC-6 (AC-6)**: GIVEN HUD 状态机注入 `FMockGameClock`，WHEN `-nullrhi` 推进受控时钟 0→1.0→2.0，THEN 相位按受控时钟推进（非读真实世界时间）。
- **Edge cases**: PIE Start→Stop→Start→Stop 四次，每次 Init/Deinit 各 1 次（无累积泄漏）；异步加载在 Start 后立即 Stop（极短窗口）仍不崩；同一固定 Seed 跨两局产出同序列（确定性可独立重放）。

## Test Evidence

- **Type**: Integration（headless 引擎验证，Sprint0 门控）
- **Path**: `tests/integration/foundation/sprint0_pie_isolation_test.cpp`
- **Status**: 未创建（待 dev-story 实现，**Sprint0 开工首步**）

## Dependencies

- **Depends on**: story-001（宿主基类）、story-002（IGameClock DI）、story-003（CancelHandle 异步加载纪律）。
- **Unlocks**: 全部下游 epic 的 headless 可测性（验证通过 = ADR-0001/0004 宿主+RNG 封装真正可测）；dice 确定性 fixture 跑绿（ADR-0001 成功判据③）。
