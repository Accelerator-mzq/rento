# ADR-0002: 棋盘数据容器（DataTable vs Primary Data Asset）

## Status

Accepted

> 2026-06-06 由 msc（用户）裁定，确认载体 = **Primary Data Asset**（`UBoardDataAsset : UPrimaryDataAsset`）+ `AdvanceIndex` 返回 `FBoardAdvanceResult` struct。引用本 ADR 的 story（移动4/所有权6/事件格7/建房8/地图编辑器26 实现）解锁。

## Date

2026-06-06 — 起草

## Last Verified

2026-06-06 — 起草时对照 UE5.7 DataTable CSV 导入限制（知识缺口 (a)）论证"CSV 驱动更优"不成立。

2026-06-06 — **Sprint0 引擎验证 spike**（`sprint0-engine-verification-2026-06-06.md`，源码 + 对抗复核）更正/确认三处：**① CSV TArray REFUTED**——`IsSupportedTableProperty` 白名单含 `FArrayProperty`（`DataTableUtils.cpp` L421），CSV 含 TArray 列**不报** `Unsupported Property type`；DataTable 不便属**实务层**（数组字面量 `(a,b,c)` 与逗号冲突、转义复杂），非类型层拒绝。**DA 选型不变**（实务 + 正向收益独立成立），实务可用性由 BD-001 编辑器验证坐实。**② UAssetManager 加载 ✅ CONFIRMED**（`GetPrimaryAssetObject`/`LoadPrimaryAsset`+`WaitUntilComplete` 稳定；新增 `FAssetManagerLoadParams&&` 重载宜优先用；`<100ms` 是工程目标非 API 保证）。**③ Floor 机制 REFUTED**——见决策③ 更正。

## Decision Makers

msc（用户）+ Technical Director（主笔）· unreal-specialist（引擎核验）— 2026-06-06 Accepted

## Summary

