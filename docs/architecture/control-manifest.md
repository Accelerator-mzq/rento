# Control Manifest

> **Engine**: Unreal Engine 5.7
> **Last Updated**: 2026-06-06
> **Manifest Version**: 2026-06-06
> **ADRs Covered**: ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0005, ADR-0006, ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0012 (全 12 Accepted)
> **Status**: Active — regenerate with `/create-control-manifest update` when ADRs change

`Manifest Version` 是本 manifest 生成日期。story 文件创建时嵌入此日期；`/story-readiness`
比对 story 嵌入版本与本字段以检出针对过期规则写就的 story。永远等于 `Last Updated`
（同一日期，服务不同消费者）。

这是程序员快速参考表 —— 从全部 12 Accepted ADR、technical-preferences、engine
reference 提取。规则**逐字保真不改写语义**;每条规则的推理见所引 ADR。
ADR 解释**为什么**,本表告诉你**做什么 / 绝不做什么**。

> **⚠ Sprint 0 引擎验证前置**:多条规则标注 `[需 5.7 核验]` —— 这些是 post-cutoff
> (LLM 知识 ~5.3) 待实测行为,**开工首步**执行验证,失败须修订对应 ADR 并重 Accept。
> 汇总见本表末「Engine Verification Required (Sprint 0)」。

---

## Foundation Layer Rules

*Applies to: UObject 宿主与生命周期、数据容器、事件总线、确定性 RNG、存档序列化*

### Required Patterns

**宿主与生命周期 (ADR-0001)**
- **per-match 服务一律继承 `UWorldSubsystem`**;`ShouldCreateSubsystem` 排除 editor-preview World,避免编辑器误建对局态 — source: ADR-0001
- **per-match 服务挂 `UWorldSubsystem`**(棋盘 DA 持有者 / RNG 服务 / owner map / 经济5 / 地产6 / 事件7 / 回合状态机 / GameStateSnapshot 生成方);生命周期边界=一局,PIE Stop 即销毁、Start 即重建 — source: ADR-0001
- **跨局持久服务挂 `UGameInstanceSubsystem`**(Save/Load 框架21、Audio22 资产/混音、Setup/主菜单20 入口) — source: ADR-0001
- **Seed 注入唯一时机 = `OnWorldBeginPlay`**(不在 `Initialize`——此时玩家配置尚未由 `StartNewGame` 落地);RNG 默认种子恒 0、禁 lazy-init — source: ADR-0001
- **异步加载纪律**:`Initialize` 中 `FStreamableManager::RequestAsyncLoad` 取得的 `TSharedPtr<FStreamableHandle>` 须存为成员,`Deinitialize` 显式 `CancelHandle()` — source: ADR-0001
- **状态机禁 Latent**:`ETurnPhase` 枚举字段 + delegate 推进;读档 `switch(CurrentPhase)` 重入 — source: ADR-0001
- **宿主不持状态写语义**:World Subsystem 持服务实例与生命周期,但 `Cash`/`house_count`/owner map 的写语义仍归各 owner;宿主裁定不改任何 owner 关系 — source: ADR-0001
- **Full Vision 迁移预留**:MVP 状态以普通 `UPROPERTY` 承载,字段命名与结构对齐 `PlayerState`/`GameState`,便于联网期平移 — source: ADR-0001
- 对局期 delegate 的订阅须在宿主 `Initialize` 绑、`Deinitialize` 全部解绑(PIE Stop 不留野绑定) — source: ADR-0001/0003

**数据容器 (ADR-0002)**
- **棋盘载体 = `UBoardDataAsset : public UPrimaryDataAsset`**,顶层持元数据字段 + 一个 `TArray<FBoardTileData> Tiles` — source: ADR-0002
- 运行时由宿主以 **`UPROPERTY UBoardDataAsset*` 强引用持有,防 GC** — source: ADR-0002
- **`AdvanceIndex` 返回 `FBoardAdvanceResult` struct**(非 out-param) — source: ADR-0002
- F-1/F-2 封装为 `UBoardMathLibrary` 的 `BlueprintPure` 静态函数 — source: ADR-0002
- **加载方式**:单棋盘 ~数 KB,用 `UAssetManager` 同步加载(`GetPrimaryAssetObject` 或 `LoadPrimaryAsset` + 等待);异步仅未来多棋盘预加载场景 — source: ADR-0002
- **校验**:加载后跑 GDD §Edge Cases 全部校验(`Validated`),失败返回结构化错误码(`BoardTooSmall`/`Index0NotGo`/…)并 `LoadFailed`,不进 Active — source: ADR-0002
- **接口隔离**:所有下游经持有者的 `GetTileData`/`GetTilesInGroup` 访问,不得各自硬引用 `UBoardDataAsset`(迁移载体时下游接口不变) — source: ADR-0002
- `bIsPlaceholderData` 用 `WITH_EDITORONLY_DATA` 包裹、Shipping 剥离,cook 前 commandlet 检测 — source: ADR-0002
- `BoardIdentifier: FName` 作顶层 `UPROPERTY` 作者手填,与 `PrimaryAssetId`/路径解耦(AC-28) — source: ADR-0002
- 运行时只读访问 `GetTileData(index)` 须 O(1) 或近 O(1) — source: ADR-0002

