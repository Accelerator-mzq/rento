---
Epic: foundation-framework
Status: Ready
Layer: Foundation
Type: Integration
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 002 — IGameClock DI 注入骨架（生产 / 测试时钟）

## Context

- **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 模块①——DI 注入点 `IGameClock`）
- **Requirement TR-ID**: 框架面支撑下游 HUD headless 可测性（HUD AC-12/24a/30/57 前提，归 HUD epic）；本 story 提供 `IGameClock` DI 注入点框架，HUD 状态机时序消费归 Presentation epic（ADR-0008）。本 epic 无独立 TR owns。
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期边界**：宿主提供统一 DI 注入点（`IGameClock`），供 headless fixture 注入 mock。时钟依赖经接口注入而非直读 `GetWorld()->GetTimeSeconds()`，被测逻辑须能在 `-nullrhi` 脱离渲染运行（可测性硬线，架构原则3 DI over Singleton）。
- **Engine**: Unreal Engine 5.7（C++ 框架）— **Risk: LOW**（纯 C++ 接口 + UObject 实现，无 post-cutoff API）
- **Engine Notes（ADR-0001 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**。`IGameClock` 是项目自定义抽象接口，无引擎版本依赖。
  - Verification Required（ADR-0001 验证④，完整见 story-007）：HUD 状态机注入 mock `IGameClock`，`-nullrhi` 下相位按受控时钟推进。
- **Control Manifest Rules**:
  - **Required（Foundation/ADR-0001）**: 提供统一 DI 注入点（`IGameClock`、Seed、mock 破产9 等），供 headless fixture 注入。
  - **Required（Presentation/ADR-0008，下游消费契约）**: 时钟依赖经 `IGameClock` 接口注入（生产 `FWorldGameClock`，测试 `FMockGameClock`）；非直读 `GetWorld()->GetTimeSeconds()`。
  - **Global**: ⚠ ADR-0008 `IGameClock*` 裸指针须改 `TWeakObjectPtr`/`TScriptInterface`（specialist finding，实现期落地）；`UFUNCTION()` 暴露用 `BlueprintCallable`/`BlueprintImplementableEvent`（source: current-best-practices）。
  - **Guardrail**: 接口调用 O(1) 非热路径；CPU 帧预算 16.6 ms / 60 FPS（source: ADR-0001）。

## Acceptance Criteria

- [ ] AC-1: 存在 `IGameClock` 接口，声明 `virtual double NowSeconds() const = 0`（语义：返回当前时间秒）。
- [ ] AC-2: 存在生产实现 `FWorldGameClock`，`NowSeconds()` 读 World time（`GetTimeSeconds()` 或等价权威世界时间）。
- [ ] AC-3: 存在测试实现 `FMockGameClock`，`NowSeconds()` 返回受控可设置的递增值（fixture 可显式推进，不依赖真实流逝）。
- [ ] AC-4: per-match 宿主基类（story-001）在 `Initialize` 时建立 `IGameClock` 注入点；headless fixture 可注入 `FMockGameClock` 替换生产时钟。
- [ ] AC-5: 在 `-nullrhi` headless 下，注入 `FMockGameClock` 后推进受控时钟，依赖方读到的时间值随之推进（证明 DI 生效、非直读世界时间）。
- [ ] AC-6: 时钟引用以 `TScriptInterface<IGameClock>` / `TWeakObjectPtr` 承载（非裸指针），符合 UObject 指针安全约束。

## Implementation Notes

> 逐字保真自 ADR-0001 Implementation Guidelines + Key Interfaces，不改写语义。

1. DI 注入点（架构原则3）——宿主提供，headless fixture 可注入 mock：
   ```cpp
   class IGameClock { public: virtual double NowSeconds() const = 0; };
   // 生产实现读 World time；测试实现返回受控递增值（HUD AC-12/24a/30/57 headless 前提）
   ```
2. `Initialize(Collection)`：取得依赖 Subsystem、**建立 `IGameClock` 注入点**；此时 World 尚未 BeginPlay，禁读玩家态。
3. 注入面须可在 `-nullrhi` headless 注入 mock：HUD 状态机 fixture 注入 mock 时钟、`-nullrhi` 推进相位（Migration Plan #2）。
4. **指针安全**（control-manifest Global / specialist finding）：`IGameClock` 引用成员避免裸指针，用 `TScriptInterface<IGameClock>` 或 `TWeakObjectPtr`，实现期落地。

## Out of Scope

- per-match 宿主基类骨架本身 → **story-001**（本 story 在其 `Initialize` 上挂 DI 注入点）。
- HUD 状态机时序逻辑（`E_cur`/`unpaused_elapsed`/F-4 过期判定等）→ Presentation epic（ADR-0008），非本 epic。
- Seed 注入（走 `OnWorldBeginPlay`，ADR-0004）→ story-007 验证 / dice epic 实现。

## QA Test Cases

> Integration（DI 注入点跨宿主生命周期）；headless `-nullrhi`。

- **TC-1 (AC-1/AC-2)**: GIVEN `FWorldGameClock` 注入有效 World，WHEN 调 `NowSeconds()`，THEN 返回与 World 当前时间一致的秒值（单调不减）。
- **TC-2 (AC-3)**: GIVEN `FMockGameClock` 初值 0，WHEN 设置受控值 5.0 再读 `NowSeconds()`，THEN 返回 5.0；再设 10.0 读到 10.0（受控可推进）。
- **TC-3 (AC-4/AC-5)**: GIVEN `-nullrhi` 宿主已 `Initialize`，WHEN 注入 `FMockGameClock` 并推进受控时钟 0→1.0→2.0，THEN 依赖方每次读到对应受控值（DI 生效，非读真实世界时间）。
- **TC-4 (AC-6)**: GIVEN 时钟引用成员，WHEN 静态检查类型，THEN 为 `TScriptInterface<IGameClock>` / `TWeakObjectPtr`，非裸 `IGameClock*`。
- **Edge cases**: 未注入时钟时依赖方读取须有安全兜底（早返 / 默认 0 + 日志，不崩）；mock 时钟可设置回退（用于测重定向场景），生产时钟单调。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/foundation/igameclock_di_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-001（DI 注入点挂宿主基类 `Initialize`）。
- **Unlocks**: 下游 HUD 状态机 headless 可测性（Presentation epic ADR-0008）；story-007（PIE 隔离验证④ 注入 mock 时钟推进相位）。
