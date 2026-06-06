# Sprint0 引擎验证报告 — UE 5.7 post-cutoff 假设核验

**Date**: 2026-06-06
**Method**: Workflow `sprint0-engine-verification-spike`（6 只读源码探针 → 各自对抗式复核）
**Engine source**: `E:\Epic Games\UE_5.7`
**Run**: 12 agent · ~470k token · 6/6 对抗复核通过（全 grounded=true / agree=true / high confidence）
**关联**: ADR-0004（确定性 RNG）· ADR-0002（棋盘数据容器）· Sprint0 DoD「3 个 post-cutoff API 假设结论落档」

> 方法学：探针被强制「禁记忆作答，必须 Read 真实源码引 path+行号」；对抗复核员默认怀疑、复查 evidence 路径真实性，专抓「拿 ~5.3 训练记忆冒充 5.7 源码」。模型知识截止 ~5.3，本报告核验的正是 5.4–5.7 行为。

---

## 结论速览

| 项 | 假设 | 结论 | ADR 动作 |
|----|------|------|----------|
| ADR-0004 ① | `RandRange` 走 `Min+TruncToInt(FRand()*Range)` 浮点中介 | ✅ CONFIRMED | 标 5.7 verified（+ 跨平台前提注） |
| ADR-0004 ③ | `FRandomStream()` 默认种子恒 0 | ✅ CONFIRMED | 标 5.7 verified |
| ADR-0004 ④ | 5.7 无新官方确定性/网络 RNG 子系统 | ✅ CONFIRMED | 标 5.7 verified（+ Mass helper 备注） |
| ADR-0002 ⑥a | `UAssetManager` 同步加载 Primary DA API 稳定 | ✅ CONFIRMED | 标 5.7 verified（+ 新重载/性能注） |
| ADR-0002 ⑥b | Floor float/double 两路径、误用 double 返 int64 截断 | ❌ REFUTED | 更正机制描述（决策不变） |
| ADR-0002 ① | CSV 含 TArray 列报 `Unsupported Property type` | ❌ REFUTED | 改论证理由为「实务不便」（决策不变） |

**4 confirmed / 2 refuted。两处 refuted 均不动 DA 选型决策，只更正论证理由。**

---

## 逐项明细

### ✅ ADR-0004 ① — RandRange 浮点中介：CONFIRMED
**实读 `RandomStream.h`**（全 header-inline，无 .cpp）：
- `RandRange(int32 Min,int32 Max)`（L197-202）= `Min + RandHelper((Max-Min)+1)`
- `RandHelper(int32 A)`（L186-190）= `(A>0) ? FMath::TruncToInt(GetFraction()*float(A)) : 0`
- `GetFraction()`（L115-124）= `*(uint32*)&Result = 0x3F800000U | (Seed>>9); return Result - 1.0f`（[0,1) float）
- `FRand()`（L176-179）= `GetFraction()` 纯别名
- 注释 L14-18："Very bad quality in the lower bits. Don't use the modulus (%) operator."

→ 确为单精度 float 乘法 + `TruncToInt` 截断的浮点中介路径，**非整数取模**。假设成立。
**前提注**：跨平台位级重放依赖单精度 float 乘法跨平台一致（非整数运算）。**同平台/同构建重放绝对安全**（MVP 单机）；ADR 若主张「跨平台位级可重放」须写明「假定目标平台 IEEE-754 单精度行为一致」。

### ✅ ADR-0004 ③ — 默认种子 0：CONFIRMED
- `FRandomStream()`（L30-33）= `: InitialSeed(0), Seed(0)` 写死常量 0，不调 `GenerateNewSeed`、不取时间/名字种子。
- 唯一非确定路径：`Initialize(FName)` 的 `NAME_None` 分支（L83）走 `FPlatformTime::Cycles()`；`GenerateNewSeed()` 走 `FMath::Rand()`（L107）须显式调用。
- `Reset()`（L92-95）用 `InitialSeed` 回滚 → 默构对象 Reset 后仍恒 0。

→ 默认构造种子恒 0、可重现，作 lazy-init 兜底安全前提在 5.7 成立。**无需回修。**

### ✅ ADR-0004 ④ — 无新 RNG 子系统：CONFIRMED
- 全 `Engine` 树 Glob `*Random*/*Rng*/*Deterministic*` + fresh-grep `*RandomSubsystem/*RngSubsystem/RandomManager/RandomService/NetworkRandom/DeterministicRandom` **零命中**。
- `FRandomStream` 仍是唯一原语（SRand-based 有状态流），未加 `UE_DEPRECATED`、未加 `NetSerialize`/复制接口；`GetInitialSeed()`/`GetCurrentSeed()`（L97/L166，重放快照依赖）5.7 未动。
- 唯一沾边：Mass 插件 `UE::RandomSequence`（`MassCommon/Public/RandomSequence.h`）—— 基于 Fibonacci hashing 的**无状态纯函数 helper**（同 index 恒同值），非 Subsystem、无网络/复制/存档语义，**不构成权威 RNG 服务**。

→ 手搓单权威 `FRandomStream` + 各系统独立非权威流 + `GetCurrentSeed` 快照重放，在 5.7 下仍是唯一官方原语路径，无更优替代。**无需重评。**
**备注**：`UE::RandomSequence` 可作「按索引取确定值」场景（如 juice 按 tile index 取确定外观）的可选补充，登记供 Full Vision 联网阶段参考。

