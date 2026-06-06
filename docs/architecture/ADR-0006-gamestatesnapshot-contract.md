# ADR-0006: GameStateSnapshot 聚合契约（AI 只读视图）

## Status

Accepted

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

msc（用户）+ Technical Director — 2026-06-06 Accepted（用户裁定全接受推荐选型）。输入方：ai-opponent.md（CR-6 字段清单 / OQ-AI-3）、player-turn.md（CR-8 hook 签名 / snapshot 宿主）、architecture.md §D.1a（snapshot 生成时机）、§8 ADR-0006 条目。

## Summary

AI(10) 三 hook 须读取一份跨系统聚合的只读视图 `FGameStateSnapshot` 来做买地/建房/出狱决策，但该 struct 的字段构成、装配时机、宿主与可写性此前仅作 OQ-AI-3 悬置。本 ADR 裁定 `FGameStateSnapshot` 为**值语义、决策期一次性装配、决策期冻结、按 `const&` 传入**的只读聚合视图，由**回合2 编排在 AI 决策阶段开头装配一次**，覆盖 CR-6 全部 per-hook 字段（含全盘暴露视图 `Rent_top1/top2` + owner map），并钉死 CI-stub AC-5/6/7/48 的关闭条件。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core / Scripting（USTRUCT 聚合 + const-ref 传参，无渲染/输入/物理域） |
| **Knowledge Risk** | LOW — `USTRUCT` 值语义、`const FStruct&` UFUNCTION 传参、`TArray`/`TMap` 成员是 UE 长期稳定 API，远早于 5.3，无 post-cutoff 依赖 |
| **References Consulted** | architecture.md §2.1（Foundation 模块①宿主/④Board DA）、§D.1a（snapshot 生成）、§8（ADR-0006 条目）、ai-opponent.md CR-6 / AC-5/6/7/48 / OQ-AI-3 |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | None（引擎风险 LOW，无需对照 post-cutoff API；唯一须验证的是性能 AC-S6/S7 的实测耗时，属实现期而非引擎兼容性） |

> **Note**: Knowledge Risk = LOW，引擎升级不强制重审本 ADR；但若 player-turn 状态机宿主（ADR-0001）或 Board DA 访问（ADR-0002）的接口变更，须回看本 ADR 的装配方依赖。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001（UObject 宿主与生命周期边界——回合2 的 `UWorldSubsystem`/状态机宿主是 snapshot 装配方的运行宿主）、ADR-0002（棋盘数据容器——snapshot 的静态底数 PurchasePrice/ColorGroup/MortgageValue 经 Board DA Holder 读取）、ADR-0007（BP-vs-C++ 边界——0007 是本 ADR 的设计前提，`const FGameStateSnapshot&` 签名已由 player-turn CR-8 冻结；非硬阻塞但拓扑序 0007 先于 0006，与 ADR-0007 的 Enables 0006 声明一致） |
| **Enables** | AI(10) 全部实现（CR-6 字段齐备是 AI 开工硬前提）；解锁 ai-opponent AC-5/6/7/48 从 CI-stub 转真实 `[Logic]` |
| **Blocks** | Epic「AI 对手」——AI 任一 hook 实现不能在本 ADR Accepted 前开工（字段缺失=AI 开工硬阻塞，ai-opponent CR-6） |
| **Ordering Note** | 须在 ADR-0001/0002 Accepted 之后；与 ADR-0007（BP-vs-C++ 边界）协同——ADR-0007 裁定 AI 决策核心落 C++，本 ADR 的 `const FGameStateSnapshot&` 签名即按 C++ 权威逻辑设计（架构 §8 依赖链 `ADR-0007 → ADR-0006`） |

## Context

### Problem Statement

AI(10) 是依赖图的纯叶子消费者——它要消费经济(5)/地产(6)/建房(8)/棋盘(1)/事件格(7) 的估值与归属事实来打分，但**绝不能直取这些系统的运行单例**（否则破坏「纯叶子、不被反调、可 headless 注入 mock」三条架构原则，architecture.md 原则3 DI-over-Singleton）。因此 AI 只能经一个**只读聚合视图** `FGameStateSnapshot` 间接读取。

