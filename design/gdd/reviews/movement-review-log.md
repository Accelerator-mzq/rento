# Movement (移动 #4) — Design Review Log

## Review — 2026-06-02 — Verdict: NEEDS REVISION → Revised R1 (pending fresh re-review)
Scope signal: M
Specialists: game-designer / systems-designer / qa-lead / unreal-specialist / economy-designer + creative-director（senior synthesis）
Blocking items: 5 | Recommended: ~12
Summary: 首次正式 full re-review（此前为 lean 起草期 inline specialist 输入）。CD 综合裁定 NEEDS REVISION，5 真 blocker 一批闭合无架构返工：① **Player Fantasy framing 越权**——移动作为编排层声称拥有"逐格期待"，实际寄生 VFX(19)；全改写为 enable-not-own Experience Contract，删 MDA Aesthetic 越权声明，对齐 dice OQ-T-Dice-4 判例。② **arrivalContext 注入未规约**——`Advance`/`TeleportTo` 签名缺 `context` 参，AC-14 假设注入；采透传模型补签名参（只有透传能区分 DiceMove vs AdvanceCard）。③ **AC-11 建于未核实 OQ-Move-1**——[Logic] BLOCKING 却标"暂定"；冻结为 [Logic·PROVISIONAL]，gate 20→19。④ **L110 溢出声称是事实错误**——economy clamp[0,1000] 仅护 passed_go 侧不护 SalaryAmount（OQ-Econ-10 未闭合）；改"部分防护"+ 登记依赖。⑤ **OQ-T-Move-4 在 systems-index 责任真空**——经济(5)无行；补登记并回链已核实的 economy AC-6/AC-7/F-4。
CD 关键调级（非照单全收）：P-3 vs board-235 由两 specialist 的 BLOCKING **降为 RECOMMENDED**（board L235 立法意图是防"漏发薪传送"，去坐牢明确不发薪不在危害域，仅缺 named exception 登记）；OQ-T-Move-1 由 BLOCKING 降 RECOMMENDED（义务文字已登记，仅回链 AC 号待补）。
锁定 APPROVE 面：AC-7 passedGo=0 边界（qa 独立复核，确认主会话此前驳回旧 F-1 正确）；去坐牢不发薪 ↔ economy CR-2 双向闭合（economy-designer 确认）。
用户 R1 决策：arrivalContext=透传+签名加参；AC-11 冻结 PROVISIONAL；Player Fantasy 全改写 enable-not-own。三项均取 CD recommended。
应用范围：movement.md 18 处编辑 + systems-index.md 3 处（补经济(5)继承义务行 + OQ-T-Move-1 回链待确认注 + 此前已含的 7 depends-on 4 确认）。
Prior verdict resolved: First review（首次正式 design-review；本条为 R1 修订记录，待 fresh re-review 核验闭合）

## Review — 2026-06-02 — Verdict: NEEDS REVISION → Revised R2 (pending fresh re-review)
Scope signal: M
Specialists: game-designer / systems-designer / qa-lead / unreal-specialist / economy-designer + creative-director（senior synthesis，全员 opus）
Blocking items: 2 真 blocker（CD 裁定）| Recommended: ~10
Summary: fresh re-review 核验 R1 的 5 BLOCKING 闭合情况。结论：**#2(arrivalContext 透传)/#3(AC-11 冻结) 全闭合**；**#1/#4/#5 部分闭合留残面**，且更深的跨系统核验**新发 2 真 blocker**：
- **Finding E (BLOCKING，economy+game+qa 三方汇聚)**：CR-3.1 的 `Total` **PUSH-and-cache** 模型与经济 F-4/AC-18(`dice_total` 解决期参数、**显式不缓存**、PR-gate) 不兼容——连续双点一回合两次 `Total` 投递无单源界定 + 事件额外骰第二写者 = stale-Total/双写者正确性缺口；且 CR-3.1 **无任何 AC 覆盖**(违"全覆盖")。CD 裁 TRUE BLOCKING。
- **Finding B (BLOCKING，qa 独立核验 player-turn.md)**：AC-15 去坐牢抑制买地/收租标 Advisory 委托 player-turn，但 player-turn 全文**无** `SentToJail`/`arrivalContext`/`OnPawnLanded`、无对应 AC、已 Approved 不回读 OQ-T 表 → 核心规则正确性("去坐牢却能买落地格地")**无 BLOCKING 覆盖**=responsibility vacuum 复发。
CD 裁级两分歧：**Finding C(完整溢出 false-closure)→ RECOMMENDED**(honesty drift，非 movement-logic 缺陷)；**Finding D(TeleportTo target==from 越权 board F-3 调用方裁决权)→ RECOMMENDED-but-mandatory**，与 Finding A + prior #1 归同一 ownership-error class，两编辑收口。
用户 R2 决策：① Finding E 改**解决期 PULL**(movement 不投递 Total，只当 steps 消费)+OQ-Move-5 留 cross-doc；② Finding D 改**调用方注入 `advanceOnZero` 参数**(AC-11 因此解冻为确定性 AC-11a/11b)；③ Finding B movement 侧升 AC-12b[Logic]，player-turn 补 AC 留 propagate followup；④ 命名规范化延后。
应用范围：movement.md ~20 处编辑（CR-1/2/3.1/4、P-2/P-3、Overview、超界防护、Edge、Dependencies、AC-4/7b/11a/11b/12/12b/13/15/18/20、OQ-Move-1/3/5、OQ-T-Move-1/2/3/4、gate 重计 19→23 Logic）。
Prior verdict resolved: R1 的 #2/#3 ✅闭合；#1/#4/#5 留残面已在 R2 收口（#1→Overview+OQ-T-Move-3 experiential；#4→OQ-Move-5 第二溢出登记；#5→economy 行在但 OQ-T-Move-1 派生 Finding B）。**R2 待 fresh re-review 核验 2 新 blocker 闭合 + cross-doc(Finding B player-turn AC / OQ-Move-5) propagate 是否已 owner。**

