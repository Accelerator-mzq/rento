---
Epic: foundation-framework
Status: Ready
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 005 — Event Bus 纪律层：统一 delegate 规范 + USTRUCT payload 契约

## Context

- **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 模块② Event Bus——去中心化 owner-held delegate 命名/payload/单一事件源纪律层）
- **Requirement TR-ID**: 框架面支撑 **TR-turn-004**（6 个回合事件用 dynamic multicast delegate + BlueprintAssignable、1-对-多去中心化）、**TR-turn-005**（payload 必须 USTRUCT(BlueprintType)，裸 TArray 须包入 USTRUCT）、**TR-dice-005**（`OnDiceRolled(FDiceRollResult)` 事件契约：DYNAMIC_MULTICAST_DELEGATE + BlueprintAssignable + USTRUCT payload）。本 story 提供这些 TR 的**事件总线规范底座**（宏/payload/命名/单一事件源/方向派生 + 防双触发约定）；各事件的具体广播时机与字段语义归各 owner epic。
- **ADR Governing**:
  - **ADR-0003（primary）— 事件总线与 Delegate 契约**：采去中心化 owner-held `DYNAMIC_MULTICAST_DELEGATE`（Foundation ② Event Bus = 命名/payload 纪律层，非中转对象）；多字段 payload 必包 `USTRUCT(BlueprintType)`；单一事件源（每个状态变更恰一个 owner 广播）；方向由消费方派生；破产职责切分（下游主反馈订经济5 `OnBankruptcyDeclared`）。
  - **ADR-0001（secondary）— 宿主生命周期**：订阅生命周期边界 = 宿主 `Initialize`（绑）/`Deinitialize`（解绑），PIE Stop 不留野绑定。
- **Engine**: Unreal Engine 5.7（C++ 框架 + BP 订阅侧）— **Risk: LOW**（`DYNAMIC_MULTICAST_DELEGATE`/`BlueprintAssignable`/USTRUCT payload 自 UE4 稳定，pre-5.3 训练覆盖，无 post-cutoff 行为变更）
- **Engine Notes（ADR-0003 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**（delegate 宏与 `BlueprintAssignable` 均非 5.4–5.7 新增）。
  - Verification Required（ADR-0003 验证(1)，Sprint0 #10）：`DYNAMIC_MULTICAST_DELEGATE` 仍要求 payload 为单一 `USTRUCT(BlueprintType)` 或 ≤ 引擎参数上限的裸参；**裸 `TArray` 作 BlueprintAssignable 参数仍编译失败**（`OnTurnOrderResolved` 须包 USTRUCT）。