OQ-AI-3 把以下三件事悬置为「AI 开工硬前提」：
1. **`FGameStateSnapshot` 的 UE struct/字段构成**——AI 的 CR-6 per-hook 字段清单是注入基准，但谁来定义这个 struct、含哪些字段、派生量在哪算，未裁定。
2. **装配时机与宿主**——snapshot 何时生成、由谁生成、是否在决策期内可变。
3. **只读性如何保证**——CR-1 要求「AI 从不写游戏状态」，但若 AI 拿到的是可写引用/各系统活指针，纪律靠约定而非类型系统强制。

不裁定的代价：ai-opponent 的 AC-5/6/7/48 被迫永久标 `[Logic·CI-stub]`（依赖未定义 struct），AI 无法开工，concept 唯一的单机验证路径（vs-AI）无法落地。

### Current State

- architecture.md §D.1a 已定方向：「AI 在自身决策阶段生成只读快照（读 1/3/5/6/7/8），含 `Rent_top1/top2`、`owner_map`、`starting_cash`、`board_tile_count` 等字段。snapshot 装配方/宿主待架构期 ADR（OQ-AI-3 → ADR-0006）」。
- player-turn CR-8 已定 hook 签名（player-turn 拥有，本 ADR 不改）：`bool DecideBuyProperty(const FGameStateSnapshot& S, const FPropertyData& P)` / `TArray<FTurnAction> DecidePostRollActions(const FGameStateSnapshot& S)` / `EJailAction DecideJailAction(const FGameStateSnapshot& S)`。三签名已用 `const FGameStateSnapshot&`——本 ADR 须将「为何 const&」从约定升为有据裁定。
- ai-opponent CR-6 已给 per-hook 实需字段清单 + 全盘暴露视图修复（`Rent_top1/top2` 三 hook 共需、owner map）。
- struct **尚不存在**——这正是本 ADR 要定义的产物。

### Constraints

- **技术（引擎）**：UE5.7 `USTRUCT` 值语义；UFUNCTION 按 `const&` 传 struct 避免深拷贝；`TMap` 不能直接作 `UPROPERTY` 的某些 BP 暴露场景须留意——但 snapshot 内部容器可不暴露给 BP（AI 决策核心落 C++，ADR-0007）。
- **架构（纯叶子/DI）**：snapshot 必须是**值快照**，不得内嵌任何系统的活指针/可写引用——否则 AI 可经引用反向触达系统状态，破坏纯叶子纪律与可测性。
- **确定性**：snapshot 在决策期内**冻结不变**——AI 的纯函数性 `(snapshot, RNG 流状态) → 决策`（CR-5②）要求同一决策期内多次读到的字段一致；若装配后中途变更，纯函数性塌陷、重放不可证。
- **性能**：snapshot 装配 + 传入是每个 AI 回合的固定开销（PC/Steam 60FPS / 16.6ms 帧预算）；按值深拷贝聚合 struct 会进 hot path（OQ-AI-3 unreal R-review B-1 点名）。
- **兼容**：须与 player-turn CR-8 已冻结的三签名逐字一致（本 ADR 不改签名，只为其背书并定义被传的类型）。

### Requirements

- **R1（字段齐备）**：`FGameStateSnapshot` 须覆盖 ai-opponent CR-6 三 hook 的全部实需字段——缺字段=AI 开工硬阻塞。
- **R2（只读强制）**：AI 经 snapshot 不得写任何游戏状态（满足 ai-opponent CR-1 / AC-1）——尽量靠类型系统而非约定强制。
- **R3（决策期冻结）**：snapshot 在一次 AI 决策阶段内不变（满足 CR-5② 纯函数性 / AC-42 重放）。
- **R4（一次性装配，明确宿主）**：装配时机与宿主单一、可审、可 headless 注入（满足 architecture 原则3 DI）。
- **R5（无深拷贝传参）**：hook 按 `const FGameStateSnapshot&` 传入（OQ-AI-3 unreal B-1）。
- **R6（派生量口径统一）**：`Rent_top1/top2`、`preaggregated_nlv` 等派生量预聚合 vs hook 内现算须二选一并钉死（OQ-AI-3 ②）。
- **R7（性能可证）**：snapshot 构造+传入端到端耗时须有一条 AC（OQ-AI-3 ③，当前 AC-56 只测 hook 内、AC-37b 是 Advisory）。