棋盘数据系统（#1 Foundation）的 `FBoardTileData` 每格结构含 `TArray<int32>` 列（`RentTable`/`DiceMultiplierTable`），须选定运行时载体：**DataTable**（行集合、CSV/JSON 导入）还是 **Primary Data Asset**（`UDataAsset` 子类 + 顶层字段）。本 ADR 裁定（Accepted 2026-06-06）采 **Primary Data Asset**："CSV 驱动经济调参更优"这一原本支持 DataTable 的论据因**实务限制**不成立——DataTable CSV 单元格数组字面量 `(a,b,c)` 与逗号分隔符冲突、嵌套 struct 转义复杂（2026-06-06 spike 实读 5.7 源码**更正**：`FArrayProperty` 在 `IsSupportedTableProperty` 白名单内，类型层并**不报** `Unsupported Property type`，故 CSV 不便是**实务层**而非类型限制）；而 Primary DA 天然支持顶层元数据（`bIsPlaceholderData`/`BoardIdentifier`/色键表）、嵌套数组与编辑器内 Details 面板编辑，更契合本系统需求。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core / Scripting（数据资产 + 资产加载） |
| **Knowledge Risk** | HIGH — LLM 知识 ~5.3，DataTable CSV/JSON 导入行为、`UAssetManager`/`FStreamableManager` 在 5.4-5.7 的 API 变化均 post-cutoff，须实查 |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`（5.4-5.7 知识缺口）；UE 官方论坛/文档关于 DataTable CSV `TArray` 列限制（见 §Related 链接） |
| **Post-Cutoff APIs Used** | `UPrimaryDataAsset`（5.7，LOW 风险，长期稳定）；`UAssetManager::GetPrimaryAssetObject` / `FStreamableManager::RequestAsyncLoad`（5.4+ 行为须实测，MEDIUM）；DataTable JSON 行导入（若选 DataTable，HIGH） |
| **Verification Required** | ① UE5.7 编辑器实测：DataTable CSV 导入含 `TArray<int32>` 列是否报 `Unsupported Property type`（**2026-06-06 spike REFUTED**：`FArrayProperty` 在 `IsSupportedTableProperty` 白名单、类型层不报此错；DataTable 不便属实务层〔数组字面量/逗号冲突〕，由 BD-001 编辑器实测坐实，DA 选型不变）；② `UAssetManager` 同步/异步加载 Primary DA 的 5.7 API 签名；③ Blueprint `Floor` 重载（关联 OQ-6(d)）**2026-06-06 spike REFUTED 机制**：5.7 `Floor`→int32 / `Floor to Integer64`→int64 入参**均 double**、差在节点名而非 float/double 推导，见决策③ |

> **Note**: Knowledge Risk = HIGH。本 ADR 依赖的 DataTable CSV `TArray` 限制与 `UAssetManager` 加载 API 若在引擎升级后变化，须重新核验并视情 Superseded。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001（UObject 宿主与生命周期边界）— 本 ADR 的"运行时 DA 持有者"宿主类型（`UWorldSubsystem` vs `UGameInstanceSubsystem` vs `GameState Component`）由 ADR-0001 统一裁定；本 ADR 只裁定**容器形态**，引用 ADR-0001 的持有者裁定，不重复裁宿主 |
| **Enables** | 移动(4)/所有权(6)/事件格(7)/建房(8) 的实现 story（硬依赖 `GetTileData`/`GetTilesInGroup`/`AdvanceIndex` 接口形态）；ADR-0006（GameStateSnapshot，依赖 0001/0002） |
| **Blocks** | 棋盘数据(#1) 实现 story；移动(4)/所有权(6)/事件格(7)/建房(8) 实现 story（OQ-6 自陈"#4/6/7/8 实现前必须 Accepted"）；地图编辑器(26) 导入方案 |
| **Ordering Note** | ADR-0001 必须先 Accepted（持有者宿主是本 ADR 加载/释放协议的前提）。本 ADR 与 0003/0004/0005 可在 0001 Accepted 后并行起草（互不触同文件） |

## Context

### Problem Statement

棋盘数据系统是设计顺序第 1 位的零依赖 Foundation 系统，有 **7+ 个上层系统硬依赖**它且均未成形。OQ-6 被标为 **BLOCKING-ADR**：必须在 #4/6/7/8 实现开工前裁定 `FBoardTileData` 的运行时载体。两个候选载体（DataTable / Primary Data Asset）在 `GetTileData`/`GetBoardId` 取值机制、异步加载协议、地图编辑器(26) 的 CSV 导入可行性、`[Config]` 测试 harness 形态上**差异显著**——下游硬依赖无法在此决策前安全开工。不裁定的代价：7 个系统的接口契约悬空，story 全部阻塞。

### Current State

棋盘数据 GDD（board-data.md，R3 Approved）已锁定：
- 每格扁平结构 `FBoardTileData`（决策一：扁平 struct，含 `RentTable: TArray<int32>`、`DiceMultiplierTable: TArray<int32>`）；
- 棋盘级元数据**另存于 DA 资产顶层**：`BoardIdentifier: FName`、`EColorGroup→hex` 色键表、`bIsPlaceholderData: bool`（CR-3/CR-6）；
- 首份实例 `DA_Board_Classic40`，art bible §8.2 明确"棋盘配置用 Data Asset（`DA_` 前缀）驱动"；
- `GetBoardId()` 返回 DA 内**显式 `BoardIdentifier` 字段**，与资产路径/`PrimaryAssetId` 解耦（AC-28）；
- 加载期一次性校验（`Validated` 状态），失败返回结构化错误码并拒绝进入对局（`LoadFailed`）。

但 GDD 显式将"DataTable 还是 Primary Data Asset"**派发给 OQ-6 ADR**，并登记知识缺口 (a)-(d)（DataTable CSV 是否支持 `TArray` 列等）尚未核验。当前 `technical-preferences.md` 的 Architecture Decisions Log 为空，无既有载体决策。

### Constraints

- **技术**：UE5.7。DataTable CSV 调参 TArray 列受**实务限制**（单元格数组字面量 `(a,b,c)` 与逗号分隔符冲突、转义复杂；2026-06-06 spike 更正：类型层 `FArrayProperty` 受支持、不报 `Unsupported Property type`，可用性由 BD-001 编辑器实测坐实）；引擎默认 DataTable 是"行集合"，**无顶层/头部字段**概念，棋盘级元数据无处天然落位。
- **引擎知识缺口（HIGH）**：LLM 知识 ~5.3，DataTable 导入行为/`UAssetManager` API 为 post-cutoff，必须实查而非臆造。
- **兼容性**：必须支持 `GetBoardId()` 与资产路径解耦（AC-28）；必须能承载顶层元数据三件套；接口契约一旦发布只增不改语义（GDD §Interactions 接口稳定承诺）。
- **资源**：本项目 Blueprint 为主、C++ 为框架层；策划/地图编辑器作者需可视化编辑棋盘数据（非纯文本）。
- **时间线**：本 ADR 阻断 #4/6/7/8 全部实现 story，属 Sprint 0 开工硬前提。

### Requirements

- **R1（功能）**：载体须无损承载 `FBoardTileData` 全字段，**含 `TArray<int32>` 列**（`RentTable` 长 6/4、`DiceMultiplierTable` 长 2）。
- **R2（功能）**：载体须承载**棋盘级顶层元数据**（`BoardIdentifier`/色键表/`bIsPlaceholderData`），且 `bIsPlaceholderData` 须可被 cook 前的 gate-check 检测（CI/构建门控语义）。
- **R3（功能）**：`GetBoardId()` 取值与资产路径/文件名/`PrimaryAssetId` **解耦**（AC-28），资产改名/移动不损坏旧存档。
- **R4（功能）**：地图编辑器(26) 须能产出符合 schema 的新棋盘实例（读写）；设计师须能输入 `RentTable` 的多个值。
- **R5（性能）**：单棋盘资产体量极小（40 格 × ~10 字段，~数 KB），加载在对局初始化一次性完成，**载入时间预算 < 100ms**（远低于任何瓶颈）；运行时只读访问 `GetTileData(index)` 须 O(1) 或近 O(1)。
- **R6（可测）**：`[Config]` 资产校验测试须有清晰 harness（commandlet/编辑器环境）；`[Logic]` schema 测试用代码构造 fixture（headless 可跑，与载体形态解耦）。
- **R7（可逆）**：若未来需迁移载体，迁移成本须可控（接口层 `GetTileData` 抽象隔离载体细节）。

## Decision

**采用 Primary Data Asset（`UPrimaryDataAsset` 子类）作为 `FBoardTileData` 的运行时载体。**

棋盘定义为一个 `UBoardDataAsset : public UPrimaryDataAsset`，顶层持有元数据字段 + 一个 `TArray<FBoardTileData> Tiles`。运行时由 ADR-0001 裁定的持有者宿主（per board-data OQ-6 推荐 `UGameInstanceSubsystem`，最终宿主由 ADR-0001 统一裁）以 `UPROPERTY` 强引用持有，防 GC。`GetTileData`/`GetTilesInGroup`/`AdvanceIndex` 由持有者（或其委托的 `UBoardMathLibrary`）暴露为只读查询接口。

> **逐选项代价分析见 §Alternatives Considered。** 下面先给推荐项的决策依据与接口契约。

### 推荐理由（为何 Primary Data Asset）

1. **`TArray` 列编辑体验 + 顶层元数据（主论据）。** `FBoardTileData` 含两个 `TArray<int32>` 列。**2026-06-06 spike 更正**：DataTable CSV 在**类型层支持** `FArrayProperty`（`IsSupportedTableProperty` 白名单含之，**不报** `Unsupported Property type`），但 CSV **实务上**调参 TArray 受限——单元格数组字面量 `(a,b,c)` 与 CSV 逗号分隔符冲突、嵌套 struct 转义复杂，须改 **JSON 行**手写（易错、`FText` 本地化 key 控制受限 NSLOCTEXT 不可用，OQ-7 张力）。Primary DA 在 Details 面板 `+` 添加 `TArray` 元素的可视化编辑**无此实务不便**，是相对 DataTable 的实质编辑体验优势。（实务可用性由 BD-001 编辑器验证 AC 坐实。）
2. **顶层元数据天然落位（R2）。** Primary DA 是单个 `UObject`，`BoardIdentifier`/色键表/`bIsPlaceholderData` 直接作为资产**顶层 `UPROPERTY`**。DataTable 无"顶层"概念（资产=行集合，无头部字段），三件元数据须**另建一个包装 `UDataAsset` 持有**——等于"DataTable + 一个 DA"两个资产，比纯 Primary DA 方案更复杂，且 `GetBoardId()` 要跨两个资产取值。
3. **`bIsPlaceholderData` 的 cook 门控更干净（R2）。** 建议 `bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离，gate-check 在 **cook 前**用 commandlet 读取资产顶层字段检测。Primary DA 顶层字段可被 `UAssetManager` 直接枚举扫描；DataTable 方案下该字段藏在包装 DA 里，扫描路径更绕。
4. **编辑器内可视化编辑（R4）。** Primary DA 在编辑器 Details 面板直接编辑每格（含 `TArray` 的 `+` 添加元素 UI），策划无需碰 JSON。地图编辑器(26) 可基于此 DA 做自定义编辑 UI，或直接复用 Details 面板。
5. **`GetBoardId` 解耦无障碍（R3）。** `BoardIdentifier: FName` 作为顶层 `UPROPERTY` 由作者手填，与 `PrimaryAssetId`/路径解耦——满足 AC-28，且 `FName` case-insensitive 陷阱（AC-28 R3 注）在两方案下同样存在，非选型差异点。

