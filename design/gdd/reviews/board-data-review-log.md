# Review Log — 棋盘数据 (Board/Tile Data)

> 本文件记录 `design/gdd/board-data.md` 的历次 design-review,供再审对照"上次判什么、blocking 是否解决"。

## Review — 2026-06-01 — Verdict: NEEDS REVISION → 已修订(待复审)
Scope signal: M
Specialists: systems-designer, game-designer, economy-designer, qa-lead, unreal-specialist + creative-director(综合裁定)
Blocking items: 7 | Recommended: 11 | 已全部应用修订

Summary: full 模式对抗审查。核心扎实——CR-6 的 40 格布局逐格对标准大富翁盘**完全正确**,F-1/F-2/F-3 经三方独立复算**数学无误**,只读/可变状态分离架构正确。CD 判 NEEDS REVISION(非 MAJOR):所有 blocking 都是 7 个未成形下游系统将消费的**接口契约精修**,无结构性缺陷,可原地修复。

### 已解决的 BLOCKING(本次修订应用)
1. **GO 双重发薪** — CR-2 新增发薪权威契约:`passed_go` 唯一授权,Go 格 `CollectSalary` 仅 UI 标记。
2. **Utility RentTable 单位混用**(3 agent) — 拆出独立 `DiceMultiplierTable` 字段(CR-3/CR-4);+AC-4b/AC-5/AC-23f/AC-23g。
3. **垄断翻倍系数归属** — CR-4 契约:经济全局常量 ×2,棋盘只存未翻倍 base。
4. **OQ-6 延迟 + GetBoardId 脆弱**(3 agent) — OQ-6 升级 ⚠ BLOCKING-ADR(下游开工前解决);GetBoardId → DA 内 `BoardIdentifier` 字段与路径解耦(AC-28)。
5. **AC 契约缺陷**(qa-lead) — AC-24 拆 24a/b/c + 结构化警告;AC-32 `[Config]`→`[Logic]`;新增 AC-34;AC-25→`[Advisory]`;AC-28/31 合并。
6. **手波枚举 + 缺校验** — `ETileAction` 封闭枚举;FreeParking 锁 `None`+预留 `TriggerHouseRuleCheck`;新增 AC-23b/c/d(EventDeck/无Jail/Go数)。
7. **Blueprint 实现陷阱**(unreal) — F-1 强制 `((A%N)+N)%N`;F-2 强制 `Floor(float/float)`。

