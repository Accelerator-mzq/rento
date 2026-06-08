# ADR-0014: 经济金钱运算整数确定性与 int32 溢出硬防护

## Status

Accepted

> 2026-06-08 由 msc（用户）裁定 **Accept**（AskUserQuestion 选「溢出边界=fatal」+ 继续簇2）。解除 econ-014（整数取整确定性）/ econ-015（int32 溢出硬防护）两个 Gap TR，使 economy-cash 实现 story 可 `/create-stories` 不被 Blocked。

## Date

2026-06-08 — 起草

## Last Verified

2026-06-08 — 起草时 grep `design/gdd/board-data.md` 核实加载期校验现状（铁纪律，非据 claim）：DiceMultiplierTable `[1, DICE_MULT_MAX]` 上界已在 board AC-23j（BLOCKING gate, fatal）+ Tuning L330（建议初值 1,000,000）；**SalaryAmount 上界 ≤2e6 与 RentTable 跨公式包络（OQ-Econ-9 ①②③）尚未在 board-data 实现**（board 仅校验 SalaryAmount>0/==0、RentTable 自身单调）→ 登记为 propagate 债（见 §Migration Plan）。整数运算跨编译配置位级一致性是标准 C++ 整数语义（非浮点，不受 `/fp:fast` 影响），无 post-cutoff 引擎不确定性。

## Decision Makers

msc（用户）+ Technical Director（主笔）· 加载期校验现状由主会话 grep board-data 核实 — 2026-06-08 Accepted

## Summary

经济现金系统（#5 Core，bottleneck）的金钱公式（F-1 发薪 / F-6 赎回 / F-8 卖回 / F-9 NLV 等）须满足两条非功能契约，GDD 把它们标为 Gap 待 ADR：

1. **整数取整确定性（econ-014）**：所有金钱运算**整数纯净、无 float**，非整数结果用整数 `num/den` + 显式取整方向（ceil/floor），跨编译配置（Debug vs Shipping）/两次冷启动**位级一致**（联网回放 OQ-3 前提，AC-32）。本 ADR 焊死：整数比率取整规约、**逐栋 floor 先于求和**（F-9 奇数 BuildingCost 差1 陷阱）、操作数非负前提下截断==floor。
2. **int32 溢出硬防护（econ-015）**：① 运行时 `passed_go` clamp 到 `[0, PASSED_GO_SAFE_MAX=1000]`（**真 clamp，非仅 dev ensure**——Shipping 剥离 ensure）；② 加载期校验 `SalaryAmount ≤ 2,000,000`、`DiceMultiplierTable[i] ≤ DICE_MULT_MAX`（本 ADR 钉 **DICE_MULT_MAX = 1,000,000**），**违反 fatal（拒加载 LoadFailed）**——溢出会致 F-1/F-4 产生负值/垃圾，是崩溃防护非平衡偏好。约束值归经济（5）拥有，校验执行归棋盘（1）加载期（OQ-Econ-9/10）。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core / Scripting（整数算术确定性 + 数据校验） |
| **Knowledge Risk** | LOW — 纯 C++ int32 算术语义，跨编译配置确定（整数运算不受 `/fp:fast`/浮点优化影响）；无 post-cutoff API。`-nullrhi` headless 完全可测 |
| **References Consulted** | `design/gdd/economy-cash.md`（F-1/F-6/F-8/F-9、AC-8/24/27/32、OQ-Econ-9/10）；`design/gdd/board-data.md`（AC-23j DiceMult 上界、AC-23h SalaryAmount 反向校验、L330 Tuning DICE_MULT_MAX 初值建议）；`docs/architecture/ADR-0004`（确定性 RNG，同确定性目标）/`ADR-0002`（棋盘 DA 加载校验宿主） |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | None（整数位级一致性是 C++ 标准语义；AC-32 fixture 实跑 Debug/Shipping 两配置坐实即可，非引擎不确定性）|

