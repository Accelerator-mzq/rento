---
Epic: player-turn
Status: Complete
Layer: Foundation
Type: Config-Data
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 009 — 回合状态机/编排落 C++ 权威模块 + CI 目录无 BP 派生类断言

## Context

- **GDD**: `design/gdd/player-turn.md` — 「接口稳定承诺」(L254)、CR-8 确定性（同步模型 headless 可重放）、AC-43（append-only 枚举强度随 OQ-1 ADR）、AC-35（受控写强度）
- **Requirement TR-ID**: TR-turn-016（回合状态机/编排逻辑落 C++ 权威模块〔目录内无 BP 派生类，CI 断言〕，保可审可测可重放）
- **ADR Governing**:
  - **ADR-0007（primary）— BP/C++ 权威边界**：写权威状态（状态机 phase / PlayerState）→ C++；CI 目录级断言权威模块（AI/经济/建房/破产/回合/RNG）目录下**无 BP 派生类资产**（结构保证无 BP 随机旁路）；CI 随机硬门 grep 禁引擎全局 RNG。
  - **ADR-0001 — UObject 宿主与生命周期**：回合状态机挂 C++ World Subsystem（per-match 宿主）。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（BP/C++ 互操作 `UFUNCTION`/`UCLASS` 是 UE 长期稳定基础设施）
- **Engine Notes（ADR-0007 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None** — 本 ADR 是组织/边界裁定，不引入 5.4+ 新 API。
  - Verification Required: 静态符号扫描门若采用，须在 CI 验证 (a) 能解析 `uasset`/导出文本以检出 BP 随机节点（`Random Integer in Range`/`Random Float` 等 K2Node）；(b) 能区分白名单（经 #3 骰子封装的 `UFUNCTION`）与裸引擎随机节点。**MVP 可先以「权威逻辑无 BP 类」的目录级断言替代逐节点扫描**。
  - 若 5.x 改变 BP K2Node 导出格式致静态扫描失效，须重验 Verification Required (a)。
- **Control Manifest Rules（Core Layer + Global）**:
  - **Required（Core）**: 写权威状态? → C++。任何写 `PlayerState`/owner map/`house_count`/`Cash`/牌堆顺序/状态机 phase 落 C++（source: ADR-0007）。是 `[Logic] BLOCKING` AC 的被测逻辑? → C++ 独立 `UObject`/纯函数，须 `-nullrhi` 可实例化（source: ADR-0007）。
  - **Forbidden（Core）**: Never 让权威逻辑出现 BP 派生类——`uasset` 二进制不可 git diff / 不可静态审 / 不可 grep（source: ADR-0007）。Never 采用「BP 为主 + 约定」——确定性义务不可机器执行、`[Logic]` AC 大面积 false-green（source: ADR-0007）。Never 用纯 code-review 软约束禁随机——可被「C++ 文本 grep 硬门 + 目录级断言」廉价取代（source: ADR-0007）。
  - **Cross-Cutting（Global）**: CI 随机硬门——对权威 C++ 模块（AI/经济/建房/破产/回合/RNG）grep 禁 `FMath::Rand`/`FMath::RandRange`/`FMath::FRand`/`rand(`/`std::rand`，白名单仅 `UDiceRngService` 内 `FRandomStream` 调用（source: ADR-0007）。CI 目录级断言——权威模块目录下无 BP 派生类资产；Alpha 若 BP 触 gameplay 边缘增 BP 静态符号扫描检出裸随机 K2Node（source: ADR-0007）。
  - **Guardrail**: 权威逻辑 C++ 热路径优于等价 BP VM，回合制低频差异非瓶颈；CPU 预算 16.6ms。

## Acceptance Criteria

- [ ] AC-1（TR-turn-016）: 回合状态机/编排逻辑（状态机 phase 写、F-1/F-2、PlayerState 字段写、AI hook 编排、聚合编排）全落 C++ 权威模块，无任何权威逻辑住 BP 图。
- [ ] AC-2（TR-turn-016，ADR-0007 CI 目录级断言）: CI 对回合模块目录执行目录级断言——目录下**无 BP 派生类资产**（结构保证无 BP 随机旁路 / 无 BP 写权威状态）。该断言在每次 push to main 与每个 PR 运行，BP 派生类出现即失败。
- [ ] AC-3（ADR-0007 CI 随机硬门）: CI 对回合权威 C++ 模块 grep 禁 `FMath::Rand`/`FMath::RandRange`/`FMath::FRand`/`rand(`/`std::rand`（文本硬门）；白名单仅 `UDiceRngService` 内 `FRandomStream` 调用——回合模块本身不应命中（AI RNG 走骰子3 唯一入口）。
- [ ] AC-4（GDD L254 接口稳定承诺）: `PlayerState` 字段一旦发布只增不改语义；回合阶段枚举 `ETurnPhase` 通过扩展新增、不破坏既有转换——code-review 据 architecture.md §D.2 事件表 + 各 GDD 核验（与 AC-43 append-only 同性质，强度随 OQ-1 ADR）。

