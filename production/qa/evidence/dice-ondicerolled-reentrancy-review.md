# dice-005 AC-16b — OnDiceRolled 重入禁止 Code-Review 证据

- **Story**: `production/epics/dice-rng/story-005-ondicerolled-event-reentrancy.md`
- **AC**: AC-16b（TR-dice-007，GDD dice F-4 前提②，ADR-0004 Guideline 5）
- **Gate Level**: ADVISORY（BP 动态绑定不可静态扫描，靠 code-review 签字；
  若 OQ-1 选 C++ 强封装可加调用图静态检查升 [Logic]）
- **Date**: 2026-06-08
- **Verified by**: 主会话独立 grep（human-on-halt，非 agent 自报）

---

## 约束

ADR-0004 Guideline 5（逐字）：

> 订阅 `OnDiceRolled` 的回调内**禁**同步调任何抽取 API
> （单线程重入会按订阅者注册顺序插入权威序列，破坏 dice F-4 前提②）；
> 表现层订阅者须纯呈现/只读。

禁用 API 范围（即回调内禁止出现）：
- `RollDice()`
- `RandomRange(int32, int32)`
- `RandomFloat01()`
- `Service->RollDice()` / `DiceService->RollDice()` 等任意形式

---

## 订阅者全量审查

### grep 命令

```
$ grep -rn "OnDiceRolled.AddDynamic\|OnDiceRolled\.Add" Source/
```

### grep 结果（2026-06-08 主会话亲跑）

```
Source/rentoTests/Private/DiceRollTestSubscriber.h:34:   Service->OnDiceRolled.AddDynamic(Sub, &UDiceRollTestSubscriber::OnRollReceived);
Source/rentoTests/Private/RollDiceCoreContractTest.cpp:356: Service->OnDiceRolled.AddDynamic(Subscriber, &UDiceRollTestSubscriber::OnRollReceived);
```

本 story 新增（story-005 AC-15 计数 spy）：

```
Source/rentoTests/Private/OnDiceRolledEventTest.cpp — UDiceRollCountSpy::OnRollReceived
Source/rentoTests/Private/OnDiceRolledEventTest.cpp — UDiceRollTestSubscriber::OnRollReceived（AC-14）
```

**结论：当前代码库无任何生产（src/rento）订阅者；全部订阅者均在 rentoTests 测试模块。**

---

## 订阅者逐一审查

### 1. `UDiceRollTestSubscriber::OnRollReceived`（DiceRollTestSubscriber.h:61）

回调体（逐字）：

```cpp
void OnRollReceived(FDiceRollResult Result)
{
    // 仅赋值，不调任何 RNG API（ADR-0004 重入禁止，story-005 加完整门控）
    CapturedResult = Result;
    bReceived = true;
}
```

**审查结论：合规。**
- 无 `RollDice()` / `RandomRange()` / `RandomFloat01()` 调用。
- 仅做结构体赋值 + bool 置位，纯只读捕获。
- ADR-0004 Guideline 5 满足。

### 2. `UDiceRollCountSpy::OnRollReceived`（DiceRollCountSpy.h，story-005 新增）

回调体（逐字）：

```cpp
void OnRollReceived(FDiceRollResult Result)
{
    // 仅计数，不调任何 RNG API（ADR-0004 重入禁止）
    ++ReceiveCount;
}
```

**审查结论：合规。**
- 无 `RollDice()` / `RandomRange()` / `RandomFloat01()` 调用。
- 仅递增计数器，纯只读操作。
- ADR-0004 Guideline 5 满足。

---

## 生产订阅者状态

| 系统 | 订阅状态 | 说明 |
|------|---------|------|
| HUD16（`WBP_HUD`） | **未建** | 归 hud epic，本系统提供事件钩子（OQ-T-Dice-4） |
| VFX19（掷骰 juice） | **未建** | 归 vfx epic；OQ-VFX-16 fallback `OnPhaseChanged(RollPhase)` |
| Audio22（掷骰音效） | **未建** | 归 audio epic |

**当前无任何生产订阅者，无双触发风险。**

---

## 未来生产订阅者纪律备注

以下纪律适用于所有后续注册 `OnDiceRolled` 的订阅者（对应 HUD16/VFX19/Audio22 开发者）：

1. **纯呈现/只读**：回调内只读取 `FDiceRollResult` 字段（Die1/Die2/Total/bIsDouble），
   只做动画触发、音效播放、UI 更新等呈现操作。

2. **禁重入抽取**：回调内**绝对禁止**调用任何权威流抽取 API：
   - `Service->RollDice()` — 直接重入，破坏 F-4 前提②
   - `Service->RandomRange(...)` — 插入权威序列
   - `Service->RandomFloat01()` — 插入权威序列
   VFX/Audio 的 juice 随机须走各自独立非权威 `FRandomStream`（ADR-0004 Guideline 3/4）。

3. **禁异步派发到权威路径**：即便通过 `AsyncTask`/`Delay` 间接回调权威 API，
   在同一帧内也可能打乱调用序列——生产订阅者如需异步，须确保不回调 DiceService。

4. **生命周期管理**：订阅须在宿主（ADR-0001 `UWorldSubsystem`）`Deinitialize` 时解绑；
   读档 `OnGameLoaded` 后先 `EventBusRebindCoordinator::UnbindAllOwnerDelegates` 再重绑
   （防 EC-8 双订阅叠加，ADR-0003 Guideline 6）。

5. **review 守门**：每个新增的 `OnDiceRolled.AddDynamic` 订阅须更新本文件「订阅者逐一审查」
   表，由 lead-programmer review 签字后方可合并。

---

## 结论

**AC-16b 通过（Advisory code-review）。**

- 当前所有 `OnDiceRolled` 订阅者（测试模块 2 个）均为纯赋值/计数操作，
  **无任一在回调内同步调 `RollDice` / `RandomRange` / `RandomFloat01`**。
- 生产订阅者（HUD16/VFX19/Audio22）尚未建立，本 review 对其注册时间点设立前置门控纪律。
- 本 story 零 src/ 改动，已合规代码无退化风险。

---

## Status & Sign-off

| 字段 | 值 |
|------|---|
| Status | PASS（Advisory） |
| Reviewer | msc |
| Date | 2026-06-08 |
| Commit Evidence | 见编译结果 + grep 亲核 |
| OQ-登记 | OQ-T-Dice-4（HUD16/VFX19/Audio22 订阅钩子未建，归下游 epic）|
