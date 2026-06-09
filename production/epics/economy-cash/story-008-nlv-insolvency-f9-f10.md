# Story 008: NLV F-9 + F-10 is_insolvent + F-8 卖回 🔴最高风险

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: 2026-06-09

## Context

**GDD**: `design/gdd/economy-cash.md`（F-8 / F-9 / F-10）— 🔴 F-9 最高风险契约
**Requirement**: `TR-econ-012`（preaggregated_nlv）、`TR-econ-013`（AI snapshot 读 note）、`TR-econ-014`（逐栋 floor 确定性）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0014（逐栋 floor 先于求和 / 确定性，primary）· ADR-0006（preaggregated_nlv 回合2 装配）
**ADR Decision Summary**: F-9 NLV 建筑卖回 = `Σ_t house_count[t] × floor(BuildingCost[t]/2)`（**逐栋 floor 先于求和**，奇数 BuildingCost 差1）；F-10 `is_insolvent` 纯谓词消费 `preaggregated_nlv`（由回合2 聚合 6 MV + 8 sellback 传入，**economy 不直读8**，防 5→8 环）。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW（引擎），🔴 HIGH（数值契约风险 — AC-27）
**Engine Notes**: 整数 floor；`is_insolvent` 严格 `<`。`-nullrhi` 可测（mock preaggregated 输入）。

**Control Manifest Rules (Core/ADR-0014/0006)**:
- Required: 逐栋 floor 先于求和；`is_insolvent` 严格 `<`；`PreaggregatedNlv` 由回合2 装配（AI 严禁自算）；单一资产枚举入口。
- Forbidden: 合并取整 `floor(Σ)`；economy 直读建房8 `house_count`（5→8 环）；nlv 现算触达 F-9 反向环（ADR-0006）；用 float。
- Guardrail: 单玩家独占全盘 nlv ~10⁵ ≪ 2³¹（int32 安全）。

---

## Acceptance Criteria

- [x] **AC-24** [整数 floor] `sellback=floor(BuildingCost×sell_num/sell_den)`（F-8）：BuildingCost=100, ratio=1/2 → 50；BuildingCost=75 → floor(37.5)==37（非 38/37.5）。
- [x] **AC-25** [核心不变式] `nlv = Σ MortgageValue(未抵押地) + Σ building_sellback(每栋,F-8)`：持 2 未抵押地（MV 100/60）+ 3 栋建筑（各 sellback 50）→ nlv==310。
- [x] **AC-26** 已抵押地贡献 0：持 1 已抵押（MV 100）+ 1 未抵押（MV 60）→ nlv 只计 60。
- [x] **AC-27** 🔴 [全档最高风险门] 三断言：① F-9 数值正确（PropA MV=100未抵押 / PropB MV=60已抵押house=0 / 2栋BuildingCost=100, Cash=30 → nlv==100+0+50+50==200）；② F-11 资产移交完整性 identity-check（同 fixture 收租破产 → creditor 新增恰 {PropA,PropB,建筑×2} 逐对象身份核对 ∧ PropB.is_mortgaged 仍 true ∧ debtor.Cash 转入后归 0）**〔②的破产移交断言在 story-009 落，本 story 落 ①〕**；③ 单一枚举入口（F-9 资产枚举一段实现，F-10 调它不另写第二段，code-review）。
- [x] **AC-28** [关键边界] `is_insolvent=(Cash + nlv < amount_due)` 严格 `<`：Cash=50, nlv=120, due=170 → `170<170==false`（付到 0 算能付、NOT 破产）；due=171 → true。
- [x] **AC-29** is_insolvent 消费 F-9 同源：AC-28 的 nlv 由 F-9 计算、非独立重算；F-10 不另写第二段资产枚举（code-review，并入 AC-27③）。
- [x] **AC-32** [确定性] F-6/F-8/F-9 取整路径两次冷启动 / Debug vs Shipping 位级相同（无 float）：MV=75 赎回恒 83、BuildingCost=75 卖回恒 37。

---

## Implementation Notes

*Derived from ADR-0014/0006:*

- **F-8 卖回（整数 floor）**：`sellback = (BuildingCost × sell_num) / sell_den`（sell_num/sell_den=1/2，整数截断==floor 因操作数≥0）。
- **F-9 NLV**：`net_liquidation_value(player) = Σ MortgageValue(t)[未抵押可购买地] + Σ_t house_count[t] × floor(BuildingCost[t]/2)`。**逐栋 floor 先于求和**（AC-27/AC-43变体C：BuildingCost=75,house=3 → 3×floor(75/2)=111，**≠** floor(112.5)=112）。已抵押地贡献 0（AC-26）。
- **建筑枚举不由 economy 直读8**（5→8 环）：建筑清单经回合2 `ResolvePhase` 调 8 `GetPlayerBuildings(player)→[(tile,house_count)]` 聚合后作 `preaggregated_nlv` 传入 F-10（ADR-0006）。F-9 的 MV 侧（地产6）+ sellback 侧（建房8 house_count）均经回合2 聚合。
- **F-10 is_insolvent**：`is_insolvent(player, amount_due, preaggregated_nlv) = (Cash + preaggregated_nlv < amount_due)`，**严格 `<`**（付到恰好 0 算能付，AC-28）。纯谓词，不内部算 nlv（防 5→8 环）。
- **单一枚举入口（AC-27③/AC-29）**：F-9 资产枚举只一段实现；F-10 调它、不另写第二段（code-review/code-search）。
- **AI 估值读（econ-013）**：AI 经只读 `FGameStateSnapshot` 读 SelfCash/PreaggregatedNlv（ADR-0006，AI 严禁自算 nlv）；本 story 只保证 economy 暴露正确的 F-9/GetCash 供 snapshot 装配，不实现 AI。
- **AC-27② 移交完整性 identity-check** 的破产移交断言在 **story-009**（F-11 现金侧 + 资产 in-kind 由破产9 经6/8）；本 story 落 AC-27① 数值正确 + ③ 单一入口。