> **代价（诚实声明）：** Primary DA 牺牲了"40 格在一张表里一屏纵览/批量编辑"的体验（DataTable Row Editor 的强项）。对**经济调参批量改租金**场景，Details 面板逐格展开不如表格直观。**缓解**：① 经济调参由经济(5)产出真实曲线后**一次性写入**，非高频逐格手调（GDD Tuning Knobs「变更发起方锁定」：字段变更由经济5发起、棋盘1执行写入）；② 可提供编辑器工具脚本（commandlet）从经济(5)的数值表批量回填 DA。此代价在本项目工作流下影响有限。

### Architecture

```
                    设计期 (Authored)
   ┌──────────────────────────────────────────────┐
   │  UBoardDataAsset : UPrimaryDataAsset          │
   │  ├─ BoardIdentifier : FName  ("Classic40")    │  ← 顶层元数据
   │  ├─ bIsPlaceholderData : bool (EditorOnly)    │  ← cook 门控
   │  ├─ ColorKeyTable : TMap<EColorGroup, FColor> │  ← 色键表
   │  └─ Tiles : TArray<FBoardTileData>            │  ← 每格数据(含 TArray 列)
   └──────────────────────────────────────────────┘
                          │ 加载
                          ▼
        ADR-0001 裁定的持有者宿主 (推荐 UGameInstanceSubsystem)
        ┌──────────────────────────────────────────┐
        │  UPROPERTY() UBoardDataAsset* LoadedBoard │  ← 强引用防 GC
        │  Initialize(): UAssetManager 同步/异步加载 │
        │  Deinitialize(): CancelHandle/ReleaseHandle│  ← 防 PIE Stop 空棋盘
        │  Loaded → Validated → Active(read-only)    │
        └──────────────────────────────────────────┘
                          │ 只读查询
        ┌─────────────────┼──────────────────┐
        ▼                 ▼                  ▼
   移动(4)            所有权(6)/建房(8)      事件格(7)/HUD(16/17)
   GetTileCount       GetTileData            GetTileData
   AdvanceIndex       GetTilesInGroup        (EventDeck/TaxAmount...)
   GetTileData(0)
   .SalaryAmount

   F-1/F-2/F-3 拓扑算法 → 封装为 UBoardMathLibrary (BlueprintPure)
   (最终签名/封装与否由本 ADR §Decision ③ 裁定，见下)
```

