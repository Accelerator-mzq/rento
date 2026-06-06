# ADR-0005: 存档序列化契约（Save Serialization Contract）

## Status

Accepted

> 2026-06-06 由 msc（用户）裁定三分叉：A=A1 `UPROPERTY(SaveGame)` 反射、B=B1 MVP 同步写盘、C=C1 CRC32；
> 容器 = `USaveGame` 子类 + 四重完整性门 + 存 DA 引用不存布局 + 枚举 append-only + MVP 单槽。
> 依赖 ADR-0001/0003/0004 均已 2026-06-06 Accepted。引用本 ADR 的存档 story 解锁。

## Date

2026-06-06 — 由 Technical Director 起草

## Last Verified

2026-06-06 — 对照 UE5.7 engine-reference + save-load.md(R-1 Approved) + 各 owner GDD 的可序列化契约核验

## Decision Makers

msc（用户）+ Technical Director（主笔）· lead-programmer / engine-programmer（实现）— 2026-06-06 Accepted

## Summary

《骰子大亨》存档/读档横切层（系统 #21，enable-not-own）需要一份钉死的序列化容器与读档完整性契约，否则任一状态系统字段漏存即造成隐形读档损坏（信任底线崩塌）。本 ADR 裁定：以 `USaveGame` 子类 + `UPROPERTY(SaveGame)` 反射序列化 + `SaveGameToSlot/LoadGameFromSlot` 为载体，前置 magic/version/checksum/manifest 四重读档完整性门，存 DA 引用不存布局，枚举 append-only，MVP 单槽、严格版本相等不迁移，读档后经 `OnGameLoaded` 信号驱动 delegate 重绑。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core（持久化 / 磁盘 I/O，非渲染/输入域） |
| **Knowledge Risk** | LOW — `USaveGame` / `UGameplayStatics::SaveGameToSlot` / `LoadGameFromSlot` / `DoesSaveGameExist` 是 pre-5.3 稳定 API，post-cutoff 无破坏性变更（已对照 `docs/engine-reference/unreal/` breaking-changes / deprecated-apis：存档框架不在弃用/迁移清单） |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`、`breaking-changes.md`、`deprecated-apis.md`、`current-best-practices.md`、`modules/`；`design/gdd/save-load.md`（R-1 Approved）；`design/gdd/tile-events.md`（牌堆序列化 CR-3）；`design/gdd/dice.md`（Seed 反射陷阱 L176） |
| **Post-Cutoff APIs Used** | None — 全部用 pre-5.3 稳定持久化 API。`AsyncSaveGameToSlot`（若选异步形态）自 4.x 即存在，非 post-cutoff |
| **Verification Required** | ① `UPROPERTY(SaveGame)` 对嵌套 USTRUCT（`FPlayerState`/`FDiceRollResult`/`FCardData`）与 `TArray`/`TMap` 的递归序列化在 5.7 仍按预期工作（写一个 round-trip 冒烟）；② `FArchive` proxy（`FObjectAndNameAsStringProxyArchive`）对 `UPROPERTY(SaveGame)` 过滤行为不变；③ 临时文件→原子替换在 Windows/Steam 目标平台的 `IFileManager::Move` 语义（EC-4/AC-21）；④ `FRandomStream` 经 struct 反射只持久化 `InitialSeed` 而丢当前 `Seed`（dice L176）——MVP 不存 Seed 故不触发，但 Full Vision 须显式 `GetCurrentSeed()`/`SetSeed()` 存取（与 ADR-0004 协同） |

> **Note**: Knowledge Risk = LOW，但若项目升级引擎版本仍须重跑上述 round-trip 冒烟确认反射序列化行为不变。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001（存档宿主：`USaveGame` 采集/写盘的服务挂在哪个 Subsystem——存档为跨关卡/跨局入口，倾向 `UGameInstanceSubsystem`；对局期采集则查询各 `UWorldSubsystem` 服务）· ADR-0003（delegate 重绑：`OnGameLoaded` 信号 + 呈现层先 Unbind 后 Bind 的事件总线规范）· ADR-0004（种子序列化：MVP 骰子不存 Seed、但事件格牌堆开局洗牌种子的存取须经 RNG 服务 `GetCurrentSeed()`/`SetSeed()`，禁 struct 反射） |
| **Enables** | save-load 实现（系统 #21）；主菜单20「继续游戏」入口；呈现层（HUD16/VFX19/property card17）读档重绑闭环 |
| **Blocks** | Epic「存档/读档」全部 story；间接 block 主菜单20「继续游戏」story |
| **Ordering Note** | ADR-0001 必须先 Accepted（宿主是采集/写盘服务的挂载点）。本 ADR 可与 0002/0003/0004 在 0001 Accepted 后并行起草（互不触同文件）。**实现排程**：本 ADR 标 Accepted 是存档 story 解锁的硬门（`Proposed` 态会令引用它的 story 被 `/story-readiness` 自动阻塞） |

## Context

### Problem Statement

save-load.md（R-1 Approved，系统 #21）把多个序列化形态决策显式 defer 到 **OQ-Save-1 架构期 ADR**：`USaveGame` 宿主与写盘形态、临时文件→原子替换的 UE 实现、序列化容器格式、版本判据、round-trip identity 校验骨架。架构总纲 §8 把这些连同**牌堆 Fisher-Yates 种子序列化 / 枚举 append-only / 49 持久化 TR** 一并指定为 ADR-0005 须钉死的契约。

**为何现在必须决策**：coding-standards 明文「stories referencing a `Proposed` ADR are auto-blocked」「Every system must have a corresponding ADR」。存档系统零 ADR 背书 → 任何存档 story 进实现即被 `/story-readiness` 阻断。**不决策的代价**：存档实现无权威序列化契约，各 programmer 各自选择容器格式 / 完整性校验强度，最终读档损坏在 QA 期才暴露——而存档系统的最大风险（序列化遗漏 = 隐形损坏）正是设计阶段反复要求「以契约清单防漏」的。

### Current State

`docs/architecture/` 持有 0 个真 ADR（审计 2026-06-06）。存档契约目前只散落在 save-load.md（CR-2/3/5/6 + F-1~F-4 + 26 AC）与 architecture.md Data Flow D.3 中，无 ADR 把「容器载体 / 完整性门强度 / 版本策略 / 重绑机制」固化为 programmer 可直接落地的工程决策。**且存在一处真实跨档缺口**（见下「跨档缺口」），须在本 ADR 一并收口。

### Constraints

- **技术**：UE5.7 Blueprint 为主 + C++ 框架。序列化载体须 BP 与 C++ 双可见（`UPROPERTY(SaveGame)` 反射满足）。回合状态机禁 `FTimerHandle`/Latent Action（player-turn L391，归 ADR-0001），故阶段须 `ETurnPhase` 枚举字段可序列化 + `switch(CurrentPhase)` 重入。
- **范围**：MVP 单存档槽 + 手动存/读 + 退出续局（save-load CR-1）。多槽/云存/autosave/版本迁移全 deferred（OQ-Save-3/4），**不进本 ADR 正文制造接缝**。
- **兼容**：序列化字段名须逐字对齐各 owner GDD（save-load CR-3 表是核对锚点）；枚举整数索引 append-only（旧存档兼容前提）。
- **资源**：横切层无独立 owner 语义——本 ADR 只裁工程搬运契约，**不裁任何被序列化字段的 gameplay 语义**（enable-not-own）。

### Requirements

- **功能**：采集 CR-3 清单全字段 → 序列化写盘 → 读盘反序列化 → 按依赖拓扑序重建 → 重绑 delegate；读档前置完整性门（magic/version/checksum/manifest）任一不符即拒绝且不破坏当前对局。
- **正确性（金标准）**：round-trip identity——存档前快照 == 读档后快照，逐字段 identity-check。任一字段漏存/错存，往返比对必 FAIL（可证伪，无「调参直到通过」逃逸）。
- **健壮性**：写盘原子性（临时文件→替换），写盘中崩溃只损坏临时文件、不破坏上一个有效存档。
- **性能**：MVP 单局存档体积小（数百 KB 量级，无全量布局），写/读盘非帧预算敏感路径（手动触发，非每帧）；不得阻塞渲染帧到可感知卡顿（异步 vs 同步见 Decision）。

## Decision

采用 **`USaveGame` 子类 + `UPROPERTY(SaveGame)` 反射序列化 + `UGameplayStatics::SaveGameToSlot`/`LoadGameFromSlot`** 作为存档载体；前置 **F-1 四重完整性门**（magic / version / checksum / field-manifest）；**存 DA 引用不存布局**；**枚举 append-only**；**MVP 单槽 + 严格版本相等不迁移**；读档后广播 **`OnGameLoaded`** 驱动呈现层 delegate 重绑（与 ADR-0003 协同）。

本 ADR 内部有 **3 个真分叉**须逐选项列代价并裁定，其余为对 save-load 已 Approved 契约的工程固化（非分叉）。

---

### 分叉 A — 序列化容器载体：`UPROPERTY(SaveGame)` 反射 vs 手写 `FArchive` 流

| | A1：`UPROPERTY(SaveGame)` 反射（**推荐**） | A2：手写 `FArchive` `Serialize()` 流 |
|---|---|---|
| **做法** | `USaveGame` 子类把全部 CR-3 字段标 `UPROPERTY(SaveGame)`；`SaveGameToSlot` 内部经 `FObjectAndNameAsStringProxyArchive` 自动遍历带 `SaveGame` 标记的属性 | 自定义 `Serialize(FArchive& Ar)` 逐字段 `Ar << Field` 手写读写顺序 |
| **Correctness** | 字段增删只改属性声明，反射自动覆盖；漏标 `SaveGame` 由 manifest 门兜底（F-4） | 读写顺序须人工严格对齐，错位即静默损坏；无编译期保护 |
| **Simplicity** | ✅ 高——声明式，BP 也能标记 | ❌ 低——每字段两处手写（写侧 + 读侧）易漂移 |
| **Maintainability** | ✅ 新字段登记 CR-3 + 标 `UPROPERTY(SaveGame)` 即可，6 个月后可读 | ❌ 手写流是漏存重灾区，正是设计阶段要防的风险 |
| **Testability** | round-trip AC（AC-1）直接覆盖；manifest 门可断言 | 同可测，但手写 bug 更隐蔽 |
| **Reversibility** | 高——容器是内部实现，AC 契约不变可换 | 低——一旦写盘格式定型，手写流难演进 |
| **代价** | 依赖反射对嵌套 USTRUCT/TArray/TMap 行为正确（Verification Required ①②） | 完全掌控字节布局（仅在需极致紧凑/跨语言时才有价值——本项目不需要） |

**推荐 A1**。理由：存档系统的首要风险是序列化遗漏，声明式 `UPROPERTY(SaveGame)` + manifest 运行时门（F-4）双层防漏远胜手写流的人工纪律；A2 的唯一优势（字节级掌控）在「单机单槽、体积小、无跨语言」的 MVP 无收益，反而把 review 锚点从「CR-3 清单核对」退化为「手写读写顺序逐行核对」。

### 分叉 B — 写盘形态：同步 `SaveGameToSlot` vs 异步 `AsyncSaveGameToSlot`

| | B1：同步 `SaveGameToSlot`（**推荐 MVP**） | B2：异步 `AsyncSaveGameToSlot` |
|---|---|---|
| **做法** | 主线程同步序列化 + 写盘，调用返回即完成 | 后台线程写盘，完成经 delegate 回调通知 |
| **卡帧风险** | MVP 存档体积小（数百 KB），同步写盘耗时通常 < 一帧到几帧；手动触发（暂停菜单/退出），非每帧 | 不卡帧，但引入异步状态机（写盘中再次触发须串行/排队，EC-10） |
| **Simplicity** | ✅ 高——调用即完成，存档点判定与写盘原子（CR-4 瞬态门后立即写） | ❌ 较低——须管理「写盘进行中」态、回调时序、与读档/退出竞争 |
| **Correctness** | 串行天然无并发（save-load EC-10：单线程回合流） | 须显式防「写盘未完成时退出/再存」竞态 |
| **Reversibility** | 高——MVP 同步，Alpha autosave（OQ-Save-3，回合末触发可能要求不卡帧）再升异步 | — |
| **代价** | 暂停菜单存档时一次性同步写盘的微小停顿（可接受——非 gameplay 帧） | 提前承担异步复杂度，MVP 无 autosave 不偿付 |

**推荐 B1（MVP 同步）**。理由：MVP 存档手动触发且体积小，同步写盘的微卡顿落在暂停菜单/退出时刻（非 gameplay 关键帧），Simplicity × Correctness 压倒性优于异步；异步的真实收益在 Alpha autosave（回合末自动存、不能卡 gameplay 帧）——届时经 OQ-Save-3 升级，本决策 Reversibility 高。**原子替换（临时文件→`IFileManager::Move`）与同步/异步正交**：无论哪种形态都先写临时文件、校验通过后原子替换 `SLOT_DEFAULT`（EC-4/AC-21）。

### 分叉 C — 完整性门 checksum 算法：`FCrc::MemCrc32` vs 加密哈希（SHA1）

| | C1：`FCrc::MemCrc32`（CRC32，**推荐**） | C2：SHA1/加密哈希 |
|---|---|---|
| **防护目标** | 检测字节级损坏（写盘中崩溃截断 / 磁盘损坏 / 意外篡改） | 同 + 抗恶意构造碰撞 |
| **威胁模型契合** | ✅ save-load F-3 明确：checksum 防**字节完整性**，不防 owner 语义、不防恶意作弊（单机本地存档，作弊只伤玩家自己） | 过度——单机存档无对抗性威胁模型 |
| **性能** | ✅ 引擎内建、极快 | 更慢，无收益 |
| **Simplicity** | ✅ 一行 `FCrc::MemCrc32(payload, len)` | 引入哈希依赖 |
| **代价** | 理论上可被刻意构造碰撞（单机无意义） | 抗碰撞（MVP 不需要） |

**推荐 C1（CRC32）**。理由：save-load F-3 的威胁模型是「字节级损坏」而非「对抗性篡改」——单机本地存档，玩家篡改只影响自己。CRC32（`FCrc::MemCrc32`）引擎内建、极快、足够检测截断/损坏。加密哈希是 over-engineering（违反 Simplicity）。**注**：checksum 只防字节完整性，**不防 owner 字段语义错误**（那是各 owner 不变式 + AC 的职责，F-3 已声明）。

---

### Architecture

```
                          USaveGame 子类 (UDiceTycoonSaveGame)
                          ┌──────────────────────────────────────┐
  存档触发(暂停菜单/退出)   │ Header: MagicTag / SaveVersion /        │
        │                 │         Checksum / FieldManifest        │
        ▼                 │ Payload (全部 UPROPERTY(SaveGame)):     │
  [CR-4 可存档点门]────────│  · BoardIdentifier (FName, DA引用)      │
   查询回合2 bIsSafeToSave │  · TArray<FPlayerState> (11字段/人)     │  采集 = enable-not-own:
   瞬态→SaveRejected       │  · CurrentPlayerIndex / CurrentPhase    │  各 owner Subsystem 供值,
        │ 通过             │  · ConsecutiveDoubles / 阈值            │  存档21 不裁语义
        ▼                 │  · FDiceRollResult (Die1/Die2/Total/..) │
  采集 CR-3 全字段─────────│  · TMap Cash / owner map / bIsMortgaged │
        │                 │  · per-tile house_count [0,5]           │
        ▼                 │  · 【事件格7】牌堆数组顺序(model B)+     │  ← 跨档缺口收口:
  写临时文件 + CRC32       │     bHoldsChanceOutCard/ChestOutCard    │     save-load CR-3 漏列7
        │                 └──────────────────────────────────────┘     本ADR纳入
        ▼
  校验通过→原子替换 SLOT_DEFAULT (IFileManager::Move)

  ─────────────────────── 读档路径 (CR-5 依赖拓扑序) ───────────────────────

  LoadGameFromSlot → [F-1 四重门] magic ∧ version ∧ checksum ∧ manifest
        │ 任一假 → 拒绝读档 + 当前对局不破坏 + 不广播 OnGameLoaded
        ▼ 全真
  1.加载 Board DA(BoardIdentifier) → 2.重建经济Cash/地产owner map/建房house_count(互不依赖)
        │                          → 重建事件格7 牌堆数组顺序 + 出狱卡持有标记
        ▼
  3.重建回合2 PlayerState+定序+CurrentPhase+FDiceRollResult
        │
        ▼
  4.骰子3 SetSeed(新非确定值) ──5.广播 OnGameLoaded──► 呈现层(16/17/19/22)各自先Unbind后Bind
        │                                              回合状态机 switch(CurrentPhase) 还原阶段+重绑该阶段delegate
        ▼
  6.读档完成,允许保存(EC-7)
