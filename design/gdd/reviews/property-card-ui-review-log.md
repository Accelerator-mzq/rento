# 地产卡与卡牌 UI (#17) — Review Log

## Verdict 定案 — 2026-06-05 — ✅ APPROVED(R-2 finishing-class,用户 accept)
R-2 = NEEDS REVISION(finishing)→ 本档内 finishing(2 must-land+5 recommended)+ 3 跨档 must-land(OQ-PC-8/11/6)全闭合 + fresh-grep 核验 → 用户 accept,标 Approved❖。剩余皆非阻 followup:art-bible §6.2/§7.4(b) 垄断视觉 + §7.2 等宽字体 + §4.4 Utility 色组(asset-spec 前,art-director)、架构 ADR OQ-PC-4(门控 6AC + AC-42 headless 前提 + CR-10 叠加 owner)。未自行同会话标 Approved——经用户显式 accept 落定。

---

## Review — 2026-06-05 — Verdict: NEEDS REVISION (R-2 fresh-session,finishing-class 收敛轮)
Scope signal: M
Specialists: economy-designer / systems-designer / qa-lead / ue-umg-specialist / game-designer + creative-director(senior)。full 模式,fresh session(防 self-taint)。
Blocking(must-land): 3 | Recommended: 5 | Deflate/Defer: 若干
CD senior verdict: APPROVED-with-followups(finishing-class)。**主审裁定:NEEDS REVISION(finishing)**——比 CD 保守一档,理由:① 项目 CD 硬约束「跨档接缝 single-doc 修订须同 PR propagate+fresh-grep,登记 OQ≠闭合」,OQ-PC-8 对端真空未闭;② 存在本档内真 spec 编辑(F-1 guard 触公式本体、vacuous-spy 使 BLOCKING AC 失真)。两 verdict 工作量相同,差别仅「现在是否标 Approved」;选 NEEDS REVISION 强制 must-land 真落地+再核后才标 Approved。

