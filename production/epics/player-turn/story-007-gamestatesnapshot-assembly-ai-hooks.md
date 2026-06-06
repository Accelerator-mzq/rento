---
Epic: player-turn
Status: Ready
Layer: Core
Type: Integration
Estimate: L
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 007 — GameStateSnapshot 装配/冻结 + AI 决策 hook + ResolvePhase 聚合编排

## Context

- **GDD**: `design/gdd/player-turn.md` — CR-8（AI 决策契约：同步 hook + GameStateSnapshot 只读视图）、CR-3.2（ResolvePhase 算租输入聚合契约）、CR-3.3（强制清算驱动循环）、CR-3.4（破产 NLV 全组合聚合）、四 hook 失败语义（L176-180）、批处理失败策略（L186）、AC-37a/AC-38/AC-38b/AC-39/AC-39b/AC-37d/AC-42/AC-45/AC-48/AC-49/AC-50/AC-51/AC-52
- **Requirement TR-ID**: TR-turn-007（GameStateSnapshot 的 UE 类型〔值语义只读 `const FGameStateSnapshot&`〕与字段构成）、TR-turn-008（CR-8 三/四 AI 决策 hook 签名传 `const FGameStateSnapshot&`，决策期冻结、AI 不写状态）、TR-turn-009（snapshot 装配方/宿主=回合2 World Subsystem，跨系统聚合注入面）
- **ADR Governing**:
  - **ADR-0006（primary）— GameStateSnapshot 契约**：`FGameStateSnapshot` 为值语义 `USTRUCT`，仅含 POD/容器成员，**绝不内嵌任何系统的指针或可写引用**；AI 决策核心经 `const FGameStateSnapshot&` 接收只读消费；snapshot 由回合2 在 AI 决策阶段开头**一次性装配**、装配后该阶段内**冻结不变**；`Rent_top1/top2`/`PreaggregatedNlv` 装配期预聚合（**AI 严禁自算**，防 5→8 反向环）。
  - **ADR-0001 — UObject 宿主与生命周期**：snapshot 生成方=回合2 World Subsystem；聚合编排（CR-3.2/3.3/3.4）由回合2 ResolvePhase 发起。
  - **ADR-0007 — BP/C++ 权威边界**：AI 决策核心落 C++，headless 可注入 mock snapshot 测；snapshot 值语义天然可注入（构造 mock 字面量直接喂 hook）。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（ADR-0006：`USTRUCT` 值语义、`const FStruct&` UFUNCTION 传参、`TArray`/`TMap` 成员均 UE 长期稳定，无 post-cutoff 依赖）
- **Engine Notes（ADR-0006 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**。
  - Verification Required: None（引擎风险 LOW；唯一须验证的是性能 AC-S6/S7 实测耗时，属实现期）。
  - **GC 陷阱（GDD OQ-1⑮）**：若 `FGameStateSnapshot`（USTRUCT）内部持对 PlayerState（UObject）的引用须用 `TObjectPtr<>`/`UPROPERTY()` 标记（USTRUCT 非 GC root，裸指针不受 GC 追踪）；**推荐 snapshot 直接嵌入拷贝值（int32 Cash 等）而非持 UObject 引用，规格化「snapshot 内不得持 UObject 裸指针」**。
- **Control Manifest Rules（Core Layer）**:
  - **Required（Core）**: `FGameStateSnapshot` 为值语义 `USTRUCT`，仅含 POD/容器成员，绝不内嵌任何系统的指针或可写引用（source: ADR-0006）。snapshot 由回合2 编排在 AI 决策阶段开头一次性装配（不是 AI 自建），装配挂回合2 状态机宿主（source: ADR-0006）。装配完成后该决策阶段内冻结不变；三 hook 看同一份视图（source: ADR-0006）。`PreaggregatedNlv` 由回合2 装配期聚合 6/8——AI 严禁自算（防 5→8 反向环）（source: ADR-0006）。`AssembleSnapshot()` 签名建议 `FGameStateSnapshot AssembleSnapshot(int32 AiPlayerId) const`（source: ADR-0006）。缺字段降级：装配方未填某必需字段，对应 hook 降保守默认（false/[]/RollDouble）+ 单条 `UE_LOG`（source: ADR-0006）。
  - **Forbidden（Core）**: Never 让 AI 自建 snapshot（直取各系统单例）（source: ADR-0006）。Never 传 AI 一组 const 只读接口指针（const 接口仍是活指针）（source: ADR-0006）。Never 让派生量全部 hook 内现算（nlv 现算须触达经济 F-9→5→8 反向环）（source: ADR-0006）。Never 各系统各自向 snapshot 推送切片（装配点分散）（source: ADR-0006）。Never 让 AI 经 snapshot 写任何游戏状态/反向触达系统状态（source: ADR-0006）。Never 在 snapshot 装配后中途变更它（破坏冻结）（source: ADR-0006）。Never 让 AI 自算 `PreaggregatedNlv`（5→8 反向环）（source: ADR-0006）。
  - **Guardrail**: 单 AI 回合（装配+三 hook）总计 ≤16ms；snapshot 装配 <0.1ms（≤40 格 POD 拷贝）；hook 传参 0（`const&` 零拷贝）；`FGameStateSnapshot` <2.5KB/快照（source: ADR-0006）。

