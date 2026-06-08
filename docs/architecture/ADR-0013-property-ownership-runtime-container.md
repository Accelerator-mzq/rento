# ADR-0013: 地产所有权运行时状态容器与批量移交原子性

## Status

Accepted

> 2026-06-08 由 msc（用户）裁定 **Accept**（AskUserQuestion 选「Accept 0013 + 继续簇2」）。解除 OQ-Prop-1 ① / ② / ④ 的容器与原子性分量 + 受控写封装强度，使 prop-001/002/012 三个 Gap TR 与 prop-006/011（Partial）落地有据，相关 property-ownership 实现 story 可 `/create-stories` 不被 Blocked。

## Date

2026-06-08 — 起草

## Last Verified

2026-06-08 — 起草时**亲读 UE5.7 引擎源码**（`E:\Epic Games\UE_5.7\Engine\Source\Runtime\Core\Public\Containers\Array.h`）核验 `TArray<bool>` 行为，**证伪** GDD OQ-Prop-1 ① / unreal Issue1 的「`TArray<bool>` 是 bitfield specialization」前提（见 §Decision 决策②的 REFUTED 块）。

## Decision Makers

msc（用户）+ Technical Director（主笔）· 引擎事实由主会话直接对照 5.7 源码核验（非 agent claim）— 2026-06-08 Accepted

## Summary

地产所有权系统（#6 Core）拥有两份**以 `TileIndex` 为键的运行时可变状态**：① 归属 map `TileIndex → OwnerPlayerId`（无主 = `INDEX_NONE`）；② 每格 `bIsMortgaged` 标记。GDD 把这两份状态的**实现层载体形态、非可购买格槽位语义、批量破产移交的原子性、以及受控写封装强度**派发给 OQ-Prop-1 ADR（标 BLOCKING，下游 8/9 开工前必裁）。本 ADR 裁定：

1. **容器形态（prop-001）**：采**单一 per-tile 记录数组** `TArray<FTileOwnershipState> TileStates`（AoS），长度 = 全盘格数 N、以 `TileIndex` 稠密索引，优于 `TMap<int32,int32>`；非可购买格槽位存默认哨兵记录 `{INDEX_NONE, false}`。前提：TileIndex 连续 `0..N-1`（经典 0..39 成立；自定义棋盘若允许编号空洞须重评）。
2. **抵押标记容器（prop-002）**：`bIsMortgaged`（bool）**内含于 `FTileOwnershipState` 记录**，不设独立 bool 数组——故「`TArray<bool>` bitfield 陷阱」**根本不出现**。且该陷阱前提经源码核验**本身证伪**（UE `TArray<bool>` 非 specialization、`operator[]` 返真 `bool&`、可 memcpy / 序列化正常；UE 的位打包容器是另一类型 `TBitArray`）。
3. **批量移交原子性（prop-012）**：先全量前置校验整批、任一失败则整批中止（无部分写、dev `ensure`+log）；逐格写 = 单次 `FTileOwnershipState` 赋值（owner 与 `bIsMortgaged` 同格原子，类型层杜绝「只清一半」）；重入锁 `bIsMidBankruptcyTransfer` 经 **RAII scope guard 无条件解除**；逐格广播在锁内（AC-33c），`deferred broadcast queue` 列为 Alpha/Full-Vision 替代（解 AC-30b 读侧中间态，MVP 不需要）。
4. **受控写封装强度（prop-006）**：`TileStates` 为 ADR-0001 宿主上的 **C++ private 成员**，仅经 `Buy`/`Mortgage`/`Unmortgage`/`TransferOwnership`/`ReturnTileToBank` 写，无 `BlueprintReadWrite`；访问器只读 `BlueprintCallable`/`BlueprintPure`。据此把 AC-3/7/31/32/35 由 `[Logic·CI-stub]` 升正式 `[Logic]` 静态扫描门。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core / Scripting（运行时数据结构 + 序列化 + 事件广播） |
| **Knowledge Risk** | LOW-MEDIUM — `TArray`/`TMap`/`TBitArray`/`DYNAMIC_MULTICAST_DELEGATE` 均长期稳定容器/反射 API，非 post-cutoff。唯一历史误区（`TArray<bool>` 特化）已亲读 5.7 源码证伪 |
| **References Consulted** | `E:\Epic Games\UE_5.7\...\Containers\Array.h`（`class TArray` 唯一模板、`operator[]` 返回类型）；`Containers\BitArray.h`（`TBitArray` 确为独立位打包类型）；`docs/architecture/ADR-0001`（宿主）/`ADR-0002`（棋盘 N/TileIndex 空间）/`ADR-0005`（序列化）/`ADR-0003`（事件总线） |
| **Post-Cutoff APIs Used** | None（本 ADR 全用 UE 长期稳定容器与反射宏） |
| **Verification Required** | None（容器/原子性/封装均 headless `-nullrhi` 可测；引擎事实已源码核验）。注：破产逐格广播下监听器同步回写的拦截须以 spy fixture 实测（AC-33c），属测试义务非引擎不确定性 |

