---
Epic: player-turn
Status: Ready
Layer: Foundation
Type: Logic
Estimate: M
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 003 — F-1 回合推进 + 破产移出 + OnGameWon 单一事件源（return-only）

## Context

- **GDD**: `design/gdd/player-turn.md` — F-1（NextActivePlayer，守卫先行 + 入口规范化）、F-2（ActivePlayerCount，计数辅助非裁决器）、CR-6（破产移出与胜负信号，对齐破产9 return-only）、Edge Cases（自回合破产/他人回合翻转/仅剩 1 人）
- **Requirement TR-ID**: TR-turn-006（OnGameWon 单一事件源纪律：由回合2 据破产9 return-only 返回的 winnerId 广播，9 不直触发、不回调）
- **ADR Governing**:
  - **ADR-0003（primary）— 事件总线**：`OnGameWon` 由**回合2 触发**（破产9 仅 return winnerId，return-only，不直接广播）= 单一事件源；`OnGameWon(int32 WinnerId)` 单 int 参数可直接作 dynamic multicast delegate 参数（无须包 USTRUCT）。
  - **ADR-0007 — BP/C++ 权威边界**：F-1/F-2 拓扑计数 + `bIsBankrupt` 写 = 写权威状态 → 落 C++ 独立 `UObject`/纯函数，`-nullrhi` headless 可测、可 spy。
  - **ADR-0001 — UObject 宿主与生命周期**：F-1/F-2 在回合2 World Subsystem 内执行；`bIsBankrupt` 字段归回合2 PlayerState（9 经返回值驱动写不直写，防 2↔9 环）。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（ADR-0003 delegate 框架自 UE4 起稳定）
- **Engine Notes（ADR-0003 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**（delegate 宏与 `BlueprintAssignable` 均非 5.4–5.7 新增）。
  - Verification Required: BP 端 `AddDynamic`/`RemoveDynamic` 在读档重绑路径下的解绑幂等性（本 story 关注 `OnGameWon` 边沿幂等，读档重绑见 story-008）。
- **Control Manifest Rules（Foundation Layer）**:
  - **Required**: 单一事件源——每个状态变更恰一个 owner 广播。`OnGameWon` 由回合2 触发（破产9 return-only）（source: ADR-0003）。owner 系统先同步算定权威结果，再广播事件（结算同步先行，呈现事件后随）（source: ADR-0003）。
  - **Required（Core）**: 写 `PlayerState`/状态机 phase 落 C++（source: ADR-0007）；F-1/F-2 封装为可 headless 测的 C++ 纯函数/`UObject`。
  - **Forbidden**: Never 把破产事件合并为单事件（删经济5 `OnBankruptcyDeclared`）（source: ADR-0003）。Never 让呈现层回写或被反调（source: ADR-0003）。
  - **Guardrail**: Event delegate broadcast O(订阅数)，MVP 回合制低频可忽略；CPU 帧预算 16.6 ms / 60 FPS。

## Acceptance Criteria

