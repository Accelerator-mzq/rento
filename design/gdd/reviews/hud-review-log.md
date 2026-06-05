# HUD（#16）Design Review Log

> 本文件记录 `design/gdd/hud.md` 的 design-review 修订历史,供 fresh-session 再审追踪。

---

## Review — 2026-06-05 — Verdict: NEEDS REVISION → 就地修订完成,待 fresh re-review
Scope signal: M（呈现层叶子,5 公式,3 硬依赖+多软依赖,无新架构;blocker 均无返工）
Depth: full（5 specialist + creative-director synthesis）
Specialists: game-designer / systems-designer / qa-lead / ux-designer / ue-umg-specialist / creative-director
Blocking items: 2 真 in-doc blocker + AC 诚实化 + 措辞 = 4 类 | Recommended: 全部廉价推荐项已并入同一修订
Prior verdict resolved: 首评（无 prior log）

### ⚠ 独立性声明
本 GDD 由 `/design-system 16` 在**同一会话**撰写,本次 design-review 亦在同会话发起。Phase 3b 五专家为**独立 subagent（fresh context）**,其对抗发现不受撰写史污染——verdict 主要由独立专家驱动。但主会话结构分析继承撰写上下文;R-1 修订亦同会话执行,**自证风险**存在 → 已建议 fresh session 复审。

### 关键发现（5 专家收敛）
- **B-1【CRITICAL,systems-designer + game-designer 收敛】** F-4 默认值在正常无延迟路径破损:T_base=1.0/T_stale=2.0,M=3 队列总时长 ≈2.52s > T_stale → 第3条 AI 动作正常路径被误判 expired 跳过(GDD 自己 Tuning L292 已诊断却发布破损默认值)。叠加 CR-11 非阻塞可丢弃 → 系统性击穿支柱②"对手在跟我斗",且无 AC 守"玩家看到 AI 关键动作"。
- **B-2【HIGH,systems-designer】** EC-4"丢弃 outro 余帧直起新高亮" = 变相叠帧,与 CR-8"不叠帧"矛盾;AC-24 在 fallback 路径断言对象("P1 outro 完成时刻")未定义不可求值。
- **AC 诚实化【qa-lead,ue-umg 收敛】** AC-12/24/36 渲染层时刻标 [Logic]/[Integration] BLOCKING 但 `-nullrhi` 无探针+帧抖动 flaky;AC-47 code-review 标 [Logic] 名实不符;AC-29/40/26 负向断言无 spy 则 vacuous-pass → 42 BLOCKING 中 4 条"纸面通过实际未测"。
- **H-1【game-designer + ux-designer 收敛】** 支柱②社交在 MVP Pass'N Play 单屏名实不符("全员可见"实=仅持屏者);Player Fantasy 措辞未承认 MVP 范围。
- **架构期输入【ue-umg-specialist】** F-1/F-2 动态时长与 UWidgetAnimation 不兼容(须 NativeTick)、Invalidation Box vs onset、delegate Dynamic 合约、Tabular Figures 须等宽字形字体(UE 无 tnum API)——多属 ADR 范畴,GDD 仅须点出"公式隐藏的实现约束"。

### CD 综合裁定
接近 Approved 的高质量 lean 首评;未 Approved 唯一原因=2 真 in-doc blocker(B-1/B-2)+ 同 PR 可改的 AC 诚实化与措辞。**无架构返工、无 pillar 重裁定、无签名变更、无身份级问题**——全是收口。Verdict: **NEEDS REVISION**。

### R-1 就地修订（同会话,本 PR 全部应用）
1. **B-1**:expired 主判据改"框架推进新状态(state_gen)";T_stale 退软上界(默认 4.0s/范围 3.0~6.0,游戏时间防暂停/读档爆炸);新增 **AC-56** critical 动作 expired 不静默;**AC-30 重写**(三判据+回归守护);Tuning T_stale 行+交互注更新;F-4 加 M=0 跳过。
2. **B-2**:定义"被迫切换"触发(新权威状态到达=读档/框架快进);CR-8 加唯一例外条款;EC-4 拆正常/被迫;**AC-24→AC-24a(正常 Logic)+AC-24b(被迫 Logic)**。
3. **AC 诚实化**:AC-12→AC-12(逻辑 Logic)+AC-12b(渲染 Adv);AC-36→AC-36(公式)+AC-36b(实录 Adv);AC-4 改 Logic;AC-47 降 Advisory·code-review;AC-26/29/40 补 DI spy;AC 节注释修正过度声称。
4. **H-1**:Player Fantasy 第3点 + CR-12 加 MVP 单屏范围限定;net_worth OQ-HUD-8 重分类为信息架构决策。
5. **廉价推荐项**:F-2 不变式;F-1/F-2 非 WidgetAnimation 指针;F-3 D_highlight 下界说明+AC-55;状态机(Active 互斥/Playing 自转换/Bankrupt+GameOver);F-5 驱逐口径笔误+k 槽规则;CR-1 布局 AC-54;AC-53→AC-27a;CR-3 等宽字体硬依赖指针;AI 行动区不挤占操作栏;缺失可读性归属;OQ-HUD-9/10/11;Status header。
6. **AC 计数**:53 → 59(42 Logic + 2 Integration + 15 Advisory)。