### 主审独立 fresh-grep 核实(不接受 specialist claim 当结论 / 不接受文档自述)
- **✅ R-1 6 跨档 propagate 中 4 条对端已真闭合**(credit R-1):OQ-PC-9(building L265/L78/222/269 已列 #17+含义务)、OQ-PC-10(board-data L70/89/370 变长 TArray+L165/305/316 已列 #17)、systems-index depends-on 已订正 1,6,8、F-6/F-8 公式 economy-cash L185-196/201-202 逐字符同构。
- **🔴 OQ-PC-8 对端真空(头号 must-land)**:`GetUnmortgageCost` 在 economy-cash(L120 仅 Credit/Debit/GetCash)/property-ownership(接口清单无)/player-turn(零命中)均不存在;AC-42(R-1 blocker#1 修复)引用幻象契约 → **R-1「7 blocker 全闭合」对 blocker#1 不成立**。economy/qa/game-designer 三方独立从不同方向同根命中。
- **🔴 OQ-PC-11 单向真空**:economy-cash 未登记 depended-on-by #17(零命中)。
- **⚠ OQ-HUD-10**:hud.md L694 仍全开(非 provisional-RESOLVED);两侧保守开放=无当前虚假闭合,但 #17 CR-10 对 HUD F-4 依赖有 claim 待核。
- **✅ 正面**:F-1(a) 与 economy F-2 piecewise 逐分支真同构、垄断翻倍含 `house_count==0`、不骗玩家。

### 3 must-land
1. **OQ-PC-8🔴(跨档)**:economy-cash 加纯函数 `GetUnmortgageCostForDisplay(MV)→int32`(无副作用/F-6 单源/不破无环);同 PR fresh-grep 核实。AC-42 已升 `[Logic·门控 OQ-PC-8]` 标 BLOCKED 防 false-green。
2. **F-1 越界 guard(本档内,已改)**:F-1 加 G-0 第 0 分支(越界走 EC-2 不 clamp、矛盾态强制 -1 含无主×垄断);AC-11g 补 house_count=6/-1 越界例。
3. **vacuous-spy(本档内,已改)**:AC-1 删末段「写接口==0」;AC-16 拆 16[Logic 正向]/16b[Advisory·code-review 负向无写引用];AC-24 改「断言绑定值输出不变」。

### 5 Recommended(本档内已改 / 跨档待落)
- EC-6+AC-32 补「无主×垄断 badge」fail-safe(已改);AC-25 措辞「叠加逻辑更新」→「cached 状态变量」+ 门控 6AC 补「ADR 前不进 CI gate」(已改);tnum/V-Own-04 负面命题→保守结论待 Pre-Production 源码核验(已改);信息架构 L319 消歧「赎回价数字必显 vs 费率算式可次级」(已改)。
- OQ-PC-11🔴(跨档,待落):economy-cash 补 depended-on-by #17。OQ-PC-6⚠(跨档,待落):fresh-grep hud F-4 + 回标 OQ-HUD-10。

### Deflate / Defer
- **Deflate**:侦察分层(game-designer 重提,R-1 已 CD 裁定「分层可接受」,本档 L22 已 codify;收敛判据「已裁定项重提须 deflate」)。
- **Defer**:卡片操作成功/失败反馈→`/ux-design`(UX Flag 已挂);CR-10 候选②/MVVM spy→OQ-PC-4 架构 ADR。

### 收敛判定
finishing-class 收敛轮:新发现全是「完成已定裁决传播 / 扫二阶残留」,无「重裁系统是什么」;severity 单调降(R-1 含赎回撒谎/索引错/越权本体级 → R-2 收窄到接口形态+公式散文+vacuous 复发);零架构返工。本质=R-1 prose-fix 没扫 owner 端接口存在性(point-fix-didn't-sweep-the-tail)。完成 3 must-land + 5 recommended 后达 Approved 水线。AC 标号:新增 AC-11g、AC-16b;AC-42 升 OQ-PC-8 门控。

### R-2 跨档 propagate 落地(2026-06-05 同 PR,用户授权,fresh-grep 核验)
- **OQ-PC-8 ✅**:economy-cash 新增纯函数 `GetUnmortgageCostForDisplay(MV)→int32`(CR-5 L65 + 接口稳定承诺 L122 + UI 节 L380,无副作用/F-6 单源/不破 5↔6 无环)。AC-42 改读此接口。fresh-grep 核验落地。
- **OQ-PC-11 ✅**:economy-cash Interactions(L112)+ 下游表(L330)登记 depended-on-by #17 + F-1 同构维护债。fresh-grep 核验。
- **OQ-PC-6 🔶**:fresh-grep 核 hud.md F-4 L190 **确实依赖 #17 CR-10 常驻通道**判 AI 抵押非 critical(#17 claim 属真);hud.md L694 OQ-HUD-10 回标 **provisional-RESOLVED**(归属由 #17 兑现,实现通道随同一架构 ADR;ADR 前 F-4 非-critical 裁定不视硬闭合)。剩 art-bible §6.2/§7.4(b) 垄断视觉待 art-director propagate(非阻)。
- **结论**:3 must-land 全闭合(2 跨档接口 + 1 回标),#17 达 **Approved 水线**。剩余非阻 followup:art-bible 垄断视觉/等宽字体/Utility 色组(asset-spec 前)、架构 ADR OQ-PC-4(门控 AC headless 前提)。待 fresh R-3 确认或用户 accept 后标 Approved。

Prior verdict resolved: R-1(NEEDS REVISION,7 blocker)— 5/7 真闭合;blocker#1(赎回价)经 R-2 fresh-grep 查实未真闭合(owner 接口真空)→ R-2 已 propagate 补 owner 接口闭合。

---

## Review — 2026-06-05 — Verdict: NEEDS REVISION (→ R-1 就地修订完成,待 fresh-session 再审)
Scope signal: M
Specialists: economy-designer / systems-designer / game-designer / ux-designer / qa-lead / ue-umg-specialist + creative-director(senior)。full 模式,首审(无 prior log)。
Blocking items: 7 | Recommended: 8 | Nice-to-have/CONCERN/defer: 若干

### 主审独立核实(不接受 specialist claim 当结论)
- **[已核实 真]** 赎回价撒谎:economy-cash F-6(L185-196)`unmortgage_cost = MortgageValue + ceil(MV×1/10)`(MV=100→110);#17 原 CR-3/V-Own-01 只显 MortgageValue,低估赎回成本 10%,违背 Player Fantasy「押多少…一眼看穿、无需心算」。economy-designer + game-designer 独立同根命中。
- **[已核实 真]** 卖回额+门控原因+建后租金预览:building-upgrade L265-266(已 APPROVED 上游)强制地产卡显示 卖回额(floor(BuildingCost/2))+ CanBuildHouse 门控原因 tooltip(非垄断/组内有抵押/非最低档/现金不足)+ 建后租金预览;#17 原档全缺。
- **[已核实 正面]** F-1(a)/(b)/(c) 生效行推导**确与** economy-5 property_rent/railroad_rent/utility_rent piecewise 分支同构(核心正确性风险 sound);OQ-Prop-5(垄断×抵押)单源依赖 #6.IsMonopoly 设计稳健,无论翻转与否无返工。

### 7 BLOCKING(R-1 全闭合)
1. 赎回价显示撒谎 → CR-3⑤ 抵押区块拆「套现额+赎回价」;CR-8 来源;V-Own-01;EC-13;AC-42;propagate OQ-PC-8(economy5/回合2 暴露 GetUnmortgageCost)。
2. 卖回额+门控原因 tooltip+建后租金预览(上游 L265-266 义务漏接)→ CR-3④;CR-7 tooltip;V-Own-01 建房区块+tooltip 行;EC-14;AC-43/44;回标 OQ-PC-9。
3. EC-6 矛盾态主动误导(owner==NONE∧house>0 高亮不存在收租档)→ EC-6 改 fail-safe(强制 highlight_row=-1+占位);AC-32 钉死输出 -1 非 3;F-1 加 EC-6 override 注。
4. FBoardTileData.RentTable 基数多态未指定(Property 6 vs Railroad 4)→ Interactions 表加消费侧基数契约+越界禁访;propagate OQ-PC-10 回 #1。
5. CR-10 棋盘叠加 widget owner 空白(比 OQ-HUD-2 更底层,AC-25/26 前提)→ CR-10 钉候选架构方向+常驻 owner;OQ-HUD-10 订正为 provisional-RESOLVED(实现通道待 ADR,防虚假闭合);OQ-PC-4(iv)。
6. AC-34 BP 双绑测试结构性 false-green(C++ spy 看不见 BP graph 绑定,-nullrhi BP 图不跑)→ 拆 AC-34a[Integration·门控·C++ delegate 计数] / AC-34b[Advisory·code-review·BP graph,诚实标 headless 不可覆盖]。
7. AC-11 同构守卫靠注释行号(drift)+ 5 门控 AC headless 前提不全 → AC-11 纯函数自洽[Logic]+同构性降[Advisory·cross-system]+propagate OQ-PC-11;AC-30 补门控;OQ-PC-4 补 spy 输出/刷新同步性/实例化协议三前提。

### Recommended(R-1 顺手 in-doc 修)
F-1 clamp 越界语义(走 EC-2 非静默 clamp)、AC-14 三路哨兵、状态机补 4 行(SM-1/2 Open 态非法 OpenCard)、不变式③去同义反复、vacuous spy AC-3(状态机 state 替幽灵方法名)/AC-5/AC-39(降 code-review)、tnum OQ 关闭(V-Own-04 定论:UE5.7 FSlateFontInfo 不暴露 per-run feature,用等宽字形字体资产)、CA-04 非生效行字重前提、Player Fantasy 侦察分层微调。

### 裁为 CONCERN / defer(分歧裁定)
- **10px 字号下限**(ux 判 BLOCKING a11y)→ CD 裁:**跨档 art-bible/a11y CONCERN,非 #17 单档 blocker**——理由:HUD #16 刚带同样 10px §7.2 被 APPROVED,单档拦 #17 制造不一致裁决。登记 OQ-PC-12,Pre-Production /ux-design 统一裁 text-scale。用户采纳。
- **CR-10 掏空侦察 fantasy**(game-designer)→ CD 裁:可接受分层(概览扫描 vs 决策深读),非缺陷;Player Fantasy 措辞微调,不动设计。
- gamepad nav / reduce_motion 抑制清单 / 卡片 anchor / EC-2 占位 partial-vs-total → /ux-design(OQ-PC-7/OQ-HUD-9)。
- VFX19 juice 兜底、机会/命运翻牌 Alpha、hit-test 归属 → defer(OQ-PC-1,enable-not-own 框定正确)。

### AC 变更
41 → 45(29 Logic〔6 门控 OQ-HUD-2:AC-21/22/23/25/26/30〕+ 2 Integration〔AC-33 provisional / AC-34a〕+ 14 Advisory)。新增 AC-42/43/44、AC-34a/b 拆分;AC-5/39 降 code-review;AC-11 同构性降 Advisory+propagate。

### fresh-session 再审重点(R-2)
1. **AC-42/43/44 是否真兑现上游义务**:对照 building-upgrade L265-266 + economy F-6/F-8 逐条核验,非仅措辞;赎回价/卖回额是否真"读 owner 值不自算"。
2. **4 条 producer 跨档 propagate(OQ-PC-8/9/10/11)** 是否需 producer 先走再 Approve:economy5 GetUnmortgageCost 接口是否存在/能否暴露;building-upgrade depended-on-by #17 回标;#1 RentTable 基数确认;economy-cash 同构维护债登记。**按 CD 流程约束:single-doc 改跨档接缝须同 PR 开 propagate 工单 + fresh-grep 核实对端,不接受登记 OQ 当闭合。**
3. **CR-10 叠加 owner 是否仍是真空**:provisional-RESOLVED 是否足够 honest,还是仍构成 OQ-HUD-10 虚假闭合风险(HUD F-4 据此判 AI 抵押可跳过)。
4. 门控 6 AC 标签 + AC-34b 诚实标注是否到位;vacuous spy 是否仍有残留(AC-1/24 模糊约束未修,留 R-2 看是否升级)。
5. 收敛判据:R-1 新发现若是正交新维度+blocker 质量下降=finishing-class approve-with-followups;若再揪「系统该不该这样」级=非收敛。

Prior verdict resolved: First review(R-1 为首审 + 就地修订)
