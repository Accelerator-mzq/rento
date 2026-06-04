# Cross-GDD Review Report —《骰子大亨 / Dice Tycoon》

**Date**: 2026-06-03
**GDDs Reviewed**: 7 系统(board-data / player-turn / dice / movement / economy-cash / property-ownership / tile-events)+ game-concept + systems-index + entities.yaml
**Systems Covered**: 全 7 个 MVP 系统(1-7),均 Approved-with-followups
**Method**: 3 个独立 agent 跑 Phase 2(一致性)/ Phase 3(设计理论)/ Phase 4(跨系统情景);主会话对每条 BLOCKING fresh-grep 独立核验(不采信 claim,遵循项目"不把意见当结论"规矩)

> **总评**:7 档经多轮 propagate 后收敛度很高。Phase 2 的 2d/2e/2f、Phase 3 的 3a/3f/3g、Phase 4 的 7 场景中 6 个 —— 全部 clean。`enable-not-own` 这一项目历史顽疾在 5 个服务/编排层系统中已系统性修复(movement 的 Experience Contract 堪称范本)。问题集中在 **player-turn 作为 orchestration spine 的两处接口契约真空**,加几处单侧改动留下的 stale 邻居。

---

## Ⅰ. 一致性问题(Consistency)

### 🔴 Blocking

**C-1 systems-index 自身依赖图自相矛盾(系统9)**
- `systems-index.md:28`(枚举表):`| 9 | 破产与胜负 | … | 2, 5, 6 |`
- `systems-index.md:123`(Dependency Map):`破产与胜负（9）— depends on: 2, 5`(**缺 6**)
- 成因:property-ownership OQ-Prop-4 propagate 时只改了枚举表一处,漏改 Dependency Map。9→6 是真依赖(破产经 `TransferOwnership` 调 6)。memory `single-doc-edit-leaves-approved-neighbor-stale` / `propagate-followup-claim-must-fresh-grep` 失效模式复发。
- **修复**:line 123 → `depends on: 2, 5, 6`。

**C-2 player-turn 缺 ResolvePhase「算租聚合」契约 —— spine owner 真空**(同 Phase 4 BLOCKER)
- 三个下游档都把算租输入聚合责任指向回合(2)的 ResolvePhase:
  - `economy-cash.md:58`:house_count"6 快照 + 8 house_count 经回合2 `ResolvePhase` 聚合传入"
  - `property-ownership.md:107`:回合(2)"聚合 6 快照 + 8 `house_count` 传经济"
  - `systems-index.md:69`:"经回合2 ResolvePhase 聚合 6 快照 + 8 house_count 后传经济"
- 但 `player-turn.md`(聚合的实际 owner)全文 0 命中 `house_count`/`聚合`/聚合接口;CR-3 ResolvePhase 行只写"委派落地结算(租金/买地/抽牌/税/发薪)",未声明它持有"调 6 取归属快照 + 读 8 house_count + 拼装传 economy"这条契约。memory `obligation-on-approved-downstream-is-vacuum`:三个系统都说"回合2会聚合",但回合2自己不知道。叠加建房(8)Not Started → MVP 第一个收租循环的 house_count **无供给方、无 provisional 缺省**。
- **修复**:player-turn 补 CR(ResolvePhase 拥有聚合契约,含接口名)+ AC;显式声明 MVP(8未实现)阶段 house_count 缺省=0(落 `RentTable[0]`/垄断×2 base 分支)。`/propagate-design-change` 回填下游;时机=建房(8)设计时或之前。

### ⚠️ Warning

**C-3 player-turn 受控写接口面欠命名(成簇)** — player-turn 持有 `CurrentTileIndex`/`Cash`/`bIsInJail`/dice Total holder 等字段并声称暴露受控写接口,但只命名了 `SetPosition`。下游因此:
- `tile-events.md:359`(AC-52)凭空命名 `SetDiceContext(X)` 写 dice holder —— player-turn 无此接口(带逃生条款挂 OQ-Event-7,故非硬 blocking)
- `movement.md`(90/92/125/137/219)称位置写接口 `SetTileIndex`,`player-turn.md`(89/198/364)称 `SetPosition` —— 同接口异名(已登记 OQ-Move-3b;深层是单层 setter vs 两层封装的架构问题)
- 监狱态受控写接口未具名(tile-events review-log 已登记)
- **修复**:player-turn 把受控写接口面命名齐,下游回填实名;SetPosition/SetTileIndex 的单层/两层归属作显式标别名 + 留 OQ-Move-3b ADR。