### propagate followups（producer 协调,不阻本档）
- art-bible §7.2 须指定具体等宽数字字体资产(给 art-director;UE 无 tnum API)
- #17 地产卡 UI 接收垄断组/抵押状态归属确认
- UMG ADR 输入(WidgetAnimation/Invalidation Box/delegate 合约/CommonUI)挂 OQ-HUD-2/7,架构期

### Re-review 重点核验（fresh session）
1. F-4 默认值确实落入 `T_stale ≥ 队列最大总时长` 安全区(逐项代入算术,不接受注释自述);expired 主判据=框架推进而非纯壁钟。
2. AC-24a/24b 两路径断言均可求值、均 headless。
3. AC-4/12/12b/36/36b/47 标签与可测性一致(渲染时刻全 Advisory,BLOCKING 均可 headless)。
4. Player Fantasy/CR-12 支柱②措辞已限定 MVP 范围。
5. AC-56 critical 动作守护逻辑可测。

---

## Review — 2026-06-05 — Verdict: NEEDS REVISION → 就地修订完成（含跨档 propagate),待 fresh re-review
Scope signal: M（呈现层叶子;3 blocker 均收口/finishing 类,无架构返工、无 pillar/身份重裁定）
Depth: full（5 specialist 独立 fresh subagent + creative-director synthesis）
Specialists: systems-designer / qa-lead / game-designer / ux-designer / ue-umg-specialist / creative-director
Blocking items: 3 | Recommended: ~12（全部同 PR 落）
Prior verdict resolved: **Yes** — R-1（2026-06-05 同 authoring session,自taint)的 B-1/B-2/AC 诚实化经独立验证确认真闭合;CR-11 gating 模型与 player-turn R6 RETREAT 一致(主会话误读 R4/R5 历史日志的假警报已查实排除)。

### R-2 独立验证亮点（fresh session,对照权威源核验,非接受 claim）
- **CR-11 gating 假警报排除**:grep 命中 player-turn L25/31/33 framework-gating 描述初判与 HUD CR-11 矛盾;读上下文确认那是**被 R6 RETREAT 取代的 R4/R5 历史日志**,现行 player-turn(L36-39/184/187)与 HUD CR-11 一致——无矛盾。
- **2 blocker 对照 player-turn 权威契约确诊**:① `state_gen` 在 player-turn 全文不存在(框架不广播此计数,L187"非框架契约");② `FAIActionDetails`(L263)无 `is_critical`/`EActionType`,仅 `{ActionIndex,ActingPlayerId,TargetTileIndex,Amount}` + 开放扩展槽。

### 3 Blocking（CD 综合裁定,均 finishing 类）
- **B-R2-1【state_gen 幻象框架契约】** F-4 主判据 `framework_advanced_past=current_state_gen>action_state_gen`(L178/196)引用框架从不广播的计数;per-action 解读会重现 B-1 击穿。**修复(in-doc):改 HUD 可观察回合代次 `E_cur`**(每 `OnTurnStarted` +1,同回合 M 条共享 `E_action`),不依赖框架契约。
- **B-R2-2【AC-56 payload 无 criticality】** `FAIActionDetails` 无 `is_critical`,HUD 纯叶子无法臆断→AC-56 不可实现/测。**修复(跨档 propagate,用户选 EActionType):player-turn `FAIActionDetails` 扩 `EActionType`**(同 PR 已落,OQ-HUD-12 RESOLVED;附带解 V-Own-07 动作类型显示缺口)。
- **B-R2-3【reduce-motion 自述 MVP 硬要求却无 AC】** 永真陷阱。**修复(in-doc,用户选 commit):新增 AC-57 [Logic] + `reduce_motion` 旋钮**;OQ-HUD-9 降为参数细化。

### 分歧裁定（CD）
- **net_worth(OQ-HUD-8):game-designer/ux-designer 主张"现在拍板",CD+doc 主张 defer**——CD 裁定 **defer 正确**(悬挂布局决策非矛盾,playtest 才是正确裁判);加预承诺 MVP fallback(仅持有地产数)防 Pre-Production stall。
- CD deflate 4 单专家 BLOCKING→Recommended:AC-36 const(→AC-36c Advisory)、Pass'N Play banner 时序(→OQ-HUD-13)、handoff 输入锁定(→OQ-HUD-9)、FSM-as-UObject(→OQ-HUD-2 必产 ADR 输出)。