## Decision

**裁定一(struct 形态)**：定义 `FGameStateSnapshot` 为**值语义 `USTRUCT`**，仅含 POD/容器成员（int32 / bool / enum / `TArray<FTileSnapshotEntry>` / 预派生标量），**绝不内嵌任何系统的指针或可写引用**。AI 决策核心（C++，ADR-0007）通过 `const FGameStateSnapshot&` 接收，只读消费。

**裁定二(装配方+宿主)**：snapshot 由**回合2 编排在 AI 决策阶段开头一次性装配**（不是 AI 自建，不是各系统各自塞），装配逻辑挂在回合2 的状态机宿主（ADR-0001 的 `UWorldSubsystem`/回合 UObject）。回合2 在进入某 AI 玩家的决策阶段时，向 1/3/5/6/7/8 拉取当前事实、派生 `Rent_top1/top2`/`preaggregated_nlv`，组装出一份 `FGameStateSnapshot`，然后把它的 `const&` 喂给该 AI 的三 hook。

**裁定三(字段冻结)**：snapshot 一旦装配完成，在**该 AI 决策阶段内冻结**——同一决策阶段内 `DecideBuyProperty`/`DecidePostRollActions`/`DecideJailAction` 看到的是同一份不变视图。AI 内部的 `DecidePostRollActions` 贪心循环依序执行动作会改真实状态，但**贪心循环用的是 AI 内部预扣的局部影子值（CR-7 预扣现金），不回读 snapshot**——snapshot 本身不随执行更新。下一个 AI 决策阶段（或下一回合）重新装配新 snapshot。

### 逐选项列代价 + 裁定（已 Accepted）

> 下方三个决策点均已采纳推荐项为最终决策（A1 / B1 / C1）；备选 A2/A3、B2/B3、C2 的分析保留作记录。

#### 决策点 A：snapshot 宿主 / 装配方

| 选项 | 含义 | 代价 |
|---|---|---|
| **A1（推荐）回合2 编排装配** | 回合2 在 AI 决策阶段开头拉 1/3/5/6/7/8 → 派生 → 组装 `FGameStateSnapshot` → 以 `const&` 喂三 hook | ✅ 与 architecture §D.1a「AI 在自身决策阶段生成」一致(由回合2 为 AI 装配)；✅ 单一装配点、可审、可 headless 注入 mock snapshot；✅ AI 保持纯叶子(不直取任何系统单例)；⚠ 回合2 须知晓 AI 所需字段清单(耦合于 CR-6，但 CR-6 本就声明为「注入方对照基准」) |
| **A2 AI 自建 snapshot** | AI hook 内部自己向各系统拉数据组装 | ❌ AI 须直取 5/6/8 单例 → 破坏纯叶子(AI 被迫依赖系统活指针)；❌ headless 测试无法注入 mock(AI 内部硬连各系统)；❌ 违 architecture 原则3 DI、违 CR-1 无环纪律 |
| **A3 各系统各自向 snapshot 推送自己的切片** | 5/6/8 各自实现 `ContributeToSnapshot()` | ❌ 装配点分散、派生量(top-2 跨系统)无单一归属；⚠ owner map/top-2 是跨系统聚合，无单一系统能独立产出；❌ 引入「谁聚合 top-2」的新 owner 真空 |

**裁定 A1（Accepted）**：唯一既满足「AI 纯叶子」又有「单一可审装配点」的方案。代价(回合2 知晓字段清单)是可控的——CR-6 本就把字段清单声明为「向 OQ-1 ADR 注入方对照同步」，注入方=回合2 编排正是预期角色。

#### 决策点 B：派生量口径（`Rent_top1/top2` / `preaggregated_nlv` 预聚合 vs hook 内现算）