**C-4 entities.yaml 残留 stale 注释(OQ-Event-4)** — `entities.yaml:424` 仍写"economy F-11 仅定义单债权人→OQ-Event-4 须 propagate",但 `tile-events.md` 已在 131/244/353/390 四处收口为"逐笔执行使每笔破产债权人单一,无须 propagate"。设计结论已对、注释漂移。**修复**:line 424 注释对齐收口结论。

---

## Ⅱ. 游戏设计问题(Design Holism)

### 🔴 Blocking
无。所有经济失衡均为 Monopoly 固有、支柱①忠实复刻刻意保留,GDD 数值未偏离经典、未引入新失衡。

### ⚠️ Warning

**D-1 concept 三个抗拖沓杠杆,MVP 实际只有一个可用** — concept(L220/234)承诺用「可调局长 / 破产加速 / 起始资金」压缩"后期翻盘无望拖沓"(自点名最高 Design Risk)。核查:
- ✅ 起始资金:`STARTING_CASH`(1500,750–2000)+ `STARTING_CASH_FAST`(750)— MVP 可用
- ⚠ 可调局长:`economy-cash.md:350` `max_laps/max_rounds` 标 **(Open)、默认禁用、Alpha、归破产(9)** — MVP 不可用
- ⚠ 破产加速:任何 MVP GDD 均无对应旋钮(F-10 严格 `<`,无加速档)
- **建议**:在 systems-index 或 concept Open Questions 显式登记"MVP 抗拖沓仅 STARTING_CASH;max_rounds 待 House Rules(23)、破产加速待破产(9)",避免预期落差。非单档 blocking。

### ℹ️ Info(知晓即可,均已诚实标注)
- 垄断雪球 / 后期 runaway / 唯一最优路径 = Monopoly 固有,接受(economy F-2 + property CR-4;board-data:133 已声明占位值不作设计有效性证据)
- 监狱 safe-haven 退化策略的诚实标注被 deferred(tile-events review-log)— 建议补一句 Edge Case 标注
- 支柱①"认出感"实依赖未完成的命名策略(OQ-3)+ art bible(board-data:32 已回流)
- 注意力预算:典型回合主动决策点 ≤2,对支柱④正向;Alpha 叠加特色机制时须复检 3b

---

## Ⅲ. 跨系统场景问题(Cross-System Scenario)

走查 7 场景:① 买地决策 ② 落他人地算租 ③ 过GO发薪 ④ 送监狱+抑制 ⑤ 双点链第3次入狱 ⑥ PayToEach致破产 ⑦ 清算→破产(9未写)

### 🔴 Blocker
- **场景②(算租聚合)= C-2**(同一问题)。命中 MVP 最核心的"落地收租"路径。

### ⚠️/ℹ️ Warning / Info
- **场景⑥ INFO**:OQ-Event-4 多债权人 seam 机制已收口,仅 entities.yaml:424 注释 stale = **C-4**
- **场景②ⓘ INFO**:dice_total PULL holder 时序本身正确,但缺端到端集成 AC 断言"前进到最近公用用额外骰非本程移动骰"(OQ-T-Econ-3 部分覆盖)。建议补集成 AC。

### 已正确收口的高危接缝(clean,记录优点)
监狱态抑制(三层一致)、双点链第3次入狱(先判入狱跳过移动结算、链终止、写权清晰)、过GO双重发薪守护(四处一致)、破产 verbatim F-9 契约(OQ-T-Econ-2 + AC-27 可证伪化)。

---

## Ⅳ. GDDs Flagged for Revision

