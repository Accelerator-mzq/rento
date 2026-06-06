# 存档/读档（#21）Design Review Log

> 本文件记录 `design/gdd/save-load.md` 的 design-review 修订历史,供 fresh-session 再审追踪。

---

## Review — R-1 — 2026-06-06 — Verdict: APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕
Scope signal: S（持久化横切层纯搬运,无 gameplay 平衡公式〔F-1..F-4 仅工程完整性校验:魔数/版本/校验和/字段清单〕,4 硬上游〔棋盘1/回合2/经济5/地产6〕+ 间接横切〔建房8/破产9/骰子3〕,无新架构、无运行态、无环〔存档依赖状态系统不被反向依赖〕;blocker 量首评即 0）
Depth: full（`/design-review` fresh session,5 specialist + creative-director synthesis）
Prior verdict resolved: 首评（无 prior log）

### Verdict
**APPROVED_WITH_FOLLOWUPS** — `verified_blockers=[]`、3 `logged_decisions`（均 finishing-class,纯文档收紧、不改 carrying contract、不阻开工）。无 in-doc BLOCKING、无跨档 must-land、无架构返工、无 pillar/身份重裁定。遗留全为 OQ 上游/架构债 + propagate 债,均不阻开工。

### 关键发现（5 专家收敛 — 无真 BLOCKING）
- **enable-not-own 横切层边界正确**（吸取 [[orchestration-layer-fantasy-enable-not-own]]）：#21 不拥有任何被序列化字段语义(现金归经济5、房数归建房8、阶段归回合2),只拥有"采集/写盘/读盘/重建/重绑"搬运流程。Player Fantasy 限"隐形的进度不丢/续局逐字段一致"信任底线,横切诚实声明明确 enable-not-own。
- **核心防漏机制健全**：CR-3 逐状态系统可序列化契约清单(字段名逐字对齐各 owner GDD)+ F-4 字段清单完整性运行时门(manifest 比对暴露遗漏)+ 金标准 round-trip identity AC-1(存档前快照==读档后快照逐字段 identity-check)。无"调参直到通过"逃逸条款——任一字段漏存/错存,往返比对必 FAIL([[ac-tune-until-pass-escape-clause-is-tautology]] 红线守住)。
- **继承义务全覆盖落 AC**（qa-lead 对照 systems-index line~86 存档行核对）：① player-turn OQ-T-2/AC-34 → AC-4/5/6;② dice OQ-T-Dice-5/B1 → AC-7/8;③ player-turn AC-34 末句/CR-6 → AC-9/10/11;④ economy OQ-T-Econ-4 → AC-12;⑤ economy OQ-Econ-4/CR-4 → AC-13/14/15。
- **AC 诚实分层正确**（吸取 [[presentation-gdd-timing-ac-headless-trap]]）：纯 fixture 确定性 [Logic·BLOCKING](20 条);多系统存→读→比对 headless 可跑 [Integration·BLOCKING](AC-1/4/9/10,4 条);真机磁盘 IO / code-review [Advisory](AC-25/26,2 条)。`Cash` 经容器写回不触发金钱事件用 spy 计数==0 反证(AC-3),非渲染时刻误标。
- **关键架构裁定无环**：`bIsBankrupt` 归回合2 PlayerState(破产9 无独立存档腿);`house_count` per-tile [0,5] 归建房8 非6(防 6↔8 环);棋盘存 DA 引用 `BoardIdentifier` 不存全量布局(CR-2);MVP 不序列化骰子 `Seed`(EC-8 防读档后序列可复现重复);F-2 严格相等版本判据不迁移(MVP 诚实立场)。

### Verified Blockers（收口）
无。`verified_blockers=[]`。

### Deflated（专家提出但经主审核实非真 BLOCKING）
- **building carrying contract 错误** → 经主审 fresh-grep 复核:carrying contract(`house_count` per-tile [0,5]/键/range)本身对齐 building L57 正确、搬运契约无误;真问题仅是 L185 把 building 误列入"已 grep 核实存在"侧的**假覆盖声明**(building 反向行实际零命中),降为 logged decision #2 就地校正,非 BLOCKING([[stated-coverage-falsification-is-needs-revision]] 但有界、carrying 正确故不拦)。
- **OQ-Save-1 USaveGame 宿主未定** → 已正确登记 ADR-级 OQ(与回合2 OQ-1 协调),非臆造为已定接口洞;`UGameplayStatics::SaveGameToSlot`/`USaveGame`/`UPROPERTY(SaveGame)` 是 pre-5.3 稳定 API,UE5.7 不臆造新 API,实现注归 ADR。
- **F-3 校验和不防语义错** → GDD 已自陈校验和只防字节完整性、不防 owner 字段语义(那是各 owner 不变式职责),非缺口而是正确的职责边界声明。
- **OQ-Save-4 不做版本迁移** → MVP 严格相等拒绝(EC-1)是诚实立场,迁移成本高/易错 deferred 到格式稳定后,非过度规格化残渣。

