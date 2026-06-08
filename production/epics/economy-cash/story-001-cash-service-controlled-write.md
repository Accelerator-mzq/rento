# Story 001: Cash 服务 + Credit/Debit 受控写 + 守恒/不变式

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD — sprint planning]
> **Manifest Version**: 2026-06-06
> **Last Updated**: 2026-06-08

## Context

**GDD**: `design/gdd/economy-cash.md`
**Requirement**: `TR-econ-001`, `TR-econ-002`, `TR-econ-018`（amount≥0 部分 `TR-econ-014`）
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001（UObject 宿主与生命周期，primary）· ADR-0007（BP-vs-C++ 边界，封装）· ADR-0014（amount≥0/溢出）
**ADR Decision Summary**: 经济服务挂 per-match `UWorldSubsystem`（一局边界，PIE Stop 销毁）；`Cash` 写权威落 C++ private 容器，只经 `Credit`/`Debit` 受控写；金钱运算整数纯净。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 无 post-cutoff API。`-nullrhi` headless 可测。宿主 `Initialize`/`Deinitialize` 生命周期；`Cash` 容器 `UPROPERTY(SaveGame)`（序列化在 story-010）。

**Control Manifest Rules (Core/Foundation)**:
- Required: per-match 服务挂 `UWorldSubsystem`（生命周期=一局）；**写权威状态 `Cash` → C++**；`Credit`/`Debit`/`GetCash` 标 `UFUNCTION(BlueprintCallable)`。
- Forbidden: 把 per-match 对局态挂 `UGameInstanceSubsystem`（PIE 隔离破裂）；外部系统直写 `PlayerState.Cash`（绕过受控写）。
- Guardrail: 宿主不持状态写语义（仅持服务实例与生命周期，`Cash` 写语义归本系统）。

---

## Acceptance Criteria

*From GDD `design/gdd/economy-cash.md`，scoped to this story:*

- [ ] **AC-1** `Cash ≥ 0` 不变式：Cash=50，Debit(80) → Cash 仍==50 ∧ `OnInsufficientFunds` 触发 ∧ 无 `OnCashChanged`（不落地为负，转 Raising Funds）。
- [ ] **AC-2** 原子转移守恒（CR-8）：payer.Cash=C_p / payee.Cash=C_e / rent=R（R≤C_p）→ `payer.Cash==C_p−R` ∧ `payee.Cash==C_e+R` ∧ 总和不变（同一 R 局部变量用于 Debit 与 Credit，非两次重算）。
- [ ] **AC-3** `Credit`/`Debit` 单调：`Credit(p,a)` 使 `Cash+=a`、`Debit(p,a)`（a≤Cash）使 `Cash−=a`，精确等值、不副作用他人 Cash。
- [ ] **AC-4** [Advisory·code-review] 受控写软约束：全代码库无外部系统直写 `PlayerState.Cash`，只经 `Credit`/`Debit`。BP-primary→code-review；C++ 强封装可加禁直写扫描升 [Logic]。
- [ ] **AC-5** `amount==0` 早返静默：amount=0（passed_go=0 / 自有地租金=0 / MV=0）→ 不调 Credit/Debit、不广播 `OnCashChanged`。
- [ ] **amount<0 契约（CR-1 R1 blocker#2）**：`Credit`/`Debit` 的 `amount<0` 是非法调用 → 运行时 assert 拒绝（丢弃 + dev log）、不改 Cash、不广播。

---

## Implementation Notes

*Derived from ADR-0001/0007/0014 Implementation Guidelines:*

- 经济服务 = `UWorldSubsystem` 子类（per-match，PIE 隔离）。`Cash` 容器为 C++ `private` 成员（`TMap<int32,int32> CashByPlayer` 或等价；以 player 为键）。**无 `BlueprintReadWrite`**。
- 公共写仅 `Credit(int32 PlayerId, int32 Amount)` / `Debit(int32 PlayerId, int32 Amount)`，读 `GetCash(int32 PlayerId)`，均 `UFUNCTION(BlueprintCallable)`。
- **`Cash≥0` 不变式硬守**：`Debit` 内若 `Amount > Cash` → 不扣、广播 `OnInsufficientFunds`、不广播 `OnCashChanged`（转 Raising Funds，状态机在 story-009）。
- **amount 非负**：`Credit`/`Debit` 入口 `if (Amount < 0) { /* dev log + 拒绝 */ return; }`；`Amount==0` 早返不广播（AC-5）。
- **原子守恒（CR-8）**：收租转移用**单一 R 局部变量**同时 `Debit(payer,R)`+`Credit(payee,R)`，禁两次重算金额。银行（发薪/抵押放款/税）视为无限池（CR-8）。
- 开局初始现金 = `STARTING_CASH`（Tuning Knob，数据驱动，AC-41 smoke）。
- 宿主 `Initialize` 绑 delegate、`Deinitialize` 全解绑（事件定义在 story-002，本 story 仅建宿主+容器+受控写骨架，`OnInsufficientFunds`/`OnCashChanged` 广播由 story-002 落 delegate 后接入；本 story 可先用前向声明/占位 spy 验证不变式逻辑）。

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002：4 个事件 delegate 的正式定义（`OnCashChanged`/`OnRentPaid`/`OnInsufficientFunds`/`OnBankruptcyDeclared`）+ payload USTRUCT + EChangeReason。本 story 只验受控写/不变式逻辑（用占位 spy 断言"是否应广播"）。
- Story 003–008：各 F 公式（发薪/租金/抵押/税/NLV）。
- Story 009：Raising Funds 状态机 + 破产现金侧。
- Story 010：`Cash` 存档序列化 round-trip。