**事件总线 (ADR-0003)**
- **宏一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_NParams]` + `UPROPERTY(BlueprintAssignable, Category="Events")`**;BP 与 C++ 双向可订 — source: ADR-0003
- **多字段 payload 必包 `USTRUCT(BlueprintType)`**(如 `FDiceRollResult`、`FPhaseChangedInfo`、`FTurnOrderResult`);`OnTurnOrderResolved` 必须 USTRUCT 封装 — source: ADR-0003
- **字段「只增不改语义」**;枚举 append-only(与 ADR-0005 协同) — source: ADR-0003
- **命名**:事件 `On<PastTense>`(`OnRentPaid`/`OnCashChanged`/`OnBuildingChanged`);payload struct `F<Event>Info` — source: ADR-0003
- **单一事件源**:每个状态变更恰一个 owner 广播。`OnGameWon` 由回合2 触发(破产9 return-only);`OnBuildingChanged` **2 字段**(actingPlayerId 由回合2 上下文取,非塞事件第3字段);`OnAIActionExecuted` 由回合2 执行 AI 动作时广播(非 AI 发) — source: ADR-0003
- **方向由消费方派生**:payload 携类型语境(`EChangeReason`/Payer/Payee 视角),方向由消费方从 delta 符号派生,owner 不为每个方向各发一个事件 — source: ADR-0003
- **读档重绑纪律**:`OnGameLoaded` 后先全量解绑呈现层既有订阅(防 EC-8 双订阅),再按权威订阅矩阵重绑;经 Foundation「② Event Bus」纪律层集中工具函数 — source: ADR-0003
- **未注册枚举值兜底**:消费方对未知 `EChangeReason`/`EArrivalContext`/`EJailReason` 走中性兜底(不崩不静默错播) — source: ADR-0003
- **破产主反馈只订经济5 `OnBankruptcyDeclared`**(出局 juice / 破产音 / BGM duck);`OnPlayerBankrupt` 仅供需 reason/creditor 或编排末增量呈现 — source: ADR-0003
- 呈现层订阅以 architecture.md §Data Flow D.2 事件总览表为权威矩阵;新增订阅必须同步更新该表 — source: ADR-0003
- owner 系统先同步算定权威结果,再广播事件(结算同步先行,呈现事件后随);呈现层纯叶子只订阅、只显示、绝不回写 — source: ADR-0003

**确定性 RNG (ADR-0004)**
- **全部 gameplay 随机(骰点/牌堆洗牌/AI 决策扰动)走唯一权威 `FRandomStream`**,挂 DiceService(宿主 ADR-0001);禁旁路引擎全局 RNG — source: ADR-0004
- **流隔离**:表现层 juice 随机(VFX/Audio/HUD)与 AI 内部扰动**不得复用骰子3 权威流**,各持独立非权威 `FRandomStream` — source: ADR-0004
- **退化安全**:`Min==Max` early-return 返回 Min 不调流(Seed 不推进);`Min>Max` ensure 不自动交换;默认构造种子恒 0 禁落 lazy-init — source: ADR-0004
- **`RollDice` 执行序铁律**(dice F-4 前提②):① 流抽 Die1,Die2 → ② 本地固化 result → ③ 广播 `OnDiceRolled(result)` → ④ 返回同一固化 result(非广播后重读流);保证 同步返回值 == 事件 payload — source: ADR-0004
- **`RandomRange` 约束**:Range = Max-Min+1 < 2^24(走 FRand() 浮点中介,超 2^24 精度崩);封装层 `ensure((int64)Max-(int64)Min+1 < (1<<24))` — source: ADR-0004
- `RandomFloat01` 返回 `[0.0,1.0)`,1.0 严格排除 — source: ADR-0004
- `FRandomStream` 原生非 BlueprintCallable,须 C++ 封装为 `UFUNCTION(BlueprintCallable)` 暴露 — source: ADR-0004
- 非权威流初始化禁默构(种子恒0),须 `Initialize(FPlatformTime::Cycles())` — source: ADR-0004
- **`OnDiceRolled` 回调内禁同步调任何抽取 API**(单线程重入会插入权威序列,破坏 dice F-4 前提②) — source: ADR-0004
- Seed 序列化:MVP 不序列化 Seed(当前骰由 player-turn 序列化完整 `FDiceRollResult` 保护);Full Vision 重放经 `GetCurrentSeed()` 显式存取 — source: ADR-0004/0005

**存档序列化 (ADR-0005)**
- **全字段标 `UPROPERTY(SaveGame)`**;`SaveGameToSlot` 经 `FObjectAndNameAsStringProxyArchive` 自动过滤 SaveGame 标记属性 — source: ADR-0005
- **新增 CR-3 字段必须同步**:(a) 标 `UPROPERTY(SaveGame)`;(b) 登记 `FieldManifest`;(c) `CURRENT_SAVE_VERSION += 1`(AC-25 code-review 核对) — source: ADR-0005
- **读档前置完整性门顺序**:magic → version → checksum → manifest,短路求值,任一假即 `ESaveResult` 失败码 + **不广播 `OnGameLoaded`** + 当前对局不破坏 — source: ADR-0005
- **写盘原子性**:先写临时文件 → CRC32 校验 → `IFileManager::Move` 原子替换 `SLOT_DEFAULT`;`Move` 失败不破坏旧档 — source: ADR-0005
- `PayloadChecksum = FCrc::MemCrc32(payload_bytes, len)`,header 不入 checksum 计算范围 — source: ADR-0005
- **round-trip identity**:存档前快照 == 读档后快照,逐字段 identity-check(可证伪,无「调参直到通过」逃逸) — source: ADR-0005
- **读档重建拓扑序(CR-5)**:DA → (经济/地产/建房/事件格牌堆 互不依赖) → 回合2 → 骰子 SetSeed → 重绑 → `switch(CurrentPhase)`;禁重建 A 时读尚未重建的 B — source: ADR-0005
- **Cash 写回经容器、不经 Credit/Debit**(AC-3:`OnCashChanged` 触发 0 次,避免误播金币 juice) — source: ADR-0005
- delegate 重绑(CR-6):存档21 只广播 `OnGameLoaded`,呈现层(16/17/19/22)监听后各自先 Unbind 后 Bind — source: ADR-0005
- **可存档点门(CR-4)**:查询回合2「是否合法可存档点」为 GIVEN,本系统不自裁阶段语义(守 enable-not-own);自判 fallback 为临时降级、无 AC、回合2 接口到位即移除 — source: ADR-0005
- 派生量 `is_monopoly`/`station_count`/house总数 读档后由 owner 重算(不存) — source: ADR-0005
- `OnGameLoaded` 重建完成恰广播一次(AC-11) — source: ADR-0005
- 序列化字段名须逐字对齐各 owner GDD(save-load CR-3 表是核对锚点) — source: ADR-0005

### Forbidden Approaches
- **Never 把 per-match 对局态全挂 `UGameInstanceSubsystem`** — PIE 隔离破裂,World 生命周期事件拿不到,Seed 注入时机被迫挪走 — source: ADR-0001
- **Never 在 MVP 用 `UActorComponent`/`AGameState` 上的 Component 形态承载对局服务** — 过早复杂度,违反 Simplicity — source: ADR-0001
- **Never 在状态机用 `FTimerHandle`/Blueprint `Delay`/`WaitForEvent`(Latent Action)** — 不可序列化、读档无法 `switch(CurrentPhase)` 重入 — source: ADR-0001/0005
- **Never 在 `Initialize` 注入 Seed** — 此时玩家配置尚未由 `StartNewGame` 落地 — source: ADR-0001
- **Never 让 `Deinitialize` 后残留悬挂异步加载回调写已 GC 对象**(PIE Stop 写空棋盘) — source: ADR-0001
- **Never use DataTable(`UDataTable`,CSV/JSON 行) 作棋盘载体** — CSV 不支持 `TArray` 列(5.7 仍报 `Unsupported Property type`,已核验),`RentTable`/`DiceMultiplierTable` 无法 CSV 导入 — source: ADR-0002
- **Never use 纯 C++ 硬编码 / `.ini`/`.json` 自解析作载体** — 违反 coding-standards「gameplay 数值外置、绝不硬编码」 — source: ADR-0002
- **Never 在 Active 对局中热替换 DA** — 须走完整 `Loaded→Validated→Active` 重初始化 — source: ADR-0002
- **Never 让下游各自硬引用 `UBoardDataAsset`** — 破坏接口隔离/可逆性 — source: ADR-0002
- **Never 用集中式 Event Bus 注册表对象**(单一 `UGameEventBus` 持全部 delegate) — 模糊「owner 即广播者」单一事件源不变式;易演变上帝对象 + 隐式全局耦合 — source: ADR-0003
- **Never 用混合事件拓扑**(高频 juice 走 owner delegate,低频跨域走集中 Bus) — 两套心智模型,维护性最差 — source: ADR-0003
- **Never 把破产事件合并为单事件**(删经济5 `OnBankruptcyDeclared`) — 破坏已 Approved 经济5 AC-36 契约;两事件语义正交 — source: ADR-0003
- **Never 把裸 `TArray` 作 BlueprintAssignable 参数**(编译失败——`OnTurnOrderResolved` 须包 USTRUCT) — source: ADR-0003
- **Never 把非 dynamic delegate 暴露给呈现层**(BP 不可订) — source: ADR-0003
- **Never 在订破产9 `OnPlayerBankrupt` 时重复播放已由 `OnBankruptcyDeclared` 触发的主反馈**(测试须断言「一次破产 → 主反馈恰 1 次」) — source: ADR-0003
- **Never 让呈现层回写或被反调**(呈现层 Exposes 为空,纯叶子不变式) — source: ADR-0003
- **Never 取消独立流让 VFX/Audio/HUD juice 抖动也调权威流** — 致命破坏重放:表现层抽取插入权威序列,同种子不同结果,联网 desync 根源 — source: ADR-0004
- **Never 旁路引擎全局 RNG**(`FMath::Rand`/`UKismetMathLibrary::RandomInteger`/BP `Random Integer in Range`)做需重放的 gameplay 随机 — source: ADR-0004
- **Never 用 `FRandomStream()` 默认构造(种子恒 0)初始化任何流** — 忘 SetSeed 的对局掷出相同确定序列 — source: ADR-0004
- **Never 在 `Min>Max` 时自动 swap** — swap 静默掩盖参数反向 bug — source: ADR-0004
- **Never 用 `UBlueprintFunctionLibrary` 作权威流宿主** — 无状态静态、不能持实例流 — source: ADR-0004
- **Never 用 JSON/文本序列化存档** — 须手写 JSON↔struct 映射,失去 `UPROPERTY(SaveGame)` 自动过滤 — source: ADR-0005
- **Never 在 MVP 实现完整版本迁移框架** — MVP 单槽,迁移路径本身成 bug 源(save-load F-2 已裁严格相等不迁移) — source: ADR-0005
- **Never 序列化全量棋盘布局** — 静态布局由 DA 重建;存 DA 引用不存布局 — source: ADR-0005
- **Never 手写 `FArchive Serialize()` 流** — 读写顺序须人工对齐、错位即静默损坏、无编译期保护 — source: ADR-0005
- **Never 用 SHA1 等加密哈希做 checksum** — 单机本地存档无对抗威胁,CRC32 即可 — source: ADR-0005
- **Never 在 MVP 制造多槽/云存/autosave/版本迁移接缝**(全 deferred) — source: ADR-0005
- **Never 让存档系统自裁任何被序列化字段的 gameplay 语义**(横切层 enable-not-own) — source: ADR-0005
- **Never 重复存派生量/`bIsBankrupt`/`bIsInJail`/MVP 骰子 Seed** — source: ADR-0005
- **Never 重排枚举整数索引**(破坏旧存档兼容) — source: ADR-0005

### Performance Guardrails
- **CPU 帧预算 16.6 ms / 60 FPS**(全 Foundation 系统共享上限) — source: ADR-0001~0006
- Board DA:**Load Time < 100ms**(对局初始化一次性);`GetTileData` ~0ms(O(1));**Memory < 0.1MB**/棋盘 — source: ADR-0002
- Event delegate broadcast O(订阅数),MVP 回合制低频可忽略;**同帧级联节流 `T_sfx_min_retrigger=0.06`**(已在 VFX19/Audio22 钉) — source: ADR-0003
- RNG `FRandomStream` LCG O(1) 抽取,非性能热点 — source: ADR-0004
- 存档:gameplay 16.6ms 帧不受影响(非帧路径);单 `USaveGame` 数百 KB;读档反序列化 + 重建 **< 1s** — source: ADR-0005

---

## Core Layer Rules

*Applies to: GameStateSnapshot 装配、BP/C++ 权威边界、Enhanced Input*

### Required Patterns

**GameStateSnapshot (ADR-0006)**
- **`FGameStateSnapshot` 为值语义 `USTRUCT`**,仅含 POD/容器成员(int32/bool/enum/`TArray<FTileSnapshotEntry>`/预派生标量),**绝不内嵌任何系统的指针或可写引用** — source: ADR-0006
- **AI 决策核心(C++)经 `const FGameStateSnapshot&` 接收,只读消费** — source: ADR-0006
- **snapshot 由回合2 编排在 AI 决策阶段开头一次性装配**(不是 AI 自建,不是各系统各自塞),装配挂回合2 状态机宿主(ADR-0001) — source: ADR-0006
- **snapshot 装配完成后在该决策阶段内冻结不变**;三 hook(`DecideBuyProperty`/`DecidePostRollActions`/`DecideJailAction`)看同一份视图 — source: ADR-0006
- 贪心循环用 AI **hook 内局部影子变量**(CR-7 预扣现金),**不回写也不重读 snapshot** — source: ADR-0006
- `AssembleSnapshot()` 签名建议 `FGameStateSnapshot AssembleSnapshot(int32 AiPlayerId) const`(返回值快照,调用点立即 `const&` 喂 hook) — source: ADR-0006
- **`Rent_top1/top2` 派生**:遍历全盘对手未抵押地产,按 piecewise `property_rent` 口径(垄断×2 仅作用 house=0 base)求单次租金取前两高;垄断判定经地产6 `is_monopoly` — source: ADR-0006
- **`PreaggregatedNlv` 由回合2 装配期聚合 6/8 —— AI 严禁自算**(economy Risk-3 防 5→8 反向环) — source: ADR-0006
- 派生标量 top-2/nlv 一律装配期算好(统一口径);现算仅限本就 per-tile 无跨系统聚合的逐格读取 — source: ADR-0006
- 派生逻辑直接调经济5 注册表 `property_rent`(不重定义) — source: ADR-0006
- 缺字段降级:装配方未填某必需字段,对应 hook 降保守默认(false/[]/RollDouble)+ 单条 `UE_LOG`(AC-48) — source: ADR-0006
- **三 hook 签名为 `const FGameStateSnapshot&`,与 player-turn CR-8 逐字一致**;须覆盖 ai-opponent CR-6 三 hook 全部实需字段(缺字段=AI 开工硬阻塞) — source: ADR-0006
- DI/可测:headless 测试构造 mock `FGameStateSnapshot` 字面量直接喂 hook — source: ADR-0006
- snapshot 是瞬态禁存档 — source: ADR-0006

**BP / C++ 权威边界 (ADR-0007)** — *亦见 Global CI 硬门*
- **写权威状态? → C++**。任何写 `PlayerState`/owner map/`house_count`/`Cash`/牌堆顺序/状态机 phase 落 C++ — source: ADR-0007
- **含 gameplay 随机? → C++ 且经 #3 `UFUNCTION`**。绝不用 BP 内置 Random 节点;juice 随机走独立非权威流仍经 C++ 封装 — source: ADR-0007
- **是 `[Logic] BLOCKING` AC 的被测逻辑? → C++ 独立 `UObject`/纯函数**。须 `-nullrhi` 可实例化(裸 `NewObject` 或 mock World)、可注入 `IGameClock`、暴露可 spy getter;不得依赖 `UMG NativeTick` 或 BP `Bind Event` graph 层 — source: ADR-0007
- 是 widget 树/动画/材质/菜单流/内容装配? → BP — source: ADR-0007
- 事件订阅→显示的转发 → BP 叶子,订阅 C++ `BlueprintAssignable` delegate,仅「收事件→播动画/SetText」,禁回写 — source: ADR-0007
- BP↔C++ handoff(如 OpenCard)→ C++ 暴露 `BlueprintCallable`,BP 经此入口(可 spy) — source: ADR-0007
- **RNG 唯一入口**:BP 须经 `UDiceRngService::RandomRange` 这一 `UFUNCTION`(唯一允许的 BP 随机来源) — source: ADR-0007
- 呈现状态机抽独立 `UObject`(非 UMG widget),经 `Init(TScriptInterface<IGameClock>)` 注入 DI — source: ADR-0007
- 「不可改的固定时长/参数」用 `static constexpr` + `static_assert`,对应 AC 从 `[Advisory·code-review]` 升 `[Logic]` — source: ADR-0007

**Enhanced Input (ADR-0011)** — *宿主归属 Foundation(ADR-0001)*
- **每个独立交互意图对应一个具名 `IA_` 资产**,Value Type 明确(`Digital`/`Axis2D`/`Axis1D`),Trigger/Modifier 在资产内配置不写死代码 — source: ADR-0011
- **三层 IMC**(`IMC_Gameplay` P=0 / `IMC_Menu` P=10 / `IMC_Modal` P=20)按优先级叠加切换,经 `AddMappingContext`/`RemoveMappingContext` 增删 — source: ADR-0011
- **AI 回合须从输入层杜绝 `IMC_Gameplay` 触发(移除 IMC,非 handler 内 if)** — source: ADR-0011
- Modal 层截获输入时 Gameplay 层 `IA_` 不触发 — source: ADR-0011
- 手柄追加仅需在对应 IMC 内增 Gamepad 键位映射,**无需改 C++ 代码** — source: ADR-0011
- 长按确认走 `IA` 资产内 `Hold` Trigger 配置(如 `IA_BuildHouse` `Hold Time=0.5s`) — source: ADR-0011
- **BindAction 宿主 = `UBoardInputManager : UWorldSubsystem`**(集中单一,不散落多 Actor;IMC 生命周期随对局 World,符合 ADR-0001) — source: ADR-0011
- `ActivateGameplayInput`/`DeactivateGameplayInput` 由回合2 在 `OnTurnStarted` 内据 `bIsAI` 调用;**不由 HUD 调用**(HUD 只读/只订阅) — source: ADR-0011
- `SwitchToModal`/`ReturnFromModal` 由 CommonUI 激活栈(ADR-0012)在 Widget 激活/关闭时调用 — source: ADR-0011
- **headless guard**:`-nullrhi` 下 `GetLocalPlayer()` 可能返回 null;`GetEISubsystem()` 须做 null guard,返 null 时 `UE_LOG` 警告并早返,不 crash — source: ADR-0011
- 资产创建顺序:先 `IA_*` → 再 `IMC_*` → 最后 `UBoardInputManager` C++ + `BP_BoardInputManager` 赋值资产引用 — source: ADR-0011
- `bGameplayInputActive`/`bModalActive` 防重复 Add/Remove;`UBoardInputManager` 加 `ShouldCreateSubsystem` override,依赖资产 null 早返 + fatal log — source: ADR-0011
- 资产路径:IA 放 `Content/Input/Actions/`,IMC 放 `Content/Input/Contexts/`,C++ 类放 `src/input/BoardInputManager.h/.cpp` — source: ADR-0011

### Forbidden Approaches
- **Never 让 AI 自建 snapshot**(直取各系统单例如 `GetWorld()->GetSubsystem<UEconomy>()`) — 破坏纯叶子、无法 headless 注入 mock — source: ADR-0006
- **Never 传 AI 一组 const 只读接口指针**(`const IEconomyRead*`) — const 接口仍是活指针,决策期内可中途变更,违冻结/重放 — source: ADR-0006
- **Never 让派生量全部 hook 内现算**(不预聚合 top-2 与 nlv) — `nlv` 现算须触达经济 F-9 → 5→8 反向环(economy Risk-3 禁止) — source: ADR-0006
- **Never 各系统各自向 snapshot 推送切片**(5/6/8 各自 `ContributeToSnapshot()`) — 装配点分散、top-2 无单一归属 — source: ADR-0006
- **Never 让 AI 经 snapshot 写任何游戏状态/反向触达系统状态** — source: ADR-0006
- **Never 采用混合口径**(nlv 预聚合、top-2 现算) — 违统一口径 — source: ADR-0006
- **Never 在 snapshot 装配后中途变更它** — 破坏冻结、塌陷纯函数性与重放 — source: ADR-0006
- **Never 用 BP 内置 Random 节点触碰 gameplay** — 走引擎全局 RNG,绕过骰子权威流 — source: ADR-0007
- **Never 让 BP 呈现层回写权威状态** — BP 仅订阅·显示·转发(纯叶子) — source: ADR-0007
- **Never 让权威逻辑出现 BP 派生类** — `uasset` 二进制不可 git diff / 不可静态审 / 不可 grep — source: ADR-0007
- **Never 采用「权威逻辑全 C++」** — 蔓延到无确定性需求的呈现/装配,违务实性 — source: ADR-0007
- **Never 采用「BP 为主 + 约定」** — 确定性义务不可机器执行、`[Logic]` AC 大面积 false-green — source: ADR-0007
- **Never 用纯 code-review 软约束禁随机** — 不可机器强制,被「C++ 文本 grep 硬门 + 目录级断言」廉价取代 — source: ADR-0007
- **Never 让 `[Logic] BLOCKING` AC 的被测逻辑依赖 `UMG NativeTick` 或 BP `Bind Event` graph 层** — `-nullrhi` 下 NativeTick 不触发、BP 绑定 C++ spy 不可见 → false-green — source: ADR-0007
- **Never use legacy `InputComponent->BindAction()`/`BindAxis()`/`GetInputAxisValue()`** — 已弃用,5.7 须全量 Enhanced Input — source: ADR-0011
- **Never use 单一 IMC + handler 内 `if(bIsMyTurn)`** — AI 越权防线降为代码层单点失效;手柄追加需改代码 — source: ADR-0011
- **Never use PlayerController `SetupPlayerInputComponent` 内绑定** — 与 ADR-0001 `UWorldSubsystem` 宿主策略分裂 — source: ADR-0011
- **Never use 分散在各 Widget(HUD BP/卡牌 UI BP) BindAction** — 违 ADR-0007 权威边界,无法 headless 测、无法统一 AI 越权阻断 — source: ADR-0011
- **Never use 共享 IA + 上下文区分** — 同一 IA 跨 IMC 语义混淆 — source: ADR-0011

### Performance Guardrails
- **单 AI 回合(装配+三 hook)总计 ≤16ms**(60FPS 单帧);snapshot 装配 <0.1ms(≤40 格 POD 拷贝);hook 传参 0(`const&` 零拷贝) — source: ADR-0006
- `FGameStateSnapshot` <2.5KB/快照(固定标量 + 40×`FTileSnapshotEntry`~48B),决策期单份阶段末释放 — source: ADR-0006
- 权威逻辑 C++ 热路径优于等价 BP VM,回合制低频差异非瓶颈;CPU 预算 16.6ms — source: ADR-0007
- 输入轮询 CPU < 0.1ms;IA+IMC 资产内存 < 0.5MB(9 IA + 3 IMC);`AddMappingContext`/`RemoveMappingContext` 帧内调用无 > 0.5ms 峰值 — source: ADR-0011

---

## Feature Layer Rules

*Applies to: AI 对手(#10) 纯叶子消费纪律 —— 决策核心落点见 Core(ADR-0006/0007)*

### Required Patterns
- **AI 决策核心是纯叶子**:只经 `const FGameStateSnapshot&` 只读消费,不直依赖经济5/地产6/建房8 活指针(CR-1 纯叶子) — source: ADR-0006
- **AI 决策扰动(F-3 Epsilon、tiebreak)走权威流**经 `RandomRange`/`Float01`(可重放) — source: ADR-0004
- **AI 内部 juice/扰动若属表现层**走独立非权威流,**严禁经 DiceService 权威 API 取随机** — source: ADR-0004
- AI 贪心循环现金预扣用 hook 内局部影子变量,不回写/重读 snapshot(AC-62) — source: ADR-0006
- AI 决策逻辑落 C++(ADR-0007),不依赖 BP 读 TMap;headless 可注入 mock snapshot 测 — source: ADR-0006/0007
- AI 回合输入越权阻断在**输入层**(移除 `IMC_Gameplay`),非 handler 内 if — source: ADR-0011

### Forbidden Approaches
- **Never 让 AI 自建 snapshot 或经 snapshot 写/反向触达系统状态**(见 Core/ADR-0006) — source: ADR-0006
- **Never 让 AI 决策扰动复用表现层 juice 流,或让 juice 抖动调权威流** — source: ADR-0004
- **Never 让 AI 自算 `PreaggregatedNlv`**(5→8 反向环) — source: ADR-0006

---

## Presentation Layer Rules

*Applies to: HUD、卡通材质、音频、CommonUI*

### Required Patterns

**HUD 动画驱动 + IGameClock DI (ADR-0008)**
- **counter 动画由 NativeTick(C++ 侧每帧)实现**,须每帧 `SetText(FText::AsNumber(round(displayed(t))))` — source: ADR-0008
- **HUD 状态机抽独立 `UHudStateMachine : public UObject`**(非 `UUserWidget`,不持 Slate 引用),可在 `-nullrhi` 下裸 `NewObject` 实例化 — source: ADR-0008
- 把所有可 headless 测的逻辑(面板状态机转换、`E_cur` 回合代次、`unpaused_elapsed` 累计、F-4 过期判定、F-1 时长公式、F-5 beat 生命周期)抽入 `UHudStateMachine` — source: ADR-0008
- **时钟依赖经 `IGameClock` 接口注入**(生产 `FWorldGameClock`,测试 `FMockGameClock`);非直读 `GetWorld()->GetTimeSeconds()` — source: ADR-0008
- Widget 树/布局/动画资产保留 WBP_ 内,C++ 状态机经 `meta=(BindWidget)` 持控件引用并设值;对控件写操作全部在 `ApplyStateMachineOutput()` — source: ADR-0008
- **C++ 路径用 `AddDynamic`**(`AddUnique` 去重);读档仍 `RemoveDynamic` 后 `AddDynamic` 防 Widget 重建残留 — source: ADR-0008
- **BP 路径**(`Bind Event`)读档重绑**必须先 `Unbind Event from` 再重绑**(EC-11/AC-48);code-review 须核验(AC-48 [Integration]) — source: ADR-0008
- **NativeTick 优化**:`UUserWidget` 须设 `bCanEverTick=true`;**静止态(无活跃 Counter/Banner/AI 队列)StateMachine::Tick 做 early-return** — source: ADR-0008
- **reduce_motion 从 `DA_HudConfig`(Primary Data Asset)读取 `bReduceMotion`,不硬编码**,`Init` 时读入 — source: ADR-0008
- `E_cur` 每收一次 `OnTurnStarted`(不含双点额外回合)`E_cur++`;主判据 `E_cur > E_action(i)` 优先,辅判据 `unpaused_elapsed > T_stale` 软上界 — source: ADR-0008
- Counter EC-1(中途重定向):新 `OnCashChanged` 到达若有活跃 Counter,以当前插值显示值(`GetDisplayedCash`)为新 `OldCash` 重启,不排队 — source: ADR-0008
- 推荐事件处理在 C++ 用 `AddDynamic`,BP 仅 Widget 树配置;若 BP 绑定不可避免须配 `Unbind`,在 `NativeDestruct` 与 `HandleGameLoaded` 各调一次 — source: ADR-0008

**卡通材质管线 (ADR-0009)**
- **建 `M_toon_unlit_base` 父材质(Unlit,`BaseColor`=参数)**,派生 `MI_char_token_*`/`MI_bld_house_*` 实例;只在父材质改节点图,实例只改参数(art-bible §8.2 `M_`/`MI_` 前缀) — source: ADR-0009
- Cel Shade 封装为 `MaterialFunction MF_CelShade(BaseColor, LightDir, Steps)`,`Steps` 控档数(2–4) — source: ADR-0009
- **棋子描边用双 pass `M_inverted_hull_outline`,不用 Post Process 全屏描边**;Outline_Width 随相机距离缩放(Camera Distance 除法节点) — source: ADR-0009
- 关卡放一个 `APostProcessVolume`(`bUnbound=true`),VFX#19 持引用(`TObjectPtr<APostProcessVolume>`,`UPROPERTY()`);默认 `BlendWeight=0`,`OnGameWon` 设 1.0;**`OnBankruptcyDeclared` 不触发全屏 PP**(破产是逐 pawn 材质参数) — source: ADR-0009
- **所有 Static Mesh 不勾 Enable Nanite Support**;材质不用 Emissive 主驱动 Lumen(art-bible §8.5);需发光用 Additive Blend 或 PP Material — source: ADR-0009
- **Project Settings > Engine > Substrate 显式禁用**(5.7 默认开启,须显式关) — source: ADR-0009
- `UMaterialInstanceDynamic` 须 `UPROPERTY()` 防 GC,pawn BeginPlay 时 `Create`,GC 回收(不手动 delete) — source: ADR-0009
- 描边宽度/色阶数/暖金强度/去饱和速度全置 `DA_MaterialConfig`,不硬编码 — source: ADR-0009
- 玩家颜色注入每 pawn 一个 MID(`UMaterialInstanceDynamic::Create`),C++ 权威控制(ADR-0007) — source: ADR-0009

**音频架构 (ADR-0010)**
- **程序化音效引擎选 MetaSound(`UMetaSoundSource`)**(5.7 production-ready,node graph 可 diff);基础 2D 播放用 `PlaySound2D`/`UAudioComponent` — source: ADR-0010
- **事件订阅层 = C++**(随 ADR-0007,headless 可测硬前提) — source: ADR-0010
- **音频随机走 #22 自建独立非权威 `FRandomStream`**(继承 ADR-0004 流隔离) — source: ADR-0010
- **中间件 = 纯原生**(MVP,Wwise/FMOD 降 Full Vision) — source: ADR-0010
- G-1:程序化逻辑(die 变体、bIsDouble 分支、pitch 抖动)用 MetaSound Source,C++ 经 `SetBoolParameter`/`SetFloatParameter` 注入参数(不在 C++ 手写 if-else 逐资产切换) — source: ADR-0010
- G-2:每 SFX 资产 Sound Class=SC_SFX、Submix Send=SSX_SFX;BGM Sound Class=SC_BGM、Submix=SSX_BGM — source: ADR-0010
- G-3:`SCY_SFX` `MaxCount=32`(`N_voice_max`)Stop Oldest;**Critical(破产/胜利音)用独立 `SCY_Critical` `MaxCount=4` 不被抢占**(引擎不支持则升 C++ VoiceManager) — source: ADR-0010
- G-3:**ThrottleGate(F-3)用 C++ per-Cue `TMap<FName,float> LastPlayTime`**,`(time - LastPlayTime[id]) >= T_sfx_min_retrigger` 才通过 — source: ADR-0010
- G-4:**`BindAll()`/`UnbindAll()` 集中管理**,在 Subsystem `Initialize`/`Deinitialize`/`OnGameLoaded` 调用;每 delegate `AddDynamic`,读档先 `Remove` 再 `AddDynamic` — source: ADR-0010
- G-5:`FAudioRNG` 在 `Initialize` 以系统时间或固定 seed(测试)初始化;MVP 不序列化 Seed — source: ADR-0010
- G-6:**[Logic] AC(AC-1~22)在 `-nullrhi` 经 mock spy(`UAudioPlayback_Mock`)断言**,要求订阅层与播放层间有可注入「播放接口」(非直接调 `PlaySound2D`) — source: ADR-0010
- BGM 用 `UAudioComponent`(`FadeIn`/`FadeOut`),`T_bgm_crossfade=0.5s`;BGM 流式加载(`Streaming=true`),SFX 全加载(`Streaming=false`) — source: ADR-0010
- `SetBusVolume(EAudioBus Bus, float Volume)` Volume∈[0,1],#23 设置屏调 — source: ADR-0010

**CommonUI 输入路由 (ADR-0012)**
- **所有屏派生 `UCommonActivatableWidget`**,override `GetDesiredFocusTarget` 返回手柄默认焦点 — source: ADR-0012
- **所有按钮派生 `UCommonButtonBase`,禁直接用 `UButton`**(无手柄/输入无关支持) — source: ADR-0012
- **激活栈宿主**:根 `UCommonActivatableWidgetStack` 挂 HUD/GameViewport;modal(地产卡/确认框)push 栈顶,菜单/HUD 为底层屏 — source: ADR-0012
- **激活栈 ↔ IMC 协作**:modal 屏 `NativeOnActivated`/`NativeOnDeactivated` 调 ADR-0011 `SwitchToModal`/`ReturnFromModal`(BP 经 `GetSubsystem` → C++) — source: ADR-0012
- **启用 CommonUI 插件**:Project Settings → Plugins → CommonUI;核验是否 Experimental(影响 Shipping cook);配置 `UCommonUIInputData` — source: ADR-0012
- code-review 核对 push/pop ↔ SwitchToModal/ReturnFromModal 配对 — source: ADR-0012

### Forbidden Approaches
- **Never 全部逻辑住 `UUserWidget`(NativeTick 直驱不抽独立 UObject)** — `-nullrhi` 下 NativeTick 不触发 → AC-12/24a/30/57 全 false-green — source: ADR-0008
- **Never 用 `UWidgetAnimation`(UMG sequencer/Timeline)+ `SetPlaybackRate` 做 counter 动画** — 无整数文本插值轨道,终点精度无法保证 — source: ADR-0008
- **Never 用 Blueprint Timeline 节点驱动 counter(不抽独立 UObject)** — BP Timeline 住 UUserWidget 生命周期,`-nullrhi` 下不触发 — source: ADR-0008
- **Never 用 MVVM 插件做 counter 逐帧插值** — 绑定依赖 property change 通知,不适合逐帧;MVVM 5.4+ post-cutoff 须 spike — source: ADR-0008
- **Never 在 reduce_motion 等 Tuning Knobs 上硬编码**(须从 `DA_HudConfig` 读) — source: ADR-0008
- **Never 采用 Substrate 全量** — Substrate Unlit 节点存在性是 post-cutoff 盲区,卡通 2.5D 无 PBR 收益(推迟 Alpha 评估) — source: ADR-0009
- **Never 采用混合路径(Legacy 主体 + Substrate PP 局部)** — 同时继承两套缺点,维护开销加倍 — source: ADR-0009
- **Never 用 Post Process 全屏描边替代 Inverted Hull** — PP 描边框住所有物体,Inverted Hull 只框指定 mesh — source: ADR-0009
- **Never 在 `OnBankruptcyDeclared` 触发全屏 PP** — 破产是逐 pawn 材质参数(`DesaturationAmount`) — source: ADR-0009
- **Never 手动 delete `UMaterialInstanceDynamic`** — 由 GC 回收(须 `UPROPERTY()` 标记) — source: ADR-0009
- **Never 全部用 Sound Cue(不引入 MetaSound)实现程序化逻辑** — Sound Cue 复杂逻辑已弃用,`.uasset` 二进制无法 git diff — source: ADR-0010
- **Never 引入 Wwise/FMOD(MVP)** — 需求规模与中间件成本不成比例,Reversibility 极低 — source: ADR-0010
- **Never 在 BP 层订阅 owner delegate** — 违 ADR-0007;AC-21 防双绑 spy 在 BP 无法注入 mock(false-green);BP `uasset` 不可 diff — source: ADR-0010
- **Never 在播放执行层直接调 `PlaySound2D`** — 须经 IAudioPlaybackInterface 注入点,否则 headless [Logic] AC 不可 spy — source: ADR-0010
- **Never 在音频随机调 `FMath::RandRange`/`FMath::FRand`** — 旁路 `FAudioRNG`/污染权威流 — source: ADR-0010
- **Never 用纯 UMG 手工实现激活栈/手柄导航/modal 截获** — 等价重造 CommonUI,后期迁移须重构全部屏 — source: ADR-0012
- **Never 用 CommonUI 仅按钮 + 裸 UMG 屏管理** — 半套方案不获完整收益反增混用复杂度 — source: ADR-0012
- **Never 直接用 `UButton` 作按钮 / Never hover-only 交互** — 所有交互须有 OnClicked/确认路径 — source: ADR-0012

### Performance Guardrails
- **HUD 帧预算目标 < 2ms**(含 WBP 渲染);有动画 ~0.3ms,静止态 ~0.05ms(early-return);4 面板活跃 counter 时 HUD Tick CPU ≤ 0.5ms — source: ADR-0008
- `UHudStateMachine` ~2KB/instance — source: ADR-0008
- 棋子材质 CPU < 0.1ms/draw(Unlit);Inverted Hull +N draw(N=同屏棋子≤4,<1ms);Victory PP < 0.5ms;破产去饱和 CPU < 0.01ms;MID 池 < 10 个 — source: ADR-0009
- 音频 CPU ~0.5–1.0ms(32 voice,音频线程独立);内存 ~20–40MB(BGM 流式降峰);Voice 峰值节流后 ≤ 32 — source: ADR-0010
- CommonUI UI tick CPU < 0.3ms;内存 < 2MB — source: ADR-0012

---

## Global Rules (All Layers)

### Naming Conventions
| Element | Convention | Example |
|---------|-----------|---------|
| Classes | 前缀 PascalCase(`A`=Actor、`U`=UObject、`F`=struct、`E`=enum) | `ABoardPawn`、`UPropertyData`、`FBoardTileData`、`ETurnPhase` |
| Variables | PascalCase;布尔加 `b` 前缀 | `bIsBankrupt`、`MoveSpeed` |
| Signals/Events | `On<PastTense>` PascalCase(C++ Multicast Delegate / BP Event) | `OnRentPaid`、`OnTurnEnded`、`OnCashChanged` |
| Files | 匹配类名去前缀 | `BoardPawn.h` / `BoardPawn.cpp` |
| Blueprint 资产 | `BP_` 前缀 | `BP_BoardManager` |
| UMG Widget | `WBP_` 前缀 | `WBP_HUD` |
| Data Asset | `DA_` 前缀 | `DA_Board`、`DA_HudConfig`、`DA_MaterialConfig` |
| Material / Instance | `M_` / `MI_` 前缀(art-bible §8.2) | `M_toon_unlit_base`、`MI_char_token_P1` |
| Input Action / Context | `IA_<意图>` / `IMC_<上下文>` | `IA_BuildHouse`、`IMC_Gameplay` |
| Constants | UPPER_SNAKE_CASE 或 `static constexpr` PascalCase | `CURRENT_SAVE_VERSION` |
| 关卡 | `.umap` | — |

### Performance Budgets
| Target | Value |
|--------|-------|
| Framerate | 60 FPS |
| Frame budget | 16.6 ms |
| Draw calls | 适中(回合制单场景,远低于动作游戏);具体上限待目标硬件 |
| Memory ceiling | 待目标硬件确定(可后调) |

> 子系统分项预算见各 Layer Performance Guardrails。

### Approved Libraries / Addons
- **MetaSound**(`UMetaSoundSource`) — 程序化音效引擎(ADR-0010)
- **Enhanced Input**(Input Action + Input Mapping Context + `UEnhancedInputComponent`) — 全部输入(ADR-0011)
- **CommonUI** 插件 — 全部呈现层屏/按钮/激活栈(ADR-0012,⚠ 须核验 5.7 是否 Experimental)
- **Legacy Unlit 材质系统**(Inverted Hull / Custom HLSL) — 卡通材质(ADR-0009;Substrate 留 Alpha go-no-go)
- *未批准*:Wwise/FMOD、MVVM 插件、GAS、Nanite/Lumen/Megalights/Substrate(本项目卡通 2.5D 显式 defer/禁用)

### Forbidden APIs (UE 5.7)
来自 `docs/engine-reference/unreal/deprecated-apis.md`(该文件不带逐行 `since X.Y`,版本由主题推断):
- `InputComponent->BindAction()` — deprecated(legacy input) → Enhanced Input `BindAction()`
- `InputComponent->BindAxis()` — deprecated(legacy input) → Enhanced Input `BindAxis()`
- `PlayerController->GetInputAxisValue()` — deprecated(legacy input) → Enhanced Input Action Values
- Legacy material nodes — deprecated(Substrate production-ready 5.7) → Substrate(**本项目例外**:ADR-0009 裁定 5.7 仍完整支持 Legacy Unlit,warning-only 非 hard break,MVP 用 Legacy)
- Forward shading(default) — deprecated → Deferred + Lumen(**本项目 defer Lumen**)
- Old lighting workflow — deprecated → Lumen GI(**本项目 defer**)
- UE4 World Composition — deprecated → World Partition
- Level Streaming Volumes — deprecated → World Partition Data Layers
- Old animation retargeting — deprecated → IK Rig + IK Retargeter
- Legacy control rig — deprecated → Control Rig 2.0
- `UGameplayStatics::LoadStreamLevel()` — deprecated → World Partition streaming
- Hardcoded input bindings — deprecated → Enhanced Input
- Cascade particle system — fully deprecated → Niagara
- Old audio mixer — deprecated → MetaSounds
- Sound Cue(for complex logic) — deprecated for complex logic → MetaSounds(ADR-0010)
- `DOREPLIFETIME()`(basic) — deprecated(basic form) → `DOREPLIFETIME_CONDITION()`(Full Vision 联网)
- `TSharedPtr<T>` for UObjects — deprecated for UObjects → `TObjectPtr<T>`
- Manual RTTI checks — deprecated → `Cast<T>()` / `IsA<T>()`

### Cross-Cutting Constraints (横切全层)
- **CI 随机硬门(ADR-0007)**:对权威 C++ 模块(AI/经济/建房/破产/回合/RNG)`grep` 禁 `FMath::Rand`、`FMath::RandRange`、`FMath::FRand`、`rand(`、`std::rand`(文本硬门);**白名单仅 `UDiceRngService` 内 `FRandomStream` 调用** — source: ADR-0007
- **CI 目录级断言(ADR-0007)**:权威模块目录下**无 BP 派生类资产**(结构保证无 BP 随机旁路);Alpha 若 BP 触 gameplay 边缘增 BP 静态符号扫描检出裸随机 K2Node — source: ADR-0007
- **RNG 流隔离(ADR-0004)**:权威随机(骰点/牌堆 Fisher-Yates/AI 决策扰动)走唯一权威流;表现层 juice(VFX 节奏/火花、Audio 音高/起始偏移、HUD juice)各走独立非权威流;**两者绝不混用** — source: ADR-0004
- **事件命名/payload 纪律(ADR-0003)**:`On<PastTense>` + 多字段必包 `USTRUCT(BlueprintType)` + 字段只增不改语义 + 单一 owner 广播;新增订阅同步更新 architecture.md §D.2 权威矩阵 — source: ADR-0003
- **禁 hover-only 交互(ADR-0011/0012)**:所有 IA 为 Digital 点击/确认,所有可操作元素有 OnClicked/确认路径(为手柄/客厅模式预留) — source: ADR-0011/0012
- **数据驱动(coding-standards + ADR-0009/0008)**:gameplay 数值与可调参数外置 Data Asset,绝不硬编码 — source: ADR-0002/0008/0009
- **UObject 指针安全(current-best-practices)**:UObject 引用成员用 `TObjectPtr<T>` + `UPROPERTY()` 防 GC,不用裸指针/`TSharedPtr<T>`;⚠ ADR-0008 `IGameClock*` 裸指针须改 `TWeakObjectPtr`/`TScriptInterface`(specialist finding,实现期落地) — source: current-best-practices.md / architecture-review-2026-06-06
- **`UFUNCTION()` 暴露(current-best-practices)**:BP 暴露用 `UFUNCTION(BlueprintCallable)`/`BlueprintImplementableEvent` — source: current-best-practices.md
- **Tick 节制(current-best-practices)**:慎用 Event Tick,优先 timers/events;缓存频繁访问的组件(不每帧 GetComponent) — source: current-best-practices.md
- **现代 C++(current-best-practices)**:UE 5.7 用 C++20(`TObjectPtr`、structured bindings、concepts) — source: current-best-practices.md

> **不适用本项目的 best-practices**(engine-ref 默认推荐但本项目 defer):GAS(无复杂能力系统)、Server-authoritative replication(MVP 无网,Full Vision 再启)、HISM(单场景棋盘暂不需要)、World Partition + Data Layers(MVP 单关卡)、Niagara(VFX 用,非本表强制)。

---

## Engine Verification Required (Sprint 0)

以下 post-cutoff(LLM 知识 ~5.3)待实测行为须**开工首步**验证;失败须修订对应 ADR 并重 Accept。

| # | 验证项 | ADR | 风险 |
|---|--------|-----|------|
| 1 | **CommonUI 插件在 5.7 是否仍 Experimental**(影响 Shipping cook);`UCommonActivatableWidget`/`UCommonActivatableWidgetStack`/`UCommonButtonBase` 签名与激活回调名(`NativeOnActivated`/`NativeOnDeactivated`) | ADR-0012 | **HIGH(blocking check)** |
| 2 | `-nullrhi` headless 下 `UUserWidget::NativeTick` **不触发**(抽独立 UObject 的根因);非 -nullrhi 下每帧触发 | ADR-0008 | HIGH |
| 3 | `UEnhancedInputLocalPlayerSubsystem::AddMappingContext` 在 `-nullrhi` 不 crash;IMC 优先级截获(`IMC_Modal` P=20 截获 `IA_Roll`);AI 回合移除 `IMC_Gameplay` 后 trigger 不触发 | ADR-0011 | HIGH |
| 4 | `FRandomStream::RandRange(int,int)` 是否仍走 `RandHelper: Min + TruncToInt(FRand()*Range)` 浮点中介;默认构造种子是否仍恒 0;是否新增官方确定性/网络 RNG 子系统 | ADR-0004 | MEDIUM |
| 5 | `USoundSubmix` + `PushSoundMixModifier` 是否被新 Audio Modulation/Audio Bus 部分替代(影响三路混音 + `SetBusVolume`);`UAudioComponent::SetFloatParameter(FName,float)` 5.7 签名;MetaSound 在 `-nullrhi` 参数注入可否 mock | ADR-0010 | MEDIUM |
| 6 | `UAssetManager` 同步/异步加载 Primary DA 的 5.7 API 签名;DataTable CSV 含 `TArray` 列报错二次确认;Blueprint `Floor(float)→int32` 重载类型推导 | ADR-0002 | MEDIUM |
| 7 | `UWorldSubsystem::Initialize/Deinitialize` 随 PIE World 正确触发;`OnWorldBeginPlay` 在 PIE Stop→Start 重触发;`Deinitialize` `CancelHandle` 后无悬挂回调 | ADR-0001 | LOW |
| 8 | `UPROPERTY(SaveGame)` 嵌套 USTRUCT/`TArray`/`TMap` 递归序列化 round-trip 冒烟;`FObjectAndNameAsStringProxyArchive` 过滤行为;Windows/Steam `IFileManager::Move` 原子替换语义 | ADR-0005 | LOW |
| 9 | Substrate Unlit 节点存在性;Post Process Material 在 5.7 Substrate 体系接口;Inverted Hull 在 Substrate 开启后兼容性;Legacy Unlit 是否 warning-only(确认非 hard break) | ADR-0009 | MEDIUM |
| 10 | `DYNAMIC_MULTICAST_DELEGATE` 裸 `TArray` 作 BlueprintAssignable 仍编译失败(`OnTurnOrderResolved` 须包 USTRUCT);BP `AddDynamic`/`RemoveDynamic` 读档重绑幂等性 | ADR-0003 | LOW |

> 详细验证清单见各 ADR「Engine Compatibility / Verification Required」节 +
> `docs/architecture/architecture-review-2026-06-06.md`。
