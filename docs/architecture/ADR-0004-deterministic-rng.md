# ADR-0004: 确定性 RNG 服务（Deterministic RNG Service）

## Status

Accepted

> 2026-06-06 由 msc（用户）裁定 DECISION FORK，确认 = **单一权威 `FRandomStream`（dice 拥有）+ 各系统独立非权威流**
> （AI 决策扰动走权威流须重放，juice 走独立流）；C++ 强封装强度联动 ADR-0007。
> 依赖 ADR-0001（DiceService 宿主，已 2026-06-06 Accepted）——宿主形态决定 `FRandomStream` 实例挂载点。

## Date

2026-06-06 — 初稿

## Last Verified

2026-06-06 — 对照 dice.md（APPROVED R3）/ ai-opponent.md（CR-5④）/ vfx-feedback.md（CR-9）/
architecture.md §8 ADR-0004 条目 + API 边界 1 逐条核实。

2026-06-06 — **Sprint0 引擎验证 spike**（`docs/architecture/sprint0-engine-verification-2026-06-06.md`，6 探针 + 对抗复核，全 grounded/high-confidence）对 UE 5.7 源码核验：**① `RandRange` 仍走 `Min+TruncToInt(FRand()*Range)` 浮点中介 ✅ CONFIRMED**（`RandomStream.h` L186-202；跨平台位级重放依赖单精度 float 一致——同平台/同构建绝对安全，MVP 单机无影响）；**③ `FRandomStream()` 默认构造种子恒 0 ✅ CONFIRMED**（L30-33，作 lazy-init 兜底安全前提成立）；**④ 5.7 无新官方确定性/网络 RNG 子系统 ✅ CONFIRMED**（全树零命中；Mass `UE::RandomSequence` 仅无状态 hash helper、非权威服务，登记供 Full Vision 联网阶段参考）。手搓单权威 `FRandomStream` 方案 **5.7 verified**。（② Seed 序列化属 OQ-2/ADR-0005，未在本次 spike 范围。）

## Decision Makers

msc（用户）+ Technical Director（主笔）— 2026-06-06 Accepted；
下游消费方 owner：dice(3) / ai(10) / vfx(19) / audio(22) / tile-events(7) / save-load(21)。

## Summary