### Key Interfaces

```cpp
// 每格数据（GDD CR-3 扁平 struct，载体无关）
USTRUCT(BlueprintType)
struct FBoardTileData
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) int32        TileIndex = 0;
    UPROPERTY(EditDefaultsOnly) ETileType    TileType = ETileType::Property;
    UPROPERTY(EditDefaultsOnly) FText        DisplayName;
    UPROPERTY(EditDefaultsOnly) EColorGroup  ColorGroup = EColorGroup::None;
    UPROPERTY(EditDefaultsOnly) int32        PurchasePrice = 0;
    UPROPERTY(EditDefaultsOnly) TArray<int32> RentTable;            // Property=6 / Railroad=4
    UPROPERTY(EditDefaultsOnly) TArray<int32> DiceMultiplierTable;  // Utility=2
    UPROPERTY(EditDefaultsOnly) int32        BuildingCost = 0;
    UPROPERTY(EditDefaultsOnly) int32        MortgageValue = 0;
    UPROPERTY(EditDefaultsOnly) int32        TaxAmount = 0;
    UPROPERTY(EditDefaultsOnly) int32        SalaryAmount = 0;      // 仅 Go 格
    UPROPERTY(EditDefaultsOnly) EEventDeck   EventDeck = EEventDeck::None;
    UPROPERTY(EditDefaultsOnly) ETileAction  SpecialAction = ETileAction::None;
};

// 顶层载体（Primary Data Asset）
UCLASS(BlueprintType)
class UBoardDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) FName BoardIdentifier;             // 与路径解耦, AC-28
    UPROPERTY(EditDefaultsOnly) TMap<EColorGroup, FColor> ColorKeyTable; // CR-5.1
    UPROPERTY(EditDefaultsOnly) TArray<FBoardTileData> Tiles;
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditDefaultsOnly) bool bIsPlaceholderData = true;    // CR-6 cook 门控
#endif
};

// 单一原子接口（GDD Interactions 原子性契约 / AC-37a/b）
// 决策 ③：本 ADR 选定 struct 返回形态（见下方裁定）
USTRUCT(BlueprintType)
struct FBoardAdvanceResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 NewIndex = 0;
    UPROPERTY(BlueprintReadOnly) int32 PassedGo = 0;
};

// F-1/F-2 封装为纯函数库（决策 ③：BlueprintPure 工具库，单测直测无需 BP 运行环境）
UCLASS()
class UBoardMathLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure) static FBoardAdvanceResult AdvanceIndex(int32 From, int32 Steps, int32 N);
    UFUNCTION(BlueprintPure) static int32 StepsBetween(int32 From, int32 Target, int32 N); // F-3
    // 注: 无独立公开 PassedGoCount 接口 (AC-37b: PassedGo 仅为 AdvanceIndex 返回分量)
};
```

