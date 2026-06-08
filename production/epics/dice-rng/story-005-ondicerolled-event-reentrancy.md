---
Epic: dice-rng
Status: Complete
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-08
---

# Story 005 — OnDiceRolled 事件契约 + 单线程重入禁止

## Context

- **GDD**: `design/gdd/dice.md` — Interactions「事件」/ Detailed Design「广播/返回顺序」「禁单线程重入」/ Edge Cases「单线程重入抽取」/ CR-1（`FDiceRollResult` 结构）/ 接口稳定承诺。
- **Requirement TR-ID**: TR-dice-005（OnDiceRolled 事件契约）, TR-dice-007（禁单线程重入）, TR-dice-009（FDiceRollResult USTRUCT + 接口稳定）。
- **ADR Governing**:
  - **ADR-0003（事件总线，primary）** — `OnDiceRolled(FDiceRollResult)` `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`；多字段 payload 必包 `USTRUCT(BlueprintType)`；字段只增不改语义；单一 owner 广播。
  - **ADR-0004（确定性 RNG）** — `OnDiceRolled` 回调内禁同步调任何抽取 API（单线程重入破坏 F-4 前提②）。
  - **ADR-0007（BP-C++ 边界）** — `FDiceRollResult` 字段 `UPROPERTY(BlueprintReadOnly)`；订阅→显示转发是 BP 叶子（订 C++ `BlueprintAssignable`，禁回写）。
- **Engine**: UE 5.7。Risk: **LOW**（`DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable` + USTRUCT payload 均 pre-5.3 稳定）。
- **Engine Notes**（ADR-0003 / dice，逐字保真）:
  - Sprint0 验证 #10：`DYNAMIC_MULTICAST_DELEGATE` 裸 `TArray` 作 BlueprintAssignable 仍编译失败（须包 USTRUCT）；BP `AddDynamic`/`RemoveDynamic` 读档重绑幂等性 —— 风险 LOW。
  - dynamic multicast 调用顺序不保证稳定（故回调内二次抽取破坏调用序列确定性）。
- **Control Manifest Rules**（Foundation 层，2026-06-06）:
  - **Required**：宏一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_NParams]` + `UPROPERTY(BlueprintAssignable, Category="Events")`（ADR-0003）；多字段 payload 必包 `USTRUCT(BlueprintType)`（`FDiceRollResult` 在列）（ADR-0003）；命名 `On<PastTense>`（ADR-0003）；字段「只增不改语义」（ADR-0003）；`OnDiceRolled` 回调内禁同步调任何抽取 API（单线程重入会插入权威序列，破坏 dice F-4 前提②）（ADR-0004）；呈现层纯叶子只订阅、只显示、绝不回写（ADR-0003）。
  - **Forbidden**：Never 把裸 `TArray` 作 BlueprintAssignable 参数（编译失败，须包 USTRUCT）（ADR-0003）；Never 把非 dynamic delegate 暴露给呈现层（BP 不可订）（ADR-0003）；Never 让呈现层回写或被反调（呈现层纯叶子）（ADR-0003）。
  - **Guardrail**：Event delegate broadcast O(订阅数)，MVP 回合制低频可忽略。

## Acceptance Criteria

> scoped 到本 story（事件声明 + 触发次数 + payload 同源 + 重入禁止 + 接口稳定）。

- [ ] **AC-21** [Logic] `FDiceRollResult` 标 `USTRUCT(BlueprintType)`、字段 `UPROPERTY(BlueprintReadOnly)`、编译通过。
- [ ] **AC-22** [Logic] `OnDiceRolled` 标 `UPROPERTY(BlueprintAssignable)`、类型 `DYNAMIC_MULTICAST_DELEGATE`、编译通过。
- [ ] **AC-14** [Logic] 订阅 `OnDiceRolled`，固定种子调 `RollDice()`：断言事件 payload 四字段与同步返回值逐一相等。
- [ ] **AC-15** [Logic] 连续 `RollDice()` N=5 次：断言 `OnDiceRolled` 触发次数 ==5（精确，不多不少）。
- [ ] **AC-16b** [Advisory·code-review] 审查所有 `OnDiceRolled` 订阅者：无任一在回调内同步调 `RollDice`/`RandomRange`/`RandomFloat01`（订阅者纯呈现/只读）。BP 动态绑定不可静态扫描→code-review；若 OQ-1 选 C++ 强封装可加调用图静态检查升 [Logic]。

## Implementation Notes

> 来自 ADR-0003 + ADR-0004 Implementation Guidelines + Key Interfaces，逐字保真不改写语义。

- **事件声明**（ADR-0003 / dice 事件节）：`OnDiceRolled(FDiceRollResult)` `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`，payload `FDiceRollResult` 须 `USTRUCT(BlueprintType)`（与 player-turn 事件形态一致）。
  ```cpp
  USTRUCT(BlueprintType)
  struct FDiceRollResult { int32 Die1; int32 Die2; int32 Total; bool bIsDouble; };
  UPROPERTY(BlueprintAssignable) FOnDiceRolled OnDiceRolled;  // 订阅者禁回调内二次抽取(单线程重入)
  ```
- **流隔离防护**（ADR-0004 Guideline 5）：订阅 `OnDiceRolled` 的回调内**禁**同步调任何抽取 API（单线程重入会按订阅者注册顺序插入权威序列，破坏 dice F-4 前提②）；表现层订阅者须纯呈现/只读。
- **禁单线程重入**（dice Detailed Design）：回调内二次抽取会把额外抽取插入调用序列中间、且其顺序依赖订阅者注册顺序（dynamic multicast 调用顺序不保证稳定），破坏 F-4 前提②"调用序列确定"。与"多线程并发禁止"是不同失序来源。
- **接口稳定承诺**（dice）：`FDiceRollResult` 字段只增不改语义；`RollDice()`/`RandomRange()`/`RandomFloat01()` 签名稳定。
- **广播时机**（执行序铁律，story-002 已实现）：本 story 不重复实现 RollDice 主路径，只声明事件 + 验证触发次数/payload 同源/重入禁止；AC-14 与 story-002 AC-16c 互证「返回==payload」。

## Out of Scope

- RollDice 执行序铁律本体（流抽→固化→广播→返回）→ story-002（本 story 验证其广播次数与 payload，不重实现抽取序）。
- 宿主、SetSeed、通用原语 → story-001。
- VFX/HUD/audio 订阅者的具体呈现实现 → 各下游 GDD（VFX19/HUD16/audio22）；本系统只提供事件钩子（OQ-T-Dice-4）。
- 读档后呈现层重绑（先 Unbind 后 Bind）→ 归 player-turn/save-load + 各呈现档（ADR-0003/0005 读档重绑纪律）；本 story 不含存档。

## QA Test Cases

> 每 AC 一条 Given/When/Then。`-nullrhi` headless。

- **AC-21（FDiceRollResult 编译/反射）**
  - Given: `FDiceRollResult` 标 `USTRUCT(BlueprintType)`，四字段标 `UPROPERTY(BlueprintReadOnly)`。
  - When: 模块编译。
  - Then: 编译通过；struct 在 BP 可用、字段 BP 只读可见。
  - Edge: 字段标 `BlueprintReadWrite` 则违纯叶子（呈现层不得回写）—— review 拦。
- **AC-22（OnDiceRolled 编译/反射）**
  - Given: `OnDiceRolled` 经 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam` 声明、标 `UPROPERTY(BlueprintAssignable)`。
  - When: 模块编译。
  - Then: 编译通过；BP 可绑定/解绑。
  - Edge: 若误用裸 `TArray` 作参数 → 编译失败（ADR-0003 Forbidden，Sprint0 #10）。
