# Review Log — 经济与现金 (Economy & Cash, #5)

> 追踪本 GDD 历轮 design-review 裁定与修订，供 fresh re-review 核对前轮 blocking 是否闭合。

## Review — 2026-06-02 — Verdict: NEEDS REVISION → 修订已应用，待 fresh re-review
Scope signal: M（修订范围；GDD 实现范围 L — 11 公式、bottleneck、~14 下游依赖）
Specialists: economy-designer / systems-designer / qa-lead / game-designer / creative-director（资深综合）
Mode: full（首轮 fresh review，5 agent 对抗性）
Blocking items: 4 真 blocker + 1 删减 | Recommended: 多条（本轮未做，留 re-review/后续）
Prior verdict resolved: 首轮（无前轮）

### CD 裁定的最小 blocking 集（用户授权"现在就改"，已全部落规格）
1. **AC-27 循环同义反复**（systems-designer + qa-lead 同根命中，最高风险）→ 重写为三可证伪断言（F-9 数值 / F-11 资产 identity-check / 单一枚举入口 code-review）；F-9/F-11/AC-29 同步澄清"非财务等价"。
2. **F-1 passed_go 溢出击穿 Cash≥0**（systems-designer）→ F-1 运行时 clamp [0,1000]（非仅 dev ensure）；CR-1 加 Credit/Debit amount≥0 assert 契约；AC-8/AC-3 同步。
3. **F-2 建首房可能降租**（systems-designer）→ F-2 数据包络约束 `RentTable[1] ≥ RentTable[0]×monopoly_rent_multiplier` + Edge Case；校验派发棋盘(1)（OQ-Econ-9 / propagate-design-change）。
4. **CR-7 Raising Funds 无界阻塞**（systems-designer + qa-lead + game-designer 三 lens）→ CR-7.3 确定性自动清算兜底（MV 升序、先卖房后抵押、退化到破产，**用户裁定：自动清算→退化到破产**）；States 表 + AC-43 终止性；触发时机 defer（OQ-Econ-8）。
5. **删减抵押套利论据**（economy-designer，过度工程化的镜像）→ 删除套利轴 + 重述 mortgage_warning_threshold 为数据质量警戒线；AC-42 由 Advisory 升 Logic（数学证伪"套利回报趋正"）；registry 2 处 notes 更正。

### CD 对 3 个专家分歧的裁定
- **翻盘严重度**（game-designer BLOCKING vs economy-designer RECOMMENDED）→ **降为 CONCERN**：翻盘工具全在 Alpha 系统、本档无法解决、已诚实 flag，只需 framing 标注（本轮未做，留后续）。
- **OQ-Econ-6 Rento vs 经典数值**（economy-designer 判 BLOCKING）→ **不升 BLOCKING，硬化 OQ 措辞为 PROVISIONAL-CLASSIC**：公式结构数值无关，错数值不会让 GDD 崩；前置调查已正确登记（本轮未硬化，留后续）。
- **抵押套利**（economy-designer 判过度工程化）→ **CD 完全支持**，已执行删减（blocker #5）。

### AC 计数变化
首轮 43（39 Logic / 4 Advisory）→ 修订后 **44（41 Logic / 3 Advisory）**：AC-42 Advisory→Logic、新增 AC-43。

### 本轮未做（留 re-review 或后续，非 blocking）
- qa-lead Recommended：AC-44/45/46（破产后停止交互、银行无限池）、AC-4 升级门、AC-41 拆分、AC-40/42 量化。
- systems-designer Recommended：F-4 dice_total clamp、fee_den/sell_den≠0 校验、F-8 sellback=0 文档、F-3 RentTable[0]>0。
- economy-designer Recommended：flat 税 honesty、faucet/sink 量化、F-9 保守估值标注、F-11 税/收租破产不对称退化激励（CD 未纳入最小集）。
- game-designer Recommended：翻盘 MVP 降级标注、Player Fantasy framing 升 OQ-Econ-8↑、MVP 策略极空心标注、is_mortgaged 竞态 Edge Case。

### 证据文件
- `design/gdd/economy-cash.md`（11 处 Edit + Status 头）
- `design/registry/entities.yaml`（3 处 Edit）