### 决策 ③ — AdvanceIndex 签名与封装（裁定 OQ-6 ③）

- **裁定**：F-1/F-2 **封装为 `UBoardMathLibrary` 的 `BlueprintPure` 静态函数**；`AdvanceIndex` 返回 **`FBoardAdvanceResult` struct**（非 out-param）。
- **struct 返回 vs out-param 维度的裁定理由**：
  - **struct 返回（选定）**：Blueprint 用 Break Struct 拆 `NewIndex`/`PassedGo`，两值由单一节点单次返回，**强制原子性**（AC-37a），单测构造/断言整 struct 最干净，且天然满足 GDD「不存在独立 PassedGoCount 可分离调用」契约（AC-37b）。
  - **out-param（`int32 AdvanceIndex(..., int32& OutPassedGo)`）**：是 Epic 自身惯用法、Blueprint 双输出 pin 更紧凑，但 out-param 的 `PassedGo` 在 C++ 侧可被单独忽略/解读，原子契约的"两值同源"靠纪律而非类型强制——与 GDD 反分离意图略有张力。
  - **结论**：本系统**原子性是硬契约**（联网回放一致性，GDD Interactions），struct 返回让"两值一次返回"成为类型层事实，优于 out-param 的纪律约束。代价：Blueprint 多一个 Break 节点，可忽略。
- **关联实现层指针**：`UBoardMathLibrary` 内 F-1 用 `((A%N)+N)%N`、F-2 用通用式 + 显式选 `Floor`(→int32) 节点（GDD Blueprint 实现约束）——属实现细节，本 ADR 不复述，由实现 story 遵 GDD 约束。**2026-06-06 spike 更正 Floor 机制**：5.7 `KismetMathLibrary` 中 `Floor`(→int32) 与 `Floor to Integer64`(→int64) **入参均为 `double`**，差在 DisplayName + 返回类型（**非** float/double 两条入参推导路径）；风险是误选 int32 的 `Floor` 处理需 int64 的值 → 在 `static_cast<int32>` 截断（`KismetMathLibrary.inl` L709）。棋盘 32 格用 int32 `Floor` 节点本就安全。

### Implementation Guidelines

1. **持有者宿主取 ADR-0001 裁定结果**（board OQ-6 推荐 `UGameInstanceSubsystem`，最终以 ADR-0001 为准）。本 ADR 不重复裁宿主，只约定：持有者以 `UPROPERTY UBoardDataAsset*` 强引用防 GC；`Initialize()` 内若用异步加载，handle 须在 `Deinitialize()` 显式 `CancelHandle`/`ReleaseHandle`（防 PIE Stop 空棋盘，OQ-6 R3 补充 / ADR-0001 关键约束）。
2. **加载方式**：单棋盘资产 ~数 KB，建议 **`UAssetManager` 同步加载**（`GetPrimaryAssetObject` 或 `LoadPrimaryAsset` + 等待）即可满足 R5（<100ms）；异步仅在未来多棋盘预加载场景考虑。具体 5.7 API 签名须实测（知识缺口 (c)）。
3. **校验**：加载后跑 GDD §Edge Cases 全部拒绝/警告校验（`Validated` 状态），失败返回结构化错误码（`BoardTooSmall`/`Index0NotGo`/…）并 `LoadFailed`，不进入 Active。
4. **热切换**：换盘走完整 `Loaded→Validated→Active` 重初始化，**不在 Active 对局中热替换 DA**（GDD 热切换边界）。
5. **接口隔离（R7 可逆性）**：所有下游经持有者的 `GetTileData`/`GetTilesInGroup` 访问，**不得各自硬引用 `UBoardDataAsset`**——若未来迁移载体，仅改持有者内部取值，下游接口不变。
6. **地图编辑器(26)（R4）**：基于 Details 面板或自定义编辑器 UI 产出新 `UBoardDataAsset` 实例；`RentTable` 多值通过 Details 面板 `TArray` 元素 UI 输入（无 CSV `TArray` 列限制问题）。

