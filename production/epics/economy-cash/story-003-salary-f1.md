# Story 003: 发薪 F-1 (clamp + 溢出 guard + gate)

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: [TBD]
> **Manifest Version**: 2026-06-06
> **Last Updated**: [set by /dev-story]

## Context

**GDD**: `design/gdd/economy-cash.md`（F-1 / CR-2）
**Requirement**: `TR-econ-015`（passed_go clamp/溢出）、`TR-econ-017`（读 SalaryAmount）、`TR-econ-014`（确定性）
*(Requirement text lives in `docs/architecture/tr-registry.yaml`)*

**ADR Governing Implementation**: ADR-0014（整数确定性与溢出，primary）· ADR-0001（宿主）
**ADR Decision Summary**: `passed_go` 运行时 clamp 到 `[0, PASSED_GO_SAFE_MAX=1000]`（**真 clamp 非仅 ensure**，Shipping 剥离 ensure）；`SalaryAmount ≤ 2,000,000` 加载期 fatal（board 执行）；金钱整数纯净。

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: 整数算术跨编译配置确定；`FMath::Clamp`/`FMath::Max` 标准。`-nullrhi` 可测。

**Control Manifest Rules (Core/ADR-0014)**:
- Required: 金钱运算整数纯净；`passed_go` 运行时 clamp（非仅 ensure）；零 float。
- Forbidden: 用 float 算薪；只靠 dev `ensure` 防溢出（Shipping 无防线）。
- Guardrail: passed_go=1000 × SalaryAmount≤2e6 = 2e9 < INT32_MAX。

---

## Acceptance Criteria

- [ ] **AC-6** `salary = passed_go × SalaryAmount`：passed_go=1, Salary=200 → 200；passed_go=2 → 400。
- [ ] **AC-7** gate 在 passed_go（防双重发薪）：passed_go≤0（0/−1）→ 不发（无 Credit、无事件）；`SpecialAction=CollectSalary` 标记不触发第二次 Credit（停 GO passed_go=1 只发一次）。
- [ ] **AC-8** 溢出 guard：passed_go=12 → 不 int32 溢出、结果正确；passed_go=13 → dev ensure（`AddExpectedError` 捕获）；**且运行时 clamp[0,1000] 在 Shipping 亦生效**（passed_go=10⁷ → clamp 1000 → 不溢出、不负值）。

---

## Implementation Notes

*Derived from ADR-0014:*

- `salary = FMath::Clamp(FMath::Max(passed_go, 0), 0, PASSED_GO_SAFE_MAX) * SalaryAmount;`，`PASSED_GO_SAFE_MAX = 1000`（锁定常量，economy 内部）。
- **gate 在 passed_go 非 salary**：仅 `passed_go > 0` 才 `Credit(player, salary, Salary)`；`passed_go ≤ 0` 早返不发、不广播（CR-2）。
- `SalaryAmount` 读棋盘 `GetTileData(0).SalaryAmount`（base），由移动(4)连同 `passed_go` 传入（移动 CR-2 / 棋盘 CR-2 读取路径契约：移动读 SalaryAmount + passed_go 一并传经济）。
- **clamp 是真运行时 clamp**（Shipping 亦生效），`dev ensure(passed_go ≤ 12)` 仅 Development 期暴露上游（移动/传送链）bug，不替代 clamp。
- `SalaryAmount ≤ 2,000,000` 的加载期 fatal 校验归 board（propagate 债，ADR-0014 §Migration）；本 story 不重复校验，但 clamp 保证 economy 侧乘法不溢出（即便 board 未拦 SalaryAmount，passed_go 已 clamp）。
- `CollectSalary` SpecialAction 仅 UI 标记，**不构成第二次 Credit**（防双发，gate 唯一在 passed_go）。

---

## Out of Scope

- Story 005：Utility 租 `dice_total`（同涉 board 上界，但属租金路径）。
- 移动(4) 的 `AdvanceIndex`/`passed_go` 计算（movement epic）；board 的 SalaryAmount 加载校验（board epic，propagate 债）。

---

## QA Test Cases

- **AC-6**：Given passed_go=1, SalaryAmount=200；Then Credit(p,200,Salary) 恰 1 次。passed_go=2 → Credit(p,400)。
- **AC-7**：Given passed_go=0/−1；Then 无 Credit、无 OnCashChanged。Given 停 GO passed_go=1 + SpecialAction=CollectSalary；Then Credit 恰 1 次（标记不触发第二次）。
- **AC-8**：Given passed_go=12, Salary=200；Then salary=2400 无溢出。Given passed_go=13；Then dev ensure（AddExpectedError）。Given passed_go=10⁷；Then clamp→1000，salary=1000×200=2e5 无溢出/不负（Shipping clamp 亦生效，非仅 ensure）。Edge：两次冷启动同输入位级一致。

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `tests/unit/economy-cash/salary_f1_test.cpp`（类目 `Rento.Economy.SalaryF1`）— 存在且通过。
**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（Credit）、Story 002（OnCashChanged 广播）
- Unlocks: None（独立公式）