- **Control Manifest Rules（Foundation Layer / 事件总线）**:
  - **Required**: 宏一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_NParams]` + `UPROPERTY(BlueprintAssignable, Category="Events")`；BP 与 C++ 双向可订（source: ADR-0003）。多字段 payload 必包 `USTRUCT(BlueprintType)`（如 `FDiceRollResult`/`FPhaseChangedInfo`/`FTurnOrderResult`）；`OnTurnOrderResolved` 必须 USTRUCT 封装（source: ADR-0003）。命名：事件 `On<PastTense>`；payload struct `F<Event>Info`（source: ADR-0003）。
  - **Required**: 单一事件源——每个状态变更恰一个 owner 广播。`OnGameWon` 由回合2 触发（破产9 return-only）；`OnBuildingChanged` **2 字段**（actingPlayerId 由回合2 上下文取，非塞事件第3字段）；`OnAIActionExecuted` 由回合2 执行 AI 动作时广播（非 AI 发）（source: ADR-0003）。方向由消费方派生：payload 携类型语境（`EChangeReason`/Payer/Payee 视角），方向由消费方从 delta 符号派生，owner 不为每个方向各发一个事件（source: ADR-0003）。字段「只增不改语义」；枚举 append-only（source: ADR-0003）。未注册枚举值兜底：消费方对未知枚举走中性兜底（不崩不静默错播）（source: ADR-0003）。owner 系统先同步算定权威结果，再广播事件（结算同步先行，呈现事件后随）（source: ADR-0003）。
  - **Forbidden**: Never 用集中式 Event Bus 注册表对象（单一 `UGameEventBus` 持全部 delegate）——模糊「owner 即广播者」单一事件源不变式（source: ADR-0003）。Never 用混合事件拓扑（高频 juice 走 owner delegate，低频跨域走集中 Bus）（source: ADR-0003）。Never 把裸 `TArray` 作 BlueprintAssignable 参数（编译失败——`OnTurnOrderResolved` 须包 USTRUCT）（source: ADR-0003）。Never 把非 dynamic delegate 暴露给呈现层（BP 不可订）（source: ADR-0003）。Never 把破产事件合并为单事件（删经济5 `OnBankruptcyDeclared`）（source: ADR-0003）。
  - **Global（横切）**: 事件命名/payload 纪律——`On<PastTense>` + 多字段必包 `USTRUCT(BlueprintType)` + 字段只增不改语义 + 单一 owner 广播；新增订阅同步更新 architecture.md §D.2 权威矩阵（source: ADR-0003）。
  - **Guardrail**: delegate broadcast O(订阅数)，MVP 回合制低频可忽略（source: ADR-0003）。

## Acceptance Criteria

- [ ] AC-1: Event Bus 是**纪律层规范 + 工具函数集，非对象**——代码中不存在集中式 `UGameEventBus` 注册表对象持全部 delegate（grep 无此类）。
- [ ] AC-2: 所有暴露给呈现层的事件均以 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_NParams]` + `UPROPERTY(BlueprintAssignable, Category="Events")` 声明，BP 与 C++ 双向可订；无非-dynamic delegate 暴露给呈现层。
- [ ] AC-3: 多字段 payload 一律包 `USTRUCT(BlueprintType)`；`OnTurnOrderResolved` 以 `FTurnOrderResult` USTRUCT 封装——**裸 `TArray` 作 BlueprintAssignable 参数编译失败**（编译通过即验证此约束被遵守）。
- [ ] AC-4: 事件命名遵 `On<PastTense>`、payload struct 遵 `F<Event>Info`/语义名（命名规范 grep 核验）。
- [ ] AC-5: 单一事件源约定成立——每个状态变更恰一个 owner 广播：`OnGameWon` 仅回合2 广播（破产9 return-only，不直触发）；`OnBuildingChanged` 为 2 字段（无第3字段塞 actingPlayerId）；`OnAIActionExecuted` 仅回合2 广播。
- [ ] AC-6: 方向由消费方派生——owner 不为每个方向各发一个事件；payload 携类型语境（`EChangeReason`/Payer/Payee）。
- [ ] AC-7: 消费方对未注册枚举值（`EChangeReason`/`EArrivalContext`/`EJailReason`）走中性兜底，不崩不静默错播。
- [ ] AC-8: 破产双事件源职责切分成立——经济5 `OnBankruptcyDeclared`(2字段) 为主反馈源、破产9 `OnPlayerBankrupt`(3字段+reason) 为增量源；两事件未被合并。

## Implementation Notes

> 逐字保真自 ADR-0003 统一 Delegate 规范 + Key Interfaces，不改写语义。

1. **宏**：一律 `DECLARE_DYNAMIC_MULTICAST_DELEGATE[_OneParam/_TwoParams/...]` + `UPROPERTY(BlueprintAssignable, Category="Events")`。禁用非 dynamic delegate 暴露给呈现层（BP 不可订）。
2. **Payload 容器**：单参或语义内聚的多字段 → 包 `USTRUCT(BlueprintType)`（如 `FDiceRollResult`、`FPhaseChangedInfo{Phase,ConsecutiveDoubles}`、`FTurnOrderResult{OrderedPlayerIds,bResolvedBySeatTiebreak}`）。裸 `TArray` 禁作 BlueprintAssignable 参数——`OnTurnOrderResolved` 必须 USTRUCT 封装。字段「只增不改语义」；枚举 append-only。
   ```cpp
   USTRUCT(BlueprintType)
   struct FPhaseChangedInfo {
       GENERATED_BODY()
       UPROPERTY(BlueprintReadOnly) ETurnPhase Phase;
       UPROPERTY(BlueprintReadOnly) int32 ConsecutiveDoubles;
   };
   DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, const FPhaseChangedInfo&, Info);
   // owner（回合2）持有，唯一广播者
   UPROPERTY(BlueprintAssignable, Category="Events")
   FOnPhaseChanged OnPhaseChanged;
   ```