### CD 裁定的两项分歧
- **RentTable 单调性**(全 5 agent 提,economy 要严格拒绝 vs systems/game 要 WARN):CD 判 **WARN 非 reject**(单调性是平衡属性归经济#5/playtest,自定义棋盘允许)+ 新增 AC-35 + 结构化警告码。
- **货币值全量延迟**(game-designer):CD 判 **temp-fill 经典公开参考值**(数值不受版权保护),解除 AC-4/AC-7 阻塞,棋盘可独立跑通;经济#5 后续覆盖。

### 用户拍板的 4 项 schema/机制决策
- Utility:独立 `DiceMultiplierTable` 字段
- 翻倍系数:经济全局常量
- GO 薪水:Go 格加 `SalaryAmount` 字段(对称 TaxAmount;OQ-2 已解决)
- GetBoardId:DA 内显式 `BoardIdentifier: FName` 字段

### 附加修复(Recommended)
色组命名锁定(粉→Magenta)+ hex 色键契约(CR-5.1);MortgageValue >100% 拒绝/>60% 警告;F-3 `from==target` + 传送事件发薪契约;GetTilesInGroup 显式 Sort;AdvanceIndex 原子性;DA 运行时持有者(GameInstanceSubsystem);`[Config]` 仅 nightly(传 devops);F-2 证明文本修正;同组 BuildingCost 警告;起始现金比例提示;OQ-7 本地化迁移成本。

Prior verdict resolved: First review.

### 复审待确认重点
- temp-fill 的占位数值是否已实际填入 `DA_Board_Classic40`(本次仅在 GDD 文档锁定 temp-fill 策略,资产填值属实现动作)。
- OQ-6 的 BLOCKING-ADR 是否需在本阶段即启动 `/architecture-decision`(下游 #4/6/7/8 开工前必须)。

---

## Review — 2026-06-01（第二轮）— Verdict: NEEDS REVISION → 已修订(待 fresh re-review)
Scope signal: L
Specialists: systems-designer, game-designer, economy-designer, qa-lead, unreal-specialist + creative-director(综合裁定)
Blocking items: 9 项 GDD 本体 + 2 项归 OQ-6 ADR + 1 项概念 carry | Recommended: 多项 | 已全部应用修订

Summary: 第二轮 full 对抗复审。布局/公式拓扑两轮验证依旧扎实(无返工)。CD 判 NEEDS REVISION(与上轮同级但**性质已变**:上轮=为未成形下游精修接口;本轮=修同一份 GDD 内部三处自相矛盾)。三方独立命中暴露一簇真·契约自相矛盾与可测性 gap,全是"改而不重设计"的收口,够不上 MAJOR,但含自相矛盾接口契约与漏守 BLOCKING AC,不可带病 APPROVED。

### 跨专家高置信信号(多人独立命中)
1. SalaryAmount 读取路径三处矛盾 + 非 Go 反向校验缺 + registry 漏注(systems R-03/R-05 + qa + economy 三方)。
2. AdvanceIndex 原子契约自相矛盾、无 AC(systems R-04 + qa B-4)。
3. 热切换竞态/状态机缺失(systems R-06 + unreal N-2)。
4. temp-fill 无强制覆盖门控(economy #3 + game-designer)。
5. OQ-6 倾向 DataTable 的 CSV 论据撞 TArray 限制(unreal N-1,技术确凿)。

### 用户拍板的 3 项接口/语义决策
- 发薪读取路径:移动(4)读 SalaryAmount + passed_go 传经济(5),事件格(7)不读。
- F-2:恒用通用式 floor((from+steps)/N)-floor(from/N)(对 from 越界鲁棒,消 Shipping ensure 失效)。
- AdvanceIndex:原子单调用返回 (newIndex, passed_go) 元组,无独立公开 PassedGoCount。

### 已应用的 BLOCKING 修订(R2)
CR-2 发薪读取路径契约;CR-3/Interactions/Edge Cases 数据流一致;registry 注册 go_salary_amount/tax_*;F-2 通用式;temp-fill 硬门控(bIsPlaceholderData + OQ-1 关闭条件);AC-4 拆 Logic/Config;+AC-22b/23g-c/23h/23i/36/37/38/39;steps range 修正 + passed_go≤0 消费契约;CR-6 IP 口径对齐 concept;States LoadFailed + 热切换边界;OQ-6 扩充(热切换协议/签名/4 知识缺口);Player Fantasy 支柱① CONCERN-carry 脚注。

### CD 裁定的归属调整(与专家相左处)
- game-designer 的 Player Fantasy "概念 BLOCKING":CD 降为 CONCERN-carry(framing 对纯数据系统恰当;深层支柱①风险归 concept/art-bible,非 board-data schema)。
- game-designer 的 IP/Trade Dress "BLOCKING":CD 裁为措辞对齐(收窄措辞而非补全分析;Trade Dress 完整声明归 art-bible/concept)。
- 热切换/DataTable-TArray/签名定型:CD 裁为偏实现层,归已存在的 OQ-6 BLOCKING-ADR,不反推 GDD 重写。

Prior verdict resolved: Yes —— 上轮(第一轮)7 项 BLOCKING 经本轮确认全部生效;本轮为新发现的内部契约自相矛盾。

### 复审纪律(CD 要求)
- 必须走 **fresh re-review(新会话)**,不接受同会话自述"已修复"。
- SalaryAmount/AdvanceIndex 这类契约修订需**逐行对照**,不继承本轮判定。

### 复审待确认重点(第二轮)
- SalaryAmount 数据流三处(CR-2/CR-3/Interactions)是否已彻底一致、无残留旧措辞。
- AdvanceIndex 原子契约(Interactions/Dependencies/§B/AC-37)是否四处一致。
- F-2 通用式与 AC-12~15(from 在 range 内)结果是否仍一致。
- temp-fill 硬门控(bIsPlaceholderData)闭环是否完整、registry 占位条目是否就位。
- OQ-6 的 4 项 UE5.4-7 知识缺口是否需在 ADR 前先做人工文档核验。

---

## Review — 2026-06-01（第三轮，fresh re-review）— Verdict: APPROVED（R3 修订后接受）
Scope signal: S–M（全部为文档/registry/AC 文字层收口，无结构性重设计）
Specialists: systems-designer, game-designer, economy-designer, qa-lead, unreal-specialist + creative-director（综合裁定）
Blocking items: 5（含用户裁定升级的 #5）| Recommended: 15 应用 + 1 有意 defer | 全部已应用修订

Summary: 第三轮 full fresh re-review（CD 要求的契约级修订复审）。主审独立核验:F-2 通用式手算对 AC-12/13/14/15/33/29 全部一致;SalaryAmount/AdvanceIndex 数据流四处一致。本轮判 NEEDS REVISION,但 CD 关键画像——**5 项 BLOCKING 全部是 registry/AC/OQ 元数据未跟上 GDD 本体已正确做出的决策,零项本体设计缺陷**,精确画像"收口"而非"带病放行"也非"重设计"。GDD 本体(布局/F-1F-2F-3/状态分离/数据流契约)三轮验证扎实自洽。R3 修订当会话应用完毕,CD 判定不需再开 fresh re-review,用户接受、标 APPROVED。

### 5 项 BLOCKING（已应用）
1. registry `passed_go_count` expression 简化式 `floor((from+steps)/N)` 未同步 R2 通用式决策（systems B-1 + economy I-4 独立命中，主审对照 entities.yaml:136 确认）→ 改通用式 + revised:2026-06-01。
2. OQ-2"已解决"条目残留"事件格(7)读 SalaryAmount"，与 R2 决策 CR-2/CR-3 直接矛盾（CD 升级，fresh re-review 逐行对照命中）→ 改"移动(4)读传经济(5)"。
3. `bIsPlaceholderData` 硬门控无任何 AC 断言，纯靠 producer 人工记忆（economy I-1，主审遍历 AC 确认）→ 新增 AC-4c [Config] gate_level:alpha_blocking。
4. AC-37 把"断言独立 PassedGoCount 接口不存在"标 [Logic]，运行时无法断言函数不存在（qa B-1）→ 拆 AC-37a [Logic] + AC-37b [Advisory]。
5. **（用户裁定升级）** 跨系统测试责任真空：AC-38/39/28b 推给未成形下游却无追踪机制，与 bIsPlaceholderData 软门控同构（qa B-2）→ 新增 OQ-8 追踪 + §AC 回链要求（下游 GDD 须回链 AC 编号，design-review 守门）。

### 主审 + CD 裁断的两处专家分歧（均降 RECOMMENDED）
- systems B-3 "F-3 from==target 传送死锁"判 BLOCKING：line 227 已明文委派调用方裁决，禁 steps=0 是语境性约束 → 降级（未单列修订，契约本已自洽）。
- game-designer "temp-fill 值源 Monopoly vs Rento"判 BLOCKING：placeholder framing + 硬门控已托底，补"占位数据 playtest 不作支柱③有效证据 + 标明值源(经典大富翁公开值)"即可 → 降 RECOMMENDED 并应用。

### 15 项低成本 RECOMMENDED（同批应用）
F-3 Blueprint 负数取余约束;AC-39 [Advisory→Integration]→[Advisory];temp-fill 值源+playtest 提醒;int32 溢出/int除法截断陷阱;AC-34 补返回断言;FName 大小写陷阱 AC-28;OQ-6 ADR 清单扩 6 项(Subsystem handle cancel/struct vs out-param/bIsPlaceholderData strip/CSV-TArray 编辑器输入/NSLOCTEXT 张力);N=4 角格唯一性警告+AC-24d;抵押套利赎回溢价前提+注册 mortgage_warning_threshold;注册 monopoly_rent_multiplier+CR-4 修正;registry placeholder 状态语义;CONCERN-carry 可检验接管前提;CR-6 四边方位映射;结构校验≠可玩性/编辑器26设计债;命名 snake/Pascal 统一注。

### 有意 defer（1 项，记入 state）
game REC-3（bIsPlaceholderData==false 后色组成员数从警告升拒绝）：未应用。理由——unreal R3-9 指 bIsPlaceholderData 应 editor-only/Shipping strip;flag-coupled 校验与 strip 冲突,须 OQ-6 ADR 先定该字段落地形态。

Prior verdict resolved: Yes —— 第二轮 9 项 + 用户 3 决策经本轮逐行核验全部生效;本轮 5 项为新发现的元数据漂移 + 1 项用户升级,均已收口。

### 遗留（非本 GDD 阻塞，下游开工前处理）
- **OQ-6 ⚠ BLOCKING-ADR**：下游 #4/6/7/8 实现开始前须 /architecture-decision（DataTable vs Primary Data Asset，R3 已纳入 6 项实现层问题 + 4 项 UE5.4-7 知识缺口须人工查 5.7 文档）。
- **OQ-8**：经济(5)/事件(7)/存档(21) GDD 成形并 APPROVED 时核对 AC-38/39/28b 回链。