## Alternatives Considered

### Alternative 1: Primary Data Asset（`UPrimaryDataAsset`）— 【选定】

- **描述**：棋盘=单个 `UBoardDataAsset : UPrimaryDataAsset`，顶层持元数据 + `TArray<FBoardTileData> Tiles`，编辑器 Details 面板编辑，`UAssetManager` 加载。
- **Pros**：
  - `TArray<int32>` 列**原生支持**（Details 面板 `+` 添加元素），无 CSV 限制；
  - 顶层元数据（`BoardIdentifier`/色键表/`bIsPlaceholderData`）天然落位，无需第二个资产；
  - `bIsPlaceholderData` 可 `WITH_EDITORONLY_DATA` + cook 前 commandlet 扫描，门控干净；
  - 编辑器可视化编辑，策划/编辑器作者无需碰文本格式；
  - art bible §8.2 明示"棋盘配置用 Data Asset（`DA_` 前缀）"——与既有美术契约一致。
- **Cons**：
  - 失去 DataTable Row Editor 的"一屏纵览 40 格/表格批量编辑"体验（经济批量调参略不便，但变更低频且可脚本回填，已缓解）；
  - 无 CSV 文件外部 diff 友好性（资产是二进制 `.uasset`）——但 DataTable 若被迫用 JSON 行同样牺牲 CSV 优势。
- **Estimated Effort**：基准（1.0×）。
- **采纳理由**：见 §Decision 推荐理由。`TArray` 列限制是决定性论据；顶层元数据、cook 门控、可视化编辑全面契合需求。

### Alternative 2: DataTable（`UDataTable`，CSV 或 JSON 行导入）— 拒绝

- **描述**：棋盘=`UDataTable`（行结构 `FBoardTileData`），CSV/JSON 导入；另建包装 `UDataAsset` 持顶层元数据；`GetTileData` 经 `FindRow`。
- **Pros**：
  - DataTable Row Editor 表格视图，40 格一屏纵览，批量编辑直观；
  - **若** CSV 支持，则经济调参可在 Excel/表格软件编辑后导入（外部 diff 友好）。
- **Cons**：
  - **CSV 调参 `TArray` 列实务受限**（2026-06-06 spike 更正：类型层 `FArrayProperty` 受支持、不报 `Unsupported Property type`，但单元格数组字面量 `(a,b,c)` 与逗号分隔符冲突、转义复杂）——`RentTable`/`DiceMultiplierTable` 实务上须改 JSON 行手写、易错、丧失表格软件优势，**"CSV 调参更优"不成立**；
  - **无顶层概念**——元数据三件套须**另建包装 `UDataAsset`**（两个资产），`GetBoardId()` 跨资产取值，`bIsPlaceholderData` cook 扫描路径更绕；
  - JSON 行内 `FText` 的 localization key 无法用 `NSLOCTEXT`（OQ-7 张力加剧）；
  - 行 `RowName` 与 `BoardIdentifier` 概念易混，须刻意保持 `GetBoardId` 取包装 DA 字段而非 RowName/路径。
- **Estimated Effort**：1.3×（包装 DA + JSON 导入工具 + 跨资产取值）。
- **Rejection Reason**：核心卖点（CSV `TArray` 调参）因**实务限制**（数组字面量/逗号冲突、被迫 JSON 手写）不成立后，相对 Primary DA 只剩"表格纵览"一项软优势，却引入两资产复杂度、顶层元数据绕路、本地化 key 受限三项实质成本。不划算。

### Alternative 3: 纯 C++ 硬编码 / 配置文件（`.ini`/`.json` 自解析）— 拒绝

- **描述**：棋盘布局写死在 C++ 表，或自定义 `.json` 由运行时 C++ 解析填 `FBoardTileData`。
- **Pros**：完全掌控加载/解析；无引擎导入限制。
- **Cons**：
  - **违反 coding-standards 硬线**「gameplay 数值外置、绝不硬编码」（C++ 硬编码方案直接出局）；
  - 自解析 JSON 重复造轮子（引擎 DA/DataTable 已有序列化/编辑器集成）；
  - 失去编辑器可视化、地图编辑器(26) 无 schema 复用、`[Config]` 资产测试无 harness。
