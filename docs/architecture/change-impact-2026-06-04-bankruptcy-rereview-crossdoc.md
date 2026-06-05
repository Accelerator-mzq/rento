# Change Impact — 破产(9) R-review 跨档传播债(2 项)

> **状态**: ✅ COMPLETE — 2026-06-04 producer `/propagate-design-change` 执行,两债均 fresh-grep 双侧核实闭合(单侧唯一 + 双侧一致)。执行证据见文末「执行结果」节。
> **创建**: 2026-06-04(`/design-review design/gdd/bankruptcy.md` 再审 full 模式触发,verdict=NEEDS REVISION)
> **触发审查**: `design/gdd/reviews/bankruptcy-review-log.md`(2026-06-04 再审)
> **主审独立核实**: 两项 claim 均经 fresh-grep 双侧坐实(非采信 specialist claim),见各项「核实」。
> **CD 流程约束(active.md)**: 任何跨档接缝语义改须同 PR 开 propagate 工单 + fresh-grep 核实对端落地,不接受「登记 OQ 当闭合」。

## 背景

bankruptcy R-review 的 R1 根因(2↔9 接口矛盾)已闭合(`change-impact-2026-06-04-bankruptcy-playerturn-2to9.md`,player-turn 已 APPROVED)。本轮再审的 6 项 in-doc blocker 已在 bankruptcy.md 就地修复。**剩 2 项 BLOCKING 是跨档接缝债,落在 Approved 邻居档中,须 propagate(非 bankruptcy 单档可闭合)。**

---

## 债 1 — economy-cash.md:302 残留旧信号 `OnLastPlayerStanding`(传播遗漏)

### 矛盾

2↔9 propagate(`change-impact-2026-06-04-bankruptcy-playerturn-2to9.md`)把胜负信号统一为 `OnGameWon(winnerId)`(回合2 据 9 返回的 winnerId 触发),删除旧 `OnLastPlayerStanding`(2→9 反向信号)。player-turn + bankruptcy 两档已对齐。**但 economy-cash.md 未在该批传播范围内**,line 302 仍编码已废信号。

### 核实(主审 fresh-grep economy-cash.md)

- **line 302**(Edge Cases 破产移交边界,**活跃规定**,非 changelog):
  > "**若债务人破产后仅剩 1 名未破产玩家**:破产(9)触发胜负(`OnLastPlayerStanding`,player-turn 契约),本系统停止经济交互。"
  → 残留旧信号名 + 错误广播者归属(称「破产(9)触发」,实为 9 返回 winnerId、回合2 触发 `OnGameWon`)。
- **line 247 / 446**(LiquidateAllBuildings + 现金侧):**已正确**,与 bankruptcy 一致,**不在本债范围**(仅 302 漏)。

### 危害

依据 economy GDD 实现经济系统的开发者会期待收听 `OnLastPlayerStanding`;实际 player-turn/bankruptcy 发 `OnGameWon`。下游(HUD16/VFX19)挂错事件 → 游戏结束不触发封王庆祝。这是真矛盾(活跃规定层),非 changelog。

### 修复(producer /propagate 执行,改 economy-cash.md:302)

> "**若债务人破产后仅剩 1 名未破产玩家**:破产(9)在 `ResolveBankruptcy` 返回 `winnerId`,回合(2)据此触发 `OnGameWon(winnerId)`(信号名 + 广播者统一,见 bankruptcy CR-6/AC-40、player-turn CR-6),本系统停止经济交互。"

### 验收(fresh-grep 双侧)

- `grep OnLastPlayerStanding design/gdd/economy-cash.md` → 0 活跃规定(仅允许 changelog/「已删除」语境)。
- economy-cash.md 破产边界胜负信号与 bankruptcy/player-turn **单侧唯一、双侧一致** = `OnGameWon`,广播者 = 回合2。

---

## 债 2 — building-upgrade.md `LiquidateAllBuildings` 无 Credit 语义未承诺

### 矛盾

bankruptcy 银行破产路径依赖 `building.LiquidateAllBuildings(debtor)` 清建筑,且 AC-19/AC-11/AC-18/AC-36 断言其 **`Credit` 调用==0**(银行破产建筑拆除无现金返还,守 economy F-11)。但 building-upgrade.md 从未定义该接口的无 Credit 语义。

