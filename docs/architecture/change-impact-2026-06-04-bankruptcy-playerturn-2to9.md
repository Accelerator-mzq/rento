# Change Impact — 破产(9) ↔ 玩家回合(2) 2↔9 接口对齐

> **状态**: ✅ COMPLETE — producer 执行 `/propagate-design-change` 闭合(2026-06-04);player-turn 13 处对齐 return-only,fresh-grep 双侧核实 (a)-(d) 通过(见 §7)。player-turn 降 NEEDS RE-REVIEW◊,待 `/design-review` 重过三锁面。
> **创建**: 2026-06-04(`/design-review design/gdd/bankruptcy.md` full 模式触发)
> **方向裁定(user 拍板 2026-06-04)**: 改 **player-turn(Approved)** 对齐 bankruptcy 的 **return-only** 架构
> **CD 背书**: creative-director 明确技术信心(无环纪律是真实架构安全阀,return-only 比 player-turn 双向耦合更健壮)
> **双独立确认**: systems-designer + qa-lead 各自独立审出同一矛盾;主审 fresh-grep 双侧坐实

## 1. 矛盾根因

bankruptcy(In Design)把系统9 设计为 **return-only 编排子程序**(回合2 调 `9.ResolveBankruptcy(debtor,creditor,snapshot)→{debtorEliminated,winnerId,rankingEntry}`,9 绝不回调 2,防 2↔9 环)。但 player-turn(**已 Approved**)仍编码旧的**双向耦合**模型(9 直接写 `bIsBankrupt`、9 消费 `OnLastPlayerStanding`)。两档各自内部自洽、彼此架构对立。任一下游实现都会撞上其一。

bankruptcy line 157「双向一致性核查」节自称查过却**完整漏掉**此接缝——已在本轮就地标注失真修正(指向本工单)。

## 2. 四个矛盾面(BLOCKING)

| # | 矛盾面 | bankruptcy(目标对齐侧) | player-turn(待改侧) |
|---|---|---|---|
| 1 | **谁写 `bIsBankrupt`** | 9 return-only、不写;回合2 据 `debtorEliminated=true` 写(CR-3/CR-5/AC-3 写 spy==0、AC-2 回调 spy==0) | **L98**:`SetBankrupt(bool)` 唯一合法调用方=9;**L247**「9 置 bIsBankrupt」;**L413**「9 写 bIsBankrupt」→ 即 9→2 回调、2↔9 环回来 |
| 2 | **APC 公式** | F-2 `count(p≠debtor ∧ !bIsBankrupt)`(显式排除 debtor、不依赖写时序) | **L311** F-2 `\|{p \| !bIsBankrupt}\|`(依赖 flag 已写)→ return-only 下末局两式给相反「游戏是否结束」判定 |
| 3 | **胜负信号协议方向** | 9 返回 `winnerId` → 回合2 触发 `OnGameWon`(AC-40) | **L247/L413** `2 广播 OnLastPlayerStanding → 9 消费裁决`→ 信号名不同、方向相反、double-fire 风险 |
| 4 | **APC==0 / P=2 双破产终局** | AC-29 断言「不产 winnerId=-1」+ fatal log/取末存活快照 | **L321** 应急路径「广播 `OnLastPlayerStanding(WinnerId=INDEX_NONE, bDegenerateTie=true)` 给 9 消费」→ 退化局契约不可调和 |

## 3. player-turn 待改五面(producer propagate 时执行)

1. **L98 受控写接口表**:`bIsBankrupt` 的「唯一合法调用方」从「破产胜负(9)」改为「**本系统(回合2),据系统9 返回值 `debtorEliminated=true` 写入**」(9 不再直接调 `SetBankrupt`)。
2. **L247 Interactions 表**:「9 置 bIsBankrupt;本系统据此移出轮转、报 OnLastPlayerStanding」→「9 **返回裁决** `{debtorEliminated,winnerId,rankingEntry}`,本系统据返回值置 bIsBankrupt + 触发 `OnGameWon`」。
3. **L311 F-2**:APC 公式改为本档显式排除 debtor 式,或明确改为「APC 由 9 在快照上算并 return」(消除 player-turn 侧 flag-依赖重算)。
4. **L413 Dependencies 破产胜负(9)行**:「写 bIsBankrupt;消费 OnLastPlayerStanding」→「返回裁决,本系统据此写 bIsBankrupt + 触发 OnGameWon」。
5. **胜负信号 schema**:统一为单一协议(9 返回 winnerId → 2 触发 `OnGameWon`),消除 `OnLastPlayerStanding` 反向信号,或明确二者单一方向关系;统一 APC==0 退化局 winnerId=INDEX_NONE 的契约归属(9 返回 vs 2 广播 payload,见 L321 应急路径)。

## 4. 触发 player-turn 重审的锁面

player-turn 改动触及三个锁面 → propagate 后须重过 `/design-review design/gdd/player-turn.md`:
- F-2 公式(APC 重算归属)
- 受控写接口面(L98 SetBankrupt 调用方)
- 胜负信号 schema(OnLastPlayerStanding vs OnGameWon)

## 5. fresh-grep 双侧核实验收标准(CD 定)

