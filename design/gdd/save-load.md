# 存档/读档 (Save/Load)

> **Status**: Approved (R-1 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕;遗留 OQ-Save-1..6 上游/架构债 + propagate 债 + Flags 不阻开工)
> **Author**: msc + systems-designer
> **Last Updated**: 2026-06-06
> **Implements Pillar**: ④易上手轻松局（随时退出/续局，降低"开一局"成本）
> **System**: #21 in systems-index | Persistence | MVP
> **依赖**: 棋盘数据(1, Approved) / 玩家回合(2, Approved) / 经济现金(5, Approved-with-followups) / 地产所有权(6, Approved-with-followups)；间接横切建房(8, Approved-with-followups) / 破产胜负(9, Approved) / 骰子(3, Approved)
> **MVP 范围**: 单存档槽 + 手动存/读 + 退出续局。多槽/云存/自动存 = Alpha/Full Vision（OQ-Save-3）。

<!-- 骨架由 /design-system 创建。逐节设计。
     核心约束（供续做参考）：
     - 横切系统：不拥有任何 gameplay 状态，只"采集→序列化→写盘 / 读盘→反序列化→重建→重绑"。
     - 高风险=序列化遗漏：任一状态系统字段漏存 = 读档损坏（隐形 bug）。本档以「逐状态系统可序列化契约清单」为核心防漏机制。
     - 不臆造上游接口：各字段名/事件名逐字对齐 owner GDD；未定接口（如 SaveGame 宿主 ADR）留 OQ。
     - 引擎 UE5.7 Blueprint 为主；USaveGame/UGameplayStatics::SaveGameToSlot 是稳定 API（pre-5.3）。
-->

## Overview

存档/读档系统是《骰子大亨》的**持久化横切层**:它本身**不拥有任何 gameplay 状态**,职责是把分散在各状态系统(棋盘1/玩家回合2/经济5/地产6/建房8/破产9/骰子3)里的"当前对局快照"**采集 → 序列化写盘**,以及读盘后**反序列化 → 重建各系统状态 → 重绑 delegate 监听器**,使玩家退出后能从断点续局。MVP 范围严格收敛为**单存档槽 + 手动存/读 + 退出续局**——一个对局只有一个槽位,玩家显式触发"保存"与"读取/继续",退出游戏时可写入续局点。多存档槽、云存档、自动存档(autosave)一律 deferred 到 Alpha/Full Vision(OQ-Save-3),不进 MVP 正文制造接缝。本系统的最大风险是**序列化遗漏**:任一状态系统的某个字段漏存,读档后该字段回退默认值,造成**隐形损坏**(如漏存 `ConsecutiveDoubles` → 读档后第3次双点不入狱、漏存 `Die1`/`Die2` → 读档后骰面 VFX 无法重现)。因此本档以一份**逐状态系统的可序列化契约清单**(Detailed Rules CR-3)为核心防漏机制,字段名逐字对齐各 owner GDD,并要求每个继承义务都落可证伪 AC。对玩家而言这个系统隐形:他们只感受到"我退出了,回来还是我离开时那一局",而不会意识到背后有一份精确的状态序列化。

## Player Fantasy

> *本系统为持久化基础设施,无独立强 player fantasy(横切服务层,同棋盘/骰子)。玩家不会想"这是一份 USaveGame 数据"。*

存档系统服务的情感目标是**间接的、隐形的**:它兑现支柱④"易上手轻松局"中**"随时能停、随时能续"的低承诺前提**。玩家真正感受到的是——晚饭打断了一局对 AI 的对战,关掉游戏,第二天打开"继续",发现**自己的现金、地产、建的房、轮到谁、刚掷出的骰子全都在**,仿佛从未离开。这降低了"开一局"的心理成本(支柱④):不必担心"开了一局就得一口气打完"。本系统对幻想的唯一贡献是一条隐形的信任底线:**"我的进度不会丢、续局后的局面与离开时完全一致"**。当读档后的对局状态与存档前**逐字段一致**(谁该行动、各人现金、地产归属与抵押、每格房数、骰子结果)、且后续操作无任何异常时,玩家把"继续"当作理所当然,全部情绪投向博弈本身——而非"会不会读档把我的房弄没了"的猜疑。它的成功标准与棋盘数据、玩家回合一样是**隐形的正确**:读档损坏一次,信任就崩塌(玩家会再也不敢中途退出)。

> **(横切层诚实声明)** 本系统是 **enable-not-own**:它**不拥有**任何被序列化字段的语义(现金归经济、房数归建房、阶段归回合),只拥有"采集/写盘/读盘/重建/重绑"这套搬运流程。任何关于"某字段该不该存、存什么值"的语义裁决归该字段的 owner GDD;本档只负责"把 owner 声明为可序列化的字段一个不漏地搬运,且搬运后值不变"。

## Detailed Rules

### Core Rules

**CR-1 单存档槽 + 手动存/读 + 退出续局(MVP 范围闸门)。** MVP 提供**恰一个**存档槽位 `SLOT_DEFAULT`:
- **手动保存** `SaveGameToSlot()`:玩家从暂停菜单显式触发,采集当前对局快照、序列化、写入 `SLOT_DEFAULT`(覆盖旧内容)。
- **读取/继续** `LoadGameFromSlot()`:主菜单/暂停菜单显式触发,读 `SLOT_DEFAULT`、反序列化、重建各系统状态、重绑监听器,恢复对局。
- **退出续局**:玩家选择"退出到主菜单/退出游戏"时,若当前在合法可存档点(CR-4),写入 `SLOT_DEFAULT` 作为续局点;下次进入显示"继续游戏"。
- **明确不在 MVP**:多槽位、命名存档、云同步、定时自动存档(autosave)、存档缩略图——全部 deferred(OQ-Save-3),不在本档正文规格化。

