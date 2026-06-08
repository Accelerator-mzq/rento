# Story 002: 经济事件契约 (4 delegate + EChangeReason)

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/economy-cash.md`
**Requirement**: `TR-econ-005`, `TR-econ-006`, `TR-econ-007`, `TR-econ-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0003（事件总线，primary）· ADR-0007（BlueprintCallable 边界）
**ADR Decision Summary**: 去中心化 owner-held `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable`；命名 `On<PastTense>`，payload `F<Event>Info` USTRUCT（全 blittable）；**方向由消费方派生**（`EChangeReason` 无方向位）；owner 先同步算定权威结果再广播。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `DYNAMIC_MULTICAST_DELEGATE` + `BlueprintAssignable` 稳定；payload USTRUCT 须 `BlueprintType`、字段 blittable。`-nullrhi` 可测（spy 订阅断言次数/payload）。

**Control Manifest Rules (Foundation/事件总线 ADR-0003)**:
- Required: 事件 `On<PastTense>`（`OnCashChanged`/`OnRentPaid`）；payload `F<Event>Info`；单一事件源（每状态变更恰一 owner 广播）；**方向由消费方派生**（payload 携 `EChangeReason`/Payer/Payee，方向由 delta 符号/视角派生，owner 不为每方向各发一事件）；owner 先算定再广播。
- Forbidden: 集中式 Event Bus 注册表（单一 `UGameEventBus`）；非 dynamic delegate 暴露呈现层；**把破产事件合并删 `OnBankruptcyDeclared`**（破坏经济5 AC-36）。
- Guardrail: broadcast O(订阅数)，MVP 回合制低频可忽略。

---

## Acceptance Criteria

- [ ] **AC-33** `OnCashChanged(Player,OldCash,NewCash,EChangeReason)`：Credit(p,200) reason=Salary → OldCash==旧 ∧ NewCash==旧+200 ∧ reason==Salary ∧ `NewCash−OldCash` 与实际变化等值。
- [ ] **AC-34** `OnRentPaid(Payer,Payee,Amount,TileIndex)`：四字段 == 实际转移（Amount==rent、Payer/Payee/Tile 正确）。
- [ ] **AC-35** `OnInsufficientFunds(Player,AmountDue,AmountShort)`：Cash=50，due=170 → AmountDue==170 ∧ AmountShort==120。
- [ ] **AC-36** `OnBankruptcyDeclared(Debtor,Creditor)`：破产移交完成后广播一次（不多不少），时机在资产转移之后。
- [ ] **AC-37** 事件次数精确：一次收租 → 恰 1 次 OnRentPaid + 2 次 OnCashChanged（payer/payee 各一，reason=Rent）；amount==0 → 0 次（对齐 AC-5）。
- [ ] **AC-38** 四事件均标 `UPROPERTY(BlueprintAssignable)`、`DYNAMIC_MULTICAST_DELEGATE`、payload `USTRUCT(BlueprintType)`、编译通过。
- [ ] **AC-39** `Credit`/`Debit`/`GetCash` 标 `UFUNCTION(BlueprintCallable)`、BP 可调、编译通过；`EChangeReason` 含全部既有项（Salary/Rent/Purchase/Mortgage/Unmortgage/Tax/Build/Trade/Bankruptcy）。

---

## Implementation Notes

*Derived from ADR-0003:*

- 定义 4 个 owner-held `DYNAMIC_MULTICAST_DELEGATE`（本系统宿主持有，非集中 Bus）：`OnCashChanged`、`OnRentPaid`、`OnInsufficientFunds`、`OnBankruptcyDeclared`，均 `UPROPERTY(BlueprintAssignable)`。
- payload 用 `USTRUCT(BlueprintType)` 或多参 delegate（与 Foundation 既有 delegate 风格一致，参 player-turn/dice spy 模式）；字段全 blittable `int32`/`enum`。
- **`EChangeReason` 枚举 9 值无方向位**：`Salary/Rent/Purchase/Mortgage/Unmortgage/Tax/Build/Trade/Bankruptcy`（uint8 基底，append-only）。**禁** `RentReceived`/`RentPaid`/`PassGo`/`JailFine`/`BuildCost` 等方向子值/别名（方向由 `NewCash−OldCash` delta 符号 + `OnRentPaid` 的 Payer/Payee 视角派生，见 GDD 方向 reconcile 裁定）。
- **owner 先算定再广播**：本系统先同步完成 Cash 写（权威结果），再广播事件；呈现层纯叶子只订阅。
- **`OnBankruptcyDeclared` 是经济5 现金侧信号**（与破产9 `OnPlayerBankrupt` 切分，禁双发；下游主反馈订经济5 此事件）。实际触发在 story-009（破产现金侧完成后）；本 story 定义 delegate + 验 AC-36 时机/次数（用 spy 驱动一次模拟破产移交）。
- 本 story 把 story-001 的占位广播点接入正式 delegate：`Credit`/`Debit` 成功后广播 `OnCashChanged`；`Debit` 不足时广播 `OnInsufficientFunds`。`OnRentPaid` 由收租路径（story-004/005）调用，本 story 定义契约 + spy 验四字段/次数。

---

## Out of Scope

- Story 001：Cash 服务 + 受控写不变式（本 story 依赖其 Credit/Debit）。
- Story 004/005：实际收租触发 `OnRentPaid`（本 story 只定义契约 + spy 验 payload/次数）。
- Story 009：实际破产触发 `OnBankruptcyDeclared`（本 story 验 AC-36 时机/次数语义）。
- 下游 VFX19/HUD16/audio22 的订阅与方向派生消费（呈现层，非本 epic）。

---

## QA Test Cases

- **AC-33**：Given Cash=300；When Credit(p,200,Salary)；Then OnCashChanged(p,300,500,Salary) 恰 1 次 ∧ delta=+200。
- **AC-34**：Given 收租 payer→payee Amount=50 Tile=21；Then OnRentPaid(payer,payee,50,21) 四字段精确。
- **AC-35**：Given Cash=50, due=170；When 强制 Debit(170)；Then OnInsufficientFunds(p,170,120) 恰 1 次。
- **AC-36**：模拟破产移交（spy 驱动）；Then OnBankruptcyDeclared(debtor,creditor) 恰 1 次 ∧ 时机在资产转移之后（spy CallLog 顺序断言）。
- **AC-37**：一次收租 → OnRentPaid×1 + OnCashChanged×2（payer/payee 各 reason=Rent）；amount==0 收租 → 全 0 次。
- **AC-38**：编译期 + reflection：4 delegate 均 BlueprintAssignable/DYNAMIC_MULTICAST，payload USTRUCT(BlueprintType)。
- **AC-39**：编译期：Credit/Debit/GetCash 均 UFUNCTION(BlueprintCallable)；`static_assert`/枚举核验 EChangeReason 含 9 既有值。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/economy_events_test.cpp`（类目 `Rento.Economy.Events`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（Credit/Debit/GetCash + 不变式）
- Unlocks: Story 003–010（各 F 公式广播 OnCashChanged/OnRentPaid；破产 OnBankruptcyDeclared）
