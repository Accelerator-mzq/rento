---
Epic: dice-rng
Status: Complete
Layer: Foundation
Type: Integration
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-08
---

# Story 007 — 流隔离 + CI 禁全局 RNG 静态扫描硬门 + Sprint0 引擎验证

## Context

- **GDD**: `design/gdd/dice.md` — CR-2（中央可种子化 RNG 服务、禁旁路）/ Edge Cases（多线程并发抽取禁止）/ Dependencies（AI/Alpha 走权威流、juice/audio 走独立流）/ UE5.7 标注（知识盲区④）/ AC-19/AC-20。
- **Requirement TR-ID**: TR-dice-011（确定性权威逻辑落 C++ + 禁全局 RNG 静态符号扫描硬门）, TR-dice-016（多线程并发禁止、独立流）, TR-dice-015（流隔离：权威 vs 独立非权威）, TR-dice-004（跨平台/跨编译配置风险登记 + Sprint0 核验）。
- **ADR Governing**:
  - **ADR-0007（BP-C++ 边界，primary）** — CI 对权威 C++ 模块 grep 禁 `FMath::Rand` 等（文本硬门）+ 目录级断言（权威模块无 BP 派生类）；RNG 唯一入口经 `UDiceRngService::RandomRange` `UFUNCTION`。
  - **ADR-0004（确定性 RNG）** — 双层 RNG 架构：单权威流 + N 条独立非权威流；划线表；流隔离 spy 硬门；多线程禁共享权威流；Sprint0 Verification Required ①–④。
- **Engine**: UE 5.7。Risk: **MEDIUM**（Sprint0 引擎验证项 #4 直接归属本 story —— `RandHelper`/默构种子/官方 RNG 子系统存在性均超模型 ~5.3 知识截止）。
- **Engine Notes**（ADR-0004 Engine Compatibility / control-manifest Sprint0 #4，逐字保真）:
  - **Verification Required（ADR-0004，开工首步执行）**：① `FRandomStream::RandRange(int,int)` 是否仍走 `RandHelper: Min + TruncToInt(FRand()*Range)` 浮点中介（影响跨平台位级一致性结论）；② `FRandomStream` 经 `UPROPERTY` 序列化是否持久化当前滚动 `Seed`（影响 OQ-2 续算）；③ 默认构造种子是否仍恒 `0`（影响 lazy-init 兜底安全）；④ 是否新增官方确定性/网络 RNG 子系统（若有，Full Vision 联网重放可能优于手搓 `FRandomStream`，须重评）。
  - Note: Knowledge Risk = MEDIUM → 若 5.7 源码确认 `RandHelper` 已改纯整数路径，则 F-4 跨平台 off-by-one 警告可放宽，标 ADR Superseded 重写。
  - CI 引擎命令：Unreal headless runner with `-nullrhi`。
- **Control Manifest Rules**（Foundation + Global，2026-06-06）:
  - **Required（Cross-Cutting）**：**CI 随机硬门**——对权威 C++ 模块（AI/经济/建房/破产/回合/RNG）grep 禁 `FMath::Rand`、`FMath::RandRange`、`FMath::FRand`、`rand(`、`std::rand`（文本硬门）；**白名单仅 `UDiceRngService` 内 `FRandomStream` 调用**（ADR-0007）。**CI 目录级断言**：权威模块目录下无 BP 派生类资产（结构保证无 BP 随机旁路）（ADR-0007）。**RNG 流隔离**：权威随机（骰点/牌堆/AI 决策扰动）走唯一权威流；表现层 juice 各走独立非权威流；两者绝不混用（ADR-0004）。非权威流初始化禁默构（种子恒0），须 `Initialize(FPlatformTime::Cycles())`（ADR-0004）。
  - **Forbidden**：**Never 旁路引擎全局 RNG**（`FMath::Rand`/`UKismetMathLibrary::RandomInteger`/BP `Random Integer in Range`）做需重放的 gameplay 随机（ADR-0004/0007）；**Never 取消独立流让 VFX/Audio/HUD juice 抖动也调权威流** —— 致命破坏重放、联网 desync 根源（ADR-0004）；Never 用纯 code-review 软约束禁随机（可被 C++ 文本 grep 硬门 + 目录级断言廉价取代）（ADR-0007）。
  - **Guardrail**：误把 juice 随机接权威流污染重放 → 划线表 + spy 硬门 + code-review。

