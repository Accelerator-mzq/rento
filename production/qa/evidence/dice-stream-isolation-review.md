# DR-007 证据 — 流隔离 + CI 禁全局 RNG 静态门 + Sprint0 引擎验证

> Story: `production/epics/dice-rng/story-007-stream-isolation-ci-gate.md`
> 日期: 2026-06-08 | 模式: Collaborative（主会话直接驱动，非 mode-A）
> 验证者: msc（主会话独立核实 —— 亲读 pt-009 源码 + 实跑权威纯净门，非据 claim）

## 0. 范围裁定（与 pt-009 + Sprint0 spike 的差集）

DR-007 的"重活"早已被 **pt-009（CI 工程）** 与 **2026-06-06 Sprint0 引擎验证 spike** 完成。本 story
不重建已覆盖部分，剩余 = 承接登记 + 审查门（AC-19/20 BP/并发）+ 划线表批准 + 跑门抓 PASS 证据。

**亲读 pt-009 源码纠正的一个 claim**：曾拟新建 `Rento.Dice.CiAuthorityGate` 给 RNG 模块独立锚点，
但 `TurnModuleNoBpDerivedCiTest.cpp`（`Rento.PlayerTurn.CiAuthorityGate`）**TC1 第 56 行已显式断言
`UDiceRngService::StaticClass()` 是 CLASS_Native 非 BP 派生**，TC2 已断言 `Source/rento`（含 RNG）无
`.uasset`。新建 dice 侧测试 = 冗余重复已覆盖断言（假覆盖），故**不新建测试**，改纯承接登记。

---

## 1. [CI 硬门] —— 承接 pt-009 覆盖登记（AC-20 C++ 部分 = [Logic] PR-gate）

| DR-007 要求 | 由谁覆盖 | 精确锚点 |
|---|---|---|
| grep 禁 `FMath::Rand`/`FMath::RandRange`/`FMath::FRand`/`rand(`/`std::rand`（权威 C++ 模块含 RNG） | pt-009 | `tools/ci/check-authoritative-purity.sh` **Gate A**（L37-54）；`AUTH_SRC="Source/rento"` 含 RNG；正则 `FMath::F?Rand[A-Za-z]*\(` 涵盖 Rand/RandRange/FRand |
| 白名单仅 `UDiceRngService` 内 `FRandomStream` | pt-009 | Gate A 禁用列表不含 `FRandomStream`（白名单即"不在禁用列表"）；本 doc §4 实证未误报 |
| 权威模块目录无 BP 派生类资产 | pt-009 | `check-authoritative-purity.sh` **Gate B1**（源码树无 .uasset，L56-68）+ **Gate B2**（Content/ 无派生权威类 BP，`AUTH_CLASSES` 含 `DiceRngService`，L70-93）；in-engine 互补 harness `TurnModuleNoBpDerivedCiTest.cpp` TC2 |
| 权威 RNG 类是 native C++（非 BP 顶替） | pt-009 | `TurnModuleNoBpDerivedCiTest.cpp` **TC1 第 56 行** `CheckNative(UDiceRngService::StaticClass(), ...)` + 第 57 行 `UMatchSubsystemBase`（RNG 宿主基类） |
| CI 云端运行（无需 UE） | pt-009 | `.github/workflows/authoritative-purity.yml`（ubuntu-latest 跑 grep/dir 门，区别于 UE Automation 自托管 job） |

**结论**：DR-007 [CI 硬门] 与 AC-20 的 C++ grep 硬门部分 = **PR-gate [Logic] 已生效**（pt-009 落地）。

---

## 2. [CI 硬门] PASS 证据（主会话 2026-06-08 实跑）

```
$ bash tools/ci/check-authoritative-purity.sh
==> [Gate A] 随机硬门：✔ Gate A PASS — 无禁用全局 RNG 命中
==> [Gate B1] 源码树无 .uasset：✔ Gate B1 PASS
==> [Gate B2] Content 无 BP 派生权威类：✔ Gate B2 PASS
✅ 权威纯净门全部通过（ADR-0007）。
EXIT: 0
```

当前 HEAD 树上权威纯净门三门全绿、EXIT 0。

---

## 3. AC-20（禁旁路）—— 分级裁定