### Logged Decisions（finishing-class,纯文档收紧,不改 carrying contract,不阻开工）
1. **AC-1 round-trip identity 金标准清单 + CR-3 玩家回合行显式补点名 `CurrentTileIndex`(per-player,owner=移动4)。** 来源 qa-lead R-1:`CurrentTileIndex` 是 player-turn L83 PlayerState 11 字段之一、player-turn L389 明文要求读档逐一还原全 11 字段;但 save-load AC-1 显式 identity 枚举清单(Cash/owner map/bIsMortgaged/house_count/PlayerState/CurrentPlayerIndex/CurrentPhase/ConsecutiveDoubles/阈值/FDiceRollResult)漏点名 `CurrentTileIndex`。虽 AC-1 同句"CR-3 表全部字段 + 全 PlayerState"形式覆盖(superset),但若实现者照显式清单逐项写测试,棋子位置可能漏断言→读档后回退默认 0(隐形损坏,正是本档信任底线要防的)。**已应用**:CR-3 玩家回合行 11 字段全列展开点名 `CurrentTileIndex`(去"等"字吞位)+ AC-1 显式清单括注 `CurrentTileIndex`。可逆:纯文档收紧,不改 carrying contract,只补显式枚举消除测试覆盖盲区。
2. **修正 L185 双向一致性登记:把 building 从"已 grep 核实存在"清单移到 OQ-Save-5 待补反向引用侧(与主菜单20/呈现层并列)。** 来源 unreal R-1 / game-designer R-1 + 主审独立 fresh-grep 复核确认:building-upgrade.md 全文 grep "存档|序列化|(21)" 对 Dependencies 反向行 = 零命中(仅命中 F-9 house_count 枚举,与持久化无关);其余 5 owner(board L164、player-turn L252、economy L114/L332、property L97/L114、dice L209)反向行均真实存在。save-load L185 把 building 列入"撰写前已 grep 核实存在"侧 = over-claim 假覆盖。carrying contract(house_count per-tile [0,5] 字段名/键/range)本身核实与 building L57 一致、搬运契约正确,故非 BLOCKING,但假覆盖声明须就地校正以免误导下次 review。**已应用**:L185 building 移到待补侧 + 标注 fresh-grep 零命中。可逆:措辞校正;实际反向行补全交 producer propagate(OQ-Save-5)。
3. **OQ-Save-2 fallback 自判路径收口:MVP 默认锁权威路径,fallback 标临时降级。** 来源 systems R-2 / unreal R-3:CR-4 裁决"可存档点最终判据归回合2、本系统不自裁阶段语义",但 OQ-Save-2 又给"本系统据 CurrentPhase∈{...}自判"fallback;AC-13/14/15 全部以"回合2 报告可存档/非可存档"为 GIVEN(正确对齐权威路径),fallback 自判谓词无 AC 守护。player-turn grep `bIsSafeToSave`/`CanSaveNow`=0 命中,确认回合2 尚未提供该接口,fallback 是真空兜底而非临时占位。若 fallback 真被实现则 save-load 复制了它声明不拥有的回合2 阶段语义(enable-not-own 轻度越权)。诚实开口非 BLOCKING,但需明确 MVP 默认路径避免 enable-not-own 声明与实现自判矛盾。**已应用**:OQ-Save-2 标注 MVP 默认锁权威路径 + fallback 为临时降级/无 AC 守护/回合2 接口到位后移除 + fresh-grep 接口未提供事实。可逆:接口待定属诚实开口;裁决方向(锁权威路径 vs 给 fallback 补 AC)留架构期 ADR 收敛。

### Followups（producer/架构期协调,不阻本档开工）
1. **OQ-Save-5**（producer `/propagate-design-change`）：补全反向引用——① building-upgrade.md Dependencies 加"存档(21)"行(随格 house_count 被序列化);② 主菜单20 "继续游戏"查 `DoesSaveGameExist(SLOT_DEFAULT)` + 触发 `LoadGameFromSlot`/`SaveGameToSlot`;③ 呈现层 HUD16/VFX19/property card17 监听 `OnGameLoaded` 信号读档后重绑各自 delegate(CR-6)。fresh-grep 双侧核实落地。
2. **OQ-Save-1 + OQ-Save-2**（架构期 `/architecture-decision`,开工前）：`USaveGame` 宿主 + 同步 vs 异步写盘 + 临时文件→原子替换平台实现(与回合2 OQ-1 协调:回合状态机用 `ETurnPhase` 枚举字段 + delegate、禁 Latent Action,本档读档重建依赖);回合2 可存档点查询接口具名(`bIsSafeToSave`/`CanSaveNow`),定回合2 接口删 fallback 或给 fallback 补 AC。
3. **OQ-Save-3/4/6**（Alpha/Beta/Full Vision）：多槽/云存/autosave、版本迁移、联网存档语义——MVP 不展开,占位入口不臆造下游接口。

### 收敛信号
blocker 量首评即 0（verified_blockers=[]）= 健康收敛终结。lean 持久化横切层 GDD,enable-not-own 边界正确 + 核心防漏机制(CR-3 契约清单 + round-trip identity 金标准)健全 + 继承义务全落 AC + AC 诚实分层 + 关键架构裁定无环。3 logged decision 均 finishing-class 纯文档收紧(显式清单补字段 / 假覆盖校正 / fallback 收口),一次性就地应用即可,不退回全量复审。直接标 **Approved**。

### AC 计数
26 AC：20 [Logic·BLOCKING]（AC-2/3/5/6/7/8/11/12/13/14/15/16/17/18/19/20/21/22/23/24）+ 4 [Integration·BLOCKING]（AC-1 全状态 round-trip / AC-4 精确阶段还原 / AC-9 事件可达 / AC-10 阶段 delegate 重绑,多系统往返 headless 可跑）+ 2 [Advisory]（AC-25 code-review 序列化标记+版本递增纪律 / AC-26 真机磁盘 IO 冒烟）。

---