## Acceptance Criteria

> scoped 到本 story（流隔离纪律 + CI 硬门 + 多线程禁止 + Sprint0 核验）。

- [ ] **AC-20** [Advisory（BP-primary）/ 可升 [Logic] 若 C++ 强封装] 所有需可重放随机的系统（AI/Alpha）无直接调引擎全局 `FMath::Rand`/BP `Random Integer in Range`。BP 节点不可静态扫描→code-review；C++ 则加禁用符号静态扫描升 [Logic] PR gate（与 player-turn AC-35a 同构）。
- [ ] **AC-19** [Advisory] 代码审查 DiceService 无并发访问 `FRandomStream` 路径；持有方有单线程所有权标注（并发禁止是架构约束，无自动化 fixture）。
- [ ] **[CI 硬门]** CI 对权威 C++ 模块（含 RNG）grep 禁 `FMath::Rand`/`FMath::RandRange`/`FMath::FRand`/`rand(`/`std::rand`，白名单仅 `UDiceRngService` 内 `FRandomStream`；权威模块目录下无 BP 派生类资产 —— 二者作 PR gate。
- [ ] **[流隔离]** 划线表落地：骰点/牌堆 Fisher-Yates/AI 决策扰动走权威流；VFX/Audio/HUD juice 各走独立非权威流（禁默构 0）；两者绝不混用。
- [ ] **[Sprint0 引擎验证]** ADR-0004 Verification Required ①–④ 对 UE 5.7 源码/Release Notes 核验完毕，结论回填 ADR-0004 Last Verified；①（`RandHelper` 浮点中介）+ ③（默构种子恒 0）须确认，否则修订 ADR 并重 Accept。

## Implementation Notes

> 来自 ADR-0004 + ADR-0007 Implementation Guidelines + control-manifest，逐字保真不改写语义。

- **划线表**（ADR-0004 Guideline 1，实现者据此判定，误接由 spy 抓出）:
  | 随机用途 | 走哪条流 | 须重放 |
  |---|---|---|
  | 骰点 `RollDice` | 权威流 | 是 |
  | 牌堆 Fisher-Yates 洗牌 | 权威流 | 是 |
  | AI 决策扰动（F-3 Epsilon、tiebreak 备选） | 权威流（经 `RandomRange`/`Float01`） | 是 |
  | VFX juice 节奏抖动 / 火花方向 | **独立非权威流** | 否 |
  | Audio 变奏（音高/起始偏移） | **独立非权威流** | 否 |
  | HUD juice 随机 | **独立非权威流** | 否 |