- **C++ 部分** = [Logic] PR-gate：pt-009 Gate A 硬门兜底（见 §1/§2）。当前 `Source/rento` 全树零 `FMath::Rand*`/`rand(`/`std::rand` 旁路命中（非注释）。
- **BP 部分** = [Advisory] code-review：MVP 阶段 dice/AI 尚无 BP graph 触 gameplay 随机；RNG 唯一 BP 入口 = `UDiceRngService::RandomRange` 这一 `UFUNCTION`（ADR-0007 Key Interfaces 2）。下游 AI(10)/VFX/Audio/HUD 的 BP 随机节点审查归各自 epic（OoS）。

| 检查项 | 状态 | Reviewer |
|---|---|---|
| 权威 C++ 模块（含 RNG）零 `FMath::Rand*` 旁路命中 | PASS（§2 Gate A 绿 + §4 grep 零命中） | msc 2026-06-08 |
| RNG 唯一 BP 入口为 `UDiceRngService::RandomRange` UFUNCTION | PASS（DiceRngService.h L179-181） | msc 2026-06-08 |

---

## 4. 白名单正确性实证（DiceRngService FRandomStream 未被误报）

权威 RNG 服务用**受准原语** `FRandomStream`，非禁用的 `FMath::Rand`：

```
Source/rento/DiceRngService.cpp:67   AuthoritativeStream.Initialize(FallbackSeed);   // lazy-init
Source/rento/DiceRngService.cpp:202  return AuthoritativeStream.RandRange(Min, Max);  // 唯一抽取点
```

权威模块全树禁用符号 grep（排除注释）：**零命中**

```
$ grep -rnE 'FMath::F?Rand[A-Za-z]*\(|(^|[^A-Za-z0-9_])rand\(|std::rand' Source/rento \
    --include=*.h --include=*.cpp | grep -vE ':[0-9]+:[[:space:]]*(\*|//|/\*)'
（无输出）
```

→ 白名单设计正确：`FRandomStream` 受准原语不被门误报；任何 `FMath::Rand` 旁路会被 Gate A 抓出。

| 检查项 | 状态 | Reviewer |
|---|---|---|
| DiceRngService 经 FRandomStream（非 FMath::Rand）且门未误报 | PASS | msc 2026-06-08 |

---

## 5. AC-19（并发禁止）—— 单线程所有权审查

- 权威流 `AuthoritativeStream` 是 `UDiceRngService`（`UWorldSubsystem`）的 **private 值成员**（`DiceRngService.h` L320），由 game thread 单一所有；无任何并发访问路径、无后台线程共享。
- 性质 D 前提②（dice F-4）要求单线程抽取序列；多线程并发抽取会插入不确定顺序、破坏重放（ADR-0004 R3）。
- 未来若后台线程需随机：**须各自持独立 `FRandomStream`，不得共享此权威流**（ADR-0004 Implementation Notes / dice Edge Cases）。
- 架构约束，无自动化 fixture（并发缺陷非确定性，headless 无法稳定复现）→ code-review 守门。

| 检查项 | 状态 | Reviewer |
|---|---|---|
| 权威流 private 值成员、game-thread 单一所有、无并发访问路径 | PASS（DiceRngService.h L309-320） | msc 2026-06-08 |
| 多线程未来用法约束（独立流不共享）已在 ADR-0004 + header 标注 | PASS | msc 2026-06-08 |

---

## 6. [流隔离] 划线表落地批准

权威流 vs 独立非权威流划线表（ADR-0004 Guideline 1）：

| 随机用途 | 走哪条流 | 须重放 | 本 story 责任 |
|---|---|---|---|
| 骰点 `RollDice` | 权威流 | 是 | ✅ DiceService 侧已落地（story-002，经 AuthoritativeStream.RandRange） |
| 牌堆 Fisher-Yates 洗牌 | 权威流 | 是 | 下游事件牌 epic（OoS） |
| AI 决策扰动（F-3 Epsilon、tiebreak） | 权威流（经 RandomRange/Float01） | 是 | 下游 AI epic（OoS；ai AC-61b spy 兜底） |
| VFX juice 节奏抖动/火花方向 | **独立非权威流** | 否 | 下游 VFX epic（OoS） |
| Audio 变奏（音高/起始偏移） | **独立非权威流** | 否 | 下游 Audio epic（OoS） |
| HUD juice 随机 | **独立非权威流** | 否 | 下游 HUD epic（OoS） |

