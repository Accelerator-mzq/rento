# AI 对手 (AI Opponent) — Review Log

> 系统 #10。审查历史,供 fresh-session 再审追踪改动。

## Review — 2026-06-05 — Verdict: NEEDS REVISION → 就地修订完成(待 fresh-session 再审)

- **模式:** full(6 specialist 并行 + creative-director 终审 synthesis)
- **Scope signal:** L(7 依赖、7 公式 F-1..F-7、横跨几乎全 gameplay、gated on 架构期 OQ-1 GameStateSnapshot ADR)
- **Specialists:** economy-designer / systems-designer / ai-programmer / game-designer / qa-lead / unreal-specialist + creative-director(senior)
- **Blocking items:** 6 | Recommended/must-land:10 | Prior verdict:First review

### 终审结论(CD synthesis)
工程结构干净收敛(无环纯叶子、签名逐字一致、确定性纪律)。NEEDS REVISION(非 MAJOR——无架构返工、无支柱重裁、heuristic-only 既定边界内可整改;非 APPROVED-with-followups——含 3 条 formula-soundness + 1 条不可证伪 gate + 1 条头号风险无硬底线)。三条 BLOCKING 跨专家收敛(F-1 Buffer / AC-38 / AI-犯蠢 gate)。

### 6 BLOCKING(本轮就地修订,fresh-grep 核实)
1. **F-1 Buffer 整簇**(economy B-3/B-4 + systems B-1 + unreal B-1):后期 clamp 钉死 Buffer_max 丧失 B_frac 语义(现金充裕不建房=concept 头号风险基线触发)+ 贪心门只防单次最差租 + MaxRentExposure 字段在 CR-6 缺失。→ MaxRentExposure 改 top-2 之和;Buffer_max 抬至 700/1400/1800 [600,2200];CR-6 三 hook 补 Rent_top1/top2(+ DecideJailAction 补 owner map/board_tile_count);AC-9/10/10b/11 重算。
2. **F-2 BuyScore**(economy B-1/B-2):四项量纲失衡(MonopolyProgress 占~86%,L142 自证)+ RentPotential 用垄断×2 与 MonopolyProgress 双重计入。→ RentPotential 归一 [0,10]、去 ×2(垄断价值唯一归 MonopolyProgress);AC-19→26。
3. **AC-38 可证伪性陷阱/死锁**(qa B-2 + ai-prog B-2 + game-des GD-B1):固定 seed 零方差却用"提高 K 非降阈值"=逻辑上无法 FAIL;①胜率≥60% 可能超无前瞻上限、唯一出口在 Alpha=死锁。→ seed-lock 20 seeds、删逃逸条款、阈值冻结写死(②③④ 行为差异承重义务⑤);①胜率降 AC-38b/OQ-AI-1 非 gate。
4. **F-5a expected_roi 悬空**(economy R-3,verified):全档仅 1 处引用无定义。→ 删悬空子句 + PAYBACK_THRESHOLD 一并移除(变量表/F-7/Tuning)。
5. **头号风险"AI 犯蠢"无 BLOCKING gate**(qa B-3 + game-des GD-B2 + ai-prog B-1):OQ-AI-2 无对应 AC。→ 新增 AC-58/59/60/60b 可枚举反幻想硬底线(现金充裕集组必建/可补垄断最后格必买/不抵押垄断黄金地/Casual ε 不漏补全格)。
6. **共享 RNG 流重放污染**(ai-prog B-3):AC-42 重置 seed=伪验证,VFX juice 走权威流未设防。→ CR-5④ 权威流隔离规则 + AC-61 全回合重放隔离 [Integration]。

### 10 Recommended / must-land(本轮一并落)
- CR-6 → OQ-1 ADR propagate 债登记 + snapshot const-ref/构造成本裁定(OQ-AI-3,unreal B-1)
- AC-47 tier 三方矛盾 → 钉 [Integration];计数行重算 **67 条(65 实测)= 61 BLOCKING + 4 Advisory**(qa B-1)
- AC-62 跨调用无泄漏(systems B-2)+ AC-63 F-4 均衡谓词一致性回归守护(systems B-3)
- systems-index line 133 stale depends-on → `1,2,3,5,6,7,8`(OQ-AI-5)
- AC-56 降 [Advisory·Performance] nightly + AC-63b 可数硬底线作 PR gate(unreal B-2/qa R-2)
- CR-7 单线程漂移边界(systems R-1);F-4 赎回-解锁建房级联(ai-prog R-3)
- Player Fantasy「坑/赢」MVP 范围诚实(game-des GR-R2)
- OQ-AI-3b(BP/C++ 裁定=义务②前置,unreal B-3)、OQ-AI-8(整圈暴露模型)、OQ-AI-9(跨 hook 融资盲区)
- CI-stub 关闭条件:OQ-1 关闭后同 sprint 转真实 [Logic](qa R-1)

### 独立 fresh-grep 核实(主审,勿再 re-litigate)
- N=1 回填 player-turn AC-37b = **真双侧落地**(L415+L528),继承义务④ 闭合(qa"单侧真空"疑虑已 deflate)。
- 三 hook 签名与 player-turn CR-8 L171/173/174 **逐字一致**。
- expected_roi 悬空、CR-6 字段缺口、AC-47/计数不自洽 = 均证实。

