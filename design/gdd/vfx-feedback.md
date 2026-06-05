# 游戏反馈 VFX（Game Feedback VFX）

> **Status**: In Design
> **Author**: msc + design-system agents
> **Last Updated**: 2026-06-06
> **Implements Pillar**: MDA Sensation（priority-4，juice 获得感）+ ① 桌游忠实（物理感反馈）+ ② 社交博弈（收租/破产/建房的可见戏剧性）

> **R-1 修订(2026-06-05 fresh-session design-review,full 6 specialist + CD)**:5 BLOCKING 闭合 —
> ① **建房 vs 卖房方向**(Issue 1):`OnBuildingChanged` 建/拆同事件无方向 → VFX 卖房误播建房 juice;**propagate 建房8 补方向字段**(OQ-VFX-10),VFX CR-6 分发建/拆,新增 AC-54 负向。**〔R-2 SUPERSEDED:`EBuildDirection` propagate 改为 #19 本地 `newCount` delta 派生,OQ-VFX-10 RESOLVED,见下 R-2 修订 ①——本条为历史记录〕**
> ② **OnBuildingAnnounced 冗余 + F-4 Critical/Social 矛盾**(Issue 2):**删 #19 对 `OnBuildingAnnounced` 订阅**(HUD 拥全员通告 banner),建房 juice 仅经 `OnBuildingChanged`(#8)=**Social**;从 critical-snap 清单移除「建房通告」(critical = 破产/胜利 only)。
> ③ **继承义务 experiential AC 未承接**(Issue 3):新增 AC-55(掷骰三阶段 feel)/AC-56(hop 逐格屏息 feel)`[Advisory·playtest-signoff]`,兑现 systems-index 继承义务表 dice OQ-T-Dice-4 / movement OQ-T-Move-3。
> ④ **N_max_vfx=12 破产级联欠配**(Issue 4):**hop 拆独立 `N_hop_max` 预算**(防级联驱逐饿死核心循环 hop)+ 非收租 juice 补粒上界 + 破产级联纳入标定场景;新增 AC-57。
> ⑤ **CR-8 追赶在 Pass'N Play 瞬移 hop**(Issue 5):当前行动者**自身 hop 保最短展示窗口**(至少压缩 min 形态,绝不瞬移),框架仍非阻塞;新增 AC-58。
> folded(Recommended):RB-1 epoch 链粒度文档化 / RB-3 `ConsecutiveDoubles` 经 `OnPhaseChanged` payload + `OnTurnStarted` 入契约 / RB-4 棋盘1 锚点依赖显式化(tile-center 非 mid-move `GetActorLocation`) / RB-5 EndPlay 解绑 AC-43b / RB-6 AC 诚实精化(AC-10/14/28/40/47/21) / RB-7 F-4 无 Motion 可驱逐 fallback(EC-14) / RB-8 胜利粒上界 `N_confetti_max`。AC 56→66。

> **R-2 修订(2026-06-05 fresh-session design-review,full 5 specialist + CD,独立验证)= NEEDS REVISION(finishing-class,收敛)→ 就地修订完成**:5 must-land BLOCKING 闭合 —
> ① **EBuildDirection 跨档依赖删除 → 本地 `newCount` delta 派生**(CR-6;镜像 property-ownership L259 owner-delta 先例 + building 原子 ±1 单步保证):#19 自持 per-tile `house_count_cache`(呈现 memo,seed-before-first-event 用 `GetHouseCount` 种子化),`newCount>cache`=Build / `<`=Sell。**OQ-VFX-10 RESOLVED**(删接缝比 propagate 更合 CD 流程约束,preserve-first 建房庆祝 MVP 即工作、零 building 侧改动);**遗留**:building(2)↔player-turn(3)`OnBuildingChanged` 字段漂移仍须 producer 单独工单(非 #19 职责)。
> ② **AC-54 拆 54a/54b/54c**:消除 phantom-contract false-green(原 54 测 owner 永不发送的 `EBuildDirection=Sell` payload)。54a 测 owner 真实 2 字段卖房派生、54b 测缓存 miss 退化、54c 测正向建房派生。
> ③ **AC-18/19/20 fixture 改 Economic**(原用 Motion=掷骰,双池后不可达 6~12 并发=vacuous-green)+ **AC-20b 补 Motion 驱逐覆盖**。
> ④ **AC-15b 收窄至 epoch 左支 + AC-15c 补 elapsed 右支覆盖**(F-3 `OR` 谓词两支均测;F-3 散文软化「链内不过期」绝对化——epoch 保护仅针对 epoch 推进,elapsed catch-up backstop 独立)。
> ⑤ **F-1 `N_eff=abs(N)` 公式层前置**(防 N<0 时 `clamp(负值)` 静默撑下界掩盖)+ AC-05 可证伪化。
> folded(Recommended):F-4 数字粒 UMG/Niagara 池归属厘清 / SnapDone ADR 须 `DeactivateImmediate` 非裸 `Deactivate` / 级联标定纳入 ownership+teardown 双事件 + 池预热 / 胜利暖金 PP↔破产去饱和 PP 同帧裁定(art-director) / AC-55 判据收紧 + AC-56 补计数门 / 社交缝 OQ(非持屏者看不到对手建房庆祝) / OQ-VFX-7 回标。AC 66→70(50→54 Logic)。

