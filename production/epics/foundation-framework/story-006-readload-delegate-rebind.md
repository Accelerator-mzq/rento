---
Epic: foundation-framework
Status: Complete
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-08
---

# Story 006 — Event Bus 读档集中解绑/重绑工具函数（防 EC-8 双订阅）

## Context

- **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 模块② Event Bus——读档集中解绑/重绑工具函数纪律层）
- **Requirement TR-ID**: 框架面支撑 **TR-turn-013**（读档后 delegate 重绑——invocation list 不序列化——+ 拓扑序重建：反序列化→重绑→才广播）。本 story 提供该 TR 的**集中重绑工具函数框架**（物理遍历各 owner delegate 先全量解绑后重绑）；存档反序列化/拓扑序重建主体归 Save epic（ADR-0005 CR-5/CR-6）。
- **ADR Governing**:
  - **ADR-0003（primary）— 事件总线与 Delegate 契约**：读档 `OnGameLoaded` 后，先**全量解绑**呈现层既有订阅（防 EC-8 双订阅叠加），再按权威订阅矩阵重绑；解绑/重绑经 Foundation ② Event Bus 纪律层的集中工具函数，物理上遍历各 owner delegate；订阅生命周期边界 = 宿主 `Initialize`（绑）/`Deinitialize`（解绑）。
  - **ADR-0005（secondary，TR-turn-013 列入）— 存档契约**：delegate 重绑（CR-6）——存档21 只广播 `OnGameLoaded`，呈现层（16/17/19/22）监听后各自先 Unbind 后 Bind；读档重建拓扑序（CR-5）：DA →（经济/地产/建房/事件格牌堆 互不依赖）→ 回合2 → 骰子 SetSeed → 重绑 → `switch(CurrentPhase)`。
- **Engine**: Unreal Engine 5.7（C++ 框架 + BP 订阅侧）— **Risk: LOW**（`AddDynamic`/`RemoveDynamic` 自 UE4 稳定）
- **Engine Notes（ADR-0003 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**。
  - Verification Required（ADR-0003 验证(2)，Sprint0 #10）：BP 端 `AddDynamic`/`RemoveDynamic` 在读档重绑路径下的**解绑幂等性**（读档重绑后订阅数 == 初次开局订阅数，无叠加）。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required（ADR-0003）**: 读档重绑纪律——`OnGameLoaded` 后先全量解绑呈现层既有订阅（防 EC-8 双订阅），再按权威订阅矩阵重绑；经 Foundation ② Event Bus 纪律层集中工具函数。对局期 delegate 的订阅须在宿主 `Initialize` 绑、`Deinitialize` 全部解绑（PIE Stop 不留野绑定）（source: ADR-0001/0003）。
  - **Required（ADR-0005）**: 读档重建拓扑序（CR-5）：DA →（经济/地产/建房/事件格牌堆 互不依赖）→ 回合2 → 骰子 SetSeed → **重绑** → `switch(CurrentPhase)`；禁重建 A 时读尚未重建的 B。delegate 重绑（CR-6）：存档21 只广播 `OnGameLoaded`，呈现层各自先 Unbind 后 Bind。`OnGameLoaded` 重建完成恰广播一次（AC-11）。
  - **Forbidden（ADR-0003）**: Never 让呈现层回写或被反调（呈现层 Exposes 为空，纯叶子不变式）。
  - **Guardrail（ADR-0003）**: 读档重绑遍历各 owner，一次性，毫秒级（不在帧热路径）。

## Acceptance Criteria

- [ ] AC-1: 存在集中重绑工具函数（如 `RebindPresentationDelegates`），物理遍历各 owner delegate，实现「先全量解绑 → 再按权威订阅矩阵重绑」。
- [ ] AC-2: 该工具在 `OnGameLoaded` 后被调用（绑定时机在拓扑序中位于「骰子 SetSeed 之后、`switch(CurrentPhase)` 之前」，符合 ADR-0005 CR-5）。
- [ ] AC-3: **解绑幂等性**——读档重绑后，每个 owner delegate 的订阅数 == 初次开局订阅数（无 EC-8 双订阅叠加）。
- [ ] AC-4: 订阅生命周期锚宿主：`Initialize` 绑、`Deinitialize` 全量解绑；PIE Stop 后无野绑定残留。
- [ ] AC-5: 呈现层重绑为纯叶子方向（订阅→显示），工具函数不引入呈现层回写/反调通道。
- [ ] AC-6: C++ 路径用 `RemoveDynamic` 后 `AddDynamic`；BP 路径先 `Unbind Event from` 再重绑（两路径均先解后绑，幂等）。

## Implementation Notes

> 逐字保真自 ADR-0003 规范#6 + Key Interfaces + ADR-0005 CR-5/CR-6，不改写语义。

1. **读档重绑纪律**：读档 `OnGameLoaded` 后，先**全量解绑**呈现层既有订阅（防 EC-8 双订阅叠加），再按权威订阅矩阵重绑。解绑/重绑经 Foundation ② Event Bus 纪律层的集中工具函数，物理上遍历各 owner delegate。订阅生命周期边界 = 宿主 `Initialize`（绑）/`Deinitialize`（解绑），PIE Stop 不留野绑定。
   ```cpp
   // 伪代码：先全量解绑防 EC-8 双订阅，再按权威订阅矩阵重绑
   void RebindPresentationDelegates(/* owner refs */);  // 由宿主 Initialize / OnGameLoaded 调
   ```
