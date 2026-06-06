# Architecture Review — 2026-06-06(全量覆盖降级版)

> **Reviewer**: technical-director
> **Scope**: 12 ADR(ADR-0001 ~ ADR-0012,全 Accepted)+ **全 16 系统 266 条 TR** 追溯
> **Engine**: Unreal Engine 5.7(Blueprint 为主,C++ 框架/性能层)
> **Verdict**: **CONCERNS**(详见 §8;修正旧版 FAIL — 旧 FAIL 因覆盖只跑 1/16 GDD + 性能预算口径误判)
> **Related**: [`requirements-traceability.md`](./requirements-traceability.md) · [`tr-registry.yaml`](./tr-registry.yaml) · [`architecture.md`](./architecture.md)

---

## 1. Traceability Summary(全量计数)

| 指标 | 数量 | 占比 |
|------|------|------|
| **Total TR**(16 系统) | 266 | 100% |
| Covered | 180 | 67.7% |
| Partial | 63 | 23.7% |
| Gap | 23 | 8.6% |
| **≥ Partial(至少部分覆盖)** | 243 | 91.4% |

### 分系统

| 系统 | Total | Covered | Partial | Gap | | 系统 | Total | Covered | Partial | Gap |
|------|------:|--------:|--------:|----:|---|------|------:|--------:|--------:|----:|
| board | 17 | 15 | 2 | 0 | | bank | 19 | 6 | 7 | 6 |
| turn | 17 | 15 | 2 | 0 | | ai | 13 | 11 | 2 | 0 |
| dice | 17 | 13 | 4 | 0 | | hud | 21 | 17 | 4 | 0 |
| move | 18 | 7 | 8 | 3 | | card | 19 | 13 | 4 | 2 |
| econ | 18 | 12 | 4 | 2 | | vfx | 16 | 8 | 5 | 3 |
| prop | 15 | 8 | 4 | 3 | | menu | 12 | 7 | 3 | 2 |
| event | 11 | 4 | 5 | 2 | | save | 17 | 15 | 2 | 0 |
| build | 15 | 11 | 4 | 0 | | audio | 21 | 18 | 3 | 0 |

- 追溯覆盖**健康**:243/266(91.4%)≥ Partial。12 ADR 的「GDD Requirements Addressed」节对 16 系统形成系统性覆盖。
- **旧版口径修正**:上一版 review 仅枚举 save/load 一系统 25 条 TR(占总量约 9%),据此判 FAIL。本版枚举全 16 系统;追溯侧无任何系统沦为「无 ADR 覆盖」的系统级真空。
- 23 条 Gap 高度集中:其中 **5 条**(prop-001/002/012、bank-004/011)同属一个 OQ-Prop-1 接缝(运行态 owner-map 容器 + 原子移交 + 重入锁),其余 18 条为 propagate 债 / `/ux-design` / 性能 checklist / asset-spec,**非系统级覆盖缺失**。

---

## 2. Coverage Gaps(汇总,逐条见 traceability §Known Gaps)

| 类别 | 数量 | 是否需新 ADR | owner |
|------|-----:|-------------|-------|
| OQ-Prop-1 单一接缝(owner-map 运行态容器/原子移交/重入锁) | 5(prop-001/002/012, bank-004/011) | **需新 ADR(一次裁定)** | architect |
| 经济整数纯净跨编译一致(econ-014) | 1 | 建议补进 ADR-0007 | lead-programmer |
| 破产瞬态守卫禁存档(bank-014) | 1 | 建议补进 ADR-0005 | lead-programmer |
| propagate 债(接缝登记/逐事件背书) | 8 | 否 | producer |
| Pre-Production `/ux-design`(a11y/reduce_motion) | 3 | 否 | ux |
| 性能 checklist(破产/VFX 帧预算标定) | 2 | 否 | performance-analyst |
| asset-spec / 算法澄清 / 实现纪律 | 3 | 否 | art / designer / lead-programmer |

> 仅 **OQ-Prop-1 单一接缝(5 条)需新 ADR**,建议下游建房8/破产9 实现前一次 `/architecture-decision` 统一裁定。其余 18 条 Gap 均为传播/Pre-Production/性能/资产类,**不构成阻塞架构通过的裁定缺口**。

---

## 3. Cross-ADR Conflicts(4 处 — 均文档同步/协同,非真阻塞)