- [ ] AC-1（GDD AC-14/AC-15/AC-16）: F-1 NextActivePlayer 基础寻路——(2,4,{0,2,3})→3；环绕+跳过 (3,4,{0,3})→0；多跳 (0,4,{0,3})→3（跳 1,2）。
- [ ] AC-2（GDD AC-17/AC-17b）: F-1 守卫——(0,4,{0})→INDEX_NONE（|A|=1 且唯一存活者==cur）；(0,2,{1})→INDEX_NONE（|A|=1 且唯一存活者≠cur，断言**不**返回 1，守卫先于枚举）。
- [ ] AC-3（GDD AC-17c）: F-1 入口规范化枚举终止性（全员存活 A={0,1,2,3}、P=4）三变体：① cur=-4→`cur_safe=0`→next=1；② cur=-2→`cur_safe=2`→next=3；③ cur=5→`cur_safe=1`→next=2（断言 `next∈A 且 ≠cur_safe`、不返回负值、不无命中终止）。
- [ ] AC-4（GDD AC-19/AC-20）: F-2 计数——([F,T,F,F])→3；([F,T,T,T])→1（前提：已据9 返回值写完 `bIsBankrupt` 之后读取）；([T,T,T,T])→0 触发 assert（正常流程不可达）。
- [ ] AC-5（GDD AC-29/AC-32）: 当前玩家自回合破产→TurnEnd 移出、F-1 推进（前提：fixture mock 破产9 置 `bIsBankrupt=true`）；双点后落地破产→移出、无额外回合。
- [ ] AC-6（GDD AC-40a）: 在 C++ headless test 层注入 `IResolveBankruptcy` stub，断言：① 返回 `{debtorEliminated=true, winnerId==B.PlayerId}`（APC==1）→触发 `OnGameWon(WinnerId)` 恰 1 次、payload `WinnerId==B.PlayerId`；② 返回 `winnerId==INDEX_NONE`（APC>1 / 退化局 APC==0）→**不**触发 `OnGameWon`（计数==0）；③ 据 `debtorEliminated=true` 置 `bIsBankrupt` + 移出轮转。**此条不得降级**（return-only 无环架构安全阀的机器证明底线）。
- [ ] AC-7（GDD AC-12）: `OnGameWon` 边沿幂等——连续两次以同 fixture 返回相同 `{debtorEliminated=true, winnerId==B.PlayerId}`，监听计数**精确==1**（第二次因已进 `Winner` 终态被边沿守卫拦截不重发）；同时断言 F-1 守卫返回 INDEX_NONE。时序前提：先写 `bIsBankrupt`→再触发 `OnGameWon`→再更新 F-1 输出。
- [ ] AC-8（GDD AC-40c）: 封堵旧 2↔9 双触发——GIVEN 玩家A `bIsBankrupt=true`（直接置 flag、不经9 ResolveBankruptcy）、玩家B `bIsBankrupt=false`；WHEN B 完成 TurnEnd、F-1 推进（本回合无任何 `ResolveBankruptcy` 调用）；THEN `OnGameWon` 广播计数 **==0**（本系统不因自身 F-2 `APC==1` 独立触发胜负信号）。

## Implementation Notes

> 逐字保真自 GDD F-1/F-2/CR-6 + ADR-0003 单一事件源裁定，不改写语义。

1. **F-1 守卫先行（GDD L289-293）**：调用前先查 `|A|`：`|A|=0`→返回 `INDEX_NONE`+assert（正常流程不可达）；`|A|=1`→返回 `INDEX_NONE`（仅剩 1 名未破产玩家时对局已由 CR-6 终结，**不**返回那个唯一存活者 index）；`|A|≥2`→进入枚举。守卫判据=`|A|`（存活者数），与 cur 是否∈A 无关。
2. **F-1 入口规范化（GDD L295-297）**：枚举入口先 `cur_safe = ((cur % P) + P) % P`（欧几里得取模，保证 `cur_safe ∈ [0,P-1]`），再枚举 `candidate_k = mod(cur_safe + k, P)`，`k = min{ k ∈ [1, P-1] | candidate_k ∈ A }`。安全性唯一落点=入口规范化（不再依赖「负 candidate 被 A 成员性过滤」旧论证）。
3. **F-1 dev/shipping 职责分离（GDD L315）**：① shipping 用 `cur_safe`（枚举实际使用）；② 另加 `ensureMsgf(cur >= 0 && cur < P, ...)` 作 dev 诊断（检查**原始** cur，规范化前）。二者不冲突。
4. **F-2 胜负判定归属（GDD L331）**：本系统 F-2 是据 `bIsBankrupt` 字段的**计数辅助**（供轮转/一致性自检/诊断），**非独立胜负裁决器**。胜负条件由破产9 在 `activePlayersSnapshot` 上算 APC（显式排除 debtor、不依赖 flag 写时序）返回 `winnerId`；本系统**不**在破产结算时独立重算「APC==1→胜利」。本 F-2 计数仅在本系统**已据返回值写完 `bIsBankrupt` 之后**作一致性自检/轮转辅助。
5. **CR-6 破产移出（GDD L205）**：玩家破产经9 `ResolveBankruptcy(debtor, creditor, activePlayersSnapshot)` 裁决——**9 为 return-only、绝不回调本系统**，返回 `{debtorEliminated, winnerId|INDEX_NONE, rankingEntry}`。本系统据 `debtorEliminated=true` 置 `bIsBankrupt=true`→永久移出轮转；据 `winnerId` 触发 `OnGameWon(winnerId)`（`winnerId==INDEX_NONE`→不触发）。退化局（APC==0）检测/fallback/胜者取值全归9 内部。
6. **OnGameWon 边沿触发（GDD L205）**：单次 `ResolveBankruptcy` 返回 `winnerId!=INDEX_NONE` 后**恰广播一次**；对局进入 `Winner` 终态后，不因后续任何 F-2 计数/一致性自检/状态读取/定时回调再次广播（电平→边沿）。

