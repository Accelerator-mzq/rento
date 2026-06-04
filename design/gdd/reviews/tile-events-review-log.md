# Tile Events (#7) — Review Log

## Review — 2026-06-03 (R2) — Verdict: APPROVED-with-followups (`CD-GDD-ALIGN: APPROVE`)
Scope signal: M–L
Specialists: systems-designer, qa-lead, creative-director (R2 panel; economy/game/unreal R1 items verified-closed-in-text or deferred)
Blocking items: 0 new (12 首评 BLOCKING 全 CLOSED) | Followups: 1 this-doc must-fix (已修) + 1 cross-doc REC + ~5 advisory + deferred/OQ
Summary: R1 闭合验证。**两条首评 MAJOR 驱动(OQ-Event-1 监狱态双写、B-12 DecideJailAction 降级)双双 CLOSED**——主审对照 player-turn 实际文件 fresh-grep 核实 L84/L85/L200/L366/CR-8#4/AC-39b 与 tile-events R1 对齐一致、零残留。qa-lead:12/12 首评 BLOCKING CLOSED(B-4 PARTIAL→advisory 措辞),0 新 BLOCKING。systems 标 2 BLOCKING 均被主审+CD 降级:① **L110 存档行 model-A 残留**("指针"+"各玩家监狱态",与 CR-3/L94/L235/CR-7 三处矛盾)=本档单 cell must-fix(无设计决策)**本轮已修**;② **player-turn 监狱态受控写接口未命名**——systems 引 vacuum memory 判 BLOCKING,但主审核实 player-turn L200/L366 已明文"bIsInJail 改写归7"(契约双向在册),判**命名缺口≠义务真空 → RECOMMENDED propagate**(CD 复核采纳;memory 已补此反向边界)。
Followups:
- **跨档 propagate(producer,低成本)**:player-turn 为监狱态显式命名受控写接口(如 SetJailState,补在 L89 SetPosition 例旁;契约已在册仅补名)。
- **ADVISORY(实现期/下一 pass)**:AC-35 删 bForceRelease==true 不可测断言;条件门 AC-21/32/33/34 升级回链给 OQ-Event-3 明确触发 owner(建房8);AC-41 措辞对齐 DecideJailAction 返回 UseCard;留狱递增补设计意图注;player-turn AC-39b 对齐核实(已 verified)。
- **deferred REC(不阻 Approved)**:Player Fantasy 结构重构 + 保释金50退化/jail-as-safe-haven 诚实标注(首评 game-designer 提、MAJOR 非由其驱动)。
- **pre-freeze / 实现期 OQ**:OQ-Event-6(Rento 监狱规则核查)/ OQ-Event-7(engine ADR:程间非重入/牌堆宿主/RNG/dispatch)/ OQ-Event-3(RepairFee house_count 待建房8,4 条 AC 已降条件门)。
Prior verdict resolved: Yes — 首评 MAJOR REVISION NEEDED 的 2 跨档结构驱动 + 12 BLOCKING 全闭合。

## Review — 2026-06-03 — Verdict: MAJOR REVISION NEEDED (R1 修订当会话已落，待 re-review)
Scope signal: M–L
Specialists: game-designer, systems-designer, economy-designer, qa-lead, unreal-specialist, creative-director (synthesis; `CD-GDD-ALIGN: REJECT`)
Blocking items: 8 (2 跨档结构 + 6 本档) | Recommended: ~20 (AC-craft 簇 + 设计诚实项)
Summary: 首评。本档自身规格质量不低(11 节齐全、体验链/零和/确定性契约俱写)——CD 判 MAJOR 的唯一驱动是**两条非单方能闭合的跨档结构问题**:① **OQ-Event-1 监狱态双写**——主审对照 player-turn(已 Approved)L84/L85/L200 **确证真矛盾、方向相反**:player-turn 持 `bIsInJail`/`JailTurnsServed` 字段+做计数,tile-events 却声称 7 拥有+7 自增+"回合2 不直写";systems F-1/qa B-12 独立收敛"双增破坏 [0,2] 不变式"。② **B-12 DecideJailAction 非法降级**——player-turn CR-8#4/AC-39b 下派给 7,本档零覆盖。另:economy 核实"economy-designer 校验 2~2.5:1"=**虚假引用**(economy L261 仅"预期偏通缩、须 playtest");AC-37 ConsecutiveDoubles 断言=同义反复;单 bool 出狱卡无法区分两堆。
独立核实(主审，对照实际文件): OQ-Event-1 真双写 ✓ / 虚假引用 ✓ / AC-37 同义反复 ✓。
qa 12 BLOCKING 中 CD 判仅 3 为本体规格 gap(bForceRelease 接口/枚举进正文/继承义务零覆盖),余 ~9 为 AC-craft。unreal 5 BLOCKING:3 须现在写清(牌堆模型/单 bool 出狱卡/枚举)、2 留 ADR(非重入机制/RNG 细节)。
Prior verdict resolved: First review.

### R1 修订(2026-06-03，同会话，用户裁定"现在一起改"；~35 edits)
用户裁定:OQ-Event-1 **以 player-turn 为准**、OQ-Event-4 **逐笔足够+重表零和**、范围 **blocker+本档REC+AC-craft**。
- **8 blocker 全解**:① OQ-Event-1 对齐 player-turn(字段存 player-turn/player-turn 计数/7 拥规则+受控写;`bInJail`→`bIsInJail`;CR-3/5/7/9/States/Interactions/AC-37/40/45/52);② B-12 补 AC-62/63(承接 player-turn AC-39b);③ 虚假引用清除;④ OQ-Event-4 收口(降说明性 note,非 propagate 债);⑤ 单 bool→按堆 `bHoldsChanceOutCard`/`bHoldsChestOutCard` + `HoldsAnyOutCard` 派生;⑥ CR-9 掷骰时序;⑦ `EJailReason`/`EJailReleaseMethod` uint8 进 Detailed Design;⑧ 牌堆 model B(无指针)+ Fisher-Yates 种子序列化。
- **AC-craft**:AC-29(不可能平局 fixture 重写为确定性+单元素)/AC-37/40(反同义反复:断言"7 不发信号")/AC-51(删永真)/AC-58(UHT 零警告)/AC-36/39(删主观措辞)/AC-52(holder 接缝)/+AC-64(序列化)。AC 61→64。
- **OQ**:Event-2 CLOSED(并入 economy F-7)、Event-5 RESOLVED(registry 已注册)、Event-7 新增(engine ADR:非重入/宿主/RNG/dispatch)。
- **fresh-grep 核实**:bInJail/JailTurnsServed++/虚假引用/bHoldsGetOutCard/须propagate补 = 0 真残留。
- **残留 propagate(唯一,低成本)**:player-turn 须显式命名监狱态受控写接口(现 L89 仅举 SetPosition)。
- **deferred REC(本轮未做,留下一 pass)**:Player Fantasy 结构重构(enable-not-own 殖民)+ 保释金50退化/jail-as-safe-haven 诚实标注。
- **下一步**:fresh session re-review 验收 R1。
