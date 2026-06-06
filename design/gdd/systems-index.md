# Systems Index: 骰子大亨 (Dice Tycoon)

> **Status**: Draft
> **Created**: 2026-05-31
> **Last Updated**: 2026-06-02
> **Source Concept**: design/gdd/game-concept.md

---

## Overview

《骰子大亨》是对 Rento Fortune 的复刻——一款 2–8 人回合制数字骰子棋盘游戏。其机械范围围绕经典大富翁核心循环展开：掷骰移动、买地、收租、建房、逼对手破产；并叠加原作的四项特色博弈机制（拍卖、命运之轮、俄罗斯轮盘、策略卡）。系统分解的核心地基是 **经济现金系统**（租金/抵押/建房公式的发源地，几乎所有交互依赖它）与 **棋盘数据 / 玩家回合** 框架。MVP 聚焦本地 + AI 跑通核心循环，特色机制留待 Alpha，联网与地图编辑器留待 Full Vision。设计与实现严格按依赖顺序自底向上推进，呼应支柱①桌游忠实、②社交博弈、③运气×策略、④易上手。

---

## Systems Enumeration

| # | System Name | Category | Priority | Status | Design Doc | Depends On |
|---|-------------|----------|----------|--------|------------|------------|
| 1 | 棋盘数据 Board/Tile Data (inferred) | Core | MVP | Approved | [board-data.md](board-data.md) | — |
| 2 | 玩家与回合管理 Player & Turn (inferred) | Core | MVP | Approved※ | [player-turn.md](player-turn.md) | 3 |
| 3 | 骰子 Dice | Core | MVP | Approved | [dice.md](dice.md) | — |
| 4 | 移动 Movement | Gameplay | MVP | Approved† | [movement.md](movement.md) | 1, 2, 3 |
| 5 | 经济与现金 Economy & Cash | Economy | MVP | Approved* | [economy-cash.md](economy-cash.md) | 1, 2 |
| 6 | 地产所有权 Property Ownership | Economy | MVP | Approved‡ | [property-ownership.md](property-ownership.md) | 1, 5 |
| 7 | 事件格 Tile Events（机会/命运/税/监狱/起点） | Gameplay | MVP | Approved§ | [tile-events.md](tile-events.md) | 1, 2, 3, 4, 5, 8(soft) |
| 8 | 建房升级 Building & Rent Scaling | Economy | MVP | Approved¶ | [building-upgrade.md](building-upgrade.md) | 1, 5, 6 |
| 9 | 破产与胜负 Bankruptcy & Win | Gameplay | MVP | Approved◊ | [bankruptcy.md](bankruptcy.md) | 2, 5, 6, 8 |
| 10 | AI 对手 AI Opponent (inferred) | Gameplay | MVP | Approved◊◊ (R-5 fresh `/design-review` 2026-06-06 = 仍有 1 blocker / **F-4 连续第二轮 peel 新接缝**；R-4 的 2 blocker 处方已落地核验通过，但 R-5 揪出 F-4 unlock 赎回(L205)与 F-5b 主动赎回(L222)未去重→AC-64 核心场景必然双发 Unmortgage(A)、违 CR-7/L69 零跳过纪律且无 AC 证伪；须用户裁定去重方向。**↳ 用户裁定 RETREAT/方案B**：F-4「unmortgage→解锁→建房」多步协调 MVP 砍掉(unlock 候选分支/联合现金门/AC-64-65/`unmortgage_cost(A)` 全删，退回单门简单贪心)，AI 主动赎回唯一路径=F-5b，同格双发接缝从根消除；unlock-then-build 降 Alpha OQ-AI-10。**↳ R-6 = Approved〔收敛终结轮〕**(2026-06-06 fresh 五专家隔离 `/design-review`+CD)：verified_blockers=0——**过批纠正闭环 R-3 过批→R-4 揪 F-4 接缝→R-5 F-4 二次 peel→用户 RETREAT→R-6 验 unlock-then-build 已砍/接缝闭合**；R-5 唯一 BLOCKING 失效(无第二条赎回路径可双发)，F-5b/F-3 补全格豁免/Buffer_max/AC-11b/AC-61a-b 全保留。blocker 量逐轮单调降 6→4→5→2→1→0 触底归零=健康收敛。最终 AC 合计 71→69(删 AC-64/65)。遗留 OQ-AI-10(unlock-then-build Alpha)及 CR-5④ VFX/CR-6→OQ-1 ADR/OQ-AI-3b BP-C++/OnAIActionExecuted BlueprintAssignable 跨档/架构债不阻开工。见 review-log R-6 条目。) | [ai-opponent.md](ai-opponent.md) | 1, 2, 3, 5, 6, 7, 8 |
| 11 | 交易 Trading (inferred) | Economy | Alpha | Not Started | — | 5, 6 |
| 12 | 拍卖 Auction | Economy | Alpha | Not Started | — | 5, 6 |
| 13 | 命运之轮 Fortune Wheel | Gameplay | Alpha | Not Started | — | 5, 7 |
| 14 | 俄罗斯轮盘 Russian Roulette | Gameplay | Alpha | Not Started | — | 2, 9 |
| 15 | 策略卡 Strategy Cards | Gameplay | Alpha | Not Started | — | 5, 6, 7 |
| 16 | HUD（玩家面板/现金/回合）(inferred) | UI | MVP | Approved❖ (R-3 fresh design-review 2026-06-05 full=NEEDS REVISION〔finishing-class,收敛〕→就地修订完成,5 must-land+7 Recommended 全闭合,CD 裁定 finishing 末圈无需 R-4,AC 61→62) | [hud.md](hud.md) | 1, 2, 5 |
| 17 | 地产卡与卡牌 UI (inferred) | UI | MVP | Approved❖ (R-2 fresh design-review 2026-06-05 finishing-class→本档内修订 + 3 跨档 must-land 全闭合+fresh-grep,用户 accept:OQ-PC-8✅ economy GetUnmortgageCostForDisplay / OQ-PC-11✅ economy depended-on-by #17 / OQ-PC-6🔶 hud OQ-HUD-10 回标✅。非阻 followup:art-bible 垄断视觉/等宽字体/Utility 色组、架构 ADR OQ-PC-4) | [property-card-ui.md](property-card-ui.md) | 1, 6, 8 (+7 Alpha) |
| 18 | 交易/拍卖 UI (inferred) | UI | Alpha | Not Started | — | 11, 12 |
| 19 | 游戏反馈 VFX（骰子/金币/建房 juice）(inferred) | UI | MVP | Approved❖ (R-8 fresh `/design-review` 2026-06-06=APPROVED_WITH_FOLLOWUPS 收敛终结) — NEEDS REVISION → R-3 就地修订完成（2026-06-05 fresh `/design-review` full 5 专家+CD,独立 fresh-grep;**新正交维度=#19↔破产/建房 teardown 事件完整性接缝**,5 自闭 must-land 闭合:**OnOwnershipChanged re-seed 封银行破产 `LiquidateAllBuildings` 静默清零 cache 漂移**(Finding A)、AC-54d/54e 补 h→0 大跳+re-seed、松绑「原子±1」、**V-Own-14b/14c 补破产移交清算 juice**(Finding B,承接 bankruptcy L191 Approved 委托)、AC-43b false-green 修(6→12 delegate);+2 上游 propagate(OQ-VFX-11 `LiquidateAllBuildings` 广播契约/OQ-VFX-3 级联 2N+同帧)+7 Recommended;AC 70→73。**↳ R-4 就地修订完成（2026-06-05 fresh full 5 专家+CD,独立 fresh-grep；无新正交维度〔R-3 预测成立〕,4 自闭 must-land=R-1/R-3 修法第二代残渣:AC-32b 双重 vacuous→改乱序窗口+bIsDouble=true+AC-32c、EC-5b/AC-43b BeginDestroy 野指针→仅 EndPlay、F-2 Amount<0 守卫公式层前置、AC-54d 补正向 Sell-spy；CD deflate game-designer 2 BLOCKING〔V-Own-08 落地 juice/OQ-VFX-12 酒店热座=re-litigate R-3,后者升可证伪 PP 门〕;AC 73→74=58 Logic+1 Integration+15 Advisory;待 fresh R-5,见 review-log）。新增跨档 propagate 债 OQ-VFX-13（producer）:`OnPlayerBankrupt`〔破产9,3字段编排末〕vs `OnBankruptcyDeclared`〔经济5,2字段现金侧〕接缝,HUD16〔已 Approved〕作同样选择,reconcile across bankruptcy9/economy5/HUD16/VFX19。**↳ R-5 就地修订完成（2026-06-06 fresh full 5 专家+CD,主审逐条对文核实专家 claim）= NEEDS REVISION〔finishing-class 终结轮〕:1 真 BLOCKING（M1=CR-2 ↔ V-Own-09 收租金币锚点矛盾——Payee 终点 payload 不可解,R-1..R-4 ~20 passes 漏的长潜矛盾非残渣;用户裁定方案 B=CR-2 scoped 命名例外〔Payee 静止态豁免 + mid-hop 退化 + AC-51b 守边界〕）+ 1 honesty softening（M2=re-seed「无视上游」过度承诺→软化+登记 OQ-VFX-11 执行序/OQ-VFX-2 #8 readiness）+ 7 folded;主审 deflate 大量 over-claim（perf 3N/4N=in-kind 无逐格 cash、systems F2-A=L168+AC-10 已覆盖、qa AC-43b/35/36=同 Approved HUD 模式、game B-2=渲染时序违 headless）;blocker 量逐轮降 5→5→5→4→2 收敛触底;AC 74→75。待 fresh R-6,见 review-log R-5 条目。**↳ R-6 就地修订完成（2026-06-06 fresh full 5 专家+CD,主审对文逐条核实 claim-wave）= NEEDS REVISION〔finishing-class 终结轮,本轮 in-doc 全闭〕:2 must-land（均 R-5 修法实现约束未落地,非重裁系统）—① **BLOCKING-1 pawn registry 来源未登记**（systems+qa+tech-art 三专家收敛+主审 fresh-grep:`pawn registry` 全文仅 4 处 #19 内部引用、无 OQ/Dependencies 接口=phantom-spy 风险,R-5 review-log L167 已预警的 R-6 焦点）→ 新增 **OQ-VFX-15**（registry owner 架构期裁定:AGameState 内置/movement4/board-manager）+ AC-51b 标 `[Logic·门控 OQ-VFX-15]`+ Dependencies 新增 pawn registry 行,零跨档;② **BLOCKING-2 N_max_umg 机制**（perf;主审收窄 over-claim:溢出已定义,真缺口=F-4 无 UMG 计数门+升级闭环漏枚举）→ F-4 补 `N_active_umg≤N_max_umg` 段+EC-18+AC-61[Logic]+升级闭环补枚举;+4 folded（AC-51c EC-16 第三退化/V-Own-01 触发源澄清/F-1 N 上界+F-4 旋钮硬下界/OQ-VFX-6 Substrate 盲区）。AC 75→77（61 Logic）。blocker 5→5→5→4→2→2 收敛触底,无重裁「#19 是什么」。**↳ R-7 就地修订完成（2026-06-06 fresh full 5 专家+CD,独立 fresh-grep + 主审对文核实）= NEEDS REVISION〔finishing-class 终结轮,本轮 in-doc 全闭〕:R-6 两 must-land 经主审 fresh-grep 复核全真落地无残渣（movement.md 确无 pawn 坐标查询=OQ-VFX-15 门控诚实）;2 must-land（均零成本 fold,4 专家 APPROVED-with-followups vs qa-lead 1 BLOCKING,CD 裁可操作内容一致取 must-land 落地前不标 Approved）—① **AC-43b 验证义务落注释不落 THEN 权威断言**（qa-lead;逐 handler `FunctionName` 验证只活注里→OQ-VFX-13 改订 `OnPlayerBankrupt` 时枚举漂移在总数仍 12 下静默存活野指针,gdd-fix-lands-in-comments-not-spec 红线）→ 逐 handler 名匹配断言移入 THEN,零架构;② **V-Own-01 掷骰意图事件来源未登记 OQ**（tech-art+game-designer 双独立收敛;OQ-VFX-1 scope=hop path 不覆盖）→ 新增 **OQ-VFX-16**（owner 骰子3/回合2,架构期,MVP fallback `OnPhaseChanged(RollPhase)` 已具名）+ V-Own-01 回指,不升 [Logic] AC,零跨档。AC 计数不变（77/61 Logic;OQ-VFX-16 是 OQ 非 AC）。blocker 5→5→5→4→2→2→2,2 条均 finishing-class 无新正交维度。**↳ R-8 = APPROVED_WITH_FOLLOWUPS〔收敛终结轮〕（2026-06-06 fresh full 5 专家+CD,主审逐条对文核实 claim-wave）:verified_blockers=0——R-7 两 must-land（AC-43b 逐 handler `FunctionName` 验证移入 THEN 权威断言 + OQ-VFX-16 掷骰意图事件来源）经主审 fresh-grep 复核全真落地无残渣;5 专家 claim-wave 经对文核实无一成立为真 BLOCKING（全归 finishing-class followup 或 deflate:已裁定项 re-litigate/已 Approved 姊妹档一致/Pre-Production 标定/渲染层 headless）。2 logged decisions（finishing-class，非 BLOCKING）:① V-Own-01 主体「点击掷骰后」残留降 followup（OQ-VFX-16 来源未定不提前写死=已知待定非已知错误，架构期裁定后改「收到掷骰意图事件后」）;② OQ-VFX-13 破产事件接缝（`OnPlayerBankrupt` vs `OnBankruptcyDeclared`）维持外置 producer 跨档债，与已 Approved HUD16 V-Enable-03/AC-16 同选择一致（[[10px-floor-consistent-with-approved-sibling]]），AC-43b 末注已登记 producer 改订时枚举同步维护义务。blocker 量逐轮单调降 5→5→5→4→2→2→2→0 触底归零=健康收敛终结。遗留 OQ-VFX-11/13/15/16 上游/架构债不阻开工;标定 hard gate 见 AC-21/OQ-VFX-3。见 review-log R-8 条目。** | [vfx-feedback.md](vfx-feedback.md) | 3, 4, 5, 8, **2, 6, 1**（+1 棋盘锚点 R-1 揪出，见脚注✪） |
| 20 | 主菜单与房间大厅 (inferred) | UI | MVP | Approved⟡ | [main-menu-lobby.md](main-menu-lobby.md) | 2 |
| 21 | 存档/读档 Save/Load (inferred) | Persistence | MVP | Approved☼ | [save-load.md](save-load.md) | 1, 2, 5, 6 |
| 22 | 音频 Audio SFX/BGM (inferred) | Audio | MVP | Approved♫ (R-1 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS〔首评即收敛终结，见脚注♫〕) | [audio.md](audio.md) | 2, 3, 4, 5, 6, 7, 8 |
| 23 | 设置 & House Rules | Meta | Alpha | Not Started | — | 1, 5 |
| 24 | 教程/引导 (inferred) | Meta | Alpha | Not Started | — | 4, 5, 6 |
| 25 | 联网 Online（同步/匹配/重连） | Networking | Full Vision | Not Started | — | 几乎全部 |
| 26 | 地图编辑器 Map Editor | Meta | Full Vision | Not Started | — | 1 |

> **\*经济(5) Approved-with-followups**(re-review full 2026-06-02):5 前轮 blocker 全闭合,R2 落 2 诚实硬线 + 软化。留 followup(不阻塞下游开工):**P-1 ✅ RESOLVED 2026-06-03**(`/propagate-design-change` 闭合:board-data line 272/273/332/338/403 + entities.yaml line 646 套利 framing → 数据质量/数据错误,对齐经济 AC-42;grep 核验残留仅否定式);**OQ-Econ-10** SalaryAmount 上界派发棋盘(1)/编辑器(26);Recommended AC(CR-4 买地/AC-43 fixture/破产终态/F-4 clamp/fee_den 校验)下一 pass 处理。

> **†移动(4) APPROVED**(fresh re-review full 2026-06-02,6 agent opus + CD;propagate 闭合 2026-06-02):本档内部已收敛(R3 改 3 处)。CD 裁定的剩余真 blocker(system-of-systems propagate 债,处方=propagate 非 RETREAT)已经 producer 协调 `/propagate-design-change` + grep 逐档核实全部落地 → **移动 APPROVED**:**① ✅** economy.md line 57/102/330 旧 push 清理为 PULL + Interactions 玩家回合行补 holder 源(holder owner=回合2);**② ✅** player-turn 补 AC-46(SentToJail 抑制)+ CR-3.1/Edge Cases 规则,实 AC 号回填 movement AC-15;**③ ✅** player-turn 补 AC-47(程间非重入,CR-3.1 承接 CR-4);**④ ✅** board-data 补 AC-23j + Validation + Tuning(`DiceMultiplierTable[i]` 上界,OQ-Move-6)。OQ-Move-5/6 均 RESOLVED。

> **‡地产所有权(6) Approved-with-followups(R2 re-review full,2026-06-03)**:8 必需节全写,54 AC(44 [Logic] + 5 [Logic·CI-stub] + 5 [Advisory])。R1 Group A(8 BLOCKING)+ Group B propagate 闭合;**R2 re-review(economy/systems/game/qa/unreal 5 specialist + CD)2 hard blocker 已修**——AC-14b 重写(消"6 检测读不到的 house_count"逻辑不可实现)+ **OQ-Prop-7** economy F-5/292/AC-20 propagate 残留清理(Group B 第二代漏列,fresh-grep 双向 0 残留)。**deferred followup(不阻下游开工)**:OQ-Prop-5(Rento 抵押-垄断核查,冻结前)/ OQ-Prop-1(owner map 生命周期 ADR,下游8/9前)/ Player Fantasy 结构重构 + 快照 per-tile 映射 + F-2/3 求值序升级等 REC。 关键裁定:**house_count 归建房(8)、6 不持**(避 6↔8 环);**is_monopoly 由 6 算**(查 board 组成员 + owner map);**买地/抵押事务由 6 拥有、调 economy `Debit`/`Credit`(6→5),economy 不反调 6**(解 5↔6 环)。**设计揪出的 producer propagate 债 —— ✅ 全部 RESOLVED 2026-06-03(`/propagate-design-change`,fresh-grep 重盘扩集)**:**OQ-Prop-2** economy CR-4/CR-5/F-11 反向调用 + AC-30/31 范围 → 改为事务归6方向6→5、economy 只执行现金侧(economy line 60/63/64/70/87/104/106/118/241/291/441/442);**OQ-Prop-3** 「house_count 由6供」→「建房8供」,**原登记 6 处,fresh-grep 实命中 9 处**(economy 58/104/152/265/318/333/470 + systems-index 67 + entities 267/277);**OQ-Prop-4** index `9 depends-on` 补 6 ✓(`16 depends-on` 暂不补——HUD 是否直读归属待 HUD GDD 裁定);**OQ-Prop-6**(R1 新增) board-data line90 RentTable 档位错归属6 → 经济5 F-3 ✓;**P5** board-data line161/301 补 `TileType`/`GetTilesInGroup` ✓。registry 已注册 `is_monopoly`/`station_count`/`utility_count`(6-owned)。

> **¶建房升级(8) APPROVED-with-followups(R4 re-review full,2026-06-04)**:8 必需节全写,35 AC(30 [Logic] + 2 [Advisory] + 2 [Integration·BLOCKING] + 1 quarantined)。R1(6 blocker)→R2(3 in-doc:AC-29/30/F-1)→R3(3 in-doc:CR-9 旁路/AC-20 tautology/AC-3-8 GIVEN)逐轮闭合;**R4(economy/systems/game/qa/unreal 5 specialist + CD)**:5 项 fast close-out 已应用(L131/L202 强制清算均衡**单一 owner 适用域**修内部自相矛盾 + AC-29 spy `OutboundCallLog` 约定 + AC-11a 删虚假调用序蕴含 + CR-8 Blueprint 同步多播重入注→OQ-Build-5 + Fantasy「全砸下去」均衡建房措辞)。**producer propagate 债 ✅ 已兑现(2026-06-04 `/propagate-design-change`,fresh-grep 双侧核实落地;OQ-Build-3 in-kind 仍 open 待破产9)**:**OQ-Build-1**(economy CR-7.3 L71 + AC-43 变体B 仍写 economy 驱动清算=5→8/5→6 反向边;player-turn 缺清算驱动相 + 「调6 Mortgage 前读8 GetHouseCount==0」AC,fresh-grep 双侧真空——含两层清算顺序 MV升序 vs F-4 全盘最高档须裁定单一规则)、**OQ-Build-2**(economy F-10 `is_insolvent` L229 缺 `preaggregated_nlv` 形参 + F-9 展开式 + player-turn CR-3.2 须覆盖 F-9/破产判据聚合路径,非仅算租)、**OQ-Build-6**(通告 beat 下游 player-turn/HUD 接收义务真空)。硬门已在 building AC-27 登记「player-turn AC 未补前 #8 抵押接缝 story 不得 Done」。**RECOMMENDED 下一 pass**:AC-25c 序列级均衡 / AC-21 OnBuildingChanged 断言 / F-1 is_monopoly-guards-first 文档化 / 奇数 BC floor 序 / 酒店租低于垄断翻倍 board 校验(并入 OQ-Econ-9)/ BP 绑定形式 + 清算 loop 帧模式→OQ-Build-5。**CD 裁决**:对手现金可见性归 UX scope(L271 UX Flag,非 #8 blocker,重申 R1/R2 裁定)。 **↳ 2026-06-04(破产9 R2 re-review propagate 债2):** `LiquidateAllBuildings(debtor)` 接口语义补全——building line 65/75/219 显式承诺**银行破产专用、逐格清零、不调经济5 `Credit`(无偿清算,守 economy F-11)**,与 `ForcedSellNextBuilding`(调 Credit 返半价)区分,使 bankruptcy AC-19「Credit==0」有对端支撑。语义为收紧(非扩张)、与 OQ-Build-3 边界 + AC-31 quarantine 一致 → **轻量标注,不触发 #8 全量 re-review**;`LiquidateAllBuildings` 仍随 OQ-Build-3 provisional(若裁定经典「卖房还银行」则 re-propagate)。工单 `change-impact-2026-06-04-bankruptcy-rereview-crossdoc.md`。
>
> **§事件格(7) APPROVED-with-followups(R2 re-review full,2026-06-03)**:8 必需节全写,64 AC(59 [Logic] + 4 [Advisory] + RepairFee 4 条件门)。首评 MAJOR REVISION NEEDED(game/systems/economy/qa/unreal 5 specialist + CD)→ R1 改 8 blocker + AC-craft → **R2(systems/qa + CD)12 首评 BLOCKING 全 CLOSED**;唯一本档 must-fix(L110 存档行 model-A 残留"指针/监狱态")已修。关键裁定:**OQ-Event-1 监狱态以 player-turn 为准**(字段存 player-turn PlayerState/player-turn 计数、7 拥规则+受控写;`bInJail`→`bIsInJail`);**B-12** 补 AC-62/63(承接 player-turn CR-8#4/AC-39b);牌堆 model B + Fisher-Yates 种子序列化;枚举 `EJailReason`/`EJailReleaseMethod` uint8。**followup(不阻下游)**:跨档 propagate=player-turn 命名监狱态受控写接口(契约已在册 L200/L366,仅补名,REC 非 vacuum);deferred REC=Player Fantasy 结构重构 + 保释金50退化诚实标注;pre-freeze OQ=OQ-Event-6(Rento 监狱核查)/OQ-Event-7(engine ADR:非重入/宿主/RNG)/OQ-Event-3 ✅ RESOLVED(RepairFee 全盘房/酒店口径经建房8 F-7 `GetTotalHouseCount`/`GetTotalHotelCount`,与 per-tile [0,5] 区分,/consistency-check 2026-06-04)。registry 已注册 `JAIL_BAIL_AMOUNT`/`MAX_JAIL_TURNS`/牌堆size + `collect/pay_to_each_total`。
>
> **◊破产与胜负(9) Designed(pending review,2026-06-04)**:11 节全写(8 必需 + Visual/Audio + UI + Open Questions),43 AC(39 [Logic] 含 2 CI-stub + 3 [Integration] + 1 Advisory),lean 模式(D/H spawn qa-lead,CD gate skip)。关键裁定:**9 = return-only 编排子程序**(回合2 调 `ResolveBankruptcy(debtor,creditor,snapshot)→{eliminated,winnerId|NONE,rankingEntry}`,只向下调 5/6/8、**绝不回调2**,防 2↔9 环;bIsBankrupt 归回合2 经返回值驱动写);**creditor 分支**(收租→资产 in-kind 转债权人 / 银行 INDEX_NONE→地回退+现金蒸发);**OQ-Build-3 裁定**=收租 in-kind 随格 + 银行调8 `LiquidateAllBuildings`(provisional 待 Rento 核查);**排名**=出局逆序 + net_worth(F-1=Cash+经济F-9)provisional MVP 不计算。registry 已注册 `net_worth`(9-owned)+ active_player_count/net_liquidation_value 补 9 引用与 APC 接缝注。**OQ-Bankrupt-3 ✅ RESOLVED 2026-06-04**(`/propagate-design-change` 已闭合 economy line479 改 identity-check + line247/446 补建筑拆除归9调8;fresh-grep 双侧核实,范围较登记收窄——「仅现金腿」一项 economy R1 已自修);OQ-Bankrupt-1/2/4/5/6 见 GDD。
> **↳ R-review(2026-06-04 `/design-review` full,economy/systems/qa/game 4 specialist + CD synthesis):verdict NEEDS REVISION,维持 NEEDS REVISION◊。** R1 的 2↔9 根因经独立 fresh-grep 确认真闭合(player-turn APPROVED 对齐)。本轮 **6 in-doc blocker 已就地修**:F-2 链式「返回破产者」洞(排除全调用链已出局 debtor,gated OQ-Bankrupt-1)/ L75/L189 `OnGameWon` 广播者订正(9 返 winnerId、回合2 触发)/ CR-3 step4 写序契约(先 bIsBankrupt 后 OnGameWon)/ AC-3 非法 `[Advisory→Logic]`→合法 `[Logic]` / AC-29 拆 +AC-29b fallback 边界 / AC-30/33/41 诚实改 `[Logic·CI-stub]`;AC 计数 43→44。**2 cross-doc propagate 债 ✅ CLOSED(2026-06-04 producer `/propagate-design-change`,fresh-grep 双侧)**:① economy-cash.md:302 残留 `OnLastPlayerStanding` → 统一 `OnGameWon`+广播者回合2(三档单侧唯一+一致);② building-upgrade.md `LiquidateAllBuildings` 无 Credit 语义已补(**实改 3 处 line 65/75/219**——工单登记 under-count 漏 line 65,且 65 残留旧「全卖还银行」与新承诺直接矛盾,fresh-grep 揪出全改;AC-19 现有对端支撑;provisional/OQ-Build-3 边界保留)。工单 `docs/architecture/change-impact-2026-06-04-bankruptcy-rereview-crossdoc.md` = ✅ COMPLETE。**#9 全 blocker(6 in-doc + 2 cross-doc)已闭合 fresh-grep 核实 → user 裁定就地批准、跳过确认再审 → #9 APPROVED(2026-06-04)。** 剩 RECOMMENDED followup(不阻下游开工,设计冻结/Pre-Production 处理):net_worth OQ-Bankrupt-2 量化措辞 / 现金侧-归属侧不一致窗口 OnCashChanged 标记 / F-3 N≥3 链调用模型 / falsifiable 假设 Gini 阈值 / Player Fantasy 验收边界注 / OQ-Bankrupt-7 owner 收窄 player-turn / AC-34 归属 player-turn / AC-37→Integration。
>
> **※玩家与回合管理(2) re-review(was APPROVED R8,2026-06-04 `/propagate-design-change` 2↔9)**:破产9 design-review 揪出 2↔9 接口与本档(Approved)系统性真矛盾(谁写 `bIsBankrupt`／APC 公式／胜负信号 `OnLastPlayerStanding` vs `OnGameWon`,systems-designer+qa-lead 双独立确认),user 拍板+CD 背书方向=**改本档对齐破产9 return-only**。propagate 已执行(13 处:CR-6/受控写面 L98/状态表/Interactions/事件 schema/F-2 胜负口径/Edge×3/Dependencies/AC-40),fresh-grep 双侧核实信号方向单侧唯一双侧一致。**✅ R-2to9 RR 已 APPROVED(2026-06-04 `/design-review`,systems-designer+qa-lead+game-designer+CD)**:三锁面经主审 fresh-grep 双侧 + 3 specialist + CD 终审确认架构干净、双侧对齐,**真 blocker=0**;4 项 must-land AC-hardening followup 已就地应用(AC-40 拆 a/b 不降级守 return-only CI 底线 + 新增 AC-40c 封堵旧双触发 bug 回归守护 + CR-6 边沿触发规格 + AC-12 收紧 + AC-19/F-2 时序前提)。本档回 **APPROVED**。跨档 followup(bankruptcy L75/L189 `OnGameWon` 广播者措辞 + APC==0 fallback OQ-Bankrupt-1)留 #9 自身 review。工单 `docs/architecture/change-impact-2026-06-04-bankruptcy-playerturn-2to9.md`。
>
> **◊◊AI 对手(10) Designed(pending review,2026-06-04 `/design-system`,lean 模式 D/H spawn systems-designer+economy-designer+qa-lead、CD gate skip)**:11 节全写。关键裁定:**AI = player-turn CR-8 三 hook(`DecideBuyProperty`/`DecidePostRollActions`/`DecideJailAction`)背后的决策逻辑**;return-only 纯叶子消费者(只读 1/3/5/6/7/8 经 snapshot,不回调2、不被反调);**启发式加权打分无前瞻**(user 裁定 MVP 上限);三档 Casual/Normal/Sharp 仅 F-7 参数差异、不作弊;一切随机走骰子3 可种子化 RNG;强制清算非 AI hook(系统驱动)。Formulas F-1..F-7(systems+economy 双 specialist),Buy Score 4 项含 BlockingValue(Normal+Sharp);57 AC(46 Logic + 4 CI-stub + 2 Integration + 4 Advisory,52 BLOCKING)。**5 继承义务全落 AC**(①AC-46 ②AC-44+47 ③AC-48 ④AC-46 N=1 回填 player-turn AC-37b ✅ ⑤AC-38 200局模拟硬阈值)。**本轮 Phase 5 跨档闭合:** ① player-turn AC-37b N=1 回填(继承义务④,fresh-grep 双侧:player-turn L528+L415 已更新);② **depends-on 订正** `3,4,5,6,7,8`→`1,2,3,5,6,7,8`(补棋盘1/回合编排2;移动4 降 Alpha——MVP 无前瞻不用距离估值,registry `steps_between` AI 标注维持 Alpha 预留)。**待办:fresh session `/design-review design/gdd/ai-opponent.md` 独立验证**(勿同会话);OQ-AI-1..7 见 GDD(OQ-AI-3 GameStateSnapshot 架构 OQ-1 ADR 为 AI 开工硬前提)。
> **↳ R-review(2026-06-05 `/design-review` full,economy/systems/ai-programmer/game/qa/unreal 6 specialist + CD synthesis):verdict NEEDS REVISION → 就地修订完成,标 NEEDS REV◊◊ 待 fresh 再审。** 工程结构干净收敛,但 6 BLOCKING(3 跨专家收敛):① F-1 Buffer 整簇(clamp 钉死 Buffer_max 致现金充裕不建房=concept 头号风险基线触发 + 贪心门只防单次最差租 + MaxRentExposure 字段 CR-6 缺失)→ top-2 暴露 + Buffer_max 抬 700/1400/1800 + CR-6 补全盘暴露视图;② F-2 BuyScore 量纲失衡(MonopolyProgress 占~86%)+ 垄断双重计入 → RentPotential 归一 [0,10] 去 ×2;③ AC-38 固定 seed 零方差却"提高 K 非降阈值"=无法 FAIL 陷阱 + ①胜率≥60% 超无前瞻上限死锁 → seed-lock 冻结阈值、①胜率降 OQ;④ F-5a expected_roi 悬空 → 删(连 PAYBACK_THRESHOLD);⑤ 头号风险"AI 犯蠢"无 gate → AC-58/59/60/60b 可枚举硬底线;⑥ 共享 RNG 流重放污染 → CR-5④ 流隔离 + AC-61。+10 must-land(CR-6 ADR propagate / AC-47 tier+计数订正 67条=61BLOCKING+4Adv / AC-62 无泄漏 + AC-63 均衡 stale 守护 / index line133 depends-on 订正 / AC-56 降 Advisory+AC-63b 可数门 / CR-7 漂移边界 / F-4 赎回-解锁 / Player Fantasy 范围诚实 / OQ-AI-3b BP-vs-C++ / OQ-AI-8/9 / CI-stub 关闭条件)。AC 计数 57→67(65 实测)。fresh-grep 核实:N=1 回填 player-turn AC-37b 真双侧落地、三 hook 签名逐字一致。详见 `design/gdd/reviews/ai-opponent-review-log.md`。**跨档 propagate 债(同 PR/下游):CR-5④ VFX(19) juice 独立流接收义务;CR-6 字段 → OQ-1 ADR 注入方;OQ-AI-3b BP/C++ 裁定 → 架构期。**
> **↳ R-3 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS〔收敛终结轮〕→ #10 Approved◊◊。** R-2 揭示的 4 BLOCKING 在散文层修了但数值/fixture/公式分层未对齐 → R-3 验出 **5 个 finishing-class verified_blocker**(均 R-2 已识别、已给处方的二阶残渣,无新正交维度、无架构返工、无支柱重裁):① **Buffer_max 偏低致 Sharp/Normal B_frac 档差中后期塌为固定常数**——F-7 Sharp Buffer_max 1800→≥2700(使 floor(1.5×1800)=2700 才触上界、覆盖 GDD 自有 Example MaxRentExposure≈1600 中后期常态);同步 F-1 上界 [600,2200]→[600,≥2700]、AC-11 fixture、L122 散文自洽、补 [Logic] AC 验 1.5:1.0 比例(数学已核:exposure=1600 时旧值差塌为固定 400);② **AC-5/6/7 GIVEN 字段清单 ↔ CR-6 L63-65 不一致**(CI-stub 无载体失效)——机械补 Rent_top1/top2/starting_cash/owner_map/board_tile_count_classic,CI-stub 阶段用固定 placeholder、注 OQ-1 ADR 关闭后换真实注入;③ **AC-61 单条 [Integration] 致 spy 断言②vacuous-pass**——拆 AC-61a [Logic 端到端重放立即可测] + AC-61b [Integration·门控 VFX(19)/HUD(16) 接入],合计行 Logic+1;④a **F-3 无补全格豁免分支致 AC-60b 断言无公式支撑**——路线 A(采纳,logged_decision):F-3 补 `(HeldInGroup+1)==GroupSize → FinalScore=BuyScore` 短路分支、变量表补 IsCompletionTile 谓词、AC-60b WHEN 引用此分支(修法落权威公式层非测试前提层,符 R-2 主审「修法落规格不落 AC 措辞」告诫);④b **F-4 赎回-解锁-建房级联为意向散文无联合现金门规格+无 AC**——规格化解锁候选谓词 + 联合现金门 + 新增正向 AC-64 [Logic](序列含有序对 [Unmortgage,BuildHouse] 且赎回先于建房)+ 负向 AC(仅够 unmortgage 不够 build 则不赎)。**CD 裁定:5 项全 finishing-class、一次性 PR 修完即 Approved,不退回 5-agent 全量复审**(blocker 质量逐轮稳定 6→4→5 且全可机械/单点路由收口,符 R-2 CD 裁定路径)。可推翻边界:若修订 PR 落地后再审发现 Buffer_max 新值或 F-3 分支引入新缝(触发其它 clamp/AC 连锁)→ 升 NEEDS_REVISION;本轮静态实据不支持预判。详见 `design/gdd/reviews/ai-opponent-review-log.md` R-3 条目。
> **↳ R-4 fresh `/design-review` 2026-06-06 = NEEDS REVISION〔R-3 过批纠正〕→ 撤回 #10 Approved◊◊。** R-3 的 5 verified_blocker 处方确已落地正文规格层,但 R-4 fresh 隔离复审揪出 **2 个新/残留 BLOCKING**:① **F-4 贪心循环守卫 `while ∃ CanBuildHouse 候选` 排除 unlock 候选**——R-3 处方 #5 新增 unlock 路径却未同步改守卫,致 AC-64 GIVEN(整组 `CanBuildHouse==false`)在权威伪代码下**结构性不可达**(散文意图 ↔ 伪代码不一致,违「修法须落规格不接受散文自述闭合」)→ 扩展守卫覆盖两类候选 + 差异化现金门(unlock 联合门/普通单门 argmax 前预过滤);② **F-4 变量表缺 `unmortgage_cost`/`BuildingCost` 操作数定义**(级联引入却仅在散文/现金门出现,违 coding-standards §4)→ 补两行注册表口径。Progress Tracker reviewed/approved 各 −1(撤回 WF-1 +1)。须用户裁定 #1 修订路线(扩展守卫 vs unlock 抽到循环前)。详见 review-log R-4 条目。
> **↳ R-5 fresh `/design-review` 2026-06-06 = NEEDS REVISION〔F-4 连续第二轮 peel〕→ 维持 #10 NEEDS REVISION◊◊。** R-4 的 2 BLOCKING 处方确已落地核验通过(F-4 守卫扩展两类候选、伪代码 L189-208 ↔ 散文 L190-214 逐字符一致、AC-64 可达、AC-65 可证伪、变量表补 `unmortgage_cost(A)`/`BuildingCost`——主审独立逐条推演核实,非采信 qa-lead claim),但 R-5 揪出 **F-4 第二轮新接缝(1 BLOCKING)**:**F-4 unlock 赎回路径(L205 `emit Unmortgage(A)`)与 F-5b 主动赎回(L222 `ShouldUnmortgage(A)`)未去重**——AC-64 核心正向场景下同一抵押格 A 必然被两路径**双发 `Unmortgage(A)`**:F-5b 单门 `Cash−unmortgage_cost(A)≥Buffer×U_frac`(U_frac∈[1.0,2.0],下界 1.0 时被 F-4 联合门 `Cash−unmortgage_cost(A)−BuildingCost(B)≥Buffer` 蕴含)同时满足,且 A 满足 `is_mortgaged ∧ A∉MortgagedThisTurn`(回合开始即抵押、churn 守卫对其失效)`∧ is_monopoly`,故同一 `DecidePostRollActions` 调用内两路径都选中 A 各 emit 一条。全档(F-4 L191-216/F-5 L218-231/CR-3④/Edge L280-287)无任何共享去重集或求值顺序声明;`MortgagedThisTurn`(L228)仅防「本回合抵押→同回合赎回」churn、不防双赎。执行层 player-turn CR-8 重校时第二条因 A 已非 `is_mortgaged` 静默跳过(不崩不负现金),但 L69 明文「单线程 AI 回合内静默跳过在 MVP 应为零常规发生——日常非零跳过 = AI 预扣逻辑 bug」——而双发跳过在 AC-58/AC-64 反幻想硬底线核心场景下**必然常规发生**,构成文档内部矛盾(CR-7/L69 零跳过纪律 ⇄ 核心场景必然双发跳过),且无 AC 证伪(AC-29 只防 churn、AC-62 只防跨调用泄漏,实现者可自由双发而测试全 green)。**处方〔autonomous〕:** F-4 贪心循环与 F-5b 共用一个 `UnmortgagedThisTurn` 去重集(任一路径 emit `Unmortgage(A)` 后 A 入集,另一路径不再选 A),或显式声明求值顺序(F-4-unlock 先评估、F-5b 对已赎格跳过);新增一条负向 [Logic] AC:同一 `DecidePostRollActions` 返回序列内同一抵押格至多一条 Unmortgage 动作(可证伪)。**因 F-4 已连续两轮(R-4/R-5)peel 出新接缝**,本轮不自证收敛——**须用户裁定去重方向**;修订 PR 落地后须 fresh-session 再审独立验证去重集落权威伪代码层、新 AC 在「A 同时满足 F-4 联合门 + F-5b 单门」fixture 下能 FAIL 双发实现,方可标 Approved。详见 review-log R-5 条目。
>
> **✦ 地产卡与卡牌 UI(17) Designed(pending review,2026-06-05 `/design-system` lean:D spawn systems-designer、Visual spawn art-director、H spawn qa-lead;B/C/E CD/specialist skip)**:11 节全写,41 AC(27 Logic〔含 5 ADR 门控 OQ-HUD-2〕+ 2 Integration + 12 Advisory)。#17=呈现层纯叶子(读 #1/#6/#8、订阅刷新、显示、转发意图,不写状态,同 HUD CR-14)。**MVP 范围**=地产/车站/公用**契据卡** + 建房/抵押动作转发;机会/命运翻牌 + 监狱卡 deferred Alpha(OQ-PC-1/5)。**关键裁定**:① `house_count` 取自 #8 不取 #6(防 6↔8 环,CR-6/AC-6);② **CR-10 棋盘抵押/垄断常驻叠加 = HUD F-4 依赖通道**;③ **V-Own-07 垄断完成视觉(色组环圈+金角标)决定 → 闭合 OQ-HUD-10**;④ F-1 三路生效行推导含经济5 同构守卫(AC-11 八边界防"卡片显示租金≠引擎实收")。**depends-on 订正** `1,6,7`→`1,6,8`(#8 是 MVP 硬读依赖 index 漏列;#7 仅 Alpha)。**propagate 债(producer,不阻):** ① OQ-HUD-10 回标 RESOLVED(回链本档 CR-10/V-Own-07);② #1/#6/#8 depended-on-by 补 #17;③ art-bible §6.2/§7.4(b) 垄断完成视觉 + §7.2 等宽字体(与 HUD CR-3 共享)+ §4.4 Utility 色组色(art-director);④ OQ-PC-2/4 handoff+widget 绑定 → 架构期 ADR(随 OQ-HUD-2)。**待办:fresh session `/design-review design/gdd/property-card-ui.md` 独立验证**(勿同会话)。
> **HUD(16) Designed(pending review,2026-06-05 `/design-system`,lean 模式:D/H spawn systems-designer/art-director/qa-lead,CD gate + B/C 专家 skip)**:11 节全写,53 AC(42 BLOCKING=38 Logic+4 Integration + 11 Advisory)。HUD=呈现层纯叶子(只读/订阅/显示,不写状态不驱动流程)。**9 条继承义务全落 AC 并回链**:player-turn AC-36(高亮≤100/≤500ms FAIL 边界,继承义务表 line78 ✅)/AC-41(席位裁定可见)/AC-44(非阻塞 AI 升序)/AC-7b(OnTurnEnded 两分支)/AC-5c(ConsecutiveDoubles);building OQ-Build-6(建房通告全员可见,line78 ✅)→AC-34/35/39;ai-opponent CR-5④/AC-61(juice RNG 独立流禁复用骰子3)→AC-47;R6 RETREAT 负向(不 gating/无回放完成回调)→AC-29;player-turn OQ-8 Submission→Fantasy+OQ-HUD-1。事件签名逐字对齐 owner GDD;双向一致性核验(board1 L14/165/304/315、economy5 L327、player-turn2 Dep HUD 行均回提 16)。无新 registry 跨档事实(呈现层无下游)。**Flags**:📌 Asset Spec(art-bible approved 后 `/asset-spec system:hud`)、📌 **UX Flag**(Pre-Production `/ux-design` 为 顶部栏/操作栏/通告区/AI 行动区/胜利覆盖层 各写 UX spec,story 引 `design/ux/[screen].md` 非直引本 GDD)。**待办:fresh session `/design-review design/gdd/hud.md` 独立验证**(勿同会话);OQ-HUD-1..8 见 GDD(OQ-HUD-2/3 BP-vs-C++ 绑定+RNG 封装→架构期 ADR)。
> **↳ R-1 design-review(2026-06-05 full,5 specialist + CD,同会话撰写+审查→独立性声明见 review-log):verdict NEEDS REVISION → 就地修订完成,标 In Review 待 fresh re-review。** 2 真 in-doc blocker:**B-1** F-4 默认值正常路径丢第3条 AI 动作(击穿支柱②,GDD 自陈 L292)→ expired 改框架推进主判据 + T_stale 软上界 4.0s + AC-56 critical 守护 + AC-30 重写;**B-2** EC-4/CR-8/AC-24 矛盾+不可求值 → 定义被迫切换触发 + AC-24 拆 a/b。AC 诚实化(渲染时刻 AC-12/24/36/4 拆逻辑/渲染、AC-47 降 Advisory·code-review、AC-26/29/40 补 spy);支柱②单屏措辞限定;net_worth 重分类。AC 53→59。**propagate 债(producer,不阻):art-bible §7.2 等宽字体指定(art-director)、#17 接收垄断组/抵押状态、UMG ADR 输入 OQ-HUD-2/7。** 详见 `design/gdd/reviews/hud-review-log.md`。
> **❖ R-2 fresh re-review(2026-06-05 full,5 specialist + CD,独立验证)= NEEDS REVISION → 就地修订完成**:3 blocker(① state_gen 幻象框架契约→改 HUD 回合代次 `E_cur` in-doc;② `FAIActionDetails` 无 criticality→**跨档 propagate `EActionType` 已落 player-turn L264**,OQ-HUD-12 RESOLVED;③ reduce-motion 无 AC→AC-57)。CR-11 gating 假警报查实排除(player-turn L25/31/33 是被 R6 RETREAT 取代的历史日志)。AC 59→61。
> **❖ R-3 fresh re-review(2026-06-05 full,5 specialist + CD synthesis)= NEEDS REVISION〔finishing-class,收敛〕→ 就地修订完成 = APPROVED**(CD 裁定 finishing 末圈,严重度逐轮单调下降〔R-1 系统级→R-2 跨档幻象→R-3 谓词/未传播兄弟/标签收口〕,无需 R-4)。主审独立 fresh-grep 核实 `EActionType` 真落 player-turn L264、`E_cur` 额外回合安全(player-turn L197 不重发)、AC-29/AC-56 spy 非 vacuous(deflate qa 过度声称)。5 must-land:AC-57 永真谓词重写+T_min 旁路、V-Own-10/AC-21 Bounce→Cubic、AC-12/24a/30/57 ADR 门控标条目、V-Own-07 删"底部操作栏或"、EC-11/AC-48 BP/C++ 绑定去重。7 Recommended 收口。AC 61→62。**propagate 债(producer,不阻):art-bible §7.2 等宽字体+§7.5 高亮曲线 Cubic(art-director)、#17 抵押/垄断状态、OQ-HUD-2/3 UMG ADR(状态机独立 UObject+IGameClock DI+RNG 封装,为 AC-12/24a/30/57 headless 硬前提)。**
>
> **⟡ 主菜单与房间大厅(20) APPROVED**(R-1 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕):8 必需节全写 + Open Questions + Flags,24 AC(16 [Logic·含 AC-2/3/15 越界防守] + 1 [Integration·BLOCKING AC-13 开局移交] + 2 [Integration·BLOCKING AC-14/15 默认值&防守对齐] + 4 [Advisory·code-review/playtest-signoff] + AC-12/6/20 Recommended)。#20=呈现/编排层纯叶子(同 HUD CR-14:不持运行态、不驱动流程、不定序、不被对局反调,#20→#2 单向无环);MVP 范围收敛为本地热座 2–4 配置 + AI 设置 + 席位/颜色互斥分配 + 开始游戏构造 `FGameSetupConfig` 移交回合2。**继承义务全覆盖**:本系统**作为下游**无 "Inherited Test Obligations" 表登记给 #20 的条目(该表无 #20 行);**作为上游契约履行方**——player-turn 主菜单大厅(20) 依赖(L251/L419 开局传参+初始化 `PlayerState` 列表)→ **AC-13/AC-14/AC-15** 覆盖、ai-opponent L340 难度选择 UI 归 #20 写 `PlayerState.AIDifficulty`(字段归回合2/行为语义归 AI10 F-7)→ **AC-7/AC-10/AC-14/AC-20** 覆盖、player-turn AC-27 越界 P 双层防守对齐 → **AC-2/AC-3/AC-15** 覆盖。**B-1 名字段对齐**已采 Option A 全档对齐 owner(setup 层 `FString` 编辑态 → F-2 `BuildSetupConfig` 边界 `FText::FromString` → `PlayerState.DisplayName:FText`,逐字对齐 player-turn L79,**非** `PlayerName/FString`)。**遗留 OQ(不阻开工,设计冻结/下游设计/架构期处理)**:OQ-Lobby-1(回合2 开局入口具名 `StartNewGame(const FGameSetupConfig&)` + `FGameSetupConfig`/`FPlayerSetupEntry` USTRUCT 归属归回合2 + B-1 双向回链 → producer `/propagate-design-change`,AC-13 接口名待此闭合冻结)、OQ-Lobby-2(续局读 #21 接口未定→入口禁用/隐藏)、OQ-Lobby-3(House Rules 面板 #23 Alpha)、OQ-Lobby-4(手动 turn order #23 Alpha)、OQ-Lobby-5(联网房间 #25 Full Vision)、OQ-Lobby-6(全 AI 禁止 Alpha 产品决策)、OQ-Lobby-7(`EPlayerColor` 8 色枚举值与 art-bible §5.2 一一对应核对,producer 轻量)。**Flags(不阻)**:📌 UX-spec(Pre-Production `/ux-design` 为 主菜单屏/房间大厅配置屏各写 UX spec 到 `design/ux/main-menu.md`/`design/ux/lobby.md`,AC-21/22/23/24 呈现层验收在 UX spec 细化,story 引 UX spec 非直引本 GDD;难度选择屏 UX 归 #20,ai-opponent L342 已明确)、📌 Asset-spec(art-bible approved 后 `/asset-spec system:main-menu-lobby`)。blocker 量首评即 0=健康收敛终结(verified_blockers=[]、logged_decisions=[])。见 review-log R-1 条目。
>
> **☼ 存档/读档(21) APPROVED**(R-1 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕):8 必需节全写 + Open Questions,26 AC(20 [Logic·BLOCKING] + 4 [Integration·BLOCKING 存→读→比对 headless 可跑] + 2 [Advisory·code-review/真机磁盘冒烟])。#21=持久化横切层(enable-not-own,不拥有任何被序列化字段语义,只"采集→序列化写盘 / 读盘→反序列化→重建→重绑"),核心防漏机制=CR-3 逐状态系统可序列化契约清单 + 金标准 round-trip identity(存档前快照==读档后快照逐字段 identity-check,无"调参直到通过"逃逸)。**继承义务全覆盖落 AC**(对照 line~86 存档行):① 精确回合阶段 + ConsecutiveDoubles + 锁定阈值(player-turn OQ-T-2/AC-34)→ AC-4/5/6;② 完整 `FDiceRollResult`(含 Die1/Die2,dice OQ-T-Dice-5/B1)→ AC-7/8;③ 读档后重绑 delegate(player-turn AC-34 末句/CR-6)→ AC-9/10/11;④ economy 每玩家 `Cash`(economy OQ-T-Econ-4)→ AC-12;⑤ Raising Funds 瞬态不中途存档(economy OQ-Econ-4/CR-4)→ AC-13/14/15。**关键裁定**:`bIsBankrupt` 归回合2 PlayerState(破产9 无独立存档腿);`house_count` per-tile [0,5] 归建房8(非6,防 6↔8 环);棋盘存 DA 引用 `BoardIdentifier` 不存全量布局(CR-2);MVP 不序列化骰子 `Seed`(当前骰由完整 `FDiceRollResult` 保护、读档重设新非确定种子防 EC-8 序列重复);读档严格相等版本判据(F-2,不迁移)。**3 logged decisions(finishing-class,纯文档收紧不改 carrying contract,不阻开工)**:① AC-1 + CR-3 玩家回合行显式补点名 `CurrentTileIndex`(player-turn L83 PlayerState 字段、owner=移动4,L389 要求读档逐一还原 11 字段;原显式枚举清单漏列致实现者照清单写测试可能漏断言棋子位置→读档回退默认 0 隐形损坏)→ 已就地展开 11 字段去"等"字吞位;② L185 双向一致性登记 building 反向行(R-1 一度因 building 侧零命中标"假覆盖、移 OQ-Save-5 待补侧";**现 building-upgrade.md L225 已补全反向行 `| 存档(21) | 软(横切) | 供 per-tile house_count([0,5])序列化读;读档写回 | 21→8 |`,fresh-grep 双侧复核 → OQ-Save-5 ① building 反向行 RESOLVED**,carrying contract `house_count` 对齐 building L57 核实正确)→ 已闭合;③ OQ-Save-2 fallback 自判路径收口:**MVP 默认锁权威路径**(AC-13/14/15 以回合2 报告可存档为 GIVEN),自判 fallback 标"临时降级、无 AC 守护、回合2 接口到位后移除"(R-1 fresh-grep:player-turn `bIsSafeToSave`/`CanSaveNow`=0 命中,接口未提供;若实现 fallback=enable-not-own 轻度越权),裁决方向留架构期 ADR。**遗留 OQ(不阻开工)**:OQ-Save-1(`USaveGame` 宿主 + 写盘原子性 ADR,与回合2 OQ-1 协调)/ OQ-Save-2(回合2 可存档点查询接口)/ OQ-Save-3(多槽/云存/autosave Alpha+)/ OQ-Save-4(版本迁移 Beta)/ **OQ-Save-5**(propagate 债:①building 反向行补全 = **RESOLVED**〔building L225 已落、双侧 fresh-grep 核实〕;余 open:主菜单20 `DoesSaveGameExist` + 呈现层 HUD16/VFX19/property card17 监听 `OnGameLoaded` 重绑,producer `/propagate-design-change`)/ OQ-Save-6(联网存档语义 Full Vision)。blocker 量首评即 0=健康收敛终结(verified_blockers=[])。见 review-log R-1 条目。
>
> **♫ 音频 SFX/BGM(22) APPROVED**(R-1 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕):11 节全写(8 必需 + Visual/Audio 资产语境 + Open Questions),27 AC(22 [Logic·BLOCKING] + 2 [Integration·BLOCKING AC-23 事件格7 链路 / AC-24 经济5+回合2 破产-胜利链路] + 3 [Advisory·playtest-signoff 节奏/情绪/响度])。#22=呈现层声音唯一 owner 纯叶子(只订阅 owner multicast delegate→触发 Sound Cue 播放+混音,不写状态、不裁决结算、不阻塞框架;owner 先算定结果,#22 收到即播,落后只丢声音不回灌),与 VFX19 由 **enable-not-own** 厘清(#22 拥声音资产/混音总线/事件→声音映射,#19 拥视觉 juice,同一事件各订各播互不双触发)。**继承义务全覆盖落 AC**(对照 line~86-95 继承义务表 #22 平行于 VFX19/HUD16 复用同组 owner 事件、各订各播):掷骰 juice 听觉腿(dice OQ-T-Dice-4)→ CR-3/AC-1/2/3/4 + Advisory AC-25;经济现金 juice 听觉腿(economy Visual,Sensation priority-4)→ CR-4/AC-5/6;建房+地产 juice(承接 #8/#6)→ CR-5/AC-7/8/9/10/11;RNG 隔离(ai CR-5④/VFX19 CR-9 禁复用骰子3 权威流)→ CR-7/AC-22;非阻塞追赶/跳过(HUD F-4/VFX19 CR-8)→ CR-9/EC-12;读档重绑防双订阅(EC-8)→ AC-21。**关键裁定**:① **声音唯一 owner**(VFX19 拥视觉、#22 拥听觉,声画分离非歧义);② `EChangeReason` 未注册值→中性兜底音(EC-5,镜像 VFX19 EC-17,运行期安全);③ 建房 Build/Sell 由本地 `house_count_cache` newCount delta 派生(镜像 VFX19 CR-6)+ `OnOwnershipChanged` re-seed 封破产清算 cache 漂移(AC-9,镜像 VFX19 Finding A);④ 破产音订**经济5 `OnBankruptcyDeclared`**(2 字段)、非直订破产9,与 VFX19/HUD16 一致;⑤ MVP 混音三路总线(SFX/BGM/Master)+ BGM duck 简化为音量阶跃(非动态 ducking)+ `PlayUISound` 为 #22 对 UI 层唯一流出 API(CR-11,UI 主动请求非状态订阅);动态音乐分层/自适应配乐/空间化全降 Alpha(OQ-Audio-1)。**5 专家裁决分裂(3 APPROVED_WITH_FOLLOWUPS vs 2 NEEDS_REVISION)→ verdict 取 APPROVED_WITH_FOLLOWUPS(1 logged decision)**:2 位 BLOCKING-camp 赖以升级的核心机制(AC-5 锁死幻象枚举→编译失败/false-green)经主审逐条对文核实**不存在**——AC-5 fixture 实际只用合法值 Salary/Tax/未注册枚举值,无锁死幻象枚举;真分歧(`EChangeReason` 字面、继承义务表缺 #22 行)均(a)逐字继承自已 Approved 姊妹档 VFX19、(b)带中性兜底运行期安全、(c)义务已落本档 AC → 判为 propagate 债而非 in-doc BLOCKING,无需重裁「#22 是什么」(可逆边界:propagate 批落地时若 VFX19 CR-5 在其 Approved 历程被显式裁为合法 owner-extend 承诺则 #22 完全继承无须再动;若 producer 裁定 economy 不扩枚举、下游须改用 9 真值,则 audio CR-4 与 VFX19 CR-5 同批重写——两路径均不影响本轮 audio 开工)。**遗留 OQ/propagate 债(不阻开工)**:OQ-Audio-1(动态音乐分层/自适应配乐/ducking/3D 空间化 Alpha)/ **OQ-Audio-2**(propagate 债 producer:各 owner GDD「事件供 UI/VFX 订阅」措辞平行补「音频(22)」消费方〔VFX19 已 line 86 登记〕 + `PlayUISound` 在 UI 屏 GDD #16/#20/#23 登记被调义务 + 破产事件接缝 OQ-VFX-13〔`OnBankruptcyDeclared` vs `OnPlayerBankrupt`〕#22 随 VFX19/HUD16 一致订经济5,producer 改订时同步)。**Flags**:📌 Asset-Spec(art-bible approved 后 `/asset-spec system:audio` 生成逐资产音效 WAV spec)、非 UI 屏无 UX-spec flag。blocker 量首评即 0=健康收敛终结(verified_blockers=[]、deflated 1 幻象枚举机制、logged_decisions=1)。见 review-log R-1 条目。

---

## Inherited Test Obligations (跨系统测试义务追踪)

> **目的(player-turn R2 / qa-lead BLK-3 引入):** 上游 GDD 审查时产生的"下游须含某集成测试"承诺(OQ-T-*),若只写在上游 GDD 的 Open Questions 里,下游 GDD 作者不会读到 → 责任真空(board-data 教训)。此表使承诺在**下游系统这一行**可见。**`/design-review` 审查下游系统时,qa-lead 须核对本表对应义务已落入该 GDD 的 Acceptance Criteria。**

| 下游系统 | 继承义务 | 来源 | 回链 AC |
|---|---|---|---|
| 玩家回合(2) | **(dice OQ-T-Dice-1)** 开局定序调 `RollDice()` 只取 `Total`、忽略 `bIsDouble`、不进双点链(已在 player-turn CR-2),并须告知 VFX"当前=定序阶段"语境使其抑制定序双点强化反馈;**(dice OQ-T-Dice-5 ✅ propagate RESOLVED 2026-06-02)** AC-34 序列化字段已从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`(含 Die1/Die2);player-turn 7 处 RECEIVE+SERIALIZE 契约已更新(IG-1 已执行);**(movement OQ-T-Move-1)** ResolvePhase 须消费 `OnPawnLanded.arrivalContext`,`SentToJail` 时**不触发**买地/收租结算分支(movement AC-15 抑制侧归 2)。**✅ propagate RESOLVED(2026-06-02,grep 核实)**:此前 player-turn.md 全文 grep `SentToJail`/`arrivalContext`/`OnPawnLanded` = 0 命中、核心规则("去坐牢却能买落地格地")无 BLOCKING 覆盖;现 player-turn 已补 **AC-46**(`arrivalContext==SentToJail`→ResolvePhase 买地决策/收租结算入口 hook 调用次数==0,对照组 DiceMove==1)+ **AC-47**(程间非重入)+ **CR-3.1**(holder + 程间原子性)+ Edge Cases 抑制规则;实 AC 号已回填 movement AC-15。movement 侧透传由 AC-12b[Logic] 覆盖 | dice OQ-T-Dice-1 / OQ-T-Dice-5 / movement OQ-T-Move-1 | player-turn 定序 AC / AC-34(已扩字段) / **✅ OQ-T-Move-1 RESOLVED → player-turn AC-46 / AC-47** |
| 事件格(7) | "双点出狱不进双点链"集成测试(player-turn OQ-T-1);**(dice OQ-T-Dice-3)** 掷双点出狱调 `RollDice()` 取 `bIsDouble`、出狱双点不进双点链、骰子只提供原值不解释出狱语境;**(economy OQ-T-Econ-3)** 须把"前进到最近公用/车站"额外骰点作 dice_total 传入经济 F-4、机会/命运卡金钱效果经经济 Credit/Debit;**(movement OQ-T-Move-2)** 须经移动 `TeleportTo(paysGo=true)` 实现"前进到最近X"、`TeleportTo(JailIndex,paysGo=false)` 实现去坐牢,并保证 `target∈[0,N−1]` | player-turn OQ-T-1 / dice OQ-T-Dice-3 / economy OQ-T-Econ-3 / movement OQ-T-Move-2 | **✅ tile-events.md 已落 AC(待 design-review 验)**:player-turn OQ-T-1 → AC-37/AC-50;dice OQ-T-Dice-3 → AC-37/AC-51;economy OQ-T-Econ-3 → AC-52/AC-53;movement OQ-T-Move-2 → AC-54 |
| 存档(21) | "中途还原精确阶段 + ConsecutiveDoubles + 锁定阈值 + **完整 `FDiceRollResult`(含 Die1/Die2,不可只存 `(Total,bIsDouble)`——否则读档后 VFX 无法重现骰面;dice OQ-T-Dice-5/B1)** + 读档后重绑 delegate 监听器"集成测试;**(economy OQ-T-Econ-4)** 序列化每玩家 `Cash`;Raising Funds 瞬态不中途存档(与回合2协同,OQ-Econ-4) | player-turn OQ-T-2 / dice OQ-T-Dice-5 / economy OQ-T-Econ-4 | AC-34 |
| AI 对手(10) | ① "PostRollAction AI 同步决策正常完成 + 回合顺畅移交"集成测试;② "AI 内部 RNG 仅走骰子(3) `RandomRange`/`RandomFloat01`、禁引擎全局随机" code-review checklist + 集成测试(**dice OQ-T-Dice-2**:可重放路径优先整数 `RandomRange`,但与 `RandomFloat01` 跨平台同源、Full Vision 联网须服务器权威,见 dice F-4);③ GameStateSnapshot 字段齐备验证;④ **(R3)定义并回填 AC-37b 的 N(移交帧数上限)**;⑤ **(R3)钉"三档 AIDifficulty 产生玩家可感知行为差异"契约(OQ-6)** **— ✅ 全部已落 ai-opponent.md AC(2026-06-04 `/design-system`):①AC-46 ②AC-44+47 ③AC-48 ④AC-46(N=1 回填 player-turn AC-37b)⑤AC-38(200局模拟硬阈值)。** | player-turn OQ-T-3 / OQ-6 / dice OQ-T-Dice-2 | **ai-opponent AC-46/44/47/48/38;player-turn AC-37b(N=1)** |
| VFX(19) | **(dice OQ-T-Dice-4,B5/rec-11,CD R2 裁定 MUST-FULFILL 移交)** 掷骰爽感端到端 owner **由 VFX(19) 视觉腿 + 音频(22) 听觉腿共担**(VFX 拥期待→翻滚→定格节奏的视觉 + 双点强化视觉反馈;音频拥同三阶段听觉节奏 + 双点庆祝/入狱警告音,见 audio AC-25)+ 背 experiential acceptance criteria;用 `Die1`/`Die2` 分别呈现两骰面值(禁从 `Total` 拆解);双点强化反馈差异化(庆祝 vs 入狱警告)须从 player-turn 取 `ConsecutiveDoubles`、不可仅凭裸 `bIsDouble`;定序阶段抑制双点强化反馈(语境由 player-turn 告知)。**qa-lead 在 VFX(19) design-review 时须核对其 AC 已含此承接,未承接 = VFX(19) GDD 的 blocking 缺口**;**(economy Visual节)** 收租金币飞溅/发薪入账/破产清算 juice 端到端 owner(监听 `OnRentPaid`/`OnCashChanged`/`OnBankruptcyDeclared`,concept Sensation priority-4);**(movement OQ-T-Move-3)** 棋子逐格 hop 回放端到端 owner(监听 `OnPawnMoveStarted` + 过 GO 高亮 + `OnPawnLanded` 落地 juice);hop path 由 VFX 自建 vs 移动提供 `HopPath` 归 VFX 裁定(movement OQ-Move-2) | dice OQ-T-Dice-4 / economy Visual节 / movement OQ-T-Move-3 | ✅ **vfx-feedback.md 已落(R-8 Approved)**:hop juice 承接 → **CR-4**(订 `OnPawnMoveStarted`/`OnPawnLanded`、过 GO 高亮、hop path 来源裁定);experiential felt-quality → **AC-55**(掷骰三阶段 feel)/ **AC-56**(hop 逐格屏息 feel)`[Advisory·playtest-signoff]` |
| HUD(16) | "OnPhaseChanged/OnTurnStarted 驱动 UI 高亮(≤100ms 起/≤500ms 完成)" + "OnAIActionExecuted(ActionIndex) 非阻塞逐步播放 AI 行动(按 ActionIndex 排序)" + "OnTurnOrderResolved 席位裁定可见"集成测试;**(R3 B-2)Pass'N Play handoff 高亮与前一回合 outro 不叠帧**;**(R6 RETREAT,取代 R5 计数收敛)框架不再 gating——HUD 自主非阻塞呈现 AI 行动,不阻塞框架推进;删除原 OnAIActionPlaybackSegmentComplete 回放完成回调义务**;消费 OnTurnEnded(回合切换过场,payload bGrantsExtraTurn 区分同玩家继续/移交,两分支须各有动画 AC);**(building OQ-Build-6,2026-06-04 propagate)消费 player-turn `OnBuildingAnnounced`(payload TileIndex/NewHouseCount/ActingPlayerId)呈现全员可见建房通告 beat——恢复 CR-8「仅自己回合建房」削弱的支柱②社交引信** | player-turn OQ-T-4 + Fantasy→HUD 契约 + **building OQ-Build-6** | **✅ 全部已落 hud.md AC(2026-06-05 `/design-system`,待 design-review 验)**:高亮 ≤100/≤500ms→**AC-12**(回链 player-turn AC-36,FAIL 边界);席位可见→**AC-26/27**(回链 AC-41);AI 非阻塞升序→**AC-28**(回链 AC-44);Pass'N Play 不叠帧→**AC-24**(R3 B-2);R6 RETREAT 不 gating/无回放完成回调→**AC-29**(负向);OnTurnEnded 两分支→**AC-22/23**(回链 AC-7b);OnBuildingAnnounced 全员可见通告→**AC-34/35/39**(building OQ-Build-6) |
| 地产所有权(6) | **(economy OQ-T-Econ-1)** 须实现 owner/is_monopoly/station_count/utility_count/is_mortgaged 接口供经济算租(**house_count 归建房8、不在6,经回合2 ResolvePhase 聚合 6 快照 + 8 house_count 后传经济**),**且经 push 模型把归属快照作参数传入经济算租调用**(经济 Dep 双向一致性①,保 index 无环);字段供给正确性 + push 快照时机归 6 | economy OQ-T-Econ-1 | ✅ **property-ownership.md 已落(Approved)**:算租接口供给 → **F-1**(`is_monopoly`)/ **F-2**(`station_count` 全盘)/ **F-3**(`utility_count` 全盘);**AC-15**(垄断正例)/ **AC-18**(抵押不破坏垄断,租金 0 由 economy 管)/ **AC-20**(空集守门 vacuous-true 防护) |
| 破产胜负(9) | **(economy OQ-T-Econ-2)** 破产判据/net_worth 用经济 F-9 net_liquidation_value 估值口径(AC-27③单一入口);**资产移交是 in-kind,AC-14 用 identity-check(逐对象枚举)、禁用「总额==F-9」**;局末排名 net worth 口径(可能含未抵押 face value,≠nlv)归 9 裁定(OQ-Econ-5/OQ-Bankrupt-2) | economy OQ-T-Econ-2 / OQ-Econ-5 | ✅ **OQ-Bankrupt-3 RESOLVED 2026-06-04**(propagate:economy line479 改 identity-check + line247/446 补建筑拆除归9调8;fresh-grep 双侧核实,范围较登记收窄——②已 economy R1 自修)。bankruptcy.md Designed,AC-14 identity-check |
| 建房(8) | **(economy OQ-T-Econ-5)** F-8 building_sellback 比率归经济、建筑清单归 8 供 F-8/F-9 枚举;建/卖房现金经经济执行 | economy OQ-T-Econ-5 | ✅ building-upgrade.md(F-5 引用比率、F-6 供 GetPlayerBuildings 枚举,Designed 2026-06-04) |
| 经济(5) | **(movement OQ-T-Move-4，R2 Finding E 改 PULL)** 消费移动 push 的 `(passedGo, SalaryAmount)`:`passedGo>0` 触发 F-1 发薪(`passedGo×SalaryAmount`)、`passedGo≤0`/无 push 不发薪——发薪部分**已由 economy AC-6/AC-7 履行**。**Utility 租 `dice_total` 改为经济在 ResolvePhase 从回合掷骰上下文 PULL**(movement 不投递 Total，对齐 economy AC-18 不缓存)——经济须保证"当前正在结算这一程" `Total` **单源**、连续双点链/事件额外骰(OQ-T-Econ-3)下不取错值。**holder 机制 + 单源去歧义 = OQ-Move-5(跨 doc propagate，涉回合2/经济5/事件7);另含 `DiceMultiplierTable[i]` 上界校验防 `12×mult` int32 溢出(OQ-Econ-10 不覆盖)**。movement 侧只断言不投递(AC-7b)。**✅ propagate RESOLVED 2026-06-02:OQ-Move-5 holder 落 player-turn CR-3.1 + economy line 57/102/330 PULL 清理 + economy Interactions 玩家回合行 holder 源;`DiceMultiplierTable` 上界落 board-data AC-23j(economy OQ-Econ-10 并轨注)。** | movement OQ-T-Move-4 / OQ-Move-5 | economy AC-6 / AC-7（发薪）/ F-4 / **AC-18（dice_total 不缓存，PULL 契约）/ player-turn CR-3.1(holder)** |
| 音频(22) | **(dice OQ-T-Dice-4 听觉腿)** 掷骰爽感声音端到端 owner(期待→翻滚→定格三阶段听觉节奏 + 双点庆祝/入狱警告音可辨)与 VFX19 视觉腿共担(声画分离、各订各播);**(economy Visual节 听觉腿)** 收租金币音 / 发薪入账音 / 破产清算音端到端 owner(订经济5 `OnRentPaid`/`OnCashChanged`/`OnBankruptcyDeclared`,破产音订经济5 非直订破产9,与 VFX19/HUD16 一致;不按金额缩放,CR-4);**(tile-events L109 订阅义务)** 订事件格7 `OnPlayerJailed`/`OnPlayerReleased`/`OnCardDrawn` → 入狱/出狱/抽牌音(端到端挂接 #7 owner delegate)。**与 VFX19 由 enable-not-own 厘清:#22 拥听觉、#19 拥视觉,同一 owner 事件各订各播互不双触发。** | dice OQ-T-Dice-4 / economy Visual节 / tile-events L109 | **audio AC-25(掷骰听觉)/ AC-6・AC-24・AC-26(收租・破产-胜利・情绪)/ AC-23(事件格7 三音链路)** |

---

## Categories

| Category | Description | 本作系统 |
|----------|-------------|----------|
| **Core** | 框架级地基，几乎所有系统依赖 | 棋盘数据、玩家回合、骰子 |
| **Gameplay** | 让游戏好玩的机制 | 移动、事件格、破产胜负、AI、命运之轮、俄罗斯轮盘、策略卡 |
| **Economy** | 资源/金钱的产生与消耗 | 经济现金、地产所有权、建房升级、交易、拍卖 |
| **UI** | 面向玩家的信息显示与反馈 | HUD、地产卡 UI、交易拍卖 UI、VFX、主菜单大厅 |
| **Persistence** | 存档与连续性 | 存档/读档 |
| **Audio** | 音效与音乐 | 音频 SFX/BGM |
| **Meta** | 核心循环之外的系统 | 设置&House Rules、教程、地图编辑器 |
| **Networking** | 多人联网（Full Vision） | 联网 |

---

## Priority Tiers

| Tier | Definition | Target Milestone | Design Urgency |
|------|------------|------------------|----------------|
| **MVP** | 核心循环运转所需。没有它们无法验证"是否好玩" | 首个可玩原型（本地+AI） | Design FIRST |
| **Alpha** | 全部特色机制（拍卖/命运之轮/俄罗斯轮盘/策略卡）就位 | Alpha 里程碑 | Design SECOND |
| **Full Vision** | 联网、地图编辑器、打磨 | Beta / Release | Design as needed |

> 本作未单列 Vertical Slice 系统层——垂直切片复用 MVP 系统集 + 一项特色机制（拍卖），不引入新系统。

---

## Dependency Map

### Foundation Layer（近零依赖）

1. 棋盘数据（1）— 格子布局/地图定义，所有空间逻辑的地基（零依赖）
2. 玩家与回合管理（2）— 2–8 玩家、回合顺序、当前玩家，流程框架。**depends-on: 3**（player-turn GDD 接口分析发现：RollPhase/双点 F-3/定序 F-4 均消费骰子输出；原标零依赖，已修正）
3. 骰子（3）— 随机数与掷骰结果，移动与多个事件的输入源（零依赖纯 RNG 服务）

> **设计顺序提示（R 发现）**：系统#2 依赖#3，但设计顺序中 #2 先于 #3。因 #3 是 S 级纯 RNG 服务且 #2 仅消费 `(点数, bIsDouble)` 契约（已在 player-turn GDD 定义），契约清晰、无返工风险。建议下一个设计 **骰子(3)** 以闭合此契约。

### Core Layer（依赖 Foundation）

1. 经济与现金（5）— depends on: 1, 2 ｜⚠ bottleneck：租金/抵押/建房公式发源地（economy GDD Phase 5 补 +1：经 GetTileData 读金额 base）
2. 移动（4）— depends on: 1, 2, 3｜**orchestrated → 经济(5)**：移动把 `(passedGo, SalaryAmount)` 在回合编排下 push 给经济发薪；Utility 租 `dice_total` 改由经济在 ResolvePhase 从回合掷骰上下文 **PULL**（移动不投递 Total，R2 Finding E 对齐 economy AC-18 不缓存）。非硬依赖、不成环（movement GDD R2 2026-06-02）
3. 地产所有权（6）— depends on: 1, 5
4. 事件格（7）— depends on: 1, 2, 3, 4, 5, 8(soft)（+3：掷双点出狱调骰子，dice GDD Phase 5 同步；**+4：调移动 `TeleportTo` 实现"前进到最近X"/去坐牢，movement GDD R-move 2026-06-02 同步**；**+8(soft)：RepairFee 牌读建房8 `GetTotalHouseCount`/`GetTotalHotelCount`,/consistency-check 2026-06-04 揪出,OQ-Event-3 RESOLVED;非环 8 不依赖 7**）

### Feature Layer（依赖 Core）

1. 建房升级（8）— depends on: 1, 5, 6
2. 破产与胜负（9）— depends on: 2, 5, 6, 8（R-xreview 2026-06-03:补 6——9→6 破产归属移交 `TransferOwnership`/`ReturnTileToBank`;此前 OQ-Prop-4 propagate 只改枚举表 line28 漏改此处。**补 8(2026-06-04 #9 设计揪出:9→8 调 `LiquidateAllBuildings` + 收租破产 in-kind 建筑随格,building 已列 9 为 dependent,index 此前漏 8)**;`2→9` 为 orchestration invoke——回合2 调 `9.ResolveBankruptcy`,9 return-only 不回调 2,非硬循环依赖，类比 movement4→经济 push）
3. AI 对手（10）— depends on: 1, 2, 3, 5, 6, 7, 8 ｜⚠ 消费几乎所有 gameplay（R-review 2026-06-05 订正：补 1 棋盘估值底数 + 2 回合编排；4 移动降 Alpha——MVP 无前瞻不用距离估值，registry `steps_between` AI 标注维持 Alpha 预留；+3：AI 随机走骰子可种子化 RNG）
4. 交易（11）— depends on: 5, 6
5. 拍卖（12）— depends on: 5, 6
6. 命运之轮（13）— depends on: 5, 7
7. 俄罗斯轮盘（14）— depends on: 2, 9
8. 策略卡（15）— depends on: 5, 6, 7

### Presentation Layer（包裹 gameplay）

1. HUD（16）— depends on: 1, 2, 5
2. 地产卡与卡牌 UI（17）— depends on: 1, 6, 8（+7 Alpha）  <!-- R-1 订正:原 1,6,7 误;#8 是 MVP 硬读依赖(house_count/建房门/卖回额),#7 仅 Alpha 翻牌卡 -->
3. 交易/拍卖 UI（18）— depends on: 11, 12
4. 游戏反馈 VFX（19）— depends on: 3, 4, 5, 8, **2, 6, 1**（**✪ #19 设计揪出补 2/6；R-1 design-review 再补 1**：+2 经 `OnPhaseChanged` payload 读 `ConsecutiveDoubles`/定序语境 + `OnTurnStarted`(epoch) + `OnGameWon`（**`OnBuildingAnnounced` 已从 #19 移除→归 HUD banner，R-1 Issue 2**）；+6 监听 `OnOwnershipChanged`/`OnMortgageChanged` 买地/抵押 juice；+1 棋盘 `GetTileWorldTransform`/环序 N（锚点 tile-center + 自建 hop path，R-1 RB-4）；+3 监听 `OnDiceRolled` 读 Die1/Die2/Total 呈现两骰；破产 juice 经经济5 `OnBankruptcyDeclared`，非直依赖 9。OQ-VFX-7 propagate：#2/#6/#1 depended-on-by 应含 #19 + line 83 回链 AC 补 AC-55/56/28-32（仍 pending）；**OQ-VFX-10 ✅ RESOLVED（R-2，#19 本地 newCount delta 派生取代 propagate，EBuildDirection 跨档依赖删除）；~~遗留:building(8)↔player-turn(2) `OnBuildingChanged` 字段漂移~~ **✅ RESOLVED 2026-06-06**(WF-3 gate-check 揪出为 open BLOCKING；用户裁定**方案②**:player-turn 不从 building 事件读 actingPlayerId 第3字段，改由 `ResolvePhase` 当前回合上下文 `CurrentPlayerId` 取〔建房只在持有者自己回合发生，归属玩家恒等于当前回合持有者〕；building 全档保持 2 字段 `OnBuildingChanged(tile,newCount)` 不变、5 个 consumer 未动；player-turn CR-3.5 L163 + AC-53 已改，双侧 fresh-grep 核验闭合)，与 #19 解耦**）
5. 主菜单与房间大厅（20）— depends on: 2

### Polish Layer（依赖广泛）

1. 存档/读档（21）— depends on: 1, 2, 5, 6（横切所有状态系统）
2. 音频（22）— depends on: 2, 3, 4, 5, 6, 7, 8（音效订阅各系统事件；硬订 dice3/player-turn2/movement4/economy5/property6/building8 + 事件触发 7，对齐 L41 表行与 audio GDD L214-219）
3. 设置 & House Rules（23）— depends on: 1, 5
4. 教程/引导（24）— depends on: 4, 5, 6
5. 联网（25）— depends on: 几乎全部 ｜⚠ Full Vision 大重构
6. 地图编辑器（26）— depends on: 1

---

## Recommended Design Order

| Order | System | Priority | Layer | Agent(s) | Est. Effort |
|-------|--------|----------|-------|----------|-------------|
| 1 | 棋盘数据（1） | MVP | Foundation | game-designer | M |
| 2 | 玩家与回合管理（2） | MVP | Foundation | game-designer / systems-designer | M |
| 3 | 骰子（3） | MVP | Foundation | game-designer | S |
| 4 | 经济与现金（5） | MVP | Core | economy-designer | L |
| 5 | 移动（4） | MVP | Core | game-designer | M |
| 6 | 地产所有权（6） | MVP | Core | economy-designer | M |
| 7 | 事件格（7） | MVP | Core | game-designer | M |
| 8 | 建房升级（8） | MVP | Feature | economy-designer | M |
| 9 | 破产与胜负（9） | MVP | Feature | systems-designer | S |
| 10 | AI 对手（10） | MVP | Feature | systems-designer / ai-programmer | L |
| 11 | HUD（16） | MVP | Presentation | ux-designer | M |
| 12 | 地产卡与卡牌 UI（17） | MVP | Presentation | ux-designer | M |
| 13 | 游戏反馈 VFX（19） | MVP | Presentation | technical-artist | M |
| 14 | 主菜单与房间大厅（20） | MVP | Presentation | ux-designer | S |
| 15 | 存档/读档（21） | MVP | Polish | systems-designer | M |
| 16 | 音频（22） | MVP | Polish | sound-designer | S |
| 17 | 交易（11） | Alpha | Feature | economy-designer | M |
| 18 | 拍卖（12） | Alpha | Feature | economy-designer | M |
| 19 | 命运之轮（13） | Alpha | Feature | game-designer | S |
| 20 | 俄罗斯轮盘（14） | Alpha | Feature | game-designer | S |
| 21 | 策略卡（15） | Alpha | Feature | systems-designer | M |
| 22 | 交易/拍卖 UI（18） | Alpha | Presentation | ux-designer | M |
| 23 | 设置 & House Rules（23） | Alpha | Meta | game-designer | S |
| 24 | 教程/引导（24） | Alpha | Meta | game-designer | M |
| 25 | 联网（25） | Full Vision | Networking | network-programmer | L |
| 26 | 地图编辑器（26） | Full Vision | Meta | game-designer | L |

> Effort：S = 1 个会话，M = 2–3 个会话，L = 4+ 个会话。同层独立系统可并行设计。

---

## Circular Dependencies

- **None found**。依赖图为有向无环：UI 依赖 gameplay 而 gameplay 不反向依赖 UI；AI 消费多系统但不被它们依赖；存档依赖状态系统而非反向。无需打破环。

---

## High-Risk Systems

| System | Risk Type | Risk Description | Mitigation |
|--------|-----------|-----------------|------------|
| 经济与现金（5） | Design | 租金/抵押/建房公式是 bottleneck，平衡错误会让全盘玩法崩坏；且"一局拖沓"通病源于经济平衡 | 早原型调参，公式数据驱动。**⚠ R-xreview 2026-06-03(D-1):concept 承诺的三个抗拖沓杠杆中 MVP 实际仅 `STARTING_CASH`(1500/750)可用——`max_laps/max_rounds` 标 Open/默认禁用/归 Alpha 破产胜负(9);"破产加速"无任何 MVP GDD 旋钮(F-10 严格 `<`)。MVP playtest 抗拖沓只有起始资金一根杠杆,与最高 Design Risk"后期拖沓"叠加,后期体感风险集中在 MVP。max_rounds 待 House Rules(23)、破产加速待破产(9)落地。** |
| AI 对手（10） | Design / Technical | MVP 靠 vs AI 验证，AI 太蠢直接毁掉核心体验；需可靠的地产估值与现金流决策 | 早原型，分难度档；估值逻辑数据驱动 |
| 联网（25） | Technical / Scope | 状态同步、回合权威、断线重连，是全项目最大技术风险 | 严格隔离到 Full Vision；先单机验证全玩法，联网作为叠加层 |
| 存档/读档（21） | Technical | 横切所有状态系统，序列化遗漏会导致读档损坏 | 状态系统设计时即定义可序列化契约 |

---

## Progress Tracker

| Metric | Count |
|--------|-------|
| Total systems identified | 26 |
| Design docs started | 16 |
| Design docs reviewed | 16 |
| Design docs approved | 16 |
| MVP systems designed | 16 / 16 |
| Alpha systems designed | 0 / 8 |
| Full Vision systems designed | 0 / 2 |

---

## Next Steps

- [ ] Design MVP-tier systems first（按设计顺序，从 棋盘数据 开始，用 `/design-system`）
- [ ] 优先验证高风险系统：经济现金、AI（早原型）
- [ ] Run `/design-review design/gdd/[system].md` on each completed GDD
- [ ] Run `/gate-check systems-design` when all MVP GDDs are complete
- [ ] Validate highest-risk systems with `/vertical-slice` before committing to Production