3. **命名**：事件 `On<PastTense>`（`OnRentPaid`/`OnCashChanged`/`OnBuildingChanged`）；payload struct `F<Event>Info` 或语义名。
4. **方向由消费方派生**：payload 携类型语境（`EChangeReason`/`EArrivalContext`/Payer/Payee 视角），方向（正负/收付）由消费方从 delta 符号 + Payer/Payee 视角派生，owner 不为每个方向各发一个事件。
5. **单一事件源**：`OnGameWon` 由回合2 触发（破产9 仅返回 winnerId，return-only）；`OnBuildingChanged` 2 字段；`OnAIActionExecuted` 由回合2 广播。
6. **破产双事件源职责切分（方案1）**：经济5 `OnBankruptcyDeclared(Debtor,Creditor)` = 现金侧完成信号（主反馈订此）；破产9 `OnPlayerBankrupt(Debtor,Creditor,Reason)` = 编排末 + reason 语境（仅 reason/creditor 增量订此，禁重播主反馈）。
   ```cpp
   DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBankruptcyDeclared, int32, Debtor, int32, Creditor);
   DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerBankrupt, int32, Debtor, int32, Creditor, EBankruptcyReason, Reason);
   ```
7. **未注册枚举值兜底**：消费方对未知 `EChangeReason`/`EArrivalContext`/`EJailReason` 走中性兜底（运行期安全，不崩不静默错播）。
8. 呈现层订阅以 **architecture.md §Data Flow D.2 事件总览表为权威矩阵**；新增订阅必须同步更新该表（review 据此核单一事件源 + 防双订阅）。

## Out of Scope

- 读档集中解绑/重绑工具函数（`RebindPresentationDelegates`、防 EC-8 双订阅）→ **story-006**。
- 各事件的具体广播实现与字段语义（`OnDiceRolled` 由 dice、`OnCashChanged` 由 economy、`OnBuildingChanged` 由 building 等）→ 各 owner epic。
- 订阅侧 BP-vs-C++ 归属（哪些 C++ 订、哪些 BP widget 订）→ ADR-0007（Core epic），本 story 只定 delegate 形态。
- 破产主反馈「恰 1 次」负向 spy AC 的实测 → Presentation epic（VFX/Audio 订阅侧）。

## QA Test Cases

> Integration（事件契约跨 owner→consumer 边界）；headless `-nullrhi`，spy 直接挂 owner delegate。

- **TC-1 (AC-1)**: GIVEN 代码库，WHEN grep 集中式 `UGameEventBus` 注册表对象，THEN 不存在（Event Bus 为纪律层非对象）。
- **TC-2 (AC-2)**: GIVEN 一个示例 owner delegate（如 `OnPhaseChanged`），WHEN BP `Bind Event` + C++ `AddDynamic` 各订一次并广播，THEN 两端各收到 1 次回调（BP/C++ 双向可订）。
- **TC-3 (AC-3)**: GIVEN `OnTurnOrderResolved` 声明，WHEN 编译，THEN 以 `FTurnOrderResult` USTRUCT 封装编译通过；裸 `TArray` 版本编译失败（验证约束生效）。
- **TC-4 (AC-4)**: GIVEN 事件与 payload 名集合，WHEN grep 命名，THEN 事件全 `On<PastTense>`、payload 全 `F<Event>Info`/语义名。
- **TC-5 (AC-5)**: GIVEN 一局完整流程，WHEN spy 挂 `OnGameWon`/`OnBuildingChanged`/`OnAIActionExecuted`，THEN `OnGameWon` 仅回合2 广播源、`OnBuildingChanged` payload 恰 2 字段、`OnAIActionExecuted` 仅回合2 广播。
- **TC-6 (AC-6)**: GIVEN `OnCashChanged(Player,Old,New,EChangeReason)`，WHEN 消费方收到一次现金增、一次现金减，THEN 同一事件单源，方向由 `New-Old` 符号派生（无成对的"增事件/减事件"）。
- **TC-7 (AC-7)**: GIVEN 消费方收到未注册 `EChangeReason` 值，WHEN 处理，THEN 走中性兜底分支，不崩、不错播。
- **TC-8 (AC-8)**: GIVEN 经济5 与破产9 两 delegate 声明，WHEN 检查，THEN `OnBankruptcyDeclared` 2 字段、`OnPlayerBankrupt` 3 字段+reason，二者并存未合并。
- **Edge cases**: 同一事件多订阅者广播一次全部收到（去中心化 1-对-多）；广播在「同步算定结果之后」（结算先行，spy 读到的状态已是权威结果）。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/foundation/event_bus_delegate_contract_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-001（订阅生命周期锚宿主 `Initialize`/`Deinitialize`）。
- **Unlocks**: story-006（读档重绑工具基于本规范的 owner delegate 集合）；各 owner epic 的事件发射 story（dice/turn/economy/building/bankruptcy）；Presentation epic 订阅 story。