**CR-2 存档=DA 引用 + 可变状态快照,不存静态布局(继承 board CR-3/L151/L164)。** 存档**不复制棋盘静态布局**(40 格的类型/价格/租金表等)。棋盘数据(1)运行时不可变(board L139),存档只存**棋盘 DA 的稳定标识** `BoardIdentifier`(board 经 `GetBoardId()` 提供,类型 `FName`);读档时按此 ID 重新加载对应 `DA_Board_*` 资产,再把可变状态(归属/房数/现金等,以 `TileIndex` 或 player 为键)叠加其上。**理由**:静态布局可由 DA 资产重建(无需复制),只有运行时可变状态是真正的对局进度。**版本兼容前提**:读档时若 `BoardIdentifier` 对应的 DA 不存在(资产被删/改名)→ 读档失败(EC-3)。

**CR-3 逐状态系统可序列化契约清单(核心防漏机制)。** 本系统采集以下**且仅以下**字段写入存档;每个字段名逐字对齐其 owner GDD,owner 负责语义、本系统负责无遗漏搬运。**新增任何状态系统字段而未登记此表 = 序列化遗漏 bug 的源头**——故本表是 review 的核对锚点。

| Owner 系统 | 可序列化字段(逐字对齐 owner) | 键 | 取值来源(读) / 重建(写) | 回链 owner GDD |
|---|---|---|---|---|
| 棋盘(1) | `BoardIdentifier`(`FName`,DA 引用 ID,**非全量布局**) | 棋盘级 | 读 `GetBoardId()` / 读档按 ID 加载 DA | board CR-3 / L151 / L164 |
| 玩家回合(2) | 全部 `PlayerState`(11 字段全列:`bIsAI`/`AIDifficulty`/`bIsBankrupt`/`bIsInJail`/`JailTurnsServed`/`ConsecutiveDoubles`/`Cash` 持有方/**`CurrentTileIndex`(棋子所在格,owner=移动4)**/`DisplayName`/`PlayerColor`/`PlayerId`——逐一对齐 player-turn L83/L389,**不可吞字段**)+ 定序结果 + `CurrentPlayerIndex`(当前回合玩家)+ `CurrentPhase`(`ETurnPhase`)+ `ConsecutiveDoubles` + 锁定的 `DOUBLES_JAIL_THRESHOLD` | per-player + 全局 | 读回合2 状态容器 / 读档 `switch(CurrentPhase)` 重入精确阶段 | player-turn AC-34 / L83 / L252 / L389 / L420 / Edge L390 |
| 玩家回合(2) | 当前回合**完整 `FDiceRollResult{Die1,Die2,Total,bIsDouble}`**(**不可只存 `(Total,bIsDouble)`**) | 全局(当前程) | 读回合2 持有的本回合骰结果 / 读档不重掷、还原全字段 | dice B1 / OQ-T-Dice-5 / player-turn AC-34 |
| 经济(5) | 每玩家 `Cash`(`int32`) | per-player | 读经济 `GetCash(player)` / 读档写回 `Cash`(经容器,不经 Credit/Debit 事件) | economy L92 / L114 / L332 |
| 地产所有权(6) | owner map(每可购买格一 `PlayerId`,`INDEX_NONE`=无主)+ `bIsMortgaged`(每可购买格一 `bool`) | per-tile(`TileIndex`) | 读 6 的 owner map + 抵押标记 / 读档写回。**派生量(`is_monopoly`/`station_count`/`utility_count`)不存,读档后重算**(property L97/L99) | property L99 / L114 |
| 建房(8) | `house_count`(每 Property 格一 `int32` ∈ [0,5],**per-tile**) | per-tile(`TileIndex`) | 读 8 的 per-tile house_count / 读档写回 | building L57 / L63 |
| 骰子(3) | **MVP 不序列化当前 `Seed`**(当前骰结果由回合2 完整 `FDiceRollResult` 保护;读档重设新非确定种子) | — | 不存 / 读档 `SetSeed(非确定值)`,**不可重设回开局种子** | dice L59 / L73 / L190 / OQ-2 |
| 破产(9) | **无独立可序列化状态**——`bIsBankrupt` 归回合2 PlayerState(已在 2 行覆盖);`net_worth`/排名为派生/MVP 不计算 | — | 见回合2 行 | bankruptcy CR-5 / F-1 |

> **关键裁定**:`bIsBankrupt` 字段归**回合2** PlayerState(bankruptcy CR-5 / L50),不归破产9,故在回合2 行序列化,破产9 无独立存档腿。`house_count` 是 **per-tile [0,5]**(建房8 拥有,**非6**,防 6↔8 环;building L57),与破产/AI 用的"全盘 house 总数"是不同接口,本档只存 per-tile 原始态。

**CR-4 合法可存档点 / 禁止存档的瞬态(与回合2 协同)。** 存档只能在**稳定回合边界**触发,以下瞬态**禁止中途存档**(序列化瞬态债务会造成读档损坏):
1. **开局定序进行中禁存档**(player-turn Edge L390):`TurnOrderInitRank` 阶段(可能多轮重掷)的"哪些席位已定 rank / 当前 round"是瞬态;存档仅在定序完成、进入首个 `TurnStart` 后可用。
2. **Raising Funds 筹款瞬态禁中途存档**(economy L92 + CR-3.3,与回合2 协同 OQ-Econ-4):当玩家进入 Raising Funds 阻塞子相(强制扣款 > 现金、正在抵押/卖房凑钱)时,存档**不应捕获该瞬态**(否则序列化"还欠多少钱"的中间债务态、读档后无法干净恢复)。本系统在该瞬态期间**拒绝保存请求**(返回 `SaveRejected_TransientState`),等回合2 解决瞬态(回 Solvent 或移交破产)后才允许。
3. **破产清算编排进行中禁存档**:`ResolveBankruptcy` 编排序(批量 in-kind 移交 / 银行回退,property 批量原子)进行中是全或全无的瞬态,禁中途存档。
> **裁决归属**:"当前是否在合法可存档点"的**最终判据归回合2**(回合2 拥有阶段状态机);本系统**查询**回合2 的可存档标志,不自行裁决阶段语义。具体查询接口名未定 → OQ-Save-2。