| 选项 | 含义 | 代价 |
|---|---|---|
| **B1（推荐）装配期预聚合标量** | 回合2 装配时算好 `Rent_top1`/`Rent_top2`/`preaggregated_nlv` 存入 snapshot 标量字段；AI 直接读 | ✅ AI 不可自调经济 F-9(防 5→8 反向环，economy Risk-3 已点名 `preaggregated_nlv` 必须预聚合)；✅ 口径单一、可在装配点测;✅ hook 内零聚合开销;⚠ 装配方须实现 top-2 选取逻辑(一次性，O(格数)) |
| **B2 提供 per-tile 原始数据，AI hook 内现算 top-2** | snapshot 给全盘 `{owner, house_count, is_mortgaged, ColorGroup}`，AI 自求 top-2 | ⚠ CR-6 把此列为「注入方可选其一」的退路；❌ `preaggregated_nlv` 明确**不可**让 AI 自算(防反向环)，故至少 nlv 必须 B1；❌ 三 hook 各自现算 top-2 重复开销 + 口径漂移风险 |
| **B3 混合** | nlv 预聚合，top-2 现算 | ⚠ 口径不统一(一半预聚一半现算)，违 OQ-AI-3 ②「统一口径」要求 |

**裁定 B1（Accepted，统一预聚合）**：满足 OQ-AI-3 ②「统一口径」、满足 economy Risk-3「`preaggregated_nlv` 必须预聚合防反向环」。snapshot 仍**可同时携带** per-tile 明细(供 BlockingValue/均衡建造等需要逐格的计算)，但**派生标量 top-2/nlv 一律装配期算好**——「现算」仅限那些本就 per-tile、无跨系统聚合的逐格读取(如某格 house_count)。

#### 决策点 C：只读强制手段

| 选项 | 含义 | 代价 |
|---|---|---|
| **C1（推荐）值语义 struct + const& 传参 + 无活指针成员** | snapshot 是纯值聚合，按 `const&` 传，内部无任何系统指针 | ✅ AI 物理上无法经 snapshot 触达可写状态(类型系统强制 R2)；✅ AC-1(写方法调用==0)由架构而非约定保障；✅ 满足 R5 无深拷贝；⚠ 须确保装配时拷入的是值快照而非引用 |
| **C2 传 AI 各系统的 const 接口指针** | 给 AI 一组 `const IEconomyRead*` 等只读接口 | ⚠ const 接口仍是活指针，AI 可在决策期读到中途变更的状态 → 违 R3 冻结；❌ headless mock 须实现整组接口，比构造值快照重 |

**裁定 C1（Accepted）**：值快照同时满足只读(R2)+冻结(R3)+无深拷贝传参(R5，因为传的是 `const&`)三项，且让「AI 不写状态」成为类型系统层面的事实而非纪律。

### Architecture

```
                 回合2 编排（UWorldSubsystem 宿主, ADR-0001）
                 │  进入 AI 玩家决策阶段
                 ▼
   ┌─────────── AssembleSnapshot()  ── 一次性, 装配期 ───────────┐
   │  读 棋盘1(经 Board DA, ADR-0002): PurchasePrice/Color/MV/board_tile_count_classic
   │  读 地产6: owner map / is_monopoly / is_mortgaged / station_count
   │  读 建房8: house_count / CanBuildHouse 谓词输入 / BuildingCost
   │  读 经济5: property_rent 派生 → Rent_top1/top2(预聚合);
   │           preaggregated_nlv(预聚合, 防 5→8 反向环); unmortgage_cost; starting_cash
   │  读 事件格7: bHasJailCard / JAIL_BAIL_AMOUNT / JailTurnsServed
   └────────────────────────────┬──────────────────────────────┘
                                ▼
                    FGameStateSnapshot  (值快照, 决策期冻结)
                                │  const&  (无深拷贝, 无活指针)
              ┌─────────────────┼─────────────────┐
              ▼                 ▼                 ▼
   DecideBuyProperty   DecidePostRollActions   DecideJailAction
        (只读)               (只读)                (只读)
              └──── return-only 决策 ───────────────┘
                                │
                                ▼
              回合2 执行返回的决策 → 改真实状态 (snapshot 不更新)
```

### Key Interfaces

