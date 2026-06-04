# Change Impact — 破产与胜负(9)→ economy-cash.md(OQ-Bankrupt-3)

> **Date**: 2026-06-04
> **Skill**: `/propagate-design-change`(适配 GDD→GDD 接缝,无 ADR 阶段)
> **Trigger**: bankruptcy(9) GDD 裁定 in-kind 移交 = identity-check(逐对象枚举完整性,AC-14)+ 9 = return-only 编排子程序;须传播闭合 economy-cash.md 旧措辞。
> **Verdict**: ✅ COMPLETE

---

## 1. Change Summary

bankruptcy.md(新档,无 git HEAD 版本)在 Detailed Design / Formulas / AC 落定:
- **9 = return-only 编排**:回合2 调 `ResolveBankruptcy(debtor,creditor,snapshot)→{debtorEliminated,winnerId|NONE,rankingEntry}`,9 只向下调 5/6/8、绝不回调 2(防 2↔9 环)。
- **in-kind 移交验证 = identity-check**(AC-14 ⭐):逐对象枚举完整性,**禁用「移交总额 == F-9」**(in-kind 非现金等价,已抵押地 F-9 记 0 但资产对象仍转出)。
- **建筑处理**(CR-4/AC-18):银行破产建筑拆除归 9 调建房8 `LiquidateAllBuildings`;地产回退归所有权6 `ReturnTileToBank`。

## 2. Architecture Inputs

- **ADRs**: 0(设计阶段,`docs/architecture/adr-*.md` 无文件)。GDD→ADR impact 阶段 N/A。
- **下游受影响 artifact**: `design/gdd/economy-cash.md`(GDD→GDD 接缝)。

## 3. Impact Analysis(fresh-grep 双侧核实)

fresh-grep economy-cash.md 全集后,**实际待修范围比 OQ-Bankrupt-3 登记清单更窄** —— economy 在 R1 已自我修正其 AC-27②(identity-check)/ AC-30 / F-11 头(只执行现金侧、不写 owner map)。登记此 OQ 时假设 economy 仍全是旧措辞,实为已自修。

| Spot | 状态 | economy 位置 | 处置 |
|---|---|---|---|
| **①** OQ-T-Econ-2「9 移交总额==F-9」 | 🔴 REAL 真矛盾 | line 479 | 已改 identity-check(对齐本档 AC-27②)+ 标 RESOLVED |
| **②** F-11 散文「in-kind 转移」误读为 economy 执行 | ✅ 已闭合不改 | line 245/445 | F-11 头 + AC-30 早已声明「只执行现金侧、不写 owner map、归属转移归 9 经 6/8」——无真矛盾 |
| **③** 银行破产「建筑拆除」执行主体悬空 | 🔴 REAL 缺失 | line 247 + AC-31 line 446 | 已补注归 9 调建房8 `LiquidateAllBuildings`、地产回退归 6 `ReturnTileToBank` |

## 4. Resolution(5 处编辑跨 3 文件,用户批准「全改」)

**economy-cash.md(×3):**
- line 479 — OQ-T-Econ-2 改 identity-check 框架 + ✅ RESOLVED(F-9 仅判据/net_worth 估值口径;移交用 identity-check)。
- line 247 — F-11 银行破产补「建筑拆除归 9 调建房8 LiquidateAllBuildings、地产回退归 6 ReturnTileToBank,本系统只现金侧」。
- line 446 — AC-31 补建筑拆除执行主体 + 现金侧范围分离。

**bankruptcy.md(×1):**
- OQ-Bankrupt-3 → ✅ RESOLVED,记录范围收窄(②已 economy R1 自修)。

**systems-index.md(×2):**
- line 72 orchestration 义务行 → ✅ RESOLVED。
- line 57 ◊ 脚注待办② → ✅ RESOLVED(fresh-grep 揪出的自造 stale 邻居,同步修正)。

## 5. Verification(CD 约束:不凭编辑成功宣称闭合)

- fresh-grep `移交总额|总额==F-9` 全 `design/gdd/`:残留命中均为**否定/禁用语境**(bankruptcy AC 纪律「严禁用」、economy line479 删除线、systems-index「禁用」)+ RESOLVED 记录的历史描述。无残留**正向旧规定**。
- fresh-grep `LiquidateAllBuildings|建筑拆除` economy:line 247/446 执行主体已补注。

## 6. Follow-Up

- 无 ADR 待写(设计阶段)。
- bankruptcy.md 仍 **Designed(pending review)** —— fresh session 跑 `/design-review design/gdd/bankruptcy.md` 独立验证。
- 后续架构期:OQ-Prop-1(owner map 生命周期 ADR)/ OQ-Bankrupt-5(出局态容器,随 OQ-Prop-1)/ OQ-Bankrupt-6(返回值 schema + 存档禁止期)。