**CR-5 读档重建序(顺序敏感,防依赖倒置)。** 读档须按**依赖拓扑序**重建,避免"重建 A 时读了尚未重建的 B":
1. 校验存档(版本号 + 校验和 + 字段清单,F-1/F-2);
2. 按 `BoardIdentifier` 加载棋盘 DA(1);
3. 重建经济(5)`Cash`、地产(6)owner map + `bIsMortgaged`、建房(8)`house_count`(三者互不依赖,可任意序);
4. 重建回合(2)`PlayerState` + 定序 + `CurrentPlayerIndex` + `CurrentPhase` + `ConsecutiveDoubles` + 阈值 + 完整 `FDiceRollResult`;
5. 骰子(3)`SetSeed(新非确定种子)`;
6. **重绑 delegate 监听器**(CR-6);
7. 进入 `switch(CurrentPhase)` 恢复精确阶段(player-turn AC-34)。
> 派生量(`is_monopoly`/`station_count`/`utility_count`)在重建后由各 owner **按需重算**(property L97),本系统不存不重建。

**CR-6 读档后重绑 delegate 监听器(继承义务③,关键)。** 反序列化只恢复**数据字段**;运行时的 **delegate 订阅关系**(谁监听 `OnCashChanged`/`OnOwnershipChanged`/`OnDiceRolled`/`OnPhaseChanged` 等)**不被序列化**,读档后必须**重新绑定**——否则读档后 HUD/VFX 不再收到更新事件(画面静止、juice 不播),是典型读档损坏。本系统在 CR-5 步6 触发各呈现层/消费方**重新 `AddDynamic` 订阅**。**裁决归属**:具体"哪些监听器要重绑"归各消费方(HUD16/VFX19/property card17)在其重建/重激活时自行 `AddDynamic`;本系统**广播一个"读档完成、状态已就绪"信号** `OnGameLoaded`,各消费方监听此信号执行重绑。**特别注意**(player-turn AC-34):还原精确阶段还须**重绑该阶段的 delegate**(枚举字段恢复值不够,如 PostRollAction 阶段须重 `AddDynamic` 该阶段事件绑定)。

> **UE5.7 实现注(归 OQ-Save-1 ADR)**:
> - 用 `USaveGame` 子类持有所有序列化字段(`UPROPERTY(SaveGame)` 标记)+ `UGameplayStatics::SaveGameToSlot/LoadGameFromSlot`(pre-5.3 稳定 API)。
> - **`FRandomStream` 经 `UPROPERTY` 反射序列化可能只持久化 `InitialSeed` 而丢当前 `Seed`**(dice L176)——但 MVP 本就不存 Seed(CR-3 骰子行),此陷阱在 MVP 不触发;Full Vision 重放(OQ-2)需存 Seed 时须用显式 `GetCurrentSeed()`/`SetSeed()` 存取,勿靠 struct 反射。
> - **`FTimerHandle` / Latent Action(BP `Delay`/`WaitForEvent`)不可序列化**(player-turn L391):回合状态机须用 `ETurnPhase` 枚举字段 + delegate 推进(可序列化、`switch(CurrentPhase)` 重入),**不可**用 Latent Action 实现阶段等待。此约束归回合2 OQ-1 ADR,本档读档重建依赖其成立。
> - **枚举 append-only**:`ETurnPhase`/`EPlayerColor`/`EAIDifficulty` 等已有枚举值的整数索引**不可重排**(否则旧存档读出错误枚举),player-turn AC-43 已登记。本档版本兼容(F-2)依赖此约束。

## Formulas

> 本系统**不拥有任何 gameplay 平衡公式**(无现金/租金/概率数学)。以下公式只规格化**存档完整性校验**:版本号、校验和、字段清单。这些是工程完整性公式,非平衡数值。

### F-1 存档完整性校验(读档前置门)

读档前必须通过完整性校验,任一项失败 → 拒绝读档(走 EC-2/EC-3/EC-4):

`load_valid = (magic_ok) ∧ (version_compatible) ∧ (checksum_ok) ∧ (field_manifest_complete)`

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 魔数匹配 | `magic_ok` | bool | {F,T} | 存档头 `MagicTag == SAVE_MAGIC`(辨识"这是本游戏存档") |
| 版本兼容 | `version_compatible` | bool | {F,T} | 见 F-2 |
| 校验和匹配 | `checksum_ok` | bool | {F,T} | 见 F-3 |
| 字段清单完整 | `field_manifest_complete` | bool | {F,T} | 反序列化后,CR-3 表列每个必需字段均成功读出(非缺失/非默认占位),见 F-4 |
| 总判据 | `load_valid` | bool | {F,T} | 四项全真才放行读档 |

**Output Range:** `bool`。**Example:** 魔数对、版本相同、校验和对、CR-3 全字段读出 → `load_valid=true`,读档继续(CR-5)。任一假 → 拒绝,对局不被破坏(EC-2)。

### F-2 版本兼容判据

`version_compatible = (save_version == CURRENT_SAVE_VERSION)`(**MVP:严格相等**)

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 存档内版本号 | `save_version` | int32 | ≥1 | 写盘时写入存档头的 `SAVE_VERSION` |
| 当前游戏版本号 | `CURRENT_SAVE_VERSION` | int32 | ≥1 | 当前构建支持的存档格式版本(常量) |