### ✅ ADR-0002 ⑥a — UAssetManager 加载：CONFIRMED
- `GetPrimaryAssetObject(const FPrimaryAssetId&) const`（`AssetManager.h` L219）完整保留，注释 "works even if the asset wasn't loaded explicitly"（同步取已加载对象，未加载返 nullptr）——与假设逐字相符。
- `LoadPrimaryAsset/LoadPrimaryAssets`（L308-319/283-305）仍返 `TSharedPtr<FStreamableHandle>`。
- 同步等待：`FStreamableHandle::WaitUntilComplete(Timeout=0.0f, bStartStalledHandles=true)`（`StreamableManager.h` L341）+ `HasLoadCompleted()`（L197）+ `GetLoadedAsset()`（L365）。

→ 同步加载通路成立。**非阻塞备注**：(1) 5.7 新增 `FAssetManagerLoadParams&&` 重载，注释 "Prefer the overloads with Param structs for new code"（L274）→ 新代码宜采用；(2) 所有 Load 重载末位新增带默认值 `UE::FSourceLocation Location=Current()`（旧调用点兼容）；(3) **`<100ms` 是工程性能预期非 API 保证** —— 源码仅保证同步阻塞通路，ADR 宜把 `<100ms` 表述为「小体积 DA + 同步 WaitUntilComplete 的工程目标，须实测验证」。

### ❌ ADR-0002 ⑥b — Floor 重载：REFUTED（机制描述错误，决策不变）
**假设的「float/double 两条按入参类型推导路径」不成立。** 5.7 `KismetMathLibrary` Floor 族**入参统一 `double`，无 float 重载**：
- `FFloor(double A)` → **int32**，DisplayName="Floor"（`.h` L787-788；`.inl` L707-709 = `static_cast<int32>(FMath::FloorToInt(A))`）
- `FFloor64(double A)` → **int64**，DisplayName="Floor to Integer64"（`.h` L803-804；`.inl` L725-727 = `(int64)FMath::FloorToDouble(A)`）
- 独立验证：`grep "FFloor(float"` 零命中；`DisplayName="Floor"` 仅一处。

→ 区别在 **DisplayName + 返回类型**，不在入参类型推导。真实风险：两个同前缀显示名（"Floor" vs "Floor to Integer64"）节点搜索易误选；要 int64 却误选 int32 的 "Floor" → 在 `static_cast<int32>`（`.inl` L709）截断（截断方向与原假设相反）。
**决策不变**：棋盘 32 格用 int32 "Floor" 节点本就安全、不溢出。
**ADR-0002 / BD-003 更正方向**：(1) 删「float/double 两条路径/按入参推导」；(2) 改截断成因为「误选 int32 'Floor' 节点处理需 int64 的值 → static_cast<int32> 截断」；(3) 保留结论「须按值域显式选对 Floor 节点」。
（注：all-double 入参 + 按 DisplayName 区分 int32/int64 是 post-cutoff LWC 产物，记忆无法预测，探针识别正确。）

### ❌ ADR-0002 ① — CSV TArray「Unsupported Property type」：REFUTED（论证理由错误，决策不变）
**「CSV 含 TArray 列报 `Unsupported Property type`」在 5.7 源码层为假。** CSV→DataTable 导入两阶段：
- 列绑定门控 `GetTablePropertyArray`（`DataTable.cpp` L843-845）：仅 `!IsSupportedTableProperty(ColumnProp)` 才报 "Unsupported Property type"。
- 白名单 `IsSupportedTableProperty`（`DataTableUtils.cpp` L407-429，**L421 显含 `FArrayProperty`**，另含 FSet/FMap）。

→ TArray 列**通过类型门控**、进 ColumnProps、由 ArrayProperty `ImportText` 解析数组字面量 `(a,b,c)`。「含 TArray 列报 Unsupported Property type」不成立。
**但 DA 选型仍正确**：TArray 列虽过类型门控，CSV 单元格数组字面量 `(a,b,c)` 与逗号分隔符冲突、嵌套 struct 转义复杂 —— 这是**实务不可用（格式/可读性/可维护性）**，不是「类型不支持」。
**两层区分**（复核员明确）：本核验是**引擎导入逻辑类型门控层**；若 ADR/GDD 原文附「编辑器实测导入失败截图/日志」，那属**实务可用性层（归 BD-001 编辑器实测）**，与源码结论不冲突但须分别标注。
**ADR-0002 / GDD / story 更正方向**（用户 2026-06-06 选「改理由为实务不便」）：把「CSV 不支持 TArray 列（报 Unsupported Property type）已实证」改为「CSV 数组字面量 `(a,b,c)` 与逗号分隔符冲突 + 嵌套转义复杂的**实务不便**」，或改以 UPrimaryDataAsset 正向收益（编辑器结构化编辑、`TArray<FStruct>` 原生、引用而非内联）立论。BD-001 编辑器验证 AC 仍跑（坐实实务层）。

---

## 待办（ADR 落档 — 起草中，待用户审批后应用）
1. **ADR-0004**：① / ③ / ④ 标 5.7 verified（Last Verified 2026-06-06）+ ① 跨平台前提注 + ④ Mass helper 备注。
2. **ADR-0002**：⑥a 标 verified + 新重载/性能注；⑥b Floor 机制更正；① CSV 理由改「实务不便」。
3. **下游传播**（propagate）：CSV「已实证」措辞散布于 `design/gdd/board-data.md`、BD-001/BD-007 story、epic —— 须 fresh-grep 全集逐处更正（不只改 ADR）。
4. **BD-001 编辑器验证 AC** 保留（实务可用性层，与本源码结论互补）。

> 失败处理铁律（FF-007/DR-007）：refuted 项已按「修订 ADR 论证、决策保持」处理，非「重 Accept 决策」。