> **R-3 修订(2026-06-05 fresh-session design-review,full 5 specialist + CD,独立 fresh-grep 验证)= NEEDS REVISION(finishing-class,收敛末圈)→ 就地修订完成**:R-3 揪出**新正交维度 = #19↔破产/建房 teardown 事件完整性接缝**(R-1 审 build/sell 方向、R-2 审 phantom contract,均未触此)。3 专家独立收敛 + 主审 fresh-grep building/bankruptcy 核实。5 自闭 must-land 闭合 —
> ① **银行破产 teardown 缓存漂移 + 假覆盖(Finding A)**:`LiquidateAllBuildings`(银行破产逐格清零,building L75/L219 provisional/OQ-Build-3)**是否广播 `OnBuildingChanged` 在 owner 档未定义**;若不广播 → `house_count_cache` 残留旧高值 → 该格回银行重买后首次建房派生 `newCount<stale`=**卖房 juice 误播在建房上**(EC-15/AC-54b 抓不到=cache 有条目非 miss)。修:**CR-6 在 `OnOwnershipChanged` 上 re-seed `cache=GetHouseCount`**(两种移交形态逐格必发,bankruptcy L191)=**自闭、无视上游怎么定都杀死 bug**;EC-15 扩 h→0 + ownership 失效。
> ② **AC-54d 补破产清零 h→0 大跳派生**(消 EC-15「含破产拆除」false-coverage:原 AC 只测自愿单档卖房 3→2)。
> ③ **松绑「原子±1 单步」→「delta 符号判向,步长无关(含 h→0 大跳仍正确派生 Sell)」**(现措辞比实际强,误导未来维护)。
> ④ **V-Own-14b/14c 补破产移交 juice(Finding B)**:bankruptcy L191/194 **已 Approved** 委托逐格清算 juice 给 #19(OnOwnershipChanged old/new 分「易主清算 P→P / 回退无主 P→None」),但 #19 唯一 handler V-Own-14 仅 `None→PlayerId`(买地);两移交形态**无 V-Own = 未兑现 Approved 上游义务** + F-4 级联池压力模型假设其存在。补 V-Own-14b/14c(Economic 优先级)。
> ⑤ **修 AC-43b false-green**:「spy 计数==6」数的是 owner **系统**数,实际 delegate 绑定 ≈12(每事件各绑一次:dice1+回合3+移动2+经济3+地产2+建房1)→ 正确实现解绑 12 被 `==6` 判 FAIL。改「12 个 delegate 绑定」口径。
> **上游 propagate(2,producer,非 #19 现能闭)**:OQ-VFX-11 登记 `LiquidateAllBuildings` 广播契约接缝(依赖 OQ-Build-3 + bankruptcy9 Not Started);OQ-VFX-3 校正级联 worst case ≈ **2N 事件**(ownership N + building N)+ 「最后一人破产 + OnGameWon 同帧」入标定集。
> folded(Recommended,本轮一并捎带):L99 `Deactivate`→`DeactivateImmediate` / `NS_vfx_property_mortgage_wipe` 第 4 个错前缀(UMG/材质非 Niagara) / seed lifecycle 顺序(#8 晚于 #19 init→wrong-low seed,OQ-VFX-2 范畴) / AC-56 补量化门对齐 AC-55 / 社交缝正式入 OQ 表(OQ-VFX-12) / ConsecutiveDoubles 跨回合旧缓存(OnTurnStarted 重置 + AC-32b) / OQ-VFX-6 补 art-director 节点 + vignette 全屏-vs-逐actor 厘清。AC 70→73(54→57 Logic:AC-54d/54e/32b)。

> **R-4 修订(2026-06-05 fresh-session design-review,full 5 specialist + CD,独立 fresh-grep 验证)= NEEDS REVISION(finishing-class,收敛末圈终结)→ 就地修订完成**:**无新正交维度**(R-3 预测成立);4 must-land 为 R-1/R-3 修法的第二代残渣(散文/机制落了,但 AC 的 GIVEN 与一条 lifecycle 子句没扫到),按 stated-coverage 红线不可作 followup 出货 —
> ① **AC-32b 非空化**:原 GIVEN 测「`OnDiceRolled` 在 `OnPhaseChanged` **之后**到」= reset 与否结果相同(`OnPhaseChanged` 已设 0)+ 原 `bIsDouble=false` 使庆祝恒不触发 = **双重 vacuous**;改测 CR-3 L62 钉的乱序危险窗口(`OnDiceRolled` **先于** `OnPhaseChanged` + `bIsDouble=true`),新增 AC-32c 守正常时序对照。
> ② **EC-5b/AC-43b `BeginDestroy`→仅 `EndPlay`**:原把 `BeginDestroy` 列为等效解绑点,但 GC 期 owner 可能已销毁,`RemoveDynamic` 恰触本条要防的野指针;`EndPlay` 是唯一安全点。
> ③ **F-2 `Amount<0` 守卫公式层前置**(镜像 R-2 F-1 `N_eff=abs(N)`):防 `Amount=-1` 时 `log2(0)=-∞`/`Amount<-1` 时 `log2(负)=NaN` 经 clamp 静默撑到 1 粒,违「不静默 clamp」+ AC-11。
> ④ **AC-54d 补正向 Sell-spy 断言**:原仅「庆祝==0」无法区分正确 Sell 与 miss-fallback snap(后者也满足庆祝==0);补「Sell 降档 spy==1」。
> folded(in-doc Recommended):**OQ-VFX-12 升为可证伪 Pre-Production 门 + 预承诺补救**(CD deflate game-designer 升 BLOCKING 之请:re-litigate R-3 + banner 补信息层 + intensity-peak 须 playtest 测量非 doc 预判)/ **OQ-VFX-3 帧预算三分项度量**(Niagara + UMG overdraw + PP passes,同帧 PP 栈子预算 + UMG 实例软上界,performance-analyst)。**跨档 CONCERN(producer,非 #19 单档可闭)= 新增 OQ-VFX-13**:`OnPlayerBankrupt`(破产9,3 字段全编排末)vs `OnBankruptcyDeclared`(经济5,2 字段现金侧)接缝——#19 V-Own-11 + **已 Approved 的 HUD16** 作同样选择(订经济5),单档拦 #19 与姊妹档裁决不一致;逐格清算委托(V-Own-14b/14c 经 property6)#19 已兑现,此缝仅关 V-Own-11 触发源选择。AC 73→74(57→58 Logic:AC-32c)。
> **CD deflate 记录(game-designer 2 BLOCKING 均降 Recommended)**:V-Own-08 当前行动者落地 juice 无保护(R-3 已知情妥协,最弱支柱②纽带,零正确性影响)/ OQ-VFX-12 酒店合并热座过期(R-3 故意 defer,banner 补位,升可证伪 PP 门)。均 re-litigate R-3 决定、无新信息。

> **R-5 修订(2026-06-06 fresh-session design-review,full 5 specialist + CD,独立 fresh-grep + 主审逐条核实专家 claim)= NEEDS REVISION(finishing-class,收敛末圈终结)→ 就地修订完成**:**1 真 BLOCKING(长潜矛盾,非前轮修法残渣)+ 1 honesty softening + 7 folded** —
> **M1(CR-2 ↔ V-Own-09 收租金币锚点矛盾,game-designer + technical-artist 独立揪出、主审对文核实)**:CR-2 列 `OnRentPaid` 为格事件锚点须 tile-center + **禁取 pawn `GetActorLocation()`**;但 V-Own-09 要金币飞向 **Payee 棋子**,而 `OnRentPaid(Payer,Payee,Amount,TileIndex)` 的 `TileIndex` 是**地产格非 Payee 所在格**、payload 不携 Payee 格位 → Payee 终点**当前规格不可解**(R-1..R-4 ~20 specialist passes 均漏,长潜矛盾非残渣)。**用户裁定方案 B**:CR-2 为收租 Payee 终点开**scoped 命名例外**(Payee 非当前行动者+静止态,CR-2 禁令危害域=mid-hop 自读插值,不适用;literal-prohibition-vs-legislative-intent),零跨档改动 + 保支柱②社交飞弧。落 CR-2 命名例外 + V-Own-09 锚点澄清 + EC-16 mid-hop 退化 + AC-51b [Logic](命名例外解析 + mid-hop spy 守边界,防例外沦后门)。
> **M2(honesty softening,systems-designer)**:CR-6/EC-15b/AC-54e「re-seed 自闭、无视上游 `LiquidateAllBuildings` 广播契约如何裁定」**过度承诺**——re-seed 正确性仍依赖 ① 清零执行序先于/同步于 ownership 广播(OQ-VFX-11 新子项)② #8 readiness(OQ-VFX-2 新子项);软化「无视上游」为「封广播契约层、但执行序+readiness 是隐含前提」。
> folded(Recommended,本轮一并捎带):**R3** V-Own-14b/14c 级联可驱逐=知情妥协注(EC-14,systems+perf 双确认零正确性影响,#17 持久态兜底)+ EC-14 全 Critical fallback 补 snap / **R4** OQ-VFX-3 三补(链式破产场景〔in-kind 不计 cash 腿〕+ game-thread CPU vs GPU 分列度量〔防 P99 总门掩盖 CPU hitch〕+ `N_max_umg` UMG 软上界 + PP 子预算 floor)+ Tuning 加 `N_max_umg` / **R5** AC-55/56 命中判据(开放式提问+词义裁定防橡皮图章,AC-56 补落地踏实感盲区)+ CR-5 `EChangeReason` 分类表 + EC-17 未知 reason 中性兜底 / **R6** AC-43b 枚举随 OQ-VFX-13 裁定 + spy 验逐 handler 名 / AC-35/36 拆 spy 路由+材质参数 vs 渲染视觉 / F-2 `==0` 短路合并入守卫块 / OQ-VFX-14 险过反馈登记。AC 74→75(59 Logic:AC-51b)。
> **deflate 记录(主审独立核实驳回的 over-claim)**:perf「3N/4N 级联」(in-kind 不广播逐格 cash,bankruptcy AC-17)+ perf 1b/3/6(已在 OQ-VFX-3 Pre-Production 标定范畴)/ systems F2-A(L168+AC-10 已覆盖 `==0`)/ qa AC-43b↔OQ-VFX-13 + AC-35/36 标签(计数仍 12 + HUD 已 Approved 姊妹档同 render-AC 模式)/ game-designer B-2 渲染时序求 [Logic] AC(违 headless 陷阱原则 + 同帧已在 OQ-VFX-6)。**收敛证据**:blocker 量逐轮单调降 5→5→5→4→**2**,严重度降至「一长潜矛盾 + 一措辞软化」,M1 是未触接缝的 coverage find 非任何修法残渣 → 终结轮,R-6 应 APPROVED-with-followups。

> **R-6 修订(2026-06-06 fresh-session design-review,full 5 specialist + CD,独立 fresh-grep + 主审逐条对文核实)= NEEDS REVISION(finishing-class 终结轮,本轮 in-doc 全闭)→ 就地修订完成**:**2 must-land BLOCKING(均 R-5 修法引入的实现约束未落地,非重裁系统;主审对文核实)+ 4 folded Recommended** —
> ① **pawn registry 来源未登记(systems+qa+technical-artist 三专家独立收敛,主审 fresh-grep 核实)**:R-5 M1 命名例外引入"经 Payee PlayerId 查 pawn registry 取 `GetActorLocation()`"(CR-2/EC-16/V-Own-09/AC-51b),但 `pawn registry` 全文仅 4 处 #19 内部引用、**无 OQ 条目、Dependencies 无该查询接口**(movement.md 亦无 pawn 坐标查询;R-5 review-log L167 已预警此为 R-6 焦点 #2「若无源则 AC-51b 不可兑现=须登记 OQ 或 propagate」)。AC-51b 是 [Logic] BLOCKING 却断言可 headless spy 一个 owner 未定义的接口 = **phantom-spy 风险**(stated-coverage 红线:测试桩有断言无被测接口)。修:**新增 OQ-VFX-15**(pawn registry 接口来源,owner 待架构裁定:AGameState 内置 PlayerArray / movement4 暴露 `GetPawnForPlayer` / board-manager 持映射)+ **AC-51b 标 `[Logic·门控 OQ-VFX-15]`**(镜像 AC-14/15c 的 `IGameClock` DI 门控先例,接口存在前 spy 注入边界待架构定)+ Dependencies 注记 + CR-2/V-Own-09/EC-16 回指 OQ-VFX-15。**零跨档**(registry owner 归架构期,非现可闭的 propagate)。
> ② **N_max_umg 旋钮存在但 F-4 正文无机制 + 升级闭环漏枚举(performance-analyst;主审收窄 over-claim)**:`N_active_umg` 全文 0 命中;旋钮仅存于 Tuning(L297)/OQ-VFX-3(溢出已定义"最旧 UMG snap")。**主审收窄**:溢出行为已定义,非"设计上无界";真缺口窄 = F-4 正文未配 UMG 计数门 + 升级闭环(AC-21/覆盖缺口3)只承诺补 `N_active_general`/`N_active_hop`、**漏 `N_active_umg`**。修:F-4 补 UMG 软上界段(`N_active_umg ≤ N_max_umg`,溢出 snap 最旧非 Critical UMG)+ EC-18 + **AC-61 [Logic]** UMG 溢出 snap + 升级闭环补 `N_active_umg` 枚举。
> folded(Recommended,本轮一并捎带):**EC-16 第三退化路径**(Payee pawn 完全解析失败)补 **AC-51c** [Logic] / **V-Own-01 期待振动触发源**(L311"点击掷骰后")vs CR-1"owner 事件唯一触发源"澄清(经 dice 意图事件 or UI 中转,登记 OQ-VFX-1 同模式注)/ **F-1** Tuning 注 N 实践上界=棋盘总格数(禁硬编码)+ **F-4** 旋钮硬下界=1 加载期 ensure / **OQ-VFX-6** 补 5.7 Substrate 是否改 PP Material 接口=LLM 知识盲区显式声明。AC 75→77(61 Logic:AC-51c/AC-61)。**收敛证据**:blocker 量 5→5→5→4→2→2(严重度降至「R-5 修法实现约束未落地」,无重裁「#19 是什么」)→ R-7 fresh 应 APPROVED-with-followups。

> **R-7 修订(2026-06-06 fresh-session design-review,full 5 specialist + CD,独立 fresh-grep + 主审逐条对文核实)= NEEDS REVISION(finishing-class 终结轮,本轮 in-doc 全闭)→ 就地修订完成**:**2 must-land(均零成本 fold,非重裁系统;4 专家判 APPROVED-with-followups vs qa-lead 1 BLOCKING,CD 裁可操作内容一致取 must-land 落地前不标 Approved)** —
> ① **AC-43b 验证义务落注释不落 THEN 权威断言(qa-lead;主审对文核实 L485 属实,[[gdd-fix-lands-in-comments-not-spec]] 红线)**:THEN 仅断言「spy 计数==12」,而「spy 须验**逐个 handler `FunctionName`**」只活在注里。一旦 OQ-VFX-13 裁定 V-Own-11 改订 `OnPlayerBankrupt`(替换 `OnBankruptcyDeclared`),delegate 总数仍 12,纯计数 spy 静默放过「漏解绑旧 `OnBankruptcyDeclared` 野指针」=枚举漂移。修:**逐 handler `FunctionName` 匹配断言由注移入 THEN**(抓①漏解绑数 owner 系统 6 + ②枚举漂移两类 false-green)。零架构。
> ② **V-Own-01 掷骰意图事件来源未登记 OQ(technical-artist + game-designer 双专家独立收敛;主审对文核实 OQ-VFX-1 scope=hop path 不覆盖)**:起振锚「意图事件来源待架构确认」只活散文注、无 OQ 表条目/owner/目标期 = R-6 自立 pawn-registry 范式(散文注+OQ 条目)的平行镜像却缺 OQ 那一半。修:**新增 OQ-VFX-16**(owner=骰子3/回合2,架构期,MVP fallback `OnPhaseChanged(RollPhase)` 已具名)+ V-Own-01 回指。**不**升 [Logic] AC(渲染层锚点,避 headless 陷阱)。零跨档。
> **收敛证据**:blocker 量 5→5→5→4→2→2→2,本轮 2 条均 finishing-class(AC-43b 挂已登记 OQ-VFX-13 接缝、V-Own-01 是 R-6 范式第二代 tracking 残留),无新正交维度、无重裁「#19 是什么」→ R-8 fresh 应为真 APPROVED-with-followups。OQ 表 AC 计数不变(77/61 Logic;OQ-VFX-16 是 OQ 非 AC)。

## Overview

游戏反馈 VFX(#19)是《骰子大亨》呈现层的 **juice / 视觉反馈唯一 owner**——它把骰子(3)、移动(4)、经济(5)、建房(8)、地产(6)、破产(9)持续广播的游戏事件,翻译成玩家眼睛看得见、身体感觉得到的"获得感":掷骰的期待→翻滚→定格、棋子逐格 hop、收租金币飞溅、建房咔哒落地、酒店合并升起、买地金色脉冲、破产去饱和消散、胜利彩带烟花。作为**呈现层纯叶子**,#19 只**订阅** owner 系统事件、只负责**播放视觉反馈**;从不写游戏状态、不裁决结算、不阻塞框架推进——所有数值/结果由 owner 先算定,#19 收到事件即播(落后只丢动画不回灌框架,同 HUD F-4)。它与 HUD(16)/地产卡 UI(17)的边界由 **enable-not-own** 协议厘清:HUD/#17 拥信息显示布局 + UMG widget 结构,把 juice 资产(Niagara 粒子、弹性动画)的所有权移交 #19;#19 **直接订阅 owner 事件**(不经 HUD/#17 转发)在世界/屏幕空间播放 juice。对玩家而言,#19 就是让每一次掷骰、收租、建房"本身就爽"的那层视觉爆点——art-bible 支撑原则 2「获得感必须视觉化」的执行者、MDA Sensation(priority-4)的主要承载系统。没有它,核心循环每个动作都退化为朴素数字变化,游戏失去桌游的物理感与社交戏剧性。技术上,#19 是一组由 gameplay 事件触发的 Niagara 粒子系统 + UMG 动画驱动器,持**独立非权威 `FRandomStream`**(juice 随机严禁复用骰子3 权威流,防联网重放污染,ai CR-5④)。

## Player Fantasy

**核心感受:"每一下都'砸'得到——骰子在桌上滚出命运,金币哗啦砸进我的账户,房子咔哒落地;这一局是我用手感赢下来的。"**

#19 服务的是 **MDA Sensation(感官愉悦,priority-4)**——它不加任何规则,却决定核心循环"摸起来爽不爽"。每个关键博弈动作都有即时、物理感强烈的视觉回报:

1. **掷骰的命运时刻**(支柱③运气×策略):按下掷骰,骰子带期待翻滚、定格——两颗骰面**分别**亮出点数(非从总和拆解);掷出双点时的强化反馈让"再走一次"的兴奋即时可见,而连掷三次双点(入狱)切换为**警告色调**——庆祝与危险一眼可辨。
2. **收租 / 发薪的爽感**(支柱②社交博弈):金币从付租方飞溅向收租方、数字哗啦跳动——"我的地替我赚钱"的满足 + "对手又被坑一笔"的社交快感,靠这道金色弧线传递。
3. **建房 / 买地的获得感**(支柱①桌游忠实):房子带弹性"咔哒"落地、四栋合并成酒店时旋转升起闪光、买地时地块泛金色脉冲——桌面上"这块地是我的了"的实体满足,被夸张为视觉爆点。
4. **破产 / 胜利的戏剧高潮**:破产玩家去饱和、棋子消散退场;胜者被彩带烟花从四周涌入包围——一局的情绪曲线在两极收束。

*Design test(art-bible 支撑原则 2)*:在"朴素呈现"与"有 juice 的反馈"之间永远选 juice;动效预算紧张需裁剪时,**优先保留收租金币 + 建房落地动效 + 掷骰/hop 核心循环节拍**,先裁背景循环/非交互装饰。**注(R-1 Issue 4)**:掷骰命运时刻 + 逐格 hop 是 Fantasy 第 1/2 节拍(支柱③),**不视为可先裁的"装饰 Motion"**——故 hop 走**独立 `N_hop_max` 预算**(F-4 双池),破产级联等一般池 churn **不驱逐 hop**;裁剪/驱逐序仅在一般池内适用。但 juice **永不阻塞**游戏推进——它"使能爽感"而非"拥有流程",落后于回合时只丢中间动画、直呈最终态(同 HUD F-4;当前行动者自身 hop 享最短展示窗口,CR-8),绝不让视觉拖慢博弈节奏。

> 注:`creative-director` 未咨询(lean 模式)。production 前须人工复核情感措辞与支柱对齐。

## Detailed Design

### Core Rules

> 约定:#19 一律**呈现侧纯叶子**——订阅 owner 事件、播放 juice;**绝不**写游戏状态、裁决结算或阻塞框架。owner 事件签名见 Interactions 表。

**CR-1 事件驱动播放(纯订阅)**:#19 启动时订阅各 owner 的 multicast delegate;收到事件即在对应锚点播放 juice 资产(Niagara/UMG 动画),**单向消费**——不回调 owner、不写状态、不返回值影响结算。owner 事件是唯一触发源(#19 不轮询、不自驱)。

**CR-2 锚点定位(世界 / 屏幕空间)**:juice 锚点从事件 payload 取——**世界空间**(棋盘 tile / 棋子坐标:掷骰、hop、建房、买地脉冲、破产消散)经 tile/pawn 的 world transform;**屏幕空间**(数字跳动、卡片飞入、胜利覆盖层)经 UMG。坐标解析失败 → EC 降级(跳过该次 juice,不崩溃)。**格定位锚点取 tile-center 非 mid-move 实时位置(RB-4)**:`OnRentPaid`/`OnOwnershipChanged` 等"格事件"的锚点**经 payload `TileIndex` → 棋盘1 `GetTileWorldTransform(TileIndex)` 取 tile-center**,**禁取 pawn 实时 `GetActorLocation()`**(hop 动画中途读会拿到插值半路坐标,金币从半空飞出)。仅"棋子本体"juice(hop/落地)才用 pawn transform,且以 owner `OnPawnLanded` 权威落点为最终基准(EC-12)。tile-center 映射对棋盘1 构成只读查询依赖(Dependencies 已列)。
> **命名例外·收租金币 Payee 终点(R-5 M1,CR-2 ↔ V-Own-09 锚点矛盾闭合)**:V-Own-09 收租金币飞弧需**两个锚点**——**起点=付租方(Payer)** = 收租地产格 `TileIndex` 的 tile-center(Payer 刚落地于该格,其位置≈tile-center,守 CR-2 默认规则、无须取 pawn);**终点=收租方(Payee)棋子世界坐标**。问题:`OnRentPaid(Payer,Payee,Amount,TileIndex)` 的 `TileIndex` 是**收租地产格**、**非 Payee 当前所在格**(地主此刻可在棋盘任意位置),payload **不携 Payee 格位** → 终点无法经 `TileIndex` 解析。**裁定(命名例外)**:#19 经 `Payee`(PlayerId)查 owner pawn registry 取 Payee pawn `GetActorLocation()` 作终点,**此处豁免 CR-2「禁取 pawn 实时位置」禁令**。**(R-6:pawn registry = PlayerId→pawn 查询接口,其 owner 系统〔AGameState 内置 PlayerArray / movement4 暴露查询 / board-manager 持映射〕在 GDD 未定、归架构期裁定 → 登记 OQ-VFX-15;此为 #19 唯一外部 pawn 坐标查询依赖,Dependencies 已注记)****豁免理由(literal-prohibition-vs-legislative-intent)**:CR-2 禁令的**危害域**是「hop 动画**中途**读棋子拿到插值半路坐标」;而 `OnRentPaid` 触发时 **Payee 非当前行动者、处于静止态(非 hop 中途)**,该危害不适用——这是禁令立法意图外的授权特例,非禁令失守。**边界(防例外沦为后门——consumer-latch 反模式)**:豁免**严格限 Payee 终点 + 严格限 `OnRentPaid`**;其余一切格事件(`OnOwnershipChanged` 等)锚点**一律 tile-center 不变**;**若 Payee 恰在移动中(罕见:同帧重叠 hop / Pass'N Play 切手帧边界)→ 终点退化为 Payee 当前格 tile-center**(经 `OnPawnLanded` 权威落点或环序推算),**绝不取 mid-hop 插值坐标**。AC-51b 守(命名例外解析 + mid-hop 退化)。**(三方案 A/B/C 中用户裁定方案 B:零跨档改动 + 保留支柱②金币飞向对手棋子的社交戏剧性,art-bible §1 Design Test 标为优先保留)**

**CR-3 掷骰 juice(继承义务 dice OQ-T-Dice-4,端到端 owner)**:订阅骰子3 `OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})`,播放**期待→翻滚→定格**三阶段节奏;**两颗骰面分别用 `Die1`/`Die2` 呈现,严禁从 `Total` 拆解**。双点强化反馈**差异化**:从 player-turn 取 `ConsecutiveDoubles`(非裸 `bIsDouble`)——1~2 连=庆祝强化;第 3 连(入狱)=警告色调。**定序阶段抑制**:player-turn 告知"当前=开局定序"语境时,#19 抑制双点强化(定序掷骰不进双点链,dice OQ-T-Dice-1)。**`ConsecutiveDoubles`/定序语境取值机制(RB-3 钉,owner 单源)**:`FDiceRollResult` **不携带** `ConsecutiveDoubles`;#19 **经 player-turn `OnPhaseChanged(FPhaseChangedInfo{OldPhase,NewPhase,ConsecutiveDoubles})` payload 读取**(与 HUD CR-6 同源,**禁直接轮询 player-turn 属性**——时序不保证)。定序语境亦经 `OnPhaseChanged`/阶段枚举判定。#19 在 `OnDiceRolled` 到达时取**最近一次 `OnPhaseChanged` 缓存的 `ConsecutiveDoubles`**;若两事件时序未对齐或值缺失 → EC-9 降级基础 juice。**`OnTurnStarted` 时 #19 重置缓存 `ConsecutiveDoubles=0`(R-3,防上家旧值污染新玩家首掷——新玩家首掷在其 `OnPhaseChanged(RollPhase)` 携新值前到达时,缓存须为 0 而非上家残留值,AC-32b)。**

**CR-4 移动 hop juice(继承义务 movement OQ-T-Move-3)**:订阅移动4 `OnPawnMoveStarted` 播放棋子**逐格 hop** 回放、过 GO 时高亮、`OnPawnLanded` 落地 juice。**hop path 来源裁定**:#19 自建路径(据 from/to + 棋盘环序自行插值)为 MVP 默认;若 movement 暴露 `HopPath` 则优先用(归 #19 裁定,见 Open Questions / movement OQ-Move-2)。

**CR-5 经济 juice(继承义务 economy Visual,Sensation priority-4)**:订阅经济5 `OnRentPaid(Payer,Payee,Amount,TileIndex)` 播金币从付租方**飞溅**至收租方(锚点见 CR-2 命名例外/V-Own-09);`OnCashChanged(Player,Old,New,EChangeReason)` 据 reason 播数字跳动;`OnBankruptcyDeclared(Debtor,Creditor)` 播清算 juice。**`EChangeReason` 分类表(R-5 R4,枚举完整性)**:**正向金色**(`Salary` 发薪 / `RentReceived` 收租 / `PassGo` 过 GO);**负向红色**(`RentPaid` 付租 / `Tax` 税 / `Purchase` 买地 / `JailFine` 保释金 / `BuildCost` 建房支出);**未知 reason** → **中性白色 + Warning,不臆断正负色**(EC-17,防罚款误显金色奖励)。分类表为 #19 呈现侧约定,经济5 新增 reason 时须回补此表(否则走 EC-17 中性兜底,不崩溃)。

**CR-6 建房 / 买地 / 抵押 juice(enable-not-own,承接 #17 / #8)**:订阅建房8 `OnBuildingChanged(tile, newCount)`(owner 2 字段,fresh-grep 核实)——**方向由 #19 本地从 `newCount` 单调性派生(R-2,镜像地产6 owner-delta 先例 property-ownership L259)**:#19 自持 per-tile `house_count_cache[tile]`(呈现层 memo,**非游戏状态**,同 `E_cur_vfx`/自建 `FRandomStream` 类),`newCount > cache` → **Build**(`newCount∈[1,4]` 建房弹性弹出+星星;`newCount==5` 四栋→酒店合并旋转升起闪光),`newCount < cache` → **Sell/拆除**(含破产拆除)播**较轻"降档/snap"反馈**(无金星庆祝,art-bible「Gold 仅奖励」不被卖房误触发),`newCount == cache` → no-op(防御);每次处理后置 `cache[tile]=newCount`。**方向只依赖 delta 符号,步长无关(R-3:含破产清零 `h→0` 大跳仍 `<cache`=正确派生 Sell,派生正确性不挂在「原子 ±1 单步」上;building Build/Sell 恰为 ±1 是事实但非派生前提)**。订阅地产6 `OnOwnershipChanged(TileIndex,Old,New)` 播买地金色脉冲+地块徽章(`None→PlayerId`,V-Own-14)+ **破产清算移交反馈(`PlayerId→PlayerId` 易主清算 / `PlayerId→None` 回退无主,V-Own-14b/14c,承接 bankruptcy L191/194 已 Approved 委托)**,并 **在每个 `OnOwnershipChanged` 上 re-seed `cache[tile]=GetHouseCount(tile)`(R-3 Finding A 关键修复:银行破产 `LiquidateAllBuildings` 逐格清零可能不广播 `OnBuildingChanged` → cache 残留旧高值 → 该格回银行重买后首次建房误判 Sell;ownership 变更对两移交形态逐格必发=cache 失效兜底,封住 building **广播契约**层——无视 `LiquidateAllBuildings` 是否/如何广播 `OnBuildingChanged`;**但 re-seed 正确性仍有两个隐含前提(R-5 M2,非无条件自闭)**:① **执行序**——`GetHouseCount` 须在 building 清零**之后**被读到 0,即清零须先于/同步于 `OnOwnershipChanged` 广播(若 ownership 广播先于清零,re-seed 取旧高值漂移仍存,OQ-VFX-11 执行序);② **#8 readiness**——re-seed 调 building `GetHouseCount` 须 #8 已就绪(OQ-VFX-2,同种子化 lifecycle))**,`OnMortgageChanged` 播抵押提交反馈。**这些 juice 的视觉资产归 #19**(#17 只提供数值显示 + 事件锚点语义,不持 VFX 引用)。
> **缓存种子化(R-2,seed-before-first-event)**:#19 订阅建房8 时**立即用 building `GetHouseCount(tile)` 种子化** `house_count_cache`,保证首个 `OnBuildingChanged` 到达前缓存已映射当前真值 → 正常路径零 miss;读档重建(EC-5)同样从快照/`GetHouseCount` 重新种子化。**`OnOwnershipChanged` 上 re-seed(见上 CR-6)封住 building 静默清零路径(R-3 Finding A)——seed-before-first-event 只保证常规建/卖,不依赖 building 在破产 teardown 必广播 `OnBuildingChanged`**。building CR-4/CR-5 恰为 `±1`,但 #19 派生只取 delta **符号**,步长无关→多档跳变(若上游引入)亦正确派生。**lifecycle 顺序(R-3 RECOMMENDED)**:种子化假设 #8 在 #19 订阅时已就绪;若 LOAD 时 #8 init 晚于 #19,`GetHouseCount` 返回 0 = wrong-low seed → 同 Finding A 类误播,须架构期钉 #8 readiness 顺序或 #19 延迟种子化(OQ-VFX-2 范畴)。**`EBuildDirection` 跨档依赖已删**(R-2 本地派生取代 propagate,OQ-VFX-10 RESOLVED——删接缝比 propagate 更合 CD 流程约束,且 preserve-first 建房庆祝在 MVP 即可工作、零 building 侧改动);若 building 未来暴露方向字段则可选优先用(同 `HopPath` OQ-VFX-1 模式)。缓存 miss(种子化失败/首见新 tile)→ EC-15 非方向 snap 兜底。
> ⚠ **遗留(非 #19 职责,producer 单独工单)**:building(2 字段 `(tile,newCount)`)↔ player-turn CR-3.5(3 字段 `(tile,newCount,actingPlayerId)`)的 `OnBuildingChanged` 字段数漂移**仍活在两个 Approved 档之间**——#19 退出该接缝(本地派生 + 不订阅 `OnBuildingAnnounced`)**不闭合此漂移**,须 producer 对 building-upgrade + player-turn 单独开工单,勿误判为本 GDD 已解决。
> ⚠ **遗留 2(R-3,非 #19 职责;producer + bankruptcy9 + OQ-Build-3)**:银行破产 `LiquidateAllBuildings(debtor)`(building L75/L219 provisional 逐格清零)**是否广播 `OnBuildingChanged`、一次 `(tile,0)` vs 逐档** 在 owner 档未定义(bankruptcy9 Not Started)。#19 的 `OnOwnershipChanged` re-seed 已**封住该广播契约层 cache 漂移 bug**(无视广播是否发出;**但前提:清零执行序先于/同步于 ownership 广播 + #8 readiness,R-5 M2,非无条件自闭**——OQ-VFX-11/OQ-VFX-2);若上游确定逐格广播,则 #19 亦可直接据 `OnBuildingChanged` 播 teardown 降档 juice(更精细)。广播粒度 + 执行序登记 OQ-VFX-11,待上游裁定。

**CR-7 胜利 juice**:订阅回合2 `OnGameWon(WinnerId)`(owner=#2,据破产9 `ResolveBankruptcy` 返回 `winnerId` 触发,player-turn L218/388)播彩带+烟花从屏幕四周涌入(art-bible 状态 F,粒子密度全局最高,**受 `N_confetti_max` 上界约束**,F-4/RB-8)。

**CR-8 非阻塞 + 追赶/跳过(同 HUD F-4 / CR-11)**:juice **永不阻塞**框架推进。若 #19 落后于回合(下一回合已开始 / juice 队列积压),**丢弃过期的中间动画、直呈最终态**(如直接显示建好的房子,跳过弹出动画),**不回灌框架、不要求 owner 等待**。关键戏剧 beat(破产/胜利)即使追赶也至少播最终态。
> **当前行动者自身 hop 最短展示窗口(Issue 5,Pass'N Play 防瞬移)**:**当前回合行动者自己的 hop**(`OnPawnMoveStarted` 触发的逐格回放)在被 F-3 判过期前,**保证至少播完压缩 min 形态**(逐格压到 `T_hop_single` 下限的快掠,绝不瞬移 snap)——保住「看着棋子逼近命运」核心节拍(支柱③ + 继承义务 movement OQ-T-Move-3)。机制:hop 实例携 `bIsCurrentActorHop` 标记,该标记下过期判定**延后一个 `T_hop_protect`(=压缩 min 全程)窗口**才允许 snap;**此窗口仅护视觉,框架照常推进**(下一玩家逻辑回合已开始,#19 不回灌、不要求等待),与「永不阻塞」不矛盾。非当前行动者的历史 hop(已交棒)仍按 F-3 整条跳。`T_hop_protect` 列 Tuning Knobs。
> **落地 juice(V-Own-08)追赶妥协(R-3 game-designer,知情标注)**:`OnPawnLanded` 触发的落地弹性/尘土/格高亮是**独立事件、无 `bIsCurrentActorHop` 标记**,且属一般池 Motion tier(最低优先级)——快播/破产级联下可能被 F-3 过期或驱逐,玩家虽 snap 到「棋子在格上」终态但失「落地一砸」。这是**知情妥协**(不影响游戏正确性,信息已达);若 Pre-Production playtest 显示「落地踏实感」(Fantasy 第 1 节拍)折损显著,可考虑把当前行动者 `OnPawnLanded` 落地 juice 也纳入 `bIsCurrentActorHop` 窗口保护(OQ-VFX-12 同期评估)。

**CR-9 juice RNG 隔离(继承义务 ai CR-5④ / HUD AC-47 / #17 CR-12)**:#19 一切 juice 随机(粒子节奏抖动、火花方向等)**必走 #19 自建独立非权威 `FRandomStream`**,**严禁复用骰子3 权威流**(防联网重放污染)。

**CR-10 性能 / reduce-motion 边界**:juice 在 60fps 预算内(Niagara 池化、粒子上限);**`reduce_motion=On` 时**所有 juice 降级为瞬现/低强度(与 HUD/#17 **共享同一全局 a11y 布尔**,不独立)。预算紧张裁剪顺序遵 art-bible Design Test(保收租+建房,先裁装饰)。

**CR-11 enable-not-own 边界(对 HUD16 / #17)**:#19 拥 **juice 视觉资产 + Niagara 系统 + 弹性/粒子动画**;HUD/#17 拥**信息显示布局 + UMG widget 结构 + 数据绑定**。#19 **直接订阅 owner 事件**(骰子/移动/经济/建房/地产/破产),**不经 HUD/#17 转发**;HUD/#17 不持 #19 引用,#19 不写 HUD/#17 的 widget。

### States and Transitions

> #19 **不持任何游戏状态**;下表是**单个 juice 效果实例**的呈现生命周期。多个效果可并发(各自独立实例);#19 全局只持"订阅是否活跃"一个布尔(整局常驻)。

**单 juice 效果实例状态机**

| 当前态 | 触发 | 新态 | 备注 |
|--------|------|------|------|
| Idle | 收到 owner 事件(锚点解析成功) | Playing | 起 Niagara/UMG 动画(CR-1/2) |
| Idle | 收到 owner 事件(锚点解析失败) | Idle | 跳过该次 juice、记告警,不崩溃(EC) |
| Idle | `reduce_motion==On` 下收到事件 | SnapDone | 瞬现最终态/低强度,不走完整动画(CR-10) |
| Playing | 动画自然播完 | Idle | 释放效果实例 |
| Playing | **判定过期**(下一回合已开始 / 队列积压,CR-8) | SnapDone | 丢弃中间动画、直呈最终态(关键 beat 至少播最终态) |
| Playing | 玩家加速/跳过(若 UX 提供) | SnapDone | 同追赶路径(呈现加速,不改游戏结果) |
| SnapDone | 最终态呈现完成 | Idle | 释放实例 |

**全局订阅状态**

| 当前态 | 触发 | 新态 | 备注 |
|--------|------|------|------|
| Subscribed | 整局运行 | Subscribed | 订阅常驻,覆盖整局(随 #19 owner 生命周期) |
| Subscribed | 读档重建(存档21) | Subscribed | 重绑全部 owner delegate(先解绑再绑,防双绑 footgun)+ 从快照重建当前态、**不回放历史 juice**(EC) |

> **状态机不变式**:① **无游戏状态**——#19 任何"状态"都是呈现实例生命周期,可丢弃/重建而不影响游戏;② **非阻塞**——任一实例的 Playing/SnapDone 都不阻塞 owner 或框架(CR-8);③ **过期可跳**——非关键 juice 过期可整条跳过,**关键 beat(破产/胜利,critical)** 过期至少呈现最终态(同 HUD F-4 critical 语义);**建房 juice = Social(非 critical,R-1 Issue 2 裁定)**:不被驱逐(F-4 Social 保护)但过期按一般 Social 处理,全员通告 beat 归 HUD `OnBuildingAnnounced` banner,**#19 不订阅 `OnBuildingAnnounced`**;**当前行动者自身 hop** 享最短展示窗口保护(CR-8,非 critical 但护视觉);④ juice 实例**不序列化**,读档从 owner 快照重建最终态、不回放。
>
> **SnapDone 实现语义(RB unreal R-1,归架构 ADR OQ-VFX-2)**:"snap 最终态"对 Niagara 非原生操作,须**分类型**实现——粒子类 juice = 停止 Niagara(`DeactivateImmediate`,**非裸 `Deactivate`**——R-3:裸 `Deactivate` 仅停发射、现有粒子缓慢消散,级联 14 连 snap 时留 14 个 draining 系统拖尾 GPU 成本;`DeactivateImmediate` 立即销毁活跃粒子)+ 覆盖静态最终态(mesh/材质参数);UMG 类 = Timeline 跳末帧;材质参数类(破产去饱和**逐 pawn 材质**,非全屏)= 直接设 Material 参数。**禁把裸 `Deactivate()` 单独当 snap**(粒子会缓慢消散而非瞬切,违 CR-8)。

### Interactions with Other Systems

> 方向约定:#19 是**呈现层叶子消费者**——`←` 订阅/读取 owner 事件;**无任何 `→` 流出**(不写状态、不转发意图,比 HUD/#17 更纯——连意图转发都没有)。

| 系统 | 数据流入 #19(#19 订阅/消费) | #19 流出 | owner |
|------|------------------------------|----------|-------|
| **骰子(3)** | `OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})`——掷骰 juice 用 `Die1`/`Die2` 分呈两骰面(禁拆 Total,CR-3) | 无 | #3 拥 RNG/掷骰结果 |
| **玩家回合(2)** | `OnPhaseChanged(FPhaseChangedInfo{...,ConsecutiveDoubles})`(双点强化差异化:庆祝 vs 入狱警告 + 定序语境抑制,CR-3,**payload 取值非属性轮询**,RB-3)+ `OnTurnStarted`(F-3 过期主判据 `E_cur_vfx` +1,RB-3)+ `OnGameWon(WinnerId)`(胜利 juice,CR-7) | 无 | #2 拥流程/双点链/胜负广播。**注:`OnBuildingAnnounced` 归 HUD 全员通告 banner,#19 不订阅(R-1 Issue 2)** |
| **移动(4)** | `OnPawnMoveStarted`(逐格 hop)+ 过 GO 高亮 + `OnPawnLanded(arrivalContext)`(落地 juice);可选 `HopPath`(CR-4) | 无 | #4 拥位移;hop path 来源待裁定(OQ) |
| **经济(5)** | `OnRentPaid(Payer,Payee,Amount,TileIndex)`(金币飞溅)+ `OnCashChanged(Player,Old,New,EChangeReason)`(数字跳动,据 reason 正/负向)+ `OnBankruptcyDeclared(Debtor,Creditor)`(清算 juice) | 无 | #5 拥现金结算/事件 |
| **地产所有权(6)** | `OnOwnershipChanged(TileIndex,Old,New)`(买地脉冲 V-Own-14 `None→PlayerId` + **破产移交清算 V-Own-14b/14c `P→P`/`P→None`,承接 bankruptcy L191** + **触发 cache re-seed = 调建房8 `GetHouseCount`,R-3 Finding A 封 teardown 漂移**)+ `OnMortgageChanged(TileIndex,bMortgaged)`(抵押提交反馈) | 无 | #6 拥归属/抵押事实 |
| **建房升级(8)** | `OnBuildingChanged(tile, newCount)`(2 字段)+ `GetHouseCount(tile)` 种子化读——#19 本地从 `newCount` delta 派生建/拆(CR-6,R-2,无需方向字段) | 无 | #8 拥 house_count/建房 |
| **破产胜负(9)** | `OnBankruptcyDeclared`(经经济5 广播,棋子去饱和消散 art-bible 状态 G)+ **逐格清算移交 juice 经地产6 `OnOwnershipChanged`(V-Own-14b/14c,bankruptcy L191 委托)** + 胜负经回合2 `OnGameWon` | 无 | #9 拥破产裁决/清算编排 |
| **棋盘数据(1)** | `GetTileWorldTransform(TileIndex)`/环序总格数 N(只读查询)——juice 锚点 tile-center 解析 + 自建 hop path 插值(CR-2/F-1,**RB-4 显式化隐依赖**) | 无 | #1 拥棋盘布局/坐标 |

### 呈现层边界(平级,非依赖)

| 系统 | 边界(enable-not-own) |
|------|----------------------|
| **HUD(16)** | HUD 拥信息布局 + UMG widget + 数据绑定;**HUD 的 juice**(现金 counter 动画的粒子层、收租 juice、AI 行动 beat 视觉资产)归 #19。#19 直订 owner 事件,不经 HUD 转发;HUD 不持 #19 引用 |
| **地产卡 UI(17)** | #17 拥卡片信息布局 + 状态视觉(色条/徽章/高亮/抵押灰化/垄断环圈);**#17 的 V-Enable juice**(建房弹出、酒店合并、卡翻面、买地飞入粒子)归 #19,#19 直订 #6/#8 事件(非经 #17,见 #17 V-Enable 表) |

> **双向一致性(已存在,无需新 propagate):** systems-index 继承义务表 line 83 已登记 VFX(19)为 dice/economy/movement 事件消费方 + MUST-FULFILL owner;#17 V-Enable-01~08 / HUD V-Enable 表已声明 juice 归 VFX19 直订。#19 本表与之对齐,无反向缺口。
>
> 注:`technical-artist`/`ue-umg-specialist` 未咨询(lean 模式)。Niagara 系统结构、UMG 动画驱动、hop path 实现 → 架构期 ADR + Pre-Production。

## Formulas

> **边界声明**:#19 是呈现层纯叶子,本节公式**仅限呈现层时序/数量推导**,不含游戏经济/概率公式。掷骰翻滚/建房弹出等固定动画时长是 art-bible 旋钮常量(列 Tuning Knobs),不在此伪公式化。**越界输入一律降级+告警(UE_LOG),不静默 clamp 掩盖**(同 #17 F-1 G-0 教训)。

### F-1 Hop 节奏(逐格时长)
`N_eff = abs(N)`(**R-2:abs() 前置于 clamp**——防 `N<0` 时 `clamp(T_hop_base×N,min,max)` 撑到下界 `T_hop_total_min` 静默掩盖负值,违「不静默 clamp」原则);`T_hop_total(N) = clamp(T_hop_base × N_eff, T_hop_total_min, T_hop_total_max)`;`T_hop_single(N) = T_hop_total(N) / max(N_eff, 1)`

| 变量 | 类型 | 范围 | 描述 |
|------|------|------|------|
| N | int | hop 格数=`abs(steps)` | 本次移动格数(取自 `OnPawnMoveStarted` payload;若不直供则 from/to+环序反算) |
| T_hop_base | float | 0.12~0.20(默认 0.16) | 单格基准时长(N=1 无压缩) |
| T_hop_total_min | float | 0.3~0.5(默认 0.4) | 总时长下界 |
| T_hop_total_max | float | 2.0~3.5(默认 2.5) | **总时长上界,防 40 格拖沓** |

**越界**:`N==0`→不播 hop 直到 `OnPawnLanded`+Warning(`max(N_eff,1)=1` 无除零);`N<0`→**公式层 `N_eff=abs(N)` 前置**(非调用点临时取绝对值)+ Warning,绝不让 clamp 见负值静默撑到下界;`N>78`→Warning(多圈极端,clamp 兜底)。**Example**:N=7→total 1.12s/单格 0.16s;N=24→total clamp 2.5s/单格 0.104s(快掠);N=2→total clamp 0.4s/单格 0.20s。

> **单格时长非单调(RB systems R-1,设计意图显式化)**:因 `T_hop_total_min` clamp,`T_hop_single` 在 N=1/2/3 段为 0.40s/0.20s/0.16s——**N=1 单步反而最慢**(被 min 下界撑慢),非 bug,是"小步不至于一闪而过"的预期代价。调参须知此非线性。
>
> **自建 hop path 的棋盘1 依赖 + 传送格处理(RB-4 / tech-art B-5)**:MVP 默认 #19 自建逐格路径须知**棋盘环序总格数 N**(经棋盘1 只读查询,**禁硬编码 N=40**——Alpha 扩展棋盘即错;Dependencies 已列棋盘1)。**传送类移动**(`OnPawnLanded.EArrivalContext ∈ {SentToJail, AdvanceCard}`,如去监狱/命运牌前进到最近X)**不得按环序逐格插值整条路径**(会穿过中间格再 snap,视觉错误)——改播**独立"跳跃/押送"过场**,最终位置以 owner `OnPawnLanded` 为准(EC-12)。仅 `EArrivalContext==DiceMove` 走逐格 hop。`HopPath` 由 movement 直供则优先(OQ-VFX-1,免自建 + 免棋盘1 隐依赖)。

### F-2 金币飞溅粒子量
**前置守卫(R-4 立,R-5 R6 合并 `==0` 短路同列,公式层,镜像 F-1 `N_eff=abs(N)` 前置)**:`if Amount < 0 → 中止 + Error;if Amount == 0 → 防御短路 + Warning(不发金币);两者均不进入下式`。**两支共同理由**:① `Amount<0` 防 `log2(0)=-∞`/`log2(负)=NaN` 经 `clamp` 静默撑到 1 粒;② `Amount==0` 防 `log2(0+1)=0→floor=0→clamp(0,1,15)=1` 经 `N_coins_min` **隐性兜底误发 1 粒**(此非 NaN 路径,而是 min 下界误触发)。**两支均须前置于公式表达式,不靠 `N_coins_min` 隐性兜底**(违「不静默 clamp」L140 + AC-10/AC-11)。
`N_coins(Amount) = clamp(floor(K_coins × log2(Amount+1)), N_coins_min, N_coins_max)`（仅当 `Amount ≥ 0`;`Amount==0` 另经 AC-10 防御短路,见越界）

| 变量 | 类型 | 范围 | 描述 |
|------|------|------|------|
| Amount | int | ≥0(经济 `OnRentPaid.Amount`) | 租金额 |
| K_coins | float | 1.5~3.0(**默认 2.0**) | 缩放系数(对数平滑,Amount 翻倍只增 K_coins 粒) |
| N_coins_min | int | 1~3(默认 1) | 小额至少 1 粒 |
| N_coins_max | int | 12~20(默认 15) | **60fps 预算上界,与 Niagara 池协调** |

**越界**:`Amount==0`(免租/抵押)→经济5 AC-5 不广播 `OnRentPaid`;#19 防御性收到 0→Warning+跳过金币;`Amount<0`→Error+跳过。**Example**(K=2.0):Amount=1→2 粒;Amount=50→11 粒;Amount=200(标准租)→触顶 15;Amount=2000→clamp 15(预算保护)。

### F-3 juice 过期判定(与 HUD F-4 同构,独立自持)
`expired_vfx(e) = (E_cur_vfx > E_effect(e)) OR (unpaused_elapsed_vfx(e) > T_stale_vfx)`

| 变量 | 类型 | 范围 | 描述 |
|------|------|------|------|
| E_cur_vfx | int | 单调递增 | **#19 独立自持**回合代次,每收 `OnTurnStarted` +1;**不引用 HUD 内部 E_cur**(平级解耦),口径严格同构 |
| E_effect(e) | int | — | 效果 e 到达时的 `E_cur_vfx`;同回合效果共享 |
| unpaused_elapsed_vfx | float(s) | 游戏时间(暂停不计、读档重置) | 软上界辅判据 |
| T_stale_vfx | float | 3.5~6.0(默认 4.0) | 须 ≥ `T_hop_total_max + 1.5s`(随 hop 上界联动) |

**主判据=下一回合已开始**(非"播得慢"),只用 #19 可观察量(`OnTurnStarted`),**不臆造框架不提供的"状态代次"**(同 HUD R-2 幻象契约教训)。读档:`E_cur_vfx` 重置、pending 效果立即 expired(不回放)。**Example**:hop 播到 0.3s 时 `OnTurnStarted` 到→`E_cur_vfx` 6>5→expired;非关键整条跳、**critical(破产/胜利)** snap 最终态(建房 = Social 非 critical,R-1 Issue 2)。

> **epoch 粒度 = 行动权持有人切换,非每次掷骰(RB-1,显式设计决定)**:player-turn L197——`OnTurnStarted` 在双点额外回合链中**整链只发一次**(`OnPhaseChanged(RollPhase)` 才每次额外回合重发)。故 `E_cur_vfx` **按行动权移交粒度 +1**,**同一玩家双点链内**(最多 3 连)的 hop/收租 juice **共享同一 epoch**、链内不被 epoch 推进互相过期,直到下一玩家 `OnTurnStarted` 才 epoch 左支整链一并过期。**这是预期行为**(链内 juice 属同一行动者的连续表演,不应被链内额外回合的 **epoch 推进**判过期),非 bug。**注(R-2):epoch 保护仅针对 epoch 左支;F-3 的 elapsed 右支(`elapsed>T_stale`)是独立 catch-up backstop,在 epoch 内仍可触发**——已 Playing 超 `T_stale` 的早链 juice 视觉上确已陈旧,按追赶 snap 最终态是正确行为(非误过期)。若链内积压超并发上限由 F-4 优先级驱逐兜底(非 epoch)。AC-15 测 `OnTurnStarted` 粒度;AC-15b 测 epoch 左支不被链内额外回合误过期;**AC-15c 测 elapsed 右支 epoch 内仍可触发**(OR 两支均覆盖)。

### F-4 并发 juice 实例上限(双预算池 + 优先级驱逐)

**双池(Issue 4,防破产级联饿死核心循环 hop):** hop juice 走**独立 `N_hop_max` 预算**,与一般 juice 的 `N_max_vfx` 池**分离**;一般池(收租/买地/抵押/数字/破产/胜利)的级联 churn **不驱逐 hop**。
- 一般池:`N_active_general ≤ N_max_vfx`;满载新效果入队→**驱逐一般池内当前最低优先级最旧者**进 SnapDone(非静默丢弃)
- hop 池:`N_active_hop ≤ N_hop_max`;hop 超载在 hop 池内自驱逐(最旧**非当前行动者** hop 先驱;当前行动者自身 hop 受 CR-8 最短窗口保护不被驱逐)

| 变量 | 类型 | 范围 | 描述 |
|------|------|------|------|
| N_max_vfx | int | 8~16(**默认 12,placeholder**) | 一般池(非 hop)同时活跃上限;**须目标硬件 60fps 性能标定后冻结**(含破产级联场景,见下)——标定前关联 AC 仅 Advisory |
| N_hop_max | int | 4~8(**默认 6,placeholder**) | hop 池独立上限(MVP 2~4 人各最多 1 活跃 hop + 余量);标定后冻结 |
| priority(e) | enum | Critical>Social>Economic>Motion | Critical=**破产宣告/胜利**(V-Own-11/16 戏剧 beat);Social=**建房/买地/抵押**;Economic=收租/数字/**破产移交逐格清算(V-Own-14b/14c)**;Motion=掷骰(hop 已移入独立池) |

**Critical/Social 全程不被驱逐**(一般池内);**无可驱逐低优先级时见 EC-14 fallback**(全 Critical/Social 满载 + 新低优先级到→静默丢弃新者 + 告警,绝不驱逐 Critical/Social)。**Example**:一般池满 12(11 Economic + 1 Critical),新 `OnRentPaid` 到→驱逐最旧 Economic snap→新入队;同时 3 个 hop 在 hop 池独立播放、不受影响。

> **非收租 juice 粒上界(Issue 4,补 F-2 仅覆盖 `OnRentPaid` 的缺口)**:`OnOwnershipChanged` 买地波纹、`OnCashChanged` 数字粒子层等**各设单实例粒上界常量**(art-bible 旋钮,placeholder:波纹 ≤8 粒、数字粒 ≤6 粒),防破产级联 N 格连发时单帧粒子爆量。`N_confetti_max`(胜利,RB-8)= 全局粒密上界(默认 placeholder,标定冻结),confetti/fireworks 总活跃粒受其约束(art-bible 状态 F「全局最高」即指此上界,**非无界**)。
>
> **UMG 类 juice 软上界 `N_active_umg ≤ N_max_umg`(R-6 BLOCKING-2,补 F-4 仅覆盖 Niagara 双池的缺口)**:UMG 类 juice(V-Own-10 数字浮字、V-Own-14b/14c 徽章、V-Own-11 vignette——**不计入 Niagara `N_max_vfx`/`N_hop_max` 池**,见 §3 资产前缀订正)在破产级联(14~28 事件同窗)下并发**无 Niagara 池约束** → 须**独立 UMG 计数门** `N_active_umg ≤ N_max_umg`(Tuning Knobs 已列 placeholder 24~40,标定冻结)。**溢出处理:snap 最旧非 Critical UMG 实例**(Timeline 跳末帧,同 SnapDone UMG 语义)+ 告警;**Critical UMG(破产 vignette V-Own-11)即使溢出亦先 snap 最终态**(EC-11 critical 语义,绝不静默丢)。`N_max_umg` 真值随 `N_max_vfx` 一并目标硬件标定冻结(OQ-VFX-3 UMG overdraw 子项);**标定前性能门为 Advisory,但溢出逻辑路由(→snap 最旧 UMG)可 headless [Logic] 验(AC-61)**。EC-18 + AC-61 守。
>
> **破产级联标定义务(performance P-1,纳入 OQ-VFX-3)**:property-ownership 破产批量移交**逐格各广播一次 `OnOwnershipChanged`**(N 格=N 事件同窗),4 人局 debtor 持 7~14 格。**R-3 校正 worst case ≈ 2N 事件**:若银行破产 `LiquidateAllBuildings` 逐格广播 `OnBuildingChanged`(OQ-VFX-11 待裁),则同窗 = `OnOwnershipChanged(N)` + `OnBuildingChanged(N)` ≈ **14~28 事件**(原「14 格连发」只数了 ownership 一半)→ 一般池瞬时需求远超 12。`N_max_vfx`/`N_hop_max` 标定**必须含 2N 破产级联压力场景 + 14~28 次连续驱逐 SnapDone 帧 + 「最后一人破产 + `OnGameWon` 同帧」(R-3:2 人局正常终局 = 最硬单帧,破产去饱和材质 + 胜利暖金 PP + confetti `N_confetti_max` + 烟花同帧)**,门控用 **P99 ≤33ms**(非仅 120 帧均值,防单帧尖峰被均值掩盖)。

> **数字 counter 时长不重定义**:#19 订阅 `OnCashChanged` 的屏幕空间数字跳动粒子层时长 **= HUD F-1 `T_counter` 同口径**(读共享旋钮),确保 HUD 数字滚完时 #19 粒子恰好消散,不视觉撕裂。引用非重复定义。
>
> 注:`systems-designer` 已咨询(lean·D 高风险节)。Niagara 系统结构/hop path 实现 → 架构期 ADR。

## Edge Cases

> 注:#19 为呈现层纯叶子,以下均为**呈现正确性**问题,绝不涉及游戏状态写入。`systems-designer` 边界陷阱(D 节)已纳入。

- **EC-1 锚点解析失败**(tile/pawn world transform 拿不到):跳过该次 juice、记告警,**不崩溃**;其余 juice 正常(CR-2)。
- **EC-2 juice 落后于回合(过期)**:据 F-3 `expired_vfx`,非关键 juice 整条跳过、**critical(破产/胜利)** snap 最终态,**不回灌框架、不要求 owner 等待**(CR-8/同 HUD F-4)。建房 = Social(非 critical),过期按一般 Social 处理;当前行动者自身 hop 享最短窗口保护(CR-8)。
- **EC-3 并发 juice 超上限**:据 F-4 优先级驱逐(最低优先级最旧者→SnapDone),**Critical/Social 不被驱逐**;非静默丢弃。
- **EC-4 `reduce_motion=On`**:所有 juice 瞬现/低强度(与 HUD/#17 共享同一全局 a11y 布尔,CR-10)。
- **EC-5 读档后(存档21)重建**:juice 实例**不序列化**;重绑全部 owner delegate(BP `Bind Event` 须**先解绑再绑**,防双绑 footgun,同 HUD AC-48)+ 从 owner 快照重建当前态(如已有房子直接显示),**不回放历史 juice**;`E_cur_vfx` 重置(F-3);**`house_count_cache` 从快照/building `GetHouseCount` 重新种子化(CR-6,R-2,防读档后首个 `OnBuildingChanged` 缓存 miss 误判建/拆)**。
- **EC-5b 销毁/关卡卸载解绑(RB unreal B-1;R-4 修解绑时机)**:#19 在 **`EndPlay`**(关卡卸载、提前销毁、PIE 停止)时**主动 `RemoveDynamic`/`Unbind` 全部 12 个 owner delegate 绑定**(按 delegate 数,AC-43b 枚举),**不依赖 GC 顺序**;否则 owner 先于解绑广播会调已销毁 UObject 的 UFUNCTION(C++ `AddDynamic` 路径崩溃窗口;BP `Bind Event` 自带弱引用守卫风险较低,**禁 C++/BP 混用**,路径由 ADR 选定)。**严禁在 `BeginDestroy` 解绑(R-4):`BeginDestroy` 在 GC 回收期调用,此时 owner UObject 可能已被销毁,`RemoveDynamic` 访问其 delegate 恰好触发本条要防范的野指针;`EndPlay` 是 owner 仍有效的唯一安全解绑点。** AC-43b 守。
- **EC-6 金币 Amount 异常**:`Amount==0`(免租/抵押,经济5 AC-5 不应广播)→ 防御性跳过金币+告警;`Amount<0` → Error+跳过(F-2,不静默 clamp)。
- **EC-7 hop 格数异常**:`N==0`→不播 hop+告警;`N<0`→取绝对值(退格牌反向 hop)+告警;`N>78`→告警(多圈极端,clamp 兜底)(F-1)。
- **EC-8 同帧多事件(8 人局/连锁)**:各事件起独立 juice 实例,受 F-4 上限约束;不互相阻塞、不串台。
- **EC-9 双点强化语境缺失**:若 `ConsecutiveDoubles` / 定序语境从 player-turn 拿不到 → 降级为**基础掷骰 juice**(不做庆祝/警告差异化、不强化),记告警;绝不臆断双点链状态(CR-3,owner 单源)。
- **EC-10 定序阶段双点强化未抑制**:= 呈现缺陷(CR-3 强制定序抑制);定序掷骰播了庆祝/警告强化即违规(AC 守)。
- **EC-11 critical juice 被驱逐/过期**:**破产/胜利**(critical)即使被 F-4 驱逐或 F-3 判过期,**至少 snap 呈现最终态**(棋子消散终态/胜利覆盖层),**绝不静默全丢**(支柱②社交 beat 错过即丢,CR-8)。**建房 juice = Social(R-1 Issue 2)**:不被驱逐(Social 保护)但非 critical;全员可见建房通告 beat 归 HUD `OnBuildingAnnounced` banner,**#19 不承担建房通告 critical-snap**(原"建房通告 critical"措辞为 R-1 前矛盾,已清除)。
- **EC-12 hop path 与最终落点不符**(#19 自建 path vs owner 真实位移):**以 owner `OnPawnLanded` 的最终位置为准**,hop 仅视觉过场;若插值路径与最终格冲突,落地 snap 到 owner 权威位置(CR-4,#19 不裁决位移)。
- **EC-13 juice RNG 误接骰子3 权威流**:设计/实现禁止(CR-9);code-review 发现复用即缺陷(ai CR-5④ 重放污染)。
- **EC-14 一般池满载且无可驱逐低优先级(RB-7,F-4 fallback)**:一般池满 `N_max_vfx` 且全为 Critical/Social(无 Economic/Motion 可驱逐),新到低优先级效果 → **静默丢弃新者 + 告警**(绝不驱逐 Critical/Social);若新者本身是 Critical(如同帧二人破产)→ 驱逐**最旧 Social**让位(Critical 不丢);**若池内全 Critical 无 Social 可让(极端不可达:N_max=12 远大于 MVP 2~4 人最多 3 破产+1 胜利=4 Critical)→ 丢弃新 Critical 前先 snap 其最终态**(critical 至少呈最终态,EC-11)。hop 池独立不受此约束。
> **V-Own-14b/14c 级联可驱逐 = 知情妥协(R-5 R3,systems+performance 双确认)**:破产移交逐格清算 juice(V-Own-14b/14c)为 **Economic** tier,在级联高 Critical/Social 压力下被本 EC-14 fallback **静默丢弃属知情妥协,非正确性问题**——逐格最终归属态由 **#17/棋盘层持久显示**(V-Own-14b/14c 仅拥**瞬态动画**,不拥状态);bankruptcy L191 委托的是 juice **呈现使能**义务,非功能性状态义务,动画丢失不影响游戏正确性。同 V-Own-08 落地 juice 追赶妥协类(CR-8)。此 Economic tier 分类亦是 F-4 一般池在 2N 级联下的**性能泄压阀**(若升 Social 则 EC-14 不驱逐、池行为不同)。
- **EC-15 缓存 miss / 卖房 / 破产清零(Issue 1,R-2 本地派生;R-3 Finding A 扩)**:`newCount < cache[tile]`(卖房/破产拆除,**含 `h→0` 大跳**如破产清零 5→0)→ 派生 Sell,播**较轻降档/snap 反馈,绝不播建房金星庆祝**(防 art-bible「Gold 仅奖励」被卖房误触发;方向只取 delta 符号,步长无关,AC-54d 守 h→0);**缓存 miss 兜底**(种子化失败/首见新 tile 无 prior)→ 退化为**非方向"档位变更"snap**(直呈新 house_count 终态,不播庆祝),记告警。
- **EC-15b 银行破产静默清零的 cache 漂移(R-3 Finding A,关键)**:银行破产 `LiquidateAllBuildings` 逐格清零 house_count **但可能不广播 `OnBuildingChanged`**(building L75/L219 provisional,OQ-VFX-11 待裁)→ 若 #19 仅靠 `OnBuildingChanged` 更新 cache,则 cache 残留旧高值,该格回银行重买后首次建房 `newCount<stale cache` 被误判 Sell(EC-15 miss 兜底抓不到=cache 有条目非 miss)。**修:#19 在每个 `OnOwnershipChanged` 上 re-seed `cache[tile]=GetHouseCount(tile)`(CR-6)**——ownership 对 in-kind(P→P)/回退(P→None)两形态逐格必发(bankruptcy L191),cache 失效兜底封住 building **广播契约**层(无视 `LiquidateAllBuildings` 是否广播 `OnBuildingChanged`);**前提(R-5 M2,非无条件自闭)**:re-seed 时 `GetHouseCount` 须返回最新值——① building 清零执行序须先于/同步于 ownership 广播(OQ-VFX-11);② #8 在 re-seed 时已就绪(OQ-VFX-2)。AC-54a/54b/54d 守卖房/miss/h→0 退化均不误播建房庆祝;AC-45 类守 re-seed 后正常响应。
- **EC-16 收租金币 Payee 终点锚点解析(R-5 M1,CR-2 命名例外)**:V-Own-09 终点经 `Payee`(PlayerId)查 pawn registry 取静止 pawn `GetActorLocation()`(CR-2 危害域外授权特例,Payee 非当前行动者);**若 Payee 恰在 hop 中途**(罕见:同帧重叠移动 / Pass'N Play 切手帧边界)→ **退化为 Payee 当前格 tile-center**(`OnPawnLanded` 权威落点或环序推算),**绝不取 mid-hop 插值坐标**(否则金币飞向半空,正是 CR-2 禁令本意);若 Payee pawn / 格位均解析失败 → 终点退化为收租地产格 tile-center(起点)附近小幅爆发,记告警、不崩溃。AC-51b/51c 守(静止/mid-hop/registry-null 三退化路径全覆盖);**pawn registry 查询源待架构裁定(OQ-VFX-15)**。
- **EC-17 未知 `EChangeReason`(R-5 R4,枚举完整性兜底)**:CR-5 据 `EChangeReason` 决定金色正向/红色负向数字跳动;若收到 #19 分类表未覆盖的 reason(未来经济5 新增类型,如保释金/策略卡现金效果)→ **降级为中性白色浮字 + Warning(UE_LOG)**,**绝不臆断正/负向色**(防把罚款误显金色奖励、违 art-bible「Gold 仅奖励/Red 仅扣钱」)。CR-5 分类表见该节。
- **EC-18 UMG 类 juice 并发超 `N_max_umg`(R-6 BLOCKING-2)**:UMG 类 juice(数字浮字/徽章/vignette,不计 Niagara 双池)级联下 `N_active_umg` 超 `N_max_umg` → **snap 最旧非 Critical UMG 实例**(Timeline 跳末帧)+ 告警,新实例入;**Critical UMG(破产 vignette)溢出先 snap 最终态不静默丢**(EC-11)。与 F-4 Niagara 双池(`N_max_vfx`/`N_hop_max`)独立计数,互不挤占(同 §3 池预算分离)。AC-61 守。

## Dependencies

> #19 是**呈现层叶子**:所有依赖均为**单向订阅/读取**(#19 ←),**无任何流出**(连意图转发都没有,比 HUD/#17 更纯)。**depends-on 状态(R-1 fresh-grep 核实):systems-index line 152 已应用 `3,4,5,8,2,6`**(✪ #19 设计揪出补 2/6,已落);本 R-1 **再补棋盘1 依赖**(RB-4 锚点/hop path 隐依赖显式化)→ systems-index #19 depends-on 须补 `1`(OQ-VFX-7 增补项)。破产 juice 经经济5 `OnBankruptcyDeclared` 获取(非直依赖 #9)。`OnBuildingAnnounced` **已从 #19 依赖移除**(R-1 Issue 2,归 HUD)。**R-2**:建房8 依赖改为 `OnBuildingChanged(tile,newCount)` + `GetHouseCount` 种子化读(本地 `newCount` delta 派生建/拆),`EBuildDirection` 跨档依赖已删(OQ-VFX-10 RESOLVED,CR-6)。

### 硬依赖(事件源——无之则对应 juice 无触发)

| 系统 | 依赖性质 | 接口/事件 |
|------|---------|-----------|
| **骰子(3)** | 硬 | 订阅 `OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})`(掷骰 juice,用 Die1/Die2 分呈) |
| **移动(4)** | 硬 | 订阅 `OnPawnMoveStarted`/`OnPawnLanded(arrivalContext)`(hop + 落地);可选 `HopPath` |
| **经济(5)** | 硬 | 订阅 `OnRentPaid`/`OnCashChanged(EChangeReason)`/`OnBankruptcyDeclared`(金币/数字/清算 juice) |
| **地产所有权(6)** | 硬(**index 已补,见下**) | 订阅 `OnOwnershipChanged`(买地脉冲 V-Own-14 + 破产移交清算 V-Own-14b/14c + **触发 cache re-seed,R-3 Finding A**)/`OnMortgageChanged`(抵押反馈) |
| **建房升级(8)** | 硬 | 订阅 `OnBuildingChanged(tile, newCount)` + `GetHouseCount(tile)`(种子化)——本地 `newCount` delta 派生建/拆(CR-6,R-2,无需方向字段) |
| **玩家回合(2)** | 硬(**index 已补,见下**) | 经 `OnPhaseChanged(FPhaseChangedInfo{...,ConsecutiveDoubles})` payload 读双点链/定序语境(CR-3,**非属性轮询**)+ `OnTurnStarted`(F-3 `E_cur_vfx`)+ 订阅 `OnGameWon`(胜利)。**不订阅 `OnBuildingAnnounced`**(归 HUD banner,R-1 Issue 2) |
| **棋盘数据(1)** | 硬(**RB-4 新增**) | 只读查询 `GetTileWorldTransform(TileIndex)`(格事件锚点 tile-center,CR-2)+ 环序总格数 N(自建 hop path 插值,F-1);`HopPath` 由 movement 直供则免此依赖(OQ-VFX-1) |
| **pawn registry(来源待裁,R-6)** | 硬(**仅收租金币 Payee 终点**) | 只读查询 `PlayerId→pawn` 取 `GetActorLocation()`(CR-2 命名例外 / V-Own-09 / AC-51b);**owner 系统未定**(AGameState 内置 PlayerArray / movement4 暴露 `GetPawnForPlayer` / board-manager)→ **登记 OQ-VFX-15,架构期钉死**;AC-51b/51c 经此门控 |

### 呈现层边界(平级,非依赖)

| 系统 | 边界 |
|------|------|
| **HUD(16)** | enable-not-own:HUD 拥信息布局+UMG+绑定;HUD 的 juice 归 #19;#19 直订 owner 事件不经 HUD;数字 counter 时长 #19 引 HUD F-1 `T_counter` 同口径 |
| **地产卡 UI(17)** | enable-not-own:#17 拥卡片信息布局+状态视觉;#17 V-Enable juice(建房/酒店/翻卡/买地飞入)归 #19,#19 直订 #6/#8 事件 |

### 横切消费方(依赖 #19 行为)

| 系统 | 关系 |
|------|------|
| **存档/读档(21)** | juice 瞬态**不序列化**;读档后 #19 重绑 owner delegate(先解绑再绑)+ 从快照重建最终态、不回放(EC-5) |

> **双向一致性待同步(propagate,producer 协调):** ① systems-index #19 depends-on `3,4,5,8,2,6`(line 152 已落)→ **R-1 再补 `1`**(棋盘1 锚点/hop path,RB-4);② **#2/#6/#1 的 depended-on-by 应含 #19**(#3/#4/#5/#8 已在继承义务表 line 83 / Dependency Map line 152 登记;#2/#6/#1 反向回标仍 pending = OQ-VFX-7 剩余债);③ 继承义务表 line 83(VFX MUST-FULFILL dice/economy/movement juice owner + experiential AC)本 GDD CR-3/4/5 + AC-55/56 已兑现,producer 回标;④ **OQ-VFX-10 RESOLVED(R-2):本地 `newCount` delta 派生取代 propagate**(CR-6),`EBuildDirection` 跨档依赖删除、无需 building 侧改动;**遗留**:building(2 字段)↔player-turn CR-3.5(3 字段)`OnBuildingChanged` 漂移仍须 producer 单独工单(非 #19 职责,见 CR-6 注)。
>
> 注:`technical-artist`/`performance-analyst`/`unreal-specialist` R-1 design-review 已咨询(full 模式)。

## Tuning Knobs

> 全部为**呈现层数据驱动旋钮**。固定动画时长(掷骰翻滚三阶段 art-bible §6.1、建房弹出/酒店合并 §6.3、UI 动效 §7.5)归 **art-bible** 拥有,#19 引用源真值、不复制为旋钮。

| 旋钮 | 来源 | 默认 | 安全范围 | 调太高 | 调太低 |
|------|------|------|---------|--------|--------|
| `T_hop_base` | F-1 | 0.16s/格 | 0.12~0.20 | hop 整体拖沓 | 棋子瞬移看不清路径 |
| `T_hop_total_min` | F-1 | 0.4s | 0.3~0.5 | 单格移动也偏慢 | 小步移动一闪而过 |
| `T_hop_total_max` | F-1 | 2.5s | 2.0~3.5 | 40 格移动拖沓 | 大步移动每格挤压到看不清(须联动抬 `T_stale_vfx`) |
| `K_coins` | F-2 | 2.0 | 1.5~3.0 | 中额租金即触粒子上限、失对数平滑 | 大额租金粒子也偏少、反馈弱 |
| `N_coins_min` | F-2 | 1 | 1~3 | 极小额也一堆粒子 | — |
| `N_coins_max` | F-2 | 15 | 12~20 | 超 60fps 预算(须与 Niagara 池协调) | 大额租金反馈封顶偏低 |
| `T_stale_vfx` | F-3 | 4.0s | 3.5~6.0 | 追赶迟钝、juice 久积压 | <`T_hop_total_max+1.5s` 则正常路径末条被误判过期(安全下界联动) |
| `N_max_vfx` | F-4 | **12(placeholder)** | 8~16 | 超 60fps 预算 | 8 人局频繁驱逐、juice 缺失 |
| `N_hop_max` | F-4 | **6(placeholder)** | 4~8 | hop 池超 60fps 预算 | 多人局 hop 频繁自驱逐 |
| `T_hop_protect` | CR-8 | =压缩 min 全程 | (跟随 `T_hop_single` 下限) | 当前 hop 久占视觉、追赶迟钝 | 保护窗口太短、快滚仍近瞬移 |
| `N_confetti_max` | F-4/RB-8 | **placeholder** | 标定冻结 | 胜利帧超 60fps 预算 | 胜利粒密偏低、情绪不足 |
| `N_max_umg` | F-4/OQ-VFX-3 | **placeholder(24~40)** | 标定冻结 | UMG overdraw 超预算 | 级联下徽章/浮字频繁 snap、丢动画 |
| `reduce_motion` | 全局 a11y(承接 HUD/#17) | Off | {Off,On} 布尔 | —(On=全 juice 瞬现/低强度) | —(Off=保留全部 juice) |

### 旋钮交互
- **`T_hop_total_max` ↔ `T_stale_vfx`**:`T_stale_vfx` 须 ≥ `T_hop_total_max + 1.5s`(掷骰+定格余量),否则长 hop 正常播放被辅判据误判过期(F-3)。调高 hop 上界须同步抬 `T_stale_vfx`。**加载期 `ensure(T_stale_vfx ≥ T_hop_total_max + 1.5)`**(RB systems R-3,DataAsset 校验,防两旋钮单独改漂移),违反 dev 崩溃暴露。
- **`N_coins_max` / `N_max_vfx` / `N_hop_max` / `N_confetti_max` / `N_max_umg` ↔ Niagara 池 / UMG**:均受 60fps 预算约束,**真实安全值须目标硬件性能标定后冻结**(标定前关联 AC 仅 Advisory);标定**必须含破产级联 + 胜利帧场景**,门控 P99 ≤33ms(OQ-VFX-3)。`N_hop_max` 独立于 `N_max_vfx`(双池,F-4),`N_max_umg` 独立于两 Niagara 池(UMG 不计 Niagara,§3),防级联驱逐饿死 hop / UMG 无界。**硬下界(R-6 systems Recommended)**:`N_max_vfx`/`N_hop_max`/`N_max_umg`/`N_confetti_max` 加载期 `ensure(>=1)`(置 0 致驱逐逻辑无元素可驱的退化未定义,DataAsset 校验崩溃暴露)。
- **`N`(hop 格数)实践上界(R-6 systems Recommended)**:F-1 `N` 的实践硬上界 = **棋盘环序总格数**(经棋盘1 只读查询,**禁硬编码 40**——Alpha 扩展棋盘即错,同 F-1 自建 hop path 注);`N>78` 走 Warning+clamp 兜底(多圈极端),但文档层上界须随棋盘1 总格数,非固定常量。
- **`reduce_motion`**:与 HUD/#17 **共享同一全局布尔**(不独立),避免三套 a11y 开关割裂;具体降幅归 OQ(随 HUD `/ux-design` 统一)。**副作用(performance P-6):On 时帧时间显著下降,可作低端硬件性能泄压阀**(不止 a11y 标签),随 OQ-VFX-5 flag 给 ux-designer。

## Visual/Audio Requirements

> **边界声明(enable-not-own)**:#19 拥**全部 juice 视觉资产 + Niagara System + 弹性/世界空间动画**;HUD(16)拥信息布局/UMG/绑定/玩家面板;地产卡 UI(17)拥卡片信息布局/状态视觉(色条/徽章/抵押灰化)。#19 **直接订阅 owner 事件**(不经 HUD/#17 转发),只读 payload、不访问 HUD/#17 widget 树。视觉真值引 `design/art/art-bible.md`(标 `[AB §X]`,art-director 逐条核实无臆造)。音效归音频(22),本节只列锚点。

### 1. #17/#19 拥有的视觉需求(V-Own)

**V-Own-01 掷骰·期待**:点击掷骰后两骰原地悬浮振动(振幅 ≤4px/~8Hz),奶白无强色;持续至 `OnDiceRolled` 到达。锚点=骰碗世界坐标。`[AB §1 原则2 / §2 状态B]`
> **触发源澄清(R-6 game-designer Recommended)**:期待振动的**起始**触发须经 **owner 事件**(非 #19 直接监听 UI 点击,守 CR-1「owner 事件唯一触发源」)——骰子3/回合2 须在掷骰意图发起时广播一个意图事件(如 `OnDiceRollStarted`/`RollPhase` 进入),#19 订阅之起振、收 `OnDiceRolled` 止振。**当前 Interactions 表骰子3 仅列 `OnDiceRolled`(掷骰结果),未列掷骰意图事件** → 若无意图事件则 #19 起振时刻无 owner 锚。**掷骰意图事件来源登记 OQ-VFX-16(R-7,独立追踪锚——非 OQ-VFX-1 范畴,后者仅 hop path)**,须架构期确认(owner=骰子3/回合2;MVP 可用回合2 `OnPhaseChanged(RollPhase)` 进入作起振锚,已具名)。**绝不让 #19 监听输入层**(违 CR-1 纯叶子)。
**V-Own-02 掷骰·翻滚**:`OnDiceRolled` 后随机轴高速旋转(**#19 独立 `FRandomStream` 驱旋转轴,不复用骰子3 权威流**,CR-9)+2~4 拖影 ghost(30% 不透明)+ 弹起抛物线+落桌溅粒(奶白 2~4);骰面 Off White/点 Dark Ink。`[AB §2 状态B / §3.1]`
**V-Own-03 掷骰·定格(基础)**:咔哒弹性 Ease-Out Back(1.0→1.15→1.0 ~200ms)+ 投影扩散;**两骰分别显 `Die1`/`Die2`(严禁拆 Total)**;Fortune Gold `#F4C430` Total 浮现上飘;**定序阶段抑制**双点强化(CR-3)。色盲:点数=圆形+数字双冗余。`[AB §4.3 / §1 原则2]`
**V-Own-04 双点庆祝(1~2 连)**:`bIsDouble ∧ ConsecutiveDoubles∈{1,2}` 非定序→金星粒子(6~10,Gold+Off White)+ 金边脉冲(~300ms)+「双点!再走一次」(Level3,Gold 底)。**色盲:星星/菱形形状**为主辨识(非仅色)。`[AB §4.3 / §4.6 / §3.1]`
**V-Own-05 第3连警告(入狱)**:`ConsecutiveDoubles==3` 非定序→感叹号「!」粒子(4~6,Property Red `#E03C31`)+ 红描边脉冲 + 短暂冷 vignette(~0.2s)+「前往监狱!」(Level2)。**色盲:与庆祝形状语汇完全不同**(感叹号/三角 vs 星星/圆),+文字冗余。`[AB §4.3 / §4.6 / §2 状态E]`
**V-Own-06 移动·逐格 hop**:棋子逐格抛物线弧位移(弧顶 ~0.3 格)+ 落地弹性压扁(~100ms)+ 每格落地尘土(奶白 2~4);节奏=F-1 `T_hop_single`;描边 1.5~2px。`[AB §2 状态B / §5.1]`
**V-Own-07 过 GO 高亮**:GO 格金色脉冲(Fortune Gold Additive ~0.3s)+ 上喷金币粒子(金圆 4~6)+ 可选发薪额浮现。`[AB §4.1 / §3.3]`
**V-Own-08 最终落地 juice**:棋子弹性放大(1.0→1.2→1.0 ~200ms)+ 落地格上凸高亮底边(scaleZ 1.05 ~300ms)+ 密集尘土(6~10);落他人地产加 ~30% Property Red 混粒预警。锚点=`OnPawnLanded` 权威位置(EC-12)。`[AB §6.2 / §4.3]`
**V-Own-09 金币飞溅(收租)**:`OnRentPaid` 从 Payer→Payee 发射 `N_coins`(F-2)金币 sprite,抛物线+重力(0.5~1s);金币=Fortune Gold 圆币 + 简单 Specular;到 Payee 触小金星爆发。**锚点解析(R-5 M1,CR-2 命名例外)**:**起点=收租地产格 `TileIndex` 的 tile-center**(Payer 落地处,守 CR-2 默认);**终点=Payee 棋子世界坐标**(经 `Payee` PlayerId 查 pawn registry 取静止 pawn `GetActorLocation()`,CR-2 命名例外;Payee 恰移动中则退化为 Payee 当前格 tile-center,EC-16)。**(R-6:pawn registry 查询接口来源待架构裁定,OQ-VFX-15;AC-51b/51c 经此门控)****色盲:硬币形状+飞行方向语义**。`[AB §1 原则2 Design Test 优先保留 / §6.4 / §7.1]`
**V-Own-10 数字跳动(`OnCashChanged`)**:浮现「+200/-150」上飘放大淡出;正向(薪水/收租)=Gold、负向(租/税)=Property Red;时长=**HUD F-1 `T_counter` 同口径**(防视觉撕裂)+ 正向金星/负向「-」粒子。**色盲:+/- 符号**为主辨识。`[AB §2 状态B / §4.3]`
**V-Own-11 破产清算**:`OnBankruptcyDeclared`→① Debtor 棋子去饱和消散(Sat 1.0→0 ~1.5s+降透明至 40%,灰粒 `#CCCCCC` 飘散)② 屏幕黑白 vignette(~0.5s 入/1s 留/0.5s 出)③ 存活棋子幸灾乐祸弹跳(~200ms)。**色盲:去饱和对所有类型一致**。`[AB §2 状态G / §4.3]`
**V-Own-12 建房弹出**:`OnBuildingChanged` 且 `newCount∈[1,4] ∧ newCount>cache`(本地派生 Build,CR-6)→房屋向上弹出(scaleZ 0→1.3→1.0 Ease-Out Back ~300ms,绿 `#3CB54A` 圆角三角顶)+ 星星粒子(6~8,Gold+Green)+ 地块上凸 flash。**色盲:三角屋顶形状**。`[AB §6.3 / §1 原则2 Design Test 优先保留 / §4.3]`
**V-Own-12b 卖房/降档(Issue 1)**:`OnBuildingChanged` 且 `newCount<cache`(本地派生 Sell,卖房/破产拆除)→房屋向下收缩消失(~200ms)+ **无金星庆祝**(Gold 仅奖励,art-bible §4.3 — 卖房非奖励)+ 轻量灰尘。**缓存 miss 兜底退化为非方向"档位变更"snap**(直呈新 house_count,EC-15)。`[AB §6.3 / §4.3]`
**V-Own-13 酒店合并**:`OnBuildingChanged` 且 `newCount==5 ∧ newCount>cache`(本地派生 Build,CR-6)→四房向心收缩(~200ms)+ 酒店旋转升起(Y 360°/scaleZ 0→1.2→1.0 ~600ms,红 `#E03C31` 高 50%弧檐)+ 强闪光辐射(8~12 光线)+ 落地星星(10~14,Gold+Red)。**色盲:高度+弧檐形状 + 旋转动作**。`[AB §6.3 / §3.3 / §4.3]`
**V-Own-14 买地金色脉冲**:`OnOwnershipChanged(None→PlayerId)`→地块金色波纹扩散(Fortune Gold Additive ripple ~0.5s)+ 所有权徽章弹出(scale 0→1.2→1.0 ~200ms,玩家色+编号;持久显示归 #17/棋盘层)。**色盲:波纹形状+玩家编号**。`[AB §4.3 / §6.2 / §5.2]`
**V-Own-14b 破产易主清算(R-3 Finding B,承接 bankruptcy L191/194)**:`OnOwnershipChanged(PlayerId→PlayerId)`(收租破产 in-kind 逐格易主)→地块徽章从旧主色**翻转/擦过**到新主色(~0.3s,**无金色奖励脉冲**——易主非买地获得感,art-bible「Gold 仅奖励」)+ 轻量清算尘。priority=**Economic**(级联 churn,F-4 一般池可驱逐)。**色盲:徽章玩家编号变更 + 方向(旧→新主)**。`[AB §4.3 / §6.2]`
**V-Own-14c 破产回退无主(R-3 Finding B)**:`OnOwnershipChanged(PlayerId→None)`(银行破产逐格回退)→徽章**收缩消失**为无主灰(~0.3s,无金星)+ 轻量灰尘。priority=**Economic**。**色盲:徽章消失 = 无主语义,与易主(14b)的「换色」区分**。`[AB §4.3 / §6.2]`
**V-Own-15 抵押反馈**:`OnMortgageChanged(true)`→灰色横向 wipe(左→右 ~0.4s)过渡到去饱和+斜纹(持久态归 #17/棋盘层);`false`(赎回)反向恢复 wipe。**色盲:去饱和灰+斜纹纹理**。`[AB §6.2 / §4.3]`
**V-Own-16 胜利**:`OnGameWon`→进 art-bible 状态 F(**粒子密度全局最高上限,受 `N_confetti_max` 约束非无界**,RB-8):Confetti 从屏幕四周涌入(地产 8 色组色盘 + 40% Gold,旋转重力下落,总活跃粒 ≤`N_confetti_max`)+ 3~5 发 fireworks(Gold 主体,每发粒数标定冻结)+ 胜者棋子放大至中央(scale→2.5 ~0.8s)+ 王冠降落弹性 + 非胜者灰半透消散 + 金色统计卡弹出(数值由 HUD/#17/破产9 供)+ 暖金 post process(~4500K,归 technical-artist)。**色盲:粒子密度+棋子放大+王冠锯齿形状+文字卡**多维冗余。`[AB §2 状态F / §3.2 / §4.3]`

### 2. 音频锚点(归音频22,#19 只列触发时刻+上下文,不拥音效)

| ID | 触发时刻 | 对应 V-Own | 上下文(非规格) |
|----|---------|-----------|------|
| AA-01 | 掷骰翻滚起 | V-Own-02 | 骰子滚动物理声 |
| AA-02 | 骰子定格 | V-Own-03 | 咔哒落定声 |
| AA-03 | 双点庆祝 | V-Own-04 | 轻快庆祝音 |
| AA-04 | 第3连警告 | V-Own-05 | 警告提示音(明显区别庆祝) |
| AA-05 | hop 每格落地 | V-Own-06 | 玩具棋子落桌「哒」 |
| AA-06 | 过 GO | V-Own-07 | 发薪入账金币声 |
| AA-07 | 最终落地 | V-Own-08 | 更重「砰」落地声 |
| AA-08 | 金币飞溅起 | V-Own-09 | 金币飞行/碰撞声 |
| AA-09 | 金币到 Payee | V-Own-09 | 收入入袋音 |
| AA-10 | `OnCashChanged` 正向 | V-Own-10 | 轻快正向提示 |
| AA-11 | `OnCashChanged` 负向 | V-Own-10 | 轻微扣钱提示(不沉重) |
| AA-12 | 破产消散起 | V-Own-11 | 「哇哦」失落音(卡通夸张) `[AB §2 状态G]` |
| AA-13 | 房屋弹出定格 | V-Own-12 | 咔哒建造音 |
| AA-14 | 酒店升起 | V-Own-13 | 雄壮升级建造音 |
| AA-15 | 买地脉冲 | V-Own-14 | 地契购入确认音 |
| AA-16 | 抵押 wipe | V-Own-15 | 「锁定」沉闷音 |
| AA-17 | 胜利 Confetti | V-Own-16 | 胜利 fanfare(整局最强) |
| AA-18 | 烟花每发 | V-Own-16 | 烟花爆炸声(3~5 次错开) |

### 3. juice 视觉资产清单(供 `/asset-spec`,命名遵 art-bible §8.2)

`NS_vfx_dice_anticipate_loop`(V-01)、`NS_vfx_dice_roll_trail`(V-02)、`NS_vfx_dice_land_base`(V-03)、`NS_vfx_dice_double_celebrate`(V-04)、`NS_vfx_dice_double_warning`(V-05)、`T_vfx_icon_exclamation_64`(V-05)、`T_vfx_icon_star_64`(V-04/12)、`NS_vfx_pawn_hop_step`(V-06)、`NS_vfx_pawn_hop_go_pulse`(V-07)、`NS_vfx_pawn_land_impact`(V-08)、`NS_vfx_coin_splash_arc`(V-09)、`T_vfx_coin_sprite_64`(V-09/07)、`NS_vfx_cash_number_positive`/`_negative`(V-10)、`NS_vfx_bankrupt_dissolve`(V-11)、`NS_vfx_bankrupt_vignette`(V-11)、`NS_vfx_house_popup_stars`(V-12)、`NS_vfx_hotel_merge_burst`(V-13)、`NS_vfx_property_buy_pulse`(V-14)、`NS_vfx_property_mortgage_wipe`(V-15)、`NS_vfx_victory_confetti`/`_fireworks`/`_crown_drop`(V-16)、`T_vfx_crown_sprite_128`(V-16)。

> **资产前缀订正(RB tech-art R-6 + R-3 补漏,asset-spec 时落实)**:`NS_vfx_bankrupt_vignette`、`NS_vfx_cash_number_positive`/`_negative`、**`NS_vfx_property_mortgage_wipe`(R-3 第 4 个错漏:V-15 灰色横向 wipe = UMG/材质动画,非 Niagara)** **不应是 Niagara System(`NS_`)**——vignette 是全屏 UMG/Post Process Material(`WBP_`/`M_`),数字浮字是 UMG Widget(`WBP_`),mortgage wipe 是 `M_`/`WBP_`。**`NS_vfx_pawn_hop_go_pulse`(V-07 GO 金色脉冲)前缀待 asset-spec 核**(若材质 additive flash 则 `M_`/`MPC_`,若一次性粒子 burst 则 `NS_` 合理)。`/asset-spec` 时按真实呈现层归类前缀(**UMG/材质类不走 Niagara 池——F-4 `N_max_vfx` 只计真 Niagara System 活跃实例,上述 UMG 类不计入,防池预算少算 3~4 slot**)。`T_vfx_icon_*` 的 `icon` category 须对齐 art-bible §8.2 category 词表(`ico` 而非 `icon`)。

### 4. 核心设计约束

| 约束 | 来源 |
|------|------|
| **粒子密度全局上限=胜利(V-Own-16)**——所有其他 juice 密度须低于它 | `[AB §2 状态F]` |
| Design Test 裁剪优先保留:收租金币(V-09)+ 建房落地(V-12) | `[AB §1 原则2]` |
| 双点庆祝 vs 警告须**形状/图标冗余**(非仅色) | `[AB §4.6]` |
| 粒子贴图 64×64/128×128 关 Mip;圆润卡通禁锐角主体/禁写实 PBR | `[AB §8.3 / §9.2]` |
| 去饱和=破产独有信号,不与其他状态重叠 | `[AB §2 状态G]` |
| Fortune Gold 仅奖励/收入;Property Red 仅危险/扣钱 | `[AB §4.3]` |

> **📌 Asset Spec Flag(#19)**:Visual/Audio 已定义。art-bible approved 后跑 `/asset-spec system:vfx-feedback` 产出上述 24 项资产的 per-asset 规格 + 生成 prompt。
>
> 注:`art-director` 已咨询(视觉系统必需)。`audio-director` 未咨询(lean;音效归 #22,本节只列锚点)。

## UI Requirements

> #19 是 VFX 系统,**无任何交互式 UI、无输入处理**(不含按钮/菜单/可聚焦元素)。本节仅厘清 #19 贡献的**屏幕空间叠加 VFX 层**与 HUD/#17 的层级关系。

### #19 贡献的屏上元素(非交互,叠加 VFX)
| 元素 | 内容 | 空间 | 来源 V-Own |
|------|------|------|-----------|
| 数字跳动浮字 | `OnCashChanged` +/- 金额浮现 | 屏幕/世界 billboard | V-Own-10 |
| 文字提示 | 「双点!再走一次」/「前往监狱!」 | 屏幕空间 | V-Own-04/05 |
| 破产 vignette | 屏幕边缘黑白渐变 | 屏幕全屏叠层 | V-Own-11 |
| 胜利覆盖层 | confetti/fireworks/王冠/统计卡动画 | 屏幕全屏叠层 | V-Own-16 |
| 暖金 post process | 胜利色温 | 全屏后处理 | V-Own-16(归 technical-artist) |

### 层级 / 输入要求
- **无输入**:#19 不接收任何玩家输入;所有触发来自 owner 事件(CR-1)。
- **不阻挡交互**:屏幕空间叠加 VFX 层须 **`SelfHitTestInvisible`/不吃点击**——juice 永不拦截玩家对棋盘/HUD/卡片的操作(即使胜利全屏 confetti 也不阻塞,胜利态交互归 HUD/#20)。
- **z-order 与 HUD 协调**:世界空间 juice 在棋盘层之上;屏幕空间 juice 层与 HUD/#17 的 z-order 须架构期厘清(数字浮字/文字提示在 HUD 面板之上可读;vignette/confetti 在最顶但不遮关键信息)——归架构 ADR。
- **reduce_motion**:叠加层同受全局 a11y 布尔(CR-10)。

### 边界
- 统计卡**数值内容**由 HUD/#17/破产9 供,#19 只拥**弹出动画**;数字浮字**金额**取自 owner payload,#19 不读 HUD widget。

> **📌 UX Flag(轻)— #19**:#19 无交互 UI,**不需**逐屏 `/ux-design`;但**屏幕空间叠加 VFX 层与 HUD/#17 的 z-order 协调**须在 Pre-Production 随 HUD `/ux-design` + 架构 ADR 一并厘清(防 juice 遮挡关键信息或吃点击)。

## Acceptance Criteria

> **类型铁律**:[Logic]=纯函数公式/状态机转换/payload 校验/可注入 spy(headless `-nullrhi` 确定性);[Integration]=多系统联动;[Advisory]=渲染层(粒子外观/动画时长/颜色/像素——`-nullrhi` 无 Niagara 探针 + 帧抖动 ±16ms 使时刻断言 flaky)或 code-review 软约束(BP 无语言级拦截)。**负向 spy 须命名可真违规的注入边界**(非 vacuous)。`qa-lead` 已咨询(lean·H 高风险节)。

### A. F-1 Hop 节奏(CR-4 / EC-7)
- **AC-01** [Logic] GIVEN `T_hop_base=0.16,min=0.4,max=2.5,N=7`;WHEN 计算;THEN `T_hop_total==1.12s`(未触界),误差 <1ms。
- **AC-02** [Logic] GIVEN `N=24`;THEN `T_hop_total==clamp(3.84,..,2.5)==2.5`,`T_hop_single==2.5/24≈0.104s`。
- **AC-03** [Logic] GIVEN `N=2`;THEN `T_hop_total==clamp(0.32,0.4,..)==0.4`,`T_hop_single==0.4/max(2,1)==0.2s`。
- **AC-04** [Logic](N==0 越界)GIVEN payload `N=0`;THEN **不播 hop**、`E_cur_vfx` 不变、`UE_LOG(Warning)`、不崩溃;`T_hop_single` 分母 `max(0,1)=1` 无除零。
- **AC-05** [Logic](N<0,R-2 可证伪化)GIVEN `N=-3`;THEN 公式层 `N_eff=abs(-3)=3`,`T_hop_total` 用 3 计算(=clamp(0.48,0.4,2.5)=0.48s)+ Warning,**不静默 clamp 掩盖**;**断言:若实现用裸 N 则 `clamp(0.16×-3,0.4,2.5)=clamp(-0.48,..)=0.4` 被 spy 抓出(0.48≠0.4)FAIL**。
- **AC-06** [Logic](N>78 极端)GIVEN `N=100`;THEN Warning + `T_hop_total` clamp 到 max,不崩溃。
- **AC-06b** [Advisory](hop 实录节奏)录帧 N=7 总时长≈1.12s/每格≈0.16s(±20ms)。证据 `vfx-hop-timing-N7.png`。

### B. F-2 金币飞溅粒子量(CR-5 / EC-6)
- **AC-07** [Logic] GIVEN `K_coins=2.0,Amount=50`;THEN `floor(2.0×log2(51))==11`。
- **AC-08** [Logic] GIVEN `Amount=200,max=15`;THEN `floor(2.0×log2(201))==15`(触顶)。
- **AC-09** [Logic] GIVEN `Amount=2000`;THEN `==15`(clamp 顶,预算保护)。
- **AC-10** [Logic](Amount==0 防御,RB-6 重写)GIVEN #19 注入 `Amount=0`(经济5 AC-5 本不应广播);THEN `ShouldSpawnCoins(0)==false`(spy 命中**防御短路分支**)+ **金币发射接口调用==0**——**防御须在进入 F-2 前短路,不得靠 `N_coins_min=1` 隐性兜底**(否则 `N_coins(0)=clamp(0,1,15)=1` 会误发 1 粒)+ Warning。
- **AC-11** [Logic](Amount<0 非法)GIVEN `Amount=-100`;THEN Error + 不发射,**不静默 clamp**。
- **AC-11b** [Advisory](视觉)Amount=50 截图≈11 粒(±1),飞弧 Payer→Payee。证据 `vfx-coin-splash-11.png`。

### C. F-3 juice 过期判定(CR-8 / EC-2 / EC-5)
- **AC-12** [Logic](主判据)GIVEN `E_cur_vfx=5`、`E_effect(e)=5` Playing;WHEN `OnTurnStarted`(→6);THEN `expired_vfx(e)==true`(6>5),非 critical 转 SnapDone 丢中间帧、不回调 owner。
- **AC-13** [Logic](同回合未过期)GIVEN `E_cur=5,E_effect=5,elapsed=2.0<4.0`;THEN `expired==false`,继续 Playing。
- **AC-14** [Logic·门控 OQ-VFX-2 ADR(`IGameClock` DI 须架构期确认接口存在,同 HUD OQ-HUD-2 模式)](辅判据软上界)GIVEN 回合未推进、`elapsed=4.5>T_stale_vfx=4.0`(注入可控 `IGameClock` mock,非壁钟);THEN `expired==true`→SnapDone。
- **AC-15** [Logic](E_cur_vfx 独立)GIVEN 连收 3 次 `OnTurnStarted`;THEN `E_cur_vfx` 单调 +3,**不引用 HUD 内部 E_cur**(平级解耦)。
- **AC-15b** [Logic](双点链共享 epoch 不被 epoch 推进误过期,RB-1)GIVEN 同玩家双点链:1 次 `OnTurnStarted`(`E_cur_vfx`5→6)+ 链内 3 次 `OnPhaseChanged(RollPhase)`(`OnTurnStarted` **不重发**);链内到达的 hop/收租 juice `E_effect==6`、`elapsed<T_stale`;THEN 链内 juice 的 **epoch 左支** `E_cur>E_effect==false`(6>6 假),**不被链内额外回合的 epoch 推进误判过期**;直到下一玩家 `OnTurnStarted`(→7)才 epoch 左支 expired。(守 F-3 epoch=行动权移交粒度;**注:elapsed 右支独立,见 AC-15c**)
- **AC-15c** [Logic·门控 OQ-VFX-2 `IGameClock` DI](elapsed 软上界在 epoch 内仍可触发,R-2 补 OR 右支覆盖)GIVEN 链内 juice `E_effect==6`、`E_cur_vfx==6`(epoch 未推进)、但 `elapsed=4.5>T_stale_vfx=4.0`(注入可控 `IGameClock` mock,非壁钟);THEN `expired_vfx==true` **经 elapsed 右支**→ SnapDone(catch-up backstop:已 Playing >T_stale 的链内 juice 视觉上确已陈旧,snap 最终态是正确追赶,非 bug)。(守 F-3 `expired=(E_cur>E_effect) OR (elapsed>T_stale)` 的 **OR 两支均覆盖**:AC-15b 测左支假、AC-15c 测右支真)
- **AC-16** [Logic](读档重置)GIVEN 读档前 `E_cur_vfx=5` + 2 Playing;WHEN 读档(重置 0);THEN 旧效果立即 expired、不回放、从 0 起计。
- **AC-17** [Logic](旋钮下界满足)GIVEN `max=2.5,T_stale=4.0`;THEN `4.0>=2.5+1.5`(等号成立),正常 hop 不被误判。
- **AC-17b** [Logic](下界违规可 FAIL 回归)GIVEN `max=3.5,T_stale=4.0`;THEN 断言 `T_stale>=max+1.5` **FAIL**(4.0<5.0)——防调高 hop 上界忘抬 T_stale。

### D. F-4 并发上限 + 优先级驱逐(CR-8 / EC-3 / EC-11)
- **AC-18** [Logic](满载驱逐最低优先级最旧者,R-2 fixture 改 Economic)GIVEN `N_max=12` 满载全 **Economic**(破产级联收租/数字,一般池可实际达此并发;**非 Motion=掷骰 single-instance 不可达 12**);WHEN 新 Economic 到;THEN 最旧 Economic→SnapDone(非静默)、新入队、活跃 ≤12。
- **AC-19** [Logic](Critical/Social 不被驱逐,R-2 fixture 改 Economic)GIVEN 满载含 1 Critical+11 **Economic**;WHEN 新 Economic 到;THEN 驱逐最旧 Economic、Critical 不被驱逐(镜像 F-4 L176 worked example)。
- **AC-20** [Logic](同级选最旧,R-2 fixture 改 Economic)GIVEN 6 **Economic** 按 t0..t5;WHEN 新 Social 到;THEN 驱逐 t0(creation_time 最早,Economic<Social)。
- **AC-20b** [Logic](Motion 在场时最先驱逐,R-2 补 Motion 覆盖)GIVEN 11 Economic + 1 Motion(掷骰 juice 在一般池,双池后 hop 已移出);WHEN 新 Social 到;THEN **先驱逐 Motion**(Motion<Economic<Social,一般池最低 tier)、Economic 不动。(守双池后 Motion 仍是一般池最低优先级、有独立驱逐覆盖)
- **AC-21** [Advisory](N_max/N_hop_max 性能门)profiler 在**目标硬件**(规格待 OQ-VFX-3 钉,如 Steam P50)跑 12 一般池 + 6 hop 并发 + **破产级联(14 格连发)+ 胜利帧**场景;门控 **平均 ≤16.6ms 且 P99 ≤33ms**(非仅 120 帧均值,RB-6 + performance P-2);**标定前必 Advisory**(headless 无 Niagara cost 探针)。证据 `vfx-perf-cascade-profiler.png`。**升级闭环(非开放承诺)**:OQ-VFX-3 resolution 时 **performance-analyst 必补 `N_active_general≤N_max` + `N_active_hop≤N_hop_max` + `N_active_umg≤N_max_umg`(R-6 补 UMG 池枚举)[Logic] 不变式 AC 并回标**,且 **#19 相关 story 不得在性能标定前置 Done**(hard gate,不由本 Advisory 放行,performance P-7)。

### E. 状态机(CR-1/2 / EC-1/4)
- **AC-22** [Logic](Idle→Playing)GIVEN 订阅活跃、锚点可解析;WHEN `OnRentPaid`;THEN 实例 Idle→Playing、`N_active`+1、不回调 owner。
- **AC-23** [Logic](Playing→Idle 自然完)GIVEN Playing、注入"动画完成"信号;THEN →Idle、`N_active`-1、释放。
- **AC-24** [Logic](Playing→SnapDone 过期)GIVEN expired;THEN →SnapDone→Idle、`N_active`-1、**中间帧渲染接口调用==0**(spy)。
- **AC-25** [Logic](reduce_motion 直通)GIVEN `reduce_motion=On`、`OnBuildingChanged(5,3)`;THEN **不经 Playing** 直接 SnapDone(测逻辑路由非视觉)。
- **AC-26** [Logic](锚点失败保 Idle)GIVEN 坐标解析失败;THEN 不进 Playing、保 Idle、`N_active` 不变、Warning、其他并发正常。
- **AC-27** [Logic](订阅常驻)GIVEN 初始化、整局无事件;THEN owner delegate `IsBound()==true`。

### F. CR-3 掷骰 juice(Die1/Die2 + 双点语境 + 定序抑制)
- **AC-28** [Logic](Die1/Die2 值断言)GIVEN `{Die1=3,Die2=5,Total=8}`;THEN 两骰面分别读到 `3`/`5`(**值断言**:若误用 `Total/2=4` 则读到 4≠3,FAIL)。
- **AC-28b** [Advisory·code-review·门控 OQ-VFX-4](禁从 Total 拆骰面路径,RB-6)reviewer 静态确认无 `Die1=Total/2` 等拆解路径;**BP 无语言级路径 spy 注入** → code-review 软约束;架构选 C++ 私有 `ParseDiceResult(payload)` 可注入 mock 升 [Logic]。证据 `vfx-dice-split-review.md`。
- **AC-29** [Logic](庆祝路由)GIVEN `bIsDouble,ConsecutiveDoubles=1` 非定序;THEN 庆祝路径 spy==1、警告路径==0。
- **AC-30** [Logic](警告路由)GIVEN `ConsecutiveDoubles=3` 非定序;THEN 警告==1、庆祝==0。
- **AC-31** [Logic](定序抑制)GIVEN `bIsDouble` + 定序语境;THEN 仅基础 juice、庆祝==0、警告==0(dice OQ-T-Dice-1)。
- **AC-32** [Logic](语境缺失降级)GIVEN `bIsDouble`、`ConsecutiveDoubles` 拿不到(null);THEN 降级基础 juice + Warning、**不臆断双点链**(owner 单源)。
- **AC-32b** [Logic](跨回合旧缓存不污染——乱序时序窗口,R-3 game-designer;**R-4 修非空化**)GIVEN 玩家 A 回合 `ConsecutiveDoubles=2` 缓存,A 回合结束、玩家 B `OnTurnStarted` 到(#19 重置缓存 `ConsecutiveDoubles=0`);WHEN B 首次 `OnDiceRolled(bIsDouble=true)` **先于** B 的 `OnPhaseChanged(RollPhase)` 携新值到达(CR-3 L62 钉的危险窗口——新玩家首掷在其 `OnPhaseChanged` 携新值**前**到;#19 取最近缓存值);THEN 双点庆祝 spy==0(取 `OnTurnStarted` 重置后的 0,而非 A 残留的 2)。**断言:若 #19 不在 `OnTurnStarted` 重置缓存而沿用上家旧值,则此乱序窗口内 B 首掷读到 A 残留的 `ConsecutiveDoubles=2` → `bIsDouble=true` 触庆祝/警告路由,spy>0 FAIL**(守 CR-3 owner 单源 + 缓存跨回合卫生)。**注(R-4):必须测「`OnDiceRolled` 先于 `OnPhaseChanged`」——若测「之后」(AC-32c),则 `OnPhaseChanged` 已把缓存设 0,重置与否结果相同 = vacuous,无法证伪 `OnTurnStarted` 重置的必要性。**
- **AC-32c** [Logic](正常时序对照,R-4 补)GIVEN 同上,A 回合 `ConsecutiveDoubles=2` 缓存,B `OnTurnStarted` 到;WHEN B 的 `OnPhaseChanged(RollPhase, ConsecutiveDoubles=0)` **先于** B 首次 `OnDiceRolled(bIsDouble=true)` 到达(正常时序);THEN 双点庆祝 spy==0(读 `OnPhaseChanged` 携的 0)。(此为对照路径,正常时序下任何实现均应正确;真正守护 `OnTurnStarted` 重置必要性的是 AC-32b 乱序场景。)
- **AC-33** [Advisory](三阶段 feel)录帧 期待→翻滚→定格,两骰显 3/5、Total=8。证据 `vfx-dice-roll-phases.png`。
- **AC-34** [Advisory](庆祝 vs 警告形状色盲)Coblis 下 星/圆 vs 感叹号/三角 形状可区分(非仅色,§4.6)。证据 `vfx-double-shapes-compare.png`。

### G. CR-8 critical 过期 snap(EC-2/11)
- **AC-35** [Logic](破产 critical snap,R-5 R6 澄清 headless 断言)GIVEN `OnBankruptcyDeclared` Playing、expired;THEN 转 SnapDone(**`SnapDone spy==1`** 状态机路由 + **去饱和材质参数 `SetScalarParameterValue(Desaturate,1.0)` 调用==1**,二者均可 headless spy)、绝不静默全丢。**(渲染层"去饱和视觉呈现"本身=Advisory 截图 AC-52,本条只验逻辑路由 + 材质参数调用,非视觉输出——避 headless false-green)**
- **AC-36** [Logic](胜利 critical snap,R-5 R6 澄清)GIVEN `OnGameWon` expired;THEN 转 SnapDone(`SnapDone spy==1` + 胜利覆盖层最终态**设置接口调用==1**,headless spy);**视觉呈现=Advisory(AC-53),本条只验路由+接口调用。**
- **AC-37** [Logic](critical 被驱逐保护)GIVEN 满载,轮到驱逐 Critical;THEN Critical 不被驱逐、改驱逐 Motion(与 AC-19 双路径守护)。
- **AC-38** [Logic](非 critical 整条跳不回灌)GIVEN Motion hop expired;THEN 整条跳过、`N_active`-1、**对 owner/框架回调==0**(注入 owner mock 验非阻塞)。

### H. CR-9 RNG 隔离(EC-13)
- **AC-39** [Advisory·code-review] reviewer 静态扫描 #19 全 juice 随机路径,确认源自 **#19 自建独立 `FRandomStream`**,无骰子3 权威流引用/别名(复用=S2)。证据 `vfx-rng-isolation-review.md`。BP 无语言级拦截;C++ 私有封装+DI spy 可升 [Logic]。

### I. CR-11 enable-not-own 负向
- **AC-40** [Logic](#19 无 HUD/#17 widget 成员引用,RB-6 重写为反射检查)GIVEN #19 Actor 初始化后;THEN 反射枚举 `#19` 成员变量,**无 `WBP_HUD`/`WBP_PropertyCard` 类型成员**(`GetMemberVariablesOfType(WBPHUD)==0`)——可 headless 真 FAIL 的边界(原"注入 HUD mock 验写接口==0"在 CR-11「#19 不持 HUD 引用」下无注入点 = vacuous,已弃)。enable-not-own 真门为 HUD/#17 侧镜像 code-review(AC-41)。
- **AC-41** [Advisory·code-review](HUD/#17 不持 #19 引用)reviewer 验 HUD/#17 无 #19 成员/调用——**#19 侧测此为 vacuous**(永真),真门在 HUD/#17 侧(AC-40 镜像)。
- **AC-42** [Logic](直订 owner 不经转发)GIVEN 经济5 广播 `OnRentPaid`;THEN #19 直接从经济5 delegate 收(自身 `IsBound`)、HUD/#17 不存在时仍触发。

### J. 读档重建(EC-5)
- **AC-43** [Integration·门控 OQ-VFX-2 ADR(C++/BP 路径择一,RB unreal B-2)](读档重绑去重)GIVEN 已绑 12 owner delegate(按 delegate 数,见 AC-43b 枚举);WHEN 读档重建;THEN 先解绑(`RemoveDynamic`/BP `Unbind`)再绑、**同 handler 触发计数==1 非 2**(同 HUD AC-48)、不崩溃。**注**:计数注入点须 C++ 中介层(BP 路径无标准 spy 注入 → 该情形降 Advisory·code-review,架构 ADR 选定后定档)。
- **AC-43b** [Logic](EndPlay 解绑防野指针,RB unreal B-1;R-3 修 false-green 口径)GIVEN #19 已绑 **12 个 owner delegate 绑定**(R-3:按 **delegate** 数非 owner **系统**数——dice `OnDiceRolled`×1 + 回合2 `OnPhaseChanged`/`OnTurnStarted`/`OnGameWon`×3 + 移动4 `OnPawnMoveStarted`/`OnPawnLanded`×2 + 经济5 `OnRentPaid`/`OnCashChanged`/`OnBankruptcyDeclared`×3 + 地产6 `OnOwnershipChanged`/`OnMortgageChanged`×2 + 建房8 `OnBuildingChanged`×1 = **12**);WHEN #19 **`EndPlay`**(关卡卸载/提前销毁/PIE 停止;**R-4:仅 `EndPlay`,禁 `BeginDestroy`——GC 期 owner 可能已销毁,见 EC-5b**);THEN **全部 12 个 `RemoveDynamic`/`Unbind` 被调用**,**且 spy 逐项核对被解绑的 handler `FunctionName` 恰为上列枚举 12 项(逐 handler 名匹配,非仅计数==12)**——此断言同时抓两类 false-green:① 「漏解绑=数 owner 系统 6」(原「==6」buggy 实现 PASS);② **「枚举漂移」**:若 OQ-VFX-13 裁定 V-Own-11 改订破产9 `OnPlayerBankrupt`(替换经济5 `OnBankruptcyDeclared`),delegate 总数仍 12,**纯计数 spy 会静默放过「漏解绑旧 `OnBankruptcyDeclared` 野指针」**,而逐 handler `FunctionName` 断言抓出该 handler 名缺失/多出 FAIL;`IsBound`(各 owner→#19 handler)==false;后续 owner 广播**不触达已销毁 #19**(注入 owner mock 广播,验 #19 handler 调用==0、不崩溃)。**(R-7:逐 handler `FunctionName` 验证由注移入 THEN 权威断言——验证义务不落注释落断言,[[gdd-fix-lands-in-comments-not-spec]] 红线;OQ-VFX-13 裁定改订事件时须同步替换枚举列表中该项名。)**
- **AC-44** [Logic](不回放历史)GIVEN 读档前 3 历史 juice;THEN 历史实例化==0、从快照直呈最终态、`E_cur_vfx` 重置 0。
- **AC-45** [Logic](重绑后正常响应)GIVEN 重建后(缓存已从快照种子化,cache[5]=0);WHEN 新 `OnBuildingChanged(tile=5, newCount=1)`(派生 Build);THEN 正常起 juice、旧 handler 不重复(AC-43 去重保证)。

### K. EC 覆盖
- **AC-46** [Logic](EC-8 同帧多事件)GIVEN 同帧 4 个 `OnCashChanged`;THEN 各独立实例(ID 不同)、受 F-4 约束、不串台。
- **AC-47** [Logic](EC-10 定序抑制独立覆盖,RB-6 去同义重复)GIVEN **定序阶段连续两次 `bIsDouble`**(定序不进双点链,player-turn CR-2);THEN 两次均**仅基础掷骰 juice**、庆祝/警告 spy 累计==0(AC-31 测单次,本条测定序链内**多次**双点仍不累强化——独立覆盖,非 AC-31 同义重复)。
- **AC-48** [Logic](EC-12 hop 以 owner 落点为准)GIVEN 预测落 X 但 `OnPawnLanded(Y≠X)`;THEN snap 到 owner Y、**不写 owner 位移**(spy 无移动4 写调用)。
- **AC-49** [Advisory](EC-4 reduce_motion 全 juice 视觉)截图各 juice 瞬现/低强度、共享全局 a11y 布尔。证据 `vfx-reduce-motion-all.png`。

### L. V-Own 视觉 Advisory 组
- **AC-50** [Advisory](V-04/05 色盲)Coblis 下庆祝(星/圆)vs 警告(!/三角)仅靠形状可区分(§4.6)。证据 `vfx-colorblind-dice.png`。
- **AC-51** [Advisory](V-09 方向语义)金币飞弧 Payer→Payee 不反向。证据 `vfx-coin-direction.png`。
- **AC-51b** [Logic·门控 OQ-VFX-15 pawn registry 接口(R-6:`Payee` PlayerId→pawn 查询源须架构期确认存在,同 AC-14/15c `IGameClock` DI 门控模式;源选定〔AGameState 内置 / movement4 / board-manager〕前 spy 注入点抽象、不可早写死)](M1 收租金币锚点解析 + mid-hop 退化,CR-2 命名例外)GIVEN `OnRentPaid(Payer,Payee,Amount=50,TileIndex=12)`、Payee pawn **静止**(非 hop 中途);THEN 金币**起点==`GetTileWorldTransform(12)` tile-center**、**终点解析经 `Payee` PlayerId 查 pawn registry 取 `GetActorLocation()`**(命名例外路径 spy==1,**spy 注入边界=架构 ADR 选定的 registry lookup 接口,OQ-VFX-15**);WHEN 同输入但 **Payee 恰在 hop 中途**(`bIsMoving==true`);THEN 终点**退化为 Payee 当前格 tile-center**、**mid-hop `GetActorLocation()` 调用==0**(spy)。**断言:若实现对 mid-hop Payee 仍调 `GetActorLocation()` 则取到插值半空坐标、spy 抓出调用==1 FAIL**(守 CR-2 命名例外的边界=危害域内仍禁取实时位置)。
- **AC-51c** [Logic·门控 OQ-VFX-15](EC-16 第三退化:Payee pawn 完全解析失败,R-6 game-designer)GIVEN `OnRentPaid(...,Payee,...)` 但 `Payee` PlayerId 查 pawn registry **返回 null**(注入 registry mock 返回失败)且 Payee 当前格位亦无法推算;THEN 终点**退化为收租地产格 `TileIndex` tile-center(起点)附近小幅爆发**(spy==1)+ Warning、**不崩溃**。**断言:若实现对 registry-null 不退化而裸调 `GetActorLocation()`(null deref)则崩溃/无 fallback 爆发 spy==0 FAIL**(守 EC-16 三退化路径全覆盖,非仅静止/mid-hop 两路径)。
- **AC-52** [Advisory](V-11 破产去饱和独有)Coblis 下破产 vs 正常一眼可辨、不与其他状态重叠(§2 状态G)。证据 `vfx-bankrupt-desaturate.png`。
- **AC-53** [Advisory](V-16 胜利密度全局最高)胜利 Confetti 密度可见最高、非胜利 juice 不得超(§2 状态F)。证据 `vfx-victory-density.png`。

### M. R-1 新增(5 BLOCKING 闭合 + folded)
- **AC-54a** [Logic](Issue 1 卖房不误播建房 juice,本地派生,R-2)GIVEN 缓存 `cache[tile]=3`,收 owner **真实 2 字段** `OnBuildingChanged(tile, newCount=2)`(卖房);THEN 派生 `newCount<cache` → Sell,**建房弹出/金星庆祝路径 spy 调用==0**、走降档/snap 路径、置 `cache[tile]=2`。(守 art-bible「Gold 仅奖励」不被卖房触发;**注:测 owner 真实发送的 2 字段 payload,非 phantom `EBuildDirection`——R-2 消除 false-green**)
- **AC-54b** [Logic](缓存 miss 退化不假定建,R-2)GIVEN `cache` 无 tile 条目(种子化失败/首见 tile),收 `OnBuildingChanged(tile, newCount=2)`;THEN 走**非方向档位 snap**(庆祝 spy==0,不假定建)、记告警。(守 miss 兜底亦不误播庆祝)
- **AC-54c** [Logic](正向建房派生,R-2 双路径守护)GIVEN `cache[tile]=1`,收 `OnBuildingChanged(tile, newCount=2)`;THEN 派生 `newCount>cache` → Build,建房弹出/星星 spy==1、置 `cache[tile]=2`(与 AC-54a 卖房路径对照,守 `newCount` delta 方向派生正确)。
- **AC-54d** [Logic](破产清零 h→0 大跳派生,R-3 Finding A;**R-4 补正向断言**)GIVEN `cache[tile]=5`(酒店),收 `OnBuildingChanged(tile, newCount=0)`(破产清零大跳,若 owner 广播);THEN 派生 `newCount<cache` → **Sell 降档路径 spy==1**(V-Own-12b,非 EC-15 非方向 miss-fallback snap)、建房弹出/金星庆祝 spy==0、置 `cache[tile]=0`。**断言:方向只取 delta 符号,5→0 大跳(非 ±1 单步)仍正确派生 Sell**——若实现假定「必 ±1 单步」而对大跳 **fallback 到 miss snap**,则 Sell 降档 spy==0 被抓出 FAIL(**R-4:仅断言「庆祝==0」不够——miss-fallback snap 也满足庆祝==0,无法区分正确 Sell 与退化 snap;须正向断言 Sell 降档 spy==1**)。(消 EC-15「含破产拆除」原仅 AC-54a 测 3→2 单档=false-coverage)
- **AC-54e** [Logic](OnOwnershipChanged re-seed 封静默清零漂移,R-3 Finding A)GIVEN `cache[tile]=5`、building 静默清零(无 `OnBuildingChanged`)、收 `OnOwnershipChanged(tile, debtor→None)`(银行回退);THEN #19 re-seed `cache[tile]=GetHouseCount(tile)==0`;后续该格重买重建 `OnBuildingChanged(tile,1)` 派生 `1>0`=**Build(正确)**、庆祝 spy==1。**断言:若缺 re-seed 则 cache 残留 5、`1<5` 误派生 Sell、庆祝 spy==0 FAIL**(守 cache 失效兜底自闭 bug)。
- **AC-55** [Advisory·playtest-signoff](Issue 3 掷骰 experiential 承接,继承义务 dice OQ-T-Dice-4)掷骰三阶段(期待振动→翻滚拖影→咔哒定格弹性)节奏感知**明确区分**,双点庆祝 vs 入狱警告情绪切换**一眼可辨**;盲测 ≥5 玩家首局 ≥4 人能描述出"期待/命运悬念"感受(非仅"骰子在转")。**命中判据(R-5 R5,防橡皮图章)**:用**开放式提问**(「描述你刚才掷骰的感受」,非诱导式「VFX 好看吗」);"命中"= 描述中出现「期待/悬念/命运/想知道点数」等语义词(评估者按词义裁定、非主观裁量);采样在玩家完成 ≥3 次掷骰后(避开学规则期偏早采样)。证据:technical-artist + playtest lead 签核 `vfx-dice-feel-playtest-signoff.md`。
- **AC-56** [Advisory·playtest-signoff](Issue 3 hop 逐格屏息 experiential 承接,继承义务 movement OQ-T-Move-3;R-3 补量化门对齐 AC-55)棋子逐格 hop 让玩家有"数格子/看着棋子逼近命运"的期待感与落地踏实感;过 GO 高亮有视觉驻留感。**量化门(R-3,防低标准签核)**:盲测 ≥5 玩家首局 ≥4 人能描述出"看着棋子逐格逼近、想知道停在哪"的期待(**非仅"棋子在动/在走"**)。**命中判据(R-5 R5,同 AC-55)**:开放式提问 + 命中词义裁定(「逐格逼近/想知道停在哪/数格子」);**补落地踏实感(R-5,V-Own-08 deflation 盲区)**:同盲测 ≥3 人能描述「棋子停下/到达/落地」的感受(而非仅期待感),使 playtest 同时捕捉 V-Own-08 缺失对 Fantasy 第 1 节拍收束的影响。证据:playtest lead 签核 `vfx-hop-feel-playtest-signoff.md`。
- **AC-57** [Logic](Issue 4 hop 独立预算不被一般池级联驱逐)GIVEN 一般池满 `N_max_vfx`(破产级联 12 个 Economic),hop 池有 3 个 hop;WHEN 一般池连续驱逐;THEN **hop 池 3 个 hop 不被驱逐**(双池隔离)、`N_active_hop` 不受一般池 churn 影响;hop 池自身超 `N_hop_max` 才在 hop 池内驱逐最旧非当前行动者 hop。
- **AC-58** [Logic](Issue 5 当前行动者自身 hop 最短窗口)GIVEN 当前行动者 hop(`bIsCurrentActorHop`)Playing 0.1s 时 `OnTurnStarted` 到(`E_cur_vfx` 推进,主判据过期);THEN hop **不立即瞬移 snap**、至少播完 `T_hop_protect`(压缩 min 全程)才允 snap;**框架仍非阻塞**(对 owner/框架回调==0,注入 mock 验);非当前行动者历史 hop 同场景**立即整条跳**(对照,守差异化)。
- **AC-59** [Logic](EC-14 满载无低优先级 fallback,RB-7)GIVEN 一般池满 `N_max_vfx` 全 Critical/Social;WHEN 新低优先级(Economic/Motion)到;THEN **静默丢弃新者 + 告警、Critical/Social 不被驱逐**;WHEN 新 Critical 到(同帧二人破产);THEN 驱逐最旧 Social 让位、Critical 不丢。
- **AC-60** [Logic](Issue 2 #19 不订阅 OnBuildingAnnounced + 建房=Social 负向)GIVEN #19 初始化;THEN `IsBound`(player-turn `OnBuildingAnnounced` → #19)==**false**(#19 不订阅该事件,归 HUD);GIVEN 建房 juice 实例;THEN 其 `priority==Social`(非 Critical)、F-4 不被驱逐但 F-3 过期不做 critical-snap(EC-11)。
- **AC-61** [Logic](R-6 BLOCKING-2:UMG 软上界溢出 snap 最旧,与 Niagara 双池独立)GIVEN UMG 池满 `N_max_umg`(注入 placeholder 值,如 4,全为非 Critical 徽章/浮字),`N_active_umg==4`;WHEN 新 UMG 类 juice(数字浮字)到;THEN **snap 最旧非 Critical UMG 实例进 SnapDone**(Timeline 跳末帧 spy==1)、新入队、`N_active_umg` 不超上界;且**同帧 Niagara 双池 `N_active_vfx`/`N_active_hop` 不受影响**(独立计数,注入并发 Niagara 实例验不被 UMG 溢出挤占)。**断言:若实现把 UMG 实例计入 `N_max_vfx` 池或无独立 `N_active_umg` 门 → UMG 级联无界 / 或误驱逐 Niagara 实例,spy 抓出 Niagara 池计数被 UMG 改变 FAIL**(守 §3 UMG 不计 Niagara 池 + EC-18 独立溢出)。GIVEN UMG 池满含 1 Critical vignette;WHEN 溢出;THEN Critical vignette **先 snap 最终态不静默丢**(EC-11,SnapDone spy==1)。**注:`N_max_umg` 真值标定冻结(OQ-VFX-3),本 AC 测溢出逻辑路由用 fixture 值、非冻结值——逻辑可 headless [Logic],性能门另见 AC-21 升级闭环。**

---

### BLOCKING / Advisory 汇总(77 条 AC,R-6 由 75 增:AC-51c +1 Logic〔EC-16 第三退化,门控 OQ-VFX-15〕、AC-61 +1 Logic〔UMG 软上界溢出 snap〕;R-5 由 74 增:AC-51b +1 Logic〔M1 命名例外锚点解析〕;R-4 由 73 增:AC-32c +1 Logic 对照路径;R-3 由 70 增:AC-54d/54e +2、AC-32b +1,均 Logic)
- **[Logic] BLOCKING(61)**:AC-01~06、07~11、12~17b(含 15b/15c/17b)、18~20b(含 20b)、22~27、28、29~32(含 32b/32c)、35~38、40、42、43b、44~48、51b、51c、54a/54b/54c/54d/54e、57~61。**注:AC-51b/51c 标 [Logic·门控 OQ-VFX-15];AC-61 溢出逻辑可 headless,性能门数值随 AC-21 升级闭环标定。**
- **[Integration] BLOCKING(1)**:AC-43(读档重绑去重,**门控 OQ-VFX-2 ADR**;BP 路径无 spy 注入则降 Advisory)。
- **[Advisory](15)**:AC-06b、11b、21(性能门 + 升级闭环)、28b(code-review)、33、34、39(code-review)、41(code-review)、49、50、51、52、53、**55(playtest-signoff)、56(playtest-signoff)**。
- **诚实说明**:全部粒子外观/动画时长/颜色/曲线(渲染层)= [Advisory];N_max/N_hop_max 性能门标定前 Advisory(升级闭环见 AC-21);RNG/enable-not-own 镜像 = code-review;**experiential felt-quality(AC-55/56)= playtest-signoff Advisory**(继承义务,不可 headless);[Logic] 项均为公式/状态机/payload/spy 路由/反射(可 headless);AC-14 软上界经 `IGameClock` DI(同 HUD 模式,非壁钟)。

### 覆盖缺口(诚实标注)
1. **V-Own-01~16 全部视觉细节**(翻滚轴随机/hop 弧顶/建房 scaleZ 曲线/酒店旋转/vignette 时长)= 渲染层,归 Advisory 截图+lead 签核;未逐条写 Advisory(防 AC 膨胀),Visual/Feel story 实现时出集中 checklist。
2. **音频锚点 AA-01~18 时序**:归音频22,#19 AC 不负责;集成 playtest 核 juice↔音效同步(OQ-VFX-8)。
3. **N_max_vfx/N_hop_max/N_max_umg 标定后 [Logic] 升级(R-1 闭环化,R-6 补 UMG 池)**:AC-21 现 Advisory placeholder;OQ-VFX-3 resolution 时 performance-analyst **必补** `N_active_general≤N_max`/`N_active_hop≤N_hop_max`/`N_active_umg≤N_max_umg` 不变式 [Logic] + 回标,**#19 story 不得在标定前置 Done**(hard gate)。注:UMG 溢出**逻辑路由**(snap 最旧)已 [Logic] 覆盖(AC-61),标定仅冻结上界数值。
4. **屏幕叠加层 z-order 协调**:VFX 浮字/vignette/confetti 与 HUD/#17 z-order(防遮挡/吃点击)归架构 ADR;Pre-Production `/ux-design`+ADR 后补 [Integration] AC。
5. **AC-43/43b C++/BP 路径择一**:架构期选 C++ `AddDynamic` 则 AC-43 计数注入可保 [Logic]、补"handler FunctionName 列表只现 1 次"(同 HUD AC-48 R-3);选 BP 则 AC-43 降 Advisory·code-review(无 spy 注入,RB unreal B-2)。AC-43b EndPlay 解绑两路径均须覆盖。
6. **CR-9 RNG 误接自动检测**:AC-39 现 code-review 软约束;架构期 ADR 定 RNG 隔离强度→可升 [Logic] 还是保 Advisory(同 OQ-HUD-3,OQ-VFX-4)。MVP 单机仅视觉不一致;**Full Vision 联网前须 C++ 封装强制**(重放正确性,RB unreal B-3)。
7. **AC-28b Die1/Die2 拆解路径 spy**:BP 无路径 spy 注入 → code-review;C++ `ParseDiceResult` 可注入升 [Logic](OQ-VFX-4)。

## Open Questions

| ID | 问题 | Owner | 目标解决期 |
|----|------|-------|-----------|
| **OQ-VFX-1** | **hop path 来源**:#19 自建插值路径 vs movement4 暴露 `HopPath`(CR-4/F-1/EC-12);MVP 默认自建,若 movement 供则优先 | technical-artist / movement(4) owner | 架构期(随 movement OQ-Move-2) |
| **OQ-VFX-2** | **Niagara 系统结构 + UMG 动画驱动 + 屏幕空间叠加层 widget owner + z-order**(与 HUD/#17 层级,防遮挡/吃点击,UI Requirements 节)——与 HUD OQ-HUD-2 / #17 OQ-PC-4 共享架构 ADR。**(R-5 M2 新增子项)#8(建房)readiness 顺序**:#19 初始 `house_count_cache` 种子化 + 运行时每次 `OnOwnershipChanged` re-seed 均调 building `GetHouseCount`,须保证 **#8 在 #19 订阅时及每次 re-seed 时已就绪**(init 顺序钉死 / 或 #19 延迟种子化);否则 LOAD/init 期 `GetHouseCount` 返回 0 = wrong-low seed → 同 Finding A 类误播(CR-6 lifecycle 注)。**亦须澄清 SnapDone 两进入路径操作差异**:Playing→SnapDone(停 Niagara `DeactivateImmediate` + 覆盖最终态)vs Idle→SnapDone(`reduce_motion`,Niagara 从未激活 → 仅设静态最终态/材质参数,无粒子可销毁) | technical-director / technical-artist / ue-umg | 架构期 ADR |
| **OQ-VFX-3** | **`N_max_vfx`/`N_hop_max`/`N_confetti_max` 性能标定**(R-1 扩,R-3 校正):默认值均 placeholder,须**目标硬件**(规格待钉,如 Steam P50)标定后冻结(F-4/AC-21);标定场景**必含破产级联(R-3:≈2N 事件 = ownership N + building N,14~28 连发,视 OQ-VFX-11 广播契约)+ 「最后一人破产 + `OnGameWon` 同帧」最硬单帧(2 人局正常终局:破产去饱和材质 + 胜利暖金 PP + confetti + 烟花同帧)+ 14~28 连续驱逐**,门控**平均 ≤16.6ms 且 P99 ≤33ms**;**池预热(R-3 folded):标定须声明 warm vs cold-start,实现期 pool 预实例化防首帧冷启动尖峰**;**R-4 补帧预算分解(performance-analyst):P99 ≤33ms 须按 (a) Niagara 活跃实例 + (b) UMG overdraw + (c) PP material passes 三类分项度量,不只数粒子——① 同帧 PP 栈(破产去饱和 vignette/材质 + 胜利暖金 PP,V-Own-11+16)叠加 GPU 成本须单列子预算(OQ-VFX-6 art-director blend 裁定须回灌 PP 实现选择再标定);② UMG 类 juice(V-Own-10 数字浮字、V-Own-14b/14c 徽章、vignette)不计 Niagara 池但级联下无并发上界——须加 UMG 活跃实例软上界 placeholder(类比 `N_max_vfx`)或在标定明列 UMG overhead 子项**;**R-5 补三项(performance-analyst)**:**① 链式破产场景**——A→B→C 同回合(债权人被击穿连锁破产,均在 `OnTurnStarted` 前完成清算)→ 多 debtor,事件数随链长 > 单 debtor 2N,作独立必含标定场景(**注:in-kind 移交不逐格广播 `OnCashChanged`——house_count 随格不变无现金事件,bankruptcy AC-17 → cash 腿不额外计入级联,故级联 = ownership N + building N 仍成立、链式只是 N 倍增**);**② game-thread CPU vs GPU 分列度量**——级联驱逐帧的 14~28 连续 `DeactivateImmediate` 是 game-thread CPU 批处理(各 O(活跃粒子)),P99 ≤33ms **总帧时间**可掩盖「CPU 阻塞 ~10ms + GPU 空闲」的可感知 hitch(总时长达标但卡顿)→ 标定须**分列 game-thread CPU 时间**,门控 cascade 帧 game-thread CPU ≤Xms(placeholder,标定冻结);**③ `N_max_umg` UMG 软上界**——UMG 类 juice(数字浮字/徽章/vignette,不计 Niagara 池)级联下并发无上界 → 加 `N_max_umg`(Tuning Knobs 已列 placeholder)或标定明列 UMG overhead 子项 + 溢出按最旧 UMG snap;**④ PP 子预算 provisional floor**——同帧 PP 栈(去饱和 vignette + 暖金,V-Own-11+16)须设 provisional ≤Xms placeholder(防 PP 吃满预算饿死 Niagara+UMG 却仍过总门),标定时由 OQ-VFX-6 art-director 决策覆写。冻结后 performance-analyst **必补** `N_active_general≤N_max`/`N_active_hop≤N_hop_max` 不变式 [Logic] + 回标,**#19 story 标定前不得置 Done**(hard gate) | performance-analyst / technical-artist | Pre-Production 性能标定 |
| **OQ-VFX-4** | **RNG 隔离强度**:CR-9 juice RNG 走 #19 独立 `FRandomStream`;BP 无语言级拦截(AC-39 code-review)vs C++ 私有封装+DI spy 可升 [Logic]——同 HUD OQ-HUD-3 | technical-director / ue-umg | 架构期 ADR |
| **OQ-VFX-5** | **reduce_motion 降幅参数**:与 HUD/#17 共享同一全局布尔(CR-10);各 juice On 时具体降幅(瞬现/低强度)归 `/ux-design` 统一(随 HUD OQ-HUD-9) | ux-designer | Pre-Production `/ux-design` |
| **OQ-VFX-6** | **胜利暖金 post process 实现**(V-Own-16,~4500K):UE5.7 Post Process Material 路径;**须对照 5.7 Substrate(production-ready)是否改 PP Material 语义 + 与破产去饱和同帧叠加 blend 规则 + UMG 统计卡层序**(RB tech-art B-2 / unreal N-1 知识盲区)。**(R-6 tech-art 显式声明)5.7 Substrate 是 Surface 材质层改动,PP Material(`Blendable`/`UMaterialInterface` 路径)是否受影响 = LLM 知识截止 ~5.3 不可靠断言区,架构 ADR 落实现前须对照 `docs/engine-reference/unreal/` 5.7 release notes 核实 `Blendable` 接口有无 breaking change**;V-Own-11 破产去饱和走逐 pawn 材质参数(`SetScalarParameterValue` DMI,已定非全屏 PP)不受 Substrate PP 语义影响,仅 vignette/暖金若走全屏 PP 才涉此核实。**R-3 前置裁定**:① V-Own-11 破产去饱和**逐 pawn 材质参数**(已定,非全屏 PP)vs vignette 部分若走全屏 PP 须与暖金 PP 定 blend 权重/优先级;② **art-director 美术决策节点**——去饱和+暖金同帧(2 人局正常终局)视觉意图(灰金?暖金压去饱和?)须在技术实现前确认 | technical-artist + **art-director(美术决策)** | Pre-Production |
| **OQ-VFX-7(propagate)** | **depends-on 订正 + 双向回标**(R-1 更新):systems-index #19 depends-on **已含 `3,4,5,8,2,6`(line 152 已落)→ 再补 `1`(棋盘1,RB-4)**;**#2/#6/#1 的 depended-on-by 应含 #19**(剩余反向回标债);继承义务表 line 83(VFX MUST-FULFILL + experiential AC)本 GDD CR-3/4/5 + AC-55/56 已兑现回标 | producer | 同 PR propagate |
| **OQ-VFX-10 ✅ RESOLVED(R-2,本地派生)** | 原「建房8 补 `EBuildDirection`」**改为 #19 本地从 `newCount` delta 派生建/拆**(CR-6,镜像 property-ownership L259 owner-delta 先例 + building 原子 ±1 单步保证):`EBuildDirection` 跨档依赖删除,preserve-first 建房庆祝 MVP 即工作、零 building 侧改动。**遗留(非 #19 职责)**:building(2 字段)↔player-turn CR-3.5(3 字段)`OnBuildingChanged` 漂移仍须 building-upgrade + player-turn 对齐,与 #19 解耦 | 本地派生已闭;**遗留漂移**:producer / building8 / player-turn2 | ✅ R-2 闭(遗留漂移另开 producer 工单) |
| **OQ-VFX-8** | **#19↔音频22 协调机制**:音频22 订阅相同 owner 事件 vs 接收 #19 VFX 开始通知(音频锚点 AA-01~18 同帧触发);集成 playtest 核 juice↔音效同步 | audio-director / 音频22 owner | 音频22 设计时 |
| **OQ-VFX-9** | **数字 counter `T_counter` 共享旋钮来源**:#19 数字跳动时长引 HUD F-1 `T_counter`(防视觉撕裂);共享同一 DataAsset/ini 键的机制 | technical-director / ue-umg | 架构期 ADR |
| **OQ-VFX-11(propagate,R-3)** | **`LiquidateAllBuildings` 广播契约接缝**:银行破产逐格清零(building L75/L219 provisional)**是否广播 `OnBuildingChanged`、一次 `(tile,0)` vs 逐档**——owner 档(building/bankruptcy9)未定义。#19 已用 `OnOwnershipChanged` re-seed 封住该**广播契约**层 cache 漂移 bug(无视广播是否发出,CR-6/EC-15b/AC-54e);若上游确定逐格广播,#19 可选直接据 `OnBuildingChanged` 播 teardown 降档 juice(更精细)。**亦决定 OQ-VFX-3 级联事件数(N vs 2N)**。**(R-5 M2 新增子项)执行序**:re-seed 读 `GetHouseCount` 须在 building 清零**之后**才取到 0;须钉死 `LiquidateAllBuildings` 清零**先于/同步于** `OnOwnershipChanged` 广播——若 ownership 广播先于清零,re-seed 取旧高值、漂移仍存。执行序是 re-seed 兜底的**隐含前提**(非无条件自闭),须在 bankruptcy9/building8 接口设计时与广播粒度一并裁定 | producer / building8 / bankruptcy9 | building8/bankruptcy9 接口设计时(OQ-Build-3 同期) |
| **OQ-VFX-12(R-3;R-4 升为可证伪 PP 门 + 预承诺补救)** | **社交缝:非持屏者错过对手建房/酒店庆祝**(Pass'N Play 热座):建房 juice = Social(F-4 不被驱逐)但**不防 F-3 时间过期**——A 快速建酒店后框架推进给 B,A 的酒店合并 juice(V-Own-13,最高社交冲击 beat)可能在屏幕传递时过期、仅剩 HUD `OnBuildingAnnounced` banner 文字承载,削弱支柱②全员戏剧性。非游戏正确性问题(banner 补位),但 felt 折损。**R-4 裁定(CD,deflate game-designer 升 BLOCKING 之请——re-litigate R-3 决定且 banner 已补信息层,intensity-peak 折损是 playtest 须**测量**而非 doc-review 预判):不在 MVP GDD 强加保护,但升为**可证伪 Pre-Production 门**——假设:`H₀`=Pass'N Play 热座下 V-Own-13 酒店合并 juice 过期率 <X%(playtest 盲测计数,X 待 ux 定,如 20%);若 `H₀` 被证伪(过期率超阈致 ≥N 玩家漏看对手关键酒店庆祝),则触**预承诺补救**:对「当前行动者刚触发的建房/酒店 juice」加 `bIsRecentBuildEvent` 标记 + `T_build_protect` 旋钮(默认 ≥V-Own-13 600ms 动画),复用 CR-8 最短窗口机制(注:此举会模糊 R-1 立的 Critical/Social tier 边界,故仅 playtest 证伪后才采纳,非预先施加) | game-designer / ux-designer | Pre-Production `/ux-design` playtest |
| **OQ-VFX-13(propagate,R-4)** | **破产事件接缝:`OnPlayerBankrupt` vs `OnBankruptcyDeclared`**——#19 V-Own-11(棋子去饱和)+ AC-35 订阅经济5 `OnBankruptcyDeclared(Debtor,Creditor)`(2 字段,现金侧转移后广播,economy L120/AC-36);但破产9(Approved)`OnPlayerBankrupt(debtor,creditor,reason)`(3 字段,**全 4 步清算编排末**广播,owner map 终态,bankruptcy L75/189/AC-39)显式登记为出局/清算 juice 使能事件。两事件**时机不同**(现金侧 vs 全编排末)且 `OnPlayerBankrupt` 携 `reason`(收租 vs 银行破产)#19 当前未用。**非 #19 单档可闭**:HUD(16,**已 Approved**)V-Enable-03/AC-16 作**同样选择**(订 `OnBankruptcyDeclared`)——单档拦 #19 与已 Approved 姊妹档裁决不一致。逐格清算 juice(V-Own-14b/14c)经 property6 `OnOwnershipChanged` 的委托(bankruptcy L191)#19 **已兑现**,此接缝仅关 V-Own-11 触发源选择。producer 核实两事件时序一致性 + 裁定 V-Own-11 是否应改订 `OnPlayerBankrupt`(语义更准、得 reason 字段),across bankruptcy9/economy5/HUD16/VFX19 | producer / bankruptcy9 / economy5 / HUD16 | 同 PR 登记,Pre-Production 前裁定 |
| **OQ-VFX-14(R-5,Fantasy 拓展)** | **"险过"反馈缺席**:棋子停在对手地产**旁一格**(不付租金)是棋盘游戏最强情绪峰值之一("用手感赢下来"支柱③×②),当前 V-Own 目录无对应 juice。**非当前规格缺陷**(Fantasy 拓展非 spec 错误)。Pre-Production playtest(随 AC-55/56)评估是否需补(如:棋子停在对手地块旁一格时该格淡红脉冲 ~0.3s 示"险些"——须不与 V-Own-08 落地 juice/破产预警混淆) | game-designer / ux-designer | Pre-Production `/ux-design` playtest |
| **OQ-VFX-16(R-7,门控 V-Own-01 起振锚)** | **掷骰意图事件来源**:V-Own-01 期待振动起振须经 owner 意图事件(骰子3 `OnDiceRollStarted` / 回合2 `OnPhaseChanged(RollPhase)` 进入),但当前 Interactions 表骰子3 仅列 `OnDiceRolled`(结果事件)、无意图事件;#19 绝不监听 UI 输入层(CR-1)。**OQ-VFX-1 scope = hop path,不覆盖此** → 独立追踪锚。MVP fallback = 回合2 `OnPhaseChanged(RollPhase)` 进入(已具名);架构期确认骰子3 是否暴露 `OnDiceRollStarted` 或采 fallback。**非 [Logic] AC**(V-Own-01 渲染层锚点,避 headless 陷阱)。 | 骰子3 / 回合2 owner / technical-director | 架构期 ADR(随 OQ-VFX-1) |
| **OQ-VFX-15(R-6,门控 AC-51b/51c)** | **pawn registry 查询接口来源**:收租金币 Payee 终点(CR-2 命名例外 / V-Own-09)须经 `Payee` PlayerId → pawn 取 `GetActorLocation()`,但该 **PlayerId→pawn 映射的 owner 系统在 GDD 未定**。候选:① **AGameState 内置** `PlayerArray`→`PlayerController`→`GetPawn()`(UE 内置,无新跨档接口,#19 直查 GameState);② **movement4** 暴露只读 `GetPawnForPlayer(PlayerId): AActor*`(须 propagate movement);③ **board-manager / WorldSubsystem** 持 `TMap<PlayerId,APawn*>`。**AC-51b/51c 经此门控**(同 AC-14/15c `IGameClock` DI 门控模式):源选定前 spy 注入边界抽象、不可早写死;选 ① 则零跨档(Dependencies 注 GameState 内置查询),选 ② 则须 movement4 propagate。架构 ADR 钉死后 AC-51b/51c 的「pawn registry lookup」改为具体接口名、spy 注入该接口。**phantom-spy 防护**:在源选定前,AC-51b/51c 标 [Logic·门控],不作 headless 已可兑现 [Logic] 计入硬门。 | technical-director / movement(4) owner | 架构期 ADR(随 OQ-VFX-2 UMG/绑定 ADR) |

> **📌 Asset Spec Flag**:见 Visual/Audio 节(art-bible approved 后 `/asset-spec system:vfx-feedback`)。
> **📌 UX Flag(轻)**:见 UI Requirements 节(无交互 UI,仅叠加层 z-order 协调随 HUD `/ux-design`)。