**Output Range:** `bool`。**MVP 立场:不做向后迁移**——`save_version != CURRENT_SAVE_VERSION` 一律判不兼容、拒绝读档并提示"存档版本不兼容"(EC-1),不尝试字段映射/迁移。**理由**:MVP 单存档槽、开发期格式可能频繁变;迁移逻辑(`save_version < CURRENT` 时升级映射)成本高、易引入错误,deferred 到格式稳定后(OQ-Save-4)。**`save_version > CURRENT`**(旧版游戏读新版存档)同样不兼容拒绝。**Example:** 存档 v1、游戏 v1 → 兼容;存档 v1、游戏 v2 → 不兼容,拒绝读档(不崩、不部分加载)。

### F-3 校验和(防截断/损坏)

`checksum_ok = (stored_checksum == compute_checksum(payload_bytes))`

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 存档内校验和 | `stored_checksum` | uint32 | [0, 2³²−1] | 写盘时对 payload 计算并写入存档头 |
| 实时计算校验和 | `compute_checksum(payload_bytes)` | uint32 | [0, 2³²−1] | 读档时对读出的 payload 重新计算(如 CRC32 / FCrc::MemCrc32) |
| payload 字节 | `payload_bytes` | bytes | — | 不含存档头本身的序列化主体 |

**Output Range:** `bool`。**用途**:检测**写盘中崩溃/磁盘损坏/外部篡改**导致的字节级损坏(EC-4)。校验和**不防** owner 字段语义错误(那是各 owner 的不变式职责),只防字节完整性。**Example:** 写盘中断电致 payload 截断 → 实时计算校验和 ≠ 存储值 → `checksum_ok=false` → 拒绝读档,旧对局/无对局,玩家不读到半截损坏存档。

### F-4 字段清单完整性(防序列化遗漏的运行时门)

`field_manifest_complete = ∀ field ∈ REQUIRED_FIELDS : deserialized(field).present == true`

**Variables:**
| 变量 | 符号 | 类型 | 范围 | 说明 |
|---|---|---|---|---|
| 必需字段集 | `REQUIRED_FIELDS` | set | CR-3 表全部行 | 写盘时把"本存档包含的字段清单"写入存档头(manifest) |
| 字段已读出 | `deserialized(field).present` | bool | {F,T} | 反序列化后该字段成功读出(存在于 payload 且类型匹配) |

**Output Range:** `bool`。**这是 CR-3 防漏机制的运行时兜底**:若代码改动新增了某状态字段但忘记写入(或读取),manifest 比对会暴露缺失。**MVP 范围**:manifest 列出 CR-3 表的字段集;读档时逐项确认已读出。**注**:此门检测"该字段在不在",**不检测**字段值语义对错(值对错归各 owner 不变式 + AC)。**Example:** 代码新增 `ConsecutiveDoubles` 到 CR-3 但写盘漏写 → manifest 标记其必需、payload 无此项 → `field_manifest_complete=false` → 拒绝读档(暴露 bug,而非静默读出默认 0)。

## Edge Cases

> 每条明确陈述"**发生什么**",非"优雅处理"。

- **EC-1 版本不兼容(`save_version != CURRENT_SAVE_VERSION`)**:`load_valid=false`(F-2)→ **拒绝读档**,弹提示"存档版本不兼容,无法继续"。**当前对局/主菜单状态不被破坏**——若从主菜单读档失败,留在主菜单;**不部分加载、不尝试迁移**(MVP)。存档文件**不删除**(留待 OQ-Save-4 未来迁移)。

- **EC-2 字段缺失(`field_manifest_complete=false`,F-4)**:某必需字段在 payload 中缺失(序列化遗漏 bug 或文件被截断)→ **拒绝整个读档**,不进行"缺失字段填默认值"的部分加载(部分加载 = 隐形损坏,违背 Player Fantasy 信任底线)。dev 构建 `ensure` + 日志列出缺失字段名;shipping 弹"存档损坏"提示、留在读档前状态。

- **EC-3 棋盘 DA 缺失(`BoardIdentifier` 对应资产不存在)**:存档引用的 `DA_Board_*` 被删/改名/未打包 → 棋盘无法重建 → **拒绝读档**,提示"存档所用棋盘不可用"。**不回退到默认棋盘**(会与存档的归属/房数 `TileIndex` 错位 → 静默损坏)。

- **EC-4 校验和不匹配 / 读档中崩溃**:`checksum_ok=false`(F-3,写盘中崩溃致截断、磁盘损坏、外部篡改)→ **拒绝读档**,提示"存档已损坏"。**写盘原子性**:保存写入**临时文件 → 写完且校验通过后再原子替换** `SLOT_DEFAULT`(rename),使"写盘中途崩溃"只损坏临时文件、**不破坏上一个有效存档**——下次读档读到的是上一次完整存档,不是半截文件。(临时文件→替换的具体 UE 实现归 OQ-Save-1。)

- **EC-5 瞬态期间请求保存(CR-4)**:玩家在 Raising Funds / 开局定序 / 破产清算编排进行中触发保存 → **拒绝保存**,返回 `SaveRejected_TransientState`,UI 提示"当前无法保存,请先完成当前操作"。**不静默捕获瞬态**(会序列化中间债务态 → 读档损坏)。等回合2 报告回到稳定可存档点后保存可用。

- **EC-6 存档槽为空 / 首次启动**:`SLOT_DEFAULT` 不存在(从未保存过)→ 主菜单"继续游戏"**禁用/灰显**(`DoesSaveGameExist(SLOT_DEFAULT)==false`);玩家只能"新游戏"。读取空槽不崩、不创建空对局。

