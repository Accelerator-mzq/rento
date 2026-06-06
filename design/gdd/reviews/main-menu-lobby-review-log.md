# 主菜单与房间大厅（#20）Design Review Log

> 本文件记录 `design/gdd/main-menu-lobby.md` 的 design-review 修订历史,供 fresh-session 再审追踪。

---

## Review — R-1 — 2026-06-06 — Verdict: APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕
Scope signal: S（呈现/编排层纯叶子,2 公式〔F-1 谓词/F-2 打包〕,1 硬依赖〔回合2〕+ 1 间接依赖〔AI10 经回合2 字段〕,无新架构、无运行态、无环;blocker 量首评即 0）
Depth: full（`/design-review` fresh session,5 specialist + creative-director synthesis）
Prior verdict resolved: 首评（无 prior log）

### Verdict
**APPROVED_WITH_FOLLOWUPS** — `verified_blockers=[]`、`logged_decisions=[]`。无 in-doc BLOCKING、无跨档 must-land、无架构返工、无 pillar/身份重裁定。遗留全为 OQ 上游/产品决策 + UX-spec/Asset-spec flags,均不阻开工。

### 关键发现（5 专家收敛 — 无真 BLOCKING）
- **纯叶子约束干净**：#20 同 HUD CR-14 模式——只在 PreGame 阶段存在,收集本地热座 2–4 配置 → 构造 `FGameSetupConfig` → 调回合2 `StartNewGame` 移交退场;不持 `PlayerState`/不写运行态/不定序/不掷骰/不被对局反调(#20→#2 单向无环)。CR-1/CR-5/AC-16/AC-17 对此有可证伪断言(调用图 spy 无掷骰/定序/反调符号)。
- **enable-not-own Fantasy 边界正确**（吸取编排层教训 [[orchestration-layer-fantasy-enable-not-own]]）：Player Fantasy 仅限"开局前低摩擦预期与欢迎感",明确**不拥有**对局爽感(掷骰/收租/建房 juice 归 #19/HUD#16);入场动画/呼吸体验契约登记给 art-bible 状态 A + Pre-Production UX spec(AC-24 [Advisory·playtest-signoff]),不在 AC 断言"感觉良好"。
- **继承义务全覆盖**：作为下游无 "Inherited Test Obligations" 表登记给 #20 的条目(该表无 #20 行);作为上游契约履行方——player-turn 主菜单大厅(20) 依赖(L251/L419)→ AC-13/14/15、ai-opponent L340 难度选择 UI 归 #20 写 `PlayerState.AIDifficulty`(字段归回合2/行为语义归 AI10 F-7)→ AC-7/10/14/20、player-turn AC-27 越界 P 双层防守对齐 → AC-2/3/15。qa-lead 已核对全部落 AC。
- **AC 诚实分层正确**（吸取 #16/#19 headless 陷阱 [[presentation-gdd-timing-ac-headless-trap]]）：配置谓词/打包逻辑 [Logic·BLOCKING](F-1 AC-1..8 / F-2 AC-9..12 / 交互谓词 AC-18/19/20);移交跨系统 [Integration·BLOCKING](AC-13/14/15);呈现/渲染时刻一律 [Advisory·code-review/playtest-signoff](AC-21/22/23/24)。无渲染时刻误标 Logic/Integration false-green。
- **MVP-minimal 收敛**（吸取 #10 过度规格化教训）：联网房间(#25)/House Rules 面板(#23)/手动 turn order(#23)/存档续局(#21)/全 AI 禁止(Alpha)全降为 OQ 占位,不写 MVP 正文,避免在未成形下游系统上制造未兑现接缝;占位入口 MVP 禁用/隐藏(CR-6/EC-8/AC-23)。

### B-1 名字段对齐（已采 Option A，全档对齐 owner）
玩家名 owner 权威字段=`PlayerState.DisplayName:FText`(player-turn L79,**非** `PlayerName/FString`)。#20 setup 层为支持文本框逐字符编辑内部用 `FString` 持名(编辑态自然),仅在 F-2 `BuildSetupConfig` 边界经 `FText::FromString` 转 `FText` 写入 `FPlayerSetupEntry.DisplayName`,再由回合2 直填 `PlayerState.DisplayName` 无再转换。F-1 C2 散文注明 `NameOf` 取 setup FString 编辑态(FText 无 `Len()` 须先 `.ToString()`)。AC-10/AC-13 断言字段名为 `DisplayName`、类型 `FText`、`.ToString()` 等值。〔可逆:若产品后确认统一 `PlayerName/FString` 切 Option B,由 producer 改 player-turn L79 owner 并回链——本档默认不锁死。〕

### CD 综合裁定
首评即高质量收敛 lean GDD:纯叶子约束清晰、enable-not-own 边界正确、继承义务全覆盖、AC 诚实分层、MVP 范围严格收敛。**无 in-doc blocker、无跨档 must-land、无架构返工、无 pillar/身份重裁定**——所有未决项均为已正确登记的 OQ 上游/产品决策 + 标准 UX/Asset flags。Verdict: **APPROVED_WITH_FOLLOWUPS**。

### Verified Blockers（收口）
无。`verified_blockers=[]`。

### Deflated（专家提出但经主审核实非真 BLOCKING）
- 难度三档行为差异/AI 难度语义 → 非 #20 范畴,行为语义归 AI10 F-7(ai-opponent L340 已明确),#20 只写值;非缺口。
- 入场动画/呼吸"感觉良好" → 已正确登记为 art-bible 状态 A + UX spec playtest-signoff 体验契约(AC-24 Advisory),enable-not-own 边界守住,非 #20 须在 AC 断言的内容。
- 回合2 开局入口未具名 → 已正确登记 OQ-Lobby-1(owner=回合2,#20 作调用方 propagate),AC-13 接口名待 OQ 闭合冻结,非臆造为已定的活契约洞。
- 续局/联机/House Rules 接口 → 已正确降 OQ 占位 + MVP 入口禁用/隐藏,无半成品接缝(非过度规格化残渣)。

### Logged Decisions
无。`logged_decisions=[]`。（B-1 Option A 已在正文落定并双向回链,非待裁定 logged decision;OQ-Lobby-1..7 为已登记 OQ 非本轮 logged decision。）

### Followups（producer/Pre-Production/架构期协调,不阻本档开工）
1. **OQ-Lobby-1**（producer `/propagate-design-change`,设计冻结前）：回合2 落定开局入口签名 `StartNewGame(const FGameSetupConfig&)` + `FGameSetupConfig`/`FPlayerSetupEntry` USTRUCT 归属(建议归回合2,与 PlayerState 构造逻辑同处) + 确认 player-turn L79 字段名/类型未变并双向回链 `FPlayerSetupEntry.DisplayName:FText`。AC-13 接口名待此闭合冻结。
2. **OQ-Lobby-7**（producer 轻量核对,设计冻结前）：确认回合2 `EPlayerColor` 8 色枚举值与 art-bible §5.2 P1..P8 玩家色一一对应(命名/顺序),避免大厅默认 `DefaultColorBySeat[8]` 与回合2 枚举不匹配。
3. **OQ-Lobby-2/3/4/5/6**（下游设计/产品决策阶段）：续局读 #21(接口未定)、House Rules 面板 #23 Alpha、手动 turn order #23 Alpha、联网房间 #25 Full Vision、全 AI 禁止 Alpha 产品决策——MVP 占位入口禁用/隐藏,不臆造下游接口。
4. **UX-spec flag**（Pre-Production `/ux-design`）：为 主菜单屏 + 房间大厅配置屏各写 UX spec 到 `design/ux/main-menu.md`/`design/ux/lobby.md`,AC-21/22/23/24 呈现层验收在 UX spec 细化,story 引 UX spec 非直引本 GDD。难度选择屏 UX 归 #20。
5. **Asset-spec flag**（art-bible approved 后 `/asset-spec system:main-menu-lobby`）：产出主菜单/大厅 UI 资产规格(背景/`WBP_`/`T_ui_*`/席位卡片/颜色选择器/难度档图标/占位入口禁用态),遵 art-bible §7/§8。

### 收敛信号
blocker 量首评即 0（verified_blockers=[]）= 健康收敛终结。lean 呈现层叶子 GDD,继承义务全落 AC + enable-not-own 边界正确 + AC 诚实分层 + MVP 范围严格收敛,无任何 in-doc/跨档 must-land。直接标 **Approved**。

### AC 计数
24 AC：16 [Logic]（含 AC-2/3/15 越界防守、AC-1/4/5/7/8 配置合法性、AC-9/10/11 打包、AC-16/17 无环退场、AC-18/19 交互谓词）+ 3 [Integration·BLOCKING]（AC-13 开局移交 / AC-14 默认值对齐 / AC-15 防守层对齐〔标 Logic·BLOCKING,断言大厅侧不静默接受越界 P〕）+ 4 [Advisory]（AC-21/22/23 code-review、AC-24 playtest-signoff）+ AC-6/12/20 Recommended。

---