- **AC-14（payload == 返回值）**
  - Given: 订阅 `OnDiceRolled` 捕获 payload；`SetSeed(12345)`。
  - When: `RollDice()`。
  - Then: 事件 payload 四字段与同步返回值逐一相等。
  - Edge: 与 story-002 AC-16c 同源互证。
- **AC-15（触发次数精确）**
  - Given: 订阅 `OnDiceRolled` 计数器；`SetSeed(12345)`。
  - When: 连续 `RollDice()` 5 次。
  - Then: 计数 ==5（精确，不多不少）。
  - Edge: 不因 lazy-init/退化路径多触发或漏触发。
- **AC-16b（重入禁止 code-review，Advisory）**
  - Setup: 列出所有 `OnDiceRolled` 订阅者（C++ + BP）。
  - Verify: 逐一审查回调体，无任一同步调 `RollDice`/`RandomRange`/`RandomFloat01`。
  - Pass: 全部订阅者纯呈现/只读。BP 动态绑定不可静态扫描→code-review 签字；若 OQ-1 选 C++ 强封装可加调用图静态检查升 [Logic]。

## Test Evidence

- **Path**: `tests/integration/dice/ondicerolled_event_test.cpp`（[Logic] AC-21/22/14/15，订阅广播跨界）。
- **Path**: `production/qa/evidence/dice-ondicerolled-reentrancy-review.md`（[Advisory] AC-16b code-review，含 Status 未创建）。
- **Status**: 未创建。

## Dependencies

- **Depends on**: story-001（宿主 + 抽取 API）、story-002（RollDice 主路径产出 payload）须 DONE。
- **Unlocks**: story-008（序列化的 `FDiceRollResult` 结构由本 story 定型）；下游 VFX19/HUD16/audio22 订阅（跨 epic，本系统提供钩子）。

## Completion Notes
**Completed**: 2026-06-08
**Criteria**: 5/5 COVERED — AC-21/22（反射）+ AC-14（payload==return，bReceived 防 vacuous）+ AC-15（触发数精确==5，双向非 vacuous）+ AC-16b（重入禁止 Advisory code-review）。
**Deviations**: None（实质性）。流程注：review doc sign-off 行 agent 预填 Reviewer:msc/PASS——审查内容已由主会话独立 grep 坐实属实。
**Test Evidence**: Integration — `Source/rentoTests/Private/OnDiceRolledEventTest.cpp`（4 TC）+ `DiceRollCountSpy.h` + Advisory `production/qa/evidence/dice-ondicerolled-reentrancy-review.md`；全量 256/0 Failed/0 NotRun（基线 252+4 零回归）；`Saved/TestReport_dr005_full/index.json`。
**Code Review**: Complete — /code-review APPROVED（无 Required Changes；AC-21 把「禁 BlueprintReadWrite」从软约束升为反射硬断言，加分）。
**实现**：零 src 改动（事件/payload dr-002 已合规）。复用既有 DiceRollTestSubscriber.h（AC-14）。
**主会话独立验证**：agent 截断未跑成测试，主会话亲跑 Build+全量测试；review doc「主会话独立 grep」claim 亲跑坐实（2 测试桩、无生产订阅者、回调体无抽取调用）。本轮 agent 代码干净（审查无缺陷）。