《骰子大亨》全部 gameplay 逻辑随机性（骰点、牌堆洗牌、AI 决策扰动）必须可由「种子 + 确定性抽取序列」
完整重放，以服务 Testability（固定 seed→零方差回归门）与 Full Vision 联网状态同步根基；本 ADR 裁定
**单一权威 `FRandomStream` 服务（骰子3 拥有，宿主由 ADR-0001 裁）+ 各表现/AI 系统自建独立非权威流**
的双层 RNG 架构，并钉死退化契约（`Min==Max` 早退不推进 Seed）、跨平台浮点风险处置（`/fp:precise` +
降 Full Vision OQ-3）、流隔离硬门。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7（Blueprint 为主 + C++ 框架） |
| **Domain** | Core（确定性基础设施，跨 gameplay/AI/呈现） |
| **Knowledge Risk** | 🟡 MEDIUM — `FRandomStream` 本身 pre-5.3 稳定（LOW），但 UE 5.4–5.7 是否新增官方确定性/网络 RNG 子系统、`RandHelper` 是否仍走 `FRand()` 浮点中介，**超出模型 ~5.3 知识截止，须实现期查 5.7 源码/Release Notes 核实** |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`（5.4–5.7 知识缺口）；`current-best-practices.md` / `deprecated-apis.md`（无 RNG 破坏性条目）；`modules/audio.md`（确认引擎示例用 `FMath::RandRange` 全局随机——正是本 ADR 禁止旁路的反例） |
| **Post-Cutoff APIs Used** | 无新增 post-cutoff API。核心类型 `FRandomStream`/`FRandomStream::RandRange(int,int)`/`FRandomStream::FRand()` 均 pre-5.3 稳定。**风险点是「5.7 是否改了 `RandRange` 内部实现或新增更优的官方确定性 RNG 子系统」——这是 MEDIUM 的来源，非现用 API 不存在** |
| **Verification Required** | 实现前须对 UE 5.7 源码/Release Notes 确认：① `FRandomStream::RandRange(int,int)` 是否仍走 `RandHelper: Min + TruncToInt(FRand()*Range)` 浮点中介（影响跨平台位级一致性结论）；② `FRandomStream` 经 `UPROPERTY` 序列化是否持久化当前滚动 `Seed`（影响 OQ-2 续算）；③ 默认构造种子是否仍恒 `0`（影响 lazy-init 兜底安全）；④ 是否新增官方确定性/网络 RNG 子系统（若有，Full Vision 联网重放可能优于手搓 `FRandomStream`，须重评） |

> **Note**: Knowledge Risk = MEDIUM → 若项目升级引擎版本，本 ADR 须重新验证 ①–④。
> 若 5.7 源码确认 `RandHelper` 已改为纯整数路径，则 F-4 跨平台 off-by-one 警告可放宽——届时标 Superseded 重写。
> **2026-06-06 spike 结论**：①③④ 已对 5.7 源码 verified（见 Last Verified）。① `RandHelper` **仍走浮点中介**（F-4 跨平台 off-by-one 警告维持；同平台/同构建重放安全）——故不放宽、不 Superseded。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | **ADR-0001（UObject 宿主与生命周期边界）** — 必须先 Accepted。ADR-0001 裁定 DiceService 挂 `UWorldSubsystem` / `UGameInstanceSubsystem` / Component；本 ADR 的 `FRandomStream` 实例生命周期、`SetSeed` 注入时机、PIE 隔离全部继承自该裁定。**硬约束（dice OQ-1）：DiceService 与 player-turn 状态容器必须落同一生命周期层**，否则 Full Vision 序列化 `Seed` 存活域与消费方错位。 |
| **Enables** | ADR-0005（存档序列化契约——牌堆 Fisher-Yates 种子序列化需本 ADR 钉死的种子语义）；ADR-0006（GameStateSnapshot——AI 决策随机经本服务）；ADR-0010（音频架构——音频随机流随本 ADR 独立非权威约定） |
| **Blocks** | 移动(4) / AI(10) 实现开工（dice IG-3 / OQ-1 自陈门控）；事件格(7) 洗牌实现；任何 [Logic] 确定性回归门测试落地 |
| **Ordering Note** | 与 ADR-0002/0003/0005 同属 Foundation 层，ADR-0001 Accepted 后可由 engine-programmer 并行起草（互不触同文件）。本 ADR 不改任何 DiceService 函数签名（签名已在 architecture.md API 边界 1 冻结），只裁定语义契约 + 流隔离纪律 + 宿主无关的实现守则。 |

## Context

### Problem Statement

《骰子大亨》的核心循环以掷骰为起点（支柱③运气×策略），随机性渗透到：骰点（移动）、牌堆抽取（事件格）、
AI 决策扰动（AI 棋力差异）、juice 节奏抖动（VFX/Audio 表现）。这些随机源若各自调用引擎全局
`FMath::Rand` / BP `Random Integer in Range`，会产生两个不可逆的技术债：

1. **不可重放** — 全局 RNG 不可种子化，一局无法由固定种子复现 → [Logic] 确定性回归门无法成立
   （本项目反复强调的失效模式：统计断言 flaky，唯有「固定种子→精确序列断言」才是可靠 gate），
   且 Full Vision 联网状态同步失去根基（服务器/客户端无法对齐随机序列）。
2. **流污染** — 若表现层 juice 随机（粒子抖动）与 gameplay 权威随机（骰点）共享同一流，
   表现层抽取会插入权威调用序列、改变后续骰点 → 联网重放污染（同种子不同结果）。

此决策**必须现在做**：dice.md 自陈 OQ-1/IG-3 为「移动(4)/AI(10) 开工硬前提」；ai-opponent.md
AC-44/47/61a/61b、vfx-feedback.md CR-9/EC-13 均把「权威流 vs 独立流隔离」登记为继承义务，
等待本 ADR 收口。不决定 = 移动/AI/事件格/VFX/Audio 全部阻塞实现。

### Current State

`docs/architecture/` 下尚无任何 ADR（含本 ADR 依赖的 ADR-0001 亦为 Proposed/未落库）。
DiceService 的接口签名已在 architecture.md「API 边界 1」冻结（`RollDice`/`RandomRange`/
`RandomFloat01`/`SetSeed`/`GetCurrentSeed`/`OnDiceRolled`），但**语义契约、流隔离纪律、退化行为、
跨平台处置、宿主无关实现守则尚未有规范性 ADR 背书**。各 GDD 分散持有这些约束（dice CR-2/3 +
F-2/F-4、ai CR-5④、vfx CR-9），本 ADR 将其聚合为单一可执行决策。

### Constraints

- **引擎**：`FRandomStream` 原生**非 BlueprintCallable**，须 C++ 封装为 `UFUNCTION(BlueprintCallable)`
  函数库/服务暴露（dice UE5.7 标注）。本项目 Blueprint 为主 → 封装层是 BP 调用的唯一入口。
- **知识缺口**：UE 5.4–5.7 RNG 子系统变更超出模型知识截止——结论须实现期对源码核实（见 Verification Required）。
- **宿主未定**：`FRandomStream` 实例挂哪个 UObject 由 ADR-0001 裁定；本 ADR 必须**宿主无关**
  （不预设 Subsystem 类型），只约束「实例唯一 + 与 player-turn 同生命周期层」。
- **兼容**：不得改动 architecture.md API 边界 1 已冻结的函数签名（下游 GDD 已逐字对齐）。
- **强度坦诚**：BP-only 下「禁旁路引擎全局 RNG」只能 code-review 软约束；C++ 强封装才可加静态符号
  扫描升 [Logic] 硬门——此强度联动 ADR-0007（BP-vs-C++ 边界）的 AI 决策核心裁定。

### Requirements

- **R1（确定性）**：给定相同 `InitialSeed` + 相同有序抽取调用序列，输出序列在同一平台/同一编译配置
  的任何执行完全相同（dice 性质 D）。
- **R2（单一权威源）**：所有需可重放的 gameplay 随机（骰点/牌堆/AI 决策）走唯一权威 `FRandomStream`，
  禁旁路引擎全局 RNG（dice CR-2）。
- **R3（流隔离）**：表现层 juice 随机（VFX/Audio/HUD）与 AI 内部扰动**不得复用骰子3 权威流**——
  须各自持独立非权威 `FRandomStream`（vfx CR-9 / ai CR-5④ / hud OQ-HUD-3）。
- **R4（退化安全）**：`Min==Max` 早退返回 Min 不调流（Seed 不推进）；`Min>Max` ensure 不自动交换；
  默认构造种子恒 0 禁落 lazy-init（dice F-2 / Edge Cases）。
- **R5（可序列化）**：当前 `Seed` 经显式 `GetCurrentSeed()`/`SetSeed()` 存取（勿靠 struct 反射）；
  MVP 不序列化 Seed，Full Vision 重放序列化（dice OQ-2，与 ADR-0005 协同）。
- **R6（跨平台）**：跨平台/跨编译配置位级一致性**不被 MVP 保证**，登记降 Full Vision OQ-3；
  正解（服务器单一权威 + RPC 下发结果 / 纯整数 LCG）在 ADR 标注、不在 MVP 实现。
- **R7（牌堆洗牌）**：事件格(7)两牌堆开局 Fisher-Yates 洗牌经本服务取随机（tile-events CR-3），
  种子序列化与 ADR-0005 协同。

## Decision

裁定**双层 RNG 架构**：一条**权威确定性流**（骰子3 拥有，承载全部需重放的 gameplay 随机）+
**N 条独立非权威流**（VFX/Audio/HUD/AI 内部各自持有，禁复用权威流）。封装层钉死退化契约、
跨平台处置、宿主无关实现守则。

### 选项逐个列代价（真分叉，2-3 选项摆清）

#### 选项 A —— 单一权威流 + 各系统独立非权威流（推荐）

- **做法**：DiceService 持唯一权威 `FRandomStream`，承载骰点/牌堆/AI 决策扰动（凡需重放者）；
  VFX(19)/Audio(22)/HUD(16) 的纯表现 juice 抖动各自 `new` 一个独立非权威 `FRandomStream`
  （自种子，不参与重放）；AI(10) 的决策扰动走**权威流**（须重放），但 AI 不持流、经 DiceService
  `RandomRange`/`RandomFloat01` 取。
- **Pros**：①重放正确性——权威流调用序列只含 gameplay 抽取，不被表现层污染（满足 R1/R2/R3）；
  ②表现层解耦——VFX 改 juice 抖动逻辑不影响骰点（满足 R3）；③联网根基——Full Vision 服务器
  单一权威流即可对齐（绕开表现层）；④与全部已 Approved 下游 GDD 一致（ai CR-5④/vfx CR-9/hud OQ-HUD-3
  本就这么写）。
- **Cons**：①「哪些随机算 gameplay（走权威流）/ 哪些算表现（走独立流）」须明确划线，否则误接污染
  （缓解：本 ADR 给出划线表 + AC-61b spy 硬门）；②每个独立流要各自正确初始化非确定种子
  （缓解：禁 `FRandomStream()` 默构 0，统一 lazy-init 用 `FPlatformTime::Cycles()`）。
- **Estimated Effort**：基准（下游 GDD 已按此设计，封装实现 + 划线表 + spy 测试）。
- **采纳**：✅ 推荐。

#### 选项 B —— 全局单一流（所有随机含 juice 都走权威流）

- **做法**：取消独立流，VFX/Audio/HUD juice 抖动也调 DiceService 权威流。
- **Pros**：①只有一个流，实现/初始化最简；②无「划线」心智负担。
- **Cons**：①**致命——破坏重放**：表现层抽取插入权威序列，juice 节奏的任何改动（粒子数/抖动调参/
  reduce_motion 开关）都会改变后续骰点 → 同种子不同结果，联网 desync 根源；②违反 R3 与全部
  已 Approved 下游 GDD（ai CR-5④/vfx CR-9 明文禁此）；③表现层与 gameplay 强耦合，违架构原则2
  「RNG 流隔离」。
- **Estimated Effort**：略低于 A（省独立流初始化），但债务不可逆。
- **Rejection Reason**：直接砸穿确定性优先（架构原则2）与联网预留，是本项目反复登记要避免的失效模式。

#### 选项 C —— 引擎官方确定性/网络 RNG 子系统（若 5.7 存在）

- **做法**：放弃手搓 `FRandomStream`，改用 UE 5.7 可能新增的官方确定性/网络 RNG 子系统。
- **Pros**：①若存在且经验证，可能原生解决跨平台位级一致性（R6）、原生联网复制——省去自实现纯整数 LCG。
- **Cons**：①**存在性未知**——超出模型知识截止，dice.md 知识盲区④明确「须实现期查证是否新增」；
  ②若存在，其 API/语义须重新核验，可能引入 post-cutoff HIGH 风险；③MVP 单机不需要其联网能力，
  为未必存在的特性赌架构是过度工程。
- **Estimated Effort**：未知（取决于是否存在）。
- **Rejection Reason**：MVP 不赌未验证的引擎特性。**但登记为 Verification Required ④ + OQ-3 重评点**——
  若 Full Vision 联网阶段查实 5.7 有成熟官方方案，届时标本 ADR Superseded 重写（Reversibility 已保留：
  权威流封装在 DiceService 单一边界后，替换实现不动调用方）。

### 推荐项与理由

**推荐选项 A（单一权威流 + 各系统独立非权威流）。**

理由：① **Correctness/Testability** ——只有权威流序列纯净（不含表现抽取）才能让「固定种子→精确序列断言」
的 [Logic] 回归门成立（dice AC-8/ai AC-61a），这是本项目反复用代价换来的核心纪律；② **Reversibility** ——
权威流封在 DiceService 单一 C++ 边界后，未来若 5.7 官方 RNG 子系统查实可用（选项 C），替换实现不动任何
调用方；③ **一致性** ——全部已 Approved 下游 GDD（ai/vfx/hud/tile-events）本就按 A 设计，选 A = 零 propagate 冲突。
选项 B 的「最简」是假象（不可逆债务），选项 C 是未验证赌注——A 是唯一既满足 R1–R7 又不引入未核实依赖的方案。

### Architecture

```
                    ┌─────────────────────────────────────────────┐
                    │  DiceService (骰子3, 宿主由 ADR-0001 裁)      │
                    │  ┌───────────────────────────────────────┐  │
   gameplay 随机 ──▶│  │  权威 FRandomStream (唯一, 可种子化)    │  │
   (骰点/牌堆/AI)   │  │  SetSeed / RollDice / RandomRange /    │  │
                    │  │  RandomFloat01 / GetCurrentSeed        │  │
                    │  └───────────────────────────────────────┘  │
                    │   ▲ AI(10) 经 RandomRange/Float01 取(须重放) │
                    │   ▲ tile-events(7) Fisher-Yates 洗牌取(重放) │
                    └───┼─────────────────────────────────────────┘
                        │  ✗ 禁复用 (vfx CR-9 / ai CR-5④ / EC-13)
   ┌────────────────────┴───────────────────────────────────────┐
   │  独立非权威 FRandomStream (各 owner 自持, 自种子, 不重放)     │
   │  VFX(19) juice 抖动流 │ Audio(22) 变奏流 │ HUD(16) juice 流  │
   └──────────────────────────────────────────────────────────────┘