## Review — 2026-06-02 — Verdict: NEEDS REVISION → Revised R3 (APPROVE-pending-propagate)
Scope signal: 本档 S / 跨档 propagate M
Specialists: game-designer / systems-designer / qa-lead / economy-designer / unreal-specialist + creative-director（senior synthesis，全员 opus）
Blocking items: 本档 1（E）+ 跨档 3 propagate（A-line102 / B+C player-turn / DiceMultiplier）| Recommended: ~9
Summary: 第三轮 fresh re-review 核验 R2 闭合。**R2 的 Finding D(advanceOnZero honor board F-3 L233)/Finding E(CR-3.1 不缓存) movement 侧逐字核验闭合**；board L235 named exception 成立。但更深跨系统核验 + 主会话独立对照核验发现**真重心已从「movement 内部」迁移到「movement↔已 Approved 邻居接缝」**：
- **A(三方汇聚+独立核验 BLOCKING)**：R2 把 Finding E 只落 movement.md，economy.md **line 102 Interactions 表仍残留旧 push 措辞「移动供 Total」**，与 CR-3.1 PULL 方向相反——R2 single-doc 修订制造的跨档真矛盾(照 economy 实现会造出被 AC-7b 立即打死的通道)。CD 校准：line 102=真矛盾(必改 economy)、F-4 line 176「显式参数传入/不读缓存」=与 PULL 兼容(非矛盾)、真缺口是 holder owner 真空(判定=回合2)。
- **B(game RECOMMENDED vs qa BLOCKING)**：SentToJail 抑制委托 player-turn，但 player-turn 零命中、无 AC、已 Approved。CD 按 owner 分流：对 movement=followup(AC-12b 已尽责)、对 player-turn=BLOCKING propagate。game「危害域 MVP 空(Jail 格惰性)」与 qa「零测试=自欺」两者都对，量的是不同东西。
- **C(qa BLOCKING)**：CR-4 程间非重入同样委托 player-turn 无 AC（与 B 同型真空）；unreal 补：OnPawnLanded 同步 Broadcast 监听者回调内同步触发新 Advance=同步嵌套重入，「禁 Latent 天然成立」不覆盖。
- **E(unreal BLOCKING)**：AC-21 无条件 `SetTileIndex` BlueprintCallable ↔ OQ-Move-3(a) C++ 强封装自矛盾，文档内实现者无法执行（**唯一必须改本档的真 blocker**）。
- **D(systems BLOCKING→CD 降级)**：advanceOnZero=false 不广播 OnPawnLanded 是正确 feature(防重复收租)非 bug，但游戏规则隐式决定=honesty 缺口。
CD 关键裁定：**这是 system-of-systems propagate 债，非 movement 不收敛——处方是 propagate 不是 RETREAT**(与 player-turn gating RETREAT 案例性质相反：那是单接缝内部反复剥面，movement 是正确委托给不回读的 Approved 邻居)。CD 直言项目风险：第三次撞见「single-doc 修订让 Approved 邻居 stale」+「义务登记在 Approved 下游=真空」，建议 producer 立流程硬约束(跨档接缝修订须同 PR 开 propagate + grep 核实，不接受 OQ-T 表登记当闭合)。
应用范围：movement.md 本档改 4 处(AC-21 条件化 / OQ-Move-5 点名+拆 OQ-Move-6 / line 127 owner 重指 / Edge Cases 显式论证) + Status 升 R3 + systems-index †脚注 + 本 review-log 条目。**剩 A-line102/B/C/DiceMultiplier 为 producer BLOCKING propagate 工单(下游 5/6/7 开工前必闭合)。**
Prior verdict resolved: R2 Finding D/E movement 侧 ✅闭合(逐字核验)。本轮新发 A/B/C/E/D；E 本档已修，A/B/C/DiceMultiplier 移交 producer propagate(用户 R3 决策：先改本档→再起干净会话跑 propagate 工单)。
