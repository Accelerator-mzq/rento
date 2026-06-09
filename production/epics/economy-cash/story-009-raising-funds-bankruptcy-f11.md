# Story 009: 凑钱状态机 CR-7 (Raising Funds + 自动清算兜底) + 破产现金侧 F-11

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: 2026-06-09

## Context

**GDD**: `design/gdd/economy-cash.md`（CR-7 / States / F-11）
**Requirement**: `TR-econ-008`（OnBankruptcyDeclared 切分）、`TR-econ-018`（破产移交锁/原子性）、`TR-econ-011`（Raising Funds 瞬态不存档 ref）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0001（宿主/Raising Funds 寄宿 ResolvePhase，primary）· ADR-0014（逐栋 floor 变体C）· ADR-0003（OnBankruptcyDeclared 现金侧切分）
**ADR Decision Summary**: economy 拥有**清算顺序 spec + 现金判据**（`Cash≥应付?`/`is_insolvent`）；**执行驱动归回合2**（调 6.Mortgage / 8.ForcedSellNextBuilding，economy 不直调 6/8，防 5→6/5→8 环）；F-11 economy 只结算**现金侧**，资产 in-kind 转移归破产9 经6/8。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 有界终止循环（资产有限）；mock 6/8 接口（Mortgage/ForcedSellNextBuilding）+ 现金升模拟。`-nullrhi` 可测。Integration：跨 6/8/9（经回合2 驱动），用 spy/mock 接缝。

**Control Manifest Rules (Core/Foundation)**:
- Required: 清算顺序确定（mortgage-empty-first 止损优先）+ 有界终止；现金够即早停；**破产主反馈只订经济5 `OnBankruptcyDeclared`**；owner 先算定再广播。
- Forbidden: economy 直调 6.Mortgage/8.SellHouse（5→6/5→8 环 — 执行驱动归回合2）；economy 写 owner map / 拆建筑；**把破产事件合并删 OnBankruptcyDeclared**。
- Guardrail: NLV order-independent（顺序只影响早停后剩余，不影响 is_insolvent）。

---

## Acceptance Criteria

- [x] **AC-30** 收租破产 in-kind **现金侧**：debtor.Cash=30 + 2地 + 1房，收租破产 → `creditor.Cash+=30`（现金腿）∧ MVP 不收继承利息。**地产/建筑 owner in-kind 转移 + 保留抵押由破产9 经地产6 执行（所有权 AC-33），本系统不写 owner map、不在本 AC 断言归属转移**。
- [x] **AC-31** 银行破产 **现金侧**：税致破产（债权人=银行）→ 建筑拆除无现金返还 ∧ 现金蒸发 ∧ MVP 不拍卖。**建筑拆除由破产9 调建房8 `LiquidateAllBuildings`、地产回退由地产6 `ReturnTileToBank`，本系统只断言现金侧**。
- [x] **AC-35** `OnInsufficientFunds(Player,AmountDue,AmountShort)`：进入 Raising Funds 时广播（Cash=50, due=170 → AmountDue=170, AmountShort=120）。
- [x] **AC-36** `OnBankruptcyDeclared(Debtor,Creditor)`：破产移交完成后广播一次，时机在资产转移之后。
- [x] **AC-43** [状态机] 确定性自动清算兜底终止性（回合2 驱动、economy 拥顺序 spec + 现金判据）：
  - 主 fixture（mortgage-empty-first，无建筑）：Cash=50、应付=200、3 空地 MV 60/80/100 → 依次 Mortgage 到 Cash≥200 → Debit(200) → Cash==90 回 Solvent。断言：顺序确定（MV升序、两次冷启动位级一致）∧ 够即停（不超额清算）∧ economy 仅供判据、回合2 驱动调 6/8。
  - 变体A（走向破产）：nlv 总和 < 应付 → 清算穷尽 → `is_insolvent==true` → 移交破产9（证伪兜底死锁）。
  - 变体B（混合资产）：Cash=50、应付=300、PropX(空地MV80house0)+PropY(2房MV100BC100) → 先 Mortgage(PropX)→130；ForcedSellNextBuilding(PropY)×2→180→230；Mortgage(PropY)→330≥300 → Debit(300) → Cash==30 回 Solvent。断言：空地优先抵押（止损）、建筑无空地可抵押后才卖、回合2 驱动。
  - 变体C（奇数 BuildingCost per-building floor）：PropZ BC=75 house=3 → Σsellback=3×floor(75/2)=111（≠112）。

---

## Implementation Notes

*Derived from ADR-0001/0014/0003:*