> **结论先行**:4 处全部为**文档同步或职责切分**类,**无真实裁决冲突、无依赖环**。其中 CONFLICT-1(ADR-0009)与 CONFLICT-2(ADR-0006)的文档同步**已在本批修正**,CONFLICT-3/4 为实现期协同约定(control-manifest 固化)。

### CONFLICT-1 — ADR-0009 ↔ ADR-0007(依赖状态 stale)— **已修**
- ADR-0009 的 Depends-On/Related 把已 Accepted 的 ADR-0007 标 **Proposed**,且 Related 链接写成 `ADR-0007-bp-vs-cpp-boundary.md`(实际 `ADR-0007-bp-cpp-boundary.md`),致 `/story-readiness` 误判 0009 依赖未决。
- **处置(纯文档同步)**:0007 状态 Proposed→Accepted,链接路径修正为 `ADR-0007-bp-cpp-boundary.md`。**本批已修。**

### CONFLICT-2 — ADR-0006 ↔ ADR-0007(依赖图方向不一致)— **已修**
- ADR-0007 Enables 列 ADR-0006(强 enable);ADR-0006 const& 签名按 0007 设计,但其 Depends-On 表只列 0001/0002,把 0007 降为弱 "Coordinates with",拓扑排序若只读 0006 表会丢边。
- **处置**:ADR-0006 Depends-On 补列 ADR-0007;拓扑取较强声明,**0007 先于 0006**。**本批已修。**

### CONFLICT-3 — ADR-0005 ↔ ADR-0004(种子非确定源 — 协同非冲突)
- 两 ADR 口径一致(均不存 Seed;牌堆走 model B 数组顺序)。需固化的接缝:读档拓扑序第4步「骰子 `SetSeed(非确定值)`」与 ADR-0004「读档重设非确定种子」须用**同一非确定源**(如 `FPlatformTime::Cycles()`),由存档服务统一注入。
- **处置(实现 checklist)**:control-manifest 固化单一非确定源约定。**非裁决冲突。**

### CONFLICT-4 — ADR-0008 ↔ ADR-0003(读档重绑职责切分 — 协同非冲突)
- 同一防双订阅(EC-8)目标:ADR-0003 集中工具走 owner 侧 delegate 生命周期编排;ADR-0008 各 Widget 自 `HandleGameLoaded` 先 Unbind 后 Bind。落点不同有双重解绑/互相推诿之嫌。
- **处置(明确分层)**:**集中广播 + 各叶子自解绑** — ADR-0003 集中工具负责 owner 侧编排与广播 `OnGameLoaded`;呈现层 widget 收信号后各自 Unbind→Bind(0008 路径)。ADR-0005 IG-7 已采此模型。control-manifest 注明该分工。**非裁决冲突。**

> **总结**:4 处均可在 control-manifest + ADR 文档同步层闭合,**无需重裁任何 ADR**,无依赖环。

---

## 4. ADR Dependency Order(拓扑序)

```
ADR-0001 → ADR-0002 → ADR-0003 → ADR-0004 → ADR-0007
  → ADR-0005 → ADR-0006 → ADR-0008 → ADR-0009
  → ADR-0011 → ADR-0010 → ADR-0012
```

- **Cycles**:无。
- **关键链**:`ADR-0007 → ADR-0005 → ADR-0006`(BP/C++ 边界 → 存档容器 → snapshot 契约)。
- `ADR-0006 → ADR-0007` 边取较强声明(0007 先于 0006,CONFLICT-2 已修)。
- 与 `architecture.md` §8 依赖链、`requirements-traceability.md` 拓扑序一致。

---

## 5. GDD Revision Flags(4 条 — Sprint0 engine 验证后再锁 AC 分类,非现在改 GDD)

> 4 处 GDD↔ADR 接缝在引擎 Verification Required 项通过前,AC 测试分类处于 UNVERIFIED。**裁定:Sprint 0 engine 核验通过后再锁定 AC 的 [Logic]/[Advisory] 分类,现在不改 GDD**(避免在未实测前误改正文)。