---

## QA Test Cases

*嵌自 GDD AC（已含确定性 fixture/定值）。developer 按此实现，勿另造。*

- **AC-1**：Given Cash=50；When Debit(player,80)；Then Cash==50 ∧ OnInsufficientFunds 触发 1 次 ∧ OnCashChanged 0 次。Edge：Debit(50)（恰等额）→ Cash==0 成功、不触 InsufficientFunds。
- **AC-2**：Given payer.Cash=100, payee.Cash=20, R=30；When 收租转移；Then payer==70 ∧ payee==50 ∧ 总和 120 不变。Edge：R==payer.Cash（全额）→ payer==0。
- **AC-3**：Given Cash=100；When Credit(p,40)→140，Debit(p,25)→115；Then 精确等值 ∧ 他玩家 Cash 不变（spy 断言）。
- **AC-4**：[code-review] grep 权威模块外无 `.Cash =` / 直写 PlayerState.Cash 路径；只经 Credit/Debit。
- **AC-5**：Given amount=0；When Credit(p,0)/Debit(p,0)；Then Cash 不变 ∧ OnCashChanged 0 次 ∧ 无 Credit/Debit 实际记账。
- **amount<0**：Given amount=−10（dev/Test build）；When Credit(p,−10)/Debit(p,−10)；Then Cash 不变 ∧ dev log 捕获（`AddExpectedError`）∧ 不广播。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/cash_service_test.cpp`（物理：`Source/rentoTests/Private/EconomyCashSubsystemTest.cpp`，类目 `Rento.Economy.CashService`）— 必须存在且通过。
**Status**: [x] Created + passing（6 TC，全量 275/275，`Saved/TestReport_econ001_final`）

---

## Completion Notes（2026-06-09，/dev-story mode-A）

**实现文件（4 新建）**：
- `Source/rento/EconomyCashSubsystem.h` / `.cpp` — `UEconomyCashSubsystem : UMatchSubsystemBase`（per-match UWorldSubsystem）；`GetCash`/`Credit`/`Debit`/`TransferCash`/`GiveStartingCash` + 占位 delegate `OnCashChanged`/`OnInsufficientFunds`。
- `Source/rentoTests/Private/EconomyEventSpy.h` — DYNAMIC delegate 计数 spy（UCLASS，G3）。
- `Source/rentoTests/Private/EconomyCashSubsystemTest.cpp` — 6 TC（类目 `Rento.Economy.CashService`）。

**AC 覆盖**：
- [x] **AC-1** Cash≥0 不变式 — TC-1（+edge 恰等额）：Cash=50,Debit(80)→Cash==50 ∧ InsufficientFunds 1 次 ∧ CashChanged 0 次。
- [x] **AC-2** 原子守恒 CR-8 — TC-2（+edge 全额）：payer100/payee20/R30→70/50,总和 120 不变,单一 Amount 双腿。
- [x] **AC-3** Credit/Debit 单调 + 无副作用 — TC-3。
- [x] **AC-4** 受控写软约束 [Advisory·code-review] — grep 核实：econ 生产代码零直写 `.Cash =`，只经 `PS->SetCash`（pt-008 序列化 record 拷贝=既有 advisory 例外，非外部 gameplay 绕过）。
- [x] **AC-5** amount==0 早返静默 — TC-5。
- [x] **amount<0 契约** — TC-6（AddExpectedError "must be non-negative" ×2）。
- [x] （GiveStartingCash 数据驱动 STARTING_CASH，Implementation Notes 项）— TC-7。

**验证证据（主会话亲跑，铁律）**：Build Succeeded；全量 **275/275**（253+22warn，0 Fail/0 notRun，EXIT 0，269 基线+6 econ 零回归）；变异坐实非 vacuous（变异A 破 Cash≥0→TC-1 FAIL／变异B 破单一 Amount→TC-2 FAIL，各复绿）。报告 `Saved/TestReport_econ001_final|_mutA|_mutB`。对抗 review（unreal-specialist）：采纳 TransferCash 返回值契约硬化 + TC-2 Edge 断言 + 运行时依赖注释，硬化后仍 275/275。

**设计偏离（DV）+ 下游协调债**：
- **DV-1**：story Implementation Notes 字面「private `TMap<int32,int32> CashByPlayer`」**未采用**；按用户 2026-06-08 裁定改写 `URentoPlayerState.Cash`（经 `SetCash`），单源真相，对齐 GDD CR-1「拥有 PlayerState.Cash」+ player-turn 预设 SetCash 接缝（否则与 pt-007 读 `PS->Cash` 双源 stale）。
- **DV-2**：playerId→PS 经 `UPlayerTurnSubsystem::FindPlayerById`（运行时 5→2 只读，非反向环；turn 是 player-state owner）。运行时依赖回合2 已 InitializeFromConfig；未就绪时安全 no-op。
- **story-002 升级债**：本 story 落**占位** `FOnCashChangedSignature(PlayerId,OldCash,NewCash)` / `FOnInsufficientFundsSignature(PlayerId,AmountDue,AmountShort)`。story-002 须升级 OnCashChanged 带 `EChangeReason`（AC-33）+ 新增 `OnRentPaid`/`OnBankruptcyDeclared`，**fresh-grep 全部 AddDynamic 绑定点 + 同步 spy 签名**（pt-004 delegate 升级先例）。

---

## Dependencies

- Depends on: None（Foundation 宿主 ADR-0001 已 Accepted；本 story 是经济实现的根基）
- Unlocks: Story 002（事件契约）、003–010（全部需 Credit/Debit + GetCash）