| GDD | Reason | Type | Priority |
|-----|--------|------|----------|
| systems-index.md | line 123 系统9依赖缺6,与 line28 矛盾 | Consistency | 🔴 Blocking |
| player-turn.md | ResolvePhase 算租聚合契约真空(C-2)+ 受控写接口欠命名(C-3) | Consistency / Scenario | 🔴 Blocking + ⚠ |
| tile-events.md | AC-52 引用未定义的 `SetDiceContext`(配合 player-turn 命名收敛) | Consistency | ⚠ Warning |
| movement.md | `SetTileIndex` 与 player-turn `SetPosition` 异名(配合收敛) | Consistency | ⚠ Warning |
| entities.yaml | line 424 OQ-Event-4 stale 注释,与 tile-events 收口矛盾 | Consistency | ⚠ Warning |
| economy-cash.md / systems-index | concept 三杠杆 MVP 仅一可用,须显式登记追踪 | Design | ⚠ Warning |

---

## Verdict: **FAIL**

存在 2 项 🔴 Blocking 须在 `/create-architecture` 前解决(架构会继承不一致):
1. **C-1** systems-index 内部依赖图自相矛盾(line 123 补 6)
2. **C-2** player-turn ResolvePhase 算租聚合契约真空 + MVP house_count 无缺省

> 两项 blocking 范围明确、成本不高,且架构非紧邻下一步(下一步是建房(8)设计)。C-2 自然在建房(8)设计时一并关闭最经济。按 skill 语义如实判 FAIL,因二者确属"架构会继承的不一致"。

### FAIL — 架构前必做
- systems-index.md:123 → `depends on: 2, 5, 6`
- player-turn 补 ResolvePhase 聚合 CR/AC(含接口名 + MVP house_count 缺省=0),`/propagate-design-change` 回填 economy/property/tile-events/index
- (同批)player-turn 受控写接口面统一命名(C-3),entities.yaml:424 注释订正(C-4),D-1 追踪登记

---

## 处置记录(2026-06-03,用户裁定「全改」——已全部落地)

| Finding | 改动 | grep 核实 |
|---|---|---|
| **C-1** index 依赖图矛盾 | `systems-index.md:123` `2, 5`→`2, 5, 6` + 成因注 | 无 `depends on: 2, 5$` 残留 ✅ |
| **C-2** ResolvePhase 聚合真空 | `player-turn.md` 新增 **CR-3.2**(聚合契约确权 + MVP house_count 缺省=0)+ **AC-48/AC-49** + CR-3 表 ResolvePhase 行回指;`property-ownership.md:106` 措辞对齐(6 经 `BuildOwnershipSnapshot` 暴露、回合2 取并转发) | CR-3.2/AC-48/AC-49 在档 ✅;聚合 owner = player-turn(对齐 economy:104/property:107/index:69) |
| **C-3** 受控写接口欠命名 | `player-turn.md:89` 新增**受控写接口面具名表**(`SetPosition`/`SetCash`/`SetJailState`/`SetCurrentRollContext`/`SetBankrupt`)+ `SetPosition`↔`SetTileIndex` 交叉引用注;CR-3.1 具名 `SetCurrentRollContext`;`tile-events.md:359`(AC-52)`SetDiceContext`→`SetCurrentRollContext`;`movement.md:92` 补交叉引用 | `SetDiceContext` 全 GDD 0 残留(仅本报告引用)✅;`SetCurrentRollContext`/`SetJailState` 在档 ✅ |
| **C-4** entities stale 注释 | `entities.yaml:424` OQ-Event-4 注释订正为"已收口、无须 propagate"(规避字面触发串) | 无活跃 stale 呼吁 ✅ |
| **D-1** 三杠杆预期登记 | `systems-index.md` 高风险表登记"MVP 抗拖沓仅 STARTING_CASH;max_rounds 待(23)、破产加速待(9)" | 已登记 ✅ |

**改动文件(8)**:player-turn.md / property-ownership.md / tile-events.md / movement.md / systems-index.md(×2:line123 + 高风险表)/ entities.yaml + 本报告。

### 修复后裁定:**CONCERNS → 实质 PASS(待 fresh re-review 复核)**

2 项 🔴 Blocking(C-1/C-2)+ 3 项 ⚠ Warning(C-3/C-4/D-1)全部已改并 grep 核实落地。原 FAIL 的两个 architecture-inherited 不一致已关闭:
- C-1:依赖图自洽。
- C-2:ResolvePhase 聚合契约在 spine owner(player-turn)确权,MVP house_count 缺省=0 消除算租路径 undefined。