### deflate(裁定,勿当 blocker 重提)
- game-designer 自报"可降 APPROVED-with-followups"让步 = CD 不接受,但 GD-B1/B2/B3 已并入 #3/#5 非静默 drop。
- unreal B-3 BP 无法机器禁随机 = 已知 BP-only 软约束,靠 OQ-1 ADR 裁定 C++ 核心(OQ-AI-3b),不阻 doc-approval。

### 待办
- **fresh session `/design-review design/gdd/ai-opponent.md`** 独立验证本轮修订(勿同会话)。
- 跨档 propagate 债(同 PR/下游):CR-5④ VFX(19) "juice 走独立流"接收义务;CR-6 字段清单 → OQ-1 ADR 注入方;OQ-AI-3b BP/C++ 裁定 → 架构期。

## Review — 2026-06-05 (R2) — Verdict: NEEDS REVISION (light) — 设计已收敛, 4 BLOCKING 待一次性修订 PR

- **模式:** full(6 specialist 并行 + creative-director senior synthesis),**fresh-session 独立复审 R1 就地修订**。
- **Scope signal:** M(7 依赖、7 公式,但修订为局部 formula-tuning + test-spec 编辑,无新 ADR)。
- **Specialists:** economy / systems / ai-programmer / game-designer / qa-lead / unreal-specialist + CD(senior)。
- **Prior verdict resolved:** 部分——R1 的 6 BLOCKING 全 VERIFIED-FIXED(≥2 specialist 各自核实);R2 揭示 4 个二阶 finishing 项。

### 主审 fresh-grep 核实(勿 re-litigate)
- 三 hook 签名 ≡ player-turn CR-8 L171/173/174 逐字一致;N=1 回填 player-turn AC-37b **双侧落地**(L415+L528);systems-index #10 depends-on 已订正 `1,2,3,5,6,7,8`。
- **AC 计数独立重算 EXACT:65 实测 = 61 BLOCKING(53 Logic + 4 CI-stub + 4 Integration)+ 4 Advisory**,与 L423 完全吻合、零误差。
- 架构干净:无环纯叶子、return-only、确定性纪律、RNG 单源。
- dice(3) 交叉核实:CR-5④「权威流消费方∈{dice,AI}」与 dice 单一权威流模型一致;dice `RandRange(Min==Max)` 短路不推游标(缓解部分 RNG-drift);**dice 确定性仅同平台**——AI L102「保跨平台确定性」略超承诺(降 Recommended)。