- **EC-7 读档后立即覆盖保存**:读档恢复对局后玩家立即再保存 → 正常覆盖 `SLOT_DEFAULT`(单槽语义)。**注**:这要求读档已**完整重建 + 重绑监听器**(CR-5/CR-6),否则二次保存可能采集到未完全重建的状态。读档完成信号 `OnGameLoaded` 发出后才允许保存(与 EC-5 同闸)。

- **EC-8 读档后骰子序列与存档前可复现重复(防 dice L190)**:读档**重设新非确定种子**(CR-3 骰子行),**不可重设回开局种子**——否则读档后整条后续掷骰序列与开局可复现地重复(可被玩家观测的统计异常,"怎么读档后每次都掷一样的")。当前回合骰结果由完整 `FDiceRollResult`(含 Die1/Die2)保护、读档不重掷。

- **EC-9 存档写盘失败(磁盘满 / 权限不足)**:`SaveGameToSlot` 返回失败 → **不破坏旧存档**(原子替换前临时文件失败,旧 `SLOT_DEFAULT` 保留),UI 提示"保存失败(磁盘空间/权限)",玩家可重试或继续游戏。

- **EC-10 多次连续保存(覆盖)**:单槽语义下每次保存覆盖前次,**无版本历史**(MVP)。无并发保存(单线程回合流、保存是同步操作);不存在"两次保存竞争同一文件"——但写盘期间若再次触发保存须串行(忽略/排队,UI 短暂禁用保存按钮)。

## Dependencies

### 上游(本系统读取其状态以采集快照)

| 系统 | 关系 | 读/写内容 | 方向 |
|---|---|---|---|
| 棋盘(1, Approved) | 硬 | 读 `GetBoardId()→BoardIdentifier(FName)`;读档按 ID 加载 DA(**不存全量布局**) | 21→1 读 |
| 玩家回合(2, Approved) | 硬 | 读全 `PlayerState`(11 字段)+ 定序 + `CurrentPlayerIndex` + `CurrentPhase`(`ETurnPhase`)+ `ConsecutiveDoubles` + `DOUBLES_JAIL_THRESHOLD` + 当前 `FDiceRollResult`;读档写回并经 `switch(CurrentPhase)` 还原精确阶段 + 重绑该阶段 delegate。**可存档点裁决归回合2**(CR-4,查询接口 OQ-Save-2) | 21↔2 |
| 经济(5, Approved-with-followups) | 硬 | 读每玩家 `Cash`;读档写回 `Cash`(经容器,不经 `Credit`/`Debit` 事件,避免读档触发金钱变动 juice)。**Raising Funds 瞬态不中途存档**(economy L92,OQ-Econ-4 协同) | 21↔5 |
| 地产所有权(6, Approved-with-followups) | 硬 | 读 owner map(`TileIndex→PlayerId`)+ `bIsMortgaged`(per-tile bool);读档写回。派生量(`is_monopoly`/`station_count`/`utility_count`)不存、读档后由6重算 | 21↔6 |
| 建房(8, Approved-with-followups) | 硬(横切) | 读 per-tile `house_count`([0,5]);读档写回 | 21↔8 |
| 破产(9, Approved) | 软(横切) | **无独立存档腿**:`bIsBankrupt` 归回合2 PlayerState(已在2覆盖);`net_worth`/排名为派生/MVP 不计算 | — |
| 骰子(3, Approved) | 软 | MVP **不序列化当前 `Seed`**(当前骰由回合2 完整 `FDiceRollResult` 保护);读档 `SetSeed(非确定值)`、不重置回开局种子(dice OQ-2) | 21→3 写种子 |

### 下游(依赖本系统的系统)

| 系统 | 关系 | 内容 |
|---|---|---|
| 主菜单与大厅(20, Not Started) | 硬 | "继续游戏"按钮可用性查 `DoesSaveGameExist(SLOT_DEFAULT)`(EC-6);触发 `LoadGameFromSlot`/`SaveGameToSlot` | 20→21 |
| HUD(16) / VFX(19) / 地产卡 UI(17)(呈现层) | 软 | 监听本系统 `OnGameLoaded` 信号、读档后**重绑各自 delegate 订阅**(CR-6);本系统不直接 `AddDynamic`,只发完成信号 | 各→21 |

> **双向一致性登记(Phase 5 propagate,producer)**:本系统是新增 GDD,以下 owner 档的 Dependencies/Interactions 的"存档(21)"行**已 fresh-grep 核实存在**对本系统的反向引用:board L164(`GetBoardId` 供存档)、player-turn L252/L420(序列化 PlayerState... 继承义务 AC-34)、economy L114/L332(序列化每玩家 Cash)、property L97/L114(序列化 owner map + bIsMortgaged)、dice L209(MVP 不序列化 Seed)、**building-upgrade.md L225**(`| 存档(21) | 软(横切) | 供 per-tile house_count([0,5])序列化读;读档写回 | 21→8 |`——随格 `house_count` 搬运;**OQ-Save-5 ① RESOLVED**:building 侧反向行已补全,fresh-grep 复核命中,carrying contract `house_count` per-tile [0,5]/键/range 对齐 building L57 核实正确)。**待补反向引用**(须在下次相关档 review 或 producer propagate 时登记,并入 OQ-Save-5):① 主菜单20(`DoesSaveGameExist`/触发存读)。② 呈现层 HUD16/VFX19/property card17(监听 `OnGameLoaded` 重绑)。

## Tuning Knobs