```cpp
// 全盘 per-tile 明细条目（供需要逐格的计算：BlockingValue、均衡建造、owner map 遍历）
USTRUCT(BlueprintType)
struct FTileSnapshotEntry
{
    GENERATED_BODY()
    UPROPERTY() int32   TileIndex      = INDEX_NONE;
    UPROPERTY() int32   OwnerId        = INDEX_NONE;   // 全盘 owner map（算 BoardDensity / BlockingValue）
    UPROPERTY() int32   HouseCount     = 0;            // [0,5]（建房8 口径）
    UPROPERTY() bool    bIsMortgaged   = false;
    UPROPERTY() bool    bIsMonopoly    = false;        // 该格所属组是否垄断
    UPROPERTY() int32   ColorGroup     = INDEX_NONE;
    UPROPERTY() int32   PurchasePrice  = 0;            // 棋盘1 静态底数（经 Board DA）
    UPROPERTY() int32   MortgageValue  = 0;
    UPROPERTY() int32   BuildingCost   = 0;
    UPROPERTY() int32   UnmortgageCost = 0;            // =MortgageValue+ceil(MV/10)，经济5 口径
    UPROPERTY() int32   PreaggregatedNlv = 0;          // 净可变现值，回合2 装配期聚合 6/8（AI 不可自算，防 5→8 反向环）
};

// AI 唯一读取面。值语义、决策期冻结、按 const& 传入。无任何系统活指针。
USTRUCT(BlueprintType)
struct FGameStateSnapshot
{
    GENERATED_BODY()

    // —— 决策主体 AI 自身 ——
    UPROPERTY() int32 SelfPlayerId = INDEX_NONE;
    UPROPERTY() int32 SelfCash     = 0;

    // —— 全盘暴露视图（三 hook 共需，算 F-1 Buffer）——
    UPROPERTY() int32 Rent_top1 = 0;   // 全盘对手未抵押地产单次租金最高（piecewise property_rent 口径，预聚合）
    UPROPERTY() int32 Rent_top2 = 0;   // 次高；不足两块取现有，无对手地产=0

    // —— 全局常量/底数 ——
    UPROPERTY() int32 StartingCash           = 1500;  // F-2 LiquidationDiscount 分母
    UPROPERTY() int32 BoardTileCountClassic  = 40;    // F-6 BoardDensity 分母
    UPROPERTY() int32 JailBailAmount         = 50;    // JAIL_BAIL_AMOUNT
    UPROPERTY() int32 MaxJailTurns           = 3;

    // —— DecideJailAction 专属 ——
    UPROPERTY() bool  bHasJailCard   = false;
    UPROPERTY() int32 JailTurnsServed = 0;

    // —— 全盘 per-tile 明细（owner map / 逐格计算）——
    UPROPERTY() TArray<FTileSnapshotEntry> Tiles;
};

// player-turn CR-8 已冻结的三签名（本 ADR 不改，仅为其背书 const& 传参 + 定义被传类型）
bool                DecideBuyProperty(const FGameStateSnapshot& S, const FPropertyData& P);
TArray<FTurnAction> DecidePostRollActions(const FGameStateSnapshot& S);
EJailAction         DecideJailAction(const FGameStateSnapshot& S);
```

> **字段→CR-6 对照**（注入方校验清单）：`DecideBuyProperty` 用 SelfCash/Tiles(PurchasePrice/ColorGroup/owner/HeldInGroup 派生)/Rent_top1/top2/StartingCash；`DecidePostRollActions` 用 SelfCash/Tiles(HouseCount/bIsMortgaged/bIsMonopoly/BuildingCost/MortgageValue/UnmortgageCost/PreaggregatedNlv)/Rent_top1/top2；`DecideJailAction` 用 SelfCash/JailBailAmount/bHasJailCard/JailTurnsServed/Tiles(OwnerId 算 BoardDensity)/BoardTileCountClassic/Rent_top1/top2。HeldInGroup/GroupSize/OppMaxInGroup 由 AI 从 `Tiles` 现算(本就 per-tile 逐格、无跨系统聚合，符合 B1 例外)。

### Implementation Guidelines

