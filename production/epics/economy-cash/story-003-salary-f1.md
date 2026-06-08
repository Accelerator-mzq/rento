# Story 003: 发薪 F-1 (clamp + 溢出 guard + gate)

> **Epic**: 经济与现金 Economy & Cash
> **Status**: Complete
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

- [x] **AC-6** `salary = passed_go × SalaryAmount`：passed_go=1, Salary=200 → 200；passed_go=2 → 400。
- [x] **AC-7** gate 在 passed_go（防双重发薪）：passed_go≤0（0/−1）→ 不发（无 Credit、无事件）；`SpecialAction=CollectSalary` 标记不触发第二次 Credit（停 GO passed_go=1 只发一次）。
- [x] **AC-8** 溢出 guard：passed_go=12 → 不 int32 溢出、结果正确；passed_go=13 → dev ensure（`AddExpectedError` 捕获）；**且运行时 clamp[0,1000] 在 Shipping 亦生效**（passed_go=10⁷ → clamp 1000 → 不溢出、不负值）。

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
**Required evidence**: `tests/unit/economy-cash/salary_f1_test.cpp`（物理 `Source/rentoTests/Private/SalaryF1Test.cpp`，类目 `Rento.Economy.SalaryF1`）— 存在且通过。
**Status**: [x] 5 TC 全通过（TC1 公式/TC2 gate/TC3 溢出 clamp/TC4 冷启动决定性/TC5 SalaryAmount 上界防线）

---

## Completion Notes（2026-06-09，mode-A 主会话亲写）

**实现**：`UEconomySubsystem::PaySalary(PlayerId, PassedGo, SalaryAmount)`（新方法，caller-injected SalaryAmount）。
- 公式 `salary = clamp(max(passed_go,0),0,1000) × SalaryAmount`；gate 唯一在 `passed_go>0`（CR-2 防双发）；内部调既有 `Credit(player, salary, Salary)`。
- 文件作用域常量 `PASSED_GO_SAFE_MAX=1000` / `PASSED_GO_ENSURE_MAX=12` / `SALARY_AMOUNT_SAFE_MAX=2,000,000`（ADR-0014）。

**用户裁定（2026-06-09）**：发薪 API = **`PaySalary(player, passed_go, salaryAmount)` 新方法**（caller-injected param，board epic 未实现故 economy 不直读 board；移动4 经 `GetTileData(0).SalaryAmount` 注入）。

**🔴 AC-8「dev ensure」逸脱为 `UE_LOG(Error)`**：① G2 已知坑（ensure 难稳定 test 捕获）；② 决定性标准（ensure once-only 顺序依赖，13/10⁷ 两次异常输入计数不可预测；本仓库无 AddExpectedError 捕获 ensure 先例）；③ TC-6 实证 `UE_LOG(Error)+AddExpectedError(Contains,N)` 决定性可用。**AC-8 硬保证=runtime clamp（story 自身写「不替代 clamp」）不受影响**；dev 信号同样达成 Development 期上游 bug 暴露。empirically TC3 通过坐实 13+10⁷ 两次 Error 正确发火+捕获。

**对抗 review（unreal-specialist）= APPROVED-WITH-CONCERNS**，采纳 **CONCERN-1**：story 主张「clamp 保证不溢出 即便 board 未拦 SalaryAmount」**证明不完全**——clamp 仅抑制 passed_go 一因子，`1000×SalaryAmount` 当 SalaryAmount>2,147,483 仍溢出 int32。ADR-0014 guardrail 要求两因子均有界（passed_go≤1000 ∧ SalaryAmount≤2e6）。board fatal(≤2e6) 未实现期间 economy 无防线=UB 窗。**采纳廉价 economy 侧独立防线**：`SalaryAmount>2,000,000 → refuse-not-clamp（dev log + 不发）`，+ TC5 坐实（含边界 2e6 正确发薪）。CONCERN-2/3·NIT 不采纳（reviewer 自解 / 他 epic 帰属 / 已编译实证）。

**全证（主会话亲跑，铁律）**：
- Build Succeeded；全量 **287/287**（282 基线 + 5 SalaryF1，0 Fail/0 notRun/EXIT 0，零回归，`Saved/TestReport_econ003_hardened`）。
- **变异坐实非 vacuous**：变异A（公式 `+100`）→ TC1/2/3/4 全 FAIL（`Saved/TestReport_econ003_mutA`）；变异B（clamp 上界→1e8）→ **仅 TC3 FAIL**（精密杀，TC1/2/4 不触上界故绿，`Saved/TestReport_econ003_mutB`）；均还原复绿。
- 权威纯净 grep：economy 模块零 `FMath::Rand`/全局随机。

**协调债 / propagate**：
- **CollectSalary（AC-7）路径测试帰属移动/事件 epic**（economy 侧只验 gate；CollectSalary「不经 PaySalary 二次发薪」是 social contract，须在移动/事件 story 的 integration 测试坐实——CONCERN-3）。
- **SalaryAmount≤2e6 board 加载期 fatal 仍待 board epic 落**（propagate 债，ADR-0014 §Migration）；本 story 已加 economy 侧独立防线兜底，board 实现后两防线并存（depth）。
- 明示 gate `if(PassedGo<=0)` 与 max-floor+Credit zero-guard 挙动冗余（防御层，CR-2 契约清晰性；TC2 验可观察契约不依赖具体层，NIT 非缺陷）。

---

## Dependencies

- Depends on: Story 001（Credit）、Story 002（OnCashChanged 广播）
- Unlocks: None（独立公式）