### R-2 就地修订（本 PR 全部应用,AC 59→61）
1. **B-R2-1**:F-4 全面改 `E_cur`/`E_action`(prose/var-table/examples/States/EC-4/5/AC-24a/24b/29/30/Tuning);AC-30 补读档重置变体;Playing→Idle 边界澄清。
2. **B-R2-2**:CR-10/Interactions/Dependencies/V-Own-07/AC-56 threaded `EActionType`(AC-56 改 spy `ShowCriticalFinalState`);**player-turn `FAIActionDetails` propagate 已落**(L263 + 修订日志条目,fresh-grep 核验)。
3. **B-R2-3**:AC-57 [Logic] + `reduce_motion` 旋钮 + OQ-HUD-9 重写 + covergap#6。
4. **folded**:Ease-Out Back→Ease-Out Cubic(F-3/V-Own-02,去超冲对齐"不跳动");T_stale floor 3.0→3.5;Active 互斥状态行;GameOver 面板终态;AC-36 拆 36c;AC-4/55 双边界;AC-26/43/48 spy 精化;CR-5 并发优先级;V-Own-08 限定语;OQ-HUD-2 强化(FSM-as-UObject+IGameClock+MVVM caveat);OQ-HUD-13 新增。

### Re-review 重点核验（fresh session R-3）
1. F-4 `E_cur` 模型:同回合 M 条共享 `E_action` 确不误判;主判据全程不引用任何框架广播计数(grep `state_gen` 仅余解释性提及)。
2. `EActionType` propagate 双向落地:player-turn L263 含字段 + HUD AC-56/V-Own-07/CR-10 引用一致;AC-44 未受影响。
3. AC-57 reduce-motion 逻辑可测(开关存在+On 去超冲 headless 可断言);`reduce_motion` 旋钮登记。
4. T_stale floor 3.5 ≥ M=5 队列 3.4s;AC-30 四变体算术。
5. OQ-HUD-2 三项 ADR 输出(FSM-as-UObject/IGameClock/MVVM caveat)登记为 BLOCKING AC headless 前提。

---

## Review — 2026-06-05 — Verdict: NEEDS REVISION〔finishing-class,收敛〕→ 就地修订完成 = APPROVED
Scope signal: M（呈现层叶子,5 公式,3 硬依赖;全部 finishing 类,无架构返工/无 pillar 或身份重裁定/无跨档 propagate）
Depth: full（5 specialist 独立 fresh subagent + creative-director synthesis）
Specialists: systems-designer / qa-lead / game-designer / ux-designer / ue-umg-specialist / creative-director
Blocking items: 5 must-land（in-doc,均 finishing） | Recommended: 7（同 PR 全落）
Prior verdict resolved: **Yes** — R-2 的 3 blocker 经主审独立 fresh-grep 对照权威源确认真闭合（`EActionType` 真落 player-turn **L264** 权威 struct 非仅 propagate 日志;`state_gen` 已不作操作性判据;`E_cur` 额外回合安全由 player-turn **L197**「整链一次不重发」背书）。

### R-3 独立验证亮点（不接受 specialist claim,对照权威源核验）
- **`EActionType` 真闭合**:读 player-turn L264 authoritative `FAIActionDetails` struct 确含 `EActionType`(枚举 BuyProperty/BuildHouse/Mortgage/Unmortgage/Move/PayRent/Bankrupt…),非幻象。AC-56 依赖真兑现。
- **`E_cur` 额外回合安全**:player-turn L197「双点额外回合链 OnTurnStarted 整链只发一次、不重发」→ 同玩家额外回合链 `E_cur` 不递增、共享 `E_action`、不误判过期。模型证明安全;但 hud.md 原未引此依赖 → R-3 补 F-4 引证 + 漂移守护。
- **deflate 两专家过度声称**:① qa-lead「AC-29 vacuous(监控已删回调,永不 FAIL)」**过度声称**——AC-29 spy 文本明写「监控对框架方向的**任何**推进/回放完成调用,非仅单一命名回调」,覆盖真实 AdvanceTurn/EndTurn(AC-26 黑名单),FAIL-able seam 存在;② qa-lead「AC-56 ShowCriticalFinalState 幻象契约」**过度声称**——它是 HUD 自拥呈现接口(HUD 是呈现叶子),合法可定义 DI seam,非 state_gen 式上游幻象(systems-designer 判断正确)。两项降为 Recommended 措辞精化。
- **deflate re-litigation**:game-designer「Player Fantasy 第3情感时刻 MVP 过度承诺」= R-1 已裁定 H-1(L25 限定 + CR-12 + OQ-HUD-13),至多 1 句措辞收紧,非新 blocker。