1. **`AssembleSnapshot()` 落回合2 编排**(ADR-0001 宿主)，签名建议 `FGameStateSnapshot AssembleSnapshot(int32 AiPlayerId) const`——返回值快照，调用点立即以 `const&` 喂 hook(RVO/move，构造一次)。
2. **`Rent_top1/top2` 派生**：遍历全盘对手**未抵押**地产，按 piecewise `property_rent` 口径(垄断×2 仅作用 house=0 的 base)求每块当前单次租金，取前两高。垄断判定经地产6 `is_monopoly`。不足两块取现有之和，无对手地产=0(ai-opponent F-1 退化守卫)。
3. **`PreaggregatedNlv` 由回合2 装配期聚合 6/8**——**AI 严禁自算**(economy Risk-3 防 5→8 反向环)。
4. **冻结纪律**：`AssembleSnapshot` 返回后该 struct 即冻结；`DecidePostRollActions` 贪心循环的现金预扣用 AI **hook 内局部影子变量**(ai-opponent CR-5② / AC-62)，**不回写也不重读 snapshot**。
5. **DI/可测**：headless 测试构造 mock `FGameStateSnapshot` 字面量直接喂 hook(值语义天然可注入，无需 mock 任何系统接口)——这正是 AC-5/6/7 CI-stub 当前用 placeholder snapshot 的形态，ADR Accepted 后换真实装配方。
6. **BP 暴露**：AI 决策核心落 C++(ADR-0007)，snapshot 容器(`TArray<FTileSnapshotEntry>`)在 C++ 内消费即可；`BlueprintType` 保留以便调试/Alpha BP 旁路，但 AI 决策逻辑不依赖 BP 读 TMap。
7. **缺字段降级**：若装配方未填某必需字段(架构期注入缺陷)，对应 hook 降保守默认(false/[]/RollDouble)+ 单条 `UE_LOG`(ai-opponent AC-48)。

## Alternatives Considered

### Alternative 1: AI 自建 snapshot（直取各系统单例）

- **Description**：AI hook 内部直接 `GetWorld()->GetSubsystem<UEconomy>()` 等拉数据自组装。
- **Pros**：无需回合2 知晓字段清单；AI 自包含。
- **Cons**：AI 直依赖 5/6/8 活指针 → 破坏纯叶子(CR-1)、无法 headless 注入 mock(违原则3 DI)、AC-1/AC-3 spy 断言无法干净通过。
- **Estimated Effort**：与 A1 相当，但测试基建更重。
- **Rejection Reason**：违反纯叶子与 DI 两条架构红线，且使 AI 不可隔离测试。

### Alternative 2: 传 AI 一组 const 只读接口指针（而非值快照）

- **Description**：定义 `const IEconomyRead*` 等接口注入 AI。
- **Pros**：无需一次性拷贝全盘 per-tile。
- **Cons**：const 接口仍是活指针——决策期内状态可中途变更，违 R3 冻结、塌陷 CR-5② 纯函数性/AC-42 重放；headless 须 mock 整组接口，比构造值快照重。
- **Estimated Effort**：高于 C1(须设计并 mock 多个接口)。
- **Rejection Reason**：无法保证决策期冻结，破坏重放确定性。

### Alternative 3: 派生量全部 hook 内现算（不预聚合）

- **Description**：snapshot 只给原始 per-tile，top-2 与 nlv 全由 AI 现算。
- **Pros**：snapshot 装配薄。
- **Cons**：`preaggregated_nlv` 现算须 AI 触达经济 F-9 → 5→8 反向环(economy Risk-3 明确禁止)；三 hook 重复算 top-2、口径易漂移，违 OQ-AI-3 ②统一口径。
- **Rejection Reason**：nlv 现算违反反向环纪律，且口径不统一。

## Consequences

### Positive

- AI 保持纯叶子消费者——经值快照只读，物理上无法写状态或反调系统(类型系统强制 AC-1/AC-2/AC-3)。
- 单一可审装配点(回合2 `AssembleSnapshot`)，headless 测试可直接注入 mock 值快照——解锁 AC-5/6/7/48 从 CI-stub 转真实 `[Logic]`。
- 决策期冻结 + 值语义 → CR-5② 纯函数性与 AC-42 重放有结构保障。
- `const&` 传参消除聚合 struct 深拷贝(每 hook 调用零拷贝)。

