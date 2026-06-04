# Review Log — 建房升级 (Building & Rent Scaling, 系统 #8)

跟踪 `design/gdd/building-upgrade.md` 的逐轮 design-review 裁定与修订,供后续 re-review 核对 R(n) 项是否真闭合。

---

## Review — 2026-06-04 — Verdict: NEEDS REVISION (R1, design-review full)
Scope signal: L(多系统集成 8↔1/2/5/6/7/9/10、7 自有公式、无新 ADR、骑 OQ-Prop-1/OQ-Build-3 容器)
Specialists: economy-designer / game-designer / systems-designer / qa-lead / creative-director(senior synthesis)
Blocking items: 6 | Recommended: ~16 | Prior verdict resolved: 首评

### 结构/接缝核验(主审独立 fresh-grep,非信 GDD 自述)
- 跨系统接缝双向一致已核实:economy F-2 `house_count`归8 / F-9 `building_sellback`;property-ownership L108 8→6 单向读、6 不读 8;tile-events OQ-Event-3 在 tile-events 侧已 RESOLVED 并回链建房8 F-7;systems-index L26 系统7 已含 `8(soft)`、L27 系统8 depends-on `1,5,6`;player-turn CR-3.2 聚合存在。
- **真空确认**:player-turn(Approved)现**无** AC「调 6 `Mortgage` 前读 8 `GetHouseCount==0`」——AC-27 回链义务未兑现,已登记 OQ-Build-1(producer propagate),非本档可独力闭合。
- 8/8 必备 section 齐(§3 标题用 "Detailed Design" 变体)。header `Status: In Design` vs systems-index `Designed` 待和解(cosmetic)。

### Blocking(6 条,R1 已全部修订)
1. [game-designer] UI 缺"建后租金预览" → 精算幻想 UI 层未兑现(§B↔UI consonance gap)。**修**:UI Requirements 增建后租金预览需求(供 house_count/CanBuildHouse;RentTable 值归经济5 F-2 经回合2 读)。
2. [systems-designer] CR-6 归纳证明漏 OwnerChange(in-kind) 第三类转移(组跨 owner 拆分,per-group 不变式无良定义)。**修**:证明增"归属移交步",不变式改按 **(组 ∩ owner)** 评估、拆分组入冻结域。
3. [systems-designer] F-1 `|G|=0` 空集 min/max UB,仅加载期断言无运行时守卫。**修**:增运行时契约 `ensure(|G|>0)`+log+`canBuildHouse→false` 兜底(镜像 BuildingCost≤0 处置);并明确 `|G|=1` 语义。
4. [qa-lead] AC-11 自降级("否则降 code-review")在 Blueprint-主语言项目使 [Logic] 静默变 Advisory。**修**:拆 AC-11a(Logic·BP 可观察)+ AC-11b(Advisory·code-review)。
5. [qa-lead] AC-29/30 "或 grep 核实"对 Blueprint 间接调用不足以证伪反向边。**修**:升 [Integration·BLOCKING] + 运行时 spy fixture,grep 非终证。
6. [qa-lead] AC-31 provisional 同义反复混入 Logic 计数;AC-27 非可测 AC。**修**:AC-31 隔离出 gating;AC-27 改可测 [Logic] + 回链义务移为〔Obligation·OQ-Build-1〕不计数;tally 订正为 **34 条(29 Logic + 2 Advisory + 2 Integration·BLOCKING + 1 quarantined)**。

### 裁决/降级(creative-director,主审独立复核同意)
- **支柱裁定**:无限滚雪球碾压是桌游忠实(①)的有意结果,**拒 rent cap 旋钮**(违①);落后者托底归 Alpha 翻盘注入器。登记 **OQ-Build-6**(playtest 验证,R1 新增),吸收 CR-8 仅自己回合建房对支柱②的削弱为同批观察项。
- **REJECT(非 blocking)**:systems-designer "canSellHouse 内部矛盾" → L196 括注"(若该格非组内 max)"精确,为 provisional OQ-Build-3 政策问题(归破产9),非 #8 blocker。
- **DOWNGRADE → RECOMMENDED**:economy CR-2(Alpha Limited 接口前瞻)/ AC-20(强制路径 NLV 可达,归经济 F-9 性质)/ RentTable 单调(归 economy OQ-Econ-9,硬 config-load 不变式)。