- **Raising Funds 瞬态**：寄宿于回合2 `ResolvePhase` 阻塞子相（回合不推进到下一玩家直到解决，ADR-0001）。economy **发起并裁决**进出判据（`OnInsufficientFunds` 进入；`Cash≥应付`回 Solvent / `is_insolvent`进 Bankrupt），瞬态本身归回合2。
- **清算顺序 spec（economy 拥有，止损优先 mortgage-empty-first）**：
  ```
  while Cash < amount_due {
    若存在可直接抵押地(owner==player ∧ 未抵押 ∧ (非Property ∨ house_count==0)):
        → 回合2 调 6.Mortgage(MV 最小的可抵押地);
    否则若存在建筑(house_count>0):
        → 回合2 调 8.ForcedSellNextBuilding(F-4 全盘最高档);
    否则: → break → is_insolvent → 破产9
  }
  ```
  止损优先理由：抵押可赎回（零半价损失），卖建筑永久半价亏损；经典禁抵押带房地，故带房须先卖房腾空才可抵押。现金够即早停；顺序不影响 is_insolvent（NLV order-independent）。
- **执行驱动归回合2（防环）**：economy **不直接调 6.Mortgage / 8.ForcedSellNextBuilding**（5→6/5→8 环）；economy 只提供"`Cash<amount_due?`"判据 + 顺序 spec，**回合2 `ResolvePhase` 驱动调 6/8**（合法 depends-on 5/6/8）。测试用 mock 6/8 接缝（持 PS* + gain 抬 Cash 模拟现金升，框架从不写 Cash）。
- **F-11 现金侧**：收租破产 `creditor.Cash += debtor.Cash`（AC-30，MVP 不收继承利息）；银行破产现金蒸发、建筑拆除无返还（AC-31）。**economy 不写 owner map、不拆建筑**——资产 in-kind 转移归破产9 经地产6（`TransferOwnership`/`ReturnTileToBank`）/建房8（`LiquidateAllBuildings`），由其 AC 断言（所有权 AC-33/34）。
- **触发时机（AI 即时/人类超时）与 UI 提示归回合2/AI/UX（OQ-Econ-8）**，本 story 只验"清算顺序确定性 + 有界终止 + 现金侧"。
- 广播 `OnBankruptcyDeclared(Debtor,Creditor)`（经济5 现金侧信号，story-002 契约）破产移交完成后恰一次（AC-36），破产主反馈下游订此事件。
- **变体C 逐栋 floor** 引用 ADR-0014 / story-008 口径（per-building floor，111≠112）。

---

## Out of Scope

- 破产9 的破产宣告/出局裁决 + 资产 in-kind 转移（bankruptcy epic，AC-30/31 资产侧）；地产6 `TransferOwnership`/`ReturnTileToBank`（property epic，ADR-0013）；建房8 `ForcedSellNextBuilding`/`LiquidateAllBuildings`（building epic）；回合2 的 ResolvePhase 驱动循环宿主（player-turn）。
- Story 010：Cash 存档（Raising Funds 瞬态不中途存档 econ-011 在 010 验）。
- 抵押现金腿 F-5/F-6（story-006，本 story 调用其口径）；NLV/is_insolvent（story-008，本 story 消费）。

---

## QA Test Cases