### Negative

- 回合2 编排须知晓 AI 的 CR-6 字段清单——CR-6 字段增删须同步装配方(已由 CR-6「注入方对照基准」契约承接，propagate 债登记在 OQ-AI-3)。
- 全盘 per-tile 明细每决策阶段拷贝一次(经典棋盘 ≤40 格 × 小 POD struct)——量级极小但非零，须由性能 AC 证明在预算内。

### Neutral

- snapshot 同时承载预聚合标量(top-2/nlv)与 per-tile 明细——AI 用标量做 Buffer/nlv 门、用明细做逐格(BlockingValue/均衡)，分工清晰。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| 装配方漏填 CR-6 某字段 → AI 降保守默认静默退化 | 中 | 中 | AC-48 缺字段降级 + `UE_LOG` 诊断；字段→CR-6 对照表作注入校验清单 |
| `Rent_top1/top2` piecewise 口径与经济5 `property_rent` 漂移 | 低 | 中 | 派生逻辑直接调经济5 注册表 `property_rent`(不重定义)；AC-S5 口径回归 |
| 全盘 per-tile 拷贝进 AI hot path 超帧预算 | 低 | 低 | ≤40 格小 POD；AC-S6/S7 端到端耗时证明；`const&` 传参零拷贝 |
| snapshot 装配后被中途变更(误实现) → 破坏冻结 | 低 | 高 | 值语义 + 文档冻结纪律；贪心循环用 hook 内局部影子值(AC-62)，不回写 snapshot |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (snapshot 装配/AI 回合) | N/A（未实现） | ≤40 格 POD 拷贝 + top-2/nlv 聚合，预期 <0.1ms | 单 AI 回合(装配+三 hook)总计 ≤16ms(60FPS 单帧) |
| CPU (hook 传参) | N/A | 0（`const&` 引用传递，零拷贝） | — |
| Memory | N/A | `FGameStateSnapshot` ≈ 固定标量 + 40×`FTileSnapshotEntry`(~每条 ~48B) ≈ <2.5KB/快照，决策期单份、阶段结束释放 | 无专项上限，量级可忽略 |
| Load Time | — | 无影响（运行态 struct，不入存档——存档契约见 ADR-0005，snapshot 是瞬态禁存档） | — |

## Migration Plan

无既有系统迁移——`FGameStateSnapshot` 是新增 struct，AI 尚未实现。落地步骤：

1. ADR-0001/0002 Accepted 后，定义 `FGameStateSnapshot` + `FTileSnapshotEntry`(本 ADR Key Interfaces)。验证：struct 编译、`const&` 签名与 player-turn CR-8 逐字一致。
2. 在回合2 编排实现 `AssembleSnapshot(AiPlayerId)`：拉 1/3/5/6/7/8 → 派生 top-2/nlv → 组装。验证：单测装配出的 snapshot 字段值与已知 fixture 相等(AC-S5)。
3. ai-opponent AC-5/6/7/48 从 `[Logic·CI-stub]` 转真实 `[Logic]`——用真实装配方替换 placeholder snapshot。验证：四条 AC 在同一 sprint 内转绿、pending 横幅清除(OQ-AI-3 关闭条件)。
4. 加性能 AC-S6/S7(装配+传入端到端耗时)。验证：nightly 趋势记录。

**Rollback plan**：本 ADR 是新增契约，无回滚目标。若决策点 A1(回合2 装配)证伪(如回合2 耦合字段清单不可接受)，退而评估 A3(各系统贡献切片)+ 引入独立「snapshot 聚合服务」承接跨系统派生量——但须先解决「谁聚合 top-2/nlv」的 owner 真空。

## Validation Criteria

> 本 ADR 落地后新增/绑定的 AC（前缀 `AC-S`=snapshot 契约 AC，落 `tests/unit/core/gamestate-snapshot/` 与 ai-opponent 既有 AC 协同）：

