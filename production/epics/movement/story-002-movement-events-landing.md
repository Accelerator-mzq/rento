# Story 002: 事件契约 (OnPawnMoveStarted/OnPawnLanded + EArrivalContext) + 落地移交

> **Epic**: 移动 Movement
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/movement.md`（CR-5 落地移交 / 事件广播）
**Requirement**: `TR-move-006`、`TR-move-007`、`TR-move-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0003（事件总线，primary）
**ADR Decision Summary**: `EArrivalContext` `UENUM(BlueprintType)`+uint8 append-only；`OnPawnMoveStarted`/`OnPawnLanded` owner-held `DYNAMIC_MULTICAST_DELEGATE`+`BlueprintAssignable`；落地移交 ResolvePhase。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: ⚠ delegate 宏 token/签名须对 UE5.7 核实（VERSION.md 知识缺口，LLM ~5.3）；`UENUM(BlueprintType)` 须 uint8 底层（勿 `:int32`）。两事件"编译通过"由 CI headless `-nullrhi` 构建验证（无 Spec 空壳）。

**Control Manifest Rules (Foundation/ADR-0003)**:
- Required: 事件 `On<PastTense>`（`OnPawnMoveStarted`/`OnPawnLanded`）；payload 字段含语境（arrivalContext）方向由消费方派生；owner 先算定再广播（结算同步先行，呈现事件后随）。
- Forbidden: 非 dynamic delegate 暴露呈现层；集中式 Event Bus；重排枚举整数索引（破坏旧档）。
- Guardrail: broadcast O(订阅数)，回合制低频可忽略。

---

## Acceptance Criteria

- [ ] **AC-16** `OnPawnMoveStarted(Player,From,To,Steps,PassedGo)` 字段含正/反向：(a) from=38,steps=5 → From==38,To==3,Steps==5,PassedGo==1；(b) from=2,steps=−5 → To==37,PassedGo==−1。
- [ ] **AC-17** 事件次数精确：一次 `Advance` → 恰 1 次 `OnPawnMoveStarted` + 1 次 `OnPawnLanded`（steps=0 见 AC-9a：0 次 Landed）。
- [ ] **AC-14** `arrivalContext` 透传：`Advance(steps=5, context=AdvanceCard)` → `OnPawnLanded.arrivalContext==AdvanceCard`。移动透传调用方注入的 context，不自决分类。
- [ ] **AC-15** [Advisory·code-review] `SentToJail` 抑制买地/收租：移动侧 context 强制取值由 AC-12b 覆盖；**实际抑制 ResolvePhase 分支归 player-turn（已补 AC-46，propagate RESOLVED 2026-06-02）**——移动不执行 ResolvePhase。
- [ ] **AC-20** [Logic·CI 构建] `EArrivalContext` 初始**恰含** `{DiceMove, AdvanceCard, SentToJail}`（之后只扩不改）；`UENUM(BlueprintType)`+uint8。两事件 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*Params`+`BlueprintAssignable` 编译通过（CI 构建验证）。

---

## Implementation Notes

*Derived from ADR-0003:*

- **`EArrivalContext`**：`UENUM(BlueprintType)` uint8，初始恰 `{DiceMove, AdvanceCard, SentToJail}`（append-only，枚举项数=可测断言 AC-20；勿 `:int32`）。
- **两事件**（owner-held `DYNAMIC_MULTICAST_DELEGATE`+`BlueprintAssignable`，本系统宿主持有，非集中 Bus）：
  - `OnPawnMoveStarted(Player, From, To, Steps, PassedGo)`（5 字段，VFX19 逐格 hop 回放 + 过 GO 高亮）。
  - `OnPawnLanded(Player, TileIndex, EArrivalContext)`（落地 juice + **移交回合2 ResolvePhase**）。
- **arrivalContext 透传（AC-14）**：移动透传调用方注入的 context（DiceMove/AdvanceCard 由回合2/事件7 区分传入），**不自决分类**。`SentToJail` 在 `TeleportTo(paysGo=false)` 由移动强制置（story-004 AC-12b）。
- **落地移交**：`OnPawnLanded` 广播 = 移交回合2 ResolvePhase 落地结算（买地/收租/抽牌等归回合2）。`SentToJail` 抑制买地/收租的**实际分支**归 player-turn AC-46（已 propagate RESOLVED），移动只透传 context。
- **owner 先算定再广播**：移动先同步写定位置 + 发薪 push（story-003），再广播事件（三步有序契约 story-005）。
- 本 story 定义事件 + EArrivalContext 契约；实际广播由 Advance（story-003）/TeleportTo（story-004）调用，本 story 验 payload 字段/次数（spy 驱动一次移动）。

---

## Out of Scope

- Story 003/004：Advance/TeleportTo 实际触发广播（本 story 定义契约 + spy 验 payload/次数）。
- player-turn 的 ResolvePhase 落地结算 + SentToJail 抑制（AC-46，player-turn epic 已实现）。
- 下游 VFX19 hop 回放 + experiential 体验（presentation，OQ-T-Move-3）。

---

## QA Test Cases

- **AC-16**：(a) from=38,steps=5 → OnPawnMoveStarted(p,38,3,5,1)；(b) from=2,steps=−5 → (...,37,−5,−1)。四/五字段精确含反向。
- **AC-17**：一次 Advance → OnPawnMoveStarted×1 + OnPawnLanded×1；steps=0 → Landed×0（AC-9a）。
- **AC-14**：Advance(context=AdvanceCard) → OnPawnLanded.arrivalContext==AdvanceCard（透传）。
- **AC-15**：[code-review] 移动不执行 ResolvePhase；抑制归 player-turn AC-46（cross-ref）。
- **AC-20**：[CI 构建] EArrivalContext 恰 3 项 + uint8；两 delegate BlueprintAssignable 编译通过（headless）。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/movement/movement_events_test.cpp`（类目 `Rento.Movement.Events`）— 存在且通过（AC-20 部分由 CI 构建）。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（SetTileIndex/GetTileIndex）
- Unlocks: Story 003/004（Advance/TeleportTo 广播）、005（三步有序）
