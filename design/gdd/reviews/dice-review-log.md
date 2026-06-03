# Dice GDD — Design Review Log

> 骰子系统(systems-index #3,Foundation/Core/MVP,零依赖纯 RNG 服务)的复审历史。
> 每次 `/design-review design/gdd/dice.md` 追加一条。供 fresh re-review 追踪上轮 blocking 是否闭合。

---

## Review — 2026-06-02 — Verdict: NEEDS REVISION (R1, revisions applied)
Scope signal: S
Specialists: systems-designer / game-designer / qa-lead / unreal-specialist + creative-director (senior synthesis)
Blocking items: 6 (+ 2 qa-blocking) | Recommended: 8 | Nice: 4

**Summary (CD synthesis):** GDD 工程严谨度高(确定性契约、[Logic]/[Advisory] 分层防 flaky、强度坦诚标注是相对 player-turn 8 轮的真实进步),问题非分层框架失败,而是边角隐含概率/序列依赖、gate 缺口、与 GDD 内部两节自相矛盾。verdict NEEDS REVISION(非 MAJOR),全部单接缝文档收口、零公式/架构返工。**专家矛盾裁定:** systems 提议"浮点漏洞→重放强制走整数 RandomRange"被 unreal 核实 UE 源码反驳(RandRange 内部就走 FRand 浮点,该修复无效);CD 独立核实确认 unreal 成立,浮点跨平台漏洞**降为 Full Vision OQ-3**(MVP 单机不触发),正解=服务器权威 RNG 或自实现整数 LCG。

**6 blocking(已应用 R1 修订):**
- B1 [systems] 序列化丢 Die1/Die2 与 VFX 契约硬冲突 → 改序列化完整 FDiceRollResult(含 Die1/Die2);新增 OQ-T-Dice-5 回链 player-turn AC-34(**触发对已 APPROVED player-turn 的 design-change,须 /propagate-design-change**)。用户裁定:选项 A(补存)。
- B2 [systems+qa] 卡方数值错 → F-5 层二 df=11/31.26 改 df=10/29.59(与 AC-4 对齐);AC-3 bIsDouble 31.26 改 df=1/10.83;AC-5 补 N=72000 基数。
- B3 [qa] 无 RandomRange 范围不变式 [Logic] AC(最实质 gate 缺口)→ 新增 AC-12b(范围不变式)+ AC-12c(floor 不越界)。
- B4 [unreal+qa+systems] lazy-init 默认种子=0 静默砸穿"不可预测"→ Edge Case 强制非确定兜底种子(FPlatformTime::Cycles)+重放 fail-fast;新增 AC-17b。用户裁定:选项 A(非确定兜底)。
- B5 [game-designer] 掷骰签名 Sensation 时刻(concept priority-4)无端到端 owner → Visual/Audio 指派 VFX19 端到端 owner + experiential AC;OQ-T-Dice-4 回链。用户裁定:轻量指派 VFX19。
- B6 [game-designer] 定序双点静默 feedback 矛盾(开局(4,4)视觉双点却无反馈 vs 回合内触发额外回合)→ Visual/Audio 声明定序阶段 VFX 抑制双点反馈(语境归 player-turn);OQ-T-Dice-1 回链。用户裁定:选项 a(定序走 RollDice+声明抑制)。

**2 qa-blocking(已应用):**
- D-1 AC-9 概率断言 → 固化两组 expected[]、撞种子判断移测试编写期离线。
- D-2 AC-6 欠游标起点前提 → 补"同新流 0 起点、抽 Die1 前零额外抽取"前提。

**8 recommended(已应用):** OQ-1 删 UBlueprintFunctionLibrary(不能持实例流)+UWorldSubsystem 倾向+同生命周期层 ADR 硬约束 / 单线程重入(事件节广播=返回顺序+Edge Case+AC-16b/16c) / "流游标"用词改"当前 Seed"+GetCurrentSeed / F-2④数据驱动禁入确定性路径 / F-3 floor(f*N) N<2²⁴ 上界 / OQ-4 DiceCount>2 时 bIsDouble 重定义义务 / OQ-5 anti-tilt 方差压缩登记(从 CR-5 关死改待决) / S-5 OutcomeCount int32 跨度溢出边界。

**Nice(未全做):** bIsDouble↔Total 几何相关性(已作附注不变式) / Player Fantasy 缺真随机连号触发玩家偏见猜疑讨论。

Prior verdict resolved: 首次复审(First review)。

**待 R2 fresh re-review 核验要点:**
① B1 序列化完整性:全文 States/Interactions/Dependencies/Edge/OQ-2 一致序列化完整 FDiceRollResult、无残留"只存 (Total,bIsDouble)";OQ-T-Dice-5 design-change 是否已 propagate。
② B2 卡方数值三处(F-5/AC-3/AC-5)与 AC-4 全对齐、无残留 31.26 误配。
③ B3 AC-12b/12c 独立可测、纯确定性(非统计)。
④ B4 lazy-init 非确定种子在 Edge Case + AC-17b 两处一致、重放 fail-fast 钉死。
⑤ F-4 浮点降级干净性:性质 D 收窄"同平台"、跨平台 caveat 与 OQ-3 一致、无残留"任何平台(同字节序)完全相同"作 live 契约;"整数比浮点安全"错误建议已清除。
⑥ B5/B6 接缝 owner 落地:VFX19 owner 指派 + 定序抑制 + 双点语境(ConsecutiveDoubles)三条在 Visual/Audio + OQ-T 回链一致。
⑦ OQ-1 候选删 UBlueprintFunctionLibrary 干净、同生命周期层 ADR 约束明确。
⑧ **「修法落注释不落规格」失效模式核验**(项目反复教训):R1 大量改散文+AC,须确认修复真落规格面(公式/AC/契约表)而非仅注释自述闭合。

---

## Review — 2026-06-02 — Verdict: NEEDS REVISION (R2 fresh re-review, revisions applied)
Scope signal: S
Specialists: systems-designer / game-designer / qa-lead / unreal-specialist + creative-director (senior synthesis)
Blocking items: 4 must-fix | Recommended: 6 | Nice: 3
Prior verdict resolved: R1 NEEDS REVISION — 大部分闭合,见下 PASS 清单;残留 4 must-fix 全为"R1 既定修订的遗漏覆盖点"或"R1 修订自身引入的标签/约束缺陷"。

**Summary (CD synthesis):** R2 fresh re-review 确认 GDD 工程严谨度高,四专家独立验证 **B2 卡方全部数值(df=1/10.83、df=10/29.59、df=5/20.52、df=25/52.62 逐一重算)/ F-4 浮点降级干净 / OQ-1 框架 / [Logic]·[Advisory] 分层全表 / D-1·D-2 / B3·B4·B5·B6** 均 PASS。**本轮不构成 player-turn 式剥面循环**——R2 全部发现收敛于 R1 已建立的 3 个根(B1 序列化 / F-4 同源浮点 / AC gate 标签),是 R1 修订的收尾清扫而非新接缝涌现。verdict NEEDS REVISION(非 MAJOR:零公式/架构/状态机返工;非 APPROVED:4 must-fix 会误导下游实现者)。**强制 fresh re-review,不走 fast-close。**

**专家分歧(CD 裁定 + 用户采纳):** B5/B6 接缝 owner——game-designer 判 BLOCKING(掷骰爽感 Sensation 链在 GDD 级断裂、散文+回链=不可测真空)vs qa-lead+unreal 判 PASS。**CD 裁 PASS**(骰子是 S 级纯 RNG 服务,义务边界已自洽兑现,爽感闭合已正确指派 VFX19、载体存在只是不在本档;game-designer 选项 A/B 会把呈现层 AC 硬塞回 RNG 层=层级错位)。**用户采纳 CD 裁定 PASS**——处方为强化 OQ-T-Dice-4 回链(owner 转 MUST-FULFILL),不加呈现层 AC。

**4 must-fix(已应用 R2 修订):**
- M1 [qa+systems收敛] **OQ-T-Dice-5 跨 APPROVED 文档悬挂**(最高危)——OQ 登记"须 propagate"但未标是否已 propagate;player-turn 已 APPROVED、AC-34 仍旧契约,B1 整链依赖未兑现却静默假设闭合。→ 加 `propagate 状态:PENDING`(两处)+ 标"实现前必须 propagate"。**实际 /propagate-design-change 动作仍待执行(归 producer 协调)。**
- M2 [systems+unreal 三方收敛] **L65/L200 契约表 B1 残留**——Interactions/Dependencies 表玩家回合(2)行仍写"收 (Total,bIsDouble)",与 B1(回合须序列化 Die1/Die2)直接矛盾(只收 Total+bIsDouble 则无 Die1/Die2 可序列化)。「修法落注释不落规格」实例(B1 改对 States/Visual/OQ、漏改契约表)。→ 两行改"收完整 FDiceRollResult(含 Die1/Die2、序列化全字段)";顺手清事件格(7)行歧义。
- M3 [qa] **AC-17b 概率断言误标 [Logic]**——"两次冷启动 Total 序列不完全相同"是概率命题,违反"统计断言永不作 [Logic]"原则、复发 AC-9 刚修掉的同缺陷。→ 改确定性断言 `InitialSeed != 0`(覆盖 B4、零 flaky)。
- M4 [systems] **F-2 S-5 溢出约束错误**——`< INT32_MAX` 不够紧;RandRange 内走 FRand 浮点,真实约束 `跨度 < 2²⁴`(与 F-3 N 上界同源,整数路径实走浮点、两路径须对称)。→ 改 `< 2²⁴` + ensure。

**6 recommended(已应用):** systems-index VFX(19) Dependency Map 补 depends-on 3 / F-4 前提④归因"禁数据驱动"改"须跨运行稳定值" / AC-12c 删"守 N≥2²⁴"过度声明(大 N 登记 OQ-4) / OQ-5 补原作伪随机调查+playtest 锚点+支柱①③权衡框架 / Player Fantasy 承认"真随机连号触发猜疑"可得性启发风险+MVP 立场 / OQ-1·OQ-3 登记 unreal 5 项引擎陷阱(Listen Server HasAuthority、Subsystem 初始化顺序、热重载 Seed 重置、跨编译配置 /fp:fast、自实现 LCG 选参)。

**Nice(未做):** AC-8 fixture oracle 问题(expected 来自被测系统自身)/ AC-16"恰滚动两步"无可执行验证法措辞 / AC-21 GENERATED_BODY() 接口规格遗漏(编译门可捕获)。

**PASS(R1 修订已干净闭合,四专家独立验证):** B2 卡方全部数值无残留 31.26 / F-4 浮点降级("整数比浮点安全"已清除、性质 D 收窄同平台) / B3 AC-12b·12c 纯确定性 / D-1 AC-9 固化常量 / D-2 AC-6 游标起点前提 / B4 lazy-init 机制(标签问题=M3,机制已落规格) / B5·B6 接缝 owner 指派+回链 / OQ-1 删 UBlueprintFunctionLibrary+UWorldSubsystem 首选 / AC-16c 广播=返回同源 / 「修法落注释不落规格」大体闭合(唯 L65/L200 残留=M2)。

**待 R3 fresh re-review 核验要点:**
① M1 OQ-T-Dice-5 propagate 状态标记 PENDING 是否在 OQ 表+跨系统义务节两处一致;**且 /propagate-design-change 是否已实际执行、player-turn AC-34 是否已更新或显式标 PENDING**(跨文档闭合)。
② M2 L65/L200 契约表是否已改完整 FDiceRollResult、全文无残留"只收/只存 (Total,bIsDouble)"作回合接收契约。
③ M3 AC-17b 是否为确定性断言(InitialSeed != 0)、不含概率命题;[Logic] 分层标签仍正确。
④ M4 F-2 约束是否为 `跨度 < 2²⁴`、与 F-3 N 上界对称、有 ensure 规格;无残留 `< INT32_MAX` 作唯一约束。
⑤ **「修法落注释不落规格」第 N 次核验**:M1~M4 是否真落规格面(契约表/AC/公式)而非仅注释;特别是 M2(本轮正是该失效模式的实例)是否彻底清干净。
⑥ 6 recommended 是否落地无副作用;B5/B6 PASS 解法(OQ-T-Dice-4 MUST-FULFILL 强化)是否未越界加呈现层 AC。
⑦ 收敛判断:本批是否一次整批收口达收敛态(全部收敛于 B1/F-4 浮点/AC 标签三根)、无新接缝涌现——若 R3 仍剥新面则升结构性不收敛信号(参 player-turn 教训 [[gating-seam-keeps-peeling]])。

---

## Review — 2026-06-02 — Verdict: APPROVED (R3 fresh re-review, CONVERGED)
Scope signal: S
Specialists: systems-designer / game-designer / qa-lead / unreal-specialist (全 opus) + creative-director (senior synthesis)
Blocking items (doc): 0 | Implementation-gates: 3 | Recommended: 2 | Nice: 3
Prior verdict resolved: R2 NEEDS REVISION — 4 must-fix(M1~M4)四专家独立逐字符核验**全部真落规格面**(契约表/AC/公式,非注释自述),「修法落注释不落规格」失效模式本轮未复发。

**Summary (CD synthesis):** R3 **CONVERGED——非 player-turn 式剥面循环**。每条 R3 发现都落入①已开 OQ-1 ADR 容器、②兄弟工件(player-turn / systems-index),或③一行级 nice——**dice.md 自身规格面零新结构接缝被剥**。独立重算四组卡方 df/临界(10/29.59、1/10.83、5/20.52、25/52.62)全对、扫所有公式 min/max 边界零退化。GDD 裁定只由其**自身规格**正确性决定:两条"BLOCKING-ish"发现均在**别的工件**(player-turn AC-34 / 索引表),dice 已诚实 flag 正确 owner+PENDING,故为 fix-up 不是 doc-reopener。**Blocks the doc: nothing.** 成功判据:dice 代码推进全程不再重开 dice.md;R4 永不发生。

**专家分歧:无(R3 收敛)。** game-designer **主动 concede** R2 的 B5/B6 争论——认同 CD 裁定(掷骰爽感 owner=VFX19,不在 RNG 层加呈现层 AC),不重开。这是分歧收敛而非新分歧。

**3 implementation-gates(阻塞实现,非阻塞文档):**
- IG-1 **[producer, already-an-OQ] OQ-T-Dice-5 propagate 须实际执行**:`/propagate-design-change` 更新 player-turn AC-34(L471)+ 存档依赖(L369)从 `(点数,bIsDouble)` → 完整 `FDiceRollResult(含 Die1/Die2)`。dice B1 整链依赖;player-turn(APPROVED)当前**直接矛盾** dice 契约。dice 已诚实标 PENDING——不阻塞 dice 收敛,阻塞 B1 链实现。
- IG-2 **[~5行索引编辑, NEW, qa-lead+game-designer 收敛] systems-index 继承义务登记缺口**:dice 的 5 条 OQ-T-Dice 移交**全部缺席** `Inherited Test Obligations` 表(该表当前仅载 player-turn 义务)。须补 VFX(19) 行(掷骰爽感 MUST-FULFILL owner + experiential AC)+ 登记 OQ-T-Dice-1..5;并修 存档(21) 行残留的 `(点数,bIsDouble)`。这是**责任真空失效模式**(board-data 教训)下沉一层复发——单工件收口,非 dice 规格剥面。
- IG-3 **[architecture] OQ-1 ADR 须在 移动(4)/AI(10) 开工前关闭**:纳入 unreal NEW 发现——`ShouldCreateSubsystem` 须门控 `EWorldType::Game|PIE`,否则编辑器预览世界实例化第二个 DiceService、违反 CR-2 唯一权威流。

**2 recommended(非阻塞,一行级):** ① OQ-5 调查项①(查原作 Rento Fortune 是否含伪随机平滑)从"playtest 后"提前为"MVP 设计冻结前"(事实查证、不需 playtest,可能推翻"真随机=支柱①忠实"前提);② L100/L172 "pre-5.3 稳定"措辞加 L177 知识盲区 caveat 回链(5.4–5.7 未验证)。

**Nice(未做):** AC-16 "恰滚动两步"措辞无可执行验证法(与 AC-8/10 冗余,可改写或删)/ AC-8 fixture oracle 自指(golden-master 固有限制,as-is 可接受)/ OQ-3 补"UE5.7 官方 RNG 子系统(若存在)"第三解支。

**PASS(R2 修订四专家独立验证已干净闭合):** M2 契约表无残留 `(Total,bIsDouble)` 作接收契约 / M3 AC-17b 确定性 `InitialSeed!=0`(AC-9 同模式缺陷未复发)/ M4 F-2 `跨度<2²⁴`+ensure 与 F-3 对称 / F-4 浮点 caveat 干净(无 "整数比浮点安全"残留)/ [Logic]·[Advisory] 20+8 分层全表正确 / OQ-1 框架引擎正确(R2 三 sub-note 全准)/ 双点好坏语境委派干净无泄漏 / 知识盲区 L177 caveat 为正确 posture。

**收敛结论:** 3 轮收敛,干净退出 [[gating-seam-keeps-peeling]] 风险。M1 propagate 是唯一跨文档悬挂(归 producer),其余 2 gate 为机械收口。dice.md 文档面已 APPROVED 且不再重开。

**Gate 闭合更新(2026-06-02,本会话后续):**
- ✅ **IG-2 已闭合** —— systems-index `Inherited Test Obligations` 表补登 OQ-T-Dice-1..5 + VFX(19) 掷骰爽感 MUST-FULFILL 行 + 新增 玩家回合(2) 行;清 存档(21) 行残留 `(点数,bIsDouble)`。
- ✅ **IG-1 已闭合** —— 执行 `/propagate-design-change`(GDD→GDD,本项目无 ADR):player-turn 7 处 RECEIVE+SERIALIZE 契约从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`(含 Die1/Die2);AC-33/34 加字段、门控不变;dice OQ-T-Dice-5 状态 RESOLVED。详见 player-turn-review-log.md 的 propagate 条目。
- ⏳ **IG-3 仍开放** —— OQ-1 宿主形态 ADR(归 architecture,移动(4)/AI(10) 开工前关闭,纳入 `ShouldCreateSubsystem` 世界类型门控)。