| GDD ↔ ADR | 假设 | 现实 | Action(Sprint0 后) |
|-----------|------|------|--------|
| hud.md ↔ ADR-0008 | HUD 逻辑 AC [Logic] BLOCKING 隐含 -nullrhi headless 可测 | `UHudStateMachine` 可测性依赖「NativeTick 在 -nullrhi 不触发」假设,5.7 未实测(Verification Required ②) | NativeTick headless 实测通过后锁 AC 分类;若仍触发,计时/动画 AC 须 [Logic]→[Advisory]+playtest-signoff,同步更新 headless 桩 |
| vfx-feedback.md ↔ ADR-0009 | VFX 基于 Legacy Unlit,Alpha 评估 Substrate | engine-ref 未记录 Substrate Unlit 节点(UNKNOWN);若无等价节点,Unlit VFX 材质迁移需重设计 | Alpha Substrate 评估须将「Substrate Unlit 节点存在性」列 go/no-go 前置;结论前 vfx-feedback 不假设迁移可行 |
| hud.md + property-card-ui.md ↔ ADR-0012 | UI 结构基于 CommonUI 激活栈稳定 API | CommonUI 5.4-5.7 迭代,回调名/Stack 签名未实测;Knowledge Risk=HIGH | 引用 CommonUI 激活生命周期的 AC 标注「依赖 ADR-0012 Verification Required ① 通过」;Sprint0 Step0 签名核验为 blocking 前置 |
| economy-cash.md + building-upgrade.md ↔ ADR-0010 | 音效触发依赖 MetaSound 参数 API + Sound Mix BGM 闪避 | Sound Mix Modifier 是否在 5.7 被 Audio Modulation Bus 替代未核(Verification Required ③) | 音频 sprint 开工前核验;若被替代,BGM 闪避契约失效,更新 ADR-0010 + 通知 economy/building 音效 AC 重验 |

---

## 6. Engine Compatibility(post-cutoff Verification Required 清单)

引擎 pinned UE 5.7;LLM 训练覆盖 ~5.3,5.4-5.7 为盲区。**以下为 Verification Required 项(非阻塞架构裁定,Sprint0 核验)**:

### HIGH 风险 — Sprint0 必核
- **ADR-0012(CommonUI)— 12 ADR 最高风险**:`UCommonActivatableWidget`/`UCommonActivatableWidgetStack`/`UCommonButtonBase` 全超训练边界。两未解假设:① **CommonUI 在 5.7 是否仍 Experimental** — Experimental 插件 cook 默认被过滤,漏配则首次打包**全部 UI 失效**;② `AddWidget/RemoveWidget` 签名逐一核对。**裁定:ADR-0012 Experimental 状态升为 Sprint0 blocking check**(原风险表未升级,此处显式升级)。
- **ADR-0008(HUD/IGameClock)**:`NativeOnActivated/NativeOnDeactivated`+`NativeTick` 在 -nullrhi headless 行为超边界;若 5.4-5.7 间 NativeTick 触发条件变更,headless 可测性假设失效,联动 §5 第1行。

### MEDIUM 风险
- **ADR-0010(音频)**:`SetFloat/Int/BoolParameter` 5.7 签名未核;`USoundSubmix + PushSoundMixModifier` BGM 闪避是否被 Audio Modulation Bus 替代未明(联动 §5 第4行)。
- **ADR-0011(Enhanced Input)**:`PlayerMappableInputConfig`(5.4 新增)已正确隔离为 MVP 不启用;IMC Priority 截获语义 5.4-5.7 一致性未实机验证,若变则 Modal 截获失效。
- **ADR-0004(确定性 RNG)**:`FRandomStream::RandRange` 5.7 是否仍走 float 中间路径影响跨平台 bit-level 一致性;若改 integer-only,基于 float 中间误差的兼容性结论需更新。
- **ADR-0002(棋盘容器)**:`UAssetManager::GetPrimaryAssetObject`/`FStreamableManager::RequestAsyncLoad` 5.7 签名未核;5.4+ async loading 行为有变,回调签名变则板块加载返工。

### Deprecated API(有意识技术债)
- **ADR-0009(卡通材质)**:选用 Legacy Unlit 材质节点 + Custom Node,Legacy Material 已列弃用(建议迁 Substrate)。ADR 已声明「Substrate 明确禁用」为**有意识技术债**;Alpha Substrate 评估里程碑执行。Legacy 5.7 仍支持但 cook 可能出弃用警告。Post-Process Material(Inverted Hull 描边备用)在 Substrate 启用后 PP 接口有 breaking change(HIGH,UNKNOWN)。

---