- **Estimated Effort**：1.5×+（自建解析/校验/编辑工具）。
- **Rejection Reason**：违反数据驱动硬线 + 重复引擎已有能力，无任何相对收益。

## Consequences

### Positive

- OQ-6 BLOCKING-ADR 的容器分量解除，#4/6/7/8 接口契约稳定，可开工。
- 顶层元数据三件套有干净落位；`bIsPlaceholderData` cook 门控（AC-4c）可机器验证。
- `TArray` 列、`GetBoardId` 解耦、可视化编辑、art bible §8.2 一致——全面满足 R1-R4。
- `AdvanceIndex` 选定 struct 返回，原子性成为类型层事实（AC-37a/b 强化）。

### Negative

- 经济批量调参在 Details 面板逐格展开不如表格直观（已缓解：低频 + 脚本回填）。
- 资产为二进制 `.uasset`，无 CSV 文本 diff（DataTable 走 JSON 同样牺牲此点，非净损失）。
- 持有者宿主决策仍依赖 ADR-0001——本 ADR 在 ADR-0001 Accepted 前不可完全落地（已在 Dependencies 声明）。

### Neutral

- `FBoardTileData` struct 本身两方案通用，未来若迁移载体，struct 不变。
- 知识缺口 (b)/(d)（FText 本地化 key、Blueprint Floor 重载）部分转入 OQ-7 与实现 story，本 ADR 仅指针。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| UE5.7 `UAssetManager` 加载 API 与 ~5.3 知识不符，实现期返工 | Medium | Low | 加载逻辑封装在持有者一处；实现前实测 API 签名（知识缺口 (c)）；资产小可退化为最简同步加载 |
| `WITH_EDITORONLY_DATA` 剥离 `bIsPlaceholderData` 后 Shipping 运行时无法读该字段 | Low | Medium | 设计即如此：cook 门控在 cook **前** commandlet 读取，Shipping 运行时本就不需要该字段；gate-check 跑在 cook 前 |
| 经济调参嫌 Details 面板低效，事后想回 DataTable | Low | Medium | 接口层 `GetTileData` 隔离载体（R7）；提供经济数值表→DA 批量回填 commandlet，消除手调痛点 |
| 持有者宿主 ADR-0001 最终选 World 边界而非 GameInstance，影响加载时机 | Medium | Low | 本 ADR 只约定"强引用防 GC + Deinitialize 取消 handle"，对三种宿主均成立；具体时机随 ADR-0001 |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | N/A（运行时只读，无逐帧成本） | ~0ms（`GetTileData` 为 `Tiles[index]` O(1) 数组访问） | 16.6ms（无占用） |
| Memory | N/A | < 0.1MB（40 格 × ~10 字段 + 两小数组，单棋盘 ~数 KB） | 待目标硬件定（远低于任何上限） |
| Load Time | N/A | < 100ms（对局初始化一次性同步加载 + 校验） | < 100ms |
| Network | N/A（MVP 不联网；Full Vision 存 DA 引用非全量布局） | N/A | N/A |

## Migration Plan

无既有系统迁移（首份载体决策，Architecture Decisions Log 此前为空）。落地步骤：

1. 定义 `FBoardTileData` struct + `UBoardDataAsset : UPrimaryDataAsset`（含顶层元数据）。验证：编辑器可创建 `DA_Board_Classic40` 实例并填入 `TArray` 列。
2. 实现持有者宿主（取 ADR-0001 裁定）的加载/校验/释放（`Initialize`/`Deinitialize` + handle 取消）。验证：PIE 反复 Start/Stop 无空棋盘（AC 隐含）。
3. 实现 `GetTileData`/`GetTilesInGroup`（显式 Sort，AC-30）/`GetBoardId`（取 `BoardIdentifier`，AC-28）；`UBoardMathLibrary::AdvanceIndex`/`StepsBetween`。验证：`[Logic]` fixture 测试 AC-1/2/8~17/26~37a 跑绿。
4. temp-fill `DA_Board_Classic40` 经典公开参考值，`bIsPlaceholderData=true`。验证：`[Config]` AC-3/4-asset/4b/5/6/7/7b 跑绿（nightly）。

**Rollback plan**：若 Primary DA 证明不可行（极低概率），因 `FBoardTileData` struct 载体无关且 `GetTileData` 接口隔离，可改持有者内部从 DataTable `FindRow` 取值，下游接口零改动——这正是 R7 接口隔离的价值。

## Validation Criteria