### 核实(主审 fresh-grep building-upgrade.md)

- **line 75 / 219**:`LiquidateAllBuildings(player)` 仅标 **「provisional 备选」**(随 OQ-Build-3),无 spec、无 Credit 语义承诺。
- **line 41(CR-5)/ 62 / 196 / AC-22(line 301)**:building 唯一具体强制卖房接口 = `ForcedSellNextBuilding`,**明确调 `Credit(player, floor(BuildingCost/2))`**(卖房返钱)。
- → 语义空白:`LiquidateAllBuildings`(bankruptcy 假设无 Credit)vs `ForcedSellNextBuilding`(有 Credit)。building 的实现者不知道 `LiquidateAllBuildings` 须**抑制 Credit**。AC-19 的「Credit==0」断言无 building 侧承诺支撑。

### 危害

若 `LiquidateAllBuildings` 被实现为 `ForcedSellNextBuilding` 的循环包装(自然假设),银行破产会向债务人 `Credit` `Σ floor(BuildingCost/2)` → **凭空创造货币流向蒸发账户**(违 economy F-11「建筑拆除无返还」+ 货币守恒)。AC-19 此时失败,但根因在 building 接口语义未定。

### 修复(producer /propagate 执行,改 building-upgrade.md;与 OQ-Build-3 协同)

在 building-upgrade.md CR/States/Dependencies 的 `LiquidateAllBuildings` 处**显式声明**:
> "`LiquidateAllBuildings(debtor)` 是银行破产专用接口:遍历 debtor 全部 `house_count>0` 格,逐格清零(`house_count=0`),**不调用经济5 `Credit`**(无偿清算,守 economy F-11 银行破产无现金返还)——与 `ForcedSellNextBuilding`(自愿/抵押腿清算,调 `Credit` 返半价)语义不同。由破产9 在银行破产分支调用(收租破产不调,建筑 in-kind 随格)。"

**注**:`LiquidateAllBuildings` 仍随 OQ-Build-3 为 provisional(最终是否「卖房还银行」待 Rento 核查)。本债只闭合**「无 Credit」语义承诺**(使 bankruptcy AC-19 有对端支撑),不预判 OQ-Build-3 终局口径。若 OQ-Build-3 裁定为经典「卖房还银行」,则改走带 Credit-to-bank-sink 的另一接口,届时 re-propagate。

### 验收(fresh-grep 双侧)

- building-upgrade.md `LiquidateAllBuildings` 处含「不调 Credit / 无偿清算」明文。
- bankruptcy AC-19「Credit==0」断言在 building 侧有对应语义承诺。

---

## 关联

- 本档(已就地修 6 in-doc blocker): `design/gdd/bankruptcy.md`
- 待改档: `design/gdd/economy-cash.md`(Approved)/ `design/gdd/building-upgrade.md`(Approved)
- 前序工单: `docs/architecture/change-impact-2026-06-04-bankruptcy-playerturn-2to9.md`(2↔9 根因,✅ COMPLETE)
- 协同 OQ: OQ-Build-3(in-kind vs 卖房还银行,building 委托 9 裁定,设计冻结前)

## 后续(producer 必做)

1. `/propagate-design-change` 执行债 1 + 债 2 改动。
2. fresh-grep 双侧核实(按各债「验收」),不凭编辑成功宣称闭合。
3. economy-cash.md / building-upgrade.md 是否触发重审锁面:债 1 = 单点 Edge Case 信号名订正(不改公式/接口面)→ 评估是否需 `/design-review`;债 2 = building 接口语义补全(触接口面)→ 大概率需 building-upgrade re-review 或 systems-index 标注。由 producer 判定。
4. 闭合后回填 bankruptcy review-log + 本工单状态 → COMPLETE。

---

## 执行结果(2026-06-04 producer `/propagate-design-change`)

> CD 约束执行:编辑前 + 编辑后均 fresh-grep 双侧,不凭编辑成功宣称闭合。memory 警示已生效——债 2 实际命中比登记多 1 处(line 65,登记仅 75/219)。