## Out of Scope

- ETurnPhase 状态机阶段转换序列 + 双点链 F-3 → **story-002**（本 story 在状态机 TurnEnd 处调用 F-1）。
- 受控写接口 `SetBankrupt(bool)` 的封装强度 → **story-005**（本 story 调用其写 `bIsBankrupt`，setter 封装归 005）。
- `OnGameWon` delegate 的声明形态（dynamic multicast + `BlueprintAssignable`）→ **story-004**（本 story 验触发语义/次数；delegate 契约声明归 004）。
- 破产9 内部 `ResolveBankruptcy` 实现（debtor 排除/APC 算法/退化局 fallback）→ bankruptcy epic。
- `OnPlayerBankrupt`（破产9 3 字段）与经济5 `OnBankruptcyDeclared`——不在本 story（OQ-5 MVP scope 裁定）。

## QA Test Cases

> 本 story 为 Logic（拓扑公式 + 事件触发次数）；每 AC 一条 Given/When/Then；确定性 fixture、headless `-nullrhi`。破产9 用 C++ `IResolveBankruptcy` stub 注入（headless 恒可行）。

- **TC-1 (AC-1)**: WHEN F-1 输入 (cur=2,P=4,A={0,2,3})/(3,4,{0,3})/(0,4,{0,3})，THEN next=3/0/3。
- **TC-2 (AC-2)**: WHEN F-1 输入 (0,4,{0})/(0,2,{1})，THEN 均 INDEX_NONE（后者断言不返回 1）。
- **TC-3 (AC-3)**: GIVEN A={0,1,2,3}、P=4，WHEN cur=-4/-2/5，THEN next=1/3/2（断言 `next∈A 且 ≠cur_safe`，不返回负值）。
- **TC-4 (AC-4)**: GIVEN bIsBankrupt=[F,T,F,F]/[F,T,T,T]/[T,T,T,T]，WHEN F-2，THEN APC=3/1/0（[T,T,T,T] 触发 assert）。
- **TC-5 (AC-5)**: GIVEN 当前玩家自回合 mock 破产9 置 `bIsBankrupt=true`，WHEN TurnEnd，THEN 移出 + F-1 推进；GIVEN 双点后落地破产，THEN 移出、无额外回合。
- **TC-6 (AC-6)**: GIVEN `IResolveBankruptcy` stub 返回 `{debtorEliminated=true, winnerId==B}`（APC==1），THEN `OnGameWon` 计数==1、`WinnerId==B`；GIVEN 返回 `winnerId==INDEX_NONE`，THEN 计数==0；据 `debtorEliminated=true` 置 `bIsBankrupt` + 移出。
- **TC-7 (AC-7)**: GIVEN stub 连续两次返回相同 `{debtorEliminated=true, winnerId==B}`，WHEN 本系统据此触发，THEN `OnGameWon` 监听计数精确==1（第二次被 `Winner` 终态边沿守卫拦截），F-1 守卫返回 INDEX_NONE。
- **TC-8 (AC-8)**: GIVEN A `bIsBankrupt=true`（直接置、不经9）、B `bIsBankrupt=false`，WHEN B TurnEnd + F-1 推进（无 `ResolveBankruptcy` 调用），THEN `OnGameWon` 计数==0。
- **Edge cases**: cur∉A 且 |A|≥2（cur 已破产异常入参）仍正确寻下一个 A 成员（防御性）；F-1 守卫 `|A|=1` 返回 INDEX_NONE 不依赖「唯一存活者==cur」；APC==0 退化局（MVP 单线程不可达）只 assert，不触发任何 2→9 反向信号。

## Test Evidence

- **Type**: Logic
- **Path**: `tests/unit/player-turn/nextactiveplayer_bankruptcy_ongamewon_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-001（PlayerState 容器，含 `bIsBankrupt`/`TurnOrderIndex` 字段）、story-002（状态机 TurnEnd 处调用 F-1）。
- **Unlocks**: story-004（`OnGameWon` delegate 契约声明承接本 story 触发语义）、story-008（破产/胜负态序列化）。