- [ ] **AC-S1**（只读强制）`FGameStateSnapshot` 无任何系统指针/可写引用成员（code-review + 编译期；AI 经 snapshot 无法触达可写状态）。绑定 ai-opponent AC-1(写方法==0)。
- [ ] **AC-S2**（const& 传参）三 hook 签名为 `const FGameStateSnapshot&`，与 player-turn CR-8 逐字一致（编译期符号比对）。
- [ ] **AC-S3**（决策期冻结）同一决策阶段内连调三 hook，snapshot 字段逐元素不变（`DecidePostRollActions` 执行不更新 snapshot）。绑定 CR-5②/AC-42。
- [ ] **AC-S4**（字段齐备）`FGameStateSnapshot` 覆盖 CR-6 三 hook 全部实需字段（对照表逐字段核对，缺一即 FAIL）。
- [ ] **AC-S5**（派生口径回归）`AssembleSnapshot` 产出的 `Rent_top1/top2` 与经济5 `property_rent` piecewise 口径一致；`PreaggregatedNlv` 由装配方聚合（AI 路径无对经济 F-9 的调用，spy 验证）。
- [ ] **AC-S6**（性能·hook 内）最复杂 snapshot 下三 hook 决策耗时 P50≤16ms（绑定/复用 ai-opponent AC-56，nightly 趋势）。
- [ ] **AC-S7**（性能·端到端，闭合 OQ-AI-3 ③）`AssembleSnapshot` 构造 + 传入 + 三 hook 端到端耗时有机器证明（nightly，非 PR gate；填补 AC-56 只测 hook 内、AC-37b 仅 Advisory 的盲区）。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/ai-opponent.md` | AI 对手(10) | OQ-AI-3：`GameStateSnapshot` UE struct/字段构成（AI 开工硬前提） | 定义 `FGameStateSnapshot`/`FTileSnapshotEntry`，覆盖 CR-6 三 hook 全字段含全盘暴露视图 + owner map |
| `design/gdd/ai-opponent.md` | AI 对手(10) | CR-6 GameStateSnapshot 消费契约（只读，继承义务③） | 值语义 + `const&` 强制只读；字段→CR-6 对照表作注入校验清单 |
| `design/gdd/ai-opponent.md` | AI 对手(10) | CR-1/CR-5②：AI 不写状态 + 决策期纯函数 | 无活指针值快照(C1) + 决策期冻结(裁定三)→ 类型系统保障 AC-1、结构保障 AC-42 |
| `design/gdd/ai-opponent.md` | AI 对手(10) | AC-5/6/7/48 `[Logic·CI-stub]` 依赖 OQ-1 ADR | 本 ADR Accepted = CI-stub 关闭条件；四条同 sprint 转真实 `[Logic]` |
| `design/gdd/ai-opponent.md` | AI 对手(10) | OQ-AI-3 ①const&/②派生口径/③端到端性能 AC | ①C1 const&;②B1 预聚合统一口径(nlv/top-2);③AC-S7 端到端耗时 |
| `design/gdd/player-turn.md` | 玩家回合(2) | CR-8 三 hook 签名传 `const FGameStateSnapshot&` | 定义被传的 struct，背书 const& 选择，签名逐字一致(本 ADR 不改签名) |
| `docs/architecture/architecture.md` | §D.1a / §8 | snapshot 装配方/宿主待 ADR | 裁定回合2 编排装配(A1)、决策期冻结、值语义只读 |

## Related

- **依赖** ADR-0001（UObject 宿主——装配方运行宿主）、ADR-0002（棋盘数据容器——静态底数来源）。
- **协同** ADR-0007（BP-vs-C++ 边界——AI 决策核心落 C++，本 ADR const& 签名据此设计；架构 §8 依赖链 ADR-0007 → ADR-0006）。
- **协同** ADR-0003（去中心化 delegate——`OnAIActionExecuted` 由回合2 执行 AI 动作时广播，非 AI emit；snapshot 不涉事件）、ADR-0005（存档契约——snapshot 是瞬态禁存档）。
- **回链** ai-opponent.md OQ-AI-3 / OQ-AI-3b（BP-vs-C++，归 ADR-0007）/ CR-6 / AC-5/6/7/48 / AC-1/42/48/62。
- **代码**（实现后回填）：`FGameStateSnapshot` 定义位置、回合2 `AssembleSnapshot` 实现、`tests/unit/core/gamestate-snapshot/`。
