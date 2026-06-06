---
Epic: dice-rng
Status: Ready
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 001 — RNG 服务宿主 + 种子注入 + 通用原语封装

## Context

- **GDD**: `design/gdd/dice.md` — CR-2（中央可种子化 RNG 服务）/ CR-3（种子与确定性）/ F-2（RandomRange）/ F-3（RandomFloat01）/ States and Transitions（流未初始化→就绪）/ UE5.7 标注（`FRandomStream` 非 BP callable，须 C++ 封装）。
- **Requirement TR-ID**: TR-dice-001, TR-dice-002, TR-dice-008, TR-dice-010, TR-dice-015。
- **ADR Governing**:
  - **ADR-0004（确定性 RNG，primary）** — 裁定单一权威 `FRandomStream`（DiceService 持），所有 gameplay 随机走此流；钉死封装层 `SetSeed`/`GetCurrentSeed`/`RandomRange`/`RandomFloat01` 语义契约。
  - **ADR-0001（UObject 宿主）** — DiceService 挂 `UWorldSubsystem`（一局=World 边界、PIE 隔离），`OnWorldBeginPlay` 注入 Seed；与 player-turn 状态容器同生命周期层。
  - **ADR-0007（BP-C++ 边界）** — 含 gameplay 随机性→C++ 且经 `UFUNCTION(BlueprintCallable)`；`UDiceRngService` 是 BP 唯一允许的随机来源。
- **Engine**: UE 5.7（Blueprint 为主 + C++ 框架）。Risk: **MEDIUM**（`FRandomStream` 本体 pre-5.3 稳定 LOW；MEDIUM 来自 5.4–5.7 是否新增官方确定性/网络 RNG 子系统、`RandHelper` 是否仍走 `FRand()` 浮点中介 —— 超模型 ~5.3 知识截止）。
- **Engine Notes**（ADR-0004 Engine Compatibility，逐字保真）:
  - Post-Cutoff APIs Used: **无新增 post-cutoff API**。核心类型 `FRandomStream` / `FRandomStream::RandRange(int,int)` / `FRandomStream::FRand()` 均 pre-5.3 稳定。
  - Verification Required（实现前对 UE 5.7 源码/Release Notes 核实）④：是否新增官方确定性/网络 RNG 子系统（若有，Full Vision 联网重放可能优于手搓 `FRandomStream`，须重评）。
  - ADR-0001 Verification：`-nullrhi` headless 下 `UWorldSubsystem::Initialize/Deinitialize` 随 PIE World 创建/销毁正确触发；`OnWorldBeginPlay` 在 PIE Stop→Start 之间确实重新触发（Seed 重注入正确性）。
- **Control Manifest Rules**（Foundation 层，Manifest Version 2026-06-06）:
  - **Required**：per-match 服务挂 `UWorldSubsystem`（RNG 服务在列），生命周期边界=一局，PIE Stop 即销毁、Start 即重建（ADR-0001）；Seed 注入唯一时机 = `OnWorldBeginPlay`（不在 `Initialize`——此时玩家配置尚未由 `StartNewGame` 落地）；RNG 默认种子恒 0、禁 lazy-init（ADR-0001）；`FRandomStream` 原生非 BlueprintCallable，须 C++ 封装为 `UFUNCTION(BlueprintCallable)` 暴露（ADR-0004）；`ShouldCreateSubsystem` 排除 editor-preview World（ADR-0001）。
  - **Forbidden**：Never 把 per-match 对局态全挂 `UGameInstanceSubsystem`（PIE 隔离破裂，Seed 注入时机被迫挪走）；Never 在 `Initialize` 注入 Seed；Never 用 `UBlueprintFunctionLibrary` 作权威流宿主（无状态静态、不能持实例流）（ADR-0004）。
  - **Guardrail**：RNG `FRandomStream` LCG O(1) 抽取，非性能热点；CPU 帧预算 16.6 ms / 60 FPS。

## Acceptance Criteria

> scoped 到本 story（宿主 + 种子 + 通用原语正常路径；退化契约见 story-003，RollDice 见 story-002，lazy-init 见 story-004）。

- [ ] **AC-23** [Logic] `RandomRange`/`RandomFloat01`（含 `SetSeed`/`GetCurrentSeed`）均标 `UFUNCTION(BlueprintCallable)`、BP 可调、编译通过。
- [ ] **AC-18** [Logic] `seed=0` 经 `SetSeed(0)` 初始化后调 `RandomRange(1,6)`：返回值 ∈[1,6]（`0` 是合法种子、无特殊路径）。
- [ ] **AC-13** [Logic] 固定种子 `RandomFloat01()` ≥100 次：所有值 `0.0 <= f < 1.0`（1.0 严格排除）。
- [ ] **AC-12c** [Logic] 固定种子，对 MVP 实际 N 范围多组（2/6/12/100），计算 `floor(RandomFloat01()*N)` ≥100 次：结果恒 `∈ [0, N-1]`（永不返回 N）。
- [ ] **[Host]** DiceService 落 `UWorldSubsystem`；`OnWorldBeginPlay` 为唯一 Seed 注入时机；`ShouldCreateSubsystem` 排除 editor-preview World；PIE Stop→Start 重建实例（与 player-turn 同生命周期层，便于 Full Vision Seed 存活域对齐）。
- [ ] **[Host]** 权威 `FRandomStream` 作 DiceService 实例成员（值语义，无需 GC）；全项目恰一个权威流。

## Implementation Notes

> 来自 ADR-0004 Implementation Guidelines + Key Interfaces + ADR-0001，逐字保真不改写语义。