- [ ] `DA_Board_Classic40` 可在 UE5.7 编辑器创建，`RentTable`(6)/`DiceMultiplierTable`(2) 经 Details 面板填入无报错（证 `TArray` 列原生支持）。
- [ ] 顶层 `BoardIdentifier`/`ColorKeyTable`/`bIsPlaceholderData` 三字段在资产顶层可编辑。
- [ ] `GetBoardId()` 返回 `BoardIdentifier` 字段值，资产改名后返回值不变（AC-28 解耦）。
- [ ] 持有者 PIE 反复 Start/Stop 后第二次仍正确加载棋盘（handle 取消生效，无空棋盘）。
- [ ] `[Logic]` 公式 fixture 测试（AC-8~17/34/37a）在 `-nullrhi` headless 跑绿——证载体与逻辑解耦。
- [ ] cook 前 commandlet 能读取 `bIsPlaceholderData` 实现 Alpha gate（AC-4c）。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/board-data.md` | 棋盘数据(#1) | **OQ-6 ⚠ BLOCKING-ADR**：DataTable vs Primary Data Asset 载体裁定 | 选定 Primary Data Asset（`UBoardDataAsset`）；DataTable 因 CSV 数组调参**实务受限**（非类型拒绝——2026-06-06 spike 更正）+ Primary DA 正向收益胜出 |
| `design/gdd/board-data.md` | 棋盘数据(#1) | CR-3：`FBoardTileData` 扁平 struct 含 `TArray<int32>` 列 | Primary DA 顶层 `TArray<FBoardTileData> Tiles`，`TArray` 列原生支持 |
| `design/gdd/board-data.md` | 棋盘数据(#1) | CR-3/CR-6：棋盘级顶层元数据 `BoardIdentifier`/色键表/`bIsPlaceholderData` | Primary DA 顶层 `UPROPERTY`；`bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` + cook 前扫描 |
| `design/gdd/board-data.md` | 棋盘数据(#1) | AC-28：`GetBoardId()` 与资产路径/`PrimaryAssetId` 解耦 | `BoardIdentifier: FName` 顶层字段，作者手填，取值不派生自路径 |
| `design/gdd/board-data.md` | 棋盘数据(#1) | OQ-6 ③：`AdvanceIndex` 最终签名 + F-1/F-2 是否封装 `UBoardMathLibrary` | 裁定：封装为 `UBoardMathLibrary` `BlueprintPure`，`AdvanceIndex` 返回 `FBoardAdvanceResult` struct（强制原子性 AC-37a/b） |
| `design/gdd/board-data.md` | 棋盘数据(#1) | OQ-6 ⑤：元数据落位 + cook 门控 | Primary DA 顶层天然落位；cook 前 commandlet 检测 `bIsPlaceholderData`（AC-4c）|
| `design/gdd/board-data.md` | 棋盘数据(#1) | OQ-6 ⑥ / OQ-7：地图编辑器多值输入 + FText 本地化 key | Details 面板 `TArray` 元素 UI 输入多值（无 CSV 限制）；FText 本地化 key 张力部分转 OQ-7 实现 story（本 ADR 指针，因 Primary DA 不强制 JSON 行，NSLOCTEXT 在 C++ CDO 仍可用） |
| `docs/architecture/architecture.md` | §8 Required ADR | ADR-0002 覆盖 board OQ-6 + OQ-7；依赖 ADR-0001（持有者宿主） | 本 ADR 裁容器形态，引用 ADR-0001 裁持有者宿主，二者职责切分清晰 |

## Related

- **依赖**：[ADR-0001 — UObject 宿主与生命周期边界](./architecture.md)（§8，持有者宿主裁定的前提）
- **被使能**：ADR-0006（GameStateSnapshot，依赖 0001/0002）
- **GDD**：`design/gdd/board-data.md`（OQ-6/OQ-7、`FBoardTileData`、AC-28/37a/37b/4c）
- **总纲**：`docs/architecture/architecture.md` §8 Required ADR 清单 + 依赖拓扑（ADR-0001→0002）
- **引擎核验来源**（DataTable CSV 不支持 `TArray` 列）：
  - [Importing datatable with array struct member — Epic Developer Community Forums](https://forums.unrealengine.com/t/importing-datatable-with-array-struct-member/309979)
  - [How to Use Data Tables in Unreal Engine 5: A Complete Guide — Quod Soler](https://www.quodsoler.com/blog/using-datatables-to-store-game-data)
  - [unreal.DataTable — Unreal Python 5.4 documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/DataTable?application_version=5.4)
- 实现后回链：`UBoardDataAsset` / `UBoardMathLibrary` 代码文件（待实现）
