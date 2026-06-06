# 玩家与回合管理 (Player & Turn Management)

> **Status**: APPROVED ◊ (2026-06-04 R-2to9 RR:`/design-review` 重过三锁面 **APPROVED**——三锁面 F-2 胜负口径 / 受控写接口面 `bIsBankrupt` 调用方 / 胜负信号 `OnGameWon` 经主审 fresh-grep 双侧 + systems-designer/qa-lead/game-designer + CD 终审确认架构干净、双侧对齐;**真 blocker=0**。专家 4 项 must-land AC-hardening followup 已就地应用:AC-40 拆 a/b(C++ stub 不降级守 return-only CI 底线)+ 新增 AC-40c(封堵旧 2↔9 双触发 bug 复发回归守护)+ CR-6 钉边沿触发规格 + AC-12 收紧 + AC-19/F-2 Example 时序前提。工单 `docs/architecture/change-impact-2026-06-04-bankruptcy-playerturn-2to9.md`。跨档 followup(bankruptcy L75/L189 `OnGameWon` 广播者措辞 + APC==0 fallback 返回破产者 OQ-Bankrupt-1)留 bankruptcy 自身 review。OQ-1 BLOCKING-ADR + OQ-8 仍开放。原 R8:0 blocking,6 recommended,gating 接缝终结)
> **Author**: msc + agents
> **Last Updated**: 2026-06-01
> **Implements Pillar**: ④易上手轻松局、②社交博弈（回合顺序=谁行动）
> **System**: #2 in systems-index design order | Foundation | Core | MVP
> **Review Mode**: lean(authoring) → full(R1) → fresh re-review full(R2) → full(R3) → full(R4) → fresh re-review full(R5, 5 specialist + CD) → fresh re-review full(R6, 5 specialist + CD — MAJOR REVISION,用户裁定 B-1 gating RETREAT) → fresh re-review full(R7, 5 specialist + CD — NEEDS REVISION「可收口纸面尾巴」,3 blocker 单遍收口已应用) → fresh re-review full(R8, 5 specialist + CD — **APPROVED**,0 blocking,gating 接缝终结) → **R-2to9 RR**(focused 三锁面 propagate re-review,systems-designer + qa-lead + game-designer + CD — **APPROVED**,真 blocker=0,4 must-land AC-hardening followup 就地应用) — 每轮多 specialist + creative-director senior synthesis
> **R1 修订摘要**: 5 项 blocking 收口 + 低成本 recommended。详见 `design/gdd/reviews/player-turn-review-log.md`。
>   ① F-1 守卫先行(消哨兵-枚举矛盾) ② AI 决策改同步+dev看门狗+ResolvePhase/JailTurn hook ③ 双点阈值开局锁定 ④ AC-35 措辞降级+强度推 ADR ⑤ F-4 round 全局计数。
> **R2 修订摘要**: fresh re-review 确认 R1 原 5 blocking 全部真闭合;但 CR-8 异步→同步迁移引入 4 项新 blocking,本轮收口。
>   ① dev 看门狗删虚拟时钟版(同步死循环单帧不可捕获)→'永不返回'靠测试框架超时、'超单帧预算'靠事后帧检查;AC-37a 第二段改写。
>   ② DecidePostRollActions 批处理失败策略=逐动作重校+不可行静默跳过+dev日志、不中断;补 AC-42。
>   ③ 快照契约等价降级为软约束(对齐 AC-35,单线程BP+禁Latent下竞态理论不可达);AC-35a 移入 OQ-1 待 ADR 后补回。
>   ④ OQ-T-1~4 回链强制力:systems-index 下游行加 test-obligations 标注。
>   同批 recommended: NR-1 AI 可观察性 hook(OnAIActionExecuted+AIActionDisplayMode);GameStateSnapshot 开口指向 OQ-1;OQ-1 ADR 清单补 C-1~5+APlayerState 排除;AC-39/40/41 覆盖缺口;AC-12/29/34/37b 措辞修;F-4 子组内裁定/F-3 认知矛盾/F-1 守卫语义澄清。
> **R3 修订摘要**: R2 mandate「CR-8 接缝正交全枚举」首次系统化执行,8 项新 blocking 全命中 mandate 清单(同接缝剥面收口),无架构返工。
>   ① **B-1(CR-8 同步 AI 否决支柱②观察对手)**:CD 推翻 R2 降级、升 BLOCKING;**用户裁定采纳 Animated gating**——`AIActionDisplayMode=Animated` 下框架进下一 TurnStart 前等呈现层回调 `OnAIActionPlaybackComplete`(dev 超时兜底);`Instant` 不等(保 OQ-3 确定性重放逃生舱)。改 CR-8 退出协议 + L105 + 状态表 + Tuning + 新 hook。
>   ② **CR-8 四 hook 失败语义整簇 codify**:BLK-1 DecideBuyProperty 现金不足=执行期可行性校验归经济5/所有权6、不可行视同 false+dev日志(补 AC-38b);BLK-2 DecideJailAction 非法返回(无现金保释/无卡)=校验归事件格7、降级留狱(补 AC-39b);BLK-3 DecideAuctionBid **现在钉死 sentinel 值域约定**(负/INDEX_NONE=放弃、0 非法、须>当前最高价且<=Cash、违反视同放弃、校验归拍卖12)。
>   ③ **AC 守门面收口**:BLK-R3-1 AC-37c 删「断言 UE_LOG fire」改纯文档化人工 dev 验证(斩断 wall-clock 后门复活);BLK-R3-2 AC-40③ 退化局标记=**OnLastPlayerStanding payload 增 `bDegenerateTie`**(给可测载体);BLK-R3-3 AC-42① 改状态断言(被跳过动作目标状态未改)、UE_LOG 降括注。
>   ④ **B-2**:Fantasy→HUD handoff 补可测下限(前一回合 outro 完成→才起 handoff 高亮,不叠帧)。
>   同批 recommended: GameStateSnapshot per-hook 字段判据 + 事件格7 注入出狱卡持有(OQ-1⑦);RNG 走骰子3 软约束强度坦诚标注(OQ-T-3);PostRollAction 空数组=EndTurn 显式声明(补 AC-37d);单动作跨系统可行性聚合主责点明;OQ-1 标 P0/P1 分层;PlayerState 容器=轻量 UObject vs snapshot=USTRUCT 判据;AC-34 读档「反序列化→重绑 delegate→才广播」时序;OnTurnEnded 下游登记(HUD16);快照A/B 时序作用域显式分隔;F-4 P=4 全员同分 Example;F-1 L202 mod 论证落点;CR-4 downtime 缓解改开关+局长;早出局玩家 post-elimination OQ;AIDifficulty 可感知差异契约;AC-35a 补 ADR 关闭前置条件锚点;继承义务节升级建议(转 OQ)。
> **R4 修订摘要**: CD 强制窄域 fresh re-review。本该收口,却从 R3 自引入的 B-1 gating 接缝剥出 3 新面 + 1 mandate② cosmetic 残留 + 1 F-1 论证洞。4 blocking 收口,零架构返工。
>   ① **BLOCK-1 AC-38b③ log-as-oracle 残留**(mandate② 未斩干净,5 专家全确认):删③编号、断言收敛纯可观测状态、log 降括注对齐 AC-42。
>   ② **BLOCK-2 B-1 回调粒度**(用户裁定:全部播完后单次回调):呈现层须等本回合**全部** OnAIActionExecuted 播完才**单次**回调 OnAIActionPlaybackComplete,否则第1条动画后即解 gating、后续 N-1 条被截断、支柱②在多动作回合失效;AC-44 补 N>1 fixture。
>   ③ **BLOCK-3 B-1 生产超时**(用户裁定:拆双阈值):gating 超时兜底拆 **dev 诊断阈值** + **生产安全阀阈值**(宽松~30s、强制推进+遥测),防 Animated 生产流程永久卡死 show-stopper。
>   ④ **BLOCK-4 gating 状态机相位**:L162 状态表加显式 `AIPlaybackGating` 相位(Animated-only、存档语义/非法转移拒绝/AC-44 fixture 注入点有落点);OQ-1 新增因子⑯(gating 相位 ETurnPhase 落点,P0)+ ⑬展开 OnAIActionPlaybackComplete 两形态约束。
>   同批 recommended: F-1 cur=-P 枚举终止洞(入口 cur 规范化,纳入 R5 强制确认);F-2 APC==0 WinnerId=INDEX_NONE;AC-39b/AC-44③ 补 OQ-1⑤ 可注入条件门 + log 降括注;OQ-5 论据修正(OnPlayerBankrupt 追加不破坏 L184);三事件 payload 清单;OQ-1⑩ SaveGame 桥接 API 误述修正 + GC 陷阱 + P0/P1 校正;CR-4 局长旋钮 cross-ref。Player Fantasy MDA framing 转 OQ-8(待 CD 复核)。
> **R5 修订摘要**: CD 强制窄域 fresh re-review。出现质变信号——B-1 gating 的核心契约介质(R4「单次无参 OnAIActionPlaybackComplete」)在协议层无法承载 AC-44 断言(框架收无参信号即推进、无法区分正常单次 vs per-action 早释);同时 mandate③ 命中真本体洞:F-1 cur=-P 修法只落注释、主公式 L225 从未更新(F-1 第二次"修法落注释不落权威规格")。6 blocking 收口,零架构返工。
>   ① **B-R5-1 F-1 主公式三层落地**(systems 1-A,主会话+CD 核验):L225 主公式 + 变量表落 `cur_safe=((cur%P)+P)%P`,删 L238 旧"A 成员性过滤"并存论证,明确 ensureMsgf 检查原始 cur;AC-17c 扩三变体(cur=-4 整除/cur=-2 非整除/cur=5 正越界)。
>   ② **B-R5-2/3 B-1 gating 介质改造=框架侧计数收敛**(ai ISSUE-1/3 + qa F-2 收敛;用户裁定采纳):回调改 `OnAIActionPlaybackSegmentComplete(ActionIndex)`(逐段带 index),框架持 `PendingPlaybackSet`/`CompletedPlaybackSet`、集合相等时解 gating;per-action 早释被集合判据天然拦截(AC-44①c 变真 [Logic]);N≡实际广播数 M(被跳过动作不广播 OnAIActionExecuted)。改 CR-8 B-1/L122/L204-205/状态表/Tuning/Visual/HUD16 行。
>   ③ **B-R5-4 AC-44③ 虚假 [Logic] gate**(ai ISSUE-2 + qa F-3):改 `[Logic·条件]` 默认 [Advisory]、删"旋钮置极小值"wall-clock 后门、钉 `SimulateTimeout()` 可注入接口(ADR 关闭后升 [Logic])、遥测须 mock 载体。
>   ④ **B-R5-5 存档语义钉直接推进**(unreal #1 + game-designer F-R5-7,用户裁定):L173 + OQ-1⑯ 删二选一、钉"读档落 gating→视已完成直接推进"(选项 A 技术不可实现:FAIActionDetails 未序列化),消解 FTimerHandle 不可序列化(unreal #4)。
>   ⑤ **B-R5-6 ConsecutiveDoubles gating 期归零时机**(systems 3-A):归零在 gating 解除后实际移交时执行,存档落 gating 时仍持本回合值、读档直接推进后归零。
>   同批 recommended: AC-5b 补 AIPlaybackGating 非法转移(unreal #3);AC-39b ② 降级留狱 +1 对称(ai ISSUE-5);AI 双点链不 gating 声明(systems 3-B);生产安全阀 30s 与 HUD 动画上限协调契约(F-R5-2);snapshot 生成时机 vs gating(ai ISSUE-4/6);OQ-T-4 bGrantsExtraTurn 两分支 + OQ-T-3⑤ bIsAI=false 防护(qa F-8/9);payload 重编译警告扩全部 struct + BP pin 静默断线(unreal #6);OQ-1⑬ 推荐形态(a)/⑯ 已裁/⑭ 占位最低完整性/P0 减负(unreal #2/5);OQ-8 期限改 HUD 开工前(F-R5-4);OQ-5 重申升 MVP 建议(F-R5-6);新增 OQ-9 多 AI gating downtime 缓解阀(F-R5-3,Alpha)+ L90 认领。
> **R6 修订摘要(MAJOR REVISION → 用户裁定 RETREAT)**: CD 强制窄域 fresh re-review。**mandate ①③④ 逐字符核验真闭合**(F-1 主公式 cur_safe 三层一致+AC-17c 三变体算术正确;AC-44③ 时钟语言已斩;存档语义单一钉死)——"修法落注释不落规格"失效模式本轮**未复发**。**但 mandate ②⑤ gating 接缝触发结构性不收敛信号**:5 专家从 5 正交方向同时剥新面、全部 root 到 R5 引入的计数收敛介质(ActionIndex 跨双点链作用域/跳过编号/0-1based/自增无 AC;集合谓词 == 死锁;破产截断广播;Timer double-fire;sync-vs-Tick;payload 生产侧 AC 缺口)。CD 判定为 R5 预注册收敛测试触发("R6 仍剥新面=结构性不收敛"),**裁定 MAJOR REVISION**(质变:执行细节→"该不该做")。
>   ① **用户裁定 RETREAT(取代 R3 用户裁定的框架 gating)**:Animated gating 从**框架硬保证**降回**呈现层软约定**——框架在 AI 同步决策执行完即时 EndTurn、不阻塞呈现回放;保留 `OnAIActionExecuted(ActionIndex)` 广播供 HUD 非阻塞展示,删除 `PendingPlaybackSet`/`CompletedPlaybackSet`/`OnAIActionPlaybackSegmentComplete`/`AIPlaybackGating` 相位/双阈值超时。支柱②"AI 可观察"降为 HUD 自主呈现职责(信息层兑现,放弃强制观看过程——观察 AI 的 Fellowship 价值本就微弱,game-designer charge#1)。
>   ② **gating 接缝相关全删**:CR-8 回放 gating 契约/退出协议、状态表 AIPlaybackGating 行+转移、AC-5b②、payload segment 回调、Tuning 三 gating 旋钮、AC-44(重写为非阻塞 hook 测试)、OQ-1 因子⑬⑯(P0 降至 ①②③⑦⑪⑭)、OQ-3 gating 兼容评估、OQ-9(RESOLVED)、Visual/L98 reframe。零架构返工。
>   ③ **retreat 后仍存活的独立修复(qa BLK-R6-3/4 + systems 发现-1)**:新增 AC-7b(`OnTurnEnded.bGrantsExtraTurn` 生产侧)/AC-5c(`OnPhaseChanged.ConsecutiveDoubles` 生产侧)防 payload 死字段;F-1 边界 L263 `mod(cur+1,P)`→`mod(cur_safe+1,P)`(第五面规范化对齐)。
>   ④ **独立待办(与 retreat 无关,保留 OQ 跟踪)**:OQ-8 Player Fantasy 缺 Submission 维度(game-designer 判设计层 BLOCKING,期限 HUD 开工前,CD 复核 framing)。
> **propagate(2026-06-05,HUD(16) R-2 design-review → OQ-HUD-12):** `FAIActionDetails`(L263)落定开口字段 **`EActionType`**(枚举)——HUD V-Own-07 文案/图标 + F-4 critical 判定(hud AC-56)依赖。**纯加性**:不改 R6 RETREAT/gating 语义、不动 AC-44(其测 `ActionIndex` 排序+非阻塞,与 `EActionType` 正交)。owner=本系统。零架构返工。

<!-- 骨架由 /design-system 创建。逐节设计：新会话运行 `/design-system 玩家与回合管理` 自动从下一个未完成节续做。 -->
<!-- 关键约束（Phase 2 上下文，供续做参考）：
     - 零依赖 Foundation；被依赖：移动(4)/经济(5)/事件格(7)/破产胜负(9)/俄罗斯轮盘(14)/HUD(16)/主菜单大厅(20)/存档(21) —— 接口要稳
     - 预答锁定：MVP 2–4 本地+AI（预留 2–8/联网接口不展开）；本系统拥有中心 PlayerState 实体（id/名/色/是否AI/位置TileIndex/现金值/监狱状态/破产标志）
     - 归属边界：经济(5)拥有改现金的公式、移动(4)拥有改位置的逻辑；本系统拥有数据与回合流程
     - 玩家位置以 TileIndex 为键（board-data 已锁定运行时可变状态分离）
     - 编码标准：数值数据驱动、不硬编码；引擎 UE5.7 Blueprint 为主
-->

## Overview

玩家与回合管理系统是《骰子大亨》的**流程地基**:它定义"谁在玩、轮到谁、一个回合如何开始与结束"。它拥有一份中心的**玩家状态实体 `PlayerState`**——记录每位玩家的身份(id / 名字 / 颜色)、是否 AI、当前位置(以棋盘 `TileIndex` 为键)、现金值、监狱状态与破产标志——并按固定顺序在玩家间传递行动权。本系统是**纯框架/数据层**:它拥有"现在轮到谁、玩家处于什么状态"的权威记录与回合推进的状态机,但**不拥有改变这些状态的规则**——经济(5)拥有改现金的公式、移动(4)拥有改位置的逻辑、事件格(7)/破产(9)拥有触发监狱/破产的判定;本系统只持有数据字段并按它们驱动回合流转。MVP 支持 **2–4 名本地玩家**(同屏轮流 Pass'N Play + AI),并为未来 2–8 人/联网**预留接口而不展开实现**。整套状态全部数据驱动、可序列化,使存档(21)能干净保存"这局有哪些玩家、各自什么状态、轮到谁"。对玩家而言,这个系统体现为围坐一桌**清晰公平的轮流节奏**(支柱④易上手)与"轮到我了"的**掌控期待**(支柱②社交博弈);它做得好时玩家不会注意到它,只感到流程顺畅不卡顿。

## Player Fantasy

> *lean 模式:`creative-director` 未咨询(仅高风险节派发专家)。本节由主会话起草,生产前建议人工复核 framing。*

玩家与回合管理服务**两层体验**。

**直接层**:玩家清楚地知道"现在轮到谁、我能不能动、我的回合什么时候结束"——这种确定性是围坐一桌玩桌游的安全感来源。当屏幕明确高亮"轮到你了",玩家获得一种被点名的**小小聚光灯感**:所有人的注意力此刻在我,我来掷骰、我来决策(支柱②社交博弈的舞台)。回合结束、行动权清晰地交到下一个人手里,没有"现在到底该谁了"的混乱——这种**公平、清晰、有序的轮流**正是休闲桌游让人放松的核心(支柱④易上手)。

**基础层**:玩家永远不会想"这是一份 PlayerState 数据表"。当回合流转正确无误——破产的人被干净地移出顺序、轮次始终不重不漏、读档后还是轮到该轮的人——玩家什么都不会注意到。这个系统的成功标准与棋盘数据一样是**隐形的正确**:它把"谁该行动"处理得如此自然,以至于玩家全部情绪都能投向真正的博弈(买地、收租、互坑),而非浪费在"流程怎么走"的困惑上。

换言之,这个系统的幻想是:**让一桌人像围着实体棋盘一样自然地轮流,而把"谁、何时、按什么顺序"的全部机械记账隐藏在水面之下。**

## Detailed Design

### Core Rules

> *lean 模式:`systems-designer` 未咨询(仅 Section D/H 派发)。本节由主会话起草。*

**CR-1 中心玩家状态实体 `PlayerState`(本系统拥有数据,但部分字段的改写规则归他系统):**

| 字段 | 类型 | 说明 | 改写规则归属 |
|---|---|---|---|
| `PlayerId` | int32 | 唯一标识,开局分配后恒定 | 本系统(开局) |
| `DisplayName` | FText | 玩家名 | 本系统(大厅20配置) |
| `TokenColor` | EPlayerColor | 棋子颜色(预留 8 色枚举,开局唯一分配) | 本系统 |
| `bIsAI` | bool | 是否 AI 控制 | 本系统 |
| `AIDifficulty` | EAIDifficulty | None/Casual/Normal/Sharp;非 AI 为 None(预留,AI系统10消费) | 本系统 |
| `CurrentTileIndex` | int32 | 当前位置(board `TileIndex`) | **移动(4)** 拥有改写逻辑 |
| `Cash` | int32 | 现金值 | **经济(5)** 拥有改写公式 |
| `bIsInJail` | bool | 是否在狱 | **事件格(7)** 拥有进/出狱规则;本系统持字段 |
| `JailTurnsServed` | int32 | 已服刑回合数(用于"满3回合强制交保释"判定,规则归7) | 本系统计数、7 定规则 |
| `bIsBankrupt` | bool | 破产标志 | **破产胜负(9)** 拥有判定 |
| `TurnOrderIndex` | int32 | 回合环中的位置 0..P-1 | 本系统(开局定序后恒定) |

> 边界契约:本系统**持有**全部字段并据其驱动回合流转,但**不裁决** `CurrentTileIndex`/`Cash`/`bIsInJail`/`bIsBankrupt` 何时变、变多少——那是移动/经济/事件/破产各自的规则。本系统对外暴露**读接口**与**受控写接口**。
>
> **受控写接口面(具名清单,R-xreview 2026-06-03 补全 —— 防下游凭空命名/异名):** 本系统对每个委派字段暴露**唯一具名**受控写接口,调用方严格受限:
> | 字段 | 受控写接口 | 唯一合法调用方 |
> |---|---|---|
> | `CurrentTileIndex` | `SetPosition(index)` | 移动(4) |
> | `Cash` | `SetCash(value)` / `Debit`/`Credit` 经济侧封装 | 经济(5) |
> | `bIsInJail` | `SetJailState(bInJail, reason)` | 事件格(7)(进/出狱规则归7) |
> | 当前程掷骰上下文 | `SetCurrentRollContext(FDiceRollResult)` | 事件格(7)(仅事件额外掷骰更新 holder,见 CR-3.1) |
> | `bIsBankrupt` | `SetBankrupt(bool)` | **本系统(回合2)**,据破产胜负(9) `ResolveBankruptcy` 返回 `debtorEliminated=true` 写入(9 为 return-only 编排、**不直写本字段**——防 2↔9 环,见 CR-6 / 破产9 CR-5) |
>
> ⚠ **`SetPosition` 与移动(4) `SetTileIndex` 的关系(OQ-Move-3b 交叉引用):** 移动(4) 对**其他系统**暴露 `SetTileIndex`/`Advance`/`TeleportTo` 作为位置写的公开 API;本系统的 `SetPosition` 是 `PlayerState.CurrentTileIndex` 的**字段级**受控写,仅供移动(4)内部最终落位调用。二者是「移动公开 API → 本系统字段 setter」的两层关系还是同层别名,由 **OQ-Move-3b ADR** 裁定,实现期须收敛为单一命名链;在裁定前两档以本注交叉引用消歧(消除 movement `SetTileIndex` ↔ player-turn `SetPosition` 的静默异名)。
>
> ⚠ **受控写是架构契约,强度待 OQ-1 ADR 裁定(R1 措辞校正)**:受控写接口是一项**架构层约定**,其强制力取决于 OQ-1 ADR 的容器实现选择,而非引擎级保证。UE 中 `BlueprintCallable` 无调用方限制,BP-primary 项目**无法在引擎层硬性阻止**任意持有引用的 BP 图调用 setter——只能靠 (a) C++ 强封装(字段 private + C++ setter,BP 只读 = 真硬保证)或 (b) BP 约定 + code-review(软约束)。OQ-1 ADR 须在 (a)/(b) 间裁定并写明强度;AC-35 随之分叉(见 Acceptance Criteria)。本契约的目标是"明确唯一合法写路径",而非声明一个 BP 兜不住的工程级防篡改保证。

**CR-2 开局回合定序(经典掷骰定序):** 开局阶段每位玩家各掷一次骰,点数高者取 `TurnOrderIndex=0`(先手),其余按点数降序顺时针排布;**平手者在平手玩家间重掷**直至分出。定序完成后顺序**全局固定**:行动权按 `TurnOrderIndex` 递增、到 `P-1` 后回环到 `0`。

> **定序掷骰形态(R1 澄清,供骰子3接口闭合):** 定序掷骰用标准双骰(与回合内一致),但定序阶段**只取点数和、忽略 `bIsDouble`**,不进双点链(`ConsecutiveDoubles` 开局为 0,定序不累加)。骰子(3) 接口无需为定序提供单骰模式。
>
> **席位裁定可见性(R1 修正,反聚光灯最小修):** 达 `MAX_TIEBREAK_ROUNDS` 后的席位升序裁定**不得静默**——须经事件(`OnTurnOrderResolved`)告知呈现层,让 HUD/VFX 以"快速可见裁定"呈现最终顺序,而非玩家不知为何某人先手。规则确定性不变,仅要求呈现层有反馈钩子。
>
> **开局定序串行等待呈现缓解(R2 提示,game-designer NR-5):** Pass'N Play 下开局每人各掷一次是**串行掷骰仪式**(P 人各掷,无策略意义的纯等待),是支柱④"快速开始"的真实代价(非平手重掷概率问题,而是首轮 P 次各掷)。本系统不改规则(忠实①),但**提示** HUD(16)/VFX(19):开局定序应有**参与感呈现**(如各玩家点数对比动画、掷骰结果即时高亮),把等待转为仪式。本系统经定序流程暴露每人掷骰结果供呈现(非静默累计)。具体设计归呈现层,此处仅标"应缓解"。

> **⚠ 支柱权衡注(R1 新增,CR-2 与 CR-4 共用):** CR-2 掷骰定序与 CR-4 双点额外回合是**经典大富翁/Rento 的标志性规则**,服务**支柱①桌游忠实(MVP 立身之本)**。design-review 指出二者在 Pass'N Play 下有体验代价——CR-2 最坏情形开局等待偏长、CR-4 他人等待一人连走多回合(支柱②④的 downtime)。**裁定(creative-director):忠实为默认,不删规则;通过 House Rules(23) 可选开关 + 呈现层缓解(席位裁定可见、额外回合的回合切换 juice)兑现支柱②④的诉求**。即:默认忠实满足①,可选简化阀满足④,两支柱不互相牺牲。CR-4 额外回合的可选关闭开关在 House Rules(23) 接口登记(见 Tuning Knobs / Dependencies)。
> **⚠ 缓解手段分轨校正(R3,game-designer RECOMMENDED):** 上句"呈现层缓解"对 **CR-2 开局定序**(一次性、开局、危害有限)成立——点数对比动画确够。但对 **CR-4 双点连走的局中 downtime**(走运玩家连走 2-3 完整回合、他人纯等待)**方向相反**:`Animated` 呈现只会**加长**等待。**CR-4 downtime 的真缓解 = `bEnableDoublesExtraTurn` 开关(真逃生阀)+ 局长压缩旋钮,而非呈现层**。呈现层缓解不负责 CR-4 的 downtime。
> **⚠ B-1 gating downtime 同族认领(R5 → R6 RESOLVED by RETREAT):** R5 曾指出 B-1 Animated 框架 gating 在多 AI 局放大 downtime(1 人类 + 3 AI 局人类每轮前观看 3 AI 回合 9-24s 纯等待)。**R6 RETREAT 删除框架 gating 后此 downtime 消失**——AI 回合即时移交,HUD 非阻塞展示。原 CR-4 双点 downtime(本系统不改规则)仍由 `bEnableDoublesExtraTurn` 开关 + 局长旋钮缓解(见 L84/Tuning),与已删除的 B-1 gating downtime 无关。详见 OQ-9(已解决)。

**CR-3 回合结构(粗粒度阶段状态机,内容委派):** 一位玩家的一个回合 = 固定阶段序列,本系统拥有**序列与转换**、每阶段**内容委派**:

| 阶段 | 本系统职责 | 委派给 | 决策点(人类 via UI / AI via 同步 hook) |
|---|---|---|---|
| `TurnStart` | 激活当前玩家;检查破产(跳过)/在狱(转 Jail 分支) | — | — |
| `RollPhase` | 请求掷骰,接收完整 `FDiceRollResult`(Die1/Die2/Total/bIsDouble),消费 `Total`+`bIsDouble` | 骰子(3) | — |
| `MovePhase` | 委派移动 `点数` 步 | 移动(4) | — |
| `ResolvePhase` | 委派落地结算(租金/买地/抽牌/税/发薪);**落无主地产时暴露买/不买决策点**;**算租前聚合 6 归属快照 + 8 house_count 传经济(见 CR-3.2)** | 事件格(7)/经济(5)/所有权(6) | **买地决策**(无主地产);**拍卖出价**(拒买后,Alpha) |
| `PostRollAction` | 收集可选动作(建房/抵押/交易)直到"结束回合"信号 | 建房(8)/交易(11)/AI(10) | **建房/抵押/交易**决策序列 → `EndTurn` |
| `TurnEnd` | 判定双点额外回合 or 移交下一玩家 | — | — |

`JailTurn` 分支额外含**出狱决策点**(交保释/掷双点出狱/用出狱卡,规则归7)。

本系统在阶段转换处广播事件(`OnTurnStarted`/`OnPhaseChanged`/`OnTurnEnded`)供他系统挂接。详见 CR-8 的 AI 决策契约。

**CR-3.1 当前程骰上下文 holder + 程间原子性(R3 propagate,承接移动 CR-3.1/CR-4):** 本系统是「当前程掷骰上下文」的 **holder**——`RollPhase` 收到的完整 `FDiceRollResult` 由本系统持有,在 `ResolvePhase` 期间将其 `Total` 暴露给经济(5) Utility 租 **PULL**(经济 F-4 显式参数消费、不缓存、不向移动索取;见经济 CR-3/F-4 与移动 CR-3.1「移动不投递 Total」)。本系统拥有**当前程 `Total` 单源**:一回合内多程位移(如机会牌"前进到最近公用"额外掷骰)时,保证经济读到的是「正在结算的这一程」的骰点,不串程;事件额外掷骰的 `dice_total` 由事件格(7)经本系统受控写接口 **`SetCurrentRollContext(FDiceRollResult)`**(见上方受控写接口面)更新 holder,再由经济在 ResolvePhase PULL(经济 OQ-T-Econ-3;事件格 AC-52 回链此具名接口)。**程间非重入(承接移动 CR-4 把程间原子性委托回合):** 本系统**不得**在前一程移动的 `OnPawnLanded` 同步返回前发起第二程 `Advance`/`TeleportTo`——多程位移严格串行,前程落地结算(ResolvePhase)未完不开下一程,杜绝同步嵌套重入(`OnPawnLanded` 同步 Broadcast,监听者回调内同步触发新 `Advance` = 同步嵌套,"禁 Latent 天然成立"不覆盖此路径,见 AC-47)。

**CR-3.2 ResolvePhase 算租输入聚合契约(R-xreview 2026-06-03 补 —— 关闭 spine owner 真空):** 算租所需的归属事实分散在两个系统(地产所有权6 持 owner/monopoly/mortgage/station/utility;建房8 持 house_count),经济(5) F-2/F-3/F-4 需要它们作为**已聚合的参数**传入。**本系统(`ResolvePhase`)拥有这一聚合编排职责**——下游 economy:104(「收(push,经 ResolvePhase 聚合)」)/ property-ownership:107(「落地结算编排聚合 6 快照 + 8 house_count 传经济」)/ systems-index 继承义务表均把聚合指向本阶段,本契约在 spine owner 侧确权(消除「三个下游都说回合2会聚合、回合2自己不声明」的责任真空)。

- **聚合步骤(收租落地时):** ① 调地产所有权(6) `BuildOwnershipSnapshot(playerId, tileIndex)` 取归属快照(`owner`/`is_mortgaged`/`is_monopoly`/`station_count`/`utility_count`,值拷贝,property AC-30b);② 读建房(8) `house_count`;③ 二者拼装为算租输入,调经济(5)算租入口(经济从本系统 PULL 当前程 `dice_total` 供 F-4,见 CR-3.1)。本系统**只编排聚合与传递**,不持有/不裁决任何归属或租金公式(归 6/8/5)。
- **MVP house_count 缺省(建房8 = Not Started):** MVP 阶段建房(8)未实现、`house_count` 无供给方时,本聚合点**注入缺省 `house_count = 0`** → 经济 F-2 落到 `RentTable[0]`(或垄断时 `RentTable[0]×monopoly_rent_multiplier`)的无房分支(economy:147-148)。建房(8)落地后,本步骤②改读其真实 `house_count`,聚合契约不变。此缺省是本阶段**唯一**合法的 house_count 来源声明(对齐 tile-events F-3 的 provisional 处理,消除算租路径在 MVP 的 undefined)。

**CR-3.3 ResolvePhase 强制清算驱动循环(Raising Funds 阻塞子相,OQ-Build-1 RESOLVED):** 当玩家现金不足以支付应付款项(租金/税/保释金等)时,`ResolvePhase` 进入 **Raising Funds 阻塞子相**,本系统作为流程 owner **驱动**清算循环直至现金够或资产耗尽。本系统拥有**执行驱动**;经济(5)拥有**顺序 spec + 现金判据**(`Cash≥amount_due`/`is_insolvent`);本系统绝不自行裁决顺序——顺序由经济规格注入。

- **清算循环(止损优先 mortgage-empty-first,用户裁定 R4):**
  ```
  while Cash < amount_due:
      若存在可直接抵押地 (owner==player ∧ 未抵押 ∧ (GetHouseCount(tile)==0)):
          调 6.Mortgage(MV 最小的可抵押地)     // 止损优先:抵押可赎回,零半价损失
      else 若存在建筑 (任一 GetHouseCount(tile) > 0):
          调 8.ForcedSellNextBuilding(player)  // building F-4 全盘最高档;卖空某组后该组 Property 转可抵押 → 回到抵押腿
      else:
          break → 调 economy.is_insolvent(player, amount_due, preaggregated_nlv) → 破产(9)
  ```
- **抵押前置 GetHouseCount 检查(building CR-7 接缝):** 本系统在调 `6.Mortgage(tile)` 前**必须先读** `8.GetHouseCount(tile)` 确认 `==0`——带房地不可抵押(经典规则:须先卖完该组建筑)。`GetHouseCount > 0` 的地跳过抵押腿、转入卖房腿。
- **有界终止保证:** 玩家资产有限(地产数 + 建筑数均有上限),每次循环迭代至少减少一笔资产;最终 `Cash≥amount_due`(早停)或所有资产耗尽→ `is_insolvent`→破产(9)。
- **无 economy→8 / economy→6 反向边:** 经济(5)仅提供顺序 spec 与现金判据,**不**调用 8 或 6 执行任何卖房/抵押操作——执行腿全部由本系统(回合2)发起。这是本接缝存在的根本理由(消 5→8 / 5→6 环,对齐 building-upgrade CR-9 接缝标注1 委托)。
- **区分自愿与强制:** CR-3.3 强制清算仅在 Cash < amount_due 时触发。自愿建/拆(building CR-4/CR-5)与自愿抵押(PostRollAction 窗口)不经此阻塞子相。
- **参考接缝:** building-upgrade CR-9 接缝标注1 把清算执行驱动委托至"回合2 `ResolvePhase`";本 CR 是该委托的 spine-owner 侧确权。

**CR-3.4 破产判据 NLV 聚合(全组合扫描,OQ-Build-2 RESOLVED):** 破产判据路径(确定玩家是否 `is_insolvent`)需要**全投资组合**净可变现价值 `preaggregated_nlv`。此聚合与 CR-3.2 收租聚合**形态不同**——收租聚合是单格快照(per-tile),破产聚合是全组合枚举。本系统在 Raising Funds 子相进入 `is_insolvent` 判断前,负责此全组合聚合:

- **聚合步骤(破产路径):** ① 调建房(8) `GetPlayerBuildings(player)` → `[(tile, house_count)]`(全组合建筑枚举);② 读所有权(6) 各地抵押价值(`MortgageValue`,经济 F-9 口径);③ 累加得 `preaggregated_nlv = Σ house_count × floor(BuildingCost/2) + Σ MortgageValue(未抵押地)`;④ 以 `(player, amount_due, preaggregated_nlv)` 调经济 `is_insolvent`。
- **接口签名约定:** 经济 `is_insolvent(player, amount_due, preaggregated_nlv)` 接受**外部预聚合 NLV**,经济**不**反向调 8 枚举建筑(消 economy→8 环);预聚合 NLV 由本系统计算传入。
- **与 CR-3.2 的区分:** CR-3.2 聚合对象 = 单格(`BuildOwnershipSnapshot` per-tile + `GetHouseCount(tile)`)供收租;CR-3.4 聚合对象 = 全组合(`GetPlayerBuildings(player)` 全枚举)供 NLV 计算。两条路径**并存不融合**,由不同业务触发(落地收租 vs 破产判定)。

**CR-3.5 建房通告 beat(OQ-Build-6 RESOLVED):** `ResolvePhase` 监听建房(8) 广播 `OnBuildingChanged(tile, newCount)`(**owner 建房8 为 2 字段权威契约,本系统只读 owner 实际产出的字段**),当收到此事件时(建/拆任一方向),本系统在当前回合 `ResolvePhase` 内广播**全玩家可见建房通告 beat**——至少携带:触发格 tile 名称、新 `house_count`、建筑归属玩家 id。**建筑归属玩家 id 由本系统从当前回合上下文取**(= 当前回合持有者 `CurrentPlayerId`,本系统作为回合 owner 本就权威持有;建房只在持有者自己回合的 `ResolvePhase`/`PostRollAction` 发生〔见触发时机〕,故归属玩家**恒等于**当前回合持有者——**不从 `OnBuildingChanged` 事件读 actingPlayerId 第3字段**,2026-06-06 用户裁定方案②,reconcile building8↔player-turn2 OnBuildingChanged 字段漂移,building 全档 2 字段不变)。通告内容是**信息层事件**,不修改任何游戏状态。

- **触发时机:** `OnBuildingChanged` 在 `ResolvePhase`(含强制清算子相)或 `PostRollAction`(自愿建/拆)期间收到时,本系统 emit 通告;`TurnStart`/`TurnEnd` 不 emit(建房只在这两个阶段发生)。
- **呈现职责归 HUD(16):** 通告 beat 的视觉/音效表现(如浮动文字、建房动画提示、全局 ticker)由 HUD(16) GDD 设计——本系统只 emit 事件并保证 payload 字段存在。**HUD(16) GDD 开工前须在 HUD 继承义务表登记此通告 beat 消费 AC**(producer propagation 债,尚无 HUD GDD 故暂登记于 systems-index 继承义务表)。
- **设计背景(creative-director 裁定):** 建房通告是"廉价社交点火"手段——玩家立即知道对手刚在哪条街建了房、威胁升级,驱动"要绕路了"的实时心理博弈(支柱②社交互坑,信息层)。不涉及供给/租金 cap 变更(建房(8) OQ-Build-6 user 裁定排除 rent cap)。

**CR-8 AI 决策契约(同步模型,R1 新增 —— 取代原 wall-clock 超时):** AI 玩家的所有决策(买/不买、建房、抵押、拍卖出价、出狱方式)是**同步确定性调用**,非异步等待。本系统作为流程 owner,在每个决策点**同步**调用 AI 接口并立即取回结果,然后推进:

- **接口形态(本系统声明其存在;内部决策逻辑归 AI(10)):**
  - `ResolvePhase` 买地:`bool DecideBuyProperty(GameStateSnapshot, PropertyData)`
  - `ResolvePhase` 拍卖(Alpha):`int32 DecideAuctionBid(GameStateSnapshot, PropertyData)`
  - `PostRollAction`:`TArray<FTurnAction> DecidePostRollActions(GameStateSnapshot)` → 一组有序动作,执行完即等价于 `EndTurn`;**返回空数组 `[]` = AI 本回合不做任何 PostRollAction,框架直接 `EndTurn`(R3 显式声明,补 AC-37d)**
  - `JailTurn`:`EJailAction DecideJailAction(GameStateSnapshot)`
  - > **⚠ 四 hook 失败语义整簇契约(R3 新增 —— ai-programmer BLK-1/2/3 + CD「四 hook 三层一致」mandate):** CR-8 散文/接口形态/AC 三层必须对 4 个 hook 的"返回了但执行期不可行"分支**一致 codify**(R2 只锁了 hook #3 PostRollAction 一面,R3 补齐另三面)。统一模式 = **执行期可行性校验归对应规则系统、不可行降级 + dev 日志、不崩不中断**:
  >   - **#1 `DecideBuyProperty` 返回 true 但现金不足/地产已被买**:扣款可行性由经济(5)/所有权(6) **执行时**校验;不可行则**视同 `false`(不买)** + `UE_LOG` dev 诊断,不扣成负现金、回合不崩。(补 AC-38b。)
  >   - **#2 `DecideAuctionBid` 值域(Alpha,R3 现在钉死 sentinel 避免半发布返工)**:**负数 / `INDEX_NONE` = 放弃出价**;**`0` 非法**(不作"出价0"解);合法出价须 **`> 当前最高价` 且 `<= Cash` 且 `>= 最低加价步长`**;**任何违反值域 = 视同放弃** + dev 诊断。出价合法性**校验归拍卖(12)**,本系统/CR-8 只钉接口值域契约(类比 L102 `DecideTradeResponse` 的开口处理,但因 int32 形态已被 GameStateSnapshot 契约+OQ-1 C-1 消费,故现在钉死而非仅标开口)。MVP 不触发。
  >   - **#3 `DecidePostRollActions` 过期/不可行动作**:见下方「批处理失败策略」(已闭合,逐动作重校+静默跳过+不中断)。
  >   - **#4 `DecideJailAction` 非法返回**:`PayBail` 但 `Cash < 保释金` / `UseCard` 但无出狱卡 → 可行性**校验归事件格(7)**;不可行则**降级为"留狱"(`JailTurnsServed+1`)** + dev 诊断,不扣成负现金、不凭空消卡。`RollDouble` 后续链见 Edge Cases(出狱双点不进双点链,已闭合)。(补 AC-39b。)
  - > **GameStateSnapshot 开口(R2 新增,ai-programmer+unreal 独立收敛;R3 补 per-hook 字段充分性判据):** 本 GDD **不**在此穷举 `GameStateSnapshot` 字段——其**UE 类型(推荐 `USTRUCT(BlueprintType)` 值语义只读)与字段构成由 OQ-1 ADR 定义**(列为 ADR 决策因子 C-1)。契约要求:snapshot 须携带 AI 各决策**实际所需**的只读视图,跨系统聚合——含本系统 `PlayerState` 全字段、所有权(6) 的色组/各玩家持有地产、建房(8) 的建房费用与均匀建造约束、拍卖(12) 的当前出价轮次/**当前最高价/最低加价步长**(Alpha)、**事件格(7) 的出狱卡持有状态(R3 补:`DecideJailAction` 判 `UseCard` 可行性必需,原 R2 注入清单漏了事件格7)**。这些系统**有义务向 snapshot 注入只读数据**;具体聚合形态归 ADR。**AI(10) GDD 开工硬前提。**
  >   - > **snapshot 生成时机(R5 → R6 RETREAT 简化):** AI 各 `DecideXxx` hook 的 snapshot 在该玩家相应决策阶段(`ResolvePhase`/`PostRollAction`/`JailTurn`)生成,反映该玩家当前阶段状态。R6 retreat 删除了 `AIPlaybackGating` 相位,故不再存在"snapshot 误携 gating 相位值"的风险(原 R5 ai ISSUE-4/6 随相位删除而消解);AI(10) 无须为任何呈现门控相位写决策分支。
  >   - > **per-hook 字段充分性判据(R3,ai-programmer REC-1 —— 防"开口踢给 ADR 却无判据"):** OQ-1 C-1 落地时**须对照 CR-8 四 hook 各列一份所需字段清单**并确认每个来源系统都在注入方清单内,例:`DecideBuyProperty` 需现金/地产价格/地产色组/**AI 已持该色组数量(派生量)**;`DecideJailAction` 需保释金额/**是否持出狱卡(归7)**/`JailTurnsServed`;`DecideAuctionBid` 需当前最高价/最低加价/自身现金。字段充分性由 OQ-T-3 ③ 验证(以本判据为对照清单)。
  - > **被动交易响应 hook(R2 标注,Alpha 缺口):** 当 AI 是被交易**发起方**(他人对 AI 发起提案),AI 的 Accept/Reject/Counter 响应**不在**上述 4 个主动 hook 内。此 `DecideTradeResponse` 接口由交易(11) GDD(Alpha) 定义并回链 CR-8;MVP 不触发,此处仅标明契约开口避免 Alpha 返工。
- **退出协议**:AI 回合的 `PostRollAction` 退出 = AI 同步返回动作列表并由框架执行完毕后,框架自行 `EndTurn`(方向:**框架调 AI、框架 EndTurn**,而非 TurnManager 被动等 AI 回调)。**注(R6 RETREAT —— 取代 R3-R5 框架 gating):** 框架在 AI 同步决策执行完毕后**即时 `EndTurn` 并移交下一玩家 `TurnStart`,不等待呈现层回放**(`Animated`/`Instant` 均不阻塞框架流程,见下方 AI 可观察性 hook)。人类玩家退出 = UI 输入层回调 `EndTurn`。
- **批处理失败策略(R2 新增,堵接口语义缺口——ai-programmer BLOCKING):** `DecidePostRollActions` 是基于**决策时刻 T0 快照**的一次性批处理,但动作依序执行会改变状态(先抵押改现金、先建房改均匀建造约束),后续动作可能在执行时**已不可行**。框架行为**裁定(逐动作重校+静默跳过)**:框架执行 `TArray<FTurnAction>` 中**每条动作前,委派对应规则系统(经济5/建房8/所有权6)重新校验该动作在当前实时状态下的可行性**;不可行则**静默跳过该条 + `UE_LOG` dev 诊断,继续执行列表后续动作**,**不中断整个回合**;全列表处理完(执行 + 跳过)后框架 `EndTurn`。理由:中断整批会丢失后续合法动作(AI 意图损失大);逐步重决策模型(AI 每步返回单动作)更干净但需重写接口、MVP 不值当。可行性**重校验逻辑归各规则系统**(本系统只发起委派),本系统只定义"跳过不中断"的框架流程契约。补 AC-42 覆盖跳过场景。**⚠ 被跳过动作不广播 `OnAIActionExecuted`(R5 起;R6 RETREAT 后仅关乎呈现一致性):** 静默跳过的不可行动作**没有实际执行、无可观察行动**,故**不广播** `OnAIActionExecuted`——呈现层只收到实际执行动作的广播(`ActionIndex` 按实际执行顺序自增),不会为被跳过动作播放空动画。(R5 曾以此支撑框架 gating 的 N≡M 收敛判据;R6 retreat 删除框架 gating 后,此约束纯为呈现一致性。)**单动作跨系统可行性聚合主责(R3,ai-programmer REC-4):** 一条 `FTurnAction` 的可行性可能跨多个系统(如"建房"= 现金够[经济5] + 均匀建造约束[建房8] + 仍持该地产[所有权6])——契约规定:**一条动作的可行性由其 `ActionType` 的主责系统裁定,主责系统负责聚合该动作所需的跨系统前提**(框架不逐系统串问)。避免实现期卡在"建房动作的现金校验由谁发起"。
- **AI 回合可观察性 hook(R2 NR-1 → R3-R5 曾升框架 gating → R6 RETREAT 降回呈现层软约定——兑现支柱②观察对手):** AI 同步决策=单帧完成,呈现层默认"一闪而过",玩家看不清 AI 买了哪块地/建了几栋房 → 削弱支柱②社交博弈(本作 priority-1 Fellowship)。框架在执行 `TArray<FTurnAction>` **每条实际执行动作(含买地/建房/抵押)时广播 `OnAIActionExecuted(FAIActionDetails)` 事件**(payload 含 `ActionIndex`——**框架拥有的轻量排序契约**:框架保证按实际执行顺序自增 `0..M-1`(M=实际执行动作数),供呈现层排序展示;R6 RETREAT 删除的仅是"框架消费它做 gating 收敛"这一职责,**自增值域本身仍是框架的可测契约,非可选提示**,见 AC-44①),供呈现层(HUD16/VFX19)以可观察节奏逐步播放 AI 行动。
  - > **⚠ R6 RETREAT 裁定(用户裁定——取代 R3-R5 框架 gating):** R3-R5 曾把"AI 回合有可观察节奏"从呈现层善意升为**框架硬保证**(框架进下一 TurnStart 前等呈现层回放完成,经 `PendingPlaybackSet`/`CompletedPlaybackSet` 集合收敛)。该 gating 接缝连续四轮(R3/R4/R5/R6)剥新面;R6 五专家从五正交方向同时剥面、全部 root 到 R5 引入的计数收敛介质(`ActionIndex` 语义/集合判据/Timer 互斥),CD 判定为**结构性不收敛信号**(把异步呈现时序绑定到确定性框架流程,本质上是一条无穷延伸的接缝)。**裁定:降回呈现层软约定**——框架在 AI 同步决策执行完毕后**即时 `EndTurn`、即时移交下一玩家 `TurnStart`,不阻塞于呈现层回放**。`OnAIActionExecuted(ActionIndex)` 广播保留,呈现层(HUD16/VFX19)**自主**决定如何非阻塞地展示 AI 行动。支柱②"AI 可观察"由**框架强保证降为 HUD 自主呈现职责**;代价是极端情况下 AI 动画可能被下一回合呈现覆盖,由 HUD 设计缓解(非框架契约)。
  - > **呈现层展示策略(归 HUD16/VFX19,本系统不约束):** 推荐 HUD 利用人类玩家下一回合 `TurnStart` 后阅读"上一 AI 回合行动摘要"的自然停顿展示 AI 行动,兑现支柱②的**信息层**(玩家知道 AI 做了什么、影响自己策略),而非依赖**强制观看过程**(对 1 人类 + 多 AI 局,过程观看的 Fellowship 价值本就微弱,见 OQ-9 已解决)。`AIActionDisplayMode` 旋钮(见 Tuning)控制 HUD 呈现风格(`Animated`=逐步动画 / `Instant`=跳过),**两者均不阻塞框架**。
  - **`AIActionDisplayMode` 与 OQ-3 确定性重放:** 框架推进不再依赖呈现层时序,故 headless/联网确定性重放天然纯净(呈现节奏从不进流程关键路径),无需为 `Instant` 设计特殊逃生舱语义——`Instant` 仅是"HUD 不播 AI 动画"的呈现选项。
  - 本系统保证 `OnAIActionExecuted(ActionIndex)` 事件 hook 存在,具体呈现策略与动画时长归 HUD(16)/VFX(19)。登记于 Dependencies HUD(16) 行 + Tuning Knobs。补 AC-44 覆盖"Animated/Instant 均即时 EndTurn + 逐条广播 ActionIndex"。
- **dev 兜底(非 gameplay 计时,R2 重定义——删除虚拟时钟看门狗):** R1 的"虚拟时钟看门狗"在**纯同步单帧模型下技术不自洽**:同步决策若真挂死(无限循环)= GameThread CPU 忙循环,单线程下虚拟时钟/同帧软件计时**永不推进**,无法在帧内捕获或中断。R2 据此拆为两条诚实的 dev 兜底,**取代帧内虚拟时钟看门狗**:
  - **"永不返回"(死循环 bug)**:不试图在帧内捕获(单线程同步下不可能)。靠**单元测试框架自身的进程级超时**(test runner timeout)在 CI/dev 暴露——挂死的 mock AI 会让该测试用例超时失败,即为 bug 信号。**非帧内机制、不进生产构建。**
  - **"返回但超单帧预算"(慢调用,非挂死)**:`DecidePostRollActions` 等同步调用**返回之后**,框架做一次**事后帧预算检查**(记录调用耗时,超 dev 阈值则 `UE_LOG` 警告),纯诊断、不强制 `EndTurn`(调用已正常返回,无需兜底)。仅 dev 构建启用。
  - 原 `AITurnTimeout`(wall-clock gameplay 旋钮,R1 已废)→ R2 看门狗常量 `ai_turn_watchdog` 重定义为此"事后帧预算诊断阈值"(详见 Tuning Knobs)。**正常 AI 同步决策即时返回,此机制不参与正常流程。**
- **确定性**:同步模型使 AI 回合可在 headless fixture 中确定性重放(消除 wall-clock 非确定性);AI(10) 内部任何随机性须走骰子(3)的可种子化 RNG,禁止自调引擎随机(为 OQ-3 Full Vision 重放预留)。**强度坦诚标注(R3,ai-programmer REC-2 —— 与 AC-35/AC-43 对齐):** 此约束在 BP-only 下为**软约束(code-review 守,见 OQ-T-3②)**——BP 项目无法引擎层阻止 AI 蓝图节点直接调 `RandomFloat`/`FMath::Rand`;若 OQ-1 ADR 选 C++ 强封装,可对 AI 模块加**静态扫描禁用引擎 RNG 符号**作硬门(同 AC-35a 性质)。MVP AI 简单、重放是 OQ-3 才真需要,故 MVP 软约束足够,但强度须坦诚标注而非留隐含承诺。
- **双点额外回合链**:`OnTurnStarted` 在回合链**每次**额外回合**不**重发(整链一次);`OnPhaseChanged(RollPhase)` 在每次额外回合重发。AI 可读 `ConsecutiveDoubles`(经 GameStateSnapshot 只读暴露)判断是否仍在链中。AI 在额外回合的每次 ResolvePhase 落地仍获买地决策点(消除"额外回合落地无法买"的断链)。

**CR-4 双点额外回合(经典规则):** 若本回合 `bIsDouble==true` 且玩家**未因此回合入狱**且未破产 → `TurnEnd` 后**回到同一玩家的 `RollPhase`** 再走一个完整回合。本系统按"回合链"累计连续双点数 `ConsecutiveDoubles`;**第 3 次连续双点 → 立即送监狱**(跳过本次移动/结算),回合链终止、不发放额外回合。`ConsecutiveDoubles` 在行动权移交下一玩家时归零。送监狱复用 GoToJail 路径(位置/状态改写归移动(4)/事件格(7),本系统只发起意图)。

> **AI 双点额外回合链与可观察性(R5 → R6 RETREAT 简化):** AI 双点额外回合链(`TurnEnd → 回 RollPhase`,同玩家继续)期间,每个额外回合内的 `OnAIActionExecuted(ActionIndex)` 仍逐条广播;框架不阻塞于呈现(R6 retreat 后无 gating),AI 动画的可观察节奏(含额外回合之间)全归 HUD(16)/VFX(19) 的时间轴设计。R5 曾因框架 gating 而需定义 `ActionIndex` 跨双点链作用域(R6 ai 4-A 剥出此接缝)——retreat 后框架不再消费 `ActionIndex` 做收敛判据,**但其"按实际执行顺序 `0..M-1` 自增"仍是框架拥有的轻量排序契约(AC-44① 可测),只是不再驱动任何 gating 收敛**;跨回合(双点链)作用域不再是框架问题——框架对每个回合内的执行序列独立从 0 自增,链间排序如何呈现归 HUD 设计。

**CR-5 回合推进与跳过:** `TurnEnd`(无额外回合)后,行动权按 `TurnOrderIndex` 顺时针传给**下一个未破产**玩家(破产者永久跳过)。**在狱玩家不跳过**——仍获得回合,但 `TurnStart` 将其路由到 Jail 分支(尝试交保释/掷双点出狱,规则归事件格7)。

**CR-6 破产移出与胜负信号(R-2to9 propagate:对齐破产9 return-only 架构):** 玩家破产经破产胜负(9) `ResolveBankruptcy(debtor, creditor, activePlayersSnapshot)` 裁决——**9 为 return-only 编排、绝不回调本系统**,执行完返回 `{debtorEliminated, winnerId|INDEX_NONE, rankingEntry}`。本系统据 `debtorEliminated=true` 置 `bIsBankrupt=true`(本系统拥有的 PlayerState 字段)→ 该玩家永久移出轮转;并据返回的 `winnerId` 触发胜负信号 **`OnGameWon(winnerId)`**(`winnerId==INDEX_NONE` 表对局未结束 / 退化局无单一胜者 → **不触发**,见 F-2)。**`OnGameWon` 为边沿触发(R-2to9 RR followup,钉死 AC-12 测的边沿语义):本系统在单次 `ResolveBankruptcy` 返回 `winnerId!=INDEX_NONE` 后**恰广播一次**;对局进入 `Winner` 终态后,**不**因后续任何 F-2 计数 / 一致性自检 / 状态读取 / 定时回调再次广播 `OnGameWon`(电平→边沿,AC-12 测此幂等)。** **胜负判定归破产9**(在 `activePlayersSnapshot` 上算 APC、显式排除 debtor、不依赖本系统 flag 写时序,返回 winnerId,见 bankruptcy CR-6/F-2);本系统**不**独立裁决胜负、**不**向9 反报信号——旧 `OnLastPlayerStanding` 的 2→9 反向耦合已删除。退化局(APC==0)的检测 / fallback / 胜者取值全部归9 内部裁决(bankruptcy AC-29),本系统只据9 返回的 `winnerId` 行事。

**CR-7 玩家规模:** MVP 2–4。所有顺序/数组结构按运行时玩家数 `P` 动态尺寸(不硬编码 4),接口预留至 8,为联网(25)留空间但不展开实现。

### States and Transitions

**(a) 玩家生命周期状态:**

| 状态 | 含义 | 转换 |
|---|---|---|
| `Active` | 正常参与,轮到时正常回合 | → `Jailed`(被送监狱);→ `Bankrupt`(破产9判定) |
| `Jailed` | 仍在局中、仍获回合,但走 Jail 分支 | → `Active`(出狱,规则归7);→ `Bankrupt` |
| `Bankrupt` | 终态,永久移出轮转 | (不可逆) |
| `Winner` | 最后存活者(破产9 经 `ResolveBankruptcy` 返回 `winnerId`,本系统据此触发 `OnGameWon` 标记) | 终局 |

> 注:`Jailed` 是 `Active` 的子状态(在狱玩家仍在局中、仍获回合),非平行第四态;为表达回合路由差异单列。

**(b) 回合阶段状态机(每个活跃回合):**

`TurnStart →(在狱? → JailTurn 分支 → TurnEnd)/(正常 → RollPhase → MovePhase → ResolvePhase → PostRollAction → TurnEnd)`;`TurnEnd →(双点且未入狱? → 回 RollPhase)/(否则 → 下一玩家 TurnStart)`。(R6 RETREAT:删除 Animated gating 分支——AI 回合 TurnEnd 与人类一致,即时移交下一玩家 TurnStart,不阻塞于呈现回放。)

| 阶段状态 | 进入触发 | 退出条件 |
|---|---|---|
| `TurnStart` | 行动权移交至本玩家 | 路由完成(正常→RollPhase / 在狱→JailTurn / 破产→跳过) |
| `RollPhase` | TurnStart 正常路由 | 收到完整 `FDiceRollResult`(Die1/Die2/Total/bIsDouble) |
| `MovePhase` | RollPhase 完成 | 移动(4)回报落点 |
| `ResolvePhase` | MovePhase 完成 | 落地结算完成(含买地决策点:人类 via UI / AI via 同步 hook) |
| `PostRollAction` | ResolvePhase 完成 | 人类:UI 发"结束回合";AI:同步返回动作列表(空数组=直接 EndTurn)并执行毕→框架即时 EndTurn(**R6 RETREAT:无论 Animated/Instant 均不阻塞于呈现回放,即时移交**)。(R3:R2 已废的"dev 看门狗强制 EndTurn"残留删除——同步决策正常即时返回,无需帧内兜底) |
| `TurnEnd` | PostRollAction 完成 | 判定额外回合 or 移交;直接下一玩家 TurnStart(R6 RETREAT:AI 回合与人类一致,无中间呈现门控相位——历史细节见 changelog) |
| `JailTurn` | TurnStart 在狱路由 | 出狱决策点(交保释/掷双点/出狱卡,人类 via UI / AI via 同步 hook,规则归7)→ TurnEnd |

> **非法转移拒绝(R1 明确,AC-5b 守门)**:状态机仅接受表内合法转移。从任意阶段发送**非当前阶段预期**的信号(如 RollPhase 阶段收到 `EndTurn`)须被拒绝(保持当前阶段不变或抛 assertable error),不得跳转。这是"状态机结构不可变"约束的可测面。

**关键:本系统运行时状态机随对局推进,但其结构(阶段序列)不可变;玩家状态实体可变但只经受控写接口。**

### Interactions with Other Systems

| 系统 | 数据流 | 谁拥有 |
|---|---|---|
| 骰子(3) | 本系统 `RollPhase` 请求掷骰 → 接收完整 `FDiceRollResult`(Die1/Die2/Total/bIsDouble) | 骰子拥有随机;本系统消费 `Total`+`bIsDouble` 做逻辑、**序列化全字段含 Die1/Die2(dice B1/OQ-T-Dice-5)** |
| 移动(4) | `MovePhase` 委派移动;移动改 `CurrentTileIndex`(经本系统 `SetPosition` 受控写) | 移动拥有位置改写逻辑 |
| 经济(5) | 经济读/改 `Cash`(经受控写);发薪经移动(4)传入;**本系统在 `ResolvePhase` 暴露「当前程掷骰上下文」`Total` 供经济 Utility 租 PULL(holder=回合2,见 CR-3.1 / 移动 CR-3.1 / 经济 F-4)** | 经济拥有现金公式;本系统持当前程 `Total` 单源 |
| 事件格(7) | `ResolvePhase` 委派;进/出狱规则、`bIsInJail` 改写归7;本系统持字段+服刑计数+跳过逻辑 | 7 拥有牌堆/监狱规则 |
| 破产胜负(9) | 本系统 `ResolvePhase` 在 `is_insolvent` 时调 `9.ResolveBankruptcy(debtor,creditor,snapshot)`→`{debtorEliminated,winnerId\|NONE,rankingEntry}`(**9 return-only,不回调本系统**);本系统据返回值置 `bIsBankrupt` + 移出轮转 + 触发 `OnGameWon(winnerId)` | 9 拥破产判定+胜负裁决+APC 计算;本系统拥 `bIsBankrupt` 字段写 + 流程/轮转 |
| AI(10) | AI 玩家回合本系统发 `OnTurnStarted(bIsAI=true)`,AI 接管 `PostRollAction` 决策,完成回调结束回合 | AI 拥有决策 |
| HUD(16) | 读 `PlayerState` 全字段 + 当前回合玩家 + 当前阶段 | HUD 拥有呈现 |
| 主菜单大厅(20) | 大厅经具名入口 `StartNewGame(const FGameSetupConfig&)` 传入玩家数/名/色/AI 档 → 本系统初始化 `PlayerState` 列表并定序(入口签名 + `FGameSetupConfig`/`FPlayerSetupEntry` 两 USTRUCT 归本系统,见下「开局入口契约」;OQ-Lobby-1 闭合 / propagate 2026-06-06) | 大厅配置,本系统建实体 |
| 存档(21) | 序列化全部 `PlayerState` + 定序 + 当前回合玩家 + 当前阶段 + `ConsecutiveDoubles` | 存档拥有序列化 |

**接口稳定承诺**:`PlayerState` 字段一旦发布只增不改语义;回合阶段枚举 `ETurnPhase` 通过扩展新增、不破坏既有转换。这是给 8 个尚未成形下游的稳定保证。

> **开局入口契约(OQ-Lobby-1 闭合,producer /propagate-design-change 落定 2026-06-06):** 大厅(20)与本系统的开局握手由本系统具名入口承接,owner=本系统(与其他 `PlayerState` 构造逻辑同处):
> - **入口签名:** `void StartNewGame(const FGameSetupConfig& Setup)` —— 大厅装配 `FGameSetupConfig` 后单次调用本系统;本系统据此初始化 `PlayerState` 列表(分配 `PlayerId` / `TokenColor` 唯一)、定序、置初始阶段。返回 void;开局拒绝路径(P<2 / P>4 / 越界)按 **AC-27** 防守(可返回错误码或拒绝,实现层定,见 L514)。
> - **`FGameSetupConfig`**(`USTRUCT(BlueprintType)`,owner=回合2):整局开局配置载体。
>   - `TArray<FPlayerSetupEntry> Players;` —— 玩家配置条目,尺寸即 `P`(动态,无硬编码 4,见 AC-13)。**`TArray` 字段须包进 USTRUCT**(`FPlayerSetupEntry` 已是 USTRUCT,合规;与 L257 dynamic delegate USTRUCT 约束同源——裸 `TArray` 不可作 BP 边界传参,此处经字段包裹满足)。
>   - 旋钮覆盖(可选,与设置&House Rules(23) 对齐):`int32 DoublesJailThreshold;` / `int32 MaxTiebreakRounds;`(开局锁定,局中不可改,见 L421;缺省取本系统默认)。
> - **`FPlayerSetupEntry`**(`USTRUCT(BlueprintType)`,owner=回合2):单玩家开局配置。
>   - `FText DisplayName;`(玩家名,**字段名/类型与 L79 `PlayerState.DisplayName:FText` 一致**) / `EPlayerColor TokenColor;`(唯一分配) / `bool bIsAI;` / `EAIDifficulty AIDifficulty;`(非 AI 为 None)。
> - **双向回链:** 大厅(20)GDD 须在其「开局/Interactions」节标明:经 `StartNewGame(const FGameSetupConfig&)` 调用本系统、`FGameSetupConfig`/`FPlayerSetupEntry` 归回合2(owner),大厅仅装配并传入(不定义 struct)。AC-13 接口名据此冻结。

> **事件广播形态(R1 新增,推 OQ-1 ADR 钉死默认):** 8 个下游需挂接同一回合事件 = 1-对-多广播。事件(`OnTurnStarted`/`OnPhaseChanged`/`OnTurnEnded`/`OnGameWon`/`OnTurnOrderResolved`/`OnAIActionExecuted`(R2))应为 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` + `UPROPERTY(BlueprintAssignable)`——UE 中 1-对-多、BP 与 C++ 均可绑定的标准模式。**不可**用 `BlueprintImplementableEvent`(1-对-1,多消费方无法同时挂接)。OQ-1 ADR 把此作为推荐默认。
> ⚠ **delegate 参数类型约束(R2 新增,unreal;R3 补具体 payload 清单):** dynamic multicast delegate 的参数 struct **必须标 `USTRUCT(BlueprintType)`**(否则 BP `BlueprintAssignable` 绑定时编译报错);`TArray`/`TMap` **不能直接作** dynamic delegate 参数(须包进 USTRUCT)。具体 payload 清单(列为 OQ-1 ADR 决策因子 C-5/⑪,ADR 须显式列出):
> - `OnTurnOrderResolved` 携带 TurnOrder 数组 → **必须包进 `FTurnOrderResult { TArray<int32> OrderedPlayerIds; bool bResolvedBySeatTiebreak; }`**(裸 `TArray<int32>` 作 dynamic delegate 参数编译失败 —— R3 unreal 点名)。
> - `OnGameWon(int32 WinnerId)` → 胜负信号(**R-2to9 propagate:取代旧 `OnLastPlayerStanding` / `FLastPlayerStandingResult` 的 2→9 反向信号**)。`WinnerId` 取自破产9 `ResolveBankruptcy` 返回的 `winnerId`(9 在 `activePlayersSnapshot` 上算 APC 裁定、退化局 APC==0 时9 内部 fallback,见 bankruptcy AC-29/F-2);本系统据此触发广播供下游(VFX19/HUD16/音频22)呈现封王 juice。`winnerId==INDEX_NONE`(对局未结束 / 退化局无单一胜者)→ **不触发**。单 `int32` 参数可直接作 dynamic multicast delegate 参数(无须包 USTRUCT)。事件归破产9 供(enable-not-own,见 bankruptcy L75/L189)、本系统据9 返回值触发、下游拥呈现。
> - **三核心事件 payload(R4,unreal——R3 漏列致 8 下游无法判断是否需额外查询):**
>   - `OnTurnStarted` → `FTurnStartedInfo { int32 PlayerId; bool bIsAI; }`(携当前玩家 id + 是否 AI,免下游回查 TurnManager 当前玩家;`bIsAI` 供 AI10/HUD16 路由)。
>   - `OnPhaseChanged` → `FPhaseChangedInfo { ETurnPhase OldPhase; ETurnPhase NewPhase; int32 ConsecutiveDoubles; }`(**携新旧阶段**供 HUD 做过渡动画;`ConsecutiveDoubles` 让 HUD 区分"首次 RollPhase"与"双点额外回合 RollPhase",见 L129 重发语义)。
>   - `OnTurnEnded` → `FTurnEndedInfo { int32 PlayerId; bool bGrantsExtraTurn; }`(`bGrantsExtraTurn`=双点额外回合标志,供 HUD 区分"同玩家继续"与"移交新玩家"的切换动画)。
> - `OnAIActionExecuted` → USTRUCT(`FAIActionDetails`),**含 `ActionIndex`(R5 引入;R6 RETREAT 后定性为**框架拥有的轻量排序契约**——框架不再消费它做 gating 收敛,但仍**保证**按实际执行顺序自增 `0..M-1`(M=实际执行动作数,被跳过动作不广播亦不占号,见 AC-44①②),供 HUD 排序展示。**非"可选提示":自增值域是框架的 [Logic] 可测契约,BP 实现不得填 `-1` 等空值**)** + `ActingPlayerId`/`TargetTileIndex`/`Amount`/**`EActionType`**(枚举:`BuyProperty`/`BuildHouse`/`Mortgage`/`Unmortgage`/`Move`/`PayRent`/`Bankrupt`…)等。**`EActionType` 为 HUD16 R-2 propagate 落定(2026-06-05,OQ-HUD-12;兑现原"其余字段由 HUD16/VFX19 呈现需求 + AI10 定"开口)**:HUD V-Own-07 文案/图标 + F-4 critical 判定(hud AC-56)+ VFX19 juice 选型依赖;owner=本系统(框架广播每条实际执行动作时填其 `ActionType`)。**字段扩展遵 L264 UE 代价:核验绑定 `FAIActionDetails` 的全部下游 BP 图(HUD16/AI10/存档21)引脚连接,防静默断线。**
> - **⚠ payload 字段扩展的 UE 特有代价(R5 unreal #6——扩展到全部 payload struct,非仅 FAIActionDetails):** 上述全部 dynamic multicast delegate 的参数 USTRUCT(`FTurnOrderResult`/`FTurnStartedInfo`/`FPhaseChangedInfo`/`FTurnEndedInfo`/`FAIActionDetails`;**R-2to9 propagate:`FLastPlayerStandingResult` 已随 `OnLastPlayerStanding`→`OnGameWon` 删除——`OnGameWon(int32 WinnerId)` 单 int 参数无 USTRUCT,不在此列**)**任何字段变更**(增/改名/改类型)都波及绑定该 delegate 的全部下游 BP 图(HUD16/AI10/存档21 等)。UE 中此变更**不主动报编译错误**——BP 节点按 pin 名称匹配,字段**改名会致下游 BP 引脚静默断线**(runtime null),仅在打开该 BP 时出现 "out of date node" 警告。**故"字段扩展"的真正代价 = 须重新打开并检查绑定该 delegate 的全部 BP 图的 payload 引脚连接,防静默断线,非"编译一次"。** 这是接口稳定承诺(L184「只增不改语义」)在 UE 实现层的强约束依据。
> - **`OnAIActionPlaybackSegmentComplete(ActionIndex)`(R3-R5 曾存在;R6 RETREAT 删除)**:此回调是 R3-R5 框架 gating 的核心介质(呈现层→框架的逐段回放完成回调,驱动集合收敛)。R6 retreat 后框架不再等待呈现层回放,**此回调彻底删除**——呈现层无须向框架报告回放进度,框架在 AI 决策执行完即时移交。
>
> **枚举扩展 append-only(R1 新增):** `ETurnPhase`/`EPlayerColor`/`EAIDifficulty` 用 `UENUM(BlueprintType) enum class : uint8`,扩展**只增不删不改顺序**(末尾追加)。理由:已序列化的 `.uasset`/存档存的是枚举整数索引,重排会使旧数据错位。"枚举扩展不破坏既有转换"的承诺靠此规则在实现层兑现。**例外澄清(R7,unreal 发现3):** append-only 仅约束**已被分配过持久化索引的值**(出现在已提交 `.uasset` 或 SaveGame 数据中)。GDD 审稿阶段引入、随后在任何引擎资产正式使用前即删除的草稿值(如 R3-R6 曾拟的 `AIPlaybackGating` 相位)**从未被分配索引**,删除它不违反 append-only,`ETurnPhase` 当前合法值仍为 7 个(TurnStart/RollPhase/MovePhase/ResolvePhase/PostRollAction/TurnEnd/JailTurn)。实现确认标准:该值从未出现在已提交 `.uasset` 或存档数据中。

> **双向一致性待同步(systems-index):** 系统索引当前系统#2 的 depends-on 列为"—"(零依赖,正确)。但被依赖方向:索引仅在 4/5/7/9/14/16/20/21 标了依赖。本 GDD 接口分析确认这些系统均直接读/写 `PlayerState` 或挂接回合事件,依赖关系一致(无需新增,Phase 5 复核)。

## Formulas

> *由 `systems-designer` 提议(lean 模式 D 节派发),主会话独立复算 F-1 防死循环与 F-4 裁定确定性。*

本系统**不拥有**掷骰随机(归骰子3)、现金/位置公式(归经济5/移动4);只规格化回合流程自身的拓扑/计数。

### F-1 NextActivePlayer — 下一个未破产玩家的 TurnOrderIndex

**守卫先行(R1 修正,先于枚举):** 调用前先查 `|A|`:
- `|A| = 0` → 返回 `INDEX_NONE`(-1) + assert(正常流程不可达,见 F-2);
- `|A| = 1` → 返回 `INDEX_NONE`(-1)。**语义:仅剩 1 名未破产玩家时对局已由 CR-6 终结,不存在"下一玩家"**。此情形调用方(TurnEnd 推进逻辑)**本不应**调用 F-1(CR-6 胜负信号应已在 ResolvePhase/破产移出时广播);此返回值仅为防御性兜底,**不**返回那个唯一存活者的 index。
  > **守卫判据=`|A|`(存活者数),与 cur 是否∈A 无关(R2 澄清——systems-designer 残留#1):** 守卫只看 `|A|`,不看 cur 成员性。即便 `cur∉A`(cur 已破产、异常入参),`|A|=1` 仍**正确**表示"全局仅剩 1 名存活者=对局已终结",返回 INDEX_NONE 语义成立(对局确实结束,非误判)。换言之 INDEX_NONE 的理由是"存活者仅 1"这一全局事实,不依赖"唯一存活者==cur"。`cur∉A 且 |A|≥2` 的防御寻路见下方枚举分支(L188)。
- `|A| ≥ 2` → 进入下方枚举。

**入口规范化先行(R5 BLOCK——三层落地,主公式权威化):** 枚举入口先把 `cur` 规范化为 `cur_safe = ((cur % P) + P) % P`(欧几里得取模,保证 `cur_safe ∈ [0, P-1]`,消除损坏入参负 candidate 来源),再进枚举:

`next = candidate_k`,其中 `cur_safe = ((cur % P) + P) % P`、`candidate_k = mod(cur_safe + k, P)`,`k = min{ k ∈ [1, P-1] | candidate_k ∈ A }`(A = 未破产玩家 TurnOrderIndex 集合)。

> **⚠ R5 修正(systems-designer 1-A,主会话+CD 独立核验):** R4 的 cur=-P 规范化修法此前**只落在下方注释与 AC-17c**,主公式表达式与变量表 `cur` Range 从未更新——实现者照旧公式 `mod(cur+k,P)` 写代码会忽略规范化、cur=-P 枚举无命中洞复活。R5 把 `cur_safe` 提进主公式表达式,使散文/公式/变量表/AC 四层一致。

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 当前位(原始) | `cur` | int32 | 任意 int32(正常 0..P-1;损坏入参如读档异常可越界/负) | 当前回合玩家 TurnOrderIndex 原始入参 |
| 当前位(规范化) | `cur_safe` | int32 | 0..P-1 | `((cur%P)+P)%P` 欧几里得取模规范化后,枚举实际使用 |
| 玩家数 | `P` | int32 | 2..8(MVP 2..4) | 玩家总数(含已破产,恒定) |
| 活跃集 | `A` | set\<int32\> | ⊆{0..P-1} | 未破产玩家 TurnOrderIndex 集合 |
| 步长 | `k` | int32 | 1..P-1 | 顺时针枚举步长,从 1 起 |
| 落点 | `next` | int32 | 0..P-1 或 `INDEX_NONE` | 下一应行动玩家 |

**Output Range:** `|A|≥2` 时 `next ∈ A` 且 `next ≠ cur_safe`(k 从 1 起,保证不返回 cur_safe 自身);`|A| ≤ 1` 由守卫先行返回哨兵 `INDEX_NONE`(-1),**不进入轮转**。
**关键:守卫先于枚举,消除矛盾(R1)。** 旧版 Output Range 声称"`|A|≤1` 返回 INDEX_NONE"但枚举逻辑会在"`|A|=1` 且唯一存活者 ≠ cur"(如 P=2, A={1}, cur=0 → k=1 命中 1)时返回有效 index,与声明自相矛盾。R1 把 `|A|≤1` 判定提到枚举**之前**作为前置守卫,枚举仅在 `|A|≥2` 时执行,二者不再冲突。
**mod 安全性真正落点(R1→R5 收敛——单一权威论证,删旧并存论证 systems 1-B):** 安全性的**唯一落点 = 入口规范化** `cur_safe = ((cur % P) + P) % P`(欧几里得取模),它消除损坏入参(`cur<0` 或 `cur≥P`)产生负/越界 candidate 的来源。**不再依赖"负 candidate 被 A 成员性过滤"的旧论证**——该旧论证在 `|A|=P`(全员存活)且 `cur=-P` 时失效:取 P=4, A={0,1,2,3}, cur=-4(未规范化)→ k∈[1,3] candidate=`(-4+k)%4`∈{`-3`,`-2`,`-1`}(C++ 截断向零)全负、全被过滤、枚举遍历完仍无命中(R4 BLOCK,systems+主会话独立复算确认)。入口规范化直接根除此洞,A 成员性过滤仅在 `|A|<P` 时有额外防御副作用,**不可作为 `|A|=P` 场景的依赖**。与 board-data `advance_index` 的关联仅是"环形索引设计模式"引用。
**防死循环:** `|A|≥2` 时 k 遍历 1..P-1 必命中(前提 `cur_safe∈[0,P-1]`,经入口规范化保证),确定终止,无需 visited 标记。
**调用前提(合约):** A 中所有值 ∈ [0,P-1] 且 P 为开局已校验值(TurnOrderIndex 完全排列,见 Edge Cases 校验)。实现入口:① 先 `cur_safe = ((cur % P) + P) % P` 规范化(**shipping 兜底,枚举实际使用 cur_safe**);② 另加 `ensureMsgf(cur >= 0 && cur < P, ...)` 作 **dev 期诊断——检查的是原始 `cur`(规范化前)**,对损坏入参(如读档异常)报警;shipping 下即使 ensure 不触发,规范化已兜底。**二者职责分离:规范化(对 cur_safe)是 shipping 正确性保证,ensure(对原始 cur)是 dev 诊断,不冲突(systems 1-D 澄清)。**
**边界:** 无破产时 k=1 即 `mod(cur_safe+1,P)`;连续多破产者逐步跳过;`cur∉A`(异常,如 cur 已破产)仍正确寻下一个 A 成员(防御性)。
**Example:** P=4, A={0,2,3}, cur=2 → `mod(3,4)=3∈A` → next=3;P=4, A={0,3}, cur=3 → `mod(4,4)=0∈A` → next=0;**P=2, A={1}, cur=0 → 守卫 `|A|=1` → INDEX_NONE(不返回 1,胜者已由 CR-6 裁定)**;P=4, A={0}, cur=0 → 守卫 `|A|=1` → INDEX_NONE。

### F-2 ActivePlayerCount — 未破产玩家数

`APC = |{ p ∈ players | p.bIsBankrupt == false }|`

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 玩家数组 | `players` | array\<PlayerState\> | length P | 全体玩家状态 |
| 破产标志 | `p.bIsBankrupt` | bool | {T,F} | 一旦 T 永不翻转 |
| 活跃数 | `APC` | int32 | 0..P | 未破产玩家数 |

**Output Range:** `[0, P]`,不 clamp。`APC==P` 为开局态。
**⚠ 胜负判定归属(R-2to9 propagate——口径单源):** 本系统 F-2 是据 `bIsBankrupt` 字段的**计数辅助**(供轮转 / 一致性自检 / 诊断),**非独立胜负裁决器**。胜负条件由破产胜负(9) `ResolveBankruptcy` 在 `activePlayersSnapshot` 上算 APC(**显式排除 debtor、不依赖 flag 写时序**,见 bankruptcy F-2/CR-6)并返回 `winnerId`;本系统据返回的 winnerId 触发 `OnGameWon`(见 CR-6)。本系统**不**在破产结算时独立重算"`APC==1`→胜利"——return-only 下9 算 APC 时 debtor 的 `bIsBankrupt` 尚未由本系统写入,本系统若同刻用 `|{!bIsBankrupt}|` 重算会**多计 debtor**、与9 给出相反的"是否终局"判定(此即 2↔9 旧矛盾根因)。故本 F-2 计数仅在本系统**已据返回值写完 `bIsBankrupt` 之后**作为一致性自检 / 轮转辅助使用,胜负口径恒以9 返回的 winnerId 为准。
**`APC==0` 退化局归属(R1 引入 → R-2to9 propagate 改归9):** MVP 主路径破产单点逐个发生,APC 单调减必经 1。**Alpha 交易/拍卖/级联破产**可能使两名玩家在同一结算事务内同时破产,APC 从 2 直接跳 0、跳过 1。**此退化局的检测、fallback 与胜者取值全部归破产胜负(9) 内部**——9 在 `activePlayersSnapshot` 上算 APC,`APC==0` 时 dev `assert`+log + fallback 取除 debtor 外末名 `bIsBankrupt==false` 存活者(无则 fatal log),返回 `winnerId|INDEX_NONE`(见 bankruptcy F-2/AC-29)。本系统**不**广播任何 2→9 反向退化局信号(旧 `OnLastPlayerStanding(bDegenerateTie)` 已随 return-only 删除);只据9 返回的 `winnerId` 行事:`winnerId!=INDEX_NONE` → 触发 `OnGameWon`;`winnerId==INDEX_NONE` → 不触发、对局态由9 裁决。Alpha 事务级"最后两人不得同事务同时破产而不定胜者"的保证亦归破产9(见 bankruptcy F-2 应急契约)。
**快照契约(R1 强化;R2 强度降级对齐 AC-35):** `A`(F-1)与 `APC` 须取自**同一只读快照**。实现要求:`TurnEnd` 阶段进入时取 `players` 一次只读副本(BP 中 local array copy,深拷贝,后续对原数组修改不影响副本——此层 BP 可硬实现),后续所有 F-1/F-2 调用使用该副本。
> **⚠ 强度澄清(R2,与 AC-35 受控写同构——systems-designer BLOCKING):** "破产胜负(9)的 `bIsBankrupt` 写入须在 ResolvePhase 内完成、不得在 TurnEnd 进行中修改"是一项**软约束(架构约定)**,**非引擎级硬保证**——与 AC-35 受控写同构:BP 中任何持 PlayerState 引用的系统可在任意时点写字段,本系统**无法引擎层硬拦截** (9) 的写入时机,强度随 OQ-1 ADR 容器选择(C++强封装可硬门 / BP约定=软约束+code-review)。R1 用"契约要求"硬措辞 over-claim,R2 校正为软约束。
> **必要性前提(R2):** 此契约防的是"A 与 APC 来自不同快照"的竞态。但 **MVP 单线程 BP + OQ-1 禁 Latent Action(同步函数调用栈)下,TurnEnd 内不存在并发修改,竞态理论不可达**——故快照契约在 MVP 实为**防御性过设计(无害但非必需)**。其真正必要性出现在:(a) 若 OQ-1 ADR 最终允许某种异步等待,或 (b) Alpha 交易/拍卖致 TurnEnd 进行中破产写入。**前提**:本系统保证 `TurnEnd` 阶段内**不委派任何可能触发 `bIsBankrupt` 写入的外部调用**(TurnEnd 只做额外回合判定/移交,不再委派经济结算),使"快照取用后无新破产写入"在本系统侧成立;(9) 侧的写入时机约束为软约束,经 code-review 守。`APC==0` 应急路径(下方)为此软约束失效时的最终防御。
> **两份快照时序作用域显式分隔(R3,systems-designer R-4 —— 防下游误读为同一快照):** 本系统涉及**两份语义相反**的快照,二者**顺序分离、非同一份**,下游(#4/5/7/9)实现者勿混淆:
> - **快照A —— `PostRollAction` 的 `DecidePostRollActions` T0 快照(CR-8):故意允许漂移**。逐动作执行会改 Cash/地产/均匀约束,逐动作重校验、不可行静默跳过。漂移是预期行为。
> - **快照B —— `TurnEnd` 进入时取的 `players` 只读副本(F-1/F-2):要求不漂移**。A 与 APC 同源。
> - **时序关系:** `PostRollAction(快照A,允许漂移)→ EndTurn → TurnEnd(快照B,在 A 之后取、禁漂移)`。快照A 内的所有状态写入(含**执行者因建房/抵押在 PostRollAction 内破产**——破产写入发生在 PostRollAction 阶段)在 TurnEnd 取快照B **之前**落定,故 B 是 A 之后的一致视图,链条**无未定义时序缺口**(前提:L223 "TurnEnd 不再委派经济结算"成立)。
**Example:** bIsBankrupt=[F,T,F,F] → APC=3(本系统计数辅助);[F,T,T,T] → 本系统已据9 返回值写完 flag 后 APC=1,胜负仍以9 返回的 `winnerId` 为准触发 `OnGameWon`(口径见上方归属注,本系统不独立判 APC==1→胜利);**[T,T,T,T] → APC=0(退化局,MVP 单线程不可达、属 Alpha 异步路径)→ 本系统仅 `assert`(AC-20),退化局检测 / fallback / 胜者取值全归破产9(bankruptcy AC-29),本系统不独立触发任何信号**。

### F-3 DoublesToJail — 连续双点入狱判定

`bShouldJail = (ConsecutiveDoubles >= DOUBLES_JAIL_THRESHOLD)`

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 连续双点 | `ConsecutiveDoubles` | int32 | 0..阈值 | 本回合链累计连续双点(本次掷骰更新后) |
| 阈值 | `DOUBLES_JAIL_THRESHOLD` | int32 | 2..5(默认 3,tuning knob) | 触发入狱的连续双点数;经典=3 |
| 入狱 | `bShouldJail` | bool | {T,F} | T=移动失效、回合终止、计数归零 |

**Output Range:** 布尔。用 `>=` 非 `==`(防御性:防状态加载异常跳过阈值)。
> **不变式 vs `>=` 防御的关系澄清(R2,消认知矛盾——systems-designer):** 阈值开局锁定后不变式 `ConsecutiveDoubles ∈ [0, threshold]` 在**正常逻辑路径**整局成立——即 `ConsecutiveDoubles > threshold` 正常**不可达**。`>=`(而非 `==`)是**额外的实现保险层(belt-and-suspenders)**:即便不变式理论成立,仍用 `>=` 兜住"内存损坏/读档位翻转致越级"等**非逻辑路径**异常(AC-23 测此越级)。二者不矛盾——不变式描述正常路径,`>=` 防御异常路径。实现者**无需**为 `ConsecutiveDoubles > threshold` 写专门的正常逻辑分支,但**应保留** `>=` 比较作为廉价防御。
**阈值开局锁定(R1 新增,堵存档+热交换炸弹):** `DOUBLES_JAIL_THRESHOLD` **开局定后整局恒定,局中不可改**。House Rules(23) 只在**开局前**配置该值,不暴露局中热切换。理由:若允许局中改,存档 `ConsecutiveDoubles=3`(在 threshold=3 下正常)、读档时 threshold 被改为 2 → `3>=2` 玩家未掷骰即入狱(`>=` 防御反成炸弹)。锁定后 `ConsecutiveDoubles ∈ [0, threshold]` 的不变式在整局成立,存档/读档安全。阈值本身随存档序列化(读档恢复开局锁定值),保证读档后判定与存档时一致。
**时序契约(本系统拥有,骰子3 只暴露 `bIsDouble` 不维护计数):** ① 骰子报 `bIsDouble`;② 若 T 则 `ConsecutiveDoubles+=1`(先累加);③ 再判 `DoublesToJail`:T→取消移动+入狱+归零+不发额外回合,F→正常移动+回合末发额外回合(CR-4);④ 若 `bIsDouble==F` → `ConsecutiveDoubles=0`。
**Example:** 阈值 3,连续 2 次后第 3 次双点 → 累加为 3 → `3>=3`=T → 入狱;第 3 次非双点 → 归零 → F。

### F-4 TurnOrderInitRank — 开局定序排名分配

按定序掷骰点数**降序**分配 `TurnOrderIndex`(0=最高=先手);平手者重掷。

`rank(i) = i 在 sort({(roll_i, i)}, key=roll_i desc) 中的位置`;`TieGroup(r)={i | roll_i=r}`,`|TieGroup|>1` 的组重掷。

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 掷骰点 | `roll_i` | int32 | 骰子定义(通常 2..12) | 玩家 i 定序掷骰点数(来自骰子3) |
| 席位 | `i` | int32 | 0..P-1 | 玩家席位索引(固定,≠TurnOrderIndex) |
| 重掷轮 | `round` | int32 | 0..MAX_TIEBREAK_ROUNDS | 第几轮重掷(0-based)。**off-by-one 澄清(R7):** 实际重掷发生在 `round ∈ [0, MAX_TIEBREAK_ROUNDS-1]`(共 `MAX_TIEBREAK_ROUNDS` 轮);`round` 达到 `MAX_TIEBREAK_ROUNDS` 是"已掷满上限、不再重掷、转席位裁定"的边界态(非再掷一轮)。与 L316/L318"达上限后裁定"一致 |
| 上限 | `MAX_TIEBREAK_ROUNDS` | int32 | 3..10(默认 5,tuning knob) | 平手重掷上限 |
| 排名 | `rank(i)` | int32 | 0..P-1 | 分配的 TurnOrderIndex(0=先手) |

**Output Range:** 全体 rank 构成 {0..P-1} 的完全排列(无重复)。
**`round` 作用域定义(R1 新增,消实现分歧):** `round` 是**全局单次计数器**,非每个平手子组独立计数。即:整个定序过程最多进行 `MAX_TIEBREAK_ROUNDS` 轮;**每一轮所有当前仍平手的子组同步各自重掷一次**(并行,非串行解决某个子组再处理下一个)。多个不相交平手子组(如 P=4 rolls=[9,9,5,5] → {0,1} 争 rank0-1、{2,3} 争 rank2-3)在同一轮同步重掷,确保所有子组消耗相同轮次预算,公平性一致。达 `MAX_TIEBREAK_ROUNDS` 后所有剩余平手统一按席位升序裁定。
> **裁定作用域=子组内(R2 澄清,消实现分歧——systems-designer):** 各平手子组竞争**不相交的 rank 段**(G1 争 rank0-1、G2 争 rank2-3),故达上限后席位升序裁定是**子组内部**(在该子组占据的 rank 段内,按席位索引升序分配),**非全局席位升序**。两者在席位跨子组交叉时结果不同——例 P=4 rolls=[9,5,9,5]:G1={0,2} 争 rank0-1、G2={1,3} 争 rank2-3,子组内裁定 → rank(0)=0,rank(2)=1,rank(1)=2,rank(3)=3。选子组内的理由:保持 rank 段边界(掷点高的子组恒占先手段),全局裁定会打乱"高点先手"语义。单一子组(全员同分,如 P=3 rolls=[9,9,9])退化为全局,二者等价。
**Gap 解(防死循环):** 平手仅平手子组重掷;达 `MAX_TIEBREAK_ROUNDS` 后剩余平手按**席位索引升序**裁定(确定性、无随机、有限终止)。`roll` 数值归骰子(3),排序逻辑归本系统。
**Example:** P=3, rolls=[9,12,9] → rank(1)=0 确定,{0,2} 平手重掷;重掷 [7,_,11] → rank(2)=1, rank(0)=2 → 玩家1先手、2 二手、0 末手。全员平手:P=3 rolls=[9,9,9] → 全部进重掷池,连续达 MAX_TIEBREAK_ROUNDS 后按席位 0,1,2 → rank 0,1,2。**P=4 全员同分(最大平手退化边界)rolls=[9,9,9,9]:** 单一 TieGroup={0,1,2,3} 争 rank0-3,连续达 MAX_TIEBREAK_ROUNDS 全平 → 席位升序裁定 → rank(0..3)={0,1,2,3},输出仍是完全排列 {0,1,2,3}、确定终止、不退化(单一子组退化为全局,与子组内裁定等价)。

### 识别的 2 处 Gap(已用默认解收口)
- **Gap-1 平手重掷无终止** → F-4 的 `MAX_TIEBREAK_ROUNDS` + 席位裁定。
- **Gap-2 全员破产退化** → F-1 的 `INDEX_NONE` GUARD + 要求调用方先查 CR-6 胜负,不进轮转。

## Edge Cases

> *lean 模式:`systems-designer` 未咨询(仅 D/H 派发)。本节由主会话起草。*

- **若开局玩家数 `P < 2`**:拒绝开局。回合制对战至少需 2 人;大厅(20)须保证 `P∈[2,4]`(MVP)。
- **若当前玩家在自己回合内破产**(无力付租/税):完成破产结算(9 `ResolveBankruptcy` 返回 `debtorEliminated=true`,本系统据此置 `bIsBankrupt`)→ 本回合立即 `TurnEnd`,该玩家移出轮转,行动权按 F-1 传下一未破产玩家。
- **若非当前玩家在他人回合内 `bIsBankrupt` 翻转**(如交易/拍卖致破产,Alpha;翻转仍由本系统据9 `ResolveBankruptcy` 返回值写,见 CR-6):本系统在任意 `bIsBankrupt` 翻转时即更新 `A` 集合;若被移出者恰是"下一个"待行动者,F-1 自动跳过。MVP 主路径破产只发生在破产者自己回合,此条为前向兼容。
- **若掷出双点但本回合被送监狱**(落 GoToJail 格 / 抽到入狱牌 / 第 3 次连续双点):**不发放额外回合**(CR-4 "未因此回合入狱"前置条件)。入狱优先于双点额外回合。
- **若移动回报 `OnPawnLanded.arrivalContext == SentToJail`**(因 GoToJail 格 / 入狱牌 / 第 3 次双点被直接传送入狱,见移动 AC-12b 强制透传)(R3 propagate,移动 OQ-T-Move-1 对端):`ResolvePhase` **不进**买地/收租/事件结算分支——被传送入狱非"正常落地",Jail 格不可购买 / 无租 / 无事件。本系统据 `arrivalContext` 路由:`SentToJail` → 跳过全部落地结算分支,直接置 `bIsInJail` 相关状态(进/出狱规则归7)并推进至 `PostRollAction`/`TurnEnd`。**危害域**:MVP 经典盘 Jail 格本不可购买 / 无租,即便不抑制也无实际收租;但自定义棋盘 / House Rules(23) 下 Jail 格可能携带可购买属性,届时不抑制将造成"被捕同时买地 / 收租"逻辑错误 → 标 **pre-Alpha gate**,不得无限期搁置(见 AC-46)。
- **若掷双点出狱**(在狱玩家掷双点离狱,规则归7):移动该点数,但**不触发 CR-4 额外回合**(用双点出狱是离狱条件,非"自由回合"的双点)。`ConsecutiveDoubles` 不因出狱双点累加。此交互边界归事件格(7)落实,本系统约定"出狱双点不进双点链"。
- **若掷双点后即破产**(双点移动 → 落地付租无力支付):破产移出,**不发放额外回合**(破产者已出局)。
- **若在狱玩家服刑达上限**(`JailTurnsServed` 达 7 定义的上限,经典 3):强制交保释(规则归7)后移动;本系统只计数 `JailTurnsServed`,上限值与强制逻辑归事件格(7)。
- **若开局定序连续平手达 `MAX_TIEBREAK_ROUNDS`**:剩余平手按席位索引升序裁定(F-4),保证有限终止。
- **若未破产玩家仅剩 1**:F-1 返回 `INDEX_NONE`,不进轮转;胜负由破产9 `ResolveBankruptcy` 返回 `winnerId`、本系统据此触发 `OnGameWon` 终局(R-2to9 propagate:取代旧"本系统广播 `OnLastPlayerStanding`")。回合推进逻辑**不得**在此情形继续调用 F-1。
- **若读档发生在回合中途**(如 `PostRollAction` 阶段存档):恢复时还原**精确阶段** + 当前回合玩家 + `ConsecutiveDoubles` + **当前掷骰结果完整 `FDiceRollResult`(含 Die1/Die2/Total/bIsDouble)** + **锁定的 `DOUBLES_JAIL_THRESHOLD`** + 全部 11 个 `PlayerState` 字段(逐一,含 `JailTurnsServed`),从该阶段续行。不得回退到 `TurnStart` 重掷(会重复掷骰/移动)。其中完整 `FDiceRollResult` 必须序列化:`bIsDouble` 供读档后 `TurnEnd` 判定是否该发双点额外回合;**`Die1`/`Die2` 供 VFX 读档后忠实重现两骰面值(dice B1/OQ-T-Dice-5,仅存 `Total=4` 无法判定是 1+3 还是 2+2)**。
  - **开局定序进行中禁止存档**(R1 明确):`TurnOrderInitRank` 阶段(可能多轮重掷)不允许存档,避免序列化"哪些席位已定 rank / 当前 round"的瞬态;存档仅在定序完成、进入首个 `TurnStart` 后可用。
  - **⚠ UE 实现地雷(R1 警告,归 OQ-1 ADR):** "还原精确阶段"要求状态机用 **`ETurnPhase` 枚举字段 + delegate 推进**(可序列化、读档 `switch(CurrentPhase)` 重入),**不可**用 Latent Action(Blueprint `Delay`/`WaitForEvent`)实现阶段等待——Latent Action 的等待状态在 Save 时丢失,读档无法从中途恢复,会强制回退 `TurnStart`,与本 Edge Case 直接冲突。此约束须在 OQ-1 ADR 落实。
  - **⚠ 读档恢复时序(R3,unreal R-3 —— delegate 绑定不进 SaveGame):** dynamic multicast delegate 的**绑定列表(invocation list)不序列化**(序列化会捕获旧实例的悬空 weak 引用)——只序列化 `ETurnPhase` 等数据字段,读档后由各下游**重新 `AddDynamic`/`AddUniqueDynamic` 重建绑定**(AC-34)。**关键顺序陷阱:** 读档恢复须 **① 反序列化所有 PlayerState/阶段字段 → ② 各下游重绑 delegate → ③ 才允许 TurnManager 续行/广播任何事件**。若 ② 之前 TurnManager 在 `PostLoad` 即按 `CurrentPhase` 广播 `OnPhaseChanged`,监听者尚未重绑 → 该次广播丢失。此顺序须在 OQ-1 ADR + 存档(21) 集成落实,回链 AC-34。
- **若 AI 玩家同步决策因实现 bug 挂死/超单帧预算**(实现层风险,R2 重定义兜底):AI 决策是**同步确定性调用**(CR-8),正常不阻塞。两类异常分别处理(R2 删除帧内虚拟时钟看门狗——单线程同步死循环帧内不可捕获):
  - **挂死/无限循环("永不返回")**:不在帧内捕获(GameThread 忙循环,虚拟时钟/同帧软件计时永不推进,不可能中断)。靠**单元测试框架进程级超时**在 dev/CI 暴露为测试失败=bug 信号。非生产机制。
  - **慢调用("返回但超单帧预算")**:同步调用**返回后**事后帧预算检查,超 `AITurnWatchdog` 阈值则 `UE_LOG` 诊断,不强制 `EndTurn`(已正常返回)。仅 dev 构建。
  (R1→R2 演进:原 wall-clock `AITurnTimeout` gameplay 旋钮[R1 废]→ R1 帧内虚拟时钟看门狗[R2 废,同步模型下不自洽]→ R2 拆为"测试框架超时 + 事后帧诊断"。回合制 AI 无需异步等待,wall-clock 超时是错误失败模型。)
- **若 AI 在 `JailTurn` 分支**:AI 经 `DecideJailAction` 同步返回出狱方式(交保释/掷双点/出狱卡),与人类经 UI 决策对称;不存在"在狱 AI 无 hook"的黑盒。
- **若同一玩家 `TurnOrderIndex` 重复或不连续**(开局定序 bug):`Validated` 阶段校验 `TurnOrderIndex` 恰好覆盖 `0..P-1` 各一次,否则拒绝开局(与 board-data 索引唯一性校验同模式)。

## Dependencies

### 上游(本系统依赖的系统)

- **骰子(3) — 硬依赖**:`RollPhase` 请求掷骰;F-3 消费 `bIsDouble`(双点逻辑);F-4 消费 `roll`(开局定序)。**⚠ 双向一致性修正(决策:确认依赖+修正索引):** systems-index 当前把系统#2 标为零依赖 Foundation(depends-on "—"),但本接口分析表明 turn(2) **硬依赖** dice(3)。Phase 5 将修正 systems-index:系统#2 depends-on 增加 3,并将系统#2 从"零依赖 Foundation"调整为"依赖骰子的 Foundation"(骰子3 仍为纯零依赖 RNG 服务,先于 2 设计或并行)。

### 下游(依赖本系统的系统)

全部硬依赖(除非注明):

| 系统 | 关系 | 读/写内容 |
|---|---|---|
| 移动(4) | 硬 | `MovePhase` 委派;经 `SetPosition` 受控写 `CurrentTileIndex` |
| 经济(5) | 硬 | 经受控写读改 `Cash`;读当前回合玩家 |
| 事件格(7) | 硬 | `ResolvePhase` 委派;写 `bIsInJail`(进/出狱规则归7);本系统持 `JailTurnsServed` |
| 破产胜负(9) | 硬 | 本系统调 `9.ResolveBankruptcy(debtor,creditor,snapshot)`→裁决;据返回值写 `bIsBankrupt` + 触发 `OnGameWon(winnerId)`(9 return-only,**不回调本系统、不直写字段**——见 CR-6/F-2) |
| AI(10) | 硬 | 消费 `OnTurnStarted(bIsAI)`,驱动 `PostRollAction` 决策;消费 `GameStateSnapshot`(OQ-1⑦,开工硬前提);**继承测试义务:AC-37b(同步决策+顺畅移交,N=1 ✅ 已由 AI(10) AC-46 回填 2026-06-04)+ RNG checklist + snapshot 字段齐备**;**须钉"三档 `AIDifficulty` 产生玩家可感知行为差异"契约(R3 OQ-6)** |
| 俄罗斯轮盘(14) | 软(Alpha) | 读 `PlayerState`、挂接回合(索引已标 14 depends 2,9) |
| HUD(16) | 硬 | 读 `PlayerState` 全字段 + 当前回合玩家 + 当前阶段;**消费 `OnAIActionExecuted(ActionIndex)`(非阻塞逐步播放 AI 买地/建房,按 ActionIndex 排序)+ `OnTurnOrderResolved`(席位裁定可见)+ `OnTurnEnded`(回合切换过场;payload `bGrantsExtraTurn` 区分"同玩家继续"vs"移交新玩家",两分支须各有动画 AC)**;**(R6 RETREAT:删除原 `OnAIActionPlaybackSegmentComplete` 回放完成回调义务——框架不再 gating,HUD 自主非阻塞呈现 AI 行动,见 Visual/Audio + OQ-8/OQ-9)**;**继承测试义务:AC-36(OnPhaseChanged 驱动 UI)+ AC-41 呈现侧 + AC-44 呈现侧(非阻塞展示 AI 行动)** |
| 游戏反馈 VFX(19) | 软(呈现层叶子) | 订阅 `OnPhaseChanged`(读 `ConsecutiveDoubles` 做双点庆祝/入狱警告差异化 + 定序语境抑制,payload 取值非属性轮询)+ `OnTurnStarted`(F-3 过期 epoch 主判据)+ `OnGameWon(WinnerId)`(胜利彩带/烟花 juice);视觉腿,不写本系统、不被回调(vfx-feedback CR-3/CR-7/OQ-VFX-7,R-1 揪出。**注:`OnBuildingAnnounced` 归 HUD banner,#19 不订阅**) |
| 主菜单大厅(20) | 硬 | 开局配置玩家数/名/色/AI 档 → 经具名入口 `StartNewGame(const FGameSetupConfig&)` 初始化 `PlayerState`(入口 + `FGameSetupConfig`/`FPlayerSetupEntry` USTRUCT 归本系统,见「开局入口契约」;OQ-Lobby-1 闭合) |
| 存档(21) | 硬 | 序列化全部 `PlayerState` + 定序 + 当前玩家 + 阶段 + `ConsecutiveDoubles` + 锁定阈值 + 完整 `FDiceRollResult`(含 Die1/Die2/Total/bIsDouble);**继承测试义务:AC-34(中途还原+重绑监听器)** |
| 设置&House Rules(23) | 软(Alpha) | 可覆盖 `doubles_jail_threshold`/`max_tiebreak_rounds` 等旋钮(**开局锁定,局中不可改**) |

> 经济(5) 在索引中 depends-on 仅标 2 —— 与本系统一致(经济依赖回合框架提供的当前玩家/PlayerState)。其余下游依赖与索引一致,Phase 5 复核。

## Tuning Knobs

| 旋钮 | 作用 | 安全范围 | 过低/过高后果 |
|---|---|---|---|
| `PlayerCount` (P) | 单局玩家数 | MVP 2..4(接口预留 8) | <2 无法对战(校验拒绝);过高单圈过长、UI 拥挤、AI 算量大 |
| `DOUBLES_JAIL_THRESHOLD` | 连续双点入狱阈值(F-3)。**开局锁定,局中不可改**(R1) | 2..5(默认 3=经典) | =1 任何双点即入狱(退化、惩罚过重);过高双点入狱形同虚设、连掷过多拖沓 |
| `MAX_TIEBREAK_ROUNDS` | 开局定序平手重掷上限(F-4) | 3..10(默认 5) | 过低频繁退化为席位裁定;过高极端连平时开局延迟 |
| `bEnableDoublesExtraTurn` | (House Rules 23 接口预留)CR-4 双点额外回合可选关闭,支柱④缓解阀(R1) | bool(默认 true=经典忠实) | false=禁用额外回合,削弱支柱①忠实但减 Pass'N Play downtime |
| `AITurnWatchdog`(原 `AITurnTimeout`) | **dev-only 事后帧预算诊断阈值**(R2 重定义):AI 同步调用**返回后**检查耗时超阈值则 `UE_LOG` 警告(纯诊断,不强制 EndTurn)。"永不返回"死循环不靠此捕获(单帧不可能)、靠测试框架进程级超时。**非 gameplay 旋钮** | dev 构建启用;生产默认禁用 | 仅诊断慢调用;不影响正常 AI 回合时长(同步决策即时返回) |
| `AIActionDisplayMode` | (R2 新增;R6 RETREAT 重定义——纯呈现旋钮,不再 gating 框架)**持有者 = HUD(16)/呈现层(非 TurnManager 框架;框架侧不读此值,见 AC-44③)**。控制 HUD 呈现 AI 行动的风格:`Animated`=呈现层经 `OnAIActionExecuted(ActionIndex)` 逐步播放 AI 动画(**非阻塞,框架不等待**);`Instant`=HUD 跳过 AI 动画。**两者均不阻塞框架流程,框架在 AI 决策执行完即时 EndTurn** | enum(默认 `Animated`) | `Instant`=AI 行动不播动画(玩家靠 HUD 摘要了解 AI 行动);具体呈现策略与时长归 HUD(16)/VFX(19) |
| `GameLengthCap`(cross-ref) | (R4——CR-4 downtime 真缓解之一)局长压缩/回合上限旋钮:CR-4 双点连走 downtime 的真缓解 = `bEnableDoublesExtraTurn` 开关 + **本旋钮**(见 L84,非呈现层)。**旋钮本体归 House Rules(23)/经济(5),此处仅 cross-ref 登记归属** | 归属系统定义 | 见 House Rules(23)/经济(5) GDD |

> **`MAX_TIEBREAK_ROUNDS` 性质澄清(R1,纠正概率直觉):** 此旋钮是**纯工程安全值**,非体验调参旋钮。P=4 连续 5 轮全平的概率约 (1/6)^(4×4) 量级(实际近乎永不触发),故"过低=少仪式感"的直觉是错的——低到 2 也几乎不会退化为席位裁定。默认 5 是保守上限,玩家在正常对局中感知不到此旋钮。

**旋钮间相互作用(调参注意):**
- `DOUBLES_JAIL_THRESHOLD` 与监狱规则(7)、局长联动:调低 = 入狱更频繁 = 节奏与现金流变化,调参须与事件格(7)/经济(5)协同(属跨系统旋钮)。
- `PlayerCount` 影响单圈时长与"经过 GO 发薪"总频率(现金注入节奏),与经济(5)、棋盘(1)的 `N`/`SalaryAmount` 联动。
- `DOUBLES_JAIL_THRESHOLD` / `MAX_TIEBREAK_ROUNDS` 注册于 entities registry(source=本系统),被破产(9)/俄罗斯轮盘(14)/House Rules(23) 引用,跨系统不一致由 `/consistency-check` 捕获。

> **数值来源说明:** `DOUBLES_JAIL_THRESHOLD=3` 来自经典大富翁规则(桌游忠实①);`MAX_TIEBREAK_ROUNDS` 是工程安全值、`AITurnWatchdog` 是 dev-only bug 兜底,均非平衡决策,原型期实测微调。

## Visual/Audio Requirements

**n/a —— 本系统是数据/流程层,不渲染视觉、不触发音频。** 玩家可见的回合呈现——"轮到你了"高亮、回合切换过场、当前玩家聚光、掷骰/双点反馈 juice——由 HUD(16)、游戏反馈 VFX(19)、音频(22) 的 GDD 拥有并实现。本系统只为这些呈现层**提供数据与事件**:`PlayerState` 字段、当前回合玩家、当前阶段、`OnTurnStarted`/`OnPhaseChanged`/`OnTurnEnded`/`OnGameWon`/`OnTurnOrderResolved` 事件。自身无视觉/音频职责。

> **Fantasy→接口最低契约(R1 新增,防"聚光灯感无人认领";R2 扩充):** 本系统的 Player Fantasy(聚光灯感、清晰轮流)兑现委派给呈现层,但本 GDD 须为下游钉一组**最低接口要求**(具体数值由 HUD(16)/VFX(19) GDD 定,此处声明"应有此需求"):
> - **当前玩家高亮**:HUD(16) 接到 `OnTurnStarted` 后须在**感知即时延迟内**呈现高亮。**R2 数值化(game-designer NR-2):** "感知即时"= **≤100ms 起呈现、≤500ms 完成视觉切换**(Flow 微反馈标准);HUD(16) AC 须用此可测数字,否则"延迟 2 秒仍合规但毁 Fantasy"。
> - **席位裁定可见**:接到 `OnTurnOrderResolved`(含席位裁定)后必须可见呈现最终顺序(不静默)。回链 AC-41 呈现侧。
> - **AI 行动可观察(R2 新增,NR-1;R3-R5 曾升框架 gating;R6 RETREAT 降回呈现层职责)**:接到 `OnAIActionExecuted(ActionIndex)` 后,呈现层**自主**以可观察节奏(非阻塞)呈现 AI 买地/建房(默认 `AIActionDisplayMode=Animated`,按 ActionIndex 排序),使玩家能了解对手行动(支柱②信息层)。**R6 RETREAT:框架不再 gating——AI 回合即时移交,不等待呈现层回放完成。** 为兑现支柱②,HUD(16) 应利用人类下一回合开始前的自然停顿展示 AI 行动摘要/动画(归 HUD 呈现设计,非框架契约;见 OQ-8 Submission 维度 + OQ-9 已解决)。HUD(16)/VFX(19) AC 验证非阻塞呈现(回链 AC-44 呈现侧)。
> - **Pass'N Play 回合交接(R2 新增,NR-4 提示;R3 B-2 补可测下限)**:Pass'N Play 多人共屏,`OnTurnStarted` 高亮兼具"请把设备交给下一位"的 handoff 信号。HUD(16) 设计须知此语境,设计引导性交接 UI(非仅"当前玩家高亮")。**R3 可测下限(game-designer B-2 —— 与 ≤100ms/≤500ms 同标准,消"handoff 无落点"缺口):** Pass'N Play 模式下 `OnTurnStarted` 触发的 handoff 高亮**须与"上一玩家回合视觉收束(outro)"在时序上不重叠**——HUD(16) AC 须验证"前一回合 outro 完成 → 才起 handoff 高亮,不叠帧",具体数值由 HUD GDD 定但**该 AC 必须存在**(回链 OQ-T-4)。本系统只提示语境 + 钉可测下限要求,不定义 UI。
> HUD(16) GDD 的 Acceptance Criteria 须含 `OnPhaseChanged`/`OnTurnStarted` 驱动的高亮状态转换验证(回链 AC-36),否则聚光灯 Fantasy 在测试体系中无落点。以上要求登记于 Dependencies HUD(16) 行。

## UI Requirements

**n/a —— 本系统无任何 UI。** 它不绘制屏幕元素,只向 UI 层暴露只读数据查询(`PlayerState`、当前回合玩家、当前阶段)与回合事件。所有面向玩家的回合/玩家信息显示(玩家面板、现金、当前回合指示、结束回合按钮)由 HUD(16) 的 GDD 与对应 `WBP_` Widget 实现;"结束回合"按钮的输入路由也由 HUD/输入层处理后回调本系统的 `EndTurn` 接口。本系统的"接口"是数据契约与事件,不是界面。

## Acceptance Criteria

> *由 `qa-lead` 校验(lean 模式 H 节派发):草案 25 条 → 补全 10 BLOCKING 缺口 + 2 标签拆分 + 4 跨系统 OQ-T 追踪 → 35 条。*
> ***R1(design-review full)新增/改写:** AC-5 拆 5a/5b(非法转移拒绝);AC-12 双发计数;AC-17b survivor≠cur 守卫;AC-33 升 FSM resume 语义;AC-35 措辞降级+ADR 分叉(AC-35a [Logic]条件门);AC-37 拆 37a [Logic]虚拟时钟/37b [Advisory];AC-38 ResolvePhase 买地 hook;AC-9 额外回合不归零;AC-11b JailTurnsServed 计数时机;AC-27 P>4 上界。*
> ***R2(fresh re-review full)新增/改写:** AC-37a 删不可实现的虚拟时钟挂死测试;AC-37c 新增帧预算诊断[Advisory];AC-12 校正测事件广播幂等(非系统9逻辑);AC-29 补 mock bIsBankrupt 前提;AC-34 明确序列化范围+重绑监听器;AC-37b 措辞可测化(帧指标);AC-35a 移入 OQ-1(消虚假[Logic]gate);新增 AC-39(DecideJailAction hook)/AC-40(APC==0 广播)/AC-41(OnTurnOrderResolved)/AC-42(批处理跳过)/AC-43(append-only 枚举)。共 35→42 条(+AC-37c)。*
> ***R3(fresh re-review full)新增/改写:** **BLK-R3-1** AC-37c 删「断言 UE_LOG fire」+「mock 真烧 wall-clock」改纯文档化人工 dev 验证(斩断 wall-clock 后门复活,非自动化);**BLK-R3-2** AC-40③ 退化局标记改断言 `OnLastPlayerStanding.bDegenerateTie==true`(给可测载体);**BLK-R3-3** AC-42① 改状态断言(被跳过动作目标状态未改)、UE_LOG 降括注;AC-37a 补 mock 可注入性前提(依赖 OQ-1⑤,否则降 Advisory);AC-32 补 mock bIsBankrupt 前提(同 AC-29);AC-12 补"边沿广播"前提澄清。新增 **AC-38b**(DecideBuyProperty 返 true 现金不足→不扣负、视同不买)/**AC-39b**(非法 JailAction:PayBail 无现金/UseCard 无卡→降级留狱)/**AC-37d**(PostRollAction 空数组=EndTurn)/**AC-44**(Animated gating 等回放/Instant 不等)/**AC-45**(DecideAuctionBid sentinel 值域,[Advisory] Alpha)。共 42→47 条。*
> ***R4(fresh re-review full)新增/改写:** **BLOCK-1** AC-38b 删断言项③「dev 诊断 fire」、log 降括注对齐 AC-42(断言对象只留可观测状态);**BLOCK-2** AC-44① 扩 N>1 多动作 gating + per-action 早释防护;**BLOCK-3** AC-44③ 改生产安全阀 `AIPlaybackGateProdSafetyValve` 强制推进断言、可注入超时声明;AC-39b log 降括注 + 补 OQ-1⑤ 可注入条件门(否则降 Advisory);AC-40 新增④断言 `WinnerId==INDEX_NONE`;**新增 AC-17c**(F-1 cur=-P 入口规范化枚举终止性)。共 47→48 条。*
> ***R5(fresh re-review full)新增/改写:** **AC-17c** 扩三变体(整除 cur=-4/非整除 cur=-2/正越界 cur=5,覆盖欧几里得取模三分支);**AC-44** 改造为框架侧计数收敛——①(c) per-action 早释变真框架侧 [Logic](集合判据天然拦截)、新增 ①(d) double-fire 幂等、③ 改 `[Logic·条件]` 默认 [Advisory] 删"旋钮置极小值"后门钉 `SimulateTimeout()`;**AC-5b** 补② AIPlaybackGating 非法转移;**AC-39b** ② 补降级留狱 `JailTurnsServed+1` 对称。AC 总数仍 48(均为既有 AC 扩写,无新编号)。*
> ***R6(fresh re-review full → MAJOR REVISION,用户裁定 RETREAT)新增/改写:** **AC-44** 重写为非阻塞 hook 测试(删 gating 集合收敛/per-action 早释/生产安全阀/double-fire/`SimulateTimeout()` 全部断言,改断言"Animated/Instant 均即时 EndTurn + 逐条广播 ActionIndex");**AC-5b** 删② AIPlaybackGating 非法转移项(相位已删);**新增 AC-7b**(`OnTurnEnded.bGrantsExtraTurn` 生产侧断言,qa BLK-R6-3)/**AC-5c**(`OnPhaseChanged.ConsecutiveDoubles` 生产侧断言,qa BLK-R6-4)。AC 总数 48→50(+AC-7b/AC-5c)。*
> ***R7(fresh re-review full → NEEDS REVISION,3 blocker 单遍收口)新增/改写:** **Blocker-1** `ActionIndex` 强度对齐——散文(L138/L152/L215)从"呈现排序提示"升为"框架拥有的轻量排序契约"(框架保证 `0..M-1` 自增、不消费做 gating),AC-44① 保持 [Logic];**Blocker-2** AC-44③ 重写为静态可测"框架不读 `AIActionDisplayMode`、无 `WaitFor*`/`Delay`/`FTimerHandle` 等待调用"(删除"两模式移交时机相同"空命题)+ Tuning 明确 `AIActionDisplayMode` 持有者=HUD;**Blocker-3** L184 状态表去 `AIPlaybackGating` 具名 + AC-43 补 append-only 例外(草稿值从未分配索引)+ OQ-1 ⑬⑯ 从因子清单物理移除(空缺不复用)。同批 recommended:AC-44①② fixture N/M 语义分注;AC-5c `≥1`→三变体精确值;AC-7b/AC-5c 补 OQ-1⑤ 可注入条件门;F-4 round off-by-one 校正。**AC 编号口径澄清:** 历轮 changelog 的"总数"按父编号计;实际独立子编号可执行条目数 = **55**(含 5a/5b/5c、7/7b、11/11b、17/17b/17c、37a–d、38/38b、39/39b 等子编号,AC-18 历史空缺不存在)。两口径并存仅影响对外报数,不影响测试执行。*
> ***building-upgrade R4 propagate(2026-06-04,兑现 OQ-Build-1/OQ-Build-2/OQ-Build-6 producer 债)新增:** 新增 **CR-3.3**(强制清算驱动循环,Raising Funds 阻塞子相 — 本系统拥有执行驱动,止损优先 mortgage-empty-first,OQ-Build-1 RESOLVED)/**CR-3.4**(破产 NLV 全组合聚合 — `GetPlayerBuildings` 全枚举传 economy `is_insolvent(player, amount_due, preaggregated_nlv)`,区分 CR-3.2 单格收租聚合,OQ-Build-2 RESOLVED)/**CR-3.5**(建房通告 beat — `ResolvePhase` 监听 `OnBuildingChanged` 并向全玩家广播 `OnBuildingAnnounced`,OQ-Build-6 RESOLVED)。新增 **AC-50**(Mortgage 前 GetHouseCount==0 前置读,spy 断言 house_count>0 的 tile 不调 Mortgage)/**AC-51**(清算顺序验证:空地先抵押、有房地后卖房、Cash≥amount_due 早停、全资产耗尽→is_insolvent)/**AC-52**(破产 NLV 聚合:GetPlayerBuildings 恰 1 次 + preaggregated_nlv 传入 is_insolvent + zero economy→8 calls spy)/**AC-53**(建房通告 beat:OnBuildingAnnounced 广播 1 次 + payload 字段正确 + 不改状态;HUD 继承义务登记)。实际独立子编号可执行条目数升至 **59**。*
> **测试分层:** `[Logic]` 纯 fixture、headless 可跑、作 PR merge gate;`[Advisory]` 集成/code-review/OQ 守门。

### A. 核心规则(CR-1~7)
- **AC-1** [Logic] PlayerState fixture 含全 11 字段、正确类型与默认(bIsAI=false→AIDifficulty=None;bIsBankrupt/bIsInJail=false;JailTurnsServed/ConsecutiveDoubles=0)。
- **AC-2** [Logic] 定序无平手 rolls=[5,9,3]→TurnOrderIndex{seat1:0,seat0:1,seat2:2}。
- **AC-3** [Logic] 平手触发重掷 rolls=[9,9,3]→{seat0,seat1} 标记重掷、seat2 固定 rank2。
- **AC-4** [Logic] 平手达 MAX_TIEBREAK_ROUNDS→剩余平手{0,2}按席位升序裁定(确定性)。
- **AC-5a** [Logic] 正常回合 `OnPhaseChanged` 事件按 [TurnStart→RollPhase→MovePhase→ResolvePhase→PostRollAction→TurnEnd] **精确顺序** fire,无跳跃、无缺失。
- **AC-5b** [Logic](R1 新增;R6 RETREAT 删除 ② AIPlaybackGating 项)非法转移拒绝:RollPhase 阶段发送 `EndTurn` 信号→断言状态机保持 RollPhase 不变(或抛 assertable error),不跳转 TurnEnd。(R5 曾补 `AIPlaybackGating` 相位非法转移项,R6 retreat 删除该相位后此项随之移除。)
- **AC-5c** [Logic](R6 新增,qa BLK-R6-4——`OnPhaseChanged.ConsecutiveDoubles` 生产侧断言;**R7 精度收紧**)`OnPhaseChanged` 进入 RollPhase 时,断言 payload `ConsecutiveDoubles` 与框架当前状态一致。**三变体精确值断言(非 `≥1` 宽松判据——防"payload 永远广播固定偏差值"逃逸,qa R7-7):** ① 首次 RollPhase→payload `==0`;② 构造第一次双点后额外回合进 RollPhase→payload `==1`;③ 构造第二次双点后额外回合 RollPhase→payload `==2`(与已 +1 的链计数一致,见 L129 重发语义)。验证 HUD 区分"首次/额外回合 RollPhase"的载体字段在源头逐值正确。**前提(qa R7-8):** 需 mock 骰子(3) 返回 `bIsDouble=true` 推进双点链;若 OQ-1⑤ ADR 选骰子 hook 不可注入形态,本 AC 降级 [Advisory](同 AC-37a/AC-39b)。
- **AC-6** [Logic](GAP-1)在狱玩家 TurnStart→路由进 JailTurn 分支(非 RollPhase)。
- **AC-7** [Logic] 双点未入狱未破产→TurnEnd 后同玩家回 RollPhase,断言 ConsecutiveDoubles 已 +1。
- **AC-7b** [Logic](R6 新增,qa BLK-R6-3——`OnTurnEnded.bGrantsExtraTurn` 生产侧断言)双点未入狱未破产路径 TurnEnd→断言广播 `OnTurnEnded` 的 payload `bGrantsExtraTurn==true`(同玩家额外回合);非双点正常移交路径 TurnEnd→断言 payload `bGrantsExtraTurn==false`(移交新玩家)。验证 payload 字段在源头正确,防 OQ-T-4④ HUD 消费侧以错误值"通过"(payload 字段无生产侧断言=死字段风险)。**前提(qa R7-5):** 需 mock 骰子(3) 返回 `bIsDouble` 两值驱动两分支;若 OQ-1⑤ ADR 选骰子 hook 不可注入形态,本 AC 降级 [Advisory](同 AC-37a/AC-39b)。与 AC-9 共享双点/非双点 fixture 基础结构,实现可参数化复用、勿写两套 setup。
- **AC-8** [Logic] ConsecutiveDoubles=3→发 SendToJail 意图+取消移动+无额外回合+归零。
- **AC-9** [Logic](GAP-2,R1 强化)双点额外回合路径:第 1 次双点后 ConsecutiveDoubles==1;TurnEnd 判为"同玩家额外回合"时 ConsecutiveDoubles **不归零**(保持 1);仅当行动权**实际移交下一玩家**(非双点结束)时归零。防"每次 TurnEnd 都归零致第3次双点永不入狱"。
- **AC-10** [Logic] CR-5 推进跳过破产(见 F-1 AC-14~18)。
- **AC-11** [Logic](GAP-3)下一玩家在狱(非破产)→仍获回合(路由 JailTurn),不跳过。
- **AC-11b** [Logic](R1 新增,JailTurnsServed 计数时机)在狱玩家 JailTurn 进入时 JailTurnsServed 尚未变;事件格(7) mock 返回"留狱"后 +1;返回"出狱"时**不**增加(出狱无需计入)。明确计数发生在"确认留狱"后,非进入即 +1。
- **AC-12** [Logic](**R-2to9 propagate 重写;R-2to9 RR 收紧——测本系统 `OnGameWon` 边沿幂等,非系统9 胜负逻辑**)fixture 注入可**连续返回**的 mock 破产9 `ResolveBankruptcy` + `OnGameWon` 监听计数器(初值 0)。WHEN ① 第一次返回 `{debtorEliminated=true, winnerId==B.PlayerId}`(末局 APC→1)→ 本系统据此触发;② **立即再次**以同 fixture 调 `ResolveBankruptcy` 返回**相同** `{debtorEliminated=true, winnerId==B.PlayerId}`(模拟意外重入 / 重试 / 终态后再查询)。THEN 监听计数**精确等于 1**——第一次触发,第二次因本系统已进 `Winner` 终态被边沿守卫拦截、**不重发**(CR-6 已规格化「终态不重发」,本 AC 是该规格的可测载体,非测未定义实现细节)。同时断言 F-1 守卫返回 INDEX_NONE。**时序前提(R-2to9 RR,REC-6):** 本系统须**先**完成 `bIsBankrupt` 写入(AC-40a③ 语义)→**再**触发 `OnGameWon`→**再**更新 F-1 输出;F-1 守卫返回 INDEX_NONE 隐含依赖此顺序,若实现乱序则 F-1 断言失败提示**时序 bug**(非 F-1 公式错误)。(注:胜负**判定**幂等归破产9 AC;本系统只测事件广播次数。)
- **AC-13** [Logic] P=2 与 P=4 结构按 P 动态尺寸(无硬编码 4)。**(OQ-Lobby-1 闭合后接口名冻结,2026-06-06)** 经具名入口 `StartNewGame(const FGameSetupConfig&)` 注入 `Players.Num()∈{2,4}` 的 `FGameSetupConfig`,断言生成的 `PlayerState` 列表尺寸 == `Players.Num()`、`TurnOrderIndex` 覆盖 `0..P-1` 唯一(无硬编码 4,见 AC-28)。

### B. 公式(F-1~4)
- **AC-14** [Logic] F-1 NextActivePlayer(2,4,{0,2,3})→3。
- **AC-15** [Logic] F-1 环绕+跳过 (3,4,{0,3})→0。
- **AC-16** [Logic] F-1 多跳 (0,4,{0,3})→3(跳1,2)。
- **AC-17** [Logic] F-1 守卫 (0,4,{0})→INDEX_NONE（|A|=1 且唯一存活者==cur）。
- **AC-17b** [Logic](R1 新增,覆盖原缺陷分支)F-1 守卫 (0,2,{1})→**INDEX_NONE**（|A|=1 且唯一存活者 ≠ cur）。断言**不**返回 1——验证守卫先于枚举,胜者已由 CR-6 裁定,不存在"下一玩家"。
- **AC-17c** [Logic](R4 新增,R5 扩三变体——欧几里得取模三分支覆盖)F-1 入口规范化枚举终止性,**全员存活 A={0,1,2,3}、P=4**,三个损坏入参变体(断言 `next∈A 且 ≠cur_safe`、不返回负值、不无命中终止):
  - **① cur=-4(负·整除特例)** → `cur_safe=((-4%4)+4)%4=0` → next=1。(注:`-4%4=0`,整除点规范化平凡通过——**此变体最不暴露 bug**,故须配②③。)
  - **② cur=-2(负·非整除)** → `cur_safe=((-2%4)+4)%4=((-2)+4)%4=2` → next=3。(真正考验欧几里得取模 `+P` 项;漏 `+P` 的错误实现在此变体失败而①通过。)
  - **③ cur=5(正向越界)** → `cur_safe=((5%4)+4)%4=(1+4)%4=1` → next=2。(覆盖 cur≥P 越界分支。)
  验证 F-1 主公式入口规范化(`cur_safe=((cur%P)+P)%P`)对负·整除/负·非整除/正越界三分支均消除枚举无命中洞。**(R5:此前仅① cur=-4 是整除特例,单点抽样不足以验证规范化实现——qa F-1/systems 1-C/ai ISSUE-7 收敛。)**
- **AC-19** [Logic] F-2 ([F,T,F,F])→3;([F,T,T,T])→1。**前提(R-2to9 RR,REC-2):** fixture 在本系统**已据9 返回值写完 `bIsBankrupt` 之后**读取 F-2(对齐 L321「写后计数辅助」口径,避免与「debtor flag 未写时刻」混淆——后者归9 在快照上显式排除 debtor 算,见 bankruptcy F-2);`[F,T,T,T]→1` 末局变体的 `OnGameWon` 触发由 **AC-40a** 覆盖,本条只测 F-2 计数输出、不重测信号触发。
- **AC-20** [Logic](GAP-6)F-2 ([T,T,T,T])→0 触发 assert(正常流程不可达)。
- **AC-21** [Logic] F-3 DoublesToJail(3,3)→true;(2,3)→false。
- **AC-22** [Logic](GAP-7)非双点→ConsecutiveDoubles 置 0、DoublesToJail→false。
- **AC-23** [Logic](GAP-8)F-3 >=防御 (4,3)→true(异常越级仍入狱)。
- **AC-24** [Logic] F-3 阈值旋钮 (2,2)→true(house rule=2)。
- **AC-25** [Logic] F-4 排名 rolls=[5,9,3]→{seat1:0,seat0:1,seat2:2}。
- **AC-26** [Logic] F-4 平手席位裁定(达上限)→席位升序。

### C. Edge / 防护
- **AC-27** [Logic] P<2→拒绝开局;**P>4(MVP 上限,接口预留 8 的边界)→拒绝开局或返回特定错误码**(R1:与大厅20接口契约对齐,本系统作防守层不静默接受越界 P)。
- **AC-28** [Logic] TurnOrderIndex 未覆盖 0..P-1 唯一→拒绝。
- **AC-29** [Logic](R2 补 mock 前提)当前玩家自回合破产→TurnEnd 移出、F-1 推进。**前提:fixture mock 破产9 置 `bIsBankrupt=true`**(不依赖真实破产9结算,保持 [Logic] headless 可跑)。
- **AC-30** [Logic] 双点但本回合入狱→无额外回合。
- **AC-31** [Logic](GAP-4)双点出狱→ConsecutiveDoubles **不**累加、无额外回合(本系统可测部分;完整出狱流程归7,见 OQ-T-1)。
- **AC-32** [Logic](GAP-9;R3 补 mock 前提同 AC-29)双点后落地破产→移出、无额外回合。**前提:fixture mock 破产9 置 `bIsBankrupt=true`**(不依赖真实破产9结算,保 [Logic] headless 可跑)。
- **AC-33** [Logic](R1 升级:字段 roundtrip → FSM resume 语义)构造处于 `PostRollAction`(currentPlayer=1, ConsecutiveDoubles=2, 含完整 `FDiceRollResult`{Die1/Die2/Total/bIsDouble})的 TurnManager fixture→序列化→反序列化到新实例→断言:① phase==PostRollAction;② currentPlayer==1;③ ConsecutiveDoubles==2;④ **向新实例发 `EndTurn`→状态机进入 TurnEnd(而非在错误阶段忽略信号或崩溃)**。④ 验证 FSM 真正处于正确状态,非仅字段值正确。

### D. 契约/跨系统(Advisory)
- **AC-34** [Advisory](LABEL-2 + OQ-T-2;R2 明确范围)**完整存档**=11 个 PlayerState 字段 + `ConsecutiveDoubles` + 当前阶段 + 完整 `FDiceRollResult`(含 Die1/Die2/Total/bIsDouble) + 锁定的 `DOUBLES_JAIL_THRESHOLD`(对齐 Edge Cases 序列化字段列表)→读档于 PostRollAction 还原精确阶段;归存档(21) Integration,回链 OQ-T-2。**含读档后重绑该阶段 delegate 监听器**(unreal R2:枚举字段恢复值不够,须重 `AddDynamic` 该阶段事件绑定)。
- **AC-35** [Advisory] 受控写契约(R2:AC-35a 条件门移入 OQ-1,消除虚假 [Logic] gate——qa-lead BLK-2)。**当前稳定门控 = [Advisory] code-review**:受控写是架构约定,BP 无法引擎级硬拦截调用方(软约束),首次实现保留 code-review checklist,记录于 `production/qa/evidence/`。
  - > **AC-35a 待 ADR 补回(R2):** 若 OQ-1 ADR 最终选 **C++ 强封装**(字段 private + C++ setter + BP 只读),则受控写升为**可回归硬门**,届时由 qa-lead **新增** `AC-35a [Logic]` 架构/静态测试(扫描非 `TurnManager`/`PlayerStateController` 文件,断言无对 `PlayerState` 字段[Cash/CurrentTileIndex/bIsInJail/bIsBankrupt]的直接赋值,仅允许经 setter)。**在 ADR 签署前 AC-35a 不作为 AC 列表确定性 gate 条目**(避免"永远 pending 的 gate")。此条件登记于 OQ-1。**ADR 关闭前置条件锚点(R3,qa-lead REC-R3-3——防"补回承诺飘在记忆里"):** "由 qa-lead 补回"须锚到一个**会真实发生的事件**:**OQ-1 ADR 标记 `resolved` 时,若容器选 C++ 强封装 → 必须同批新增 AC-35a 并更新 review log,否则 ADR 不得标 resolved**(把补回 AC-35a 变成 ADR 关闭的前置条件,而非飘在 qa-lead 记忆里的承诺)。已登记于 OQ-1 关闭检查清单。
- **AC-36** [Advisory](LABEL-1 + OQ-T-4)`OnPhaseChanged` 驱动 UI;归 HUD(16) Integration,回链 OQ-T-4。
- **AC-37a** [Logic](R1 新增;R2 删除不可实现的虚拟时钟挂死测试;R3 补 mock 可注入性前提)AI `PostRollAction` 同步决策:注入 mock AI 同步返回动作列表→框架执行毕→断言阶段切到 TurnEnd、行动权移交下一未破产玩家。**确定性 fixture,作本系统 PR gate。** **前提(R3,qa-lead REC-R3-1):** AI hook 须经 interface/依赖注入暴露,fixture 可替换为 mock——**具体形态待 OQ-1⑤(BlueprintNativeEvent / UInterface);若 ADR 最终选不可注入形态,AC-37a 降级 [Advisory]**(避免重蹈 AC-35a「强度依赖未决 ADR 的 [Logic] gate」风险)。 (R2:原第二段"挂死 mock AI + 虚拟时钟→强制 EndTurn"已删——同步死循环是 GameThread 忙循环,单线程下虚拟时钟无法推进、测试线程被卡,该测试技术不可实现。"永不返回"由测试框架进程级超时暴露,非本 AC 范围;"慢调用诊断"见 AC-37c。)
- **AC-37c** [Advisory · 非自动化](R2 新增;**R3 BLK-R3-1 改写——斩断 wall-clock 后门复活**)dev 构建下 `AITurnWatchdog` 事后帧预算诊断为**不可自动断言的辅助日志**(无约定 log category/格式 → 不能作测试 oracle;且制造"超阈值"须 mock 真烧 wall-clock → 把 R1/R2 清除两轮的 wall-clock 非确定性引回、违反 Determinism 规则、CI flaky)。**验证方式 = 人工 dev 构建跑慢 mock,目视 Output Log 出现诊断行,记录于 `production/qa/evidence/`。非自动化测试,非 PR gate,不写「断言 UE_LOG fire」。** (R2 原写法"断言诊断 fire"已删——dev 日志不是可断言 oracle、wall-clock 计时不可进确定性测试体系。)
- **AC-37b** [Advisory](OQ-T-3;R2 措辞可测化)真实 AI(10) 集成:AI 决策在**单帧预算(≤16ms,60FPS)内同步返回**、行动权在 `OnTurnStarted` fire 后**≤N 帧内移交**下一未破产玩家(**N=1**,已由 AI(10) GDD AC-46 回填——AI 决策+批执行+EndTurn 单帧完成,无 async/Latent,2026-06-04;将来若引入 Latent 节点须重裁 N 并同步本 AC);归 AI(10) Integration,回链 OQ-T-3。("正常预算/顺畅"已改为可观测帧数指标。)
- **AC-38** [Logic](R1 新增,CR-8)ResolvePhase 买地决策点:AI 落无主地产→框架同步调 `DecideBuyProperty` hook(mock 返回 true/false)→断言购买意图按返回值触发/不触发。验证 AI **有买地 hook**(消除"AI 永远无法买地"断链)。
- **AC-38b** [Logic](R3 新增,BLK-1 买地失败语义)mock `DecideBuyProperty` **返回 true 但 fixture `Cash < PropertyData.Price`**(或地产 mock 为已被买)→框架委派经济5/所有权6 执行期校验→断言:① **不扣成负现金**(Cash 字段未变);② 购买**视同 `false`(不买,地产所有权未变更)**;③ 回合不崩、正常推进到下一阶段。验证 CR-8「执行期可行性校验归经济5/所有权6、不可行视同 false」收口(可行性 mock 由经济5 模拟)。**(R4 BLOCK-1:删原断言项③「dev 诊断 fire」——dev 诊断日志为辅助信息、非断言对象,不写「断言 UE_LOG fire」,对齐 AC-42 标准;断言对象只留可观测状态。)**
- **AC-39** [Logic](R2 新增,CR-8 覆盖缺口)JailTurn AI hook:在狱 AI 玩家 JailTurn 进入→框架同步调 `DecideJailAction`(mock 返回 `EJailAction::PayBail`/`RollDouble`/`UseCard`)→断言出狱/留狱路径按返回值正确发起(保释意图/掷骰意图)。验证"在狱 AI 无 hook"黑盒不存在(CR-8/Edge Cases 承诺)。
- **AC-39b** [Logic](R3 新增,BLK-2 非法 JailAction 失败语义)mock `DecideJailAction` 返回**前提不成立的非法值**:① 返回 `PayBail` 但 fixture `Cash < 保释金`→断言降级为"留狱"(`JailTurnsServed+1`)、**Cash 未变(不扣成负现金)**;② 返回 `UseCard` 但 fixture 无出狱卡→断言降级留狱(**`JailTurnsServed+1`,与 ① 对称——R5 ai ISSUE-5:降级留狱=留狱的一种,同 AC-11b「确认留狱后 +1」语义**)、**出狱卡持有数未变(不凭空消卡)**。两路均断言回合不崩、正常推进。验证非法返回不崩、按 CR-8「校验归事件格7、不可行降级留狱」收口(可行性 mock 由事件格7 模拟)。**(R4:① dev 诊断日志降为辅助、非断言对象,对齐 AC-42/AC-38b;② 前提:事件格7 可行性校验须经 interface/依赖注入暴露[OQ-1⑤],fixture 可替换为 mock;若 ADR 选不可注入形态,AC-39b 降级 [Advisory]——同 AC-37a。)**
- **AC-40a** [Logic](**R-2to9 propagate 核心 return-only 契约;R-2to9 RR 拆分——不随 OQ-1⑤ 降级**)在 **C++ headless test 层**注入 `IResolveBankruptcy` stub(C++ stub 注入于 UE Automation headless **恒可行**,独立于 OQ-1⑤ 容器/BP-注入形态选择),断言本系统侧三条契约:① 返回 `{debtorEliminated=true, winnerId==B.PlayerId}`(APC==1)→本系统触发 `OnGameWon(WinnerId)` 恰 1 次、payload `WinnerId==B.PlayerId`;② 返回 `winnerId==INDEX_NONE`(对局未结束 APC>1 / 退化局 APC==0 两情形)→本系统**不**触发 `OnGameWon`(广播计数==0);③ 据返回 `debtorEliminated=true` 断言本系统置 `bIsBankrupt` + 移出轮转。**此条是无环 return-only 架构安全阀的机器证明底线 —— 不得降级**(stakes 高于 AC-37a 的「AI 可观察性」:本条验的是「游戏能否正确结束 + bIsBankrupt 是否正确写入」,return-only 无环纪律若无机器证明则 CI 无法验,qa-lead RR BLK-3)。退化局(APC==0)的 assert/fallback/胜者取值归破产9 内部(见 bankruptcy AC-29),本系统不重测。旧"本系统广播 `OnLastPlayerStanding` + `bDegenerateTie` payload"随 return-only 删除。**对应9 输出侧验证见 bankruptcy AC-40(两条 AC 互引,改接口须同步)。**
- **AC-40b** [Logic→Advisory](BP mock 注入变体,OQ-1⑤ 依赖)在 **Blueprint fixture 层**注入 mock 破产9 `ResolveBankruptcy`,覆盖 AC-40a 同三条契约的 BP 层验证;若 OQ-1⑤ ADR 选**不可注入形态**,本变体降 [Advisory]——但 **AC-40a(C++ stub)已兜机器证明底线,核心 return-only 契约不掉出 CI**(降级先例同 AC-37a,但底线由 AC-40a 守,非整条契约可降)。
- **AC-40c** [Logic](**R-2to9 RR followup —— 封堵旧 2↔9 双触发 bug 复发路径;qa-BLK-4 + systems-REC-1 收敛**)GIVEN 2 名玩家:玩家A `bIsBankrupt=true`(**直接置 flag、不经9 `ResolveBankruptcy`**,模拟 A 已在前序回合被9 裁决出局)、玩家B `bIsBankrupt=false`;WHEN 玩家B 完成 TurnEnd、F-1 推进(本回合**无任何 `ResolveBankruptcy` 调用发生**);THEN `OnGameWon` 广播计数 **==0** —— 本系统**不**因自身 F-2 `APC==1` 独立触发胜负信号(只有9 返回 `winnerId!=INDEX_NONE` 时才触发,见 L321/CR-6)。**与 AC-40a② 互补:②测「9 返回 INDEX_NONE→不触发」;本条测「9 根本未被调→不触发」**——验证 L321 散文禁令(回合2 不独立判胜)在代码层有回归覆盖,直击 change-impact §1 旧 bug 根因(旧 player-turn 在9 写 flag 前自算 APC 触发 `OnLastPlayerStanding` 致双发/反向)。
- **AC-41** [Logic](R2 新增,CR-2 OnTurnOrderResolved 覆盖)定序完成→断言 `OnTurnOrderResolved` 被广播且 payload 含最终 TurnOrder 数组:① 无平手正常定序完成也广播;② 平手达 MAX_TIEBREAK_ROUNDS→席位裁定后广播(呈现层有反馈钩子,席位裁定不静默)。
- **AC-42** [Logic](R2 新增,CR-8 批处理失败策略;**R3 BLK-R3-3 ① 改状态断言**)`DecidePostRollActions` 返回含"执行时已不可行"动作的列表(如先抵押耗尽现金、后续建房动作付不起)→框架逐动作执行前重校验→断言:① **不可行动作被跳过 = 该动作的目标状态未被改变**(如被跳过的建房动作→该地产房屋数不变;`UE_LOG` 诊断为辅助、非断言对象——R3 取代 R2"断言 UE_LOG fire",因 [Logic]/PR gate 不可依赖无约定格式的日志);② 列表后续合法动作仍执行;③ 回合不中断、处理完毕后 EndTurn。验证"批处理遇过期快照不崩、不丢整批"。**覆盖 L104 两类过期:** 含"先抵押耗现金致后续建房付不起"与"先建房改均匀约束致后续建房违反约束"两个 fixture 变体(R3 N-1)。
- **AC-43** [Advisory](R2 新增,append-only 枚举约束)`ETurnPhase`/`EPlayerColor`/`EAIDifficulty` 已有枚举值的整数索引未变化(防重排破坏旧存档)。C++ 实现可加 `static_assert` 固定既有枚举值整数;BP-only 环境靠 code-review。归架构约束,与 AC-35a 同性质,强度随 OQ-1 ADR。
- **AC-37d** [Logic](R3 新增,CR-8 空数组路径——最高频 AI 回合)mock `DecidePostRollActions` 返回**空数组 `[]`**(AI 本回合不做任何 PostRollAction)→断言框架**直接 EndTurn**(执行 0 条动作即等价 EndTurn)、阶段切 TurnEnd、行动权移交。覆盖"AI 大多数回合不建房不抵押"这一最高频路径(此前 AC-37a 测非空列表、AC-42 测含不可行列表,空数组无专门 AC)。
- **AC-44** [Logic](R3 新增;R4/R5 曾扩为框架 gating;**R6 RETREAT 重写——框架不再 gating**)AI 行动可观察性 hook(非阻塞):mock AI 返回**含 N≥1 条可行动作**(如 `[买地, 建房]`)→断言:
  - ① **(全部可行 fixture:N 条动作全部可执行 → 执行数 M=N)** 框架对**每条实际执行动作**广播 `OnAIActionExecuted`,payload `ActionIndex` 按实际执行顺序自增 `0,1,…,M-1`。**此自增值域是框架拥有的 [Logic] 可测契约(非可选提示,见 payload 定性 L215)**——框架不消费它做 gating,但保证其正确生成;
  - ② **(含不可行 fixture:N 条中 K 条执行期不可行 → 执行数 M=N-K<N,与 AC-42「含过期动作列表」共享场景结构)** 被静默跳过的不可行动作**不广播**,`OnAIActionExecuted` 广播数 ≡ 实际执行数 M,`ActionIndex` 在执行序列上连续(跳过动作不占号)。此为**呈现正确性要求**(防 HUD 为未发生动作播空动画),独立于已删除的 gating;
  - ③ **框架推进不被任何呈现回调门控**——断言框架在 AI 决策执行完毕后即时 `EndTurn` 并移交下一玩家 `TurnStart`,其 TurnEnd/移交路径中**不存在任何 `WaitFor*`/`Delay`/`FTimerHandle` 等待调用,也不读取 `AIActionDisplayMode`**(该旋钮为 **HUD(16) 持有的纯呈现配置**,框架侧不可见、不分支——见 Tuning Knobs)。**"Animated vs Instant 框架行为一致"由"框架根本不读此旋钮"结构性保证,而非靠对比两次运行的移交时机**(后者在纯同步单帧模型下信息量为零、无法区分"无 gating"与"两模式等长 gating")。验证 = mock AI 同步返回 N 条动作的 headless fixture 断言即时移交、无等待调用;可选加静态检查/grep 守门断言框架模块不引用 `AIActionDisplayMode` 符号。
  - **(R6 RETREAT:R3-R5 的 gating 断言[`AIPlaybackGating` 相位/集合收敛/per-action 早释/生产安全阀/double-fire/`SimulateTimeout()`]随框架 gating 删除而全部移除;呈现层如何展示 AI 行动归 HUD16/VFX19,见 OQ-T-4③。本 AC 不再依赖任何 wall-clock 或可注入超时接口,纯同步可测。)** 呈现侧动画时长归 HUD16/VFX19(AC-44 呈现侧回链)。
- **AC-45** [Advisory](R3 新增,BLK-3 DecideAuctionBid sentinel 值域,**Alpha**)mock `DecideAuctionBid` 返回各值域:**负数/`INDEX_NONE`→视同放弃;`0`→非法视同放弃;`> 当前最高价 且 <= Cash 且 >= 最低加价`→合法出价;违反任一→视同放弃** + dev 诊断。验证 sentinel 约定(避免 int32 半发布在 Alpha 返工)。出价合法性**校验归拍卖12**;MVP 不触发,Alpha 拍卖12 GDD 落实并回链。

- **AC-46** [Logic](R3 propagate,移动 AC-15/OQ-T-Move-1 对端——SentToJail 落地抑制,见 CR-3.1/Edge Cases)`SentToJail` 落地抑制:GIVEN 移动回报 `OnPawnLanded(arrivalContext=SentToJail)`,WHEN `ResolvePhase` 处理,THEN spy 买地决策 hook(`DecideBuyProperty`)与收租结算入口**调用次数 ==0**(被传送入狱不触发任何落地结算分支);**对照组** `arrivalContext=DiceMove` 落无主地产时 `DecideBuyProperty` 调用次数 ==1,证明抑制是 `arrivalContext` 驱动而非恒不调用。**纯 fixture、headless 可跑;危害域 pre-Alpha gate(MVP 经典盘 Jail 不可购买,自定义盘引爆)。**
- **AC-47** [Logic](R3 propagate,移动 CR-4 程间原子性对端——程间非重入,见 CR-3.1)程间非重入:spy 在 `OnPawnLanded` 回调入口置 `bInLandedCallback=true`、出口置 `false`;GIVEN 第一程 `Advance` 触发 `OnPawnLanded`(spy 监听者在回调内同步尝试发起第二程 `Advance`/`TeleportTo`),WHEN 本系统编排多程位移,THEN 断言本系统发起第二程 `Advance` 时 `bInLandedCallback==false`(第二程在第一程落地回调完全同步返回之后),验证多程严格串行、无同步嵌套重入。**纯 fixture、headless 可跑。**
- **AC-48** [Logic](R-xreview 2026-06-03,CR-3.2 算租聚合契约确权——关闭 spine owner 真空)ResolvePhase 算租聚合:GIVEN 当前玩家落他人**有主**地产、需收租,WHEN `ResolvePhase` 编排算租,THEN 断言:① 调地产所有权(6) `BuildOwnershipSnapshot(playerId, tileIndex)` **恰 1 次**取归属快照;② 读建房(8) `house_count`(8 未实现时见 AC-49);③ 以「快照 + house_count + 当前程 dice_total(PULL,见 CR-3.1)」为参数调经济(5)算租入口**恰 1 次**,参数值与上游一致。验证聚合编排归本阶段(对齐 economy:104 / property-ownership:107 / systems-index 继承义务表「经回合2 ResolvePhase 聚合」)。**纯 fixture、headless 可跑。**
- **AC-49** [Logic](R-xreview 2026-06-03,CR-3.2 MVP house_count 缺省——消算租路径 MVP undefined)MVP house_count 缺省:GIVEN 建房(8)= Not Started(mock 无 house_count 供给方),WHEN `ResolvePhase` 聚合算租输入,THEN 注入 `house_count == 0`,经济收到的算租参数 `house_count == 0`(经济 F-2 据此落 `RentTable[0]` 或垄断 `RentTable[0]×monopoly_rent_multiplier` 无房分支)。验证 MVP 第一个收租循环不依赖未实现的建房(8)、house_count 有唯一合法缺省来源。**纯 fixture、headless 可跑;建房(8)落地后本 AC 改读真实 house_count(契约不变)。**

- **AC-50** [Logic](building R4 propagate,CR-3.3 mortgage-pre-read — 兑现 OQ-Build-1 空白:调 6.Mortgage 前须先读 8.GetHouseCount==0)GIVEN 一个 Property 地格其 `GetHouseCount(tile) > 0`(mock 建房8 返回 house_count=2),WHEN `ResolvePhase` 强制清算循环(CR-3.3)或任何决策点考虑抵押该格,THEN 断言:① `6.Mortgage(tile)` **未被调用**(spy 记录 Mortgage 调用次数 ==0);② 框架转入卖房腿(`8.ForcedSellNextBuilding` 被调用)。**对照组** GIVEN 同一格 `GetHouseCount(tile) == 0`,WHEN 清算循环,THEN `6.Mortgage(tile)` 被调用 ==1 次、`ForcedSellNextBuilding` 未被调用(证明前置读是 `house_count` 驱动而非恒跳抵押)。**Spy 断言:Mortgage 对任何 house_count>0 的 tile 调用次数恒==0。纯 fixture、headless 可跑。前提:mock 建房8 `GetHouseCount` 返回预设值、mock 所有权6 `Mortgage` 可 spy。**

- **AC-51** [Logic](building R4 propagate,CR-3.3 清算顺序 + 有界终止 — 止损优先 mortgage-empty-first)GIVEN 玩家 Cash=100 欠租 amount_due=300,持有:{一块空地(未抵押,MV=100,low MV)、一块有房地(house_count=1)},WHEN `ResolvePhase` 强制清算循环(CR-3.3),THEN 断言以下调用顺序:① `6.Mortgage` 先被调用,对象为**空地**(MV 最小可抵押地);② **当且仅当** `6.Mortgage` 执行后 Cash 仍 < amount_due 时,`8.ForcedSellNextBuilding` 才被调用;③ 任意时刻当 `Cash >= amount_due` 时循环立即停止(早停,后续资产不再强制处置)。**终止性断言:**  `8.GetPlayerBuildings` 返回空后、`6.GetOwner` 也无可抵押地 → 调 `economy.is_insolvent` ==1 次 → 循环终止、不无限重试。纯 fixture、headless 可跑。**前提:mock 经济5 `Cash` query 可返回预设值序列;mock 6/8 可 spy 调用顺序。**

- **AC-52** [Logic](building R4 propagate,CR-3.4 破产 NLV 全组合聚合 — 兑现 OQ-Build-2:economy 不调 8)GIVEN 玩家持有 2 格建筑(`GetPlayerBuildings` mock 返回 `[(tileA, 2), (tileB, 1)]`)及若干未抵押地,WHEN `ResolvePhase` Raising Funds 子相进入 `is_insolvent` 判断前,THEN 断言:① 调 `8.GetPlayerBuildings(player)` **恰 1 次**(全组合枚举);② `preaggregated_nlv` 计算正确(按 `house_count × floor(BuildingCost/2) + Σ MortgageValue(未抵押地)` 累加);③ 调 `economy.is_insolvent(player, amount_due, preaggregated_nlv)` **恰 1 次**,传入的 `preaggregated_nlv` 与上方计算值一致;④ **economy 模块内部未直接调用 `8.GetPlayerBuildings` 或 `8.GetHouseCount`**(zero economy→8 calls,spy 确认)。验证破产判据 NLV 聚合归本系统(CR-3.4)、非经济5 反向拉取、无 5→8 环。**纯 fixture、headless 可跑;mock 经济5 `is_insolvent` 接口可 spy。**

- **AC-53** [Logic](building R4 propagate,CR-3.5 建房通告 beat — 兑现 OQ-Build-6)GIVEN 当前回合持有者 `CurrentPlayerId==1` ∧ 建房(8)在其 `ResolvePhase` 或 `PostRollAction` 期间广播 `OnBuildingChanged(tile=tileA, newCount=2)`(**2 字段 owner 契约,无 actingPlayerId**),WHEN `ResolvePhase`/`PostRollAction` 阶段正在进行,THEN 断言:① 本系统向所有玩家可见的通告事件(暂命名 `OnBuildingAnnounced`)被广播 **==1 次**;② payload 至少含 `TileIndex`(或 tile 名称)、`NewHouseCount==2`、`ActingPlayerId==1`(**`ActingPlayerId` 取自当前回合上下文 `CurrentPlayerId`,非 `OnBuildingChanged` 事件字段——方案②**);③ 广播不修改任何 PlayerState 字段或地产/建筑状态(纯信息事件)。**对照组:** `OnBuildingChanged` 在 `TurnStart`/`TurnEnd` 期间触发(异常情形)→ 断言 `OnBuildingAnnounced` **不广播**(建房只在 ResolvePhase/PostRollAction 发生,此 guard 验证时机约束)。**呈现侧 AC(HUD 继承义务):HUD(16) GDD 须登记消费 `OnBuildingAnnounced` 的 AC,回链本 AC-53(producer propagation 债,待 HUD GDD 开工时履行)。纯 fixture、headless 可跑;mock OnBuildingChanged 注入可控触发时机。**

> **跨系统责任追踪(qa-lead,防责任真空):** AC-31 完整出狱流程、AC-34/36/37 的 BLOCKING 测试归下游(事件格7/存档21/HUD16/AI10),由 **OQ-T-1~4** 追踪回链(见 Open Questions),下游 GDD 审查时 qa-lead 核对回链承诺——避免重蹈 board-data 责任真空。

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-1 ⚠ BLOCKING-ADR** | 实现层:回合框架落在 UE 的 `AGameStateBase`/`APlayerState` 还是自定义 `UTurnManagerSubsystem`?PlayerState 实体用 UE `APlayerState` 还是自定义 `UObject`/struct? | **下游 #4/5/7/9 实现开始前**由 `/architecture-decision` 解决(类比 board-data OQ-6)。ADR 须裁定(R1 扩充清单):① **PlayerState 容器形态 + 受控写强度**(C++ 强封装=AC-35a [Logic] 硬门 / BP 约定=软约束,见 AC-35);② **回合状态机宿主 + 异步等待机制**——必须用 `ETurnPhase` 枚举字段 + delegate 推进(可序列化、读档重入),**禁用 Latent Action**(Save 时状态丢失,毁"精确阶段恢复",见 Edge Cases);③ **事件用 `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`**(8 下游需 1-对-多,非 BlueprintImplementableEvent,R1 推荐默认);④ **`APlayerState` 在线迁移陷阱评估**——APlayerState 有 PlayerController 耦合/服务器权威假设,AI 玩家无 PlayerController(手动 SpawnActor+PlayerArray 注入是非标准路径);CR-7/OQ-3"预留在线接口不展开"须明确:选 APlayerState 时"预留"是空 Replicated UPROPERTY 占位还是 day-1 写 GetLifetimeReplicatedProps,是否有不可逆耦合;⑤ AI 决策 hook(CR-8)的 UE 接口形态(BlueprintNativeEvent / Interface);⑥ 与 board-data OQ-6 的 DA 持有者协调。 **R2 新增决策因子(unreal C-1~5):** ⑦ **`GameStateSnapshot` 的 UE 类型定义**(推荐 `USTRUCT(BlueprintType)` 值语义只读;须聚合所有权6色组/建房8费用与均匀约束/拍卖12轮次等跨系统只读视图)——**AI(10) GDD 开工硬前提**;⑧ **tuning knob 开局锁定值的运行时序列化字段位置**(`DOUBLES_JAIL_THRESHOLD`/`MAX_TIEBREAK_ROUNDS`/`bEnableDoublesExtraTurn` 须存运行时 SaveGame 快照字段、以快照为准,**不**用 CDO/DataAsset 默认值读档恢复,防 CDO 被项目更新覆盖打架);⑨ **容器选 C++ 强封装时须防 Blueprintable 子类遮蔽漏洞**(`meta=(IsBlueprintBase=false)` 或选 UObject 而非 AActor,否则 private 字段可被 BP 子类变量遮蔽绕过——`SetPropertyValue` 反射路径不绕过 BlueprintReadOnly,非风险);⑩ **状态机宿主三路选型(非"任选一")**:推荐 **`AGameStateBase` 子类**(生命周期=对局 Level、`GetGameState<>()` 零耦合访问、单机不开 bReplicates;**R4 unreal API 误述修正:AGameStateBase 的 `UPROPERTY` 字段【不自动】进 SaveGame——UE `SaveGameToSlot` 只序列化 `USaveGame` 子类的 `SaveGame` 标记字段,存档21 须手动从 GameState 读字段写入 USaveGame、读档反向注入;勿误以为 `UPROPERTY(SaveGame)` 标记即自动解决存档**)vs `UWorldSubsystem`(序列化需手动桥接)vs `UGameInstanceSubsystem`(跨 Level 存活但过度);⑪ **事件 payload struct 须标 `USTRUCT(BlueprintType)`** + 列清单(否则 `BlueprintAssignable` 绑定编译报错);⑫ **`APlayerState` 明确排除为容器**:AI 玩家无 PlayerController,APlayerState 有 PlayerController 耦合/PlayerArray 权威/UniqueId 假设,手动 SpawnActor+注入是联网**不可逆耦合**——MVP 推荐 `USTRUCT(BlueprintType)` 或轻量 `UObject`,联网阶段(OQ-3)需要时再迁移(有成本但可控)。**另:AC-35a 条件门**(受控写 [Logic] 硬测)登记于此——ADR 选 C++ 强封装后由 qa-lead 补回 AC 列表(见 AC-35)。 **R3 新增决策因子 + 交付节奏(unreal R-1/R-2 + ai REC-1):** ⑭ **per-hook 字段充分性判据**(对照 CR-8 四 hook 各列所需字段清单,确认含事件格7 出狱卡持有、拍卖12 当前最高价/加价的注入方齐备,作 OQ-T-3③ 验证对照);⑮ **PlayerState 容器 = 轻量 `UObject`(引用语义)而非 `USTRUCT`(值语义)的判据**——三需求(可序列化/受控写硬门/BP 可读)+ "多系统读同一玩家本体"诉求下,UObject 引用语义匹配且支持 AC-35a 硬门;**注意与因子⑦ 区分:`GameStateSnapshot` 用 `USTRUCT` 值语义只读(快照要拷贝隔离)是对的,PlayerState 容器与 snapshot 是两个不同诉求、不要用同一答案**(unreal R-2);**⚠ GC 陷阱(R4 unreal):若 `GameStateSnapshot`(USTRUCT)内部持对 PlayerState(UObject)的引用,须用 `TObjectPtr<>`/`UPROPERTY()` 标记(USTRUCT 非 GC root,裸指针不受 GC 追踪、snapshot 生命周期内 PlayerState 可能被回收);推荐 snapshot 直接嵌入拷贝值(int32 Cash 等)而非持 UObject 引用,规格化"snapshot 内不得持 UObject 裸指针"**。 **交付节奏(P0/P1 分层,unreal R-1;R5 减负——防 ADR 过载到无法按时):** ADR 不必拆多份文档(因子间有交叉引用),但标关键路径:**P0(#4/5/7/9 开工硬门,必须先裁)= ①②③⑦⑪⑭**(R6 RETREAT:删除 ⑬⑯ 两个 B-1 gating 相关因子,P0 降至六项;**R7:⑬⑯ 已从因子清单物理移除,空缺编号不复用、不占位,历史见 changelog R6;现存因子 = ①–⑫ + ⑭⑮**);**P1(可 P0 后补、不阻塞 MVP 开工)= ④⑫(联网迁移,OQ-3 才兑现)/⑥(board-data 协调,P0 阶段须先声明兼容假设——R4 unreal)/⑧(SaveGame 字段,存档21 阶段;⚠ R4 unreal:⑧实为 P0 隐含前提——TurnManager 序列化层开工即需定字段位置,P0 做时须先定、文档可 P1 补)/⑨(子类遮蔽,仅 ①选 C++ 时相关)/⑮ 子判据**。 **⚠ ⑭ 循环阻塞风险(R4 unreal;R5 补占位最低完整性):** ⑭ per-hook 字段清单作"#4/5/7/9 开工硬门",但清单完整性依赖 AI10/经济5/事件格7 等尚未写完的下游 GDD;若清单因下游未定而卡住,#4/5/7/9 开工被阻塞——缓解:P0 阶段先列"已知字段 + 待下游回填占位",不因个别字段未定阻塞整体开工。**占位最低完整性定义(R5 unreal #5——防"清单实质为空却标 resolved"):** 四 hook 字段清单**须至少列出本 GDD + board-data 已确定的字段**(如 `Cash`/`JailTurnsServed`/`bIsInJail`/`CurrentTileIndex`/`ConsecutiveDoubles`/地产 `TileIndex`),且"待下游回填"项**须有具名占位符**(如 `[待经济5:建房费用]`、`[待事件格7:出狱卡持有]`),**非空白**;若某 hook 清单全是空白占位则 ⑭ 不得标 resolved(空白=未实际分析,放行 #4/5/7/9 会让 snapshot 字段真空)。 **ADR 关闭检查清单(R3,qa-lead REC-R3-3):** OQ-1 标 `resolved` 时,**若容器选 C++ 强封装 → 必须同批新增 AC-35a [Logic] 并更新 review log,否则 ADR 不得标 resolved**(把补回 AC-35a 锚到 ADR 关闭这一真实事件,防"永远 pending")。 |
| OQ-2 | `TokenColor`(EPlayerColor 8 色)与棋盘地产色组 `EColorGroup`(CR-5.1)如何区分?玩家棋子色不应与地产色组色混淆。 | art bible §6 / HUD(16) 阶段确认调色板;两枚举须语义分离(玩家色 vs 地产组色)。 |
| OQ-3 | 联网(25)回合权威与可重放:MVP 本地权威;Full Vision 服务器权威需回合状态机 + RNG 定序(F-4)/双点(F-3)可确定重放。**R6 RETREAT 后简化:框架推进不再依赖呈现层时序(无 gating),确定性重放天然纯净,无 gating↔服务器权威兼容问题需评估**。 | 联网(25)阶段;MVP 预留接口(P 动态、状态可序列化)不展开。 |
| OQ-4 | systems-index 系统#2 标零依赖,但本系统硬依赖骰子(3)。 | **已决策(确认依赖+修正索引)**:Phase 5 修正 systems-index 系统#2 depends-on 加 3。 |
| **OQ-T-1** 跨系统测试追踪 | 事件格(7) GDD 须含"双点出狱不进双点链"集成测试,回链本 GDD **AC-31**。 | 事件格(7) GDD;`/design-review` 审7时 qa-lead 核对回链。 |
| **OQ-T-2** 跨系统测试追踪 | 存档(21) GDD 须含"中途还原精确阶段+ConsecutiveDoubles"集成测试,回链本 GDD **AC-34**。 | 存档(21) GDD;审21时核对。 |
| **OQ-T-3** 跨系统测试追踪 | AI(10) GDD 须含:① "PostRollAction AI **同步决策**正常完成 + 回合顺畅移交"集成测试,回链本 GDD **AC-37b**;② **(R2)** "AI 内部 RNG 仅走骰子(3)可种子化接口、禁调引擎随机"的 code-review checklist 条目(CR-8 约束的下游落实,**BP-only 下为软约束、C++ 强封装可加静态扫描硬门——见 CR-8 确定性 bullet 强度标注**;为 OQ-3 重放预留);③ **(R2)** AI 决策消费 `GameStateSnapshot`(OQ-1 ADR ⑦定义)所需字段在 snapshot 中齐备的验证(以 OQ-1⑭ per-hook 字段清单为对照);④ **(R3,qa-lead REC-R3-2)** AI(10) GDD **须定义并记录 AC-37b 的 `N`(行动权移交帧数上限)具体值及依据,回填本 GDD AC-37b**(防"两边都以为对方定 N"真空);⑤ **(R5 qa F-9)** fixture 验证 `OnTurnStarted.bIsAI=false`(人类回合)时 AI 决策 hook **不被调用**(防 AI 在人类回合干预,payload `bIsAI` 路由分支的防护性 AC)。(R1:原 wall-clock 超时模型已废,改 CR-8 同步;dev 兜底详见 R2 Edge Cases。) | AI(10) GDD;审10时核对。骰子(3)/所有权(6)/建房(8)/**事件格(7,出狱卡持有)** 须为 snapshot 注入只读视图。 |
| **OQ-T-4** 跨系统测试追踪 | HUD(16) GDD 须含:① "OnPhaseChanged/OnTurnStarted 驱动 UI 高亮(≤100ms 起/≤500ms 完成)"集成测试,回链本 GDD **AC-36**;② **(R3 B-2)** "Pass'N Play handoff 高亮与前一回合 outro 不叠帧(前 outro 完成→才起 handoff 高亮)"可测下限测试;③ **(R3-R5 → R6 RETREAT 简化)** "Animated 模式 HUD 经 `OnAIActionExecuted(ActionIndex)` 非阻塞逐步播放 AI 行动(按 ActionIndex 排序),不阻塞框架推进"呈现侧测试,回链 **AC-44 呈现侧**;(R6:删除原 `OnAIActionPlaybackSegmentComplete` 集合收敛回调义务——框架不再 gating。)④ **(R5 qa F-8)消费 `OnTurnEnded`,须含两 fixture 变体:`bGrantsExtraTurn=true`→"同玩家继续"动画(不显示换手 UI)、`bGrantsExtraTurn=false`→"移交新玩家"动画,两分支各有 AC**(否则 payload 该字段无下游消费测试=死字段),回链本 GDD AC-5a/AC-7。 | HUD(16) GDD;审16时核对。 |
| **OQ-5**(R3,game-designer RECOMMENDED) | 破产即出局(终态不可逆)→ 4 人局早出局者可能干等 30+ 分钟(支柱②社交受众体验黑洞,"出局即出席"反模式)。MVP 不解决,但本系统(流程 owner)是否应预留 `OnPlayerBankrupt` 事件 payload 供 Alpha 做观察者/预测/对赢家下注?**论据修正(R4,game-designer——R3 原写"Alpha 改事件签名破坏接口稳定承诺"为误):** 新增 `OnPlayerBankrupt` multicast delegate 是**追加事件、非改写现有事件语义**,**不违反** L184「只增不改语义」承诺(该承诺明确允许"扩展新增")。真正代价是:Alpha 时下游(HUD16/俄罗斯轮盘14)已对接无此事件的接口,需补接新钩子=有限开发成本,**非接口破坏**。另:当前 CR-6 仅有最终胜者信号 `OnGameWon`(R-2to9 propagate:原 `OnLastPlayerStanding` 已删),无"谁出局了"的逐次破产事件——MVP 期 HUD 只能轮询 `bIsBankrupt` 变化做"XX已出局"提示(可行但不如事件干净)。是否 MVP 即补 `OnPlayerBankrupt`(低成本=一个 multicast)留作裁定。**(R5 F-R5-6,game-designer 重申建议升 MVP):** 4 人局 MVP 场景下首个破产者中途出局即"体验真空"(无出局仪式/旁观视角/交互),`OnPlayerBankrupt` 成本极低(一个 multicast),建议 MVP 即补"最小可行出局感",Alpha 再设计观察者/下注完整后置体验。**升 MVP 与否属 scope 裁定,留 producer/用户 sprint planning 决定**(本轮不单方扩 MVP scope)。 | Alpha 设计/MVP scope 裁定;MVP 记开口。建议 Alpha 俄罗斯轮盘(14)/交易(11) 设计时一并评估 post-elimination 参与。 |
| **OQ-6**(R3,game-designer RECOMMENDED) | `AIDifficulty`(None/Casual/Normal/Sharp)字段存在,但 MVP 无"三档难度产生玩家**可分辨行为差异**"的契约——可能三档在玩家眼里无区别,支柱④易上手的 Competence 阶梯失效。本系统拥有字段、不拥有决策(归 AI10)。 | AI(10) GDD;须钉"三档难度产生玩家可感知行为差异(不仅数值权重)"契约。本系统 Dependencies AI(10) 行登记此要求。 |
| **OQ-7**(R3,qa-lead RECOMMENDED) | 继承测试义务节(systems-index)强制力仍是程序性(依赖 qa-lead 在 /design-review 时主动核对),非结构性。建议:① 把"核对继承义务"升为 `/design-review` skill 硬步骤(grep 下游 GDD 的 AC 确认回链存在,否则 BLOCKING);② 把该表纳入 `/consistency-check` 双向校验(回链 AC 必须在目标 GDD 存在,防编号漂移失链)。 | 跨 skill 改进(非本 GDD);记此处供后续 skill-improve 采纳。 |
| **OQ-8**(R4,game-designer RECOMMENDED) | Player Fantasy 节(L39 lean 模式自标"未经 CD 复核 framing")存两处 MDA framing 问题:① "基础层"用 Fantasy 术语包装基础设施需求(玩家无"感到隐形正确"的幻想,实为"流畅"=Flow/Fellowship)——类型错位;② 缺 **Submission 维度**(轮流制核心松弛感"等到你了才需专注",与支柱④易上手轻松局对应),Fantasy 节只提 Fellowship。不影响规则实现,但 Fantasy→HUD 下游指导力弱(可能被误读为"无需特别设计"、削回合切换 juice 投入)。 | **(R5 F-R5-4)期限收紧:由"生产前"改为"HUD(16) GDD 开工前"**——Player Fantasy 节是 HUD/VFX/音频的设计指导文件,缺 Submission 维度(轮流制松弛感)会让 HUD 设计 AI 回合观察体验时无从知晓"等待 AI 时玩家处于放松注意力状态、AI 动画应节奏舒缓"这一取向(与 gating downtime F-R5-3/OQ-9 连锁)。由 `creative-director` 在 HUD GDD 开工前复核 framing(L39 警告已搁置四轮);非规则层缺陷,转 OQ 跟踪而非主会话单方改写 framing。 |
| **OQ-9**(R5,game-designer F-R5-3;**R6 RESOLVED by RETREAT**) | R5 提出:多 AI 局连续 `AIPlaybackGating` 制造 downtime(人类每轮前被动观看 3 AI 回合动画收敛,9-24s 纯等待),且支柱②"观察 AI"价值远低于观察人类对手——用②名义引入④代价却不充分兑现②。**R6 RETREAT 删除框架 gating 后此 downtime 消失**——框架不阻塞于 AI 呈现,AI 回合即时移交,HUD 自主决定非阻塞展示节奏。原拟缓解阀(每轮 AI gating 上限/按键跳过/`AnimatedSummary`)不再需要。**这正是 R6 retreat 战略裁定的设计依据之一**(game-designer charge#1:观察 AI 的 Fellowship 价值不足以支撑框架 gating 复杂度+downtime 代价)。 | **已解决(随 R6 RETREAT)**。HUD(16) 仍应设计舒缓的 AI 行动非阻塞呈现(见 OQ-8 Submission 维度)。 |