- **权威流唯一性**（ADR-0004 Guideline 2）：全项目恰一个权威 `FRandomStream`，挂 DiceService（宿主 ADR-0001）。`UBlueprintFunctionLibrary` 不可作宿主（无状态静态、不能持实例流，dice OQ-1 已删此候选）。
- **签名冻结**（ADR-0004 Key Interfaces，**不改签名**，只规范语义）:
  ```cpp
  UFUNCTION(BlueprintCallable) int32 RandomRange(int32 Min, int32 Max);
  UFUNCTION(BlueprintCallable) float RandomFloat01();   // [0.0,1.0), 1.0 严格排除
  UFUNCTION(BlueprintCallable) void  SetSeed(int32 InitialSeed);   // 0 是合法种子(非哨兵)
  UFUNCTION(BlueprintCallable) int32 GetCurrentSeed() const;       // 显式存取,勿靠 struct 反射(OQ-2)
  ```
- **`RandomFloat01` 半开 `[0,1)`**（ADR-0004 Guideline 3）：下游 `floor(f*N)` 仅 MVP 小 N 安全；`N≥2^24` 越界登记 OQ-4。
- **种子语义**：`SetSeed(0)` 合法（`0` 非哨兵）；正常对局每局不同种子，固定种子仅测试/调试（`bUseFixedSeed`）。
- **Seed 注入时机**（ADR-0001 / Manifest）：`OnWorldBeginPlay`，**不在 `Initialize`**（玩家配置尚未由 `StartNewGame` 落地）。
- **`GetCurrentSeed()` 显式 accessor**（ADR-0004 R5 / UE5.7 标注 L176）：勿依赖 `UPROPERTY(SaveGame)` struct 反射（反射可能只持久化 `InitialSeed` 而丢当前 `Seed`、静默破坏 OQ-2 续算）。
- **实现期源码核验**（ADR-0004 Guideline 8 / Engine Compatibility）：开工首步执行 Verification Required ①–④（详见 story-007 Sprint0 引擎验证）。

## Out of Scope

- `RollDice()` 主 API 与执行序铁律 → story-002。
- `RandomRange` 退化契约（Min==Max / Min>Max / 2^24 约束）→ story-003。
- lazy-init 兜底种子（未 SetSeed 路径）→ story-004。
- `OnDiceRolled` 事件 → story-005。
- 确定性序列 fixture（AC-8/9）→ story-006。
- CI 禁全局 RNG 静态扫描硬门 + 流隔离 spy → story-007。
- 当前骰序列化 → story-008。

## QA Test Cases

> 每 AC 一条 Given/When/Then，开发据此实现（非自创）。`-nullrhi` headless。

- **AC-23（接口编译/BP 可调）**
  - Given: `UDiceRngService : UWorldSubsystem`，`RandomRange`/`RandomFloat01`/`SetSeed`/`GetCurrentSeed` 标 `UFUNCTION(BlueprintCallable)`。
  - When: 模块编译 + 反射元数据生成。
  - Then: 编译通过；四函数在 BP 节点面板可见可调。
  - Edge: `FRandomStream` 成员不暴露 BP（仅经封装函数访问）。
- **AC-18（seed=0 合法）**
  - Given: `NewObject` DiceService，`SetSeed(0)`。
  - When: `RandomRange(1,6)`。
  - Then: 返回值 ∈[1,6]，不崩、不走特殊哨兵路径。
  - Edge: 负数种子（如 `SetSeed(-12345)`）同样合法、返回值 ∈[1,6]。
- **AC-13（RandomFloat01 半开区间）**
  - Given: 固定种子，`RandomFloat01()` 调 100 次。
  - When: 收集全部返回值。
  - Then: 每值 `0.0 <= f < 1.0`，无一 ==1.0。
  - Edge: 至少一次 f 接近 0（验下界可达）。
- **AC-12c（floor 不越界，MVP N 范围）**
  - Given: 固定种子，N ∈ {2,6,12,100}。
  - When: 每 N 计算 `floor(RandomFloat01()*N)` ≥100 次。
  - Then: 结果恒 `∈ [0, N-1]`，永不返回 N。
  - Edge: **R2 声明收窄** —— 本条只覆盖 MVP N 范围；`N ≥ 2^24` 浮点舍入越界在 MVP 不可达、无可测对象，守门登记 OQ-4，不在本条声称已覆盖（避免假安全感）。
- **[Host] 宿主生命周期**
  - Given: PIE World 启动。
  - When: `Initialize` → `OnWorldBeginPlay` → PIE Stop → Start。
  - Then: `Initialize/Deinitialize` 随 World 创建/销毁触发；`OnWorldBeginPlay` 在 Stop→Start 重触发；editor-preview World 不创建本 subsystem。

## Test Evidence

- **Path**: `tests/unit/dice/rng_service_host_primitives_test.cpp`（[Logic] AC-13/12c/18/23）。
- **Path**: `tests/integration/dice/rng_service_host_lifecycle_test.cpp`（[Host] 宿主生命周期，PIE World，依赖 ADR-0001 Verification）。
- **Status**: 未创建（dev-story 阶段落地）。

## Dependencies

- **Depends on**: 无（Foundation 零依赖根；ADR-0001/0004 已 Accepted）。**Sprint0 引擎验证（story-007）建议先行或并行**——宿主与 RNG 行为均待 5.7 核验。
- **Unlocks**: story-002（RollDice 依赖本流 + 封装）/ story-003（退化契约扩展本封装）/ story-004（lazy-init 兜底本宿主路径）/ story-005（事件）/ story-006（fixture）/ story-008（序列化）。
