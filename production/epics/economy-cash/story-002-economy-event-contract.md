# Story 002: 经济事件契约 (4 delegate + EChangeReason)

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Complete
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
**Status**: [x] Created + passing（`EconomyEventsTest.cpp` 类目 `Rento.Economy.Events` 7 TC；全量 282/282）

---

## Completion Notes（2026-06-09，/dev-story mode-A）

**实现/改动文件**：
- `Source/rento/EconomyTypes.h`（新）— `EChangeReason`（9 值 uint8 append-only）+ 4 payload USTRUCT（FCashChangedInfo/FRentPaidInfo/FInsufficientFundsInfo/FBankruptcyDeclaredInfo）+ 4 DYNAMIC delegate 签名。
- `Source/rento/EconomySubsystem.h` / `.cpp`（**重命名** `EconomyCashSubsystem`→`EconomySubsystem`，类 `UEconomyCashSubsystem`→`UEconomySubsystem`）— 4 BlueprintAssignable delegate 成员；Credit/Debit/TransferCash 加 `EChangeReason` 参；广播构造 Info struct；EChangeReason append-only `static_assert`×9。
- `Source/rentoTests/Private/EconomyEventSpy.h` — spy 升级为收 USTRUCT payload + 加 OnRentPaid/OnBankruptcyDeclared 处理。
- `Source/rentoTests/Private/EconomyCashSubsystemTest.cpp` — econ-001 6 TC 适配新签名（reason 参 + 类名）。
- `Source/rentoTests/Private/EconomyEventsTest.cpp`（新）— AC-33~39 共 7 TC（Rento.Economy.Events）。

**AC 覆盖**：
- [x] **AC-33** OnCashChanged payload（含 EChangeReason + delta）— AC33_CashChangedPayload。
- [x] **AC-34** OnRentPaid 四字段 — AC34（契约 plumbing；实际收租触发 story-004/005）。
- [x] **AC-35** OnInsufficientFunds payload — AC35。
- [~] **AC-36** OnBankruptcyDeclared — **拆分**：本 story discharge {delegate 存在 + 2 字段反射（FBankruptcyDeclaredInfo）+ 1 次广播 plumbing}（AC36）；**时机子句「广播在资产转移之后」DEFERRED→story-009**（002 无真实破产移交序列可排序，CallLog 顺序断言须在 009 真实清算路径验，否则 vacuous）。
- [x] **AC-37** 事件次数（一次收租 1 OnRentPaid + 2 OnCashChanged reason=Rent；amount==0 全 0）— AC37。
- [x] **AC-38** 4 delegate BlueprintAssignable/FMulticastDelegateProperty + payload USTRUCT 反射 — AC38。
- [x] **AC-39** Credit/Debit/GetCash/TransferCash/GiveStartingCash BlueprintCallable + EChangeReason 9 值 — AC39。

**验证证据（主会话亲跑，铁律）**：Build Succeeded；全量 **282/282**（259+23warn，0 Fail/0 notRun，EXIT 0，275 基线+7 Events 零回归，**Foundation EventBusDelegateContract TC1-8 无回归**，`Saved/TestReport_econ002_commit`）；变异坐实非 vacuous（变异C 硬编码 Credit reason→AC-37/TC-2 reason==Rent 精确 FAIL，复绿）；AC-4 grep：econ 生产零直写只经 SetCash。

**设计裁定/偏离 + 协调债**：
- **DV-3（类重命名，用户 2026-06-09 裁定）**：`UEconomyCashSubsystem`→`UEconomySubsystem`（文件随名 `EconomySubsystem.{h,cpp}`）。对齐 Foundation `EventBusDelegateContractTest` TC8 预期（FindFirstObject("EconomySubsystem")）+ 系统5「经济与现金」全职责语义。econ-001 文件 git mv 保留历史。
- **discharge Foundation TC8 deferred（部分）**：本 story 在自身测试（AC36）兑现 OnBankruptcyDeclared {2 字段 + 存在性}（TC8 注释指示「在 economy story 对应测试补充」）；TC8「两事件并存未合并」（vs 破产9 OnPlayerBankrupt 3 字段）仍 DEFERRED→bankruptcy epic。
- **GiveStartingCash reason 决策**：起始资金经 `Credit(StartingCash, EChangeReason::Salary)`。**deliberate 复用 Salary**（EChangeReason GDD 固定 9 值无「开局/初始」语义）。**GDD propagate follow-up**：若下游 VFX/HUD 需区分「开局初始资金」vs「过 GO 发薪」，须 GDD 为 EChangeReason 追加 reason（append-only）；当前下游不区分。
- **EChangeReason 越界中性兜底**（Foundation TC7 deferred）：判定归**呈现层 consumer 职责**（econ-002 无 consumer，enum-owner 不实现 fallback）；留呈现 epic（VFX19/HUD16/audio22）兑现，非 econ 缺口。
- **TransferCash Credit-after-Debit ensure**（既有 econ-001 硬化）：Shipping 下 ensure 不崩，极端不可达路径（同帧二次 ResolvePlayer 不一致）理论静默毁币；MVP 单线程接受，Full Vision 联网阶段复查。

---

## Dependencies

- Depends on: Story 001（Credit/Debit/GetCash + 不变式）
- Unlocks: Story 003–010（各 F 公式广播 OnCashChanged/OnRentPaid；破产 OnBankruptcyDeclared）