```

### Key Interfaces

```cpp
// 存档容器 —— 全部序列化字段标 UPROPERTY(SaveGame)（分叉A=A1 反射）
UCLASS()
class UDiceTycoonSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    // ── Header（完整性门，F-1）──
    UPROPERTY(SaveGame) uint32  MagicTag;            // == SAVE_MAGIC，F-1 magic_ok
    UPROPERTY(SaveGame) int32   SaveVersion;         // == CURRENT_SAVE_VERSION，F-2 严格相等
    UPROPERTY(SaveGame) uint32  PayloadChecksum;     // FCrc::MemCrc32(payload)（分叉C=C1），F-3
    UPROPERTY(SaveGame) TArray<FName> FieldManifest; // CR-3 必需字段集，F-4 防漏运行时门

    // ── Payload（CR-3 清单，逐字对齐 owner GDD）──
    UPROPERTY(SaveGame) FName              BoardIdentifier;        // 棋盘1：DA引用，非全量布局(CR-2)
    UPROPERTY(SaveGame) TArray<FPlayerState> PlayerStates;        // 回合2：11字段全列，不可吞字段
    UPROPERTY(SaveGame) int32             CurrentPlayerIndex;     // 回合2
    UPROPERTY(SaveGame) ETurnPhase        CurrentPhase;           // 回合2：switch重入(禁Latent)
    UPROPERTY(SaveGame) int32             ConsecutiveDoubles;     // 回合2
    UPROPERTY(SaveGame) int32             DoublesJailThreshold;   // 回合2：锁定阈值
    UPROPERTY(SaveGame) FDiceRollResult   CurrentDiceResult;      // 回合2持有：完整Die1/Die2/Total/bIsDouble
    UPROPERTY(SaveGame) TMap<int32,int32> CashByPlayer;           // 经济5：经容器写回,不经Credit/Debit
    UPROPERTY(SaveGame) TMap<int32,int32> TileOwnerMap;           // 地产6：TileIndex→PlayerId(INDEX_NONE=无主)
    UPROPERTY(SaveGame) TMap<int32,bool>  TileMortgaged;          // 地产6：per-tile bIsMortgaged
    UPROPERTY(SaveGame) TMap<int32,int32> HouseCountByTile;       // 建房8：per-tile [0,5]（非全盘总数）
    // ── 事件格7（跨档缺口收口，save-load CR-3 漏列，见下「跨档缺口」）──
    UPROPERTY(SaveGame) TArray<FCardData> ChanceDeckOrder;        // 事件格7：牌堆数组顺序(model B,无指针)
    UPROPERTY(SaveGame) TArray<FCardData> ChestDeckOrder;         // 事件格7：同上
    UPROPERTY(SaveGame) bool              bHoldsChanceOutCard;    // 事件格7：出狱卡持有(离堆期)
    UPROPERTY(SaveGame) bool              bHoldsChestOutCard;     // 事件格7
    // 注：bIsBankrupt/bIsInJail/JailTurnsServed 归回合2 PlayerState（已在 PlayerStates 覆盖，不重复存）
    // 注：MVP 不存骰子 Seed；派生量 is_monopoly/station_count/house总数 读档后由 owner 重算
};