| 旋钮 | 作用 | 安全范围 | 影响 / 过低过高后果 |
|---|---|---|---|
| `SAVE_SLOT_NAME` | MVP 单存档槽的槽位名(`USaveGame` slot 字符串) | 非空字符串,默认 `"DiceTycoon_Save0"` | 工程标识,非平衡值;改名会使旧存档不可见(等价清档),仅开发期调整 |
| `CURRENT_SAVE_VERSION` | 当前存档格式版本号(F-2 输入) | int32 ≥1,默认 `1` | **每次改动 CR-3 字段集/序列化格式时 +1**;不递增会让旧存档被错误判兼容→读出错位字段(隐形损坏)。过度递增只是更频繁拒绝旧档(MVP 可接受) |
| `SAVE_MAGIC` | 存档头魔数(F-1 辨识) | 固定 uint32/FName 常量 | 工程常量,锁定;改动使所有旧存档 `magic_ok=false` 被拒(等价清档) |
| `bAllowExitSave` | 退出时是否写续局点(CR-1) | bool,默认 `true` | `false`=退出不存档(玩家须手动保存才续局);`true`=退出自动写续局点(更友好,支柱④)。**注**:仅在合法可存档点(CR-4)写,瞬态期退出不写 |
| `bRejectSaveDuringTransient` | 瞬态期是否拒绝保存(CR-4/EC-5) | bool,默认 `true`(**MVP 锁定 true**) | `true`=瞬态拒存(防损坏);`false`=**禁用**(会序列化瞬态债务态、读档损坏)。仅以旋钮形态登记防硬编码,MVP 不暴露为 false |

> **数值来源说明**:本系统**无任何 gameplay 平衡数值**。所有旋钮为工程/格式参数。`CURRENT_SAVE_VERSION` 是唯一需随开发演进的值,其递增纪律(改字段集即 +1)是版本兼容(F-2)正确性的前提。

## Acceptance Criteria

> **测试分层**:`[Logic]` = 纯 fixture、headless、确定性、PR merge gate(BLOCKING);`[Integration]` = 多系统往返(存→读→比对),可 headless 跑(无渲染依赖),BLOCKING;`[Advisory]` = 渲染时刻 / 平台磁盘 IO 真机 / code-review(非门控)。
> **核心纪律**:存档正确性的金标准 = **round-trip identity**(存档前快照 == 读档后快照,逐字段 identity-check)。这是可证伪的:任一字段漏存/错存,往返比对必 FAIL。无"调参直到通过"逃逸条款。

### A. 核心存读往返(CR-1 / CR-3,继承义务核心)

- **AC-1** [Integration·BLOCKING] **全状态 round-trip identity**:构造一局已推进的对局 fixture(多玩家、部分地产已购/已抵押、部分格已建房、当前轮到某玩家某阶段)→ 调 `SaveGameToSlot` → 调 `LoadGameFromSlot` → **逐字段 identity-check**:CR-3 表全部字段(每玩家 `Cash`、owner map 每格、`bIsMortgaged` 每格、`house_count` 每格、全 `PlayerState`〔含每玩家 **`CurrentTileIndex`(棋子所在格,owner=移动4)**——显式点名防实现者照清单写测试时漏断言棋子位置→读档回退默认 0 隐形损坏〕、`CurrentPlayerIndex`、`CurrentPhase`、`ConsecutiveDoubles`、`DOUBLES_JAIL_THRESHOLD`、完整 `FDiceRollResult`)读档后值 == 存档前值,无一例外。**这是防序列化遗漏的骨干断言。**
- **AC-2** [Logic·BLOCKING] **不存全量布局(CR-2)**:保存后,存档 payload 含 `BoardIdentifier`,**不含** 40 格的静态 `FBoardTileData`(价格/租金表/类型)——断言 payload 字段集与 CR-3 表一致、无棋盘静态布局字段。读档后棋盘数据经 DA 加载而非 payload(以"删 DA payload 不影响布局"反证)。
- **AC-3** [Logic·BLOCKING] **`Cash` 经容器写回、不触发金钱事件**:读档重建 `Cash` 时,断言 `OnCashChanged` **触发次数 == 0**(spy 计数)——证明读档是状态写回、非走 `Credit`/`Debit` 业务路径(否则读档会误播金币 juice / 重复触发下游)。

### B. 继承义务①:精确回合阶段 + ConsecutiveDoubles + 锁定阈值(来源 player-turn OQ-T-2 / AC-34)

- **AC-4** [Integration·BLOCKING] **精确阶段还原**:存档于 `PostRollAction` 阶段(非 `TurnStart`)→ 读档 → 断言 `CurrentPhase == PostRollAction`(经 `switch(CurrentPhase)` 进入该阶段,非回退 TurnStart)。覆盖 player-turn Edge L390"还原精确阶段"。回链 player-turn AC-34 / OQ-T-2。
- **AC-5** [Logic·BLOCKING] **ConsecutiveDoubles 往返**:fixture 玩家本回合链已掷出 2 次双点(`ConsecutiveDoubles==2`)→ 存读 → 断言读档后 `ConsecutiveDoubles==2`(非归零)。**反证**:若漏存则读出 0 → 读档后第3次双点不入狱(隐形规则损坏)。回链 player-turn AC-8/AC-9。
- **AC-6** [Logic·BLOCKING] **锁定阈值往返**:存读后断言 `DOUBLES_JAIL_THRESHOLD` 读档值 == 存档值(对齐 player-turn Edge Cases 序列化字段列表 / AC-34)。

### C. 继承义务②:完整 FDiceRollResult(含 Die1/Die2)(来源 dice OQ-T-Dice-5 / B1)

- **AC-7** [Logic·BLOCKING] **完整骰结果往返**:fixture 当前回合骰结果 `Die1=2,Die2=2,Total=4,bIsDouble=true` → 存读 → 断言读档后 `Die1==2 ∧ Die2==2 ∧ Total==4 ∧ bIsDouble==true`(四字段全等)。**反证**:若只存 `(Total=4,bIsDouble=true)` 则 Die1/Die2 丢失 → 读档后无法判定是 1+3 还是 2+2、VFX 无法重现骰面(dice L61/L232)。回链 dice OQ-T-Dice-5。
- **AC-8** [Logic·BLOCKING] **payload 不变式保持**:读档后 `FDiceRollResult` 满足 `Total==Die1+Die2 ∧ bIsDouble==(Die1==Die2)`(dice L189 构造不变式,序列化/重建须保持)。