```

### Key Interfaces

> 函数签名已在 architecture.md「API 边界 1」冻结（逐字对齐 dice.md），本 ADR **不改签名**，
> 只规范语义契约。下为契约伪码（非新签名）。

```cpp
// ── DiceService（骰子3）：唯一权威流，C++ 封装暴露 BP ──
USTRUCT(BlueprintType)
struct FDiceRollResult { int32 Die1; int32 Die2; int32 Total; bool bIsDouble; };

UFUNCTION(BlueprintCallable) FDiceRollResult RollDice();
//   执行序铁律(dice F-4 前提②): ① 流抽 Die1,Die2 → ② 本地固化 result
//   → ③ 广播 OnDiceRolled(result) → ④ 返回同一固化 result(非广播后重读流)
//   保证: 同步返回值 == 事件 payload; Total==Die1+Die2; bIsDouble==(Die1==Die2)

UFUNCTION(BlueprintCallable) int32 RandomRange(int32 Min, int32 Max);
//   Min==Max → early-return Min, 不调流(Seed 不推进, dice F-2/F-4④)
//   Min>Max  → ensure(Min<=Max)+UE_LOG(Warning), 返回 Min, 绝不自动交换
//   约束: Range = Max-Min+1 < 2^24 (RandRange 走 FRand() 浮点中介, 跨度超 2^24 精度崩)
//         封装层 ensure((int64)Max-(int64)Min+1 < (1<<24))