---

## Out of Scope

- Story 009：F-11 破产移交（AC-27② 的资产 identity-check 在 009）、Raising Funds 状态机。
- 建房8 的 `GetPlayerBuildings`/house_count（building epic）；回合2 的 preaggregated_nlv 装配（player-turn，ADR-0006）；地产6 的 MV/is_mortgaged（property epic）。
- AI(10) 的 snapshot 估值消费（ai-opponent epic）。

---

## QA Test Cases

- **AC-24**：BuildingCost=100 → 50；BuildingCost=75 → floor(37.5)==37（非 38）。
- **AC-25**：Given 2 未抵押地 MV 100/60 + 3 栋 sellback 50；Then nlv==310。
- **AC-26**：Given 1 已抵押 MV 100 + 1 未抵押 MV 60；Then nlv==60（已抵押贡献 0）。
- **AC-27①**：Given PropA(MV100未抵押)/PropB(MV60已抵押house0)/2栋(BC100)/Cash30；Then nlv==100+0+50+50==200。
- **AC-27③**：[code-review] F-9 枚举一段实现；F-10 调它不另写。
- **AC-28**：Cash=50,nlv=120,due=170 → is_insolvent==false（170<170 false）；due=171 → true。断言边界两侧。
- **AC-29**：[code-review] AC-28 nlv 由 F-9 算非独立重算。
- **AC-32**：MV=75 赎回恒 83、BC=75 卖回恒 37，Debug vs Shipping / 两次冷启动位级一致。**变体C**：BC=75,house=3 → Σsellback=3×37=111（≠112，逐栋 floor）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/nlv_insolvency_test.cpp`（类目 `Rento.Economy.NlvInsolvency`）— 存在且通过。AC-27 🔴 最高风险门须变异坐实非 vacuous。
**实际物理路径**: `Source/rentoTests/Private/NlvInsolvencyTest.cpp`（UE 模块约定）。
**Status**: [x] 8 TC 全通过（TC1 F-8 floor/TC2 NLV 求和/TC3 已抵押贡献0/TC4 综合 nlv200/TC5 🔴逐栋 floor 变体C 111/TC6 🔴严格<边界/TC7 单一枚举+防御/TC8 负 HouseCount/负 nlv 守门 W-1/W-2）。证据 `Saved/TestReport_econ008_w12/index.json`；全 `Rento.Economy` **50/0 无回归**。
**🔴 变异坐实**: 临时把 ComputeNetLiquidationValue 改合并 floor(Σ)→**TC5 真 FAIL**（6/1，`Saved/TestReport_econ008_mut/`），还原后复 — 证 TC5 非 vacuous 真守逐栋 floor 契约（AC-27/AC-32）。
**code-review**: APPROVED；采纳 W-1（负 HouseCount dev log）/W-2（负 PreaggregatedNlv 守门）。**propagate 债**：fresh-grep 坐实 player-turn 2 处内联 NLV（`PlayerTurnSubsystem.cpp:1996/:2154` 硬编 /2 绕过 F-9，潜在 House-Rules 分歧）→ `production/qa/bugs/BUG-econ008-pt007-nlv-duplicate-enumeration.md`（econ-008 自身 AC 满足，债归 player-turn follow-up）。

---

## Dependencies

- Depends on: Story 001（GetCash）、Story 006（F-6/F-8 取整口径共用）
- Unlocks: Story 009（凑钱状态机/破产用 nlv + is_insolvent）

---

## Completion Notes
**Completed**: 2026-06-09
**Criteria**: 6/6 AC（AC-24/25/26/27①③/28/29/32）COVERED + 变异坐实；AC-27② 破产移交 identity-check 按 story 设计 DEFERRED→story-009。8 TC，全 Rento.Economy 50/0 无回归。
**Deviations**: ADVISORY 测试物理路径 UE 模块约定（同 econ-001..007）；采纳 code-review W-1（负 HouseCount dev log）/W-2（负 PreaggregatedNlv 守门，TC8 覆盖）。
**Propagate 债（跨 story，已登记）**: player-turn 2 处内联 NLV（`PlayerTurnSubsystem.cpp:1996/:2154` 硬编 /2 绕过 economy F-9，潜在 House-Rules 分歧）→ `production/qa/bugs/BUG-econ008-pt007-nlv-duplicate-enumeration.md`，债归 player-turn follow-up（econ-008 自身 AC 满足）。
**Test Evidence**: Logic — `Source/rentoTests/Private/NlvInsolvencyTest.cpp`（8 TC，`Saved/TestReport_econ008_w12/index.json` 50 succeeded/0 failed）；🔴 TC5 逐栋 floor 变异坐实（合并 floor→6/1 FAIL，`Saved/TestReport_econ008_mut/`）。
**Code Review**: Complete（/code-review APPROVED；unreal-specialist W-1/W-2 采纳；qa-tester FINDING-2 跨 story 分歧转 bug report）。
**实现**: F-8 ComputeBuildingSellback=floor(BC×num/den) 整数纯净；F-9 ComputeNetLiquidationValue=Σ MV[未抵押]+Σ HouseCount×sellback（逐栋 floor，单一枚举入口）；F-10 IsInsolvent=(GetCash+nlv)<due 严格<消费传入不重算；+USTRUCT FNlvAssetEntry + 旋钮 BuildingSellbackNum/Den=1/2。economy 不直读6/8（ADR-0006）。