### D. 继承义务③:读档后重绑 delegate 监听器(来源 player-turn AC-34 末句 / CR-6)

- **AC-9** [Integration·BLOCKING] **读档后事件可达**:读档完成后,触发一次会改现金的操作(如付租)→ 断言此前订阅 `OnCashChanged` 的监听器**收到回调**(spy 命中 ≥1)——证明读档后订阅关系已重绑、未失效。**反证**:若不重绑,读档后监听器收不到事件(HUD 静止)。回链 CR-6 / player-turn AC-34"含读档后重绑该阶段 delegate 监听器"。
- **AC-10** [Integration·BLOCKING] **阶段 delegate 重绑**:存档于 `PostRollAction` → 读档 → 断言该阶段事件触发时绑定的 handler 被调用(证明枚举字段恢复值之外、`AddDynamic` 也已重做)。
- **AC-11** [Logic·BLOCKING] **`OnGameLoaded` 完成信号**:`LoadGameFromSlot` 成功完成后广播 `OnGameLoaded` **恰一次**(spy 计数 ==1);失败路径(EC-1/2/3/4)**不广播** `OnGameLoaded`(==0)。

### E. 继承义务④:经济每玩家 Cash(来源 economy OQ-T-Econ-4)

- **AC-12** [Logic·BLOCKING] **每玩家 Cash 往返**:fixture 4 玩家 `Cash=[1500, 320, 0, 880]` → 存读 → 断言读档后每玩家 `Cash` 逐一精确相等(per-player identity)。回链 economy L114/L332 / OQ-T-Econ-4。

### F. 继承义务⑤:Raising Funds 瞬态不中途存档(来源 economy OQ-Econ-4 / CR-4)

- **AC-13** [Logic·BLOCKING] **瞬态期拒绝保存**:把对局置于 Raising Funds 子相(回合2 报告"非可存档点")→ 调 `SaveGameToSlot` → 断言返回 `SaveRejected_TransientState` ∧ `SLOT_DEFAULT` 内容**未改变**(旧存档不被覆盖)。回链 economy L92 / CR-4 / OQ-Econ-4。
- **AC-14** [Logic·BLOCKING] **可存档点放行**:回合2 报告"在合法可存档点"(`TurnStart`/`PostRollAction` 稳定态)→ `SaveGameToSlot` 成功(返回 success、写入存档)。与 AC-13 对照,证明闸门由可存档点判据驱动、非恒拒。
- **AC-15** [Logic·BLOCKING] **开局定序进行中拒绝保存**:置于 `TurnOrderInitRank` 阶段 → 保存被拒(`SaveRejected_TransientState`)。回链 player-turn Edge L390。

### G. 完整性校验(F-1~F-4)

- **AC-16** [Logic·BLOCKING] **版本不兼容拒绝(EC-1/F-2)**:构造 `save_version=1`、置 `CURRENT_SAVE_VERSION=2` → 读档 → 断言 `load_valid==false` ∧ 读档被拒 ∧ **当前状态不被破坏**(读档前的对局/主菜单状态不变) ∧ `OnGameLoaded` 不广播。
- **AC-17** [Logic·BLOCKING] **字段缺失拒绝(EC-2/F-4)**:构造一个 manifest 声明含 `ConsecutiveDoubles` 但 payload 缺该字段的存档 → 读档 → 断言 `field_manifest_complete==false` ∧ 整档被拒(**不部分加载、不填默认值**)。
- **AC-18** [Logic·BLOCKING] **校验和不匹配拒绝(EC-4/F-3)**:对一份有效存档篡改 1 字节 payload(不改 `stored_checksum`)→ 读档 → 断言 `checksum_ok==false` ∧ 读档被拒。
- **AC-19** [Logic·BLOCKING] **棋盘 DA 缺失拒绝(EC-3)**:存档 `BoardIdentifier` 指向不存在的 DA → 读档 → 断言读档被拒(**不回退默认棋盘**)。
- **AC-20** [Logic·BLOCKING] **空槽继续禁用(EC-6)**:无 `SLOT_DEFAULT` 时 `DoesSaveGameExist(SLOT_DEFAULT)==false`;读空槽不崩、不创建空对局。

### H. 写盘原子性 / 鲁棒性(EC-4/EC-9)

- **AC-21** [Logic·BLOCKING] **写盘原子性(临时文件→替换)**:模拟"写盘中途中断"(写临时文件后未完成替换)→ 断言 `SLOT_DEFAULT` 仍是**上一次完整有效存档**(读档 round-trip 仍通过 AC-1),损坏只在临时文件。验证 EC-4 写盘原子性契约。
- **AC-22** [Logic·BLOCKING] **写盘失败不破坏旧档(EC-9)**:模拟 `SaveGameToSlot` 失败(磁盘满/权限)→ 断言旧 `SLOT_DEFAULT` 内容不变、返回失败码。
- **AC-23** [Logic·BLOCKING] **读档骰子种子不重置回开局(EC-8)**:记录开局 `InitialSeed` → 推进若干掷骰 → 存读 → 断言读档后 RNG 流 `GetInitialSeed() != 开局 InitialSeed`(重设为新非确定值,非续回开局种子)。回链 dice L190 / OQ-2。**注**:此断言验"不等于开局种子"是确定性单点(开局种子已知),非统计断言。

### I. 派生量重算(CR-5 / property L97)

- **AC-24** [Logic·BLOCKING] **派生量读档后重算**:fixture 某玩家垄断红组(owner map 三格归他)→ 存读 → 读档**不存** `is_monopoly`,断言读档后查询 `is_monopoly(redGroup, player)==true`(由6从 owner map 重算)。证明派生量未被错误持久化、读档后由 owner 重算正确。