### Recommended(~16,R1 未应用,留后续 pass)
- economy:CR-2 加 supply-hook 前瞻注;`RentTable[1]≥RentTable[0]×2` 硬化为 board config-load 不变式;AC-20 并入 OQ-Build-2;F-4 全盘max-非最低ROI designer-note;OQ-Build-1 加"早停 vs 遍历"澄清;OQ-Build-4 量化 playtest 判据。
- game-designer:R-1 批量建房 vs "全砸下去" MDA;R-2 CR-8 支柱②成本显式登记(部分已并入 OQ-Build-6)。
- systems-designer:强制路径 owner-guard 时序注;F-7 调用约定(player=当前停格玩家);F-4 "全盘max必组内max" 补1行证明;CR-9 decrement→Credit 原子序 vs CR-4 注。
- qa-lead:AC-3/AC-14/AC-25b GIVEN 补全;新增 AC-9b(CR-2 累计 5×BuildingCost);AC-12 执行序消歧;AC-15b 酒店门收拢路径。

### 跨档传播债(producer,实现前)
- OQ-Build-1 → player-turn(新增 Mortgage 前读 GetHouseCount==0 AC)+ economy CR-7.3 清算措辞澄清。
- OQ-Build-2 → player-turn CR-3.2 聚合扩展覆盖 F-9 建筑枚举。

### 下一步
用户选择:**/clear 后 fresh re-review**(`/design-review design/gdd/building-upgrade.md`)。Re-review 须核 R1 上述 6 blocker 真闭合 + 决定 ~16 recommended 是否本轮处理。

---

## Review — 2026-06-04 — Verdict: NEEDS REVISION (R2, design-review full)
Scope signal: L(多系统集成 8↔1/2/5/6/7/9/10、7 自有公式、无新 ADR、骑 OQ-Prop-1/OQ-Build-3 容器)
Specialists: economy-designer / game-designer / systems-designer / qa-lead / creative-director(senior synthesis)
Blocking items(in-doc): 3 | Recommended(in-doc): ~9 | Propagate 债(producer): 2 | Prior verdict(R1) resolved: 5/6 真闭合,1 部分

### R1 6 blocker 闭合核验(主审独立读原文 + fresh-grep,非信 GDD 自述)
- ✅ #2 CR-6 in-kind 证明步(L175–182)真闭合;✅ #6 AC-31 隔离 + AC-27 可测真闭合;✅ #1 建后租金预览 UI(L265)in-doc 闭合(对手现金可见性经 CD 裁定归 UX,非 #8 scope)。
- ⚠ #3 F-1 |G|=0 守卫(L99)**部分**:守卫在,但 min/max 在空集的**返回值未定义** → F-2/F-3 消费 UB 风险(systems B3)。
- ⚠ #4 AC-11(L288)**部分**:AC-11a ≡ AC-10(断言完全相同、标题"调用序"无对应断言),冗余非复制但 gating 覆盖未丢(qa,主审降 RECOMMENDED)。
- ❌ #5 AC-29/30(L309/310)**未真闭合**:AC-29 spy 用 MockEconomy 断言"economy 不调 8"= 永真同义反复(Mock 手写本不调 8);AC-30"未实现期 code-review 占位"= 静默把 BLOCKING 降 Advisory(同 R1 杀掉的 AC-11 自降级反模式)。

### R2 in-doc blocker(本轮已全部修订)
1. [qa·confirmed] **AC-29 空转 spy** → 重设计:只认领可独证子断言(真实 8 出边集=={economy.Credit} + 回合2 唯一驱动 ForcedSellNextBuilding);"真实 economy 不调 8"反向边证伪显式移交 OQ-Build-1(Mock 不可证)。
2. [qa·confirmed] **AC-30 占位漏洞** → 删"code-review 占位",改 `[Integration·BLOCKING·deferred-gate]`,gate F-9/破产集成 story、不 gate 单测 PR、不得 code-review 充过。
3. [systems B3·confirmed] **F-1 求值序** → min/max 在 |G|=0 返回哨兵 INDEX_NONE,F-2/F-3 消费前短路,闭合 UB 缺口。