- **独立流初始化**（ADR-0004 Guideline 4）：每个非权威流亦禁默构 0，用 `Initialize(FPlatformTime::Cycles())` 或各自非确定源。
- **禁 BP 随机节点硬门**（ADR-0007 Implementation Guidelines，MVP 分级）：CI 对权威 C++ 模块 grep 禁 `FMath::Rand`、`FMath::RandRange`、`FMath::FRand`、`rand(`、`std::rand`（文本硬门，免费）。白名单：`UDiceRngService` 内 `FRandomStream` 调用。CI 目录级断言：权威模块（AI/经济/建房/破产/回合/RNG）目录下无 BP 派生类资产。
- **RNG 唯一入口**（ADR-0007 Key Interfaces 2）：BP 须经 `UDiceRngService::RandomRange` 这一 `UFUNCTION`（唯一允许的 BP 随机来源）；禁 BP graph 内 K2Node "Random Integer in Range" 触碰 gameplay。
- **多线程禁止**（dice Edge Cases / ADR-0004 R3）：性质 D 前提②要求单线程。若未来后台线程需随机，须用独立 `FRandomStream`、不得共享此权威流（共享会插入不确定顺序的抽取、破坏重放）。
- **强度联动 ADR-0007**（ADR-0004 Guideline 6）：若 ADR-0007 裁 AI 决策核心 + RNG 封装为 C++，则可把 dice AC-20 从 [Advisory·code-review] 升 [Logic] PR gate；本 ADR 推荐 C++ 封装。**ADR-0007 已 Accepted 并裁 RNG 落 C++**（控制清单 Cross-Cutting CI 硬门已生效）→ AC-20 的 C++ grep 硬门部分应实装为 PR gate；BP 节点部分保 code-review。
- **实现期源码核验**（ADR-0004 Guideline 8）：见 Engine Compatibility Verification Required ①–④，开工首步执行。

## Out of Scope

- DiceService 宿主、原语、RollDice、退化契约、事件、fixture → story-001~006（本 story 是横切纪律 + CI 工程 + Sprint0 验证，不重复实现服务本体）。
- 各下游（AI/VFX/Audio/HUD）独立流的具体实现 → 各下游 epic（本 story 只定划线表 + CI 硬门 + 验证 DiceService 侧权威流唯一）。
- Full Vision 跨平台重放正解（服务器权威 + RPC / 纯整数 LCG）→ OQ-3，不在 MVP；本 story 只登记 TR-dice-004 风险 + 完成 Sprint0 核验。
- ai AC-61a/61b 权威流端到端 spy → 归 AI epic（本 story 定 DiceService 侧划线表与 grep 硬门基线）。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

> Logic/Integration 给 Given/When/Then；CI/审查门给 Setup/Verify/Pass。`-nullrhi` headless。

- **[CI 硬门]（grep + 目录断言）**
  - Setup: CI 流水线对 `src/` 下权威模块（AI/经济/建房/破产/回合/RNG）执行文本 grep + BP 资产目录扫描。
  - Verify: grep 命中 `FMath::Rand`/`FMath::RandRange`/`FMath::FRand`/`rand(`/`std::rand`（白名单 `UDiceRngService` 内 `FRandomStream` 除外）→ 失败；权威模块目录存在 BP 派生类资产 → 失败。
  - Pass: 零命中（白名单外）+ 权威目录无 BP 类 → PR gate 通过。
- **AC-20（禁旁路）**
  - Setup: 审查 AI/Alpha 随机调用点（含 C++ grep 硬门 + BP code-review）。
  - Verify: 无直接调引擎全局 `FMath::Rand`/BP `Random Integer in Range`；全部经 `UDiceRngService` 原语。
  - Pass: C++ 部分由 grep 硬门 [Logic] 兜底（ADR-0007 已裁 C++）；BP 部分 code-review 签字 [Advisory]。
- **AC-19（并发禁止审查）**
  - Setup: 审查 DiceService 持有与调用路径。
  - Verify: 无并发访问 `FRandomStream` 路径；持有方有单线程所有权标注。
  - Pass: 架构约束满足（无自动化 fixture，code-review 签字）。
- **[流隔离] 划线表落地**
  - Setup: 审查各随机消费点对照划线表。
  - Verify: 权威用途走权威流、juice 用途走独立非权威流（且独立流禁默构 0）；两者不混用。
  - Pass: 全部消费点归类正确；误接由下游 ai AC-61b spy 兜底（归 AI epic）。