### 5 Blocking（CD 综合裁定,均 finishing 收口类）
- **B-R3-1【AC-57 永真谓词 + 不可达】**(qa+ux 收敛) ②「scale 不越 1.05」永真(Cubic 本无超冲,谓词不测任何东西);①「counter ≤200ms」在 T_min=0.3s 默认下永不可 PASS(无旁路机制)。**修复:②改「缓动曲线无超冲采样」、①补 `reduce_motion=On` 旁路 F-1 `T_min`、加⑤操作栏切换;Tuning row + covergap#6 同步。**
- **B-R3-2【V-Own-10/AC-21 未传播去超冲】**(qa+gd 收敛,CD 核实 Bounce 在 spec L390 + AC L534 双处) R-2 去超冲(Back→Cubic)只扫了面板高亮,操作栏阶段切换仍 Ease-Out Bounce(弹性超冲),与「不跳动」Submission 矛盾。**修复:V-Own-10 + AC-21 → Cubic,纳入 AC-57⑤。**
- **B-R3-3【ADR 门控藏在 covergap】**(qa) AC-12/24a/30 的 OQ-HUD-2 headless 前提只在覆盖缺口#8,AC 条目本身无标 → 实现期 false-green。**修复:四条(含 AC-57)标 `[Logic·门控 OQ-HUD-2]` + 汇总表分组。**
- **B-R3-4【V-Own-07 内部矛盾】**(ux,主审核实) V-Own-07「底部操作栏或 HUD 专属区」与 UI-Req「AI 行动区不挤占操作栏」MVP 硬约束矛盾。**修复:删「底部操作栏或」,统一「HUD 专属区(非操作栏)」。**
- **B-R3-5【BP 绑定双触发 footgun】**(ue-umg) BP `Bind Event` 非 `AddUnique`,重绑不显式解绑会重复注册;EC-11/AC-48 未区分 BP/C++ 路径,spy 可能漏掉 BP 双绑。**修复:EC-11 + AC-48 补 BP/C++ 区分 + BP 双绑测试覆盖。**

### Recommended（7,同 PR 全落)
F-4 T_stale 最坏旋钮角约束(T_base=1.5/K_compress=0.05/M=5 队列 6.5s>6.0s 上界,补 `T_stale ≥ Σ T_slot` 联动)/ E_cur 额外回合引 player-turn L197 + 漂移守护 / critical 集合 Mortgage/Unmortgage **exclude-with-justification**(状态持久显示棋盘/#17,CD 裁定)/ CR-3 tnum 5.7 核验标注(原作确定事实 propagate art-bible)/ F-2 PlaybackSpeed 排除理由精化(无整数文本轨)/ **新增 AC-58**(FAIActionDetails BP 引脚静默断线 Adv·code-review)/ OQ-HUD-8 net_worth fallback 安全前提。

### CD 综合裁定
**finishing 末圈,健康收敛,非无限剥面。** 三判据:零架构返工 / 零 pillar 或身份重裁定 / 新发现是 R-2 修订的可预测二阶残余(去超冲漏扫兄弟、ADR 门控漏标条目、AC-57 谓词未验)。严重度严格单调下降(R-1 核循环破损 → R-2 跨档幻象契约 → R-3 谓词/未传播兄弟/标签)。5 项与 R-1/R-2 自身视为 blocking 的同类(AC 诚实/vacuous/mislabel)→ 按本档自身标准应同 PR 修而非降 followup;且全部廉价(1 个单 token 删除、3 个一行、AC-57 谓词重写)。**Verdict: 就地修 5 项 + 7 Recommended → APPROVED,无需 R-4 fresh 重审**(下一轮预期仅 diff 确认,非第 6 个 owned-surface blocker)。AC 61→62。

### propagate followups（producer 协调,不阻本档 APPROVED)
- art-bible §7.2 等宽数字字体资产指定 + §7.5 高亮/操作栏曲线选 Ease-Out Cubic(给 art-director;tnum 须 5.7 核验)
- #17 地产卡 UI 接收抵押/垄断组状态归属(OQ-HUD-10)
- OQ-HUD-2/3 UMG ADR:状态机抽独立 UObject + IGameClock DI + RNG 强封装——**为 AC-12/24a/30/57 headless 硬前提**,架构期必产出
- OQ-HUD-9/11/13 留 Pre-Production `/ux-design`(reduce-motion 降幅参数 / 跨系统并发仲裁 / Pass'N Play 建房时序补偿)
