# Change Impact — 建房升级(8) R4 APPROVED propagate

> **Date:** 2026-06-04
> **Trigger:** `design/gdd/building-upgrade.md` R4 design-review APPROVED-with-followups → 3 producer propagate 债
> **Skill:** `/propagate-design-change`
> **Note:** 项目处 Design 阶段,**ADR 数 = 0**(`technical-preferences.md` 确认)。本次为 **GDD→GDD 接缝传播**(非 GDD→ADR);TD-CHANGE-IMPACT 门以 0 ADR 为空,跳过。沿用项目先例(P-1 / OQ-Prop-2/3 均经本 skill 作跨档传播)。

---

## Change Summary

building-upgrade.md(系统#8)R4 批准为 APPROVED-with-followups,产生三条须在**已 Approved 下游 GDD**(economy-cash 5 / player-turn 2)兑现的接缝义务。主审 fresh-grep 确认三债在下游**真空**(登记≠闭合,对齐项目纪律),用户授权全改 + 裁定清算顺序后执行传播。

**核心设计裁定(用户,OQ-Build-1):** 强制自动清算顺序 = **止损优先 mortgage-empty-first**:
```
while Cash < amount_due:
    if 存在可直接抵押地 (owner==player ∧ 未抵押 ∧ (非Property ∨ house_count==0)):
        6.Mortgage(MV 最小的可抵押地)        # 抵押可赎回、零半价损失
    elif 存在建筑 (house_count>0):
        8.ForcedSellNextBuilding()           # building F-4 全盘最高档;卖空某组后该组Property转可抵押
    else:
        break → is_insolvent → 破产(9)
```
顺序**不**影响 `is_insolvent`(NLV 是 order-independent 和),仅影响早停后剩余资产与玩家损失额。理由:抵押可赎回,卖建筑永久半价亏损;经典规则禁抵押组内有房地,故带房地须先卖房腾空才可抵押。

**无环纪律(贯穿):** economy(5)是 6/8 上游,**绝不**调 8(`SellHouse`/`ForcedSellNextBuilding`/`GetPlayerBuildings`)或 6(`Mortgage`)。清算**执行驱动归回合2(2)`ResolvePhase`**(合法 depends-on 5/6/8)。economy 只拥有顺序 spec + 现金判据;building 拥有"卖哪一栋"(F-4)+ 有界终止。

---

## Affected Documents & Edits(GDD→GDD,非 ADR)

### A. building-upgrade.md(本档自洽订正)
- **CR-9 接缝标注1(驱动环示例)**:buildings-first → mortgage-empty-first(对齐用户裁定;消本档示例与下游不一致)。
- **OQ 表 + 跨系统义务 + AC-27〔Obligation〕**:OQ-Build-1/2 标 ✅ RESOLVED;OQ-Build-6 player-turn 侧 RESOLVED(HUD 侧登记 systems-index);AC-27 删"player-turn 真空"旧 claim(已 fresh-grep player-turn L143/L549 反验),硬门转移至实现侧。

### B. economy-cash.md(economy-designer 执行,6 edits)
| # | 段 | old(stale) | new |
|---|---|---|---|
| 1 | CR-7.3(L71) | 「本系统提供清算:按 MV 升序遍历资产、带房地先 F-8 卖房再抵押」= 5→8/5→6 反向边 | economy 拥**仅**顺序 spec(mortgage-empty-first)+ 现金判据;执行归回合2;不直调 6/8 |
| 2 | F-9(L216) | `Σ building_sellback(b)` 无展开 | 加展开式 `Σ_t house_count[t]×floor(BuildingCost[t]/2)`(逐栋 floor;奇数 BC 差异:BC=75,h=3→111≠112)+ 枚举经回合2聚合传入 |
| 3 | F-10(L231) | `is_insolvent(player, amount_due)` | `is_insolvent(player, amount_due, preaggregated_nlv)`——回合2 预聚合传入,F-10 不内部算 NLV(防 5→8) |
| 4 | AC-43 变体B(L464) | 「先 F-8 卖 2 房再抵押」(旧顺序) | 重写 mortgage-empty-first + 回合2 驱动;新增变体C 奇数 BC fixture(BC=75→sellback 37,逐栋 floor) |
| 5 | Dep building(8) 行(L320) | 「建筑清单供 F-8/F-9」 | 「F-9 建筑枚举经回合2 ResolvePhase 聚合 GetPlayerBuildings 传入(economy 不直读8,防5→8环)」 |
| 6 | OQ-Econ-9(board 校验批) | 仅 `RentTable[1]≥RentTable[0]×mult` | +②`RentTable[5]>RentTable[4]`(strict,防末档 ROI=0)+③`RentTable[5]>RentTable[0]×mult`(防酒店低于垄断空地翻倍) |

AC 计数不变(变体 B/C 为同一 AC 子例):[Logic] gate 41 / Advisory 3 / 总 44。

### C. player-turn.md(systems-designer 执行)
| # | 新增 | 内容 |
|---|---|---|
| 1 | **CR-3.3** 强制清算驱动环 | Raising Funds 阻塞子相;回合2 拥执行驱动;mortgage-empty-first 伪码;有界终止;无 economy→8/6 反向边 |
| 2 | **CR-3.4** 破产判据 NLV 聚合 | 全组合扫描 `GetPlayerBuildings`(异于 CR-3.2 单格快照);算 `preaggregated_nlv` 传 economy `is_insolvent`;economy 不反调8 |
| 3 | **CR-3.5** 建房通告 beat | 监听 `OnBuildingChanged` → 广播 `OnBuildingAnnounced`(全员可见;TileIndex/NewHouseCount/ActingPlayerId);HUD(16) 消费义务登记 |
| 4 | **AC-50** | Mortgage-pre-read:spy 断言 `6.Mortgage` 对 `house_count>0` tile 恒不调用(对照 house_count==0 调1次) |
| 5 | **AC-51** | 清算顺序+终止:Mortgage(空地)先于 ForcedSellNextBuilding;早停;穷尽→is_insolvent |
| 6 | **AC-52** | 破产 NLV 全组合聚合:GetPlayerBuildings 调1次,preaggregated_nlv 传 F-10,zero economy→8 |
| 7 | **AC-53** | 通告 beat:OnBuildingAnnounced 广播1次 + payload + 零状态改;TurnStart/End 期不广播 |

player-turn 可执行条目 55→59。

---

## Independent Verification(主审 fresh-grep,非信 subagent 自述)

| 核查 | 结果 |
|---|---|
| economy 旧环措辞(`本系统提供清算`/`遍历资产`/`先卖房后抵押`) | **0 命中** ✓ 已清除 |
| economy F-10 新签名 `is_insolvent(player, amount_due, preaggregated_nlv)` | L231 命中 ✓ |
| economy CR-7.3 = mortgage-empty-first + 「不直调 6/8」 | L71 核实 ✓ |
| player-turn CR-3.3 驱动环调 `economy.is_insolvent(player, amount_due, preaggregated_nlv)` | L143-148 ✓ **签名与 economy 完全一致** |
| player-turn CR-3.4 全组合 NLV 聚合 | L156-159 ✓ |
| 三档清算顺序一致(building L51 == economy CR-7.3 == player-turn CR-3.3 = mortgage-empty-first) | ✓ 全一致 |

---

## Residual / Open

- **OQ-Build-3**(破产建筑 in-kind vs 卖房还银行):仍 open,待破产(9)设计 + Rento 核查。本批未触及。
- **OQ-Build-6 HUD 侧**:player-turn 已供 `OnBuildingAnnounced` 数据源;HUD(16)消费义务已登记 systems-index 继承义务表,待 HUD GDD 兑现(future-doc 义务,非当前真空)。
- **building RECOMMENDED 下一 pass**(非本批):AC-25c 序列级均衡 / AC-21 OnBuildingChanged 断言 / F-1 is_monopoly-guards-first 文档化 / BP 绑定形式 + 清算 loop 帧模式→OQ-Build-5。
- **AC-30 deferred-gate**:GDD 侧契约(F-10 签名 + 回合2 聚合)已落;运行时集成 spy 断言仍 deferred 至实现期(real economy × real 8),producer 须在实现 sprint 通知 qa-lead 激活。

---

## Follow-Up

- 设计顺序下一系统 **#9 破产与胜负**(depends-on 2/5/6 均 Approved;承接 OQ-Build-3 in-kind 裁定 + 消费 F-10/F-9 口径)。
- 无 ADR 待写(Design 阶段);架构期 `/create-architecture` 时本接缝(清算驱动归回合2、NLV 预聚合、无环纪律)须落 ADR。