- **AC-30**：Given debtor.Cash=30 收租破产 creditor；Then creditor.Cash+=30 ∧ 不收继承利息 ∧ economy 不写 owner map（spy 无归属写）。
- **AC-31**：Given 税致破产（债权人=银行）；Then 现金蒸发 ∧ 无现金返还 ∧ economy 只断言现金侧（建筑拆除/地回退归9/6/8）。
- **AC-35**：Given Cash=50, due=170；Then OnInsufficientFunds(p,170,120) 恰 1 次。
- **AC-36**：破产移交完成 → OnBankruptcyDeclared(debtor,creditor) 恰 1 次，时机在资产转移之后（spy CallLog 顺序）。
- **AC-43 主**：Cash=50,应付=200,3空地MV60/80/100 → Mortgage×3 到 Cash=290≥200 → Debit(200) → Cash=90。断言 MV升序确定 + 够即停（第3地后停）+ economy 仅供判据回合2驱动。
- **AC-43 变体A**：nlv<应付 → is_insolvent → 破产9（无死锁）。
- **AC-43 变体B**：混合资产，空地先抵押后卖房（mortgage-empty-first），回合2驱动调 6/8。
- **AC-43 变体C**：BC=75 house=3 → Σsellback=111（≠112）。
- 终止性：循环有界（每步资产严格减，有限步终止）；两次冷启动位级一致。

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/economy-cash/raising_funds_bankruptcy_test.cpp`（类目 `Rento.Economy.RaisingFundsBankruptcy`）— 存在且通过（mock 6/8 接缝）。AC-43 状态机须变异坐实（去 mortgage-empty-first → 顺序断言 FAIL）。
**实际物理路径**: `Source/rentoTests/Private/RaisingFundsBankruptcyTest.cpp`（UE 模块约定）。
**Status**: [x] 11 TC 全通过（TC1 mortgage-empty-first+MV最小/TC2 无空地卖房/TC3 None偿付/TC4 耗尽Insolvent/TC5 破产转玩家AC-30+AC-36时机序列/TC6 破产蒸发银行AC-31/TC7 0额仍广播AC-36/TC8 OnInsufficientFunds AC-35/TC9 变体B腾空再抵押/TC10 平手确定性/TC11 变体C逐栋floor自证）。
**集成覆盖**: AC-43 完整清算循环（回合2 驱动 6/8）在 `GameStateSnapshotAiHooksTest` TC8/TC8b（已改用 economy `DecideNextLiquidationStep` spec）。全 `Rento` **329/0 无回归**（`Saved/TestReport_econ009_final2/`）。
**🔴 双变异坐实**: ① DecideNextLiquidationStep 改 sell-first → **TC1 真 FAIL**（7/1，`Saved/TestReport_econ009_mut/`）证 mortgage-empty-first；② SettleBankruptcy Broadcast 错位 → **TC5/6/7 FAIL**（8/3，`Saved/TestReport_econ009_ac36mut2/`）证 AC-36 时机序列断言；均还原复 329/0。
**code-review hardening**: 采纳 unreal W-1（MV>0 守门）/W-2（注释修正）+ qa BLOCKING-1（AC-36 EventSequence 时机断言，spy 加 EventSequence 字段）/GAPS-1（变体B多步TC9）/GAPS-2a（平手TC10）/GAPS-3（变体C自证TC11）。
**架构（用户裁定 option 3）**: 清算顺序 spec 从 pt-007 抽到 economy `DecideNextLiquidationStep`（economy 拥有 spec，回合2 驱动调 6/8）；破产现金侧 `SettleBankruptcy`（F-11）。**Design X 结构性破产判定** → 删 pt-007 `CheckInsolvencyWithNlv` + TC9，**解除 propagate 债 :2154**（NLV 统一归 economy F-9）。AC-27② 资产 identity-check 仍归破产9。

---

## Dependencies

- Depends on: Story 001（Credit/Debit）、Story 002（OnInsufficientFunds/OnBankruptcyDeclared）、Story 006（Mortgage 现金腿）、Story 008（nlv/is_insolvent + 逐栋 floor）
- Unlocks: None（最复杂叶子）

---

## Completion Notes
**Completed**: 2026-06-09
**Criteria**: 5/5 AC（AC-30/31/35/36/43 含变体A/B/C+平手）COVERED + 双变异坐实。11 TC（RaisingFundsBankruptcy）+ 集成（pt-007 TC8/TC8b）；全 Rento 329/0 无回归。资产 in-kind 移交（AC-30/31 资产侧）按设计归破产9（out of scope）。
**架构（用户裁定 option 3）**: 清算顺序 spec 从 pt-007 抽到 economy `DecideNextLiquidationStep`（economy 拥有 spec，回合2 驱动调 6/8，AC-43 字面达成）；破产现金侧 `SettleBankruptcy`（F-11）。Design X 结构性破产判定 → 删 pt-007 `CheckInsolvencyWithNlv` + TC9，**解除 propagate 债 :2154**（NLV 统一 economy F-9）。
**Deviations**: ADVISORY 测试物理路径 UE 模块约定；OUT OF SCOPE（授权）跨系统重构动 player-turn（RunForcedLiquidation 重构 + pt-007 TC8b 调整/删 TC9 + 共享 EconomyEventSpy +EventSequence 字段）。`:1996/:1991`（AssembleSnapshot AI 内联 NLV + float 赎回价）留 AI-snapshot follow-up（bug report 登记）。
**Test Evidence**: Integration — `Source/rentoTests/Private/RaisingFundsBankruptcyTest.cpp`（11 TC，`Saved/TestReport_econ009_final2/`）；双变异坐实（TC1 顺序 `Saved/TestReport_econ009_mut/` + AC-36 时机 `Saved/TestReport_econ009_ac36mut2/`）。
**Code Review**: Complete（/code-review CHANGES REQUIRED → hardening 全改后 APPROVED；unreal W-1/W-2 + qa BLOCKING-1/GAPS-1/2a/3 全采纳；TC8b/TC9 改动两专家判合理）。
**实现**: DecideNextLiquidationStep（静态纯函数清算顺序 spec，MV>0 守门）+ SettleBankruptcy（玩家债主 TransferCash/银行 INDEX_NONE 蒸发 + OnBankruptcyDeclared 现金腿后广播）+ ELiquidationAction 枚举 + FNlvAssetEntry+TileIndex。