### R2 recommended(本轮顺手改)
- AC-11a 强化"Debit 调用恰1次"(区别 AC-10);AC-14 补 owner 前置;CR-6 证明 L181 补 B1/B2(不变式仅单一owner整组有良定义 + canSellHouse 跨owner锁卖副作用显式登记,provisional 随 OQ-Build-3)。
- 未应用(留 R3 / 归经济侧):economy-designer F-9 NLV h>1 fixture 覆盖 / BC=1→sellback=0 退化 / 酒店末档 ROI RentTable[5]≥[4] board 校验 / F-5 vs CR-5 floor 措辞;game-designer OQ-Build-4 代价显式化 / OQ-Build-6 落后者底线 / "全砸下去"vs均衡建房语言摩擦;systems AC-25b 歧义 / L201 引用 / 冻结域 RepairFee Edge Case / F-2 clamp 定位;qa AC-12 并列 min 歧义。

### Propagate 债(producer,实现前硬门 — 本轮 sharpen 未兑现)
- **OQ-Build-1**(CONFIRMED 真矛盾):economy CR-7.3(economy-cash L71)operative 散文 + AC-43 读作 economy 驱动清算并调 F-8/Mortgage = 5→8/5→6 反向边,直接矛盾 building CR-9/AC-29;economy cap 句指向 building 一侧但算法散文相反。须 producer propagate 改 economy CR-7.3+AC-43 主语 + 补 player-turn「Mortgage 前读 GetHouseCount==0」AC(已 grep 核实真空)。**本档加硬门:player-turn AC 未补前 #8 抵押接缝 story 不得 Done。**
- **OQ-Build-2**(R2 sharpen):economy F-10 `is_insolvent(player, amount_due)`(economy-cash L229)签名无 NLV/建筑枚举形参 → "经回合2聚合传入"无环解需一次具名签名变更,否则 F-10 内部算 NLV 必调 8=5→8 环。propagate 须点名 F-10 签名缺口。