2. **拓扑序（ADR-0005 CR-5）**：DA →（经济/地产/建房/事件格牌堆 互不依赖）→ 回合2 → 骰子 SetSeed → **重绑** → `switch(CurrentPhase)`；禁重建 A 时读尚未重建的 B。本工具落在「重绑」步。
3. **CR-6**：存档21 只广播 `OnGameLoaded`，呈现层（16/17/19/22）监听后各自先 Unbind 后 Bind。`OnGameLoaded` 重建完成恰广播一次（AC-11）。
4. **两路径先解后绑**（ADR-0008 协同，下游呈现层落实）：C++ 路径 `RemoveDynamic` 后 `AddDynamic`（`AddUnique` 去重）；BP 路径必须先 `Unbind Event from` 再重绑。本 story 提供 C++ 侧集中工具函数；BP 侧 Unbind 纪律由下游呈现 epic 落实（本工具提供集中入口与权威矩阵遍历）。
5. **权威矩阵**：重绑依 architecture.md §Data Flow D.2 事件总览表（权威订阅矩阵）。

## Out of Scope

- delegate 声明/命名/payload USTRUCT 规范本身 → **story-005**。
- 存档反序列化、四重完整性门、`OnGameLoaded` 广播的 Save 侧实现 → Save epic（ADR-0005）。
- 各呈现层 widget 的 BP `Unbind Event from` 具体落实（HUD/卡UI/VFX/Audio）→ Presentation epic（ADR-0008/0010）。
- `switch(CurrentPhase)` 状态机重入逻辑 → turn epic（ADR-0001/0005）。

## QA Test Cases

> Integration（读档重绑跨 owner→consumer，防双订阅）；headless `-nullrhi`，spy 计订阅数。

- **TC-1 (AC-1/AC-3)**: GIVEN 初次开局后各 owner delegate 有 N 个订阅，WHEN 触发 `OnGameLoaded` 调 `RebindPresentationDelegates`，THEN 重绑后订阅数仍为 N（先全量解绑生效，无叠加为 2N）。
- **TC-2 (AC-2)**: GIVEN 读档重建序列，WHEN 观察调用顺序，THEN 重绑发生在「骰子 SetSeed 之后、`switch(CurrentPhase)` 之前」（拓扑序 CR-5）。
- **TC-3 (AC-4)**: GIVEN 宿主 `Initialize` 绑定后，WHEN 触发 `Deinitialize`（PIE Stop），THEN 全部订阅被解绑，无野绑定残留（重新广播 spy 计数==0）。
- **TC-4 (AC-5)**: GIVEN 重绑工具运行，WHEN 检查呈现层接口，THEN 仅订阅方向（订→显示），无回写/反调通道（纯叶子不变式）。
- **TC-5 (AC-6)**: GIVEN C++ 订阅者重绑两次，WHEN 检查，THEN 每次 `RemoveDynamic` 后 `AddDynamic`，订阅数不增（幂等）。
- **Edge cases**: 连续两次 `OnGameLoaded`（读两次档）订阅数仍恒 N（多次重绑幂等）；某 owner 无订阅者时遍历安全跳过不崩；`OnGameLoaded` 恰广播一次（AC-11，多次重建不重复广播）。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/foundation/readload_delegate_rebind_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-005（重绑遍历的 owner delegate 集合 + 命名/payload 规范）、story-001（订阅生命周期锚宿主 Init/Deinit）。
- **Unlocks**: Save epic（ADR-0005 读档拓扑序「重绑」步）；Presentation epic（HUD/VFX/Audio 读档重绑落实，TR-turn-013 功能面）。

## Completion Notes
**Completed**: 2026-06-08
**Criteria**: 6/6 COVERED（TC1-5 全 Success）。AC-2 可验部分（拓扑序时机）覆盖，`OnGameLoaded` 广播接线 DEFERRED-to-save-epic。
**Deviations**: 2 ADVISORY，登记 docs/tech-debt-register.md：① `UnbindAllOwnerDelegates` 硬编码 owner 列表（7+1），economy/building/bankruptcy owner epic 落地新增 delegate 时须同步扩展遍历（ADR-0003 事件表↔代码漂移风险）；② AC-2 OnGameLoaded→RebindPresentationDelegates 接线 defer-to-save-epic。
**Test Evidence**: Integration — `Source/rentoTests/Private/ReadLoadDelegateRebindTest.cpp`（5 TC）+ `RebindConsumerSpy.h`；全量 252/0 Failed/0 NotRun（基线 247+5 零回归）；`Saved/TestReport_ff006_full/index.json`。
**Code Review**: Complete — /code-review APPROVED（无 Required Changes；2 INFO suggestion 已转 tech-debt）。
**实现产出**：新建 `Source/rento/EventBusRebindCoordinator.{h,cpp}`（UEventBusRebindCoordinator:UWorldSubsystem）。架构=用户 2026-06-08 选项①「中央解绑+注册表重绑」，调和 ADR-0003 中央遍历 ↔ ADR-0005 CR-6 消费者自重绑。
**主会话独立验证**：agent 截断未跑测试（最终消息=Gauntlet 运行问题）；亲跑 Build+全量测试核实，独立审查抓修 2 真缺陷（TC1 Delta3==Delta3 同义反复→真比 ReferenceDelta；TC3 查新建对象天然空假覆盖→直接 UnbindAllOwnerDelegates() 验 Clear 真生效）。两者「测试绿但不真验」，build/run 不暴露，仅人工审查能抓。
