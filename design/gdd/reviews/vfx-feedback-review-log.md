# VFX Feedback (#19) — Design Review Log

> 游戏反馈 VFX GDD 审查历史。fresh re-review 起点须读最近条目。

## Review — 2026-06-05 — Verdict: NEEDS REVISION → 就地修订完成（R-1，pending fresh R-2）
Scope signal: M
Specialists: systems-designer, game-designer, qa-lead, technical-artist, performance-analyst, unreal-specialist, creative-director（full 模式，6 专家 + CD 综合）
Blocking items: 5 | Recommended: ~12（多数 folded）
Prior verdict resolved: First review

### 主审结构核验（fresh-grep，非接受 claim）
- owner 事件契约全部 fresh-grep 核实**真存在且签名一致**:`OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})`(dice CR-1)、`OnPawnMoveStarted(Player,From,To,Steps,PassedGo)`/`OnPawnLanded(...,EArrivalContext)`(movement 87/88)、`OnRentPaid`/`OnCashChanged(EChangeReason)`/`OnBankruptcyDeclared`(economy 117/118/120)、economy `amount==0`→不广播(AC-5/AC-37)、`OnOwnershipChanged`/`OnMortgageChanged`(property 117/118)、`OnBuildingChanged`(building CR-4)、`OnBuildingAnnounced`(player-turn **CR-3.5 + AC-53**,owner=#2,非幻象)。
- `OnBuildingChanged` **fresh-grep 揪出真矛盾**:building(8) 全文 2 字段 `(tile,newCount)`,player-turn CR-3.5/AC-53 引用 3 字段 `(tile,newCount,actingPlayerId)`;且建/拆同事件无方向(building AC-9/AC-19)。
- systems-index line 152 **已**应用 depends-on `+2,+6`(非"待补",VFX 原文 stale)。

### 5 BLOCKING（R-1 已就地修订）
1. **Issue 1 — build/sell 歧义**:`OnBuildingChanged` 建/拆同事件无方向 → VFX 卖房误播建房 juice(违 art-bible「Gold 仅奖励」)。修:CR-6 按 `EBuildDirection` 分发 + V-Own-12b 卖房 + EC-15 退化 + AC-54;**OQ-VFX-10 propagate 建房8 补方向字段(producer,owner-side 未闭)**。
2. **Issue 2 — OnBuildingAnnounced 冗余 + F-4 Critical/Social 矛盾**:VFX 双订阅(#8 OnBuildingChanged + #2 OnBuildingAnnounced),后者归 HUD;F-4 枚举建房=Social 但不变式③/EC-11 称建房通告 Critical,无对应 AC。修:**删 #19 OnBuildingAnnounced 订阅**(自洽,无需 propagate),建房=Social,critical-snap 清单=破产/胜利 only,AC-60 负向。**(self-contained 已闭)**
3. **Issue 3 — experiential AC 未承接(继承义务 BLOCKING)**:全 AC 为 Logic/Advisory-screenshot,无 felt-quality playtest 签核。修:AC-55(掷骰 feel)+ AC-56(hop 屏息 feel)[Advisory·playtest-signoff],兑现 dice OQ-T-Dice-4 / movement OQ-T-Move-3。**(已闭)**
4. **Issue 4 — N_max_vfx=12 破产级联欠配**:per-tile OnOwnershipChanged → 14~25 并发 vs 12,hop(Motion)首驱逐破坏核心循环。修:**F-4 双池(hop 独立 N_hop_max)**+ 非收租粒上界 + N_confetti_max + 级联/P99 纳入 OQ-VFX-3 标定 + AC-57。**(VFX-side 闭;标定属 Pre-Production OQ)**
5. **Issue 5 — CR-8 追赶在 Pass'N Play 瞬移 hop**:快滚使当前 hop 过期整条跳。修:当前行动者自身 hop 最短展示窗口 `T_hop_protect`(非阻塞框架)+ AC-58。**(已闭)**

### Folded（Recommended）
RB-1 epoch 链粒度(F-3 + AC-15b)/ RB-3 ConsecutiveDoubles 经 OnPhaseChanged payload + OnTurnStarted 入契约 / RB-4 棋盘1 锚点依赖显式化(CR-2/F-1/Deps,tile-center 非 mid-move GetActorLocation)/ RB-5 EndPlay 解绑(EC-5b/AC-43b)/ RB-6 AC 诚实(AC-10 短路/AC-14 ADR 门控/AC-21 升级闭环/AC-28→28b 拆/AC-40 反射/AC-47 去同义)/ RB-7 F-4 无 Motion fallback(EC-14/AC-59)/ RB-8 胜利粒上界。资产前缀(NS_→WBP_/M_)+ Substrate PP(OQ-VFX-6)flag。AC 56→66。

### 分歧裁定(CD)
- N_max Advisory 标注:systems-designer「诚实正确」vs performance「升级义务未闭 + 场景漏级联」→ **互补非矛盾**:保 Advisory 标签 + 闭 escape-clause(OQ-VFX-3 钉协议/owner/触发 + 级联场景)。已采纳。
- RNG 隔离 before Approve:unreal/tech-art 主张 → **CD 裁不 block**(MVP 单机,Full Vision 联网才硬,OQ-VFX-4 + 覆盖缺口6 标注)。

### fresh R-2 重点（self-taint 防护:勿同会话）
1. **OQ-VFX-10 / OQ-VFX-7 propagate 是否真闭**:建房8 是否已补 `EBuildDirection`(fresh-grep building-upgrade.md,非接受 VFX 自述);#2/#6/#1 depended-on-by 是否回标 #19;8↔2 字段数漂移是否消解。**不接受登记 OQ 当闭合(CD 流程约束)**。
2. **AC-55/56 是否真兑现 experiential 义务**:对照 systems-index 继承义务表 line 83 措辞,playtest-signoff 是否构成可验收 felt-quality 门(非空壳)。
3. **F-4 双池自洽**:N_hop_max 与 N_max_vfx 分离后,AC-18/19/20(原用 Motion=hop 作 fixture)是否仍自洽(hop 已移出一般池)。
4. **EC-15 退化路径**:方向字段未落地时"非方向 snap"是否真不误播庆祝(AC-54 退化分支可 FAIL)。
5. **收敛判据**:R-1 修订为 finishing-class(完成契约/对齐优先级/加保护窗口/propagate 事件方向),非 redesign;fresh 核 verdict 应为 APPROVED-with-followups 若无新正交维度。

## Review — 2026-06-05 — Verdict: NEEDS REVISION（finishing-class，收敛）→ 就地修订完成（R-2，pending fresh R-3）
Scope signal: M
Specialists: systems-designer, qa-lead, technical-artist, game-designer, creative-director（full 5-agent + CD）
Blocking items: 5（全 in-doc 闭合）| Recommended: ~10（folded）
Prior verdict resolved: Yes — R-1 5 BLOCKING 经 fresh-grep 复核

### 主审 fresh-grep 核实（非接受 VFX 自述）
- building-upgrade.md `OnBuildingChanged(tile,newCount)` = **2 字段无 EBuildDirection**（L39/78/222/260/AC-9/16/19）；player-turn CR-3.5 = 3 字段 `(tile,newCount,actingPlayerId)`；`EBuildDirection` owner 侧不存在 = **phantom contract**。OQ-VFX-10 R-1 未真闭（CD 流程约束:不接受登记 OQ 当闭合）。
- property-ownership L259：#19 据 old/new owner delta **本地派生 + 批处理** = 已 Approved 先例（technical-artist 揪出，本地派生改法的依据）。

### 5 must-land（R-2 已就地修订）
1. **EBuildDirection 跨档依赖删除 → 本地 `newCount` delta 派生**（CR-6:`house_count_cache` 呈现 memo + `GetHouseCount` seed-before-first-event 种子化;原子±1 单步保证方向唯一）。**OQ-VFX-10 RESOLVED**。CD 推翻主审入场「等 propagate」框架（删接缝比 propagate 更合 CD 流程约束，preserve-first 建房庆祝 MVP 即工作、零 building 侧改动）。
2. **AC-54 拆 54a/54b/54c**（消 phantom payload false-green:原 54 测 owner 永不发送的 `EBuildDirection=Sell`）。
3. **AC-18/19/20 fixture 改 Economic**（Motion=掷骰双池后不可达 6~12 并发=vacuous-green）+ **AC-20b** 补 Motion 驱逐覆盖。
4. **AC-15b 收窄 epoch 左支 + AC-15c 补 elapsed 右支**（F-3 OR 谓词两支均覆盖;散文软化「链内不过期」绝对化——epoch 保护仅针对 epoch 推进，elapsed catch-up backstop 独立）。
5. **F-1 `N_eff=abs(N)` 公式层前置**（防 N<0 时 clamp 静默撑下界掩盖）+ AC-05 可证伪化。

AC 66→70（50→54 Logic）。

### 未在本档闭合（非 #19 职责，producer 单独工单）
- **building(2 字段)↔player-turn CR-3.5(3 字段) `OnBuildingChanged` 字段漂移**:两个 Approved 档之间真矛盾，#19 退出该接缝（本地派生+不订阅 OnBuildingAnnounced）不闭合它。已登记 CR-6 ⚠ 注 + OQ-VFX-10 遗留项。
- **OQ-VFX-7 反向回标**:systems-index line 83 回链 AC 仍「(19)GDD 待写」（应指 AC-55/56/28-32）;#2/#6/#1 depended-on-by 补 #19。

### 分歧裁定（CD）
- **EBuildDirection**:主审入场判 propagate;ta+gd 近乎一致主张**本地派生**;CD 裁本地派生（property L259 先例 + building 原子±1 + #19 已自持呈现态三前提 fresh-grep 验证）。采纳。
- **verdict label**:gd/qa 判 NEEDS REVISION;CD 判 APPROVED-converging（finishing-class + 5 must-land）。可操作内容相同（5 机械修订零 redesign，其一净简化）。主审按 finishing-class NEEDS REVISION 走流程（must-land 落地前不标 Approved，防 false-green），认同 CD 收敛判断。

### fresh R-3 重点（self-taint 防护:勿同会话）
1. **本地派生自洽**:`house_count_cache` 是否真「非游戏状态」（呈现 memo，同 E_cur_vfx 类）;seed-before-first-event 在所有订阅时序/读档下是否成立;building 原子±1 单调假设是否被任何批量操作破坏。
2. **新 AC 可证伪**:AC-54a/b/c、AC-15c、AC-20b 是否真可 FAIL（非 vacuous）。
3. **遗留漂移正确外置**:building↔player-turn 字段漂移确非 #19 职责，未被本 verdict 误判已闭。
4. **收敛判据**:R-2 是否 finishing-class（其一净简化），无新正交维度 → R-3 应 APPROVED。
5. **Recommended 是否需纳入**:F-4 数字粒池归属、SnapDone DeactivateImmediate、级联 ownership+teardown 双事件+池预热、PP 同帧裁定、AC-55/56 强化、社交缝 OQ。

## Review — 2026-06-05 — Verdict: NEEDS REVISION（finishing-class，收敛末圈）→ 就地修订完成（R-3，pending fresh R-4）
Scope signal: M
Specialists: systems-designer, qa-lead, game-designer, performance-analyst, technical-artist, creative-director（full 5 specialist + CD,opus）
Blocking items: 5 自闭 + 2 上游 propagate | Recommended: 7（均 folded 落档）
Prior verdict resolved: Yes — R-2 5 must-land 经主审独立 fresh-grep 复核**全部成立**（本地派生 2 字段+原子±1、property L259 先例真实、OnPhaseChanged 携 ConsecutiveDoubles、5 新 AC 真可证伪、building↔player-turn 漂移正确外置）

### 主审 fresh-grep 核实（非接受专家 claim 当结论，CD 流程约束）
- **building-upgrade L75/L219 + bankruptcy L72/131/AC-8/11/18 核实**:银行破产 `LiquidateAllBuildings(debtor)` 是**独立 provisional 接口**（OQ-Build-3,bankruptcy9 Not Started）;building 仅在 `BuildHouse`/`SellHouse`（CR-4/CR-5/AC-9/16/19）文档化 `OnBuildingChanged` 广播——**`LiquidateAllBuildings` 是否逐格广播 `OnBuildingChanged` 在 owner 档未定义**。3 专家（systems/qa/perf）独立收敛此发现。
- 收租破产 in-kind:house_count 随格不变、无事件（bankruptcy AC-17）→ cache 该路径不漂移（systems 入场 in-kind 漂移 claim 自我修正,真洞在银行破产）。
- bankruptcy L191/194:逐格清算 juice **已 Approved 委托给 #19**（OnOwnershipChanged old/new 分易主/回退）,但 #19 仅 V-Own-14 覆盖 `None→PlayerId`=undischarged obligation（[[downstream-ui-misses-approved-upstream-obligation]] 模式）。

### 5 自闭 must-land（R-3 已就地修订）
1. **Finding A — 银行破产 teardown 缓存漂移 + 假覆盖**:`LiquidateAllBuildings` 若不广播 → cache 残留旧高值 → 该格回银行重买后首次建房 `newCount<stale`=**卖房 juice 误播在建房上**（EC-15 miss 兜底抓不到=cache 有条目非 miss）。修:**CR-6 在 `OnOwnershipChanged` 上 re-seed `cache=GetHouseCount`**（两移交形态逐格必发,自闭无视上游）+ EC-15b + AC-54e。
2. **AC-54d 补破产清零 h→0 大跳派生**（消 EC-15「含破产拆除」false-coverage:原仅 AC-54a 测自愿 3→2 单档）。
3. **松绑「原子±1 单步」→「delta 符号判向,步长无关」**（CR-6/seed 注;现措辞比实际强,误导未来维护以为多档跳变破坏派生）。
4. **Finding B — V-Own-14b/14c 补破产移交清算 juice**（易主清算 P→P / 回退无主 P→None,Economic 优先级;Interactions/Deps/破产9 行回标 bankruptcy L191 委托）。
5. **AC-43b false-green 修**:「spy 计数==6」数 owner **系统**数,实际 delegate 绑定 ≈**12**（每事件各绑一次）→ 漏解绑 6 的 buggy 实现误 PASS。改 12 口径,AC-43/EC-5b 同步。

### 2 上游 propagate（producer,非 #19 现能闭）
- **OQ-VFX-11**:`LiquidateAllBuildings` 广播契约接缝（一次 `(tile,0)` vs 逐档,依赖 OQ-Build-3 + bankruptcy9 Not Started）。#19 re-seed 已自闭 bug,登记待上游裁定。
- **OQ-VFX-3 校正**:级联 worst case ≈ **2N 事件**（ownership N + building N,原仅数 14 格 ownership 一半）+「最后一人破产+OnGameWon 同帧」最硬单帧入标定集 + 池预热。

### Recommended（7,本轮 folded 一并落档）
L99 `Deactivate`→`DeactivateImmediate`(R-2 folded 未落正文) / `NS_vfx_property_mortgage_wipe` 第 4 个错前缀(UMG/材质,RB tech-art R-6 漏)+ F-4 池账目 cross-ref / seed lifecycle 顺序(#8 晚于 #19 init→wrong-low seed,OQ-VFX-2) / AC-56 补量化门对齐 AC-55 / OQ-VFX-12 社交缝正式入表(非持屏者错过酒店合并庆祝) / AC-32b ConsecutiveDoubles 跨回合重置(防上家旧值污染) / OQ-VFX-6 art-director 节点 + vignette 全屏-vs-逐pawn 厘清 / V-Own-08 落地 juice 追赶妥协知情标注。AC 70→73(54→57 Logic:AC-54d/54e/32b)。

### 分歧裁定（CD）
- **APPROVED-with-followups（qa+game）vs NEEDS REVISION（systems+perf）**:CD 裁 **NEEDS REVISION 级正确**。判据非严重度/收敛性（两派都认同有界收敛),而是「**finish 已定裁决（→approve-with-followups）vs 修正正文此刻断言错误的 spec（→needs-revision）**」——Finding A（V-Own-12b/EC-15 字面声称覆盖破产拆除却无 AC + 真实 sell-juice-on-build bug=stated-coverage 证伪,项目红线）与 Finding B（bankruptcy 已 Approved 委托但两移交形态无 V-Own=undischarged obligation）均属后者,非可 park followup。
- **非 MAJOR**:零架构/支柱/签名改动,re-seed 复用既有事件、新 V-Own 套既有表、AC-54d 镜像 54a/b/c,全机械有界。
- **tech-art 6 BLOCKING deflate**:仅 AC-43b false-green 升 must-land（红线）,DeactivateImmediate 措辞/资产前缀/lifecycle 顺序 = RECOMMENDED-class。
- **关键结构洞见(CD)**:`LiquidateAllBuildings` 广播契约真上游不可解（bankruptcy9 Not Started）,但 `OnOwnershipChanged` re-seed 完全 #19 自闭、无视上游怎么定都杀死 wrong-juice bug → 现在修、只登记真上游那一片 = needs-revision 非 blocked。

### fresh R-4 重点（self-taint 防护:勿同会话）
1. **re-seed 自洽**:`OnOwnershipChanged` re-seed `cache=GetHouseCount` 在所有移交时序（in-kind/回退/链式破产）下是否真封漂移;AC-54e 是否真可 FAIL（缺 re-seed 时 1<5 误 Sell）。
2. **新 AC 可证伪**:AC-54d（h→0 大跳）、AC-54e（re-seed）、AC-32b（跨回合重置）是否真 FAIL-able 非 vacuous。
3. **V-Own-14b/14c 优先级自洽**:Economic tier 在 F-4 双池+级联驱逐下是否与 AC-18/19/20 一致;14b/14c 是否被 EC-14 fallback 误丢。
4. **遗留正确外置**:`LiquidateAllBuildings` 广播契约（OQ-VFX-11）+ building↔player-turn 字段漂移确非 #19 职责,未被误判已闭。
5. **收敛判据**:R-3 是否 finishing-class（5 自闭机械 + 2 上游登记,blocker 质量较 R-1/R-2 降到一连贯主题）→ R-4 应 APPROVED-with-followups（OQ-VFX-11 上游债不阻下游开工,但 #19 story 标定/破产9 落地前关联门见 AC-21/OQ-VFX-3 hard gate）。

## Review — 2026-06-05 — Verdict: NEEDS REVISION（finishing-class，收敛末圈终结）→ 就地修订完成（R-4，pending fresh R-5）
Scope signal: M
Specialists: systems-designer, qa-lead, game-designer, performance-analyst, technical-artist, creative-director（full 5 specialist + CD synthesis）
Blocking items: 4 自闭 must-land | Recommended: ~10（含 1 跨档 CONCERN→producer，其余 in-doc/OQ folded）
Prior verdict resolved: Yes — R-3 5 自闭 must-land + 2 上游 propagate 经主审独立 fresh-grep 复核**全部成立**（OnOwnershipChanged re-seed 自闭、AC-54d/54e/32b 真可证伪〔32b 有例外见下〕、V-Own-14b/14c Economic tier 自洽、遗留漂移正确外置、AC-43b 12 口径）

### 主审 fresh-grep 核实（非接受专家 claim 当结论，CD 流程约束）
- **跨档契约全部 fresh-grep 复核真实**：building `OnBuildingChanged(tile,newCount)` 真 2 字段无 EBuildDirection（L39/78/222/260/AC-9/16/19）;player-turn CR-3.5 真 3 字段 `(tile,newCount,actingPlayerId)`（L163）→ 遗留漂移真实、正确外置;player-turn `OnPhaseChanged` 携 ConsecutiveDoubles（L262）、`OnTurnStarted` 双点链不重发（L197）、`OnGameWon` 归 #2;property L117/259 逐格广播 + #19 old/new 派生 + 批处理先例真实;bankruptcy L191/194 逐格清算 juice 委托经 `OnOwnershipChanged`（#19 已兑现 V-Own-14b/14c）;economy amount==0 不广播（AC-5）。
- **新接缝 fresh-grep 揪出（R-1/R-2/R-3 均未触）**：bankruptcy9（Approved）`OnPlayerBankrupt(debtor,creditor,reason)`（3 字段,**全 4 步清算编排末**广播,L75/189/AC-39）显式登记出局 juice 使能,但 #19 V-Own-11/AC-35 订经济5 `OnBankruptcyDeclared(Debtor,Creditor)`（2 字段,现金侧,economy L120/AC-36）。两事件时机不同 + reason 字段 #19 未用。**HUD16（已 Approved）V-Enable-03/AC-16 作同样选择** → 单档拦 #19 裁决不一致 → 登记 OQ-VFX-13 跨档 CONCERN 交 producer（非 #19 blocker，[[10px-floor-consistent-with-approved-sibling]] 模式）。

### 4 自闭 must-land（R-4 已就地修订）
1. **AC-32b 双重 vacuous（qa-lead）**：原 GIVEN 测「OnDiceRolled 在 OnPhaseChanged **之后**到」= reset 与否相同（OnPhaseChanged 已设 0,违 CR-3 L62 钉的危险窗口=「先于」）+ 原 `bIsDouble=false` 使庆祝恒不触发（V-Own-04 需 bIsDouble）= 双重 vacuous,`OnTurnStarted` 重置必要性无法证伪。改测乱序窗口（OnDiceRolled 先于 OnPhaseChanged + bIsDouble=true）+ 新增 AC-32c 正常时序对照。**stated-coverage 证伪红线**。
2. **EC-5b/AC-43b BeginDestroy 野指针（technical-artist）**：原列 `EndPlay`/`BeginDestroy` 等效解绑;`BeginDestroy` GC 期 owner 可能已销毁,`RemoveDynamic` 恰触本条要防的野指针。改仅 `EndPlay`。
3. **F-2 Amount<0 守卫公式层前置（systems-designer）**：镜像 R-2 F-1 `N_eff=abs(N)`;防 `log2(0)=-∞`/`log2(负)=NaN` 经 clamp 静默撑到 1 粒（违「不静默 clamp」+ AC-11）。
4. **AC-54d 补正向 Sell-spy 断言（qa-lead）**：原仅「庆祝==0」无法区分正确 Sell 与 miss-fallback snap;补「Sell 降档 spy==1」。

### Recommended（folded）
- **OQ-VFX-12 升可证伪 PP 门 + 预承诺补救**（CD deflate game-designer 升 BLOCKING 之请）/ **OQ-VFX-3 帧预算三分项**（Niagara+UMG overdraw+PP passes,同帧 PP 栈子预算 + UMG 实例软上界,performance-analyst）/ AC-14↔AC-15c 标签、AC-57b hop 自驱逐、AC-60 拆分、AC-56 GO 高亮、V-Own 锚点声明、Niagara 池材质 reset→OQ-VFX-2、FRandomStream 颗粒度→OQ-VFX-4、资产拆分 V-07/V-11-dissolve、OQ-VFX-6 暖灰 note（次轮或随架构 ADR）。AC 73→74（57→58 Logic：AC-32c）。

### 分歧裁定（CD）
- **game-designer 2 BLOCKING 均 deflate 至 Recommended**：① V-Own-08 当前行动者落地 juice 无保护 = R-3 已知情妥协、最弱支柱②纽带、零正确性影响;② OQ-VFX-12 酒店合并热座过期 = R-3 故意 defer、banner 补信息层、intensity-peak 折损须 playtest **测量**非 doc-review 预判,且强加 CR-8 机制会模糊 R-1 立的 Critical/Social tier。均 re-litigate R-3 决定无新信息 → 不 overturn,但 OQ-VFX-12 升可证伪 PP 门 + 预承诺补救（honoring 正确 instinct）。
- **NEEDS REVISION 非 APPROVED-with-followups**：无新正交维度（R-3 预测成立）,但 vacuous AC（AC-32b）+ crash-endorsing 子句（BeginDestroy）是正文此刻 spec 缺陷 = R-1/R-3 修法第二代残渣（散文/机制落了,AC GIVEN 与一子句没扫到）→ 按 stated-coverage 红线不可作 followup 出货。零架构/支柱/签名改动。

### 跨档 propagate 债（producer,非 #19 现能闭）
- **OQ-VFX-13（R-4 新增）**：`OnPlayerBankrupt`(破产9) vs `OnBankruptcyDeclared`(经济5) 接缝,across bankruptcy9/economy5/HUD16/VFX19;producer 核实两事件时序一致性 + 裁定 V-Own-11 是否改订 `OnPlayerBankrupt`（语义更准 + 得 reason）。**注:HUD16 已 Approved 作同样选择,reconcile 须同步评估 HUD AC-16。**
- 续 R-3：OQ-VFX-11（`LiquidateAllBuildings` 广播契约）+ building↔player-turn `OnBuildingChanged` 字段漂移仍 open。

### fresh R-5 重点（self-taint 防护:勿同会话）
1. **4 must-land 真落地**：AC-32b 乱序窗口 + bIsDouble=true 是否真可证伪 `OnTurnStarted` 重置;AC-32c 对照非冗余;BeginDestroy 两处真删;F-2 守卫前置;AC-54d Sell spy==1 正向断言。
2. **AC-32b/32c 与 EC-9 交互**：乱序窗口下 #19 读 reset 后 0 vs EC-9 降级基础 juice,两路径是否一致（均 spy==0）。
3. **OQ-VFX-13 正确外置**：未被误判为 #19 已闭;HUD16 同选择的一致性裁决。
4. **收敛判据**：R-4 是否 finishing-class 末圈终结（4 机械自闭,零跨档自闭项,blocker 全为 AC GIVEN/lifecycle 子句精修）→ R-5 应 APPROVED-with-followups（OQ-VFX-11/13 上游债不阻开工,标定 hard gate 见 AC-21/OQ-VFX-3）。

## Review — 2026-06-06 — Verdict: NEEDS REVISION（finishing-class，收敛末圈终结）→ 就地修订完成（R-5，pending fresh R-6）
Scope signal: M
Specialists: qa-lead, systems-designer, game-designer, performance-analyst, technical-artist, creative-director（full 5 specialist + CD synthesis）
Blocking items: 1 真 BLOCKING（M1，长潜矛盾非残渣）+ 1 honesty softening（M2）| Recommended: 7（均 folded 落档）
Prior verdict resolved: Yes — R-4 4 must-land 经主审独立 fresh-grep 复核**全部成立**（AC-32b 乱序窗口可证伪、AC-32c 对照、BeginDestroy 两处真删仅 EndPlay、F-2 守卫前置、AC-54d Sell spy==1）

### 主审独立核实（CD 流程约束:逐条核验专家 claim 对文，不接受 claim 当结论）
- **R-5 与前轮关键区别**：R-5 专家提出**大量** BLOCKING claim（systems 6、perf 4、qa 2-3、game 2），主审逐条对文核实后**仅 1 条成立为真 BLOCKING**，其余 deflate（见下）——验证「再审收敛质量=批准信号」+「不把 claim 当结论」两红线。
- **M1 真 BLOCKING（game-designer + technical-artist 独立揪出 + 主审对文核实）**：CR-2（L68）列 `OnRentPaid` 格事件锚点须 tile-center + **禁 pawn `GetActorLocation()`**;V-Own-09（L308）要金币飞向 **Payee 棋子**;`OnRentPaid(Payer,Payee,Amount,TileIndex)` 的 `TileIndex`=**地产格非 Payee 所在格**、payload 不携 Payee 格位 → **Payee 终点当前规格不可解**。R-1..R-4 ~20 specialist passes 均漏（每轮盯 build/sell/teardown 缝,无人核「Payee 世界坐标从哪来」）= **长潜矛盾非任何修法残渣**。

### 1 真 BLOCKING + 1 honesty（R-5 已就地修订，用户裁定方案 B）
1. **M1 — CR-2 ↔ V-Own-09 锚点矛盾**：用户裁定**方案 B（CR-2 scoped 命名例外）**——Payee 非当前行动者+静止态,CR-2 禁令危害域=mid-hop 自读插值不适用（literal-prohibition-vs-legislative-intent）;零跨档 + 保支柱②飞弧。落 CR-2 命名例外（仅 Payee 终点+仅 OnRentPaid+mid-hop 退化 tile-center）+ V-Own-09 锚点澄清 + EC-16 + **AC-51b [Logic]**（命名例外解析 + mid-hop `GetActorLocation` 调用==0 spy 守边界，防例外沦后门）。
2. **M2 — honesty softening（systems-designer）**：CR-6/EC-15b/遗留2「re-seed 自闭、无视上游」过度承诺 → 软化为「封广播契约层、但执行序（清零先于/同步 ownership 广播,OQ-VFX-11 新子项）+ #8 readiness（OQ-VFX-2 新子项）是隐含前提」。

### Recommended（7，本轮 folded 一并落档）
R3 V-Own-14b/14c 级联可驱逐=知情妥协注（EC-14,systems+perf 双确认零正确性影响,#17 持久态兜底）+ 全 Critical fallback snap / R4 OQ-VFX-3 三补（链式破产〔in-kind 不计 cash 腿,bankruptcy AC-17〕+ game-thread CPU vs GPU 分列〔防 P99 总门掩盖 CPU hitch〕+ `N_max_umg` UMG 软上界 + PP 子预算 floor）+ Tuning 加 `N_max_umg` / R5 AC-55/56 命中判据（开放式提问+词义裁定+AC-56 补落地踏实感盲区）+ CR-5 `EChangeReason` 分类表 + EC-17 未知 reason 中性兜底 / R6 AC-43b 枚举随 OQ-VFX-13+spy 验逐 handler 名 / AC-35/36 拆 spy 路由+材质参数 vs 渲染视觉 / F-2 `==0` 短路合并入守卫 / OQ-VFX-14 险过登记。AC 74→75（59 Logic:AC-51b）。

### 分歧裁定（CD）+ deflate 记录（主审独立核实驳回的 over-claim）
- **CD 裁 NEEDS REVISION 非 APPROVED-with-followups**：M1 是**正文此刻字面矛盾的 spec**（V-Own-09 不可实现）→ stated-coverage 红线,不可作 followup 出货,即便有界。**非 MAJOR**:零架构/支柱/签名改动,M1 收敛为一处命名例外。
- **deflate（主审对文驳回）**：① perf「3N/4N 级联」= in-kind 不逐格广播 `OnCashChanged`（bankruptcy AC-17 house_count 随格不变无事件）→ undercount A 不成立;② perf 1b/3/6（链式/UMG cap/CPU split）= 已在 OQ-VFX-3 Pre-Production 标定范畴（agent 自陈「no new systems, no AC false-greens」）→ folded 非 BLOCKING;③ systems F2-A（Amount==0）= L168 cross-ref + AC-10 已覆盖短路;④ systems F3-B（elapsed 措辞）/ F4-A（全 Critical 不可达边界）= 同 Approved HUD 姊妹档构造 / 实践不可达;⑤ qa AC-43b↔OQ-VFX-13 + AC-35/36 标签 = 计数仍 12 + HUD 已 Approved 同 render-AC 模式（[[10px-floor-consistent-with-approved-sibling]]）;⑥ game-designer B-2 渲染时序求 [Logic] AC = 违 headless 陷阱原则 + 同帧已在 OQ-VFX-6。
- **game-designer 两 deflated 项（V-Own-08/OQ-VFX-12）**：本轮**未 re-raise 为 BLOCKING**（仅作 AC-56 落地踏实感盲区的新角度,尊重 R-4 CD 裁定）。
- **收敛证据（CD）**：blocker 量逐轮单调降 **5→5→5→4→2**,严重度降至「一长潜矛盾 + 一措辞软化」;M1 是**未触接缝的 coverage find**非任何修法残渣,无 re-adjudication「#19 是什么」→ 健康收敛触底（非「接缝连续剥面」非收敛）。**终结轮**。

### fresh R-6 重点（self-taint 防护:勿同会话）
1. **M1 命名例外是 scoped 不是后门**：CR-2 命名例外是否严格限 Payee 终点+OnRentPaid+mid-hop 退化（非 blanket 重开 CR-2 禁令）;AC-51b mid-hop `GetActorLocation` 调用==0 spy 是否真可 FAIL（守边界,防 consumer-latch escape-hatch）。
2. **M1 解析机制可实现性**：#19 经 Payee PlayerId 查 pawn registry 是否有源（架构期 owner pawn registry / movement4 提供?）—若无源则 AC-51b 不可兑现 = 须登记 OQ 或 propagate。
3. **M2 软化一致性**：CR-6/EC-15b/遗留2 三处「无视上游」是否均软化（line 25 R-3 历史 header 保留为历史日志,非现行 spec）;OQ-VFX-11 执行序 + OQ-VFX-2 #8 readiness 子项是否真登记。
4. **folded 真落地**：EC-14 知情妥协注 / OQ-VFX-3 三补 / EC-17 + CR-5 reason 表 / AC-55/56 命中判据 / F-2 `==0` 合并 是否散文+AC 一致非仅措辞。
5. **收敛判据**：R-5 是否真终结轮（1 长潜矛盾闭合 + honesty + folded,无新正交维度）→ R-6 应 APPROVED-with-followups（遗留 OQ-VFX-11/13 上游债 + M1 解析机制可能新增 OQ,均不阻开工;标定 hard gate 见 AC-21/OQ-VFX-3）。

## Review — 2026-06-06 — Verdict: NEEDS REVISION（finishing-class 终结轮,本轮 in-doc 全闭）→ 就地修订完成（R-6,pending fresh R-7）
Scope signal: M
Specialists: game-designer, systems-designer, qa-lead, technical-artist, performance-analyst, creative-director（full 5 specialist + CD synthesis）
Blocking items: 2 must-land（均 R-5 修法引入的实现约束未落地,非重裁系统）| Recommended: 4（均 folded 落档）
Prior verdict resolved: Yes — R-5 1 真 BLOCKING（M1 命名例外）+ M2 honesty + 7 folded 经主审 fresh-grep 复核**全部成立**（CR-2 命名例外/V-Own-09/EC-16/AC-51b 真落地、M2 三处软化真到位、OQ-VFX-11/2 子项真登记、公式 F-1~F-4 全边界无退化、75 AC 计数一致、三项继承义务全承接）

### 主审独立核实（CD 流程约束:逐条对文核验专家 claim,不接受 claim 当结论）
- **R-6 与 R-5 同构的 claim-wave 验证**：专家提多条 BLOCKING（systems 1、qa 1、tech-art 1、perf 1，game 0），主审对文 fresh-grep 核实后 **2 条成立为真 BLOCKING**（其中 1 条三专家收敛），1 条主审收窄 over-claim，game-designer 直接判收敛。
- **BLOCKING-1 真（systems+qa+technical-artist 三专家独立收敛 + 主审 fresh-grep）**：R-5 M1 命名例外引入"经 Payee PlayerId 查 pawn registry 取 `GetActorLocation()`"（CR-2 L75/EC-16 L244/V-Own-09 L319/AC-51b L487 四处）;主审 grep 证实 `pawn registry` 全文仅这 4 处 #19 内部引用、**无 OQ、Dependencies 无该查询接口、movement.md 无 pawn 坐标查询**。AC-51b 是 [Logic] BLOCKING 却断言 spy 一个 owner 未定义的接口 = **phantom-spy 风险**（[[rereview-fix-names-phantom-contract]] 模式,stated-coverage 红线）。**R-5 review-log L167 已显式预警**此为 R-6 焦点 #2「若无源则 AC-51b 不可兑现=须登记 OQ 或 propagate」→ R-5 把验证 defer 到 R-6,本轮证实未登记 = R-5 计划内遗留,非失控残渣。
- **BLOCKING-2（performance-analyst,主审收窄）**：`N_active_umg` 全文 0 命中,N_max_umg 仅存 Tuning（L297）/OQ-VFX-3（L527 溢出已定义"最旧 UMG snap"）。**主审 deflate over-claim**：perf 称"设计上无界"偏重——溢出行为已定义;真缺口窄 = F-4 正文无 UMG 计数门 + 升级闭环（AC-21/覆盖缺口3）漏枚举 `N_active_umg`。

### 2 must-land（R-6 已就地修订）
1. **BLOCKING-1 — pawn registry 来源未登记**：新增 **OQ-VFX-15**（registry owner 候选 AGameState 内置 PlayerArray / movement4 `GetPawnForPlayer` / board-manager,架构期裁定,门控 AC-51b/51c）+ **AC-51b 标 `[Logic·门控 OQ-VFX-15]`**（镜像 AC-14/15c `IGameClock` DI 门控先例,源选定前 spy 注入边界抽象不早写死）+ Dependencies 新增 pawn registry 硬依赖行（来源待裁）+ CR-2/V-Own-09/EC-16 回指 OQ-VFX-15。**零跨档**（registry owner 归架构期,非现可闭 propagate）。
2. **BLOCKING-2 — N_max_umg 机制**：F-4 补 `N_active_umg ≤ N_max_umg` 软上界段（溢出 snap 最旧非 Critical UMG + Critical vignette 溢出先 snap 最终态 EC-11）+ **EC-18** + **AC-61 [Logic]**（UMG 溢出 snap + Niagara 双池不受挤占,断言 UMG 误计入 N_max_vfx 则 FAIL）+ AC-21 升级闭环 & 覆盖缺口3 补 `N_active_umg≤N_max_umg` 不变式枚举。

### Recommended（4,本轮 folded 一并落档）
- **AC-51c [Logic·门控 OQ-VFX-15]**（EC-16 第三退化:Payee pawn registry-null → 起点附近爆发,不崩溃,补三退化路径全覆盖,game-designer）/ **V-Own-01 期待振动触发源**澄清（"点击掷骰后"须经 owner 意图事件 `OnDiceRollStarted`/`OnPhaseChanged(RollPhase)` 进入,绝不监听 UI 层,守 CR-1;dice Interactions 仅列 `OnDiceRolled`,意图事件来源待架构同 OQ-VFX-1 模式,game-designer）/ **F-1 N 实践上界=棋盘总格数**（禁硬编码）+ **F-4 旋钮硬下界 `ensure(>=1)`**（防驱逐逻辑无元素退化,systems）/ **OQ-VFX-6 补显式 Substrate PP 知识盲区声明**（5.7 Substrate Surface 层改动,PP `Blendable` 是否受影响=LLM ~5.3 盲区,须对照 5.7 release notes;逐 pawn 去饱和材质参数不受影响,tech-art）。AC 75→77（61 Logic:AC-51c/AC-61）。

### 分歧裁定（CD）+ deflate 记录
- **CD 裁 NEEDS REVISION（finishing-class 终结轮,本轮 in-doc 全闭）非 APPROVED 非 MAJOR**：BLOCKING-1 命中 stated-coverage 红线（[Logic] AC 断言可兑现但前提 owner 未定义）→ 不可 park,但闭法是 doc 内已 Approved 的 IGameClock 门控先例、零跨档零架构 → finishing-class。BLOCKING-2 主审收窄为"已定裁决传播未完成"→ 降 folded must-land,不独立升级 verdict。
- **R-5 预测的"R-6 应直接 APPROVED-with-followups"略乐观**：它假设 pawn registry park 即可,但 stated-coverage 红线要求 in-doc 落门控（OQ-VFX-15 + AC 门控标签）才能闭;修完（7 项机械落地）后 **R-7 fresh 才是真 APPROVED-with-followups**。
- **deflate（主审对文驳回/收窄）**：① perf「N_max_umg 设计上无界」= OQ-VFX-3 已定义溢出"最旧 UMG snap",收窄为"F-4 正文无机制 + 升级闭环漏枚举"窄缺口;② game-designer 多项已裁定项（V-Own-08 落地 juice/OQ-VFX-12 酒店热座）本轮未 re-raise（尊重 R-3/R-4 CD 裁定）;③ tech-art SnapDone/资产前缀/delegate 12 解绑/暖金 blend 均核实**收敛**,仅 pawn registry 升 BLOCKING。
- **收敛证据（CD）**：blocker 量逐轮 **5→5→5→4→2→2**,本轮 2 条全是"R-5 修法实现约束未落地"性质（pawn registry 接口源 + UMG 池机制）,无任何"重裁 #19 是什么";BLOCKING-1 是 R-5 计划内 defer 到 R-6 的遗留按计划处理 → 健康收敛终结轮。

### fresh R-7 重点（self-taint 防护:勿同会话）
1. **OQ-VFX-15 是真门控不是空壳**：AC-51b/51c 标 `[Logic·门控 OQ-VFX-15]` 是否与 AC-14/15c IGameClock 门控同构（接口存在前不计 headless 硬门）;OQ-VFX-15 三候选源是否覆盖（AGameState 内置=零跨档 vs movement4=须 propagate）;Dependencies pawn registry 行是否正确标"来源待裁"。
2. **AC-61 + EC-18 + F-4 UMG 段自洽**：N_active_umg 独立计数是否真不挤占 Niagara 双池;AC-61 "UMG 误计入 N_max_vfx 则 FAIL" 断言是否可证伪;Critical vignette 溢出先 snap 是否与 EC-11 一致。
3. **AC-51c 可证伪**：registry-null 注入 → 起点爆发 spy==1 是否真 FAIL-able（非 vacuous）。
4. **升级闭环三池齐全**：AC-21 + 覆盖缺口3 是否均补 `N_active_umg`,与 N_active_general/N_active_hop 并列。
5. **收敛判据**：R-6 是否真终结轮（2 条 R-5 修法实现约束落地,无新正交维度）→ R-7 应 APPROVED-with-followups（遗留 OQ-VFX-11/13/15 上游/架构债不阻开工;标定 hard gate 见 AC-21/OQ-VFX-3）。

## Review — 2026-06-06 — Verdict: NEEDS REVISION（finishing-class 终结轮,本轮 in-doc 全闭）→ 就地修订完成（R-7,pending fresh R-8）
Scope signal: M
Specialists: systems-designer, qa-lead, game-designer, performance-analyst, technical-artist, creative-director（full 5 specialist + CD synthesis）
Blocking items: 2 must-land（均零成本 fold,非重裁系统）| Recommended: 0 独立项（2 项即 must-land）| Nice: 3
Prior verdict resolved: Yes — R-6 两 must-land（OQ-VFX-15 pawn registry 门控 + N_max_umg UMG 池机制）经主审独立 fresh-grep 复核**全部真落地无残渣**（OQ-VFX-15 三候选源 + AC-51b/51c `[Logic·门控]` 同构 IGameClock 先例、F-4 UMG 段 + EC-18 + AC-61 四处一致独立计数、AC-21 升级闭环三池齐全、movement.md 确无 pawn 坐标查询=门控诚实）

### 主审独立核实（CD 流程约束:逐条对文核验专家 claim,不接受 claim 当结论）
- **R-6 两 must-land fresh-grep 复核全成立**：`movement.md` 独立 grep 确无 `GetPawnForPlayer`/pawn 坐标查询 → 证实 OQ-VFX-15 闭法诚实（接口确实尚不存在,正确归架构期、非空壳门控）;`pawn registry`/`N_active_umg`/`OQ-VFX-15` 16 处命中 → 两 must-land 正文已落。
- **claim-wave 验证（同 R-5/R-6 模式）**：5 专家提多条,主审对文核实后 **2 条成立**——3 专家(systems/perf/game 核心)判 0 in-doc BLOCKING,qa-lead 1 BLOCKING(AC-43b),tech-art+game-designer 双独立收敛 1 项(V-Own-01)。
- **BLOCKING-1 真(qa-lead;主审对文 L485 核实)**：AC-43b THEN 仅断言「spy 计数==12 + IsBound 各 handler==false」,而「spy 须验**逐个 handler `FunctionName`**」只活在**注**里、未进权威 THEN。OQ-VFX-13 裁定 V-Own-11 改订 `OnPlayerBankrupt` 时枚举漂移在总数仍 12 下静默存活（漏解绑旧 `OnBankruptcyDeclared` 野指针）=[[gdd-fix-lands-in-comments-not-spec]] 红线（验证义务落注释不落断言）。当前固定 12 枚举下现 AC 非 vacuous（spy==12 抓得住「数 owner 系统 6」buggy 实现）,但红线要求移入断言。
- **BLOCKING-2（tech-art F7-1 + game-designer R7-01 双独立收敛;主审对文核实）**：V-Own-01 期待振动起振锚「掷骰意图事件来源」只活散文注,称「同 OQ-VFX-1 模式」但 **OQ-VFX-1 scope=hop path 不覆盖掷骰意图事件** → 无 OQ 表条目/owner/目标期 = R-6 自立 pawn-registry 范式（散文注+OQ 条目）的平行镜像缺 OQ 那一半。

### 2 must-land（R-7 已就地修订）
1. **AC-43b 逐 handler `FunctionName` 验证由注移入 THEN 权威断言**（抓①漏解绑数 owner 系统 6 + ②OQ-VFX-13 改订时枚举漂移两类 false-green;OQ-VFX-13 裁定改订须同步替换枚举列表该项名）。零架构。
2. **新增 OQ-VFX-16**（掷骰意图事件来源,owner=骰子3/回合2/技术总监,架构期随 OQ-VFX-1;MVP fallback `OnPhaseChanged(RollPhase)` 已具名）+ V-Own-01 散文注回指。**不**升 [Logic] AC（渲染层锚点,避 headless 陷阱）。零跨档。AC 计数不变（77/61 Logic;OQ-VFX-16 是 OQ 非 AC）。

### Nice-to-Have（不阻,Pre-Production 提醒）
- OQ-VFX-12 阈值 X 待 ux-designer 填（进 Pre-Production playtest 前）/ OQ-VFX-13 producer 裁定纳入破产高潮时序权重 / OQ-VFX-6 art-director 暖金↔去饱和同帧主导决策节点 / reduce_motion=On 最低情绪底线分级（OQ-VFX-5）。

### 分歧裁定（CD）
- **4 专家 APPROVED-with-followups vs qa-lead 1 BLOCKING**：CD 裁表面分裂、**可操作内容一致**（全归 2 处零成本 fold,无人要求重裁「#19 是什么」）→ finishing-class 终结轮签名。取 must-land 落地前不标 Approved（防 false-green,前 6 轮一贯）,但确认非 redesign。
- **不同意 R-6「R-7 应直接 APPROVED-with-followups」预测**（与 R-6 对 R-5 预测的修正同构 L192）：2 处 land 后 **R-8 fresh 才是真 APPROVED-with-followups 终结**。
- **非 MAJOR**：零架构/零支柱/零签名,2 处零成本机械 fold;AC-43b 挂已登记 OQ-VFX-13 接缝、V-Own-01 是 R-6 第二代 tracking 残留 = 收敛非剥面。

### fresh R-8 重点（self-taint 防护:勿同会话）
1. **AC-43b THEN 逐 handler 名断言真可证伪**：枚举漂移场景（OQ-VFX-13 改订 1 项)下,纯计数==12 通过但逐 handler 名缺失 FAIL 是否真可注入区分（非 vacuous）。
2. **OQ-VFX-16 是真追踪锚**：owner/目标期/MVP fallback 齐;V-Own-01 回指正确,未误并入 OQ-VFX-1 scope。
3. **收敛判据**：R-7 是否真终结轮（2 零成本 fold 落地,无新正交维度）→ R-8 应 APPROVED-with-followups（遗留 OQ-VFX-11/13/15/16 架构/上游债不阻开工;标定 hard gate 见 AC-21/OQ-VFX-3）。blocker 量 5→5→5→4→2→2→2,严重度持续在「注释未进断言/tracking 残留」层。