### J. 接口稳定性(编译/反射)

- **AC-25** [Advisory·code-review] `USaveGame` 子类全部序列化字段标 `UPROPERTY(SaveGame)`;新增 CR-3 字段时同步 `CURRENT_SAVE_VERSION +1`(版本递增纪律,code-review 核对)。
- **AC-26** [Advisory] 真机磁盘 IO 冒烟:目标平台(Steam/PC)实际写读 `SLOT_DEFAULT` 文件成功(headless 不覆盖真实文件系统差异,归手动冒烟)。

> **[Logic] gate(BLOCKING PR)**:AC-2/3/5/6/7/8/11/12/13/14/15/16/17/18/19/20/21/22/23/24(20 条)。
> **[Integration] gate(BLOCKING)**:AC-1/4/9/10(4 条,多系统往返、headless 可跑)。
> **[Advisory](非门控)**:AC-25/26(2 条)。
> **合计 26 AC**(20 Logic + 4 Integration + 2 Advisory)。

### 继承义务回链汇总(对照 systems-index 存档行 line~84)

| 继承义务 | 来源 | 落地 AC |
|---|---|---|
| ① 序列化精确回合阶段 + ConsecutiveDoubles + 锁定阈值 | player-turn OQ-T-2 / AC-34 | AC-4 / AC-5 / AC-6 |
| ② 完整 `FDiceRollResult`(含 Die1/Die2,不可只存 Total/bIsDouble) | dice OQ-T-Dice-5 / B1 | AC-7 / AC-8 |
| ③ 读档后重绑 delegate 监听器 | player-turn AC-34 末句 / CR-6 | AC-9 / AC-10 / AC-11 |
| ④ economy 序列化每玩家 Cash | economy OQ-T-Econ-4 | AC-12 |
| ⑤ Raising Funds 瞬态不中途存档(与回合2 协同) | economy OQ-Econ-4 / player-turn | AC-13 / AC-14 / AC-15 |

## Open Questions

| # | 问题 | 负责方 / 解决于 |
|---|---|---|
| **OQ-Save-1 ⚠ ADR** | `USaveGame` 宿主与写盘形态:`UGameplayStatics::SaveGameToSlot`(同步,简单)vs `AsyncSaveGameToSlot`(异步,不卡帧)的选择;临时文件→原子替换(EC-4/AC-21)的具体 UE 实现(平台文件 API)。**与回合2 OQ-1 协调**:回合状态机须用 `ETurnPhase` 枚举字段 + delegate(可序列化、`switch` 重入),禁 Latent Action(player-turn L391)——本档读档重建依赖此 ADR 成立。 | 架构期 `/architecture-decision`(与 player-turn OQ-1 协调) |
| **OQ-Save-2** | 回合2"当前是否在合法可存档点"的**查询接口名/形态**未定(CR-4):本系统须查询回合2 的可存档标志(如 `bIsSafeToSave` / `CanSaveNow()`),具体接口名待回合2 提供(R-1 fresh-grep:player-turn 全文 `bIsSafeToSave`/`CanSaveNow` = 0 命中,回合2 尚未提供此接口)。**MVP 默认路径 = 锁权威路径**:AC-13/14/15 全部以"回合2 报告可存档/非可存档"为 GIVEN(正确对齐权威判据,本系统不自裁阶段语义,守 enable-not-own / CR-4 裁决归属)。**自判 fallback**(本系统据 `CurrentPhase ∈ {TurnStart, PostRollAction 稳定态}` 且非 Raising Funds/定序/破产编排)**为临时降级、无 AC 守护、回合2 接口到位后移除**——勿默认实现,否则本系统将复制它声明不拥有的回合2 阶段语义(enable-not-own 轻度越权,与 CR-4 裁决归属矛盾)。裁决方向(开工前定回合2 接口删 fallback / 或给 fallback 补一条 AC 并标临时降级)留架构期 ADR 收敛。 | 回合2 接口扩展 / 架构期 ADR(开工前定);MVP 默认锁权威路径 |
| **OQ-Save-3** | 多存档槽 / 命名存档 / 云存档 / 自动存档(autosave):MVP 锁单槽手动(CR-1)。Alpha/Full Vision 若开放,须扩 `SAVE_SLOT_NAME` 为槽位列表 + 槽位元数据(时间戳/缩略图/局面摘要)+ autosave 触发点(回合末?)。autosave 须复用 CR-4 可存档点判据。 | Alpha/Full Vision;MVP 不做 |
| **OQ-Save-4** | 存档版本迁移:MVP 严格相等、不迁移(F-2,EC-1 拒绝)。格式稳定后(Beta?)若需向后兼容旧存档,须设计 `save_version < CURRENT` 时的字段映射/升级路径。涉及枚举 append-only 纪律(player-turn AC-43)。 | 格式稳定后(Beta);MVP 不做 |
| **OQ-Save-5** | **双向一致性 propagate 债(producer)**:本系统新增的反向引用——主菜单(20)的"继续游戏"查 `DoesSaveGameExist` + 触发存读;呈现层(HUD16/VFX19/property card17)监听 `OnGameLoaded` 重绑 delegate(CR-6)——须在 #20/#16/#17/#19 撰写或下次 review 时登记到其 Dependencies。上游 owner 档(1/2/5/6/8/3)的"存档(21)"行撰写前已 grep 核实存在,无需反向补。 | producer `/propagate-design-change`;不阻本档 |
| **OQ-Save-6** | Full Vision 联网下的存档语义:服务器权威对局如何存档(服务器存?)、断线重连是否复用存档机制、Seed 续算(dice OQ-2/OQ-3)。MVP 单机不触发。 | Full Vision 联网(25);MVP 不展开 |