UFUNCTION(BlueprintCallable) float RandomFloat01();  // [0.0,1.0), 1.0 严格排除

UFUNCTION(BlueprintCallable) void  SetSeed(int32 InitialSeed);   // 0 是合法种子(非哨兵)
UFUNCTION(BlueprintCallable) int32 GetCurrentSeed() const;       // 显式存取,勿靠 struct 反射(OQ-2)
UPROPERTY(BlueprintAssignable) FOnDiceRolled OnDiceRolled;       // 订阅者禁回调内二次抽取(单线程重入)

// ── 独立非权威流（VFX/Audio/HUD 各自私有,不暴露给权威路径）──
// 各 owner 内部持有 private FRandomStream JuiceStream;
// 初始化禁用默认构造(种子恒0), 须 JuiceStream.Initialize(FPlatformTime::Cycles());
// juice 抖动只调此私有流, 严禁经 DiceService 权威 API 取随机
```

### Implementation Guidelines

1. **划线表（哪条随机走哪条流）** —— 实现者据此判定，误接由 AC-61b spy 抓出：

   | 随机用途 | 走哪条流 | 须重放 | 依据 |
   |---|---|---|---|
   | 骰点 `RollDice` | 权威流 | 是 | dice CR-1 |
   | 牌堆 Fisher-Yates 洗牌 | 权威流 | 是 | tile-events CR-3 |
   | AI 决策扰动（F-3 Epsilon、tiebreak 备选） | 权威流（经 `RandomRange`/`Float01`） | 是 | ai CR-5④/AC-42 |
   | VFX juice 节奏抖动 / 火花方向 | **独立非权威流** | 否 | vfx CR-9/V-Own-02 |
   | Audio 变奏（音高/起始偏移） | **独立非权威流** | 否 | audio（随本 ADR 独立流约定） |
   | HUD juice 随机 | **独立非权威流** | 否 | hud OQ-HUD-3 |

2. **权威流唯一性** —— 全项目恰一个权威 `FRandomStream`，挂 DiceService（宿主 ADR-0001）。
   `UBlueprintFunctionLibrary` 不可作宿主（无状态静态、不能持实例流，dice OQ-1 已删此候选）。

3. **退化与边界**（封装层硬门，对应 dice F-2/F-4）：
   - `Min==Max` early-return 不推进 Seed（否则破坏已固化 fixture 序列）。
   - `Min>Max` ensure + 返回 Min，**绝不 swap**（swap 静默掩盖参数反向 bug）。
   - `RandomFloat01` 半开 `[0,1)`，下游 `floor(f*N)` 仅 MVP 小 N 安全；`N≥2^24` 越界登记 OQ-4。
   - lazy-init 兜底种子用 `FPlatformTime::Cycles()`，**禁** `FRandomStream()` 默构 0
     （否则忘 SetSeed 的对局掷出相同确定序列，砸穿「骰子不可预测」信任底线，且范围断言查不出）。
     Full Vision 重放模式下未 SetSeed = fail-fast，不走 lazy-init。

4. **独立流初始化** —— 每个非权威流亦禁默构 0，用 `Initialize(FPlatformTime::Cycles())` 或各自非确定源。

5. **流隔离防护** —— 订阅 `OnDiceRolled` 的回调内**禁**同步调任何抽取 API（单线程重入会按订阅者
   注册顺序插入权威序列，破坏 dice F-4 前提②）；表现层订阅者须纯呈现/只读。

6. **强度联动 ADR-0007** —— 若 ADR-0007 裁 AI 决策核心 + RNG 封装为 C++，则可加「禁用引擎全局
   RNG 符号（`FMath::Rand`/`UKismetMathLibrary::RandomInteger`/BP `Random Integer in Range`）」
   静态扫描，把 dice AC-20 / ai AC-44 从 [Advisory·code-review] 升 [Logic] PR gate；BP-only 则保软约束。
   **本 ADR 推荐 C++ 封装**（见 DECISION FORK F2），但最终强度裁定归 ADR-0007。

7. **Seed 序列化（与 ADR-0005 协同）** —— MVP 不序列化 Seed（当前骰由 player-turn 序列化完整
   `FDiceRollResult` 保护，读档 `SetSeed(非确定值)` 不重掷）；牌堆数组顺序由 tile-events 序列化
   （model B 无指针，序列化数组即重现牌序）；Full Vision 重放经 `GetCurrentSeed()` 显式存当前 Seed。

8. **实现期源码核验** —— 见 Engine Compatibility「Verification Required」①–④，开工首步执行。

## Alternatives Considered

见上「选项逐个列代价」：选项 B（全局单一流，破坏重放，已拒）/ 选项 C（引擎官方 RNG 子系统，
存在性未知，MVP 不赌、登记 Full Vision 重评）。此处不重复。

## Consequences

### Positive

- 权威流序列纯净 → [Logic] 确定性回归门（dice AC-8 / ai AC-61a）可成立，零方差可重放。
- 表现层 juice 改动不影响 gameplay 随机 → VFX/Audio 解耦，satisfies 架构原则2 流隔离。
- Full Vision 联网有清晰升级路径（服务器单一权威流 + RPC 下发结果，绕开客户端浮点）。
- 与全部已 Approved 下游 GDD 一致 → 零 propagate 冲突。
- DiceService 单一 C++ 边界封装 → 未来替换实现（选项 C）不动调用方，Reversibility 高。

### Negative

- 须维护「划线表」纪律，误把表现随机接权威流会污染重放（缓解：AC-61b spy 硬门 + code-review）。
- 多条独立流各须正确非确定初始化（缓解：统一禁默构 0、统一 `FPlatformTime::Cycles()`）。
- BP-only 下「禁旁路」仅软约束，真正硬门须 ADR-0007 裁 C++（强度坦诚，非本 ADR 单独可关）。
- 跨平台位级一致性 MVP 不保证（接受：单机不触发；Full Vision OQ-3 处置）。

### Neutral

- `FRandomStream` 函数签名不变（已冻结）——本 ADR 只加语义契约，不影响已对齐的下游 GDD 措辞。
- 宿主形态延后到 ADR-0001——本 ADR 宿主无关，ADR-0001 裁定后实现挂载点自然落定。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| 5.7 改了 `RandRange` 内部实现（不再走 `FRand()` 浮点） | 中 | 低 | Verification Required ① 实现前查源码；若改纯整数路径，跨平台风险放宽（利好），标 Superseded 重写 F-4 警告 |
| 实现者误把 juice 随机接权威流 | 中 | 高（污染重放） | 划线表 + AC-61b 权威流 spy 硬门（spy 断言消费方 ∈ {dice,AI}）+ EC-13 code-review |
| BP 节点旁路引擎全局 RNG（不可静态扫描） | 中 | 中 | ADR-0007 裁 C++ 封装可升静态符号扫描硬门；BP-only 保 code-review（dice AC-20/ai AC-44） |
| lazy-init 落默构种子 0（每局同序列） | 低 | 高（砸穿公平信任，范围断言查不出） | 封装层禁默构 0、强制 `FPlatformTime::Cycles()`；dice AC-17b 断言 `InitialSeed != 0` |
| Full Vision 跨平台/跨编译配置 off-by-one desync | 中（仅 Full Vision） | 高 | 降 OQ-3；正解服务器单一权威 + RPC 下发 `FDiceRollResult`，或纯整数 LCG（已知良好参数 + chi-square 验证） |
| 5.7 新官方 RNG 子系统使本方案次优 | 低 | 中 | Verification Required ④；Reversibility 已保留（DiceService 单一边界），届时标 Superseded |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | n/a | 可忽略（`FRandomStream` LCG 每次抽取 O(1) 整数运算；回合制单帧抽取次数极少） | 16.6 ms / 帧 |
| Memory | n/a | 可忽略（每流 2×int32；权威流 1 个 + 独立流个位数） | 待目标硬件 |
| Load Time | n/a | 可忽略（开局 `SetSeed` + 牌堆 Fisher-Yates 一次性） | n/a |
| Network (Full Vision) | n/a | 服务器下发 `FDiceRollResult`（~16B/掷）远低于状态全量同步；绕开客户端浮点 | Full Vision 定 |

> RNG 非性能热点（回合制单场景、抽取稀疏）。性能预算的真实约束在 VFX 粒子上限（vfx F-4 N_max_vfx），
> 与本 ADR 无关。

## Migration Plan

无既有系统迁移（绿地实现，DiceService 尚未实现）。落地步骤：

1. ADR-0001 Accepted → 确定 DiceService 宿主类型与生命周期层。
2. C++ 实现 DiceService 封装权威 `FRandomStream`，按 Key Interfaces 钉退化契约 + 执行序铁律。
3. 实现期执行 Verification Required ①–④ 源码核验，结论回填本 ADR Last Verified。
4. VFX/Audio/HUD 各加私有独立流（禁默构 0）。
5. dice [Logic] fixture（AC-8 确定性序列）在 `-nullrhi` headless 跑绿 → 验证宿主+RNG 封装可测
   （architecture §8 成功判据③）。

**Rollback plan**：若 Verification Required 查实 5.7 有更优官方确定性 RNG 子系统 → 标本 ADR
Superseded，新 ADR 改用官方子系统；因 DiceService 是单一封装边界，调用方（移动/AI/事件格/VFX）不动。

## Validation Criteria

- [ ] dice AC-8（固定种子 + 固定调用序列 ≥20 抽取，两次冷启动逐字段精确相等）在 `-nullrhi` headless 跑绿。
- [ ] dice AC-10（`RandomRange(5,5)` 退化不推进 Seed：路径 A `[RollDice]` == 路径 B `[RandomRange(5,5),RollDice]` 逐字段）通过。
- [ ] dice AC-11（`RandomRange(10,3)` 返回 10 + ensure 触发，不 swap）通过。
- [ ] dice AC-17b（lazy-init 后 `InitialSeed != 0`）通过。
- [ ] ai AC-61a（权威流 + AI 决策端到端两次重放逐步全同，headless 隔离）通过。
- [ ] ai AC-61b（权威流 spy 断言消费方 ∈ {dice,AI}，VFX/HUD 未消费权威流，非 vacuous）通过。
- [ ] 实现期 Verification Required ①–④ 全部对 5.7 源码核验完毕，结论回填。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/dice.md` | 骰子(3) | CR-2 中央可种子化 RNG 服务（唯一权威流、禁旁路） | 裁定单一权威 `FRandomStream`（选项 A）；划线表 + 流隔离纪律 |
| `design/gdd/dice.md` | 骰子(3) | CR-3 种子与确定性 + F-4 性质 D | 钉执行序铁律 + 退化契约 + 跨平台处置；MVP/Full Vision 种子语义 |
| `design/gdd/dice.md` | 骰子(3) | F-2 RandomRange 退化（Min==Max 早退 / Min>Max ensure） | Key Interfaces 封装层硬门逐条钉死 |
| `design/gdd/dice.md` | 骰子(3) | OQ-1 / IG-3 RNG 服务宿主（与 ADR-0001 协调） | 宿主无关，依赖 ADR-0001；硬约束「与 player-turn 同生命周期层」 |
| `design/gdd/dice.md` | 骰子(3) | OQ-2 当前 Seed 序列化（显式 GetCurrentSeed） | R5 + Implementation Guideline 7（与 ADR-0005 协同） |
| `design/gdd/ai-opponent.md` | AI(10) | CR-5④ 权威流 vs juice 流隔离 / AC-42/44/47/61a/61b | 划线表：AI 决策扰动走权威流（须重放），juice 走独立流；AC-61b spy 硬门 |
| `design/gdd/vfx-feedback.md` | VFX(19) | CR-9 juice RNG 必走独立非权威流，严禁复用骰子3 权威流 / EC-13 | 独立非权威流裁定 + 划线表 + 误接由 AC-61b 抓出 |
| `design/gdd/tile-events.md` | 事件格(7) | CR-3 开局 Fisher-Yates 洗牌经骰子3 + 种子序列化 | 牌堆洗牌走权威流（须重放）；序列化与 ADR-0005 协同（model B 数组顺序） |
| `design/gdd/save-load.md` | 存档(21) | MVP 不序列化 Seed / 读档重设非确定种子 | R5 + Guideline 7：MVP 不存 Seed，Full Vision 经 GetCurrentSeed 存 |
| `docs/architecture/architecture.md` | 全局 | 架构原则2 确定性优先（单一种子化流 + 流隔离 + 禁逃逸条款） | 本 ADR 是原则2 的 Foundation 落点 |

## Related

- **依赖 ADR-0001**（UObject 宿主与生命周期边界）—— 必须先 Accepted；裁定 DiceService 宿主。
- **enables ADR-0005**（存档序列化契约）—— 牌堆 Fisher-Yates 种子序列化协同。
- **enables ADR-0006**（GameStateSnapshot）—— AI 决策随机经本服务权威流。
- **enables / 强度联动 ADR-0007**（BP-vs-C++ 边界）—— C++ 封装决定 RNG 禁旁路硬门 vs 软约束。
- **enables ADR-0010**（音频架构）—— 音频随机流随本 ADR 独立非权威约定。
- architecture.md §8「ADR-0004」条目（本 ADR 来源定义）+「API 边界 1 — DiceService」（冻结签名）。
- 代码文件：DiceService 实现（待 ADR-0001 Accepted 后落 `src/`）。
```