- **[Sprint0 引擎验证] ADR-0004 ①–④**
  - Setup: 取 UE 5.7 源码（`FRandomStream`/`RandHelper`）+ Release Notes。
  - Verify: ① `RandRange` 是否仍走 `FRand()` 浮点中介；② `UPROPERTY` 序列化是否持久化当前 `Seed`；③ 默构种子是否仍恒 0；④ 是否新增官方确定性/网络 RNG 子系统。
  - Pass: 四项核验完毕，结论回填 ADR-0004 Last Verified；①③ 不符预期则修订 ADR-0004 并重 Accept（①影响跨平台结论、③影响 lazy-init 兜底 story-004）。

## Test Evidence

- **Path**: `tools/ci/` RNG 静态扫描脚本 + GitHub Actions step（CI 硬门 grep + 目录断言，[Logic] PR gate for C++）。
- **Path**: `production/qa/evidence/dice-stream-isolation-review.md`（[Advisory] AC-19/20 BP 部分 + 划线表落地审查，含 Status 未创建）。
- **Path**: `docs/architecture/ADR-0004-deterministic-rng.md` Last Verified 节回填（Sprint0 ①–④ 结论）。
- **Status**: 未创建。

## Dependencies

- **Depends on**: story-001（DiceService 宿主 + `UDiceRngService::RandomRange` 唯一入口，CI 白名单锚点）须 DONE。**Sprint0 引擎验证 ①–④ 建议最先执行**（开工首步），其结论门控 story-004（③默构 0）/ story-006（①`RandHelper` 浮点基线）。
- **Unlocks**: 下游 AI(10)/移动(4) 实现（ADR-0004 Blocks 列：移动/AI 实现开工依赖流隔离 + 禁旁路硬门生效）；ai AC-61a/61b 权威流 spy（归 AI epic）。

## Completion Notes
**Completed**: 2026-06-08
**Criteria**: 5/5 COVERED（[CI 硬门]/AC-20-C++ 自动化 PR-gate + AC-19/20-BP/流隔离/Sprint0①–④ 审查/cross-ref）
**Deviations**: None。纠正记录：盘点中亲读 pt-009 源码发现拟建 `Rento.Dice.CiAuthorityGate` 会冗余重复 `TurnModuleNoBpDerivedCiTest.cpp` TC1 对 UDiceRngService 的 CLASS_Native 断言 → 未建测试（避免假覆盖）。
**Test Evidence**: Integration — `production/qa/evidence/dice-stream-isolation-review.md`（documented review，全 sign-off msc 2026-06-08）+ pt-009 既有 CI 自动化门（`tools/ci/check-authoritative-purity.sh` 实跑三门绿 EXIT 0 + `Rento.PlayerTurn.CiAuthorityGate` TC1/TC2 覆盖 UDiceRngService）。零 src/CI 改动；全量基线不变 269/269。
**Code Review**: N/A — 无新代码；依赖的 CI 脚本/测试已在 pt-009 `/code-review` APPROVED。
**实现要点（主会话直接驱动，非 mode-A）**：DR-007 重活（CI 硬门 + 引擎验证）已被 **pt-009**（`check-authoritative-purity.sh` Gate A 随机硬门 `AUTH_SRC=Source/rento` 含 RNG + Gate B 目录断言 `AUTH_CLASSES` 含 DiceRngService + `authoritative-purity.yml` 云端 + `TurnModuleNoBpDerivedCiTest` TC1 L56 显式断言 UDiceRngService CLASS_Native）与 **2026-06-06 Sprint0 spike**（ADR-0004 ①③④ verified+回填）完成。本 story = evidence/承接 doc + 实跑门抓 PASS 证据。白名单实证：DiceRngService 经 `AuthoritativeStream.RandRange`（FRandomStream 受准原语，cpp L202）未被门误报，Source/rento 零 FMath::Rand 旁路。
**下游移交（evidence doc §6 登记，非 tech-debt）**：VFX/Audio/HUD/AI 各自独立非权威流实现归各下游 epic；误接由 ai AC-61b 权威流 spy 硬门兜底（AI epic）。