- **DiceService 侧权威流唯一性**已落地：全项目恰一个权威 `FRandomStream`（`DiceRngService.h` L309-320 注释"全项目恰一个权威流"）；所有须重放随机经 `UDiceRngService` 封装函数。
- 独立非权威流的具体实现归各下游 epic（本 story OoS）；禁默构 0（统一 `Initialize(FPlatformTime::Cycles())`）的约束已在 ADR-0004 Guideline 4 锚定。
- 误接（juice 接权威流污染重放）由下游 **ai AC-61b 权威流 spy 硬门**兜底（归 AI epic）。

| 检查项 | 状态 | Reviewer |
|---|---|---|
| 划线表批准；DiceService 侧权威流唯一性已落地 | PASS（DiceRngService.h L309-320） | msc 2026-06-08 |
| 下游独立流实现 OoS（各 epic）；误接由 ai AC-61b spy 兜底 | 登记（移交） | msc 2026-06-08 |

---

## 7. [Sprint0 引擎验证] ADR-0004 ①–④ —— cross-ref（已 verified）

| 验证项 | 结论 | 锚点 |
|---|---|---|
| ① `RandRange` 仍走 `Min+TruncToInt(FRand()*Range)` 浮点中介 | ✅ CONFIRMED | ADR-0004 Last Verified（2026-06-06 spike，RandomStream.h L186-202）；F-4 跨平台 off-by-one 警告维持，**不放宽不 Superseded**（同平台/同构建重放安全） |
| ② `FRandomStream` 经 `UPROPERTY` 序列化是否持久化当前 Seed | 移交 ADR-0005 / OQ-2 | 不在本 spike 范围；MVP 不序列化 Seed，已由 **DR-008**（当前骰序列化契约）+ G7 引擎实证（memory `ue57-savegame-builtin-path-no-savegame-filter`）闭合 |
| ③ 默认构造种子是否恒 0 | ✅ CONFIRMED | ADR-0004 Last Verified（RandomStream.h L30-33）；lazy-init 兜底安全前提成立（story-004 AC-17b 断言 InitialSeed!=0） |
| ④ 是否新增官方确定性/网络 RNG 子系统 | ✅ CONFIRMED 无 | ADR-0004 Last Verified（全树零命中；Mass `UE::RandomSequence` 仅无状态 hash helper，登记供 Full Vision 参考） |

详见 `docs/architecture/ADR-0004-deterministic-rng.md` §Last Verified（2026-06-06 回填）+ `docs/architecture/sprint0-engine-verification-2026-06-06.md`。①③④ 已对 5.7 源码 verified；②（Seed 序列化）属 ADR-0005 范畴，已由 DR-008 闭合。

| 检查项 | 状态 | Reviewer |
|---|---|---|
| ADR-0004 ①③④ 已 verified+回填；② 移交 ADR-0005 已由 DR-008 闭合 | PASS | msc 2026-06-08 |

---

## 8. AC 覆盖小结

| AC | 类型 | 覆盖 |
|---|---|---|
| [CI 硬门] grep + 目录断言 | [Logic] PR-gate | ✅ pt-009（§1/§2 实跑 PASS） |
| AC-20 禁旁路 | C++ [Logic] / BP [Advisory] | ✅ C++ pt-009 Gate A；BP code-review（§3） |
| AC-19 并发禁止 | [Advisory] code-review | ✅ 单线程所有权审查（§5） |
| [流隔离] 划线表 | [Advisory] review | ✅ 批准 + DiceService 侧权威流唯一（§6）；下游 OoS |
| [Sprint0 验证] ①–④ | verification | ✅ ①③④ verified（§7）；② 移交 ADR-0005/DR-008 |

**裁定**：DR-007 全 AC COVERED（[CI 硬门]/AC-20 C++ 由 pt-009 自动化 PR-gate 兜底；AC-19/20-BP/流隔离/①–④ 由本 evidence doc 审查 + cross-ref）。无新增自动化测试（避免冗余 pt-009）。