## Acceptance Criteria

- [ ] AC-1（TR-turn-007/TR-turn-008）: `FGameStateSnapshot` 为值语义 `USTRUCT(BlueprintType)`，仅含 POD/容器成员（int32/bool/enum/`TArray<FTileSnapshotEntry>`/预派生标量），不内嵌任何系统指针或可写引用；三 hook（`DecideBuyProperty`/`DecidePostRollActions`/`DecideJailAction`）签名为 `const FGameStateSnapshot&`（决策期冻结、AI 不写状态）。
- [ ] AC-2（TR-turn-009，GDD ADR-0006 IG#3）: snapshot 由回合2 在 AI 决策阶段开头一次性装配（`AssembleSnapshot(int32 AiPlayerId)`），装配后该阶段内冻结；`PreaggregatedNlv`/`Rent_top1/top2` 装配期预聚合 6/8，AI 严禁自算（断言 AI hook 内无对经济/建房单例的调用、无 nlv 现算）。
- [ ] AC-3（GDD AC-37a/AC-37d）: AI `PostRollAction` 同步决策——注入 mock AI 同步返回动作列表→框架执行毕→阶段切 TurnEnd、行动权移交下一未破产玩家；mock 返回空数组 `[]`→框架直接 EndTurn（执行 0 条即等价 EndTurn）。前提：AI hook 经 interface/依赖注入暴露（OQ-1⑤）；若选不可注入形态降 [Advisory]。
- [ ] AC-4（GDD AC-38/AC-38b）: ResolvePhase 买地决策点——AI 落无主地产→框架同步调 `DecideBuyProperty`（mock true/false）→购买意图按返回值触发/不触发；mock 返回 true 但 `Cash < Price`（或地产已被买）→委派经济5/所有权6 执行期校验→① 不扣成负现金；② 视同 false（地产未变更）；③ 回合不崩、正常推进。
- [ ] AC-5（GDD AC-39/AC-39b）: JailTurn AI hook——在狱 AI 进入→框架同步调 `DecideJailAction`（mock `PayBail`/`RollDouble`/`UseCard`）→出狱/留狱按返回值正确发起；非法值（`PayBail` 但现金不足 / `UseCard` 但无卡）→降级留狱（`JailTurnsServed+1`）、Cash 未变/出狱卡持有数未变、回合不崩。
- [ ] AC-6（GDD AC-42）: `DecidePostRollActions` 返回含「执行时已不可行」动作的列表→框架逐动作执行前重校验→① 不可行动作被跳过=该动作目标状态未被改变；② 后续合法动作仍执行；③ 回合不中断、处理完毕 EndTurn。覆盖「先抵押耗现金致后续建房付不起」与「先建房改均匀约束致后续建房违反约束」两 fixture 变体。
- [ ] AC-7（GDD AC-48/AC-49）: ResolvePhase 算租聚合——落他人有主地产需收租→① 调地产6 `BuildOwnershipSnapshot(playerId, tileIndex)` 恰 1 次；② 读建房8 `house_count`（8 未实现时注入缺省 `house_count==0`，经济 F-2 落 `RentTable[0]`/垄断无房分支）；③ 以「快照 + house_count + 当前程 dice_total(PULL)」调经济5 算租入口恰 1 次，参数值与上游一致。
- [ ] AC-8（GDD AC-50/AC-51）: 强制清算驱动循环（CR-3.3）——调 `6.Mortgage(tile)` 前先读 `8.GetHouseCount(tile)==0`（house_count>0 的 tile 调 Mortgage 次数恒==0、转卖房腿 `ForcedSellNextBuilding`）；清算顺序止损优先 mortgage-empty-first（空地 MV 最小先抵押→仍不足才卖房→`Cash≥amount_due` 早停→全资产耗尽调 `economy.is_insolvent` 1 次终止）。
- [ ] AC-9（GDD AC-52）: 破产 NLV 全组合聚合（CR-3.4）——Raising Funds 进 `is_insolvent` 前：① 调 `8.GetPlayerBuildings(player)` 恰 1 次（全组合枚举）；② `preaggregated_nlv` 按 `Σ house_count × floor(BuildingCost/2) + Σ MortgageValue(未抵押地)` 计算正确；③ 调 `economy.is_insolvent(player, amount_due, preaggregated_nlv)` 恰 1 次、传入值一致；④ economy 模块内部 zero `8.GetPlayerBuildings`/`8.GetHouseCount` 调用（spy 确认，无 5→8 环）。
- [ ] AC-10（GDD AC-45，Alpha [Advisory]）: `DecideAuctionBid` sentinel 值域——负数/`INDEX_NONE`→视同放弃；`0`→非法视同放弃；`>当前最高价 且 <=Cash 且 >=最低加价`→合法；违反任一→视同放弃。出价合法性校验归拍卖12；MVP 不触发。