### 债 1 — economy-cash.md:302 信号订正 ✅ CLOSED

- **改动(1 处)**:economy-cash.md:302
  - BEFORE:`破产(9)触发胜负(OnLastPlayerStanding,player-turn 契约),本系统停止经济交互。`
  - AFTER:`破产(9)在 ResolveBankruptcy 返回 winnerId,回合(2)据此触发 OnGameWon(winnerId)(信号名 + 广播者统一,见 bankruptcy CR-6/AC-40、player-turn CR-6),本系统停止经济交互。`
- **fresh-grep 编辑前**:`OnLastPlayerStanding` 仅 302 一处活跃命中、`OnGameWon` 0 命中(登记「仅 302 漏」核实属实,无扩大集)。
- **fresh-grep 编辑后(双侧)**:
  - economy-cash.md `OnLastPlayerStanding` → **0 命中**(grep exit 1);`OnGameWon` → 仅 302(正确)。
  - player-turn.md / bankruptcy.md 残留 `OnLastPlayerStanding` 命中**全为 changelog/「已删除」/「取代旧」/「旧 bug 根因」语境**(player-turn L20/204/258/264/322/387/465/533/535/574 + bankruptcy:159),**无活跃规定**。
  - 三档胜负信号 = `OnGameWon`、广播者 = 回合2:**单侧唯一 + 三档一致**(economy:302 / bankruptcy:46,52,69,287 AC-40 / player-turn CR-6,L387,AC-40a)。

### 债 2 — building-upgrade.md LiquidateAllBuildings 无 Credit 语义 ✅ CLOSED

- **fresh-grep 编辑前暴露登记 under-count**:`LiquidateAllBuildings` 实际命中 **3 处(line 65/75/219)**,工单仅登记 75/219——**漏 line 65**(Detailed Rules 散文定义点,且残留旧「全卖还银行」措辞,若不改会与新「无偿清算」承诺自相矛盾)。三处全改。
- **改动(3 处)**:
  - **line 65**(散文定义):删旧「`LiquidateAllBuildings(player)`(全卖还银行)」→ 补完整承诺:银行破产专用、逐格清零、**不调用经济5 `Credit`**(无偿清算守 economy F-11)、与 `ForcedSellNextBuilding`(调 Credit 返半价)语义不同、provisional/OQ-Build-3 边界 + re-propagate caveat。
  - **line 75**(Interactions 表):`provisional LiquidateAllBuildings(player) 备选` → `银行破产调 provisional LiquidateAllBuildings(debtor) 逐格清零、不调 Credit(无偿清算,守 economy F-11)`。
  - **line 219**(Dependencies 表):同步补「逐格清零、不调 Credit=无偿清算,守 economy F-11」。
- **fresh-grep 编辑后(双侧)**:
  - building-upgrade.md 三处 `LiquidateAllBuildings` 均含「不调 Credit / 无偿清算」明文、语义一致;旧「全卖还银行」自相矛盾措辞已消除。
  - bankruptcy AC-19(line 247)「`Credit` 在 `LiquidateAllBuildings` 路径中调用==0」**对端断言仍在**,现有 building 侧语义承诺支撑(building 明文 cite AC-19,双侧互引闭环)。
  - provisional/OQ-Build-3 保留(L65 caveat + L232/322 OQ-Build-3 + L312 AC-31 quarantine 一致),**未预判 OQ-Build-3 终局**。

### 重审锁面建议(producer 评估,不单方裁定)

- **债 1**:单点 Edge Case 信号名订正,**不触公式/接口面**——economy-cash.md 无需 re-review。
- **债 2**:**触 building 接口语义面**(`LiquidateAllBuildings` 从「无 spec provisional」升为「承诺无 Credit 语义」)。建议**不**走全量 building-upgrade re-review(语义为收紧、与现有 AC-31 quarantine + OQ-Build-3 边界一致、未改公式/AC 编号),改走**轻量 systems-index 标注 + 留待 OQ-Build-3 破产9 设计时一并 re-review**。最终是否需 `/design-review design/gdd/building-upgrade.md` 由用户裁定。
