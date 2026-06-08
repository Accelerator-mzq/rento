# Review Log — 音频 SFX/BGM (audio.md)

## Review — 2026-06-06 (R-1 fresh `/design-review`) — Verdict: APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕
Scope signal: S
Specialists: 5 专家隔离 fresh `/design-review` + creative-director(senior synthesis)
Verdict tally: 3 APPROVED_WITH_FOLLOWUPS vs 2 NEEDS_REVISION → CD synthesis 取 **APPROVED_WITH_FOLLOWUPS**
Blocking items(verified): **0** | Deflated: 1(幻象枚举机制) | Logged decisions: 1 | Followup OQ: OQ-Audio-1 / OQ-Audio-2

### Summary
#22=呈现层声音唯一 owner 纯叶子(只订阅 owner multicast delegate→触发 Sound Cue 播放 + 混音,不写状态、不裁决结算、不阻塞框架推进;owner 先算定结果,#22 收到即播,落后只丢声音不回灌框架)。与 VFX19 边界由 **enable-not-own** 厘清(#22 拥声音资产/混音总线/事件→声音映射,#19 拥视觉 juice;同一 gameplay 事件各自独立订阅 owner、各播各的,互不双触发互不依赖=声画分离标准模式,非歧义)。11 节全写,27 AC(22 [Logic·BLOCKING] + 2 [Integration·BLOCKING AC-23 事件格7 入狱/出狱/抽牌链路 + AC-24 经济5 破产 + 回合2 胜利链路] + 3 [Advisory·playtest-signoff 节奏/情绪/响度])。

主审逐条对文核实 5 专家 claim-wave:工程结构干净收敛,**继承义务全覆盖落 AC**——掷骰 juice 听觉腿(dice OQ-T-Dice-4)→ CR-3/AC-1/2/3/4 + Advisory AC-25;经济现金 juice 听觉腿(economy Visual,Sensation priority-4)→ CR-4/AC-5/6;建房 + 地产 juice(承接 #8/#6)→ CR-5/AC-7/8/9/10/11;RNG 隔离(ai CR-5④ / VFX19 CR-9 禁复用骰子3 权威流防联网重放污染)→ CR-7/AC-22;非阻塞追赶/跳过(HUD F-4 / VFX19 CR-8)→ CR-9/EC-12;读档重绑防双订阅(EC-8)→ AC-21。`OnOwnershipChanged` re-seed 封破产清算 cache 漂移(AC-9,镜像 VFX19 Finding A);`EChangeReason` 未注册值→中性兜底音(EC-5,镜像 VFX19 EC-17);破产音订**经济5 `OnBankruptcyDeclared`**(2 字段)非直订破产9,与 VFX19/HUD16 一致。

### Verified blockers
**verified_blockers = []**(0 条真 in-doc BLOCKING)。

### Deflated(1)
- **AC-5「锁死幻象枚举→编译失败/false-green」机制不存在**:2 位 NEEDS_REVISION-camp 专家赖以升级 BLOCKING 的核心机制经主审逐条对文核实——AC-5 fixture 实际只用合法值 `EChangeReason=Salary`(正向音色)/`=Tax`(负向音色)/未注册枚举值(中性兜底音),**未锁死任何幻象/不存在的枚举名**,不会触发编译失败或 false-green。该升级前提失效 → 不支持 NEEDS_REVISION。

### Logged decisions(1,finishing-class,非 BLOCKING)
- **decision**:将 `EChangeReason` 枚举字面分歧与继承义务表缺 #22 行判为 **propagate 债而非 in-doc BLOCKING**,verdict 取 APPROVED_WITH_FOLLOWUPS。
  - **rationale**:5 专家裁决分裂(3 vs 2)。2 位 BLOCKING-camp 赖以升级的核心机制(AC-5 锁死幻象枚举)经核**不存在**(AC-5 fixture 只用合法值 Salary/Tax/未注册)。真分歧(枚举字面、表缺行)均(a)逐字继承自已 Approved 姊妹档 VFX19、(b)带中性兜底运行期安全(EC-5/EC-6 未知值不误播)、(c)义务已落本档 AC。无 in-doc BLOCKING、无需重裁「#22 是什么」。
  - **reversible_note**:可逆——propagate 批落地时若发现 VFX19 CR-5 在其 Approved 历程中曾被显式裁为合法 owner-extend 承诺,则 audio 完全继承无须再动;反之若 producer 裁定 economy 不扩枚举、下游须改用 9 真值,则 audio CR-4 与 VFX19 CR-5 同批重写。两路径均不影响本轮 audio 开工,仅决定 propagate 批的具体改法。

### Followups(不阻开工)
- **OQ-Audio-1**:动态音乐分层 / 自适应配乐 / 动态 ducking / 3D 空间化音源全降 Alpha(MVP 只做核心循环 SFX + 1–2 条 BGM + 三路混音总线 + BGM duck 音量阶跃简化)。
- **OQ-Audio-2**(propagate 债,producer):**① ✅ RESOLVED 2026-06-08**——各 owner GDD 已含音频(22)消费方(economy L117/118、property L24、building L224、player-turn L268;dice 经 systems-index 脚注♫;fresh-grep 核实已兑现);**② ⏸ DEFERRED**——`PlayUISound(EUISoundId)` 在 UI 屏 GDD(#16/#20/#23 多 Not Started)登记被调义务,待下游 UI 屏 GDD 撰写时落实(阻塞,无法提前);**③ ✅ RESOLVED 2026-06-08**——破产事件接缝 OQ-VFX-13 producer 裁定=呈现层统一订经济5 `OnBankruptcyDeclared`(ADR-0003 §3);#22 与 VFX19/HUD16 一致,破产9 `OnPlayerBankrupt` 不被呈现层订阅(防双发)。
- **Flags**:📌 Asset-Spec(art-bible approved 后 `/asset-spec system:audio` 生成逐资产音效 WAV 48kHz/16bit spec);非 UI 屏,无 UX-spec flag(音量设置 UI 复用 #23 设置屏/主菜单,本系统只供混音总线参数语义)。

### Convergence signal
blocker 量首评即 0 = 健康收敛终结(verified_blockers=[]、deflated 1 幻象枚举机制、logged_decisions=1)。无需 R-2。遗留 OQ-Audio-1/2 + Asset-Spec Flag 均不阻开工。

Prior verdict resolved: First review(首评即收敛)