## Implementation Notes

> 逐字保真自 ADR-0006 Implementation Guidelines + GDD CR-8/CR-3.2/3.3/3.4 + 四 hook 失败语义，不改写语义。

1. **`AssembleSnapshot()` 落回合2 编排（ADR-0006 IG#1）**：签名 `FGameStateSnapshot AssembleSnapshot(int32 AiPlayerId) const`——返回值快照，调用点立即以 `const&` 喂 hook（RVO/move，构造一次）。
2. **`Rent_top1/top2` 派生（ADR-0006 IG#2）**：遍历全盘对手未抵押地产，按 piecewise `property_rent` 口径（垄断×2 仅作用 house=0 的 base）求每块当前单次租金取前两高；垄断判定经地产6 `is_monopoly`；不足两块取现有之和，无对手地产=0。
3. **`PreaggregatedNlv` 由回合2 装配期聚合 6/8（ADR-0006 IG#3）——AI 严禁自算**（economy Risk-3 防 5→8 反向环）。
4. **冻结纪律（ADR-0006 IG#4）**：`AssembleSnapshot` 返回后该 struct 即冻结；`DecidePostRollActions` 贪心循环的现金预扣用 AI hook 内局部影子变量，不回写也不重读 snapshot。
5. **DI/可测（ADR-0006 IG#5）**：headless 测试构造 mock `FGameStateSnapshot` 字面量直接喂 hook（值语义天然可注入，无需 mock 任何系统接口）。
6. **缺字段降级（ADR-0006 IG#7）**：装配方未填某必需字段，对应 hook 降保守默认（false/[]/RollDouble）+ 单条 `UE_LOG`。
7. **四 hook 失败语义整簇契约（GDD L176-180）**：统一模式=执行期可行性校验归对应规则系统、不可行降级 + dev 日志、不崩不中断。#1 `DecideBuyProperty` 返 true 但现金不足/地产已被买→视同 false；#2 `DecideAuctionBid` 值域 sentinel；#3 `DecidePostRollActions` 过期动作逐动作重校 + 静默跳过；#4 `DecideJailAction` 非法返回→降级留狱（`JailTurnsServed+1`）。
8. **批处理失败策略（GDD L186）**：`DecidePostRollActions` 是基于决策时刻 T0 快照的一次性批处理；框架执行每条动作前委派对应规则系统（经济5/建房8/所有权6）重新校验当前实时状态可行性，不可行则静默跳过该条 + `UE_LOG` dev 诊断、继续执行后续，不中断整个回合；全列表处理完后 EndTurn。被跳过动作不广播 `OnAIActionExecuted`。
9. **CR-3.2/3.3/3.4 聚合（GDD L134-161）**：本系统 ResolvePhase 拥有聚合编排职责（只编排聚合与传递，不持有/不裁决任何归属或租金公式，归 6/8/5）；强制清算驱动循环顺序由经济规格注入（本系统拥执行驱动，经济拥顺序 spec + 现金判据）；无 economy→8/economy→6 反向边。

## Out of Scope