## Implementation Notes

> 逐字保真自 ADR-0007 Implementation Guidelines 边界判据 + 禁 BP 随机节点硬门，不改写语义。

1. **写权威状态？→ C++**（ADR-0007 #1）：任何写 `PlayerState`/状态机 phase 的逻辑必落 C++（原则 4 + 单一 owner 不变式）。
2. **`[Logic] BLOCKING` AC 被测逻辑 → C++ 独立 `UObject`/纯函数**（ADR-0007 #3）：须 `-nullrhi` 可实例化（裸 `NewObject` 或 mock World）、可注入 `IGameClock`、暴露可 spy getter；不得依赖 `UMG NativeTick` 或 BP `Bind Event` graph 层。
3. **禁 BP 随机节点硬门（ADR-0007 分叉二，MVP 分级）**：CI 对权威 C++ 模块 grep 禁 `FMath::Rand`、`FMath::RandRange`、`FMath::FRand`、`rand(`、`std::rand`（文本硬门，免费）；白名单 `UDiceRngService` 内 `FRandomStream` 调用。CI 目录级断言：权威模块（AI/经济/建房/破产/回合/RNG）目录下**无 BP 派生类资产**（结构保证无 BP 随机旁路）。
4. **MVP 分级**：MVP 可先以「权威逻辑无 BP 类」目录级断言替代逐节点扫描；Alpha 若 BP 触碰 gameplay 边缘（交易/拍卖 AI hook）增 BP 资产静态符号扫描检出裸随机 K2Node。
5. **CI 接入（unreal 引擎）**：headless 运行器用 `-nullrhi` flag；测试套件在每次 push to main 与每个 PR 运行，失败为阻塞 gate，不得 disable/skip 失败的目录断言。

## Out of Scope

- 状态机/F-1/F-2/聚合的功能实现（落 C++ 的具体逻辑）→ **story-002/003/007**（本 story 只钉「落 C++ + CI 断言」结构门）。
- 受控写接口面的 C++ 强封装强度门 AC-35a → **story-005**（本 story 钉目录级无 BP 派生类，与 AC-35a 受控写强度门正交但同 ADR-0007 性质）。
- 枚举 append-only 的序列化兼容验证 → **story-008**（本 story 验接口稳定承诺 code-review 面）。
- CI 框架/runner 脚手架本身（GitHub Actions workflow + headless runner 配置）→ test-setup（已由 `/test-setup` 搭建，本 story 增回合模块的目录断言规则）。

## QA Test Cases

> 本 story 为 Config-Data（CI 构建门控/目录断言）；给 Setup/Verify/Pass condition。CI 在 headless `-nullrhi` 运行。

- **TC-1 (AC-1)**: **Setup** 回合模块源码树。**Verify** grep/静态检查所有写状态机 phase / PlayerState 字段的逻辑落点。**Pass** 全在 C++ `.cpp`/`.h`，无 BP 图承载权威逻辑。
- **TC-2 (AC-2)**: **Setup** CI 目录级断言脚本指向回合模块目录。**Verify** 在目录中放一个 BP 派生类资产（负向测试）跑 CI。**Pass** CI 断言失败检出该 BP 派生类；移除后 CI 通过。
- **TC-3 (AC-3)**: **Setup** CI 随机硬门 grep 规则覆盖回合权威 C++ 模块。**Verify** 在回合模块插入 `FMath::Rand` 调用（负向测试）跑 CI。**Pass** grep 硬门命中失败；移除后通过（回合 AI RNG 走 `UDiceRngService` 白名单入口不命中）。
- **TC-4 (AC-4)**: **Setup** `PlayerState` 字段 + `ETurnPhase` 枚举定义。**Verify** code-review 据 architecture.md §D.2 事件表核验字段「只增不改语义」、枚举扩展只 append。**Pass** 无破坏既有语义/转换的改动；记录于 code-review。
- **Edge cases**: 负向测试（注入 BP 派生类 / 裸随机）须真触发 CI 失败（断言非空壳）；目录断言不误伤呈现层 BP（仅约束回合权威模块目录）；CI 失败为阻塞 gate，禁 disable/skip。