> **Note**: 与 ADR-0004（RandRange 浮点中介需 spike 核验）不同，本 ADR 的金钱运算**刻意排除一切 float**——故无浮点平台分歧风险。确定性来源是「整数 num/den + 显式取整」，标准 C++ 跨编译配置位级一致。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0002（棋盘容器）— `SalaryAmount`/`DiceMultiplierTable`/`RentTable` 加载期校验挂在 board DA 加载路径（`Validated` 状态、`LoadFailed` 拒绝）；ADR-0004（确定性 RNG）— 同「联网回放位级一致」目标，本 ADR 是其算术轴对应物（RNG 流确定性 + 金钱算术确定性 = 重放完整前提） |
| **Enables** | 经济现金(#5) 实现 story（解除 econ-014/015 Blocked）；破产9 NLV 移交（F-9/F-10 共用同一整数枚举口径） |
| **Blocks** | 经济现金(#5) 全部含金钱公式的实现 story |
| **Ordering Note** | ADR-0002/0004 均已 Accepted，本 ADR 无前置阻塞。加载期校验的**执行**依赖 board-data（已 Approved+实现 8 story）：DiceMult 上界已落（AC-23j），SalaryAmount 上界 + RentTable 包络须 propagate 到 board（见 §Migration Plan，已实现系统 → 后续 board story 或 producer propagate） |

## Context

### Problem Statement

经济 GDD（economy-cash.md，APPROVED-WITH-FOLLOWUPS）把两条非功能契约标 Gap：

- **econ-014（Determinism）**：F-6/F-8/F-9 整数取整路径须跨编译配置/两次冷启动位级一致、无 float（AC-32）。无 ADR 焊死「金钱运算整数纯净 + 取整规约 + 逐栋 floor」，实现者可能引入 float（如 `BuildingCost * 0.5`）或合并取整（`floor(Σ)` 而非 `Σ floor`），破坏重放一致性与经典忠实数值。
- **econ-015（Overflow）**：`passed_go × SalaryAmount`（F-1）/`dice_total × DiceMultiplierTable[i]`（F-4）有 int32 溢出风险。`ensure` 在 Shipping 编译为空，不能作唯一防线（GDD R1 blocker #2）。无 ADR 焊死运行时 clamp + 加载期边界值，自定义棋盘（地图编辑器26）可设极端值致溢出。

未裁的代价：tr-registry **econ-014 / econ-015 两个 Gap TR** 无 ADR，economy-cash 含金钱公式的实现 story 被 `/create-stories` 标 Blocked；AC-32（确定性门）/ AC-8（溢出 guard）缺架构层背书。

### Current State（grep 核实 2026-06-08）

- economy GDD 已定金钱公式整数规约（F-6 ceil `(MV×num+den−1)/den`、F-8 floor、F-9 逐栋 floor `Σ house_count×floor(BuildingCost/2)`，OQ-Build-2 RESOLVED）、运行时 clamp 散文（F-1 `PASSED_GO_SAFE_MAX=1000`）、加载期边界派发（OQ-Econ-9/10）——但**散文非 ADR**，无架构层契约/registry。
- board-data（1，已 Approved+实现 8 story）加载校验现状：**DiceMultiplierTable `[1, DICE_MULT_MAX]` 上界已落**（AC-23j BLOCKING gate, fatal `DiceMultiplierOutOfRange`；Tuning L330 建议 DICE_MULT_MAX 初值 1,000,000，`12×mult≤INT32_MAX`，留 ~178× 裕度，「最终值并入经济 OQ-Econ-10 裁定」）；SalaryAmount 仅校验 `>0`（Go）/`==0`（非 Go 反向，AC-23h），**无上界**；RentTable 仅校验自身单调 `[i]≤[i+1]`，**无跨公式包络**（OQ-Econ-9 ①②③ 未落）。
- ADR-0004 已定 RNG 流确定性（联网回放前提一轴）；本 ADR 补算术轴。

### Constraints

- **技术**：UE5.7，MVP 单线程同步。金钱量级：单玩家独占全盘约 10⁵ ≪ 2³¹，但乘法路径（F-1/F-4）可溢出。整数除法在 C++ 向零截断；金钱操作数恒 ≥0（Cash≥0 不变式 CR-1、MV≥1、BuildingCost>0），故截断 == floor。
- **确定性**：联网回放（OQ-3）要求金钱算术跨平台/编译配置位级一致——排除 float 即可（整数运算无 `/fp:fast` 分歧）。
- **依赖图无环**：F-9 NLV 不直读建房8 house_count（5→8 反向环），由回合2 聚合 `preaggregated_nlv` 传入（F-10 签名，ADR-0006）。

### Requirements

- **R1**：金钱运算全整数、无 float；比率用 int `num/den` + 显式取整方向。
- **R2**：F-9 逐栋 floor 先于求和（`Σ house_count×floor(BuildingCost/2)`），不得合并取整。
- **R3**：取整路径 Debug/Shipping/两次冷启动位级一致（AC-32）。
- **R4**：F-1 `passed_go` 运行时 clamp `[0,1000]`（非仅 ensure），salary 下界 0。
- **R5**：加载期校验 `SalaryAmount ≤ 2,000,000`、`DiceMultiplierTable[i] ≤ DICE_MULT_MAX=1,000,000`，违反 **fatal（拒加载）**。
- **R6（可测）**：AC-32/AC-8 headless `-nullrhi` 可跑；溢出边界纯 fixture。

## Decision

### 决策① — 金钱运算整数纯净 + 取整规约（econ-014）

**所有金钱运算整数纯净，严禁 float。** 非整数结果一律用整数 `num/den` + 显式取整方向：

- **向上取整（ceil）**：`(x × num + den − 1) / den`（整数除法）。前提 `x,num,den ≥ 0`（金钱恒非负），该式对正操作数恒等于 `ceil(x×num/den)`。用于 F-6 赎回费 `fee = (MortgageValue × fee_num + fee_den − 1) / fee_den`（fee_num/den=1/10）。
- **向下取整（floor）**：直接整数除法 `(x × num) / den`——C++ 整数除法向零截断，金钱操作数 ≥0 故截断 == floor。用于 F-8 卖回 `sellback = (BuildingCost × sell_num) / sell_den`（1/2）。
- **逐栋 floor 先于求和（R2，OQ-Build-2 焊死）**：F-9 NLV 的建筑卖回聚合 = `Σ_t house_count[t] × floor(BuildingCost[t] / 2)`——**每栋单独 floor 再乘再加**，**不得** `floor(Σ house_count × BuildingCost / 2)`（合并取整）。奇数 BuildingCost 二者差1：BuildingCost=75, house=3 → 逐栋 3×floor(75/2)=3×37=**111**；合并 floor(75×3/2)=floor(112.5)=**112**。经典半价须逐栋取整。
- **`is_mortgaged` 短路先于表查找**（F-2 `if is_mortgaged: rent=0`），数组查找一律守 `count≥1`/`clamp` 防越界（这些是确定性整数路径的一部分，非本 ADR 新增但纳入契约）。

**位级一致性（R3）**：因全程整数、无 float，金钱算术跨编译配置（Debug vs Shipping，无 `/fp:fast` 截断分歧）/跨平台/两次冷启动**位级一致**——AC-32 由「无 float」直接保证，是联网回放（OQ-3）算术轴前提（与 ADR-0004 RNG 流轴互补）。

### 决策② — int32 溢出硬防护：运行时 clamp + 加载期 fatal 边界（econ-015）

**运行时 clamp（economy 自持，F-1）**：
`salary = clamp(max(passed_go, 0), 0, PASSED_GO_SAFE_MAX) × SalaryAmount`，**`PASSED_GO_SAFE_MAX = 1000`（锁定常量）**。这是**真运行时 clamp**，非仅 `dev ensure`——`ensure` 在 Shipping 编译为空，不能作唯一防线。`dev ensure(passed_go ≤ 12)` 并存仅用于 Development build 暴露上游（移动4/传送链）bug。下界 0 + gate `passed_go>0` 双重确保 `salary≥0`，永不产生负 `Credit`。

**加载期边界（约束值 economy 拥有、校验执行棋盘1，违反 fatal）**：
- `SalaryAmount ≤ 2,000,000`：clamp 护 passed_go 不护 SalaryAmount——`1000 × 2,000,000 = 2×10⁹ < 2.147×10⁹`（int32 max），安全（~7% 裕度）。超界 → **fatal 拒加载**（否则 F-1 溢出产生负/垃圾薪，坏局）。
- `DiceMultiplierTable[i] ≤ DICE_MULT_MAX`，**本 ADR 钉 DICE_MULT_MAX = 1,000,000**：`12 × 1,000,000 = 1.2×10⁷ ≪ 2.147×10⁹`，留 ~178× 裕度，F-4 `dice_total(≤12) × mult` 不溢出。超界 → **fatal 拒加载**。（值取自 board L330 建议初值 1,000,000；经典倍率 4/10 远内，该上限纯溢出防护、不约束玩法。本 ADR 据 OQ-Econ-10「最终值并入经济裁定」职责钉定。）

**severity = fatal（用户 2026-06-08 裁定）**：溢出边界违反 → board DA 加载校验返 `LoadFailed`、拒进入对局。区别于 OQ-Econ-9 的 RentTable **平衡包络**（建首房降租等，warning 不 fatal）——溢出是崩溃/坏局防护故 fatal，平衡包络是激励反激励故 warning。

### Architecture

```
   设计期 DA (UBoardDataAsset, ADR-0002)
   ├─ SalaryAmount   ──┐
   ├─ DiceMultiplierTable ─┤  加载期校验 (board 1, Validated 状态)
   └─ RentTable      ──┘     ├─ SalaryAmount ≤ 2,000,000      → 超界 LoadFailed (fatal) [propagate 债]
                             ├─ DiceMult[i]  ≤ 1,000,000      → 超界 LoadFailed (fatal) [已落 AC-23j]
                             └─ RentTable 跨公式包络 (OQ-Econ-9) → warning [propagate 债, econ-016]
                                          │ 校验过 → Active
                                          ▼
   运行期 经济5 金钱公式 (全整数, 无 float):
     F-1 salary = clamp(max(passed_go,0), 0, 1000) × SalaryAmount   ← 运行时 clamp
     F-6 fee    = (MV×1 + 10−1)/10          ; cost = MV + fee        ← 整数 ceil
     F-8 sellback = (BuildingCost×1)/2                                ← 整数 floor
     F-9 nlv = Σ MV(未抵押) + Σ_t house_count[t]×floor(BuildingCost[t]/2)  ← 逐栋 floor 先于求和
                                          │ 整数纯净 → 位级一致 (AC-32)
                                          ▼
                            联网回放确定性 (算术轴; RNG 轴 = ADR-0004)
```

### Key Interfaces

```cpp
// 经济金钱运算的整数取整契约 (实现层规约, 非新类型)
//   PASSED_GO_SAFE_MAX = 1000;      // F-1 运行时 clamp 上限 (economy 内部常量)
//   SALARY_AMOUNT_MAX  = 2'000'000; // 加载期校验上界 (economy 拥有, board 执行)
//   DICE_MULT_MAX      = 1'000'000; // 加载期校验上界 (economy 拥有, board 执行 AC-23j)
//
// 取整辅助 (整数纯净, 操作数 ≥0):
//   static int32 CeilDiv(int32 X, int32 Num, int32 Den) { return (X*Num + Den-1)/Den; } // F-6
//   static int32 FloorDiv(int32 X, int32 Num, int32 Den) { return (X*Num)/Den; }        // F-8 (截断==floor, X≥0)
//
// F-1 发薪 (运行时 clamp, 非仅 ensure):
//   int32 salary = FMath::Clamp(FMath::Max(PassedGo, 0), 0, PASSED_GO_SAFE_MAX) * SalaryAmount;
//
// F-9 NLV 逐栋 floor (preaggregated, 由回合2 聚合传入 — ADR-0006/F-10):
//   nlv = Σ MortgageValue(未抵押地) + Σ_t (house_count[t] * FloorDiv(BuildingCost[t], 1, 2));
//   // ✗ 禁: FloorDiv(Σ house_count*BuildingCost, 1, 2)  ← 合并取整, 奇数差1
```

### Implementation Guidelines

1. **零 float**：金钱代码路径禁出现 `float`/`double`/`* 0.5`/`/ 2.0` 等浮点字面量；比率一律 int `num/den`。code-review + 静态扫描（pt-009 authoritative-purity 同套思路可扩）。
2. **运行时 clamp 非 ensure**：F-1 实现先 `FMath::Clamp(passed_go, 0, 1000)` 再乘，dev ensure 并存但不替代 clamp。
3. **逐栋 floor**：F-9 实现遍历建筑逐栋 `floor(BuildingCost/2)` 累加，禁先合并乘加再 floor。AC-27 变体C（BuildingCost=75 → 111 非 112）守此。
4. **加载期边界**：SalaryAmount≤2e6 / DiceMult≤1e6 校验落 board DA `Validated` 批次（DiceMult 已 AC-23j；SalaryAmount 须新增，见 propagate 债），fatal 路径返 `LoadFailed`。
5. **AC-32 实测**：fixture 跑 Debug + Shipping（或 `-fp:fast` 开关对照）两配置，断言 F-6/F-8/F-9 取整结果位级相同（MV=75→fee=8→cost=83 恒；BuildingCost=75→sellback=37 恒）。

## Alternatives Considered

### Alternative 1: 整数 num/den + 显式取整 + 运行时 clamp + 加载期 fatal 边界 — 【选定】

- **描述**：见 §Decision。全整数、逐栋 floor、passed_go 运行时 clamp、溢出边界加载期 fatal。
- **Pros**：位级确定（无 float）；溢出在加载期（数据进入前）或运行时 clamp 双重堵死；经典忠实数值（逐栋 floor）；headless 可测。
- **采纳理由**：唯一同时满足确定性（R1-R3）+ 溢出防护（R4-R5）的方案。

### Alternative 2: float 运算 + 末尾取整（`round/floor`）

- **描述**：`fee = ceil(MV * 0.1)`、`sellback = floor(BuildingCost * 0.5)` 用浮点中间值。
- **Cons**：浮点跨编译配置（`/fp:fast`）/跨平台可能位级分歧 → 联网回放 desync；`0.1` 无法精确表示二进制，边界值（如 MV=某些值）ceil 结果可能差1。
- **Rejection Reason**：直接违反 econ-014 位级一致要求（AC-32）。浮点确定性需 `/fp:strict` 全局约束 + 平台核验，成本远高于「全整数」的零成本确定。

### Alternative 3: 仅 dev `ensure` 防溢出（无运行时 clamp / 无加载期边界）

- **描述**：靠 `ensure(passed_go ≤ 12)` + `ensure(SalaryAmount 合理)` 暴露异常，不做运行时 clamp / 加载期 fatal。
- **Cons**：`ensure` 在 Shipping 编译为空——Shipping 下溢出无任何防线（GDD R1 blocker #2）；自定义棋盘极端值在 Shipping 直接坏局。
- **Rejection Reason**：Shipping 无防护 = 不可接受。ensure 仅作 Dev 期诊断补充，非防线。

### Alternative 4: 溢出边界 warning（不拒加载）

- **描述**：SalaryAmount/DiceMult 超界仅 log warning，照常进入对局。
- **Cons**：溢出数据进入对局 → F-1/F-4 必溢出产生负/垃圾值 → 坏局，需额外运行时 clamp 兜底所有乘法（增成本）。
- **Rejection Reason**：用户 2026-06-08 裁定溢出边界 fatal。溢出是崩溃/坏局防护（区别于平衡包络 warning），加载期拒绝比运行时处处兜底更干净。

## Consequences

### Positive

- econ-014/015 两 Gap TR 解除，economy-cash 含金钱公式 story 可 `/create-stories` 不被 Blocked。
- AC-32（确定性门）/AC-8（溢出 guard）有架构层背书；金钱算术联网回放安全（与 ADR-0004 RNG 轴合成完整重放前提）。
- 溢出在加载期（fatal）+ 运行时（clamp）双重堵死，Shipping 亦受保护。
- DICE_MULT_MAX 最终值钉定（1,000,000），消解 board L330「最终值并入经济裁定」悬空，board AC-23j 可锁实。

### Negative

- 逐栋 floor 与合并取整差1 是隐性陷阱，实现者易错（AC-27 变体C 守门，code-review 须查）。
- 加载期 fatal 边界依赖 board-data（已实现系统）新增校验（SalaryAmount 上界）——propagate 债（见 §Migration Plan）。

### Neutral

- RentTable 跨公式平衡包络（OQ-Econ-9 ①②③）是 warning 非本 ADR fatal 范畴，归 econ-016（Partial under 0002），propagate 债另记。
- PASSED_GO_SAFE_MAX/SALARY_AMOUNT_MAX/DICE_MULT_MAX 是 economy 拥有的内部/约束常量，不入 architecture registry（单系统拥有 + 加载校验跨系统但值由 economy 定）。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| 实现者用 float（`* 0.5`）破坏位级一致 | Medium | High（联网 desync） | 零-float 静态扫描（authoritative-purity 扩展）+ AC-32 Debug/Shipping 对照 fixture + code-review |
| 逐栋 floor 误写合并取整（奇数 BuildingCost 差1） | Medium | Medium（数值穿帮） | AC-27 变体C（75→111≠112）BLOCKING gate；F-9 实现 review 查求和顺序 |
| SalaryAmount 上界未 propagate 到 board → 自定义棋盘 Shipping 溢出 | Medium（board 已实现，需补 story） | High | §Migration Plan 登记 propagate 债；MVP 经典 SalaryAmount=200 不触发；编辑器(26) 是 Alpha |
| SalaryAmount=2e6 时 passed_go=1000 仅 ~7% 裕度，未来调高 SalaryAmount 上界会溢出 | Low | Medium | 上界 2e6 与 clamp 1000 联动锁定；若提高 SalaryAmount 上界须同步降 PASSED_GO_SAFE_MAX，本 ADR 记此联动 |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | N/A | 整数算术 ~0ms；加载期校验一次性 O(N) | 16.6ms（无占用） |
| Memory | N/A | 0（无新容器，仅常量 + 算术） | N/A |
| Load Time | N/A | 加载期边界校验并入 board `Validated` 批次，~0ms | < 100ms（board 预算内） |
| Network | N/A（MVP 不联网；整数确定性是 Full Vision 回放前提） | N/A | N/A |

## Migration Plan

无既有实现迁移（economy-cash 尚未拆 story）。落地步骤：

1. economy 实现金钱公式时遵本 ADR 整数规约（CeilDiv/FloorDiv 辅助、逐栋 floor、F-1 运行时 clamp）。
2. 定义常量 `PASSED_GO_SAFE_MAX=1000`/`SALARY_AMOUNT_MAX=2'000'000`/`DICE_MULT_MAX=1'000'000`（economy 拥有）。
3. AC-32/AC-8/AC-27 变体C 落 `tests/unit/economy-cash/`，AC-32 跑 Debug+Shipping 对照。
4. **🔴 propagate 债（board-data 已 Approved+实现 8 story）**：
   - **SalaryAmount ≤ 2,000,000 加载期 fatal 校验**：board 现仅校验 SalaryAmount>0/==0（AC-23h），**无上界** → 须 `/propagate-design-change` 或后续 board story 新增 `SalaryAmountOverflow` fatal 校验 + AC。**未落地前登记债，不谎报已闭。**
   - **DICE_MULT_MAX 最终值 1,000,000**：board AC-23j 已用符号 `[1, DICE_MULT_MAX]`、L330 建议初值 1,000,000 → 本 ADR 钉定后 board 只需确认该常量值（已对齐，无新校验逻辑，低成本）。
   - **RentTable 跨公式包络（OQ-Econ-9 ①②③，warning）**：board 现仅校验自身单调 → propagate 债（econ-016 Partial，warning 非 fatal，优先级低于溢出）。

**Rollback plan**：整数规约是实现纪律，无数据迁移；常量值可调（须同步联动 clamp/上界）。

## Validation Criteria

- [ ] F-6 MV=75 → fee=ceil(7.5)=8 → cost=83（整数 ceil，AC-21/22）。
- [ ] F-8 BuildingCost=75 → sellback=floor(37.5)=37（整数 floor，AC-24）。
- [ ] F-9 BuildingCost=75/house=3 → Σ=3×37=111（逐栋 floor，**≠112**，AC-27 变体C）。
- [ ] AC-32：F-6/F-8/F-9 取整结果 Debug vs Shipping 位级相同。
- [ ] F-1 passed_go=10⁷ → clamp 1000 → 不溢出/不负（AC-8）；Shipping 下 clamp 仍生效（非 ensure）。
- [ ] board 加载 SalaryAmount=3,000,000 → LoadFailed（fatal）[propagate 落地后]。
- [ ] board 加载 DiceMult=[4, 2,000,000,000] → LoadFailed（AC-23j，已落）。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/economy-cash.md` | 经济现金(#5) | **econ-014 / AC-32**：F-6/F-8/F-9 取整跨编译配置位级一致、无 float | 决策①：整数 num/den + 显式取整 + 逐栋 floor + 零 float → 位级一致 |
| `design/gdd/economy-cash.md` | 经济现金(#5) | **econ-015 / F-1 / AC-8**：passed_go 运行时 clamp[0,1000] 防 int32 溢出 | 决策②：真运行时 clamp（非仅 ensure），PASSED_GO_SAFE_MAX=1000 |
| `design/gdd/economy-cash.md` | 经济现金(#5) | **econ-015 / OQ-Econ-10**：SalaryAmount/DiceMult 加载期上界 | 决策②：SalaryAmount≤2e6、DICE_MULT_MAX=1,000,000，fatal 拒加载；执行派 board（propagate） |
| `design/gdd/economy-cash.md` | 经济现金(#5) | **F-9 / AC-27 变体C / OQ-Build-2**：逐栋 floor 先于求和（奇数差1） | 决策①：`Σ house_count×floor(BuildingCost/2)` 焊死，禁合并取整 |
| `design/gdd/board-data.md` | 棋盘数据(#1) | **AC-23j / L330**：DICE_MULT_MAX 最终值（「并入经济裁定」） | 决策②：钉 DICE_MULT_MAX=1,000,000（与 board 建议初值对齐） |
| `design/gdd/economy-cash.md` | 经济现金(#5) | **OQ-Econ-9（econ-016, warning）**：RentTable 跨公式包络 | 部分：本 ADR 确认归 warning（非 fatal）、约束定义归 economy，校验执行 propagate board（debt，未闭） |

## Related

- **依赖**：ADR-0002（棋盘 DA 加载校验宿主）、ADR-0004（确定性 RNG，算术轴互补）
- **被使能**：经济现金(#5) 实现 story；破产9 NLV 移交（F-9/F-10 共用整数枚举）
- **GDD**：`design/gdd/economy-cash.md`（F-1/F-6/F-8/F-9、AC-8/24/27/32、OQ-Econ-9/10）；`design/gdd/board-data.md`（AC-23j/AC-23h/L330）
- **propagate 债**：SalaryAmount≤2e6 fatal 校验 + RentTable 包络 warning → board-data（已实现，须后续 story / `/propagate-design-change`）
- **总纲**：`docs/architecture/architecture.md` §8 + `tr-registry.yaml`（econ-014/015/016）