- AI 决策内部逻辑（`DecideBuyProperty`/`DecidePostRollActions`/`DecideJailAction` 怎么决策）→ ai-opponent epic（本系统只声明 hook 接口形态 + 调用编排 + 失败降级）。
- `FGameStateSnapshot` 字段穷举与 per-hook 字段充分性清单 → OQ-1 ADR（C-1/⑭）+ ai-opponent epic（本系统验值语义/冻结/装配方=回合2）。
- 经济5 算租公式 F-1..F-11 / 建房8 `ForcedSellNextBuilding` 实现 / 所有权6 `BuildOwnershipSnapshot` 实现 → 各自 epic（本系统验聚合编排恰 N 次调用 + 无反向环 spy）。
- AI hook 经骰子3 权威流取随机 → **story-006**（本 story 验 snapshot 装配/冻结/失败语义）。
- `OnAIActionExecuted` delegate 声明 → **story-004**（本 story 验框架执行 AI 动作时广播契约）。

## QA Test Cases

> 本 story 为 Integration（多系统聚合 + 跨界 hook + 无环 spy）；每 AC 一条 Given/When/Then；确定性 fixture、headless `-nullrhi`。snapshot 用 mock 字面量注入；6/8/5 用可 spy mock。

- **TC-1 (AC-1)**: GIVEN `FGameStateSnapshot` 定义，WHEN 静态/reflection 检查，THEN 仅 POD/容器成员、无 UObject 裸指针/可写引用；三 hook 签名为 `const FGameStateSnapshot&`。
- **TC-2 (AC-2)**: GIVEN AI 决策阶段，WHEN `AssembleSnapshot(AiPlayerId)`，THEN 一次性装配、阶段内冻结；spy 断言 AI hook 内无经济/建房单例调用、无 nlv 现算（`PreaggregatedNlv` 装配期预聚合）。
- **TC-3 (AC-3)**: GIVEN mock AI 返回非空动作列表 / 空数组 `[]`，WHEN 执行，THEN 前者执行毕切 TurnEnd 移交、后者直接 EndTurn。
- **TC-4 (AC-4)**: GIVEN AI 落无主地产 mock `DecideBuyProperty` 返回 true/false / true 但 `Cash<Price`，WHEN ResolvePhase，THEN 按返回值触发/不触发；现金不足时不扣负、视同 false、不崩。
- **TC-5 (AC-5)**: GIVEN 在狱 AI mock `DecideJailAction` 返回 `PayBail`/`UseCard` 合法/非法，WHEN JailTurn，THEN 合法按返回值发起、非法降级留狱（`JailTurnsServed+1`）、Cash/卡数未变、不崩。
- **TC-6 (AC-6)**: GIVEN `DecidePostRollActions` 返回含不可行动作（先抵押耗现金/先建房改约束两变体），WHEN 逐动作执行，THEN 不可行被跳过（目标状态未改）、后续合法动作执行、不中断、处理完 EndTurn。
- **TC-7 (AC-7)**: GIVEN 落他人有主地产收租，WHEN ResolvePhase 聚合，THEN `BuildOwnershipSnapshot` 恰 1 次、house_count（8 未实现注入 0）、调经济算租入口恰 1 次参数一致。
- **TC-8 (AC-8)**: GIVEN Cash<amount_due 持空地(MV 最小)+有房地，WHEN 清算循环，THEN 空地先 Mortgage（house_count>0 的 tile Mortgage 调用恒==0、转 ForcedSellNextBuilding）、`Cash≥amount_due` 早停、全耗尽调 `is_insolvent` 1 次终止。
- **TC-9 (AC-9)**: GIVEN 持 2 格建筑 + 未抵押地，WHEN Raising Funds 进 is_insolvent 前，THEN `GetPlayerBuildings` 恰 1 次、`preaggregated_nlv` 计算正确、调 `is_insolvent` 恰 1 次传入一致、economy zero→8 calls（spy）。
- **TC-10 (AC-10, Alpha)**: GIVEN mock `DecideAuctionBid` 各值域，THEN 负/INDEX_NONE/0→放弃、合法值→出价、违反→放弃（MVP 不触发）。
- **Edge cases**: snapshot 装配后中途变更须被禁止（冻结）；缺字段降级保守默认 + 单条 UE_LOG；单动作跨系统可行性由其 `ActionType` 主责系统聚合裁定（框架不逐系统串问）；强制清算有界终止（每迭代至少减一笔资产）。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/player-turn/gamestatesnapshot_assembly_ai_hooks_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-002（ResolvePhase/PostRollAction/JailTurn 阶段）、story-006（AI hook RNG + 当前程 dice_total PULL）、foundation-framework story-002（IGameClock DI）。
- **Unlocks**: ai-opponent epic（AI 决策核心消费本系统装配的 snapshot）；economy/property/building epic 经本系统聚合编排被调用。