## Test Evidence

- **Type**: Config-Data
- **Path**: `tests/unit/player-turn/turn_module_no_bp_derived_ci_test.cpp`（目录断言 harness）+ CI workflow grep 硬门规则 + `production/qa/smoke-[date].md`（接口稳定承诺 code-review 记录）
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-002/003/007（回合权威逻辑落 C++ 的实现存在，方可对其目录断言）；test-setup（CI 框架/headless runner 已搭建）。
- **Unlocks**: epic DoD「CI 目录级断言：回合模块目录下无 BP 派生类」；为所有 `[Logic]` AC 的 false-green 防护提供结构保证。

---

## Completion Notes（2026-06-08，主会话直写 YAML+脚本+harness，非 mode-A）

**Status: Complete。4/4 AC 覆盖。全量 239/239 Success, 0 Fail, 0 notRun, EXIT 0**
（基线 237 + 2 CiAuthorityGate TC，零回归；主会话独立全证 `Saved/TestReport_pt009_main` 2026.06.08-06.27.13）。

### 交付（3 新建）
- **`tools/ci/check-authoritative-purity.sh`**：CI 权威纯净门 bash 脚本，3 门——
  - Gate A（AC-3 随机硬门）：grep 禁 `FMath::Rand*`/`rand(`/`std::rand`（精确匹配紧跟 `(` + 排除注释行，避免误判 doc 注释里的禁用名；白名单 UDiceRngService FRandomStream 非禁用项无需特判）。
  - Gate B1（AC-2）：权威 C++ 源码树 `Source/rento` 无 `.uasset`。
  - Gate B2（AC-2）：Content/ 无【含 `BlueprintGeneratedClass` 标记 + 引用权威 C++ 类】的 BP 派生类（DataAsset 数据实例如 DA_Board 无该标记 → 放行，不误判）。
- **`.github/workflows/authoritative-purity.yml`**：ubuntu-latest 云端 workflow（纯 grep/find 无需 UE），push to main + PR 触发，跑上述脚本。与 tests.yml（自托管 UE Automation）分离。
- **`Source/rentoTests/Private/TurnModuleNoBpDerivedCiTest.cpp`**：in-engine 结构断言 harness（`Rento.PlayerTurn.CiAuthorityGate`，2 TC）——TC1 权威类（PlayerTurnSubsystem/RentoPlayerState/DiceRngService/MatchSubsystemBase）StaticClass 是 `CLASS_Native` 且非 `CLASS_CompiledFromBlueprint`；TC2 IFileManager 扫描权威源码树无 `.uasset`。
- **`production/qa/smoke-2026-06-08-pt009-interface-stability.md`**：AC-4 接口稳定承诺 code-review 记录（PlayerState 字段只增不改 + ETurnPhase append-only，强度已由 story-008 static_assert 升为编译期门）。

### falsifiability 坐实（非空壳门）
- **bash 脚本三门负向测试**（主会话亲跑）：注入 `FMath::RandRange(` → Gate A exit 1 FAIL；Source/rento 放 `.uasset` → Gate B1 FAIL；Content/ 放含 BlueprintGeneratedClass+权威类名伪 .uasset → Gate B2 FAIL；清理后回归 exit 0 无残留。
- **harness TC-2 变异坐实**（in-engine 独立代码路径）：植入 `.uasset` 于 Source/rento → TC2 精确 FAIL（TC1 不受影响）→ 清理复绿。
- 现码正向全 PASS（DA_Board DataAsset 未被 B2 误判，确认含 0 个 BlueprintGeneratedClass 标记）。

### MVP 分级（ADR-0007）
本 story 钉「权威逻辑无 BP 类」目录级断言 + 随机文本硬门（MVP）。Alpha 若 BP 触 gameplay 边缘（交易/拍卖 AI hook）再增 BP 资产逐 K2Node 静态符号扫描检出裸随机节点（Out of Scope，已记 ADR-0007 Verification Required）。

### Out of Scope 守界
权威逻辑功能实现归 story-002/003/007（已 Complete）；受控写 C++ 强封装 AC-35a 归 story-005；枚举序列化兼容归 story-008；CI runner 脚手架归 test-setup（已建 tests.yml，本 story 仅增权威门规则 + 独立 purity workflow）。