## 7. Specialist Findings(8 条实现 checklist — control-manifest / ADR 修措辞固化)

| # | ADR | 发现 | 修正 |
|---|-----|------|------|
| S-1 | ADR-0008 | `UHudStateMachine` 以 raw pointer 存 `IGameClock* Clock`,World 切换被 GC 则悬空 | UObject 实现者改 `TWeakObjectPtr<UObject>`(配 Cast);非 UObject 须保证实现者生命周期长于状态机。ADR 已登记但未给修正措辞 |
| S-2 | ADR-0011 | `UBoardInputManager::OnWorldBeginPlay` 取 EIC 时 InputComponent 不保证已初始化(dedicated server/-nullrhi) | 取到 EIC 后再 BindAction + null guard,否则 headless smoke test crash。Key Interfaces 示例未展示 guard |
| S-3 | ADR-0012 | CommonUI 5.3 仍 Experimental,5.4-5.7 转正未知;仍 Experimental 则 Shipping cook 默认过滤 | **升 Sprint0 blocking check**;`.uplugin`/`DefaultEditor.ini` 显式启用,漏配则首次打包全 UI 失效 |
| S-4 | ADR-0009 | Substrate Unlit 节点存在性未确认(engine-ref 仅记录 Slab/Blend/ThinFilm/Hair) | Alpha Substrate 评估须将「Substrate Unlit 可行性」列 go/no-go;若无则 art-bible §5.1 Unlit 风格需重设计 |
| S-5 | ADR-0003 | 裸 TArray 不能作 DYNAMIC_MULTICAST 参数(须 USTRUCT 包装)— 已标;增量通知场景未说明 | 仅全量 snapshot 广播;后续细粒度委托是已知可扩展性限制,**建议登记 OQ** |
| S-6 | ADR-0005 | MVP 同步写(SaveGameToSlot 阻塞),大存档单帧卡顿;ADR 未给数据量估算 | 实现时对 `FGameStateSnapshot`(≤40 tile)序列化体积预估,**超 1 MB 提前迁异步写**,不等 Alpha |
| S-7 | ADR-0010 | Audio 独立 `FRandomStream`(#22 非权威流)Seed 联网同步策略未说明 | 联网阶段客户端/服务器 AudioRNG 不同步则音效不一致;接口层预留 Seed 同步钩子 |
| S-8 | 全局 | TObjectPtr 一致性:ADR-0011 资产引用正确用 TObjectPtr | ADR-0001~0010 未发现系统性 raw pointer 暴露 GC 问题 — **ADR-0008 `IGameClock*` 是唯一例外**(见 S-1) |

---

## 8. Verdict

```
CONCERNS
```

**修正口径**:旧版 Verdict=FAIL 是**口径误判** — ① 追溯覆盖只跑了 1/16 GDD(save 系统 25 条),据此无法对全架构判 PASS/FAIL;② 性能预算项被当作硬 FAIL 触发器,但其实质是「未量化封顶 + 引擎假设未实测」,属 CONCERNS 而非真阻塞冲突。本版全量追溯后修正为 **CONCERNS**。

**判 CONCERNS(非 FAIL)的依据**:

1. **无真阻塞冲突**:4 处 Cross-ADR Conflicts 全部为文档同步/职责切分(CONFLICT-1/2 本批已修,CONFLICT-3/4 为 control-manifest 协同约定),**无依赖环、无需重裁任何 ADR**。
2. **追溯覆盖健康**:266 条 TR 中 243(91.4%)≥ Partial;无系统级覆盖真空;23 条 Gap 中仅 5 条(单一 OQ-Prop-1 接缝)需新 ADR,其余为 propagate/`/ux-design`/性能/asset 类。
3. **tr-registry 已补全**:本批枚举全 16 系统至追溯矩阵,稳定 TR-ID 可供 story 引用(旧版仅 save 25 条)。

**CONCERNS 来源(非 FAIL,但 gate 前须收敛)**:

- **C-1 engine Verification Required**:ADR-0012 CommonUI Experimental 状态、ADR-0008 -nullrhi NativeTick、ADR-0010 Sound Mix vs Audio Modulation、ADR-0004 RandRange float 路径、ADR-0002 async load 签名 — **Sprint0 核验**(其中 ADR-0012 升 blocking check)。
- **C-2 性能预算待目标硬件量化**:Memory Ceiling / Draw Calls 在 technical-preferences.md 仍「待目标硬件确定」;ADR-0005 同步写体积未估算(S-6,超 1MB 迁异步)。**待量化封顶**,非架构裁定错误。
- **C-3 OQ-Prop-1 单一新 ADR 接缝**:owner-map 运行态容器 + 原子移交 + 重入锁(5 条 Gap),下游 8/9 实现前一次 `/architecture-decision`。

> CONCERNS = 架构裁定本身质量高、无环、无需重裁;收敛项为**引擎实测(Sprint0)+ 性能预算量化(目标硬件)+ 一次 owner-map 接缝 ADR**。完成后复审转 PASS。

---

## 9. Concern Items(CONCERNS 收敛清单)

| # | Concern | Owner | 解除条件 |
|---|---------|-------|----------|
| C-1a | ADR-0012 CommonUI Experimental 状态未核(cook 全 UI 失效风险)→ 升 Sprint0 blocking | ue-umg-specialist | Sprint0 Step0 核验 5.7 是否 Experimental + 显式启用配置 |
| C-1b | ADR-0008 -nullrhi NativeTick 行为未实测,HUD AC 分类不可锁 | unreal-specialist | Sprint0 对照引擎源码实测 NativeTick headless 行为 |
| C-1c | ADR-0010 Sound Mix vs Audio Modulation / ADR-0004 RandRange / ADR-0002 async load 签名未核 | unreal-specialist | Sprint0 逐项核验,更新 ADR Engine Compatibility 节 |
| C-2a | 性能预算未量化封顶:Memory Ceiling / Draw Calls 仍「待目标硬件确定」 | technical-director + producer | 确定目标硬件,量化封顶写入 technical-preferences.md |
| C-2b | ADR-0005 同步写无存档体积估算,大快照单帧卡顿风险 | lead-programmer | `FGameStateSnapshot`(≤40 tile)体积预估;超 1MB 迁异步写 |
| C-3 | OQ-Prop-1 owner-map 运行态容器 + 原子移交 + 重入锁(prop-001/002/012, bank-004/011)无 ADR | architect | 下游 8/9 实现前一次 `/architecture-decision` 统一裁定 |
| C-4 | 文档同步 CONFLICT-3/4 + S-1/S-3 修正措辞 | technical-director | control-manifest 固化非确定源约定 + 读档重绑分层 + IGameClock TWeakObjectPtr + CommonUI Experimental |

---

## 10. Required ADRs

**需新增 1 份 ADR(OQ-Prop-1 接缝)**:owner-map 运行态容器形态(`TArray<int32>`/`TBitArray`)+ 批量移交事务原子性 + 破产移交重入锁 `bIsMidBankruptcyTransfer`。覆盖 Gap:prop-001/002/012、bank-004/011(5 条)。**下游建房8/破产9 实现前裁定。**

**建议补现有 ADR(非新建)**:
- ADR-0007 补「禁 float / 整数纯净跨编译一致」条款(覆盖 econ-014)。
- ADR-0005 补「破产 `ResolveBankruptcy` 执行期瞬态守卫禁存档」(覆盖 bank-014)。

**其余 16 条 Gap 不需新 ADR**:propagate 债(producer)/ `/ux-design`(ux)/ 性能 checklist(performance-analyst)/ asset-spec(art)。

---

## 11. Pre-gate 前置(advance 前须跑)

本 review 判 CONCERNS,**进入下一阶段 gate 前须先完成**:

1. **`/test-setup`** — 搭建 UE Automation headless `-nullrhi` 测试框架 + CI;这是 §5 GDD Revision Flags 锁 AC 分类、§7 S-2/S-6 headless smoke、追溯中大量 `[Logic]` AC 可执行的前提。
2. **`/ux-design`** — 收口 Gap G-19/G-22(reduce_motion 全局 a11y 布尔)+ Partial 聚类 P-c(net_worth 显示口径/HUD↔card 共享 a11y)+ HUD/property-card 交互细化;UI 类 AC 在 UX 规格冻结前处于 UNVERIFIED。

> 两者完成 + §9 Concern Items 收敛后,`/architecture-review` 复审可转 PASS,方过 Pre-Production → Production gate。

---

*Review by technical-director · 2026-06-06 · 16 系统全量覆盖降级 · Verdict 修正 FAIL→CONCERNS*