### 4 BLOCKING(一次性 finishing PR,无须 re-review-from-scratch)
1. **Buffer_max clamp 复发**(F-1 L119/227/229)——B-3 修复值偏低。Sharp B_frac=1.5 × MaxRentExposure>1200(中后期常态)再钉 1800、Normal 钉 1400,50% 档差塌为 400 → 现金充裕不建房(concept #1 风险)中后期再触发。数值论证非抽象。
2. **B-1 修复未到 test-spec 层**(AC-5/6/7 GIVEN vs CR-6 L63-65)——字段清单仍缺 `Rent_top1/top2`;AC-5 缺 `starting_cash`(F-2 LiquidationDiscount 分母);AC-7 缺 `owner map`/`board_tile_count`。CI-stub fixture 会撞 B-1 同款「无载体」失效。机械修。
3. **AC-61 vacuous-pass**(L418)——spy「呈现层未消费权威流」在 VFX(19)/HUD(16) 尚未实现时 trivially pass。拆 AC-61a `[Logic 立即可测]` + AC-61b `[Integration 待 VFX 接入]`。
4. **AC 断言公式无法兑现的保证**——AC-60b(L417)要求「补全格豁免 ε」但 F-3(L154)对所有格均匀加 ε、无豁免分支(AC-59 同缺口);**且** R-3 赎回-解锁-建房级联(L183)是意向非规格(赎回无 BuildScore、联合现金门未定义、无 AC)。须加 F-3 补全格分支 + 规格化级联,或为 AC 加参数约束前提。

### Specialist 分歧(user 裁定:保 CD tight 4,下列降 Recommended)
- **#2 RentPotential [0,10] 饱和** — economy: BLOCKING(真实 Monopoly 数值聚 7-10,MonopolyProgress 仍主导垄断补全决策 ~36 vs ≤12,B-1 仅部分修);CD: 降 Recommended(AC-59 硬底线已强制关键买入)。**user 裁定 → Recommended**。
- **#5 AC-38 买地率方向**(L381/429)— ai-programmer: BLOCKING(Sharp 高 BuyThreshold 可能买地反少 → ④「Casual≤Sharp×0.8」天生 FAIL);CD: Recommended(OQ-AI-1 已自标 + 给重调出口)。**user 裁定 → Recommended**。
- **#8 HeldInGroup 买前/买后歧义**(L132/141)— economy: BLOCKING(首格 MonopolyProgress 1-vs-4 = Sharp 12 分摆动,HeldInGroup=0 无 AC);**user 裁定 → Recommended**(同 PR free add)。

### Recommended 簇(同 PR 一并落)
BuyScore 下界 −40→−26(MonopolyProgress 永 ≥1) · F-4 Example RentTable `[14,10,30…]` house1<base 笔误(使 AC-28 自相矛盾) · LiquidationDiscount Example 依赖未声明 `starting_cash=1500` · 定点乘序「建议」→「必须」+ AC-63b 2500 上界未推导(纳入赎回候选 ≈3136)+ L102 跨平台超声明 · `OnAIActionExecuted` 须标 `BlueprintAssignable`(propagate player-turn) · CR-7 现金预扣正确性无 BLOCKING AC · NP-1/NP-2 范围诚实(「AI 抢地卡位」是统计偶发非主动;Casual B_frac=0.6 频繁破产 ≠「宽容陪练」、Sharp 高门槛少地 ≠ 威胁)+ 补 Casual-存活/Sharp-棋盘存在感 OQ 底线。

### Nice-to-Have
stale OQ-AI-4/OQ-AI-5(已落地却仍标 pending Phase-5)· 「含 0 永久 stub」措辞 vs 4 CI-stub · snapshot 构造期排除不可见信息(OQ-1 ADR)非依赖 AI 被动忽略 · F-4 贪心循环内虚拟 house_count 副本未命名。

### CD senior synthesis(终审)
**APPROVED-with-followups。** 7 个新发现共享三性质:零架构返工、零支柱/heuristic-only 边界破坏、且均为「在散文层修 6 BLOCKING 而未扫清数值尾巴与 test-spec 层」的可预期二阶残留。诊断:非收敛 = 须重裁「系统是什么」;收敛 = 须「完成已定裁决的传播」——7 项皆后者。真 BLOCKING = 4,一次性 PR,无须重启 5-agent re-review。

### 主审补充(收敛但须验数)
认同设计已收敛(架构干净、R1 全 BLOCKING 设计层已闭、无 redesign)。但 4 项是真 must-land-before-implementation 缺陷(复发的偏低调参 / 未到 AC 的修复 / vacuous gate / 公式无法兑现的 AC),doc 尚非实现就绪。**关键告诫:#2/#3 之所以存在,正因 R1 修复仅散文层、未对照数值/fixture 验证——R3 修订须对照数学与 AC fixture 核验,非仅改措辞**,以免第三轮「followup 又留洞」。

### 待办(R3)
- **fresh session 修订**:一次性 PR 修 4 BLOCKING + Recommended 簇,对照 F-1/F-2 数值与 AC-5/6/7 fixture 核验,然后标 Approved 无须 5-agent 重审。
- 跨档 propagate 债(同 PR/下游)延续:CR-5④ VFX(19) juice 独立流(dice 模型=自建 FRandomStream,非 dice 暴露第二流——R3 措辞订正)· CR-6 字段 → OQ-1 ADR · OQ-AI-3b BP/C++ → 架构期 · OnAIActionExecuted BlueprintAssignable → player-turn。

## Review — 2026-06-06 (R-3) — Verdict: APPROVED_WITH_FOLLOWUPS〔finishing-class 收敛终结轮〕→ #10 Approved

- **模式:** full(6 specialist 并行 + creative-director senior synthesis),**fresh-session 独立复审 R-2 修订 PR**。
- **Scope signal:** M(7 依赖、7 公式;修订为局部 formula-tuning + test-spec/公式分层对齐,无新 ADR、无架构返工)。
- **Prior verdict resolved:** R-2 的 4 BLOCKING「在散文层修了但数值/fixture/公式分层未对齐」→ R-3 验出 5 个 finishing-class 二阶残渣(均 R-2 已识别、已给定处方,无新正交维度、无支柱重裁、无「系统是什么」的 identity fork)。
- **收敛信号:** blocker 质量逐轮稳定 6→4→5,全部可机械或单点路由收口,符合 R-2 CD「一次性 PR 修完即 Approved」裁定路径。**verdict=APPROVED_WITH_FOLLOWUPS**,不退回 5-agent 全量复审。

### 5 verified_blockers(全 finishing-class,已给定处方随一次性修订 PR 收口)

1. **R-2 BLOCKING #1 未落:Buffer_max 偏低致 Sharp/Normal B_frac 档差在中后期塌为固定常数。** 数学已核:MaxRentExposure=1600(文档自有 Example 暴露水平)时旧值 Sharp=clamp 1800 / Normal=clamp 1400,差=400(未 clamp 应为 2400:1600=800 差),50% 档差确塌为固定 400,且触发点恰在文档自有 Example 暴露水平。**处方〔autonomous〕:** F-7(L229)Sharp Buffer_max 1800→≥2700(使 floor(1.5×1800)=2700 才触上界);同步 (a) F-1(L119)`Buffer_max` 范围上界 [600,2200]→[600,≥2700];(b) AC-11(L351)上界 fixture 用新值;(c) L122 散文「不再被 clamp 钉死」与新数值自洽(否则改诚实表述);(d) 补一条 [Logic] AC 验证 MaxRentExposure=1600 时 Buffer(Sharp):Buffer(Normal)≈1.5:1.0 比例(非固定 400 差)。
2. **R-2 BLOCKING #2 未落:AC-5/6/7 GIVEN 字段清单与 CR-6 L63-65 不一致(CI-stub 无载体失效)。** 逐条核实三 AC GIVEN 当前文本确缺字段、CR-6 确声明需求。**处方〔autonomous〕机械补全:** AC-5(L341)加 Rent_top1/Rent_top2(算 F-1 Buffer)+ starting_cash(F-2 LiquidationDiscount 分母);AC-6(L342)加 Rent_top1/Rent_top2;AC-7(L343)加 owner_map + board_tile_count_classic + Rent_top1/Rent_top2(算 F-6 BoardDensity/F-1 Buffer)。CI-stub 阶段用固定 placeholder(Rent_top1=500/Rent_top2=300/starting_cash=1500/board_tile_count_classic=40),注 OQ-1 ADR 关闭后换真实注入。
3. **R-2 BLOCKING #3 未落:AC-61(L418)单条 [Integration],呈现层未实现致 spy 断言②vacuous-pass。** **处方〔autonomous〕拆两条:** AC-61a [Logic]——headless 隔离环境(无 VFX/HUD)连跑两次完整多回合序列、逐步全同(端到端重放,立即可测);AC-61b [Integration·门控 VFX(19)/HUD(16) 接入]——注入 VFX/HUD stub 挂权威流 spy,断言消费方∈{dice,AI}、呈现层未消费(待接入后转非 vacuous)。更新合计行 L423(Logic+1,total 65→66 实测;Integration 仍计 AC-61b)。
4. **R-2 BLOCKING #4a 未落:F-3(L154)无补全格豁免分支,AC-60b(L417)断言无公式支撑。** **处方〔logged_decision——采路线 A〕:** F-3 补短路分支 `if (HeldInGroup+1)==GroupSize then FinalScore=BuyScore(跳过 ε 扰动) else FinalScore=BuyScore+RNG.RandRange(−Epsilon,+Epsilon)`,变量表补 IsCompletionTile/补全格谓词定义,AC-60b 的 WHEN 引用此分支。AC-59 非 Casual Epsilon=0 时自然满足,无需额外分支。
5. **R-2 BLOCKING #4b 未落:F-4(L183)赎回-解锁-建房级联为意向散文,无联合现金门规格+无 AC。** **处方〔autonomous〕规格化级联:** (a) 定义解锁候选谓词(垄断组有抵押格 ∧ 赎回后 ≥1 格 CanBuildHouse 变 true ∧ 联合现金门 Cash−unmortgage_cost−BuildingCost(最优解锁后建房)≥Buffer);(b) 赎回前置动作的预期收益=赎回后首栋 BuildScore(与贪心候选同量纲);(c) 新增正向 AC-64 [Logic](垄断组单格抵押致整组 CanBuildHouse==false ∧ 满足联合现金门 → 返回序列含有序对 [Unmortgage(A),BuildHouse(B)] 且赎回先于建房);(d) 负向 AC(仅够 unmortgage 不够 unmortgage+build → 序列不含该 Unmortgage)。更新合计行计数。

### logged_decisions(可推翻判断)

- **#4a 采路线 A(F-3 补全格短路分支 FinalScore=BuyScore),而非路线 B(AC-60b 加参数前提)。** 理由:路线 A 与 AC-60b 文字契约「扰动仅作用于非补全格的边际决策」语义直接对齐,且修复点落在权威公式层(F-3)而非测试前提层,符合「修法落规格不落 AC 措辞」原则——R-2 主审明确告诫第三轮须对照数学/公式核验而非仅改措辞。路线 B 把豁免责任推给 AC 前提,留下「实现可不豁免、AC 仍 green」的二阶洞风险。**可推翻:** 若 user/作者偏好最小公式改动且接受 AC-60b 以参数前提界定豁免边界,可改用路线 B(F-3 不动);两路均能闭合 AC-60b,选择属呈现/规格分层偏好,非正确性分歧。
- **5 条 verified_blocker 全裁 finishing-class、verdict=APPROVED_WITH_FOLLOWUPS,不退回 5-agent 全量复审。** 理由:5 项均为 R-2 已识别、已给定处方的二阶残渣(散文/数值/fixture/公式分层未对齐),无新正交维度、无架构返工、无支柱重裁、无 identity fork;blocker 质量逐轮稳定且全部可机械或单点路由收口,符合 R-2 CD「一次性 PR 修完即 Approved」裁定路径。**可推翻:** 若 R-3 修订 PR 落地后再审发现 BLOCKING #1 的 Buffer_max 新值或 #4a 的 F-3 分支引入新缝(如新 Buffer_max 触发其它 clamp/AC 连锁),应升级为 NEEDS_REVISION;但本轮文档静态实据不支持预判该风险。

### 待办(followup,不阻开工)

- **一次性修订 PR** 落地上述 5 处处方(对照 F-1/F-7 数值、AC-5/6/7 fixture、F-3/F-4 公式分层核验,非仅改措辞)。落地后即随 #10 Approved,无须再启 5-agent re-review。
- 跨档 propagate 债延续:CR-5④ VFX(19) juice 独立流接收义务;CR-6 字段 → OQ-1 GameStateSnapshot ADR 注入方;OQ-AI-3b BP/C++ 裁定 → 架构期;OnAIActionExecuted BlueprintAssignable → player-turn。

## Review — 2026-06-06 (R-4) — Verdict: NEEDS REVISION — **R-3 过批纠正**:仍有 2 BLOCKING / 熔断

- **模式:** full(5 specialist 隔离并行 + senior synthesis),**fresh-session 独立复审 R-3 一次性修订 PR 落地结果**。
- **Scope signal:** M(7 依赖、7 公式;复审聚焦 R-3 处方落地后的规格层一致性,无新 ADR)。
- **Prior verdict 纠正:** R-3 的 **APPROVED_WITH_FOLLOWUPS 系过批**。R-3 的 5 verified_blocker 处方**确已应用**到正文规格层(WF-1b Apply 阶段,5 处方均落地),但 R-4 fresh 隔离复审独立验证揪出 **2 个新/残留 BLOCKING**——其中 1 条是 R-3 处方自身引入的**新接缝**(F-4 贪心守卫与新增 unlock 候选路径冲突),1 条是 R-3 级联规格化时遗漏的**操作数未定义**。故 #10 由 Approved 撤回为 NEEDS REVISION,Progress Tracker reviewed/approved 各 −1(撤回 WF-1 的 +1)。

### 过批根因
R-3 处方 #5(F-4 级联规格化)新增了 unlock 候选路径(赎回-解锁-建房序列)与联合现金门,但:① 未同步修改 F-4 贪心循环的守卫条件,导致权威伪代码 `while ∃ CanBuildHouse 候选` 在「整组 CanBuildHouse==false」场景下直接退出循环、**不可达**评估 unlock 候选的代码路径——而这正是 R-3 新增 AC-64 的 GIVEN 场景,AC-64 在权威伪代码下**结构性不可达**;② 散文意图(L190-194)与 L189 伪代码不一致(散文描述了 unlock 路径,伪代码未实现),违反「修法须落规格不接受散文自述闭合」原则;③ 级联引入的两个新操作数 `unmortgage_cost`/`BuildingCost` 仅在散文/现金门表达式中出现,未进 F-4 变量表,违反 coding-standards §4「Formulas 须含变量定义与范围」。

### 2 verified_blockers(R-4,须用户裁定修订方向后再审)

1. **F-4 贪心循环守卫 `while ∃ CanBuildHouse 候选` 排除 unlock 候选——AC-64 在权威伪代码下不可达(R-3 新接缝)。**
   **处方〔autonomous〕:** 扩展 F-4 贪心循环守卫覆盖两类候选:`while ∃ (CanBuildHouse 候选 OR 满足解锁候选谓词的 unlock 候选)`,unlock 候选以「赎回后首栋 BuildScore」代入 argmax;或将 unlock 路径抽到循环前单独检查(无 CanBuildHouse 候选且满足联合现金门时,先执行赎回-建房序列再重评 CanBuildHouse 集)。同时在伪代码中明确 unlock 候选用联合现金门 `Cash − unmortgage_cost(A) − BuildingCost(首栋最优) ≥ Buffer`、普通候选用单门 `Cash − BuildingCost(best) ≥ Buffer` 的差异化筛选时机(在 argmax 前预过滤)。修订后须确保 AC-64 的 GIVEN 场景(整组 CanBuildHouse==false)能到达评估 unlock 候选的代码路径,且 AC-65 负向 fixture(联合门不满足)在新守卫下仍可证伪。L189 伪代码与 L190-194 散文意图须逐字符一致,不接受散文自述闭合。

2. **F-4 变量表缺级联新增操作数 `unmortgage_cost` 与 `BuildingCost`——公式操作数未定义(coding-standards §4)。**
   **处方〔autonomous〕:** 在 F-4 变量表(L187 之后)补两行:① `unmortgage_cost(A) | int32 | 注册表口径(=MortgageValue+ceil(MV/10),来源 F-5/economy-5) | 格 A 赎回费用(级联联合现金门分量)`;② `BuildingCost | int32 | 注册表口径(来源 building-8/economy-5) | 建一栋房费用(per color-group)`。范围引用注册表条目或经典值。散文提及不等于变量表收录;L191 联合现金门两个操作数必须进表才满足「Formulas 须含变量定义与范围」。

### deflate(裁定,勿当 blocker 重提)

- **AC-9 fixture 硬编码 Buffer_max=1400 与 F-7 Normal 1800 失配**(economy/systems/game-designer 三方 RECOMMENDED)——引文属实(L359 确为 Buffer_max=1400,F-7 Normal=1800)。但 AC-9 测退化下界路径(MaxRentExposure=0 → Buffer==150),结论不依赖 Buffer_max,测试不会 FAIL。属 stale fixture 文档失配,非 BLOCKING → 降为 followup(改 1800 或加注「下界测试上界任意」)。
- **AC-23 缺 IsCompletionTile=false 显式前提,F-3 短路分支致 ε 扰动 vacuous**(systems RECOMMENDED)——引文属实(L375 GIVEN 未指定 IsCompletionTile;F-3 L154-159 补全格短路跳过 ε)。真实 vacuous 风险但仅当实现者以补全格构造 fixture 时;且断言本身仍 pass。降为 followup(GIVEN 补 IsCompletionTile=false),非 BLOCKING。
- **game-designer F-4 RECOMMENDED:贪心伪代码未明确解锁候选差异化现金门造成实现歧义**——属实且正确,但与 economy F-4 BLOCKING 为同一接缝的不同视角(实现歧义 vs 公式自相矛盾)。已并入 R-4 BLOCKING #1 的 prescription(明确联合门 vs 单门筛选时机),不单列。
- **AC-59 BuyScore≥BuyThreshold 依赖散文背书而非推导**(game-designer ADVISORY)——文档质量建议(在 F-3 补 1-2 行推导),非缺陷:专家自验无反例(MonoProg 最小场景仍超 BuyThreshold)。ADVISORY,可选 followup。
- **合计行 57/Integration 净计数说明**(economy ADVISORY + game-designer 多条确认性 ADVISORY)——专家自验合计算术自洽(53+4=57 Logic,Integration 删 AC-61 加 AC-61b 净 0=4,69 实测+2 锚点=71 与 L438 吻合)。建议补拆分括注以防后续审计误计,纯防御性 followup,非缺陷。修订①/②确认性报告(AC-10/11b/5/6/7 真落规格层)均核验通过、无需修订。

### 须用户裁定的方向

两条 BLOCKING 的处方均标 〔autonomous〕(机械/单点路由可收口),但因 R-3 已被证为「散文层修了未对照公式/伪代码核验」的过批,R-4 不再自证收敛:**须用户裁定修订方向**(BLOCKING #1 选「扩展贪心守卫」还是「unlock 路径抽到循环前」两条路线之一;确认 #2 变量表补录口径),修订 PR 落地后须 fresh-session 再审独立验证 L189 伪代码与 L190-194 散文逐字符一致、AC-64 可达、AC-65 可证伪,方可标 Approved——**不接受散文自述闭合,不接受同会话自证**。

### 待办(R-5)

- **用户裁定** BLOCKING #1 修订路线 + #2 变量表口径确认。
- **fresh session 修订 PR** 落地 2 处处方,对照 F-4 伪代码/散文逐字符一致 + AC-64 可达性 + AC-65 可证伪核验(非仅改措辞)。
- **fresh session 再审** 独立验证后方可标 Approved,撤回的 reviewed/approved 计数届时回补。
- followup(不阻修订):AC-9 fixture Buffer_max 失配、AC-23 GIVEN 补 IsCompletionTile=false、AC-59 推导补行。
- 跨档 propagate 债延续(不变):CR-5④ VFX(19) juice 独立流;CR-6 字段 → OQ-1 ADR;OQ-AI-3b BP/C++ → 架构期;OnAIActionExecuted BlueprintAssignable → player-turn。

## Review — 2026-06-06 (R-5) — Verdict: NEEDS REVISION — **维持**:仍有 1 BLOCKING(F-4 连续第二轮 peel 新接缝)

- **模式:** full(specialist 隔离并行 + senior synthesis),**fresh-session 独立复审 R-4 两处处方落地结果**。
- **Scope signal:** M(7 依赖、7 公式;复审聚焦 R-4 处方落地后的规格层一致性 + F-4/F-5 赎回路径交互,无新 ADR)。
- **Prior verdict resolved:** 部分——R-4 的 2 BLOCKING 处方**确已落地且核验通过**(见下「R-4 处方落地核实」),但 R-5 隔离复审在 **F-4 与 F-5 的接缝**揪出 **1 个新 BLOCKING**(F-4 unlock 赎回路径 ↔ F-5b 主动赎回未去重)。此为 **F-4 连续第二轮(R-4→R-5)peel 出新接缝**——R-4 修复 unlock 路径的可达性,R-5 暴露 unlock 路径与既有 F-5b 路径的双发冲突。

### R-4 处方落地核实(主审独立逐条推演,非采信 qa-lead claim)

- **R-4 BLOCKING #1(守卫扩展)= VERIFIED-FIXED。** F-4 贪心循环(L191-208)守卫已改为 `Cands := { c | IsBuildCand(c) } ∪ { u | IsUnlockCand(u) }`(L195),两类候选同入;unlock 候选用联合现金门 argmax 前预过滤(L203/L208/L211)、普通候选用单门(L200)。L189-208 伪代码与 L190-214 散文意图**逐字符一致**(IsUnlockCand 谓词 L211、EffScore 同量纲 L198/L212、序列顺序保证 L205-206/L213)。AC-64 GIVEN(整组 `CanBuildHouse==false`)现可达评估 unlock 候选路径;AC-65 负向 fixture(联合门不满足)在入集前预过滤仍可证伪。
- **R-4 BLOCKING #2(变量表补操作数)= VERIFIED-FIXED。** F-4 变量表 L188/L189 已补 `unmortgage_cost(A)`(注册表口径 =MortgageValue+ceil(MV/10),来源 F-5/economy-5)与 `BuildingCost`(注册表口径,来源 building-8/economy-5,每档一单位、棋盘1供值),范围引注册表条目,满足 coding-standards §4。
- **R-4 deflate 三项(AC-9 / AC-23 / game-designer F-4 实现歧义)** 维持 followup,本轮不重提。

### 1 verified_blocker(R-5,须用户裁定去重方向后再审)

1. **F-4 unlock 赎回路径(L205 `emit Unmortgage(A)`)与 F-5b 主动赎回(L222 `ShouldUnmortgage`)未去重——AC-64 核心正向场景下同一抵押格 A 必然被双路径双发 `Unmortgage(A)`,违反 CR-7/L69「MVP 静默跳过应零常规发生」纪律,且无 AC 锚定证伪。**

   **独立核验属实(主审逐条推演,非采信 claim):** AC-64 GIVEN 联合门 `Cash − unmortgage_cost(A) − BuildingCost(B) ≥ Buffer`(F-4 L203/L211)满足时,F-5b 单门 `Cash − unmortgage_cost(A) ≥ Buffer×U_frac`(L222;U_frac∈[1.0,2.0] L227,下界 1.0=Sharp 时被联合门蕴含——联合门左边再减一个非负 `BuildingCost` 仍 ≥Buffer,故单门必然成立)同时满足;A 满足 `is_mortgaged ∧ A∉MortgagedThisTurn`(A 是回合开始即抵押格、非本回合抵押,L228/L287 churn 守卫对它**失效**)`∧ is_monopoly`(组),故 F-4 unlock 路径与 F-5b 主动赎回在同一 `DecidePostRollActions` 调用内**都会选中 A 并各 emit 一条 `Unmortgage(A)`**。全档(F-4 L191-216 / F-5 L218-231 / CR-3④ L45 / Edge Cases L280-287)**无任何声明两条赎回路径共用去重集或求值顺序**;`MortgagedThisTurn`(L228)语义只防「本回合抵押→同回合赎回」churn(L287 印证),不防双赎。执行层 player-turn CR-8 重校时第二条 `Unmortgage(A)` 因 A 已非 `is_mortgaged` 而静默跳过(不崩、不负现金),但 L69 明文「单线程 AI 回合内不应有合法漂移,静默跳过在 MVP 应为零常规发生——日常出现非零跳过 = AI 预扣逻辑 bug、非正常路径」——而双发跳过在 AC-58/AC-64 这类反幻想硬底线**核心场景**下是**必然常规发生**,非边缘漂移,构成文档内部矛盾(CR-7/L69 零跳过纪律 ⇄ 核心场景必然双发跳过)。且全档无 AC 证伪(AC-29 只防 churn、AC-62 只防跨调用泄漏),实现者可自由双发而测试全 green。

   **处方〔logged_decision〕:** 在 F-4 贪心循环与 F-5b 间补一句共享去重裁定——unlock 候选与 F-5b 主动赎回**共用一个 `UnmortgagedThisTurn` 去重集**(任一路径 emit `Unmortgage(A)` 后 A 入集,另一路径不再选 A),或显式声明二者求值顺序(如 F-4-unlock 先评估、F-5b 对已赎格跳过);并新增一条 [Logic] AC:同一 `DecidePostRollActions` 返回序列内,同一抵押格至多一条 Unmortgage 动作(负向可证伪)。修订后须 fresh-session 再审独立验证去重集落权威伪代码层、新 AC 在「A 同时满足 F-4 联合门 + F-5b 单门」fixture 下能 FAIL 双发实现。

### 须用户裁定的方向

- 处方虽标可机械收口(共享去重集 / 显式求值顺序二选一),但**因 F-4 已连续两轮(R-4 unlock 可达性 / R-5 unlock×F-5b 双发)peel 出新接缝**,R-5 不再自证收敛——**须用户裁定去重方向**(共享 `UnmortgagedThisTurn` 去重集 / 显式求值顺序),修订 PR 落地后须 fresh-session 再审独立验证,方可标 Approved——**不接受散文自述闭合、不接受同会话自证**。

### 残留 blocker 清单

- **#1(本轮唯一 BLOCKING):** F-4 unlock 赎回(L205)↔ F-5b 主动赎回(L222)未去重 → AC-64 核心场景双发 `Unmortgage(A)`,违 CR-7/L69 零跳过纪律,无 AC 证伪。**须用户裁定去重方向。**

### 待办(R-6)

- **用户裁定** 去重方向(共享 `UnmortgagedThisTurn` 去重集 vs 显式求值顺序)。
- **fresh session 修订 PR** 落地去重处方 + 新增负向 [Logic] AC(同一 `DecidePostRollActions` 返回序列同一抵押格至多一条 Unmortgage),对照 F-4 伪代码/F-5b 谓词逐字符核验(非仅改措辞)。
- **fresh session 再审** 独立验证去重集落权威伪代码层、新 AC 在「A 同时满足 F-4 联合门 + F-5b 单门」fixture 下能 FAIL 双发实现后方可标 Approved;撤回的 reviewed/approved 计数届时回补。
- followup(不阻修订,延续):AC-9 fixture Buffer_max 失配、AC-23 GIVEN 补 IsCompletionTile=false、AC-59 推导补行。
- 跨档 propagate 债延续(不变):CR-5④ VFX(19) juice 独立流;CR-6 字段 → OQ-1 ADR;OQ-AI-3b BP/C++ → 架构期;OnAIActionExecuted BlueprintAssignable → player-turn。

## Review — 2026-06-06 (R-6) — Verdict: Approved〔收敛终结轮〕— **RETREAT 落地复审**:verified_blockers=0,F-4 接缝闭合

- **模式:** full(5 specialist 隔离并行 + creative-director senior synthesis),**fresh-session 独立复审用户裁定 RETREAT/方案B 落地结果**(非同会话自证)。
- **Scope signal:** M(7 依赖、7 公式;复审聚焦 RETREAT 退回后 F-4 单门简单贪心的规格层一致性 + F-4↔F-5b 赎回路径去重终态,无新 ADR、无架构返工)。
- **Prior verdict resolved:** R-5 的 1 BLOCKING(F-4 unlock 赎回 L205 ↔ F-5b 主动赎回 L222 未去重 → AC-64 核心场景必然双发 `Unmortgage(A)`)经用户裁定 **RETREAT/方案B** 后**从根失效**——unlock 路径整体砍掉,无第二条赎回路径可双发,接缝不复存在。

### 过批纠正闭环(本系统完整审查弧,登记备案)

R-1 NEEDS REVISION(6 BLOCKING)→ R-2 NEEDS REVISION-light(4 BLOCKING,设计已收敛)→ **R-3 APPROVED_WITH_FOLLOWUPS〔过批〕**(5 finishing-class blocker 给定处方,误判可一次性 PR 收口即 Approved)→ **R-4 NEEDS REVISION/过批纠正**(2 BLOCKING:R-3 处方 #5 级联规格化自身引入 F-4 贪心守卫新接缝 → AC-64 在权威伪代码下结构性不可达;级联操作数未进变量表)→ **R-5 NEEDS REVISION/F-4 连续第二轮 peel**(1 BLOCKING:R-4 修复 unlock 可达性后,unlock 路径 ↔ 既有 F-5b 主动赎回在 AC-64 核心场景双发 `Unmortgage(A)`,违 CR-7/L69 零跳过纪律,无 AC 证伪)→ **用户裁定 RETREAT/方案B**(F-4 连续两轮 peel 新接缝=结构性不收敛信号;多步跨动作协调对 MVP 价值不抵复杂度,砍回单门简单贪心)→ **R-6 Approved**(本轮:RETREAT 落地 fresh 复审,接缝闭合,收敛归零)。

**纠正要点:** R-3 的过批根因 = 「在散文层修了 BLOCKING 但未对照公式/伪代码逐字符核验」(R-2 主审已明确告诫第三轮须验数学非改措辞,R-3 仍过批),且 R-3 处方 #5 自身是 unlock 路径的**引入轮**——后续 R-4/R-5 连续 peel 的接缝皆源于该处方。RETREAT 选择不再试图修补 unlock 多步协调(连续两轮 peel = 结构性不收敛),而是降回 MVP 简单贪心 + Alpha OQ,符合 [[gating-seam-keeps-peeling]] 与 [[converge-test-finishing-vs-identity]]:连续 peel 新接缝 ≠ finishing,触发 RETREAT 而非继续剥面。

### RETREAT 落地核实(主审独立逐条核验,非采信 claim)

- **① F-4 贪心循环 = 单门简单贪心,无 unlock 残渣。** fresh-grep 核实:解锁候选谓词 `IsUnlockCand`、赎回后首栋 BuildScore 代入 argmax、`emit Unmortgage(A)` 赎回前置路径**全删**;贪心守卫退回 `while ∃ CanBuildHouse 候选`、单一现金门 `Cash − BuildingCost(best) ≥ Buffer`。L189-208 伪代码与散文意图逐字符一致,无「散文描述 unlock 但伪代码已删」的反向 stale。
- **② 联合现金门全规格删除。** `Cash − unmortgage_cost(A) − BuildingCost(B) ≥ Buffer` 在 F-4 伪代码/散文/AC 三处均无残留引用。
- **③ AC-64(unlock-then-build 正向)/AC-65(负向)删除,AC 合计 71→69。** 计数独立重算自洽(删 2 条 Logic)。
- **④ F-4 变量表删 `unmortgage_cost(A)`(仅服务 unlock),保留 `BuildingCost`(普通建房单门仍用)。** 变量表无悬空操作数,满足 coding-standards §4。
- **⑤ OQ-AI-10 诚实标注「主动赎回-解锁-建房多步协调 deferred Alpha」已新增。** R-5 接缝降为 Alpha 范围诚实 OQ,非已知错误。
- **F-5b 主动赎回(还债/现金纪律)/F-3 IsCompletionTile 补全格买地豁免/Buffer_max/AC-11b/AC-61a-b 全保留不动**,fresh-grep 确认未被 RETREAT 误伤。

### verified_blockers(R-6)

**verified_blockers = []。** R-5 唯一 BLOCKING 经 RETREAT 从根失效(无第二条赎回路径可双发,CR-7/L69 零跳过纪律 ⇄ 核心场景双发的内部矛盾消除);5 specialist 隔离复审无新正交 BLOCKING(全归 finishing-class followup 或 deflate)。本轮 in-doc 全闭。

### 收敛证据

blocker 量逐轮:**6(R-1)→ 4(R-2)→ 5(R-3 过批轮)→ 2(R-4 纠正)→ 1(R-5)→ 0(R-6 RETREAT 落地)**。R-3 过批轮的 5 是「散文修了未验数」的二阶残渣误判,经 R-4 纠正后真实 blocker 单调收敛 2→1→0。RETREAT 砍掉连续 peel 的 F-4 unlock 子系统后触底归零,无重裁「AI 对手是什么」(系统 identity 不变:启发式、单机、主动方、无前瞻三档加权打分)。

### logged_decision(可推翻判断)

- **verdict=Approved,不退回 5-agent 全量复审。** 理由:R-5 BLOCKING 经 RETREAT 结构性失效(非修补,是删除其前提路径),F-4 退回单门简单贪心后无跨路径去重需求;RETREAT 落地经主审 fresh-grep 五项核实全真、无残渣、无反向 stale;系统 identity 不变。**可推翻:** 若后续发现 RETREAT 退回引入了新缝(如删 `unmortgage_cost(A)` 后某 AC/Example 仍引用该操作数致悬空),应升 NEEDS_REVISION;但本轮文档静态实据不支持预判该风险。
- **unlock-then-build 降 Alpha OQ-AI-10 而非永久弃案。** 多步跨动作协调(赎回-解锁-建房序列)对完整策略 AI 有价值,Alpha 阶段重启时须连同 F-4↔F-5b 去重集一并设计(R-5 处方=共享 `UnmortgagedThisTurn` 去重集 / 显式求值顺序,可作 Alpha 设计起点)。

### 待办(followup,不阻开工)

- followup(延续,不阻):AC-9 fixture Buffer_max 失配、AC-23 GIVEN 补 IsCompletionTile=false、AC-59 推导补行。
- OQ-AI-10(unlock-then-build 多步协调)→ Alpha 阶段设计(连带 F-4↔F-5b 去重集)。
- 跨档 propagate 债延续(不变):CR-5④ VFX(19) juice 独立流;CR-6 字段 → OQ-1 GameStateSnapshot ADR 注入方;OQ-AI-3b BP/C++ 裁定 → 架构期;OnAIActionExecuted BlueprintAssignable → player-turn。