propagate 闭合后,grep player-turn 与 bankruptcy 两档须满足:
- (a) `bIsBankrupt` 的写者、APC 公式、胜负信号方向三处 **单侧唯一、双侧一致**;
- (b) AC-2 spy==0 在 return-only 架构下可真实断言且通过;
- (c) P=2 双破产终局两档对 winnerId 取值给出**同一个**契约;
- (d) bankruptcy line 157 核查节如实反映 2↔9 已对齐(回填,删「闭合后回填」占位)。

## 6. 关联

- 触发审查: `design/gdd/reviews/bankruptcy-review-log.md`(2026-06-04 MAJOR REVISION NEEDED)
- 待改档: `design/gdd/player-turn.md`(Approved)
- 本档: `design/gdd/bankruptcy.md`(In Design,自闭合项已在本轮就地修)
- CD 流程约束(active.md):跨档接缝改须同 PR fresh-grep 双侧核实,不接受登记 OQ 当闭合

## 7. 执行与验收(2026-06-04 `/propagate-design-change` 闭合)

**方向**:改 player-turn(Approved)对齐 bankruptcy return-only(user 拍板 + CD 背书,本次只执行)。

### 7.1 player-turn.md 改动(17 处 = 13 工单面 + 4 fresh-grep 增补)

工单 §3 登记 5 面;fresh-grep 全集重盘后实改 **13 面**(工单清单漏列 6 面:Winner 状态行 / 事件清单 / `OnLastPlayerStanding` payload 块 / `APC==0` 应急路径 / Edge「仅剩1」/ AC-40)。落盘后**再 fresh-grep 又揪出 4 处自身清单亦漏的真残留**(纪律:登记清单必漏,执行须 fresh-grep 重盘):

| 增补 | 位置 | 残留 → 修 |
|---|---|---|
| E1 | payload struct 清单 | 删已废 `FLastPlayerStandingResult`(`OnGameWon(int32)` 单 int 无 USTRUCT) |
| E2 | Visual/Audio 事件清单 | `OnLastPlayerStanding` → `OnGameWon` |
| E3 | **AC-12** | 整条测对象 `OnLastPlayerStanding` 广播幂等 → `OnGameWon` 广播幂等(旧广播已删=真矛盾) |
| E4 | OQ-5 描述 | "当前 CR-6 仅有 `OnLastPlayerStanding`" → `OnGameWon`(原已删) |

改动面清单:Status(降 NEEDS RE-REVIEW◊)/ 受控写面 L98 / CR-6 / 状态表 Winner 行 / Interactions / 事件 schema(清单+payload)/ F-2 Output Range + APC==0 路径 + Example / Edge×3 / Dependencies / AC-40 / E1-E4。
**历史 changelog 故意保留不改写**:L20(R3 摘要)/ L463-465(R1-R3 AC 修订摘要)仍含 `OnLastPlayerStanding` 字样,是当时修订的真实记录,非活跃规定。

### 7.2 双侧 fresh-grep 验收(CD 约束:不凭编辑成功宣称闭合)

- **(a) 单侧唯一 + 双侧一致** ✓:`grep OnLastPlayerStanding` player-turn 残留**全部**为历史 changelog(L20/465)或显式「已删除」语境标注,**无活跃旧规定**;两档胜负信号统一 `OnGameWon`。`bIsBankrupt` 写者两档一致 = 回合2 据 `debtorEliminated`(9 不直写,player-turn L98 / bankruptcy CR-5/AC-3)。APC 胜负口径**单源归9**(snapshot 显式排除 debtor),player-turn F-2 降为写后计数辅助、不独立判胜。
- **(b)** bankruptcy AC-2(对回合2 公开接口加 spy → 计数==0)在 return-only(9 绝不回调2)下**可真实断言**;player-turn 全文一致编码「9 return-only / 不回调本系统」 ✓
- **(c)** P=2 双破产终局:bankruptcy AC-27(winnerId==B)+ AC-29(退化局9 内 fallback);player-turn AC-40(winnerId==INDEX_NONE→不触发 OnGameWon,退化局归9 不重测)。**两档对 winnerId 取值同一契约** ✓
- **(d)** bankruptcy L159 双向一致性核查节**已回填**(删「闭合后回填」占位,改为闭合陈述 + 双侧一致结论) ✓

### 7.3 状态流转

- `player-turn.md`:APPROVED(R8)→ **NEEDS RE-REVIEW◊**(触三锁面 F-2 胜负口径 / 受控写接口面 / 胜负信号 schema)。
- `systems-index.md`:#2 `Approved` → **`re-review※`** + ※脚注;#9 维持 `NEEDS REVISION◊`(本闭合解除其 2↔9 blocker 之一)。
- `bankruptcy.md`:无规则改动,仅 L159 回填(本档 return-only 设计自始正确)。

### 7.4 后续(必做)

1. **`/design-review design/gdd/player-turn.md`** 重过三锁面(F-2 公式 / 受控写接口面 / 胜负信号 schema)——独立验证,审者须独立于本授权上下文。
2. 通过后:player-turn 回 APPROVED、systems-index #2 去 ※、#9 回 Approved 候选。
3. bankruptcy.md 仍待其自身 `/design-review` 独立验证(MAJOR REVISION NEEDED 的 2↔9 根因已闭合,余项见 review-log)。