> **遗留(非本轮 blocking,均已登记)**:OQ-Move-3b(SetPosition/SetTileIndex 单层/两层 ADR,实现期收敛)/ OQ-Event-7(holder 直传 vs 经受控写的架构裁定,AC-52 已带逃生条款)/ 监狱 safe-haven 诚实标注(deferred REC)/ dice_total 来源端到端集成 AC(建议 tile-events 补)。建房(8)设计时须实现 player-turn CR-3.2 步骤②读真实 house_count。

> **建议**:`/clear` 后 fresh session 跑 `/review-all-gdds since-last-review` 或 `/gate-check systems-design` 复核本轮修复,再进 `/create-architecture` 或 `/design-system 建房`。

---

## Re-Review 复核(`/review-all-gdds since-last-review`,2026-06-03,fresh session)

**方法**:对上述「全改」changeset(6 文件)逐条 **fresh-grep 独立核验**——处置记录的 claim **不采信**,每条从文件实证重新推导(遵循项目「不把意见当结论」规矩)。

### 5 项 finding 复核 — 全 CLOSED

| Finding | 核验结果(fresh-grep) |
|---|---|
| **C-1** 🔴 index 依赖图 | ✅ `systems-index:121` = `破产与胜负（9）— depends on: 2, 5, 6` + 成因注;无 `depends on: 2, 5$`(系统9)残留。 |
| **C-2** 🔴 ResolvePhase 聚合真空 | ✅ CR-3.2 @133 在 spine owner 确权;AC-48/49 @515-516;CR-3 表 ResolvePhase 行 @123 回指;MVP house_count=0 缺省 @136。 |
| **C-3** ⚠ 受控写接口欠命名 | ✅ player-turn:94-98 具名表全;`SetDiceContext` **GDD 内 0 残留**(仅本报告引用);tile-events AC-52 @359 + movement:92 交叉引用对齐。 |
| **C-4** ⚠ entities stale 注释 | ✅ `entities.yaml:424` 已订正为「propagate 呼吁 stale、不再适用…收口为说明性 note」。 |
| **D-1** ⚠ 三杠杆预期 | ✅ `systems-index:195` 已登记「MVP 抗拖沓仅 STARTING_CASH;max_rounds 待(23)、破产加速待(9)」。 |

### 接缝复核(单档改→stale 邻居 失效模式)— 检查,**本轮未复发**

C-2 引入的接缝网经核实**全向自洽**:
- CR-3.2 引用接口 **`BuildOwnershipSnapshot`** → 确在 owner 档定义(`property-ownership:121` 接口稳定承诺 + AC-30/30b/31b 测试),非幻引用。
- CR-3.2 引用 **economy:104**(「收(push,经 ResolvePhase 聚合)」)与 **property:106-107**(「由回合(2) ResolvePhase 调本接口…聚合编排归 player-turn CR-3.2」)→ 两端文本均确认,且 property 反向回指 CR-3.2(双向)。
- AC-48/49 引用 **economy:147-148** RentTable[0]/垄断分支 → 确认 `elif is_monopoly and house_count == 0: rent = RentTable[0] × monopoly_rent_multiplier`;MVP `house_count=0` 缺省落入**已定义分支**,无 undefined 路径。
- 继承义务表 `systems-index:69` 命名同一 owner/契约 → 一致。

无新单侧改、无幻引用、无漂移行号。

### 遗留(pre-existing,已登记,非 blocking,本轮无变化)
OQ-Move-3b(SetPosition/SetTileIndex ADR)· OQ-Event-7(holder 直传 ADR)· 监狱 safe-haven 诚实标注 · dice_total 端到端集成 AC。

### Re-Review 裁定:**PASS** ✅
2 项 🔴 Blocking(C-1/C-2)关闭并独立核实;3 项 ⚠ Warning 全关闭;无新接缝。原报告「实质 PASS(待 fresh re-review 复核)」**现经 fresh-grep 确认**。清理通过,可进 `/create-architecture` 或 `/design-system 建房(8)`(建房落地时一并实现 CR-3.2 步骤②读真实 house_count,最经济收尾 C-2)。
