# Review Log — 玩家与回合管理 (player-turn.md)

> 修订历史,供 fresh re-review 追踪。

## Review — 2026-06-01 — Verdict: NEEDS REVISION (design-review full R1)
Scope signal: L (multi-system Foundation, 4 formulas, ~10 deps, drives OQ-1 ADR)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis, 独立复核 F-1/F-2/F-3)
Blocking items: 5 | Recommended: 7
Prior verdict resolved: First review (GDD 此前为 lean 模式单会话设计,本轮为首次 full 对抗复审)

### 终审裁定(creative-director)
机制本体扎实、ownership 边界清晰的 Foundation 框架 GDD。与 board-data 上轮"全部 blocker 皆元数据滞后"不同——本轮混入 **1 个真 GDD-body 规格矛盾(F-1)+ 2 个真设计裁定缺口(AI 同步模型、阈值锁定)**,不可带病放行,故非 APPROVED;但无架构返工、无新机制簇、无签名颠覆,退出 MAJOR 区间 → NEEDS REVISION。

### 5 项 BLOCKING(本轮已修订 R1)
1. **F-1 哨兵-枚举矛盾**(真 body 缺陷,CD 独立复核成立):Output Range 称"|A|≤1 返 INDEX_NONE",但枚举在"唯一存活者≠cur"时返有效 index。→ 守卫先行:|A|≤1 在枚举之前返 INDEX_NONE,|A|≥2 才枚举。补 AC-17b 覆盖缺陷分支。
2. **AI 决策模型断裂**(CD 判头号价值,项目#1风险命门):wall-clock 超时是回合制错误失败模型 + ResolvePhase 无买地 hook → AI 永远无法买地。→ CR-8 同步决策契约 + ResolvePhase/JailTurn hook + dev 看门狗取代 wall-clock 超时。补 AC-37a/38。
3. **F-3 阈值热交换炸弹**(CD 独立复核成立):存 ConsecutiveDoubles=3/阈3,读档改阈2 → 3>=2 即入狱。→ DOUBLES_JAIL_THRESHOLD 开局锁定、局中不可改、随存档序列化。
4. **AC-35 受控写 over-claim**:"防止任意系统乱写"是 BP 兜不住的工程级措辞。→ 降级为架构契约 + 强度推 OQ-1 ADR 分叉(C++强封装=AC-35a [Logic]硬门 / BP约定=[Advisory]软约束)。
5. **F-4 round 作用域未定义**:全局 vs 子组独立计数歧义。→ 定义为全局单次计数,所有平手子组同轮同步重掷。

### 关键画像
真 body 缺陷仅 F-1 一处(局部"拆两种语义"手术);其余 blocker 为决策接口缺口(AI/阈值)与 spec 补全。机制地基稳、数据↔规则委派干净、接口稳定承诺到位。

### 争议裁决(CD)
- AITurnTimeout:**采纳 ai-programmer——删除改同步 + dev 看门狗**(非保留重规格化)。
- AC-35:两者可调和,分歧源于 over-claim——降级措辞 + 强度推 ADR 分叉。
- CR-2/CR-4 支柱冲突(①忠实 vs ④易上手):**二者皆正确忠实克隆选择,非 BLOCKING**;忠实为默认 + House Rules 开关 + 呈现层缓解。game-designer 的 4 BLOCKING 降为 1 RECOMMENDED + 1 hook 登记。

### 低成本 RECOMMENDED(R1 同批应用)
F-2 APC==0 Alpha 应急路径 + 快照契约强化;AC-12 双发计数;AC-33 FSM resume;AC-5b 非法转移拒绝;序列化字段补全(骰点/tiebreak/Latent 地雷警告);events 用 DYNAMIC_MULTICAST_DELEGATE + 枚举 append-only;CR-2/4 支柱权衡注 + 席位裁定可见 + bEnableDoublesExtraTurn 开关登记;Fantasy→HUD 最低接口契约;F-1 mod 安全澄清 + ensure 守卫;MAX_TIEBREAK_ROUNDS 概率注;OQ-1 ADR 清单扩充(异步等待禁 Latent / APlayerState 在线迁移陷阱 / AI hook 形态)。定序掷骰单/双骰澄清(供骰子3闭合)。

### CD 明确要求
本轮含 F-1 body 缺陷 + AI 决策链契约级修订 → **修订后强制 fresh re-review(/clear 新会话),不接受同会话自述闭合**。

### 待 fresh re-review 复核要点
- F-1 守卫先行是否真正消除矛盾(AC-17b 通过)
- CR-8 同步 AI 契约是否自洽、ResolvePhase hook 是否覆盖全部决策点
- 阈值锁定与存档序列化一致性
- OQ-1 BLOCKING-ADR 仍开放(下游 #4/5/7/9 开工前必须启动 /architecture-decision)

---

## Review — 2026-06-01 — Verdict: NEEDS REVISION (design-review full R2 / fresh re-review)
Scope signal: M (4 项窄契约修订,聚集 CR-8 单接缝,无架构返工/无新机制簇/无签名颠覆)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis, 独立边界值复算 R1 五项 blocking)
Blocking items: 4 | Recommended: 多项(本轮同批应用)
Prior verdict resolved: **R1 原 5 项 blocking 经 CD 独立复算全部真闭合(非 cosmetic 自证)**

### 终审裁定(creative-director)
R1 原 5 blocking(F-1 守卫/CR-8 同步/F-3 阈值锁定/AC-35 降级/F-4 round)经独立边界值复算**全部真闭合,机制语义实际改对**。但 CR-8 异步→同步是一次**机制簇迁移**,新缺陷高度聚集 CR-8 单接缝(single-seam-peeled),独立顶住 verdict。非 MAJOR(无架构返工)、非 APPROVED(Foundation 传播半径最大,含 2 个 R1 引入的真机制语义缺陷 + 谎报 [Logic] 的虚假 gate)。

### 4 项 BLOCKING(R2 已修订)
1. **dev 看门狗/AC-37a 同步自相矛盾**(ai-programmer + qa-lead **独立收敛**=高置信):R1 帧内虚拟时钟看门狗在同步单帧模型下不自洽(同步死循环=CPU 忙循环,虚拟时钟永不推进)。→ **裁定:删虚拟时钟看门狗**。"永不返回"靠测试框架进程级超时;"超单帧预算"靠同步调用返回后的事后帧诊断。AC-37a 第二段删除、新增 AC-37c[Advisory]。
2. **DecidePostRollActions 批处理失败策略未定义**(ai-programmer,接口语义缺陷):AI 基于 T0 快照批量决策,执行中途状态已变遇不可行动作行为未定义。→ **裁定:逐动作执行前重校验 + 不可行静默跳过 + dev 日志 + 不中断回合**。补 AC-42。
3. **快照契约/AC-35a 虚假 gate**(systems-designer #5 + qa-lead BLK-2 同类):"把依赖未决 ADR 的软约束当成已闭合硬契约写"。→ 快照契约等价降级为软约束(对齐 AC-35,论证单线程 BP+禁 Latent 下竞态理论不可达);AC-35a 从 [Logic] AC 列表移入 OQ-1,ADR 选 C++ 强封装后由 qa-lead 补回。
4. **OQ-T-1~4 回链无强制力**(qa-lead BLK-3):下游不知承诺存在。→ systems-index 新增「继承测试义务」追踪节(7/21/16/10 行可见)。

### 同批 RECOMMENDED(R2 应用)
NR-1 AI 可观察性(OnAIActionExecuted 事件 + AIActionDisplayMode 旋钮 + HUD16 登记;game-designer 判 BLOCKING,CD 降 RECOMMENDED——与"数据层提供 hook、呈现层兑现"边界一致);GameStateSnapshot 开口指向 OQ-1(ai-programmer+unreal 独立收敛);OQ-1 ADR 清单补 C-1~5(GameStateSnapshot UE 类型/tuning knob 序列化字段位置/Blueprintable 子类遮蔽/宿主三路选型/payload USTRUCT)+ APlayerState 明确排除;AC-39(DecideJailAction)/40(APC==0 广播)/41(OnTurnOrderResolved)/43(append-only 枚举)覆盖缺口;AC-12 测错系统校正/29 mock 前提/34 范围+重绑/37b 措辞可测化;F-4 子组内裁定/F-3 认知矛盾/F-1 守卫语义澄清;Fantasy→HUD 数值化(≤100ms/≤500ms)+ Pass'N Play handoff 提示 + 开局定序呈现缓解。

### unreal 本体裁定
GDD **本体 APPROVED**(R1 UE 判断技术准确:Latent 警告/DYNAMIC_MULTICAST_DELEGATE/受控写降级均站得住,无新 body blocker)。仅 OQ-1 ADR 清单不足→已补 5 条决策因子。

### 争议裁决(CD)
- NR-1 严重度:game-designer 判 BLOCKING(MVP=PnP+AI 体验完整性);CD 降 RECOMMENDED(呈现层缓解问题,修法低成本,与既有委派边界一致)。**本轮仍一并补 hook**(下游 HUD 需此 hook)。
- GameStateSnapshot 层级:GDD 留开口、字段定义转 OQ-1 ADR(C-1)。

### CD 明确要求(R3)
本轮最小集含第 1、2 项触及 **CR-8 机制语义 + AC 断言面**(AI 决策模型=项目#1 风险接缝)→ **第二轮修订后仍需一次 fresh re-review(/clear 新会话),不接受同会话自述闭合**。R3 mandate 须含 **CR-8 接缝正交全枚举**:逐一过 4 个 AI hook 失败语义 / 每条 AC 的虚拟时钟·wall-clock 性质自洽 / 快照取用时序 / 每个 OnXxx 事件下游消费方登记 —— 防 R3 再从同一接缝第 N+1 面掉 gap。

### 待 R3 fresh re-review 复核要点
- dev 兜底拆分(测试框架超时 + 事后帧诊断)是否真消除同步自相矛盾;AC-37a/37c 是否可实现
- DecidePostRollActions 逐动作重校+跳过策略是否覆盖全部失败分支;AC-42 是否充分
- 快照契约软约束降级是否对齐 AC-35;单线程+禁 Latent 可达性论证是否成立
- systems-index 继承义务节是否被下游 design-review 真正消费(机制有效性)
- OQ-1 BLOCKING-ADR 仍开放(下游 #4/5/7/9 开工前必须 /architecture-decision,清单已扩至 ⑫项 + AC-35a 待补回)
- CR-8 四 hook(DecideBuyProperty/AuctionBid/PostRollActions/JailAction)失败语义是否各自闭合

---

## Review — 2026-06-01 — Verdict: NEEDS REVISION (design-review full R3 / fresh re-review)
Scope signal: M(8 项窄契约/AC 收口 + 1 支柱裁定[B-1 gating],无架构返工/无新机制簇/无签名颠覆)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis, 独立逐条核验 5 专家发现)
Blocking items: 8 | Recommended: 多项(本轮同批应用)
Prior verdict resolved: **R1/R2 全部 blocking 经 CD 复核真闭合;R3 是 R2 mandate「CR-8 接缝正交全枚举」首次系统化执行**

### 终审裁定(creative-director)
R3 精确印证「单接缝分轮剥面 + 收口动作自留尾巴」收敛形态:8 项真 blocker 全命中 R2 mandate 清单,性质高度同质——CR-8 给 4 个 AI hook,R2 只锁 PostRollAction 一面执行期失败契约,R3 戳出另三面(买地/出狱/拍卖出价)失败语义全空;R2 新增的修补型 AC 把已清除两轮的 wall-clock 非确定性从 AC-37c/AC-42「断言 UE_LOG fire」后门引回。blocker 质量从 R1 架构断裂降到 R3「hook 对称面 + AC 守门面没跟上已正确本体」——零架构返工,机制地基(F-1~4 边界值复算真闭合、UE 约束站稳)依旧健康。唯一例外是 B-1:被 CD R2 错误降级的真支柱冲突。非 APPROVED(CR-8 #1 风险接缝仍按面渗漏 + wall-clock 后门复活不可带病放行)、非 MAJOR(无架构返工、修法均既有模式复用)。

### 8 项 BLOCKING(R3 已修订)
1. **B-1(game-designer):CR-8 同步 AI「不阻塞呈现」契约否决支柱②观察对手**(本作 priority-1 Fellowship)。CD 推翻 R2 降级、升 BLOCKING。**用户裁定:采纳 Animated gating**——Animated 模式框架进下一 TurnStart 前等呈现层 `OnAIActionPlaybackComplete`(dev 超时兜底);Instant 不等(保 OQ-3 确定性重放逃生舱)。改 CR-8 退出协议/L105/退出条件表/Tuning(新增 AIPlaybackGateTimeout)/Visual 契约;补 AC-44。
2. **B-2(game-designer):Fantasy→HUD handoff 无可测下限**(只 OnTurnStarted 享受 ≤100ms/≤500ms)。→ handoff 补"前一回合 outro 完成→才起高亮、不叠帧"可测下限,回链 OQ-T-4②。
3. **BLK-1(ai):DecideBuyProperty 返 true 但现金不足行为未定义**(hook #3 同型契约缺镜像面)。→ 执行期校验归经济5/所有权6、不可行视同 false+dev 日志;补 AC-38b。
4. **BLK-2(ai):DecideJailAction 非法返回(无现金保释/无卡)语义全空**。→ 校验归事件格7、降级留狱;补 AC-39b。
5. **BLK-3(ai):DecideAuctionBid 值域半发布**(int32 已被 snapshot 契约+OQ-1 消费,失败语义全空)。**用户裁定:现在钉死 sentinel**(负/INDEX_NONE=放弃、0 非法、须>当前最高价且<=Cash且>=最低加价、违反视同放弃、校验归拍卖12);补 AC-45[Advisory/Alpha]。
6. **BLK-R3-1(qa):AC-37c「断言 UE_LOG fire」+mock 真烧 wall-clock = wall-clock 非确定性后门复活**。→ 删自动断言,降纯文档化人工 dev 验证。
7. **BLK-R3-2(qa):AC-40③「标记退化局」无可观测载体**(board-data BLOCK-2 同型真空)。**用户裁定:OnLastPlayerStanding payload 增 `bDegenerateTie`**;AC-40③ 改断言该字段;CR-6/F-2 同步。
8. **BLK-R3-3(qa):AC-42①「静默跳过(UE_LOG fire)」把日志当 [Logic]/PR gate oracle**(比 AC-37c 严重)。→ 改状态断言(被跳过动作目标状态未改),UE_LOG 降括注。

### 同批 RECOMMENDED(R3 应用)
GameStateSnapshot per-hook 字段判据 + 事件格7 注入出狱卡(OQ-1⑭);RNG 软约束强度坦诚标注(CR-8 确定性 bullet + OQ-T-3②);PostRollAction 空数组=EndTurn 显式声明(AC-37d);单动作跨系统可行性聚合主责;OQ-1 标 P0/P1 分层 + 新增因子⑬⑭⑮ + ADR 关闭检查清单(AC-35a 锚点);PlayerState 容器=轻量 UObject vs snapshot=USTRUCT 判据;AC-34 读档「反序列化→重绑 delegate→才广播」时序;OnTurnEnded 下游登记(HUD16);快照A/B 时序作用域显式分隔;F-4 P=4 全员同分 Example;F-1 L202 mod 论证落点校正;CR-4 downtime 缓解分轨校正(开关+局长,非呈现层);AC-37a mock 可注入性前提;AC-32 mock 前提;AC-12 边沿广播前提;AC-37b 的 N 回填(OQ-T-3④);新增 OQ-5(早出局玩家)/OQ-6(AIDifficulty 可感知)/OQ-7(继承义务节升级建议)。

### 专家分歧裁决(CD)
- **分歧A(B-1,核心):** CD 推翻自己 R2 的「呈现层缓解」降级——L105 明文「不阻塞呈现」使呈现层工程上无法缓解契约层的拒绝。**呈现给用户裁定**(支柱② priority-1 vs OQ-3 联网重放纯净性),用户选 Animated gating(Instant 保留确定性逃生舱,两者兼得)。
- **分歧B(verdict):** systems-designer + unreal 判 APPROVED(域内正确:公式边界全跑无退化、UE 约束站稳);game/ai/qa 判 NEEDS REVISION。CD 裁定后者顶住 verdict。

### unreal 本体裁定
GDD 本体维持 R2 APPROVED(所有 UE 约束经 5.7 deprecated/breaking 文档核对站在稳定 API;OQ-1 ⑫因子完整无失效)。R3 仅补 ADR 护栏(P0/P1 分层、UObject 判据、读档重绑时序、OnTurnEnded 登记)+ payload 清单(FTurnOrderResult/bDegenerateTie/FAIActionDetails)。

### CD 明确要求(R4)
B-1 改 CR-8 核心契约 + BLK-1/2 涉机制语义 + AC 计数 42→47 → **修订后强制 R4 窄域 fresh re-review(/clear 新会话),不接受同会话自述闭合**。R4 mandate 须含:① **CR-8 四 hook 失败语义整簇三层对照**(散文/接口形态/AC 一致,不接受锁一面漏三面);② **AC 时钟语言斩断**(全文扫 [Logic] AC 确认无「断言 UE_LOG fire」或 wall-clock 时间断言);③ **可测载体核对**(每条断言状态的 AC 其对象在 PlayerState 字段或事件 payload 有承载);④ **B-1 gating 与确定性重放(OQ-3)不互斥、Instant 逃生舱完整**;⑤ OnXxx 下游登记全枚举(OnTurnEnded 缺口是否补)。

### 待 R4 fresh re-review 复核要点
- 四 hook 失败语义(AC-38b/39b/42/45)是否三层一致闭合、无第 N+2 面 gap
- AC-37c/AC-42 wall-clock/UE_LOG 后门是否真斩断;AC-37a mock 可注入性前提是否化解虚假 gate 风险
- B-1 Animated gating 自洽性 + AC-44 三分支(Animated 等/Instant 不等/超时兜底)是否可确定性实现
- bDegenerateTie 载体是否消除 AC-40③ 真空;CR-6/F-2/OnLastPlayerStanding payload 三处一致
- OQ-1 P0/P1 分层 + ADR 关闭清单是否防过载/防永远 pending
- OQ-1 BLOCKING-ADR 仍开放(下游 #4/5/7/9 开工前必须 /architecture-decision,清单已扩至 ⑮项 + AC-35a 待补回)

---

## Review — 2026-06-01 — Verdict: NEEDS REVISION (design-review full R4 / fresh re-review)
Scope signal: M(4 blocking:B-1 gating 接缝 3 面 + 1 cosmetic 残留;9 recommended;零架构返工/无新机制簇)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis, 独立逐条核验 + 主会话独立复算 F-1 cur=-P 数学断言)
Blocking items: 4 | Recommended: 9
Prior verdict resolved: **R3 全部 8 blocking 经本轮核验真闭合;R4 是 R3 强制的窄域 fresh re-review,执行 R4 mandate 五项**

### 终审裁定(creative-director)
R4 本该窄域收口,却从 **R3 自己用户裁定采纳的 B-1 Animated gating 接缝**剥出 3 个新面 + 1 个 mandate② cosmetic 残留 + 1 个 F-1 论证洞。精确印证"收口动作自留尾巴"形态:B-1 gating 由 game-designer(回调粒度+生产超时)与 unreal(中间状态枚举+回调形态)**从不同域独立收敛**=同一接缝新面,方向对、机制复用既有状态机/事件模式 → **NEEDS 非 MAJOR**(零架构返工)。blocker 质量延续 R1架构断裂→R4「gating 尾巴+cosmetic+论证洞」的持续下降轨迹,机制地基健康(F-1~4 边界值复算无退化、UE 约束站稳)。

### 4 项 BLOCKING(R4 已修订)
1. **BLOCK-1(5 专家全确认)AC-38b③ log-as-oracle 残留**(mandate② 未斩干净):删③编号、断言收敛纯可观测状态、log 降括注对齐 AC-42。
2. **BLOCK-2(game-designer)B-1 回调粒度未规格化**:多动作回合若 per-action 回调,gating 第1条后即解、后续截断、支柱②失效。**用户裁定:全部播完后单次回调**;改 CR-8 B-1 + AC-44① 补 N>1 fixture + HUD16 行。
3. **BLOCK-3(game-designer)AIPlaybackGateTimeout 生产构建行为缺失**:Animated 生产流程可永久卡死 show-stopper。**用户裁定:拆双阈值**(dev 诊断 `AIPlaybackGateTimeout` + 生产安全阀 `AIPlaybackGateProdSafetyValve` 强制推进+遥测);改 Tuning + entities.yaml + AC-44③。
4. **BLOCK-4(unreal)gating 期间无显式状态机相位 + 回调形态未约束**:存档语义/非法转移/fixture 注入点不全;OQ-1⑬「与 EndTurn 同类」递归开放引用。→ L162 加 `AIPlaybackGating` 相位 + OQ-1 新增因子⑯(P0)+ ⑬展开两形态约束。

### 9 项 RECOMMENDED(R4 同批应用)
F-1 cur=-P 枚举终止洞(主会话+systems 独立复算成立——入口欧几里得取模规范化 `((cur%P)+P)%P`,补 AC-17c,**R5 强制确认**);F-2 APC==0 WinnerId=INDEX_NONE(补 AC-40④);AC-39b log 降括注 + OQ-1⑤ 可注入条件门;AC-44③ 可注入超时声明;OQ-5 论据修正(OnPlayerBankrupt 追加不破坏 L184「只增不改语义」);三事件 payload 清单(OnTurnStarted/OnPhaseChanged/OnTurnEnded);OQ-1⑩ SaveGame 桥接 API 误述修正 + GameStateSnapshot GC 陷阱 + P0/P1 校正(⑧实为 P0 隐含前提、⑭循环阻塞缓解);CR-4 局长旋钮 cross-ref(GameLengthCap);Player Fantasy MDA framing 转 OQ-8(待 CD 复核,L39 警告已搁置四轮)。

### 专家分歧裁决(CD)
- 分歧A(AC-39b 性质):ai 判"不同于 AC-38b③"、qa 判"同型 log 残留"。CD 裁定:**qa 归类对(确属同型)、ai 严重度直觉对(比 AC-38b 轻)→ 降 RECOMMENDED**。
- 分歧B(B-1 gating 性质,核心):game-designer + unreal 独立收敛"R3 采纳的 Animated gating 本身规格不足"。CD 裁定:**收口尾巴非新机制 → NEEDS 非 MAJOR**。
- verdict:本轮 5 专家**全判 NEEDS REVISION**(R3 的 systems+unreal APPROVED holdout 本轮因发现真本体问题[F-1 cur=-P / gating 相位]转 NEEDS)。

### OQ-1 ADR 撑大
因子⑬扩容 + 新增⑯(gating 相位 ETurnPhase 落点,P0,与因子②同族);**P0 集合 = ①②③⑦⑪⑬⑭⑯**(未过载)。⑩ SaveGame API 误述已修、⑧标 P0 隐含前提、⑭标循环阻塞缓解。

### CD 明确要求(R5)
B-1 gating 接缝改触状态机(L162 加相位)+ AC 本体修改(AC-44 扩、AC-17c 新增)→ **修订后强制 R5 窄域 fresh re-review(/clear 新会话),不接受同会话自述闭合**。R5 mandate:① **B-1 gating 三面一次性整簇核验**(回调粒度单次/生产安全阀双阈值/AIPlaybackGating 相位三层散文-状态表-AC 一致);② **正交接缝全枚举防虹吸**(gating 相位是否引入新的非法转移/存档/payload 缺口);③ **F-1 cur=-P 修法落地确认**(入口规范化 + AC-17c 覆盖);④ AC-44 三分支(N>1 Animated 等/Instant 不进相位/生产安全阀强制推进)可确定性实现;⑤ 三事件 payload 是否被下游 design-review 真正消费。

### 待 R5 fresh re-review 复核要点
- B-1 gating 三面(回调粒度/生产超时/相位)是否三层一致、无第 N+1 面 gap
- F-1 cur=-P 入口规范化是否落地、AC-17c 是否覆盖;F-1~4 边界值是否仍健康
- AIPlaybackGating 相位的存档语义(读档重等 vs 视已完成)是否在 OQ-1⑯ 钉死
- OQ-1 P0 集合(①②③⑦⑪⑬⑭⑯)是否过载;⑭循环阻塞缓解是否有效
- OQ-1 BLOCKING-ADR 仍开放(下游 #4/5/7/9 开工前必须 /architecture-decision,清单已扩至 ⑯项 + AC-35a 待补回)

---

## Review — 2026-06-01 — Verdict: NEEDS REVISION (design-review full R5 / fresh re-review)
Scope signal: M(6 blocking:B-1 gating 介质改造 + F-1 主公式三层落地 + 存档语义钉死 + AC 时钟二次斩断;13+ recommended;零架构返工/无新机制簇)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis,独立核验 L225/AC-44/N-M 三处代码事实)
Blocking items: 6 | Recommended: 13+
Prior verdict resolved: **R4 全部 4 blocking 经本轮核验真闭合;R5 是 R4 强制的窄域 fresh re-review,执行 R5 mandate 五项**

### 终审裁定(creative-director)
R5 出现**质变信号**:B-1 gating 的核心契约介质 `OnAIActionPlaybackComplete`(R4 单次无参回调)在**协议层根本无法承载 AC-44 要它承载的断言**——框架收无参信号即推进,无法区分"正确单次"vs"per-action 早释"(ai ISSUE-1 + qa F-2 从两正交方向独立收敛=介质证伪)。这不是"同接缝再剥一面",而是 **R4 那一刀切歪了**(把"等全部播完"放在零信息量信号上)。同时 mandate③ 命中真本体洞:F-1 cur=-P 修法只落注释、主公式 L225 从未更新(F-1 第二次"修法落注释不落权威规格")。**为何非 MAJOR**:零架构返工,修法均既有模式复用(回调加 ActionIndex 参数 + 框架集合判据 / cur_safe 进主公式 / 存档钉直接推进)。机制地基(F-1~4 边界值、UE 约束、ownership 边界)仍健康。

### 6 项 BLOCKING(R5 已修订)
1. **B-R5-1 F-1 主公式 L225 未落 cur_safe、三层不一致**(systems 1-A,主会话+CD 核验):L225 改 `mod(cur_safe+k,P)` + 变量表补 cur_safe 行 + 删 L238 旧并存论证 + AC-17c 扩三变体。
2. **B-R5-2 AC-44①(c) 单次无参回调框架侧不可实现**(ai ISSUE-1 + qa F-2 收敛):**采纳框架侧计数收敛**——回调 `OnAIActionPlaybackSegmentComplete(ActionIndex)`,框架持 PendingPlaybackSet/CompletedPlaybackSet 集合相等时解 gating,early-release 天然拦截。
3. **B-R5-3 N/M 歧义(被跳过动作是否广播 OnAIActionExecuted 未定义)**(ai ISSUE-3,R4 未察觉新接缝):N≡实际广播数 M,被静默跳过动作不广播、不入集合。
4. **B-R5-4 AC-44③ 虚假 [Logic] gate**(ai ISSUE-2 + qa F-3):改 `[Logic·条件]` 默认 Advisory、删"旋钮置极小值"wall-clock 后门(AC-37c 同型)、钉 SimulateTimeout() 可注入接口、遥测须 mock 载体。
5. **B-R5-5 AIPlaybackGating 存档语义二选一应钉死**(unreal #1 + game-designer F-R5-7 收敛):钉"读档落 gating→直接推进"(选项 A 技术不可实现:FAIActionDetails 未序列化),消解 FTimerHandle 不可序列化(unreal #4)。
6. **B-R5-6 ConsecutiveDoubles gating 期归零时机未定义**(systems 3-A):归零在 gating 解除后实际移交时执行。

### 同批 RECOMMENDED(R5 应用,13+)
AC-17c 三变体(qa BLOCKING vs unreal NICE→CD 裁 RECOMMENDED 随 B-R5-1 同批);AC-5b 补 AIPlaybackGating 非法转移(unreal #3);AC-39b ② 降级留狱 +1 对称(ai ISSUE-5);AI 双点链不 gating 声明(systems 3-B);生产安全阀与 HUD 动画上限协调契约(F-R5-2);snapshot 生成时机 vs gating(ai ISSUE-4/6);OQ-T-4 bGrantsExtraTurn 两分支 + OQ-T-3⑤ bIsAI=false 防护(qa F-8/9);payload 重编译警告扩全部 struct + BP pin 静默断线(unreal #6);OQ-1⑬ 推荐形态(a) BlueprintCallable + ⑯ 已裁 + ⑭ 占位最低完整性 + P0 减负至六项(unreal #2/5);OQ-8 期限改 HUD 开工前(F-R5-4);OQ-5 重申升 MVP 建议(F-R5-6,scope 裁定留 producer);新增 OQ-9 多 AI gating downtime 缓解阀(F-R5-3,Alpha)+ L90 认领。

### 专家分歧裁决(CD)
- 分歧A(AC-17c 覆盖洞严重度):qa 判 BLOCKING / unreal 判 NICE。CD 裁 **RECOMMENDED**——qa 直觉对(cur=-4 是整除平凡点、最不暴露 bug),但属测试覆盖深度、随 B-R5-1 同批修,不独立顶 verdict。
- 分歧B(多 AI downtime F-R5-3):game-designer 判 BLOCKING(第 N+1 面)。CD 裁 **RECOMMENDED 认领即可**——B-1 正确工作时的已知设计代价(非规格缺陷),价值主张已 R3 用户拍板,重开需新用户裁定→落 OQ-9 + L90 认领。
- 核心战略(B-1 是否斩接缝):CD 裁 **不回退呈现层善意,但斩断"单次无参回调"失效介质,改框架侧计数收敛**——支柱②原始理由仍成立,前 3 轮剥的是粒度/超时/相位正常细化,R5 暴露的是 R4 介质选错。若 R6 仍从 gating 剥新面,才是结构性不收敛信号。

### blocker 质量画像
R5 与前四轮"尾巴下降"不同=混合:1 协议设计缺陷(质变,更上游)+ 1 本体规格不一致(F-1 第二次"修法落注释不落主公式",回升)+ 1 守门面虚假 gate。F-1"修法落注释不落权威规格"是本 GDD 反复失效模式(R4 cur=-P、R5 L225)。

### CD 明确要求(R6)
修订触及 F-1 主公式本体 + B-1 gating 核心契约介质改造 + 存档语义钉死 → **修订后强制 R6 窄域 fresh re-review(/clear 新会话,不接受同会话自述闭合)**。R6 mandate:① **F-1 三层一致硬核验**(逐字符比对 L225 主公式是否真改、非"注释说改了";AC-17c 三变体);② **B-1 gating 介质改造收敛性**(回调带 ActionIndex、框架集合判据、per-action 早释变真 [Logic]、N≡M——**判断 gating 接缝是否最终收敛的关键轮,若仍剥新面升级评估降回呈现层善意**);③ **AC 时钟语言二次斩断**(AC-44③ 改 [Logic·条件]、删旋钮极小值后门、全文无 [Logic] AC 依赖 wall-clock);④ **存档语义落地**(读档 gating 直接推进单一钉死、FTimerHandle 消解、归零时机);⑤ **正交接缝防虹吸**(介质改造是否引入新 payload/非法转移/下游消费缺口;AIPlaybackGating 入 AC-5b)。

### 待 R6 fresh re-review 复核要点
- F-1 L225 主公式是否真改 cur_safe(不接受注释自述);AC-17c 三变体;F-1~4 边界值仍健康
- gating 完成判据是否从"呈现层诚实回调"改为"框架集合收敛";AC-44①c 是否变真可测;N≡M + 跳过动作不广播
- AC-44③ 是否 [Logic·条件] 默认 Advisory、删 wall-clock 后门
- 读档 gating 直接推进单一钉死;ConsecutiveDoubles 归零时机;FTimerHandle 消解
- OQ-1 P0 集合(主动裁定降至 ①②③⑦⑪⑭ 六项);⑬⑯ R5 预裁确认落实
- OQ-1 BLOCKING-ADR 仍开放(下游 #4/5/7/9 开工前必须 /architecture-decision)
- F-R5-3 是否落 OQ-9 + L90 认领(RECOMMENDED 已认领即可,非 R6 blocker)

---

## Review — 2026-06-01 — Verdict: MAJOR REVISION NEEDED (design-review full R6 / fresh re-review) → 用户裁定 RETREAT,修订已应用
Scope signal: L(Foundation,8 下游硬依赖;retreat 净简化但属本体改动 + 需 user 重拍 → MAJOR)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis,独立核验 mandate ①③④ 逐字符闭合 + 裁定收敛信号)
Blocking items: gating 接缝集群(5 专家 / 5 正交方向)+ 2 独立(payload 生产侧 AC)| Recommended: 多项
Prior verdict resolved: **R5 全部 6 blocking 经本轮逐字符核验真闭合(mandate ①③④);R6 是 R5 强制的窄域 fresh re-review,执行 R6 mandate 五项**

### 终审裁定(creative-director)
**mandate ①③④ 逐字符核验真闭合**——F-1 主公式 L244 字面 `mod(cur_safe+k,P)`、变量表 cur_safe 行、Output Range `≠cur_safe`、AC-17c 三变体算术(next=1/3/2)独立重算正确;AC-44③ 时钟语言已斩;存档语义单一钉死。**本 GDD 反复失效模式"修法落注释不落规格"本轮未复发**。**但 mandate ②⑤ gating 接缝触发 R5 预注册的结构性不收敛信号**:5 专家从 5 正交方向同时剥新面,全部 root 到 R5 引入的计数收敛介质(`ActionIndex` 跨双点链作用域[ai 4-A]/跳过编号[systems 发现-4]/0-1based[systems 发现-3]/自增无 AC 致 R5 修复可被静默 nullify[qa BLK-R6-5];集合谓词 `==` 死锁[ai 1-A/unreal];破产截断广播[ai 2-A];Timer double-fire[unreal B-R6-UE-1];sync-vs-Tick 推进[qa BLK-R6-1];payload 生产侧 AC 缺口[qa BLK-R6-3/4])。CD 自评:R5 换介质把"无参回调歧义"换成"ActionIndex 语义歧义"=把同一个洞挪了位置。**判定 MAJOR REVISION**(质变:执行细节→"该不该做";需 user 重拍,非改几行)。

### 战略裁定(用户)
- **A:RETREAT(CD + game-designer 推荐,用户采纳)**:Animated gating 从框架硬保证降回呈现层软约定。理由汇总:① 价值不足以支撑成本(观察 AI 的 Fellowship 价值远低于观察人类对手,game-designer charge#1);② 结构性不收敛信号已触发(R3→R4→R5→R6 连续四轮剥面);③ 替代方案(非阻塞展示 + 回合摘要)更廉价且兑现支柱②信息层;④ 移除接缝后 GDD 正确性大幅改善(AIPlaybackGating 相位/集合判据/双阈值/segment 回调/OQ-1⑬⑯ 全消失,OQ-9 自然解消)。

### R6 修订已应用(RETREAT)
- **gating 介质全删**(权威面):CR-8 回放 gating 契约+退出协议、状态表 AIPlaybackGating 行+转移、AC-5b②、payload `OnAIActionPlaybackSegmentComplete`、Tuning 三 gating 旋钮(`AIPlaybackGateTimeout`/`AIPlaybackGateProdSafetyValve`/HUD 协调)、OQ-1 因子⑬⑯(P0 ①②③⑦⑪⑭)、OQ-3 兼容评估、Visual/L98 reframe。
- **AC-44 重写**:删 gating 全部断言(集合收敛/per-action 早释/生产安全阀/double-fire/`SimulateTimeout()`),改"Animated/Instant 均即时 EndTurn + 逐条广播 ActionIndex,纯同步可测"。
- **OQ-9 RESOLVED**(随 retreat,gating downtime 消失);`AIActionDisplayMode` 重定义为纯呈现旋钮;`ActionIndex` 降为呈现排序提示。
- **retreat 后存活的独立修复**:新增 **AC-7b**(`OnTurnEnded.bGrantsExtraTurn` 生产侧)/ **AC-5c**(`OnPhaseChanged.ConsecutiveDoubles` 生产侧)防 payload 死字段(qa BLK-R6-3/4);F-1 边界 L263 `mod(cur+1,P)`→`mod(cur_safe+1,P)`(第五面对齐,systems 发现-1)。AC 总数 48→50。
- **零架构返工**;systems-index #2 维持 Designed(待 R7 确认)。

### 专家分歧裁决(CD)
- 战略(retreat vs 修完整簇 R7):game-designer 判 RETREAT;实现侧专家(ai/systems/unreal/qa)把发现 scope 为"本轮可修→R7"。CD 站 game-designer 侧(四轮验证接缝结构不收敛 + 价值最薄),呈用户裁定(B-1 是 R3 用户决策,撤销需 user 签字)→用户采纳 RETREAT。
- verdict:5 专家发现 root 同源(R5 介质),CD 判定为收敛失败信号,非"再细化一面"。

### 独立待办(与 retreat 无关,保留跟踪)
- **OQ-8** Player Fantasy 缺 Submission 维度(game-designer 判设计层 BLOCKING,期限 HUD(16) 开工前,由 CD 复核 framing)——非 gating 相关,retreat 不消解,保留 OQ 跟踪。

### CD 明确要求(R7)
RETREAT 触及 CR-8 核心契约 + 状态机相位删除 + AC 本体重写 → **修订后强制 R7 窄域 fresh re-review(/clear 新会话,不接受同会话自述闭合)**。R7 mandate:① **gating 介质删除干净性**(全文无 PendingPlaybackSet/CompletedPlaybackSet/AIPlaybackGating/SegmentComplete/双阈值 作为 live 契约,仅历史 changelog/删除注脚可留);② **retreat 后 AI 可观察性接缝是否真消失**(框架即时 EndTurn 与人类一致、无新呈现↔框架绑定面);③ **新增 AC-7b/AC-5c 是否独立可测**(payload 生产侧断言);④ **F-1~4 边界值仍健康**(retreat 不触公式,确认无回归);⑤ **OQ-8 是否在 HUD 开工前被 CD 复核**(独立待办未漏)。

### 待 R7 fresh re-review 复核要点
- gating 五构件(相位/集合/回调/双阈值/ActionIndex 收敛键)是否全部降为历史引用、无 live 契约残留
- AC-44 是否纯同步可测、无 wall-clock/可注入超时依赖;AC-7b/AC-5c 生产侧载体齐备
- 框架推进是否对 Animated/Instant 一致(均即时移交)、确定性重放天然纯净
- OQ-1 P0 集合(①②③⑦⑪⑭);⑬⑯ 已删确认
- OQ-1 BLOCKING-ADR 仍开放(下游 #4/5/7/9 开工前必须 /architecture-decision)
- OQ-8 Player Fantasy Submission 维度(HUD 开工前 CD 复核,独立于 retreat)

---

## Review — 2026-06-01 — Verdict: NEEDS REVISION (design-review full R7 / fresh re-review)
Scope signal: S(3 blocker 全单遍纸面编辑:强度标签对齐 + 1 AC 重写 + 2 处删名;零架构/零公式改动/无新 ADR)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis,独立裁定收敛信号 + L184 分歧)
Blocking items: 3 | Recommended: 多项(本轮低成本部分同批应用)
Prior verdict resolved: **R6 RETREAT 修订经本轮核验:retreat 运行时绑定真斩断(L133 无 gating 决策分支、框架即时 EndTurn);F-1~4 零公式回归(systems 独立重算 cur_safe 三层一致、AC-17c 算术正确);"修法落注释不落规格"失效模式未复发**

### 终审裁定(creative-director)
**NEEDS REVISION——可收口的一种,非第 6 次剥面。** 使 B-1 结构性不收敛的运行时绑定(框架执行阻塞于呈现回放)已可验证地斩断。3 专家(ai/qa/unreal)在 AC-44/ActionIndex 上的收敛是 retreat 的**纸面尾巴、非第 5 次剥面**——一个跨三处自相矛盾的强度标签,单遍可修、零新机制。机制地基稳固:零公式回归,失效模式未复发。**3 blocker 全单遍编辑。收口纸面、放行地基。**

### 3 项 BLOCKING(R7 已修订)
1. **ActionIndex 强度分裂**(ai F1-A / unreal 发现5 / qa R7-1 三方收敛):散文(L138/L152/L215)称"呈现排序提示"(可选),AC-44① 却以 [Logic] PR-gate 断言 `0..M-1` 精确自增。CD 裁(BP 无 `TOptional<int32>` 破平局)**升散文为"框架拥有的轻量排序契约"**(框架保证自增、不消费做 gating),AC-44① 保持 [Logic]。三层(CR-8 散文/L215 payload/AC-44①)对齐。
2. **AC-44③ 无效 [Logic] 标签**(qa R7-1/2/4):"Animated≡Instant 移交时机相同"在框架不读旋钮时是空命题/或需 HUD 实例破 headless 纯度。**重写为静态可测**"框架无 `WaitFor*`/`Delay`/`FTimerHandle` 等待、不读 `AIActionDisplayMode`";Tuning L383 点名旋钮持有者=HUD。
3. **删名落权威 artifact**(unreal 发现1/2;**CD 裁 unreal 对、qa「L184 PASS」错**——artifact 风险压过"描述无误"):(a) 状态表 L184 去 `AIPlaybackGating` 具名 + AC-43/L219 补 append-only 例外(草稿值从未分配持久化索引);(b) OQ-1 ⑬⑯ 从因子清单**物理移除**(空缺编号不复用),历史留 changelog。

### 同批 RECOMMENDED(R7 应用)
AC-44①② fixture N/M 语义分注(ai F6-A);AC-44② 去溯源弱化、正面陈述呈现正确性(ai F4-A);AC-5c `≥1`→三变体精确值 0/1/2(qa R7-7);AC-7b/AC-5c 补 OQ-1⑤ 可注入条件门(qa R7-5/8);AC 编号口径澄清(父编号"50" vs 实际子编号 55,qa R7-13);F-4 round off-by-one 校正(systems F7-2)。

### 专家分歧裁决(CD)
- **L184 状态表(核心分歧):** unreal 判 BLOCKING(权威表格具名已删枚举值=实现者可能重加),qa 判 PASS(描述无误)。**CD 裁 unreal 对**——授权 artifact 内具名已删值是隐患,artifact 风险压过"技术描述准确"。
- **game-designer 两 BLOCKING:** F-1(可观察性下限)**降 RECOMMENDED**(重新litigate 用户 R6 已裁的软约定;内核转 HUD GDD cross-ref);F-2/OQ-8(Submission)是真 retreat 后缺口但**正确锚 HUD 开工前**,非 R7 blocker(mandate⑤ 说"复核"、已复核)。
- **核心战略(AC-44 是否第 5 次剥面):** CD 裁**纸面尾巴非剥面**——runtime binding 已死(L133 确认),剩余仅文档强度标签矛盾,单遍可修。

### blocker 质量画像
R7 延续"尾巴下降"轨迹至最低点:3 blocker 全为纸面(强度标签/AC 重写/删名),零机制语义、零公式、零架构。retreat 净简化已兑现(FTimerHandle/FAIActionDetails 序列化问题真消解)。

### CD 明确要求(R8)
3 blocker 虽单遍但触 AC-44 本体重写 + payload 定性 + 权威 artifact 删名 → **修订后强制 R8 窄域 fresh re-review(/clear 新会话,不接受同会话自述闭合)**。R8 mandate:① **ActionIndex 三层一致硬核验**(CR-8 散文/L215 payload/AC-44① 是否真"框架契约"单一定性、无"提示"残留);② **AC-44③ 静态可测性**(断言对象=框架无等待调用/不读旋钮,非"两模式时机对比"空命题);③ **删名干净性二次确认**(L184 + OQ-1 全文无 `AIPlaybackGating`/⑬⑯ 作 live 契约/因子,仅 changelog/AC 删除注脚/AC-43 例外说明可留);④ **F-1~4 边界值仍健康**(R7 仅动 AC/散文,确认公式无回归);⑤ **本 GDD"修法落注释不落规格"失效模式第三次核验**(R7 改的是散文+AC,须确认 AC-44①/L215/L138 真三层落地,非注释自述)。

### 待 R8 fresh re-review 复核要点
- ActionIndex:散文/payload/AC-44① 三层是否单一"框架轻量排序契约"定性,无"可选提示"残留
- AC-44③ 是否纯静态可测(无 WaitFor/Delay/Timer + 不读 AIActionDisplayMode)、[Logic] 标签是否成立
- L184 状态表 + OQ-1 因子清单是否全文无 `AIPlaybackGating`/⑬⑯ live 残留;AC-43 append-only 例外是否自洽
- AC-5c 三变体精确值、AC-7b/AC-5c OQ-1⑤ 条件门是否齐备
- F-1~4 边界值无回归;AC 编号口径(55)澄清是否落地
- OQ-1 BLOCKING-ADR 仍开放(下游 #4/5/7/9 开工前必须 /architecture-decision,P0=①②③⑦⑪⑭)
- OQ-8 Player Fantasy Submission(HUD 开工前 CD 复核,独立于 retreat);pillar-② 可观察性下限转 HUD GDD cross-ref(R7 降 RECOMMENDED)

---

## Review — 2026-06-01 — Verdict: APPROVED (design-review full R8 / fresh re-review)
Scope signal: L(Foundation,8 下游硬依赖,4 公式,驱动 OQ-1 ADR — 终态 verdict,GDD 设计层闭合)
Specialists: game-designer, systems-designer, ai-programmer, qa-lead, unreal-specialist + creative-director(senior synthesis,独立核验 binding 死亡 + 5 mandate char-by-char)
Blocking items: 0 | Recommended: 6 | Nice: 3
Prior verdict resolved: **R7 全部 3 paper blocker 经本轮核验真闭合(ActionIndex 三层一致 / AC-44③ 重写删空命题 / 删名落权威面 + OQ-1⑬⑯ 物理移除)**

### 终审裁定(creative-director)
**APPROVED——8 轮以来首次,gating 接缝终结。** R8 mandate 五项全部 char-by-char PASS:① ActionIndex 三层(散文 L138/L152 + payload L215 + AC-44①)单一"框架拥有的轻量排序契约"定性、无"可选提示"残留;② AC-44③ 删"Animated≡Instant 时机"空命题、改结构性断言(无 WaitFor/Delay/FTimerHandle、不读 AIActionDisplayMode);③ 全文 `AIPlaybackGating`/⑬⑯ 仅存于 changelog/删除注脚,权威面(L184 状态表 / OQ-1 因子表"现存=①–⑫+⑭⑮" / AC-43 例外)全干净;④ F-1~4 零公式回归(cur_safe 进主公式 + AC-17c 三变体算术 −4→1/−2→3/5→2 独立重算正确);⑤「修法落注释不落规格」失效模式第三次核验未复发(R7 改动真三层落地)。

### 核心战略问题裁定(AC-44③ 是否第 6 次剥面?)
qa-lead(F1-a)+ unreal(F6/F7)独立收敛:AC-44③ 的 `[Logic]` gate 第二半"框架不读 AIActionDisplayMode"靠**可选 grep**(且 BP `.uasset` 二进制不可 grep)、runtime fixture 证不了 → 守门覆盖缺口。**事实正确,但 CD 裁定严重度过冲。** 判据:使 gating 结构性不收敛的是**活的 runtime binding**(框架推进阻塞于呈现回放);ai-programmer R8 确认该 binding **可验证地死亡**(无任何新 presentation↔framework 绑定面,框架 wait on nothing,核验 L136/L490③/L217)。死的 binding 不再生面。AC-44③ 是"证明已死 binding 保持死"的**回归守卫精度瑕疵**(机制层即时移交由 path-A headless fixture 完整证明),非机制层剥面。**决定性测试:闭 AC-44③ 不需任何 runtime 回耦/新机制/改移交语义 → 接缝触底,降 RECOMMENDED。**

### 6 项 RECOMMENDED(非阻塞,留 HUD/AI GDD 开工前或下次修订统一处理)
1. **AC-44③ [Logic] 标签拆分**(qa F1-a + unreal F6/F7):拆为 `[Logic]`(即时移交/无等待调用,path-A fixture)+ `[Advisory]`(不读旋钮=静态扫描/code-review,强度随 OQ-1 容器选择;BP-primary 下 .uasset 不可 grep,与 AC-35a/AC-37a 同性质)。
2. **支柱②可观察性测试守门**(game-designer R8-1):retreat 后支柱②唯一保证来自 HUD;建议把"HUD GDD 覆盖 `OnAIActionExecuted` 消费 AC"升为 HUD(16) GDD design-review 的 BLOCKING 检查项(R7 降级的 pillar-② lower-bound 的正确 cross-ref 落地)。
3. **OQ-8 resolved 条件结构化**(game-designer R8-2):"HUD 开工前 CD 复核"无 resolved 判据/无强制机制;建议定义 resolved 条件 + 把"OQ-8 resolved"列为 HUD GDD review BLOCKING 前置。
4. **AC-44① N≥2 fixture**(qa):N=1 无法验证 ActionIndex 自增语义。
5. **AC-44② 标签澄清**(game-designer R8-3 + qa):"呈现正确性要求"实为框架侧 [Logic] AC,易误导 QA 跳过框架断言。
6. **OQ-1⑤ 粒度 + 杂项**(qa F7 + unreal F3/F5):AC-5c/AC-7b 降级门依赖骰子 hook 可注入性但 OQ-1⑤ 只写 AI hook 形态;FAIActionDetails 开放字段未继承 OQ-1⑮ GC 约束;payload BP-pin 警告未覆盖嵌套 struct 缺 BlueprintType;AC 计数 50/55 口径。

### Nice-to-Have
- changelog 4 处 `AIPlaybackGating` 无内联"已删"标注(unreal F1;mandate ③ 明文允许 changelog 历史提及,权威面已干净 → NICE)。
- AC-26 无法区分"子组内 vs 全局席位裁定"(systems;**既存缺口,非 R7 回归**——区分 fixture P=4 rolls=[9,5,9,5] 仅在 L317 散文)。
- L101 "回合切换 juice" 语义括注(game-designer)。

### 专家分歧裁决(CD)
- **verdict:** game-designer 判 APPROVED(对);qa+unreal 事实对但把守卫面精度瑕疵升 BLOCKING-class(严重度过冲)→ 综合 APPROVED + AC-44③ 作 RECOMMENDED。ai/systems 域内全清。
- **成本裁定:** 8 轮已耗;终态判据满足(binding 死 + mandate 全过 + 残留纯 paperwork/test-taxonomy,零机制零公式零架构零 binding)→ **in-session APPROVED,豁免 R9,不需 user 裁定。**

### APPROVED 附带强制说明(与 verdict 不冲突)
- **OQ-1 BLOCKING-ADR 仍开放。** APPROVED 仅指 GDD 设计层健全,**不代表 ADR 已满足**。下游 #4/5/7/9 实现前必须先启动 `/architecture-decision`(P0 因子 ①②③⑦⑪⑭)。
- **OQ-8 仍开放。** Player Fantasy Submission 维度须在 HUD(16) GDD 开工前由 CD 复核 framing 并落写 Fantasy 节(CD 自认领此义务)。
- **gating 接缝正式终结**(R3→R6 的结构性不收敛已由 RETREAT 根除,R7/R8 为纸面尾巴收口至触底)。

---

## Propagate — 2026-06-02 — 来源:dice GDD B1 / OQ-T-Dice-5(IG-1)
触发:骰子(3) design-review R1-B1 裁定"序列化须携带 Die1/Die2"(否则读档后 VFX 无法忠实重现两骰面值,Total=4 无法判定 1+3 还是 2+2)。dice R3 APPROVED 后,作为 implementation-gate IG-1 执行 `/propagate-design-change`。本项目尚无 ADR(`docs/architecture/` 仅 tr-registry.yaml),故为 GDD→GDD 受控传播,非 GDD→ADR。

**变更范围(player-turn 7 处,纯加字段、零逻辑/公式/状态机改动):**
- RECEIVE 契约(与 dice 返回值双向一致):L110 阶段表 RollPhase / L180 状态表退出条件 / L195 Interactions 骰子(3)行 —— `(点数,bIsDouble)` → 完整 `FDiceRollResult`(Die1/Die2/Total/bIsDouble)。
- SERIALIZE 契约(B1 强制,供读档后 VFX 重现骰面):L338 Edge Case 权威序列化列表(理由扩为 bIsDouble→TurnEnd 判定 + Die1/Die2→VFX 重现)/ L369 存档(21)依赖行 / L468 AC-33 fixture / L471 AC-34 完整存档定义。

**门控不变:** AC-33 仍 [Logic](确定性字段 roundtrip)、AC-34 仍 [Advisory](存档集成)——加 Die1/Die2 不改测试性分级。

**跨文档闭合:** dice OQ-T-Dice-5 propagate 状态 PENDING → RESOLVED(dice.md 两处 + systems-index 玩家回合(2)行同步)。B1 整条论证链已闭合,dice 实现前置依赖兑现。

**不影响 player-turn APPROVED 状态:** 变更为受控加字段,未触及任何机制/公式/binding/gating;无需重启 player-turn re-review。

---

## Review — 2026-06-04 — Verdict: APPROVED (design-review full / R-2to9 focused re-review)
Scope signal: S(纯架构管道对齐 propagate re-review,无新公式/无新依赖/无新 ADR;followup 均 player-turn 侧 AC 增改 + 1 句 CR-6 规格)
Specialists: systems-designer, qa-lead, game-designer + creative-director(senior synthesis,独立 deflate 5 项专家「BLOCKING」)
Blocking items: 0(真 blocker)| Recommended/must-land followup: 4(已就地应用)| 跨档 followup: 2(留 bankruptcy review)
Prior verdict resolved: **R8 APPROVED → 2026-06-04 `/propagate-design-change`(2↔9 return-only 对齐)降 NEEDS RE-REVIEW◊ → 本轮重过三锁面确认对齐干净、回 APPROVED**

### 触发背景
破产9 `/design-review` 揪出 9(return-only)↔ player-turn(Approved 旧双向耦合)系统性真矛盾(systems-designer + qa-lead 双独立确认);user 拍板 + CD 背书方向=改 player-turn 对齐 return-only。propagate 已执行(13 工单面 + 4 fresh-grep 增补),触三锁面 → 本轮 focused re-review 独立验证。工单 `docs/architecture/change-impact-2026-06-04-bankruptcy-playerturn-2to9.md`。

### 终审裁定(creative-director)
**APPROVED-with-followups,真 blocker=0。** 三锁面(F-2 APC 胜负口径 / 受控写接口面 `SetBankrupt` 调用方 / 胜负信号 `OnGameWon` schema)经主审 fresh-grep 双侧 + 3 specialist + CD 终审确认**架构干净、双侧对齐**。所有专家「BLOCKING」独立核验后全部 deflate:
- **systems-BLK-2(SetBankrupt 非法调用方 ensure 缺 AC)→ DEFLATED**:`bIsBankrupt` 已在 AC-35a 扫描集(L524);受控写契约(L102)本就只承诺 setter-routing 强度(pending OQ-1 软约束),不承诺运行期调用方身份校验。属既有延迟机制覆盖,非新 gap。
- **systems-BLK-1(bankruptcy L75/L189 `OnGameWon` 列「9→下游」措辞)→ 跨档 followup**:player-turn L258 已 enable-not-own 框定(9 供 winnerId、回合2 广播、下游呈现),本档内部干净;ambiguity 在 bankruptcy 侧,留其自身 review。
- **qa-BLK-1/3/4 → 「正确 spec 缺测试」型 followup**:spec 已对(L321 明文禁回合2 独立判胜),缺的是回归测试。

CD 把收敛 gap(qa-BLK-4 + systems-REC-1:无 AC 守「回合2 不独立 fire OnGameWon」=旧 2↔9 bug 复发路径)**升格为批准硬前置兑现项**——withhold 会重开 spec 已正确的架构面、制造无限剥面;must-land 既守住「不可静默 ship」又不污染干净结论。本档回 **APPROVED**。

### F-2 APC reconciliation 核验(systems-designer + CD 独立确认非同义反复)
player-turn `|{!bIsBankrupt}|`[0,P] 与 bankruptcy `count(p≠debtor ∧ !bIsBankrupt)`[0,P-1] **由 measurement-timing 真和解**(9 在 flag 写前算须显式排 debtor;回合2 写后算为计数辅助、明文禁独立判胜),非「X==Y 因 Y 用 X 口径估」型 tautology。胜负单源归9 winnerId。

### 4 项 must-land AC-hardening followup(本轮已就地应用)
1. **拆 AC-40 → AC-40a/AC-40b**(qa-BLK-3):AC-40a C++ headless stub 注入、**不随 OQ-1⑤ 降级**(守 return-only 无环契约的 CI 机器证明底线,stakes 高于 AC-37a)+ AC-40b BP mock 可降级。
2. **新增 AC-40c**(qa-BLK-4 + systems-REC-1 收敛):9 从未被调(直接置 flag)→ 回合2 TurnEnd/F-1 推进时 `OnGameWon` 广播计数==0,封堵旧 2↔9 双触发 bug 复发路径(直击 change-impact §1 根因)。
3. **CR-6 钉边沿触发规格 + AC-12 收紧**(qa-BLK-1 + REC-6):CR-6 补「`OnGameWon` 边沿触发一次、`Winner` 终态不重发」;AC-12 改写为构造真实二次触发机会 + bIsBankrupt→OnGameWon→F-1 时序前提。
4. **AC-19 + F-2 Example 时序前提**(qa-REC-2 + systems-NTH-1):AC-19 补「写后计数辅助口径」前提;F-2 Example 补 `[T,T,T,T]→APC=0→仅 assert、归9` 退化局行。

### 跨档 followup(留 bankruptcy 自身 `/design-review`)
- bankruptcy Interactions L75 / Visual L157 把 `OnGameWon` 列「9→下游 / 9 供事件」→ 应改 enable-not-own 显式措辞(回合2 广播、9 供 winnerId、下游呈现),与其 AC-40(回合2 触发)对齐,消实现者「9 广播」误读 double-fire 风险。
- APC==0 退化局 fallback 可能返回实际已破产玩家作 winnerId → `OnGameWon` 播给破产者(Alpha-only,MVP 单线程不可达)→ 登记 OQ-Bankrupt-1 子项。

### 专家分歧裁决(CD)
- game-designer(设计正确性域)直接判 0 BLOCKING / 三锁面 PASS / 封王 Core Fantasy 同帧无双发无丢失完整保留;与 systems/qa 的「test-hardening」定性互补不矛盾。
- systems/qa 的 5 项「BLOCKING」标签经主审独立对照代码 + CD 复核全部 deflate(见终审裁定),无相互冲突需用户裁定。

### blocker 质量画像
延续「尾巴下降」轨迹至收敛态:本轮 0 架构/0 公式/0 机制语义问题,findings 全为「正确 spec 的测试覆盖 + 跨档措辞清晰度」硬化。focused propagate re-review 确认对齐干净 → APPROVED-with-followups,非无限剥面。

### APPROVED 附带强制说明(承袭 R8,未因本轮改变)
- **OQ-1 BLOCKING-ADR 仍开放**(下游 #4/5/7/9 实现前必须 `/architecture-decision`,P0=①②③⑦⑪⑭)。
- **OQ-8 仍开放**(Player Fantasy Submission 维度,HUD(16) 开工前 CD 复核)。
- 本轮不触 R8 已裁定项(gating retreat / Fantasy / AC-35a 强度),三锁面外全文无回归。