### CD 裁决/降级(senior synthesis,主审独立复核同意)
- **严重度订正**:economy CR-7.3 + F-10 BLOCKING→propagate 债(不阻 GDD 设计批准、阻实现,先例 economy/property/tile-events 均 Approved-with-followups);game-designer 对手现金→归 UX 非 #8 scope;systems B1/B2→provisional-rigor(houses 仅垄断整组下存在,破产整组转单一债权人通常不拆分,Alpha 交易才常态拆分);AC-11a BLOCKING→RECOMMENDED(覆盖未丢)。
- **支柱裁定(CD 终审权)**:领先者滚雪球碾压=支柱①忠实有意结果,**rent cap 维持 REJECT**;真风险在支柱②(无限供给 + Pass'N-Play 建房不可见),provisional 接受,OQ-Build-6 锐化为可证伪 playtest 假设 + 预承诺廉价补救(ResolvePhase 全员可见建房通告 beat,不动供给/不加 rent cap)。

### 下一步
用户选择:**/clear 后 fresh re-review**(`/design-review design/gdd/building-upgrade.md`)。R3 须核:① 3 in-doc blocker(AC-29/AC-30/F-1)真闭合;② OQ-Build-1/2 propagate 是否已由 producer 兑现(若已跑 propagate,核 economy/player-turn 对端落地);③ 决定剩余 economy/game/systems recommended 是否本轮处理。systems-index #8 维持 **Designed**(未 Approved)。

---

## Review — 2026-06-04 — Verdict: APPROVED-with-followups (R4, design-review full)
Scope signal: L(多系统集成 8↔1/2/5/6/7/9/10、7 自有公式、无新 ADR、骑 OQ-Build-5 容器 ADR、2 producer propagate 债)
Specialists: economy-designer / game-designer / systems-designer / qa-lead / **unreal-specialist(R4 新增引擎可行性)** / creative-director(senior synthesis)
Blocking(in-doc fast close-out): 5 已应用 | Propagate 债(producer): 3 | RECOMMENDED(下一 pass): ~10 | Prior verdict resolved: R2 3 in-doc + R3 3 in-doc 全真闭合

> **流程异常记录:** R3 修订(CR-9 旁路/AC-20 tautology/AC-3-8 GIVEN/通告 beat 义务)已落 doc(header 自述),但**未写 R3 review-log 条目**——R4 一并补核:R2/R3 全部 in-doc closure 经主审读原文核验真闭合。

### 主审独立 fresh-grep(非信 GDD 自述)
- **R2/R3 in-doc closure 真闭合**:AC-29 rescope(真实8出边=={Credit})/ AC-30 deferred-gate / F-1 哨兵+短路 / AC-20 可证伪 / AC-33 空组 / CR-9-F-4 旁路 canSellHouse / GIVEN 补全。✓
- **两 propagate 债 CONFIRMED 仍 OPEN**(doc 诚实登记、未假闭合):**OQ-Build-1** economy-cash:71 CR-7.3 verbatim 仍「economy 提供清算:按 MV 升序遍历资产、带房地先 F-8 卖房再抵押」= 5→8/5→6 反向边;AC-43 变体B(:464)仍「先 F-8 卖 2 房再抵押」;player-turn 无清算驱动相 + 无「调6 Mortgage 前读8 GetHouseCount==0」AC(真空)。**OQ-Build-2** economy-cash:229 F-10 `is_insolvent(player,amount_due)` 仍缺 `preaggregated_nlv`;F-9(:215)仍「Σ building_sellback(b)」;player-turn CR-3.2+AC-48 仅覆盖算租、非 F-9/破产判据路径。

### R4 5 specialist 关键发现(主审severity + 独立核验)
- **[systems B-2,主审核验 reachable-but-narrow-gated]** F-4 L131 + Edge L202 「不破坏任何组均衡」是**无条件断言**,与 doc 自身 L181-182 单一-owner 范围**自相矛盾**;破产 in-kind 拆分组(OQ-Build-3 provisional)中 A 持格 5→4→3 而 C 持格恒 5 → 组 [3,5] 差2,全盘 max(A)≠组内 max(C)。→ **in-doc 已修**(L131/L202 加单一-owner 适用域 + 拆分组 provisional 标注)。
- **[qa BLOCKING-1]** AC-29 spy 机制未在 fixture 约定定义 → Integration·BLOCKING 名不副实。→ **in-doc 已修**(L277 加 `OutboundCallLog` spy 约定)。
- **[unreal BLOCKING-2,主审核验蕴含确为无效]** AC-11a「Debit==1∧state未写 ⇒ Debit在+=1之前」假蕴含(写后回滚亦满足)。→ **in-doc 已修**(L289 删假蕴含、改"不证调用序、序归 AC-11b")。
- **[unreal BLOCKING-1]** CR-8「单线程=无并发」引擎天真:Blueprint 同步多播 OnBuildingChanged 监听者可重入写接口。→ **in-doc 已注**(CR-8 重入注,防御口径→OQ-Build-5 ADR;AC-28 现 Advisory,ADR 后升 guard)。
- **[game B-2]** Fantasy「全砸下去」vs 均衡建房逐栋摩擦(R1 R-1 未应用)。→ **in-doc 已修**(L18 改「一栋栋砸下去、轮着建到各两栋」)。
- **[economy F1-4]** 清算 cycle-break 正确但 player-turn 须定义清算驱动相+AC(非仅 Mortgage-pre-read)+ 两层顺序(MV升序 vs F-4 全盘最高档)须裁定单一规则。→ OQ-Build-1 propagate。**主审更正 economy 过陈述**:两层顺序**不**影响 `is_insolvent`(NLV 是顺序无关求和,solvent/bankrupt 结果顺序不变),仅影响早停后剩余资产组合。
- **[economy F5/F6,REC]** F-9 奇数 BC floor 序(111 vs 112)+ 酒店租 `RentTable[5]` 可低于垄断空地翻倍 → 并入 OQ-Build-2 / OQ-Econ-9 propagate。
- **[qa BLOCKING-2/3,REC]** CR-9 缺序列级多步均衡 AC-25c(仅单一-owner)+ AC-21 缺 `OnBuildingChanged(A,4)` 断言(VFX/HUD 静默错误)→ 下一 pass。

### CD 裁决(senior synthesis,主审复核同意)
- **R4 发现为正交新维度(六面:AC-rationale/fixture-spec/coverage/engine/internal-consistency/ludonarrative),非同一接缝重复剥面** → blocker 质量逐轮下降(R1 架构面→R4 一行 caveat/AC-craft)= **收敛曲线**,非 peeling;无需 RETREAT。
- **VERDICT APPROVED-with-followups**:本档设计逻辑自洽(5 项 in-doc 修后),诚实登记 OQ-Build-1/2/3 并自带实现硬门;持 economy/property/tile-events **同一 bar**——以三 in-doc cheap 编辑判 NEEDS REVISION 将对 building 施更严标准,无原则依据。
- **显式 OVERRULE game-designer BLOCKING-1**(对手现金/距离可见性):前轮 CD 已裁归 UX scope,有 L271 UX Flag 归属,非 #8 doc-blocker(重申非重开)。

### 跨档传播债(producer,实现前硬门)
- OQ-Build-1 → economy CR-7.3(:71)+ AC-43 变体B(:464)主语改"economy 拥顺序 spec、执行驱动归回合2"+ 两层顺序裁单一规则 + player-turn 补清算驱动相/AC + 「Mortgage 前读 GetHouseCount==0」AC。
- OQ-Build-2 → economy F-10(:229)加 `preaggregated_nlv` 签名 + F-9(:215)展开式(含奇数 BC fixture)+ player-turn CR-3.2 扩 F-9/破产判据聚合路径。
- OQ-Build-6 → player-turn(2)/HUD(16)补通告 beat 接收义务。
- **硬门:** 上述 player-turn AC 未补前,#8 抵押接缝 / F-9 破产判据 story 不得 Done(building AC-27 / AC-30 deferred-gate 已登记)。

### 下一步
5 项 in-doc fast close-out 已应用;systems-index #8 **Designed→Approved¶**。

### R4 propagate 执行(2026-06-04,`/propagate-design-change`,同会话)
用户裁定清算顺序 = **止损优先 mortgage-empty-first** → 全改授权 → 三档传播执行(economy-designer 改 economy / systems-designer 改 player-turn / 主审改 building):
- **OQ-Build-1 ✅ RESOLVED**:economy CR-7.3(消「本系统提供/遍历」5→8/5→6 环措辞,改 economy 拥顺序 spec+判据、执行归回合2)+ AC-43 变体B 重写 + Dep 行;player-turn 补 CR-3.3 驱动环 + AC-50(Mortgage-pre-read)+ AC-51(顺序+终止);building CR-9 驱动示例同步 mortgage-empty-first。
- **OQ-Build-2 ✅ RESOLVED**:economy F-10 加 `preaggregated_nlv` 形参 + F-9 展开式/奇数BC 变体C;player-turn 补 CR-3.4 全组合 NLV 聚合 + AC-52(zero economy→8)。
- **OQ-Build-6 ✅ player-turn 侧 RESOLVED**:player-turn CR-3.5 `OnBuildingAnnounced` + AC-53;HUD(16)消费侧登记 systems-index 继承义务表(待 HUD GDD)。
- **RECOMMENDED 并轨**:economy OQ-Econ-9 加 `RentTable[5]>RentTable[4]` + `RentTable[5]>RentTable[0]×mult` board 校验。
- **主审独立 fresh-grep 核实(非信 subagent 自述)**:economy 旧环措辞 0 命中;F-10 新签名 L231;player-turn CR-3.3 调 `is_insolvent(player,amount_due,preaggregated_nlv)` 与 economy **签名完全一致**;三档清算顺序一致。详见 `docs/architecture/change-impact-2026-06-04-building-upgrade.md`。
- **残留**:OQ-Build-3 仍 open(破产9);AC-30 deferred-gate 运行时断言待实现期激活。

**下一步:** 设计顺序 #9 破产与胜负(depends-on 2/5/6 均 Approved;承接 OQ-Build-3 in-kind 裁定 + 消费 F-10/F-9 口径)。