> **Note**: 与 ADR-0002（HIGH 风险，依赖 post-cutoff DataTable/AssetManager 行为）不同，本 ADR 的引擎面是 UE 自 UE4 起稳定的核心容器，风险低。唯一的「知识缺口」是 LLM 易把 UE `TArray<bool>` 与 C++ STL `std::vector<bool>` 混淆，已在 §Decision 决策② REFUTED 块澄清。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001（UObject 宿主与生命周期）— `TileStates` 字段挂载的宿主类型（per-match 服务，OQ-Prop-1 ③ 已由 0001 Covered）；ADR-0002（棋盘容器）— 提供全盘格数 N 与 TileIndex 空间（`0..N-1` 连续前提的事实来源）；ADR-0005（存档契约）— `TileStates` 经 `UPROPERTY(SaveGame)` 序列化、PostLoad 派生量重算顺序（OQ-Prop-1 ⑥）；ADR-0003（事件总线）— `OnOwnershipChanged`/`OnMortgageChanged` 广播契约（OQ-Prop-1 ④ 广播策略的上层纪律） |
| **Enables** | 地产所有权(#6) 实现 story（解除 prop-001/002/012 Blocked）；建房(#8) house_count 容器（build-002 Partial，本 ADR 确立的 per-tile 稠密记录数组模式可被 8 复用，见 §Consequences） |
| **Blocks** | 地产所有权(#6) 全部实现 story；破产胜负(#9) 批量移交实现（依赖本 ADR 的原子性与重入锁契约） |
| **Ordering Note** | ADR-0001/0002/0005/0003 均已 Accepted，本 ADR 无前置阻塞，可立即裁。本 ADR 不重裁宿主类型（ADR-0001）、不重裁序列化是否按 SaveGame 过滤（ADR-0005 §G7 已实证内置路径全量序列化）、不重裁事件总线模式（ADR-0003），只补「运行时容器形态 + 批量原子性 + 封装强度」三件 0001-0005 未细化的实现层契约 |

## Context

### Problem Statement

地产所有权 GDD（property-ownership.md，APPROVED-with-followups）把以下实现层决策显式派发给 **OQ-Prop-1 ADR**（标 BLOCKING，「下游 8/9 实现开始前 `/architecture-decision`」）：

- **①** owner map 容器形态（`TArray` 稠密 vs `TMap`）+ 非可购买格哨兵槽语义 + TileIndex 连续无洞前提；抵押标记容器须避「`TArray<bool>` bitfield specialization」（unreal Issue1）；
- **②** 受控写封装强度（C++ private + 访问器 vs 纯软约束）——决定 AC-3/7/31/32/35 能否由 `[Logic·CI-stub]` 升正式 `[Logic]`；
- **④** 破产级联广播策略（逐格广播 + 重入锁 vs deferred broadcast queue）+ 锁无条件解除不变式；
- 以及关联的 **批量移交原子性**（States/AC-33b/34b：全或全无、owner 与 `bIsMortgaged` 同格原子清零）。

未裁的代价：tr-registry 中 **prop-001 / prop-002 / prop-012 三个 Gap TR** 无 ADR，property-ownership 全部实现 story 被 `/create-stories` 标 Blocked；且 AC-3/7/31/32/35 五条受控写/无环/移交方断言长期停在 `[Logic·CI-stub]`（skip 占位 + CI 报手动审查），无正式 merge gate。

### Current State

- ADR-0001 已裁宿主：per-match 服务挂 `UWorldSubsystem`（OQ-Prop-1 ③ Covered，prop-003）。
- ADR-0002 已裁**静态**棋盘载体为 `UBoardDataAsset`（`TArray<FBoardTileData> Tiles`，N 格、TileIndex 空间）——但**运行时可变归属/抵押状态的容器**不在 0002 范围（0002 只管设计期 authored 静态数据）。
- ADR-0005 已裁存档契约（`USaveGame`+`UPROPERTY(SaveGame)`+四重门），并经 §G7 实证内置 `SaveGameToMemory`/`SaveGameToSlot` **不按** `SaveGame` 标记过滤（全量序列化）；`TileStates` 随存档序列化，派生量（is_monopoly/count）不存、读档后重算（prop-004/005/014 Covered/Partial）。
- ADR-0003 已裁去中心化 owner-held `DYNAMIC_MULTICAST_DELEGATE` 事件总线（prop-010 Covered）。
- GDD §States「破产级联」已由设计期 D3 裁定「逐格广播 + 重入锁」，并要求 ADR 对比 deferred queue、固化「锁无条件解除」（systems FINDING-2）。

即：宿主、静态载体、序列化、事件模式四件根须已定；**缺的是运行时容器的具体形态、批量写的原子性协议、容器的封装强度**——本 ADR 补这三件。

### Constraints

- **技术**：UE5.7，单线程同步对局模型（MVP 不联网，ADR-0004 确定性 RNG / ADR-0006 值快照同此前提）。容器须 `UPROPERTY(SaveGame)` 可序列化、headless `-nullrhi` 可测、与 BP 受控接口兼容。
- **依赖图无环**：本系统 6→5（push 快照算租）、8→6 / 9→6 / 17→6 等单向（CR-6/CR-7）。容器与封装不得引入反向读（如 6 读 8 的 house_count）。
- **确定性**：3 派生谓词（is_monopoly/station_count/utility_count）纯整数、每次实时重算无缓存（F-1~3 / AC-40/40b），容器须支持全盘遍历且不丢格。
- **破产级联重入**：`Broadcast()` 同步执行所有监听器，监听器（AI10 等）可能在回调内尝试回写——容器写路径须能在批量移交期间拒绝外部受控写（重入锁），且锁绝不挂起致死锁（FINDING-2）。

### Requirements

- **R1**：容器无损承载「每可购买格的 owner（int32, INDEX_NONE 哨兵）+ bIsMortgaged（bool）」，且支持非可购买格槽位（哨兵）使全盘遍历 `[t]` 不越界。
- **R2**：owner 与 bIsMortgaged 的**同格原子清零**（银行回退）须是类型层事实，不可只清一半（AC-34b）。
- **R3**：批量移交全或全无（AC-33b），任一格异常不留部分转移中间态。
- **R4**：运行态访问 O(1) 或近 O(1)（`GetOwner`/`IsMortgaged`/全盘 count 遍历）。
- **R5**：容器随存档（21）`UPROPERTY(SaveGame)` 序列化（owner + bIsMortgaged，不存派生量）。
- **R6（可测/封装）**：受控写绕过、5↔6 无环、push 方向、6↔8 无环、移交方（AC-3/7/31/32/35）须能由静态扫描断言——要求容器对外只读、写仅经受控接口，无 BP 直写路径。
- **R7（可逆）**：内部记录形态（AoS vs SoA）对外不可见——下游只见 `GetOwner`/`IsMortgaged`/`BuildOwnershipSnapshot`，未来改内部布局接口不变。

## Decision

### 决策① — 容器形态：单一 per-tile 记录数组（AoS），稠密 TileIndex 索引

运行时归属/抵押状态由 ADR-0001 宿主持有一个 **`TArray<FTileOwnershipState> TileStates`**，长度 = 棋盘全盘格数 N（取自 ADR-0002 加载的 `UBoardDataAsset::Tiles.Num()`），以 `TileIndex` 稠密索引。每格一条记录，含 `OwnerPlayerId`（int32，无主 = `INDEX_NONE`）+ `bIsMortgaged`（bool）。

**非可购买格哨兵语义（焊死，unreal Issue6）**：非可购买格（Go/Tax/Jail/Chance/…）的槽位**仍存在**于数组中，持默认哨兵记录 `{OwnerPlayerId=INDEX_NONE, bIsMortgaged=false}`。这使 F-2/F-3 的全盘遍历 `TileStates[t]` 对所有 `t∈[0,N)` 不越界；非可购买格由遍历内的 `GetTileData(t).TileType` 过滤先行短路（CR-5 / F-2 求值顺序约束：TileType 过滤先于 owner 比较），哨兵 owner 永不被误计入任何玩家 count。

**前提（ADR-locked，unreal Issue7）**：`TArray` 稠密索引依赖 **TileIndex 连续无洞 `0..N-1`**（经典盘 0..39 成立）。自定义棋盘若允许 TileIndex 编号空洞，本方案须重评（退化为 `TMap` 或在加载期补洞为哨兵）；棋盘加载校验（board 1）须保证 TileIndex 连续，否则 owner map 长度/索引前提破坏——此约束登记 §Risks 并应由 board-data 加载校验兜底。

**选 AoS（记录数组）而非 SoA（两并行数组）的理由**：owner 与 bIsMortgaged 同格原子清零是硬要求（R2/AC-34b）。AoS 下「清一格」= `TileStates[t] = FTileOwnershipState{}`，单次 struct 赋值**类型层杜绝「只清 owner 留 bIsMortgaged==true 的非法 Unowned 态」**；SoA 下须记得两数组都清（纪律约束，易漏）。AoS 同时让破产移交逐格写成为单次记录赋值，配合下方原子性协议最干净。代价：snapshot 装配（`BuildOwnershipSnapshot` 仍按 owner/mortgage 分字段返回，§Key Interfaces）从记录读两字段，可忽略。

**选稠密 `TArray` 而非 `TMap` 的理由**（GDD 已论证，此处收口）：cache locality + O(1) 索引 + 全盘遍历自然；「`TMap` 对 BP 友好」论据**无效**——BP 只调受控接口（决策④），容器对 BP 不可见。`TMap` 仅在 TileIndex 稀疏（大量空洞）时才有空间优势，经典/自定义连续盘不触发。

### 决策② — 抵押标记容器：内含于记录，且「TArray<bool> bitfield」前提 REFUTED

`bIsMortgaged` 作为 `FTileOwnershipState` 的字段，**不设独立 `TArray<bool>` / `TArray<uint8>` / `TBitArray`**。故 GDD OQ-Prop-1 ① / unreal Issue1 担忧的「抵押标记容器须避 `TArray<bool>`」在本设计下**根本不出现**（无独立 bool 数组）。

> **🔴 REFUTED（亲读 5.7 源码，2026-06-08）—— 该前提本身不成立**
>
> GDD OQ-Prop-1 ① 称「UE `TArray<bool>` 是 bitfield specialization——不能 memcpy、`operator[]` 返 proxy、range-for 赋值静默失败、序列化路径不透明」。**对照 `E:\Epic Games\UE_5.7\Engine\Source\Runtime\Core\Public\Containers\Array.h` 核实，此说法为假**：
> - `class TArray` 仅有**单一**模板定义（`template<typename InElementType, typename InAllocatorType> class TArray`，L669）；全 Core 树内**无任何 `TArray<bool>` 偏/全特化**（grep `template.*TArray<bool` = 0 命中）。
> - `operator[]` 返回 **`ElementType& operator[](SizeType Index)`**（L1171）——`TArray<bool>` 下即真 `bool&` 引用，**非 proxy 对象**。
> - 因此 UE `TArray<bool>` 每元素存 1 字节、`operator[]` 返真引用、range-for 赋值生效、可 memcpy/`Memzero`、反射序列化走标准字节路径——**全部正常**。
>
> 「bitfield specialization + proxy operator[]」是 **C++ STL `std::vector<bool>`** 的特性（STL 确实特化为位打包+proxy），LLM 与设计期把它误植到 UE。UE 的位打包容器是**另一独立类型 `TBitArray`**（`BitArray.h` 确认存在），`TArray<bool>` 与之无关。
>
> **结论**：`TArray<bool>` 在 UE 下可安全使用——本 ADR 选 AoS 内含 bool 是因 R2 原子性（决策①），**非**因 bool 数组有 bug。此条与 ADR-0002「CSV TArray REFUTED」同型：设计期对 UE 行为的臆测须以源码为准（见记忆 `sprint0-engine-spike-findings`）。GDD OQ-Prop-1 措辞「改用 TArray<uint8>/TBitArray」基于伪前提，已随本 ADR 同步更正。

### 决策③ — 批量移交原子性 + 重入锁 + 广播策略

破产9 的批量移交（收租 in-kind / 银行回退）须**全或全无**（R3/AC-33b），owner 与 bIsMortgaged 同格原子清零（R2/AC-34b）。协议：

**1. 先全量前置校验，后逐格写（pre-validate-then-commit）**：
进入批量移交时，先遍历债务人持有的全部目标格，校验整批前置——in-kind 须 `creditor != INDEX_NONE`（AC-33e，否则产生非法 Unowned 态）、各格可购买（AC-33d）、owner == debtor。**任一格前置失败 → 整批中止，不写任何一格**（dev `ensure` + log）。前置全过后才进入写阶段。单线程同步下记录赋值本身不可能失败，故「全过则全成」——无部分转移中间态。

**2. 逐格写 = 单次记录赋值（同格原子）**：
- 收租 in-kind：`TileStates[t].OwnerPlayerId = creditor;`（`bIsMortgaged` 随地保留，经济 F-11，AC-33）——owner 改、标记不动，同一记录内一致。
- 银行回退：`TileStates[t] = FTileOwnershipState{};`（owner 与 bIsMortgaged 同一赋值清零，AC-34/34b）——**类型层不可能只清一半**。

**3. 重入锁 `bIsMidBankruptcyTransfer`，RAII scope guard 无条件解除（FINDING-2）**：
批量移交全程置 `bIsMidBankruptcyTransfer=true`，锁期内任何 `Buy`/`Mortgage`/`Unmortgage`/`TransferOwnership`/`ReturnTileToBank` 外部受控写被拒绝（dev `ensure`+log，AC-33c），防 `Broadcast()` 同步调起的监听器（AI10 等）在级联中途回写 owner map。锁的置/复位用 **RAII scope guard**（C++ 栈对象析构必复位，或 `ON_SCOPE_EXIT`）——无论批量全成、中途 `ensure`、还是异常路径，离开移交作用域时锁**无条件解除**，绝不挂起致后续 `Buy`/`Mortgage` 永久死锁。

**4. 逐格广播在锁内（D3，AC-33b/33c）**：
每格写后各广播一次 `OnOwnershipChanged`（N 格 = N 次），监听器（VFX19 等）自行视觉批处理。广播在锁内进行，监听器回调期间外部写被锁拦截。

> **deferred broadcast queue 替代（unreal Issue2，列 Alpha/Full-Vision，MVP 不采）**：
> 逐格广播下监听器可观察「部分移交」的读侧中间态（tile1 已转、tile2 未转），这是 AC-30b 读侧问题的一面。替代方案：移交期把广播入队、整批完成后批量广播（监听器只见终态）。本 ADR **MVP 选逐格广播 + 重入锁**（与既定 AC-33b/33c 一致、实现简单、单线程下写原子性已由协议 1-3 保证；读侧中间态由消费方经快照值拷贝 AC-30b 隔离）。deferred queue 留待 Alpha（联网/复杂监听）评估，届时若采则 prop-011 升级、AC-33b「逐格各广播」措辞随之调整。此 fork 显式登记，不在 MVP 默默改向。

### 决策④ — 受控写封装强度：C++ private 容器 + 只读访问器（升 5 条 CI-stub AC）

`TileStates` 为 ADR-0001 宿主类（`UWorldSubsystem`）上的 **C++ `private` 成员**，**不加** `UPROPERTY(BlueprintReadWrite)`/`EditAnywhere`（仅 `UPROPERTY(SaveGame)` 供序列化）。状态变更**仅经** 5 个受控写接口（`Buy`/`Mortgage`/`Unmortgage`/`TransferOwnership`/`ReturnTileToBank`，均 `UFUNCTION(BlueprintCallable)` 带 `playerId`/前置校验）；读经只读访问器（`GetOwner`/`IsMortgaged`/`IsMonopoly`/`GetStationCount`/`GetUtilityCount`/`BuildOwnershipSnapshot`，`BlueprintPure`/`BlueprintCallable`）。

据此，GDD §AC 引言 PROP-013 的 5 条 `[Logic·CI-stub]`（AC-3 受控写绕过、AC-7 economy 不反向调 6、AC-31 push 方向、AC-32 6 不读 8、AC-35 移交方=9）**升为正式 `[Logic]` 静态扫描门**：

- AC-3：全代码库静态扫描无外部系统直写 `TileStates`/记录字段（C++ private 编译期已防直写，扫描坐实无 friend/反射后门）。
- AC-7/AC-31：economy 实现内静态扫描对本系统写/查接口调用 = 0（5↔6 无环）。
- AC-32：本系统对系统#8 调用 = 0（6↔8 无环，快照不含 house_count）。
- AC-35：`TransferOwnership`/`ReturnTileToBank` 调用方静态扫描 = 9 编排路径，economy 不直调。

> **BP 边界（引用 ADR-0007，不重裁）**：ADR-0007 已定「关键逻辑 C++ / 受控写硬封装」。BP 无模块级访问控制——本 ADR 的强封装来自 **C++ `private` + 不暴露 BP 写属性**（编译期），非靠 BP 软约束。受控写接口虽 `BlueprintCallable`（供回合2/HUD 输入路由调用），但它们**内部跑前置校验**，不等于「BP 可直写容器」。`meta=(BlueprintPrivate)`（unreal Issue4）对本设计非必要——容器根本不暴露为 BP 属性。

### Architecture

```
                ADR-0001 宿主 (UWorldSubsystem, per-match)
   ┌───────────────────────────────────────────────────────────┐
   │  private:                                                   │
   │    UPROPERTY(SaveGame)                                      │
   │    TArray<FTileOwnershipState> TileStates;  // 长度 N, 稠密 │
   │    bool bIsMidBankruptcyTransfer = false;   // 重入锁       │
   │                                                            │
   │  受控写 (BlueprintCallable, 前置校验 + 锁检查):            │
   │    Buy / Mortgage / Unmortgage                             │
   │    TransferOwnership / ReturnTileToBank  ← 仅破产9 编排调   │
   │                                                            │
   │  只读 (BlueprintPure/Callable):                            │
   │    GetOwner / IsMortgaged / IsMonopoly                     │
   │    GetStationCount / GetUtilityCount                       │
   │    BuildOwnershipSnapshot  → push 给经济5 (6→5, 值拷贝)    │
   └───────────────────────────────────────────────────────────┘
        │ 事件 (ADR-0003 owner-held DYNAMIC_MULTICAST_DELEGATE)
        ▼
   OnOwnershipChanged(TileIndex,Old,New) / OnMortgageChanged(TileIndex,bMortgaged)
   逐格广播 (破产 N 格 = N 次), 锁内, 监听器回写被拦截

   批量移交 (破产9 触发):
     RAII guard{ bIsMidBankruptcyTransfer=true }
       1. 全量前置校验整批 (creditor≠NONE / 可购买 / owner==debtor)
          └ 任一失败 → 整批中止, 无写, ensure+log
       2. 逐格: TileStates[t]=... (in-kind: owner=creditor;
                 bank-return: = FTileOwnershipState{} 同格原子清)
       3. 逐格广播
     } ← 离作用域: 锁无条件复位 (FINDING-2)
```

### Key Interfaces

```cpp
// 运行时单格归属状态记录 (AoS 元素; 内部存储形态, 对外经访问器)
USTRUCT()
struct FTileOwnershipState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 OwnerPlayerId = INDEX_NONE; // 无主 = -1; 哨兵同值
    UPROPERTY(SaveGame) bool  bIsMortgaged  = false;      // 同格随 owner 原子清零
};

// 经济算租消费的归属快照 (CR-6, 值拷贝, 不含 house_count)
// — 内部 AoS 形态对外不可见 (R7); 此 struct 形态归 ADR-0006 GameStateSnapshot 体系
USTRUCT(BlueprintType)
struct FOwnershipSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Owner = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) bool  bIsMortgaged = false;
    UPROPERTY(BlueprintReadOnly) bool  bIsMonopoly = false;
    UPROPERTY(BlueprintReadOnly) int32 StationCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 UtilityCount = 0;
    // 注: 不含 house_count (归建房8, 防 6↔8 环, AC-30 负断言)
};

// 宿主类受控写/只读契约 (签名稳定, GDD 接口稳定承诺)
//   private: TArray<FTileOwnershipState> TileStates;  bool bIsMidBankruptcyTransfer;
//   public:
//     bool Buy(int32 TileIndex, int32 PlayerId);                       // CR-2
//     bool Mortgage(int32 TileIndex, int32 PlayerId);                  // CR-3
//     bool Unmortgage(int32 TileIndex, int32 PlayerId);               // CR-3
//     bool TransferOwnership(int32 TileIndex, int32 From, int32 To);  // 9→6, To≠NONE
//     bool ReturnTileToBank(int32 TileIndex);                          // 9→6, 同格清零
//     int32 GetOwner(int32 TileIndex) const;
//     bool  IsMortgaged(int32 TileIndex) const;
//     bool  IsMonopoly(int32 PlayerId, EColorGroup Group) const;      // F-1, 实时重算
//     int32 GetStationCount(int32 PlayerId) const;                    // F-2, 全盘遍历
//     int32 GetUtilityCount(int32 PlayerId) const;                    // F-3
//     FOwnershipSnapshot BuildOwnershipSnapshot(int32 PlayerId, int32 TileIndex) const;
```

### Implementation Guidelines

1. **初始化**：宿主 `Initialize`/对局开始时，`TileStates.SetNum(N)`（N = 棋盘格数）并默认填哨兵记录（`FTileOwnershipState` 默认成员已是 `{INDEX_NONE,false}`，`SetNum` 默认构造即得哨兵）。不对可购买/非可购买格区分初始化——全部哨兵，可购买格经 `Buy` 改 owner。
2. **全盘遍历守序（CR-5 / F-2 RECOMMENDED-2）**：count 遍历 `for t in [0,N): if GetTileData(t).TileType==Railroad/Utility && TileStates[t].OwnerPlayerId==playerId` ——**TileType 过滤先于 owner 比较**，非可购买格哨兵不进 count。
3. **批量原子性**：实现 `TransferAllTiles(debtor, creditor)` / `ReturnAllTilesToBank(debtor)` 时严格走「先全量前置校验 → 逐格写 → 逐格广播」三段，RAII guard 包裹锁。前置阶段不得有任何 `TileStates` 写。
4. **派生量不缓存（AC-40b）**：`IsMonopoly`/`GetStationCount`/`GetUtilityCount` 每次从 `TileStates` + 棋盘1 实时重算，不设成员缓存。读档后无须重建缓存（无缓存）；PostLoad 仅需 `TileStates` 已反序列化 + 棋盘1 已加载（`GetTilesInGroup` 可用，OQ-Prop-1 ⑥，ADR-0001/0005 序）。
5. **快照值拷贝（AC-30/30b）**：`BuildOwnershipSnapshot` 返回 `FOwnershipSnapshot` 值（非活引用/指针），`static_assert` 字段集恰 5 个且不含 house_count（AC-30 负断言）。
6. **封装坐实**：CI 加静态扫描（pt-009 authoritative-purity 同套思路）断言 AC-3/7/31/32/35，替换 GDD PROP-013 的 skip 占位 CI-stub。

## Alternatives Considered

### Alternative 1: 单 per-tile 记录数组 `TArray<FTileOwnershipState>`（AoS）— 【选定】

- **描述**：见 §Decision 决策①。owner+mortgage 同记录，长度 N 稠密，哨兵填非可购买格。
- **Pros**：同格原子清零是类型层事实（R2/AC-34b 单赋值杜绝半清）；批量逐格写=单记录赋值最干净（R3）；dense O(1) + cache locality（R4）；无独立 bool 数组，彻底绕开 `TArray<bool>` 之争；序列化为单一 `TArray<USTRUCT>`（R5）。
- **Cons**：snapshot 装配从记录读两字段（可忽略）；与 GDD「两 map」叙述形态不同（但对外经访问器，AC 走 GetOwner/IsMortgaged 行为断言、不依赖内部形态，R7）。
- **采纳理由**：原子性（R2/R3）是本 ADR 最硬要求，AoS 在类型层兑现，胜出。

### Alternative 2: 两并行稠密数组（SoA）`TArray<int32> OwnerMap` + `TArray<uint8/bool> MortgageFlags`

- **描述**：owner 与 mortgage 各一数组，均长度 N、TileIndex 索引。
- **Pros**：贴合 GDD「owner map + 抵押标记两份」叙述；各数组序列化独立。`uint8`/`bool` 经源码核验均安全（决策② REFUTED）。
- **Cons**：同格原子清零靠实现纪律（须记得两数组都清），AC-34b「不可只清一半」无类型层保障——正是 prop-012 要焊死的风险点；批量移交逐格写两次。
- **Rejection Reason**：把 R2 原子性从「类型层事实」降为「纪律约束」，与 prop-012 立法意图相悖。`uint8`/`bool` 之争在 AoS 下根本不存在（无独立标记数组），SoA 反而把该争议引回来。

### Alternative 3: `TMap<int32, FTileOwnershipState>`（稀疏键）

- **描述**：只为可购买格建键，非可购买格不入 map。
- **Pros**：非可购买格不占槽；TileIndex 空洞天然兼容。
- **Cons**：全盘 count 遍历须先判键存在性、cache locality 差；「TMap 对 BP 友好」无效（容器不暴露 BP）；`GetOwner(非可购买格)` 须额外缺省逻辑；与 ADR-0002 静态 `TArray<FBoardTileData> Tiles` 的稠密索引模式不一致。
- **Rejection Reason**：经典/自定义连续盘下稠密 `TArray` 全面更优；TMap 仅在 TileIndex 高度稀疏时才值得，非 MVP 场景。空洞前提由 board 加载校验兜底（§Risks），不为假想稀疏盘牺牲常态性能。

## Consequences

### Positive

- prop-001 / prop-002 / prop-012 三个 Gap TR 解除，property-ownership 实现 story 可 `/create-stories` 不被 Blocked。
- AC-3/7/31/32/35 由 `[Logic·CI-stub]` 升正式 `[Logic]` 静态扫描门（受控写/无环/移交方有 merge gate，消除执行真空）。
- owner+mortgage 同格原子清零成为类型层事实（AC-34b 不靠纪律）；批量移交全或全无有明确协议（AC-33b）。
- 澄清并纠正 GDD `TArray<bool>` 伪前提（源码实证），与 ADR-0002 CSV 更正同型，积累「UE 行为以源码为准」纪律。
- 确立的「per-tile 稠密记录数组 + 哨兵 + 受控写封装」模式可被建房(8) house_count 容器（build-002 Partial）直接复用（同宿主、同索引、同封装），降低 8 的实现决策成本。

### Negative

- AoS 内部形态与 GDD「两 map」叙述不一致——须靠「对外经访问器、AC 走行为断言」消化（R7 已隔离，但读 GDD 的实现者需理解内部≠叙述）。GDD OQ-Prop-1 已随本 ADR 同步标注。
- 自定义棋盘 TileIndex 空洞会破坏稠密前提——须 board 加载校验兜底（外部依赖，§Risks）。

### Neutral

- `FOwnershipSnapshot` 的最终 struct 形态归 ADR-0006 GameStateSnapshot 体系协调（本 ADR 只保证不含 house_count、值拷贝）；本 ADR 给参考形态，不与 0006 冲突。
- deferred broadcast queue 未采但已登记为 Alpha 替代，未来联网阶段可重评（prop-011）。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| 自定义棋盘 TileIndex 非连续（有洞），破坏 `TArray` 稠密索引前提 | Low（MVP 经典盘）/ Medium（Alpha 编辑器） | Medium | board-data 加载校验须保证 TileIndex 连续 `0..N-1`（登记为 board 1 加载校验义务）；否则本系统 `SetNum(maxIndex+1)` 补洞为哨兵亦可，但须 ADR 重评。MVP 经典盘不触发 |
| 监听器经间接路径（非直接受控写）在级联中改共享态，重入锁拦不住 | Low | Medium | 重入锁拦直接受控写已覆盖 AC-33c 主路径；间接路径若出现，deferred broadcast queue（已登记 Alpha 替代）彻底隔离。MVP 单线程 + 监听器只读快照（AC-30b 值拷贝）降低暴露面 |
| 实现者误把 AoS 当 SoA、批量回退只清 owner 不清 mortgage | Low | High | AoS 单赋值 `TileStates[t]=FTileOwnershipState{}` 类型层杜绝；AC-34b [Logic] 门坐实；code-review 检查无「只写 .OwnerPlayerId 不写记录」路径 |
| RAII guard 漏用、锁挂起致死锁（FINDING-2） | Low | High | 强制用栈 scope guard / `ON_SCOPE_EXIT`，不手写 try-finally；AC-33c fixture 须含「批量中途 ensure 后锁仍复位、后续 Buy 可用」断言 |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | N/A | `GetOwner`/`IsMortgaged` = O(1) 数组索引 ~0ms；count 全盘遍历 O(N)（N≤40+）每次算租调用，~µs 级 | 16.6ms（远低于占用） |
| Memory | N/A | `TArray<FTileOwnershipState>` = N × ~8 字节（int32+bool+pad）≈ 经典盘 ~320 字节，单局 | 待目标硬件定（极小） |
| Load Time | N/A | 对局初始化 `SetNum(N)` 一次性，~0ms | 无额外预算 |
| Network | N/A（MVP 不联网；Full Vision 须重评单线程原子性 + deferred queue） | N/A | N/A |

## Migration Plan

无既有实现迁移（property-ownership 尚未拆 story 实现，本 ADR 是其容器实现的前置裁定）。落地步骤：

1. 定义 `FTileOwnershipState` USTRUCT（`UPROPERTY(SaveGame)`）。
2. ADR-0001 宿主类加 `private: TArray<FTileOwnershipState> TileStates; bool bIsMidBankruptcyTransfer;`；`Initialize` 内 `SetNum(N)`。
3. 实现 5 受控写 + 只读访问器 + `BuildOwnershipSnapshot`（值拷贝、不含 house_count）。
4. 实现批量移交三段协议 + RAII 锁 guard。
5. `/create-stories property-ownership` 时把本 ADR 嵌入 story 的 ADR 约束；AC-3/7/31/32/35 解 skip，补静态扫描测试（替换 PROP-013 CI-stub）。

**Rollback plan**：若 AoS 证明不可行（极低），因下游只见访问器（R7），可改内部为 SoA 而接口不变；序列化字段名变更须走存档迁移（ADR-0005 枚举/字段 append-only 纪律）。

## Validation Criteria

- [ ] `TileStates.SetNum(N)` 后所有槽位 `{INDEX_NONE,false}`（AC-1 初始态）。
- [ ] 银行回退 `TileStates[t]=FTileOwnershipState{}` 后 owner 与 bIsMortgaged 同时清零，无「只清一半」（AC-34/34b）。
- [ ] 批量移交前置任一失败 → 无任何格被写（AC-33b 全或全无）。
- [ ] 批量移交中途 ensure 后 `bIsMidBankruptcyTransfer` 复位、后续 `Buy` 可用（FINDING-2 锁无条件解除）。
- [ ] 监听器在 `OnOwnershipChanged` 回调内 `Buy`/`Mortgage` 被拒（AC-33c 重入锁）。
- [ ] AC-3/7/31/32/35 静态扫描测试跑绿（替换 CI-stub），headless `-nullrhi` 可跑。
- [ ] `BuildOwnershipSnapshot` `static_assert` 字段集 5 个且不含 house_count（AC-30 负断言）。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/property-ownership.md` | 地产所有权(#6) | **OQ-Prop-1 ①**：owner map 容器形态 + 非可购买格哨兵槽 + TileIndex 连续前提 | 决策①：`TArray<FTileOwnershipState>` 稠密、长度 N、哨兵填非可购买格、连续 `0..N-1` 前提焊死 |
| `design/gdd/property-ownership.md` | 地产所有权(#6) | **OQ-Prop-1 ① / unreal Issue1**：抵押标记须避 `TArray<bool>` bitfield | 决策②：bool 内含于记录无独立数组；且源码核验该前提 **REFUTED**（UE `TArray<bool>` 非特化、operator[] 返真引用），GDD 同步更正 |
| `design/gdd/property-ownership.md` | 地产所有权(#6) | **States/AC-33b/34b**：批量移交原子性、owner 与 bIsMortgaged 同格原子清零 | 决策③：先校验后写 + 单记录赋值（类型层同格原子）+ RAII 锁 |
| `design/gdd/property-ownership.md` | 地产所有权(#6) | **OQ-Prop-1 ④ / States 破产级联 / FINDING-2**：逐格广播 vs deferred queue + 锁无条件解除 | 决策③：MVP 逐格广播+重入锁、RAII 无条件解除；deferred queue 列 Alpha 替代 |
| `design/gdd/property-ownership.md` | 地产所有权(#6) | **OQ-Prop-1 ② / prop-006 / AC-3/7/31/32/35**：受控写封装强度升 [Logic] | 决策④：C++ private + 无 BP 写属性 + 只读访问器，5 条 CI-stub 升正式 [Logic] 静态扫描 |
| `design/gdd/building-upgrade.md` | 建房(#8) | **build-002**：house_count 容器形态（共享 OQ-Prop-1 生命周期） | 本 ADR 确立的 per-tile 稠密记录数组 + 哨兵 + 封装模式供 8 复用（§Consequences，非本 ADR 强裁 8） |

## Related

- **依赖**：ADR-0001（宿主）、ADR-0002（棋盘 N/TileIndex 空间）、ADR-0005（序列化 + §G7 内置路径全量序列化实证）、ADR-0003（事件总线）
- **被使能**：地产所有权(#6) 实现 story；建房(#8) house_count 容器（build-002 模式复用）；破产9 批量移交
- **GDD**：`design/gdd/property-ownership.md`（OQ-Prop-1、CR-1/2/3、States 破产级联、AC-3/7/30/30b/31/32/33b/33c/33e/34/34b/35）
- **引擎核验来源**：`E:\Epic Games\UE_5.7\Engine\Source\Runtime\Core\Public\Containers\Array.h`（L669 单一 `class TArray` 模板、L1171 `operator[]` 返 `ElementType&`、全树无 `TArray<bool>` 特化）；`BitArray.h`（`TBitArray` 独立位打包类型）
- **同型先例**：ADR-0002（CSV TArray REFUTED）；记忆 `sprint0-engine-spike-findings`（claim≠结论，UE 行为以源码为准）
- **总纲**：`docs/architecture/architecture.md` §8 + `tr-registry.yaml`（prop-001/002/012）