// 存档服务接口（边界8，宿主由 ADR-0001 裁——倾向 UGameInstanceSubsystem 跨局入口）
UFUNCTION(BlueprintCallable) ESaveResult SaveGame();   // CR-4门→采集→写manifest→CRC32→原子写盘
UFUNCTION(BlueprintCallable) ESaveResult LoadGame();   // F-1四重门→加载DA→反序列化→重建→发OnGameLoaded
UFUNCTION(BlueprintCallable) bool        DoesSaveGameExist() const;       // EC-6 继续游戏可用性
UPROPERTY(BlueprintAssignable) FOnGameLoaded OnGameLoaded;                // 重建完成恰广播一次(AC-11)

// 返回码（EC-1~EC-9 全枚举，append-only）
enum class ESaveResult : uint8 { Success, SaveRejected_TransientState, /* EC-5 */
    Load_VersionMismatch, Load_FieldMissing, Load_BoardDAMissing, Load_ChecksumFail, /* EC-1~4 */
    Save_DiskFailure, Load_SlotEmpty /* EC-9/EC-6 */ };
```

### Implementation Guidelines

1. **分叉A=A1**：全字段标 `UPROPERTY(SaveGame)`；`SaveGameToSlot` 经 `FObjectAndNameAsStringProxyArchive` 自动过滤 `SaveGame` 标记属性。新增 CR-3 字段必须同步：(a) 标 `UPROPERTY(SaveGame)`；(b) 登记 `FieldManifest`；(c) `CURRENT_SAVE_VERSION += 1`（AC-25 code-review 核对）。
2. **分叉B=B1**：MVP 同步 `SaveGameToSlot`。无论同步异步，**先写临时文件 → CRC32 校验 → `IFileManager::Move` 原子替换** `SLOT_DEFAULT`（EC-4/AC-21）；`Move` 失败不破坏旧档（EC-9/AC-22）。
3. **分叉C=C1**：`PayloadChecksum = FCrc::MemCrc32(payload_bytes, len)`，header 本身不入 checksum 计算范围（F-3 `payload_bytes` = 不含 header 的主体）。
4. **完整性门顺序**：magic → version → checksum → manifest，短路求值，任一假即 `ESaveResult` 对应失败码 + **不广播 `OnGameLoaded`**（AC-11/16）+ 当前对局不破坏。
5. **读档重建拓扑序**（CR-5）严格：DA → (经济/地产/建房/事件格牌堆 互不依赖) → 回合2 → 骰子 SetSeed → 重绑 → switch(CurrentPhase)。**禁止**重建 A 时读尚未重建的 B。
6. **Cash 写回经容器、不经 Credit/Debit**（AC-3：`OnCashChanged` 触发 0 次）——读档是状态写回非业务路径，避免误播金币 juice。
7. **delegate 重绑（CR-6，与 ADR-0003 协同）**：存档21 **不直接 AddDynamic**，只广播 `OnGameLoaded`；呈现层（16/17/19/22）监听后**各自先 Unbind 后 Bind**（防双绑）。回合状态机 `switch(CurrentPhase)` 还原阶段须**重绑该阶段 delegate**（枚举字段恢复值不够，player-turn AC-34）。
8. **可存档点门（CR-4）**：查询回合2「是否在合法可存档点」（接口名待回合2 提供，OQ-Save-2）。**MVP 默认锁权威路径**：以回合2 报告为 GIVEN，本系统不自裁阶段语义（守 enable-not-own）。自判 fallback 为临时降级、无 AC 守护、回合2 接口到位后移除——勿默认实现。
9. **枚举 append-only**：`ETurnPhase`/`EPlayerColor`/`EAIDifficulty`/`ESaveResult`/`FCardData` 内枚举的整数索引**不可重排**（player-turn AC-43），否则旧存档读出错误枚举。改动只能尾部 append。

## Alternatives Considered

### Alternative 1: JSON/文本序列化（人类可读存档）

- **Description**：把对局快照序列化为 JSON 文本文件而非 `USaveGame` 二进制。
- **Pros**：可人工 diff / 调试期肉眼读；跨工具友好。
- **Cons**：体积更大；须手写 JSON ↔ struct 映射（绕过反射，回到分叉A的A2 困境）；`FName`/`TObjectPtr` DA 引用的文本序列化须自定义；失去 `UPROPERTY(SaveGame)` 自动过滤。
- **Estimated Effort**：高于 A1（手写映射层）。
- **Rejection Reason**：违反 Simplicity；可读性收益在「单机单槽、玩家不该手编存档」场景无价值；调试可由 round-trip AC + dev 日志覆盖，无需牺牲序列化健壮性。

### Alternative 2: 完整版本迁移框架（MVP 即支持向后兼容）

- **Description**：MVP 即实现 `save_version < CURRENT` 时的字段映射/升级路径，而非严格相等拒绝。
- **Pros**：开发期改格式不丢旧存档。
- **Cons**：迁移逻辑成本高、易引入错误（save-load F-2 理由）；MVP 单槽、开发期格式频繁变，迁移路径本身成为 bug 源。
- **Estimated Effort**：显著高于「严格相等 + 拒绝」。
- **Rejection Reason**：save-load F-2 已裁 MVP 严格相等不迁移（OQ-Save-4 defer 到格式稳定后）。本 ADR 遵从该 Approved 设计裁决，不在 MVP 提前承担迁移复杂度。

### Alternative 3: 序列化全量棋盘布局（不依赖 DA 重建）

- **Description**：存档复制 40 格静态布局（类型/价格/租金表），读档不依赖 DA。
- **Pros**：DA 删除/改名后仍可读档（无 EC-3）。
- **Cons**：存档体积膨胀（重复静态数据）；静态布局可由 DA 完全重建，复制是冗余；与 save-load CR-2 / board CR-3 的「DA 引用」裁决冲突。
- **Rejection Reason**：save-load CR-2 已裁「存 DA 引用不存布局」，EC-3（DA 缺失→拒绝读档）是接受的代价。违反 DRY，且 board 静态数据运行时不可变（board L139）本就该由 DA 重建。

## Consequences

### Positive

- 存档实现有单一权威序列化契约，programmer 有明确落地锚点（容器类 / 完整性门 / 重建序）。
- `UPROPERTY(SaveGame)` + manifest 双层防漏，直接服务设计阶段最关注的「序列化遗漏 = 隐形损坏」风险。
- 四重完整性门 + 原子写盘 → 读档损坏/写盘崩溃不破坏既有存档，兑现 Player Fantasy 信任底线。
- LOW Knowledge Risk（全 pre-5.3 稳定 API），引擎升级再验证成本低。
- **收口一处真实跨档缺口**（事件格7 牌堆序列化腿，save-load CR-3 漏列）——把潜在隐形损坏（读档后牌堆顺序错乱/出狱卡丢失）在架构期消除。

### Negative

- 接受 EC-3 代价：DA 删除/改名 → 读档失败（换取存档体积小 + 不冗余）。
- 接受 EC-1 代价：MVP 严格版本相等，开发期改格式即令旧存档不可读（换取不承担迁移复杂度）。
- `UPROPERTY(SaveGame)` 反射依赖嵌套 USTRUCT/容器行为正确（Verification Required，须 round-trip 冒烟兜底）。
- MVP 同步写盘在暂停菜单/退出时有微小停顿（非 gameplay 帧，可接受；Alpha autosave 再升异步）。

### Neutral

- 存档为 enable-not-own 横切层：本 ADR 不改任何 owner 字段语义，只裁搬运契约。
- 牌堆 Fisher-Yates 洗牌种子的存取经 RNG 服务（ADR-0004），MVP 骰子 Seed 不存但牌堆数组顺序存（model B 无指针，存数组即重现牌序）。

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| 反射序列化对嵌套 USTRUCT/TMap 在 5.7 行为异常 | LOW | HIGH（静默漏字段） | Verification Required ①②；AC-1 round-trip + F-4 manifest 门运行时兜底 |
| 新增状态字段忘记登记 CR-3/manifest（序列化遗漏） | MED | HIGH | F-4 manifest 门读档拒绝（暴露而非静默默认）；AC-25 code-review 版本递增纪律 |
| 事件格7 牌堆序列化腿因 save-load CR-3 漏列被实现者遗漏 | MED | HIGH（读档牌序错乱/出狱卡丢） | **本 ADR 显式纳入容器 + Key Interfaces；登记 save-load CR-3 补登 follow-up（OQ-Save-5 并轨）** |
| 可存档点查询接口（OQ-Save-2）未定，实现自判 fallback 越权 | MED | MED（复制回合2 阶段语义） | MVP 锁权威路径；fallback 标临时降级无 AC、回合2 接口到位即删（与 ADR-0001 协同） |
| 枚举重排破坏旧存档兼容 | LOW | HIGH | append-only 纪律（AC-43）；CURRENT_SAVE_VERSION 递增 |
| 写盘中崩溃破坏唯一存档 | LOW | HIGH（进度全失） | 临时文件→原子替换（EC-4/AC-21），崩溃只损坏临时文件 |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| CPU (frame time) | 0ms（无存档） | 同步写盘瞬时 < 数帧（手动触发，非 gameplay 帧） | 16.6ms gameplay 帧不受影响（存档不在 gameplay 帧） |
| Memory | — | 单 `USaveGame` 实例数百 KB 量级（无全量布局） | 待目标硬件定（远低于上限） |
| Load Time | — | 读档反序列化 + 重建 < 1s（单局规模） | 续局体验目标 < 1s 可感知 |
| Network | N/A（MVP 单机） | — | Full Vision 联网存档语义 OQ-Save-6 |

## Migration Plan

本 ADR 是新建决策（当前 0 ADR），无既有系统迁移。实现排程：

1. ADR-0001 已 Accepted（宿主裁定）；本 ADR 已 Accepted（2026-06-06，msc 裁定分叉 A/B/C）。
2. lead-programmer/engine-programmer 实现 `UDiceTycoonSaveGame` 容器 + 存档服务（边界8 接口）。
3. 先落 AC-1 round-trip 冒烟（验证反射序列化在 5.7 正确）→ 再落 F-1~F-4 完整性门 AC → 再落继承义务 AC（精确阶段/骰结果/重绑/Cash/瞬态）。
4. **跨档缺口偿还**：save-load.md CR-3 表补登事件格7 序列化行（牌堆数组顺序 + 出狱卡持有标记）——经 producer `/propagate-design-change` 并入 OQ-Save-5。

**Rollback plan**：本 ADR 若证伪（如反射序列化在 5.7 不可靠），回退到分叉 A2（手写 `FArchive`），容器字段集与 AC 契约不变——Reversibility 高（容器是内部实现）。

## Validation Criteria

- [ ] AC-1 全状态 round-trip identity 在 `-nullrhi` headless 跑绿（含 CurrentTileIndex / 事件格牌堆顺序逐字段）。
- [ ] F-1 四重门 AC（AC-16/17/18/19）：版本/字段/校验和/DA 缺失任一不符 → 拒绝读档 + 当前对局不破坏 + 不广播 OnGameLoaded。
- [ ] AC-21 写盘原子性：模拟写盘中断 → SLOT_DEFAULT 仍是上一次完整有效存档。
- [ ] AC-3 Cash 写回经容器：`OnCashChanged` 触发 0 次（读档非业务路径）。
- [ ] AC-9/10/11 delegate 重绑：读档后事件可达 + 阶段 delegate 重绑 + `OnGameLoaded` 恰一次。
- [ ] tile-events AC-64 出狱卡 + 牌堆顺序往返还原（跨档缺口收口验证）。
- [ ] `/story-readiness` 对存档 story 不再因「无 ADR / Proposed ADR」阻塞（本 ADR Accepted 后）。

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/save-load.md` | Save/Load #21 | OQ-Save-1：`USaveGame` 宿主 + 写盘形态 + 临时文件→原子替换 | 分叉 A/B/C 裁定容器载体（A1 反射）/ 写盘形态（B1 同步）/ checksum（C1 CRC32）；原子替换 `IFileManager::Move` |
| `design/gdd/save-load.md` | Save/Load #21 | CR-2 存 DA 引用不存布局 | 容器存 `BoardIdentifier:FName`，Alt-3 拒绝全量布局；EC-3 DA 缺失拒绝读档 |
| `design/gdd/save-load.md` | Save/Load #21 | CR-3 逐状态系统可序列化契约清单 | Key Interfaces 容器逐字对齐 CR-3 表；F-4 manifest 运行时防漏门 |
| `design/gdd/save-load.md` | Save/Load #21 | CR-5 读档依赖拓扑序重建 | Architecture 读档路径 6 步严格拓扑序 |
| `design/gdd/save-load.md` | Save/Load #21 | CR-6 读档后 delegate 重绑 | `OnGameLoaded` 信号 + 呈现层先 Unbind 后 Bind（与 ADR-0003 协同） |
| `design/gdd/save-load.md` | Save/Load #21 | F-1~F-4 完整性门（magic/version/checksum/manifest） | 四重门短路求值；F-2 严格相等不迁移（Alt-2 拒绝）；F-3 CRC32（分叉C） |
| `design/gdd/save-load.md` | Save/Load #21 | OQ-Save-2 可存档点查询 | MVP 锁权威路径（查询回合2），fallback 标临时降级无 AC（与 ADR-0001 协同） |
| `design/gdd/tile-events.md` | 事件格 #7 | CR-3 L44/L45/L94/L110/AC-64：序列化牌堆数组顺序(model B,无指针) + `bHoldsChanceOutCard`/`bHoldsChestOutCard` + 开局洗牌种子 | **收口 save-load CR-3 漏列缺口**：容器纳入 `ChanceDeckOrder`/`ChestDeckOrder`/两出狱卡标记；种子存取经 RNG 服务（ADR-0004） |
| `design/gdd/player-turn.md` | 玩家回合 #2 | AC-34 序列化精确阶段 + ConsecutiveDoubles + 重绑该阶段 delegate；L391 禁 Latent Action | `CurrentPhase:ETurnPhase` 可序列化 + switch 重入 + 阶段 delegate 重绑；阶段用枚举非 Latent（归 ADR-0001） |
| `design/gdd/dice.md` | 骰子 #3 | OQ-T-Dice-5 完整 `FDiceRollResult`；L176 Seed 反射陷阱 | 容器存完整 Die1/Die2/Total/bIsDouble；MVP 不存 Seed（Full Vision 显式 GetCurrentSeed/SetSeed，ADR-0004） |
| `design/gdd/economy-cash.md` | 经济 #5 | OQ-T-Econ-4 序列化每玩家 Cash，写回经容器不触发事件 | `CashByPlayer:TMap` 写回，AC-3 `OnCashChanged` 0 次 |
| `design/gdd/property-ownership.md` | 地产 #6 | 序列化 owner map + bIsMortgaged，派生量读档后重算 | `TileOwnerMap`/`TileMortgaged`；派生量 is_monopoly 不存、读档由 6 重算 |
| `design/gdd/building-upgrade.md` | 建房 #8 | 序列化 per-tile house_count [0,5] | `HouseCountByTile:TMap`（per-tile，非全盘总数，防 6↔8 环） |
| architecture.md §8 | Foundation | 枚举 append-only / 49 持久化 TR | 枚举 append-only 纪律（AC-43）；49 持久化 TR 经 CR-3 容器全字段落地（待 tr-registry 填充引用） |

## Related

- **Depends on**: ADR-0001（宿主）· ADR-0003（事件总线/重绑）· ADR-0004（RNG/种子序列化）
- **Coordinates with**: player-turn OQ-1（状态机宿主禁 Latent Action，本 ADR 读档重建依赖其成立）
- **Source design**: `design/gdd/save-load.md`（R-1 Approved）· `design/gdd/tile-events.md`（牌堆序列化）
- **Architecture context**: `docs/architecture/architecture.md` §8 ADR-0005 条目 + Data Flow D.3
- **Follow-up（跨档缺口）**: save-load.md CR-3 表须补登事件格7 序列化行（producer `/propagate-design-change`，并入 OQ-Save-5）
- **Implemented by**（待实现）: `src/core/save/` 存档服务 + `UDiceTycoonSaveGame` 容器类