---

## Review — 2026-06-02（re-review）— Verdict: APPROVED-WITH-FOLLOWUPS
Scope signal: S（本轮修订 2 处 blocking honesty 编辑 + 同批软化；GDD 实现范围仍 L）
Specialists: economy-designer / systems-designer / qa-lead / game-designer / creative-director（资深综合）
Mode: full（5 agent 对抗性 re-review）
Blocking items: 2（均落规格）| Recommended: 8（留后续）| Followup propagate: 1（P-1）
Prior verdict resolved: **是 — 5 前轮 blocker 经算术层独立核验全部闭合**

### 前轮 5 blocker 闭合核验
1. AC-27 循环同义反复 → 闭合（②F-11 identity-check 真可证伪、独立于 F-9；fixture nlv=200 三专家+主审核验）
2. F-1 溢出 → 闭合（passed_go 路径 runtime clamp）；残留 SalaryAmount 无上界 + "≪2³¹"措辞 → 本轮修(P-2/OQ-Econ-10)
3. F-2 建首房降租 → 闭合（[0]→[1] 包络；[1]→[5] 由 board-data RentTableNotMonotonic 覆盖,已核验）
4. CR-7 无界阻塞 → 闭合（资产有限⇒自动清算必终止）；残留 even-sell 顺序 + AC-43 fixture → Recommended
5. 删减套利 → 经济 GDD 内 + registry 已闭合；残留 board-data 套利措辞 → P-1 propagate **✅ RESOLVED 2026-06-03**

### R2 落规格的 2 条 blocking（均诚实硬线）
- **B-1**（line 31）：Player Fantasy "交易"标〔Alpha〕——防 Alpha 能力混入 MVP 幻想契约。
- **B-2**（line 39 后追加）：落实首轮 CD 约定但未写的 MVP 翻盘 framing 声明（主动翻盘工具全在 Alpha,MVP 仅抵押延命+收租咬住）。
- 同批：P-2 line 131 "≪2³¹"诚实化 + 派发 SalaryAmount 校验（OQ-Econ-10）；faucet line 259 软化为"预期/pending playtest/依赖 OQ-Econ-6"。

### CD 对两个新专家分歧的裁定
- **AC-42 是否冗余**（qa-lead 判可由 AC-19+AC-21 代数推导→降 Advisory vs 首轮 CD 升 Logic）→ **维持 [Logic]**：它是删减套利轴后唯一机器可验证的反退化回归哨兵,derived 但守护"被删机制不复活",挣得 slot；措辞收窄为 `derived·回归哨兵`（随手硬化）。
- **翻盘 framing 严重度**（game-designer 判 BLOCKING vs 首轮 CD CONCERN）→ **拆分裁定**：空心翻盘维持 CONCERN（无新证据,Alpha-bound 结构不可解）；但 game-designer 撞到的 line 31 Alpha 泄漏是**独立诚实硬线**=B-1,采纳；翻盘升级驳回。

### 本轮未做（留后续,非阻塞下游开工）
- ~~P-1：board-data line 271/331/337 + registry line 599 套利残留措辞 → `/propagate-design-change`。~~ **✅ RESOLVED 2026-06-03**(实际命中 board-data line 272/273/332/338/403 + entities.yaml line 646;旧记行号已漂移、且漏列 line 273 套利理论整段与 entities.yaml:646 注释。全部 reframe 套利→数据质量/数据错误,对齐经济 AC-42;grep 核验"套利"仅余否定式)。
- Recommended AC：CR-4 买地无 AC、AC-43 变体 A/B 喂定值不全、破产终态隔离 AC（宜归系统9）、F-4 dice_total clamp、F-6/F-8 fee_den/sell_den≠0 校验、AC-29 标签矛盾、AC-41 调参注记、F-9 nlv 上界推导。

### 证据文件
- `design/gdd/economy-cash.md`（B-1 line 31 / B-2 line 39 / P-2 line 131 / faucet line 259 / OQ-Econ-10 / Status 头 — 共 6 处 Edit）
- `design/gdd/systems-index.md`（#5 Status→Approved* + 脚注 + Progress Tracker reviewed/approved 3→4）
