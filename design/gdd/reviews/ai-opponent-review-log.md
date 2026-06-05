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
