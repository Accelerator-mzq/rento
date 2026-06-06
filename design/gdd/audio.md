# 音频 SFX / BGM（Audio — Sound Effects & Background Music）

> **System #**: 22（Audio，MVP）
> **Status**: Approved（R-1 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS〔首评即收敛终结〕，sound-designer 撰写；遗留 OQ-Audio-1/2 + Asset-Spec Flag 不阻开工，见 reviews/audio-review-log.md）
> **Author**: msc + sound-designer
> **Last Updated**: 2026-06-06
> **Implements**: MDA Sensation（priority-4，听觉获得感，与 VFX19 共同承载）+ ② 社交博弈（收租/破产可闻戏剧性）+ ④ 易上手（音频线索辅助状态可辨）
> **Flags**: 📌 **Asset-Spec Flag**（art-bible §8.1 备注音效 WAV 48kHz/16bit，本系统正式拥有；art-bible approved 后 `/asset-spec system:audio` 生成逐资产 spec）。非 UI 屏，**无 UX-spec flag**（音量设置 UI 复用 #23 设置屏 / 主菜单，本系统只供混音总线参数语义）。

---

## Overview

音频系统（#22）是《骰子大亨》呈现层的 **声音唯一 owner**——它把骰子(3)、移动(4)、经济(5)、地产(6)、建房(8)、破产/胜负经回合(2)、事件格(7) 持续广播的 gameplay 事件，翻译成玩家**听得到**的反馈：掷骰的翻滚撞击、金币哗啦入账、房子咔哒落地、抵押契据翻动、抽牌翻面、入狱铁门、破产沉闷、胜利号角，以及一条贯穿全局的轻快 BGM。作为**呈现层纯叶子**，#22 只**订阅** owner 系统的 multicast delegate、只负责**触发声音播放与混音**；从不写游戏状态、不裁决结算、不阻塞框架推进（owner 先算定结果，#22 收到事件即播，落后只丢声音不回灌框架）。它与 VFX(19) 的边界由 **enable-not-own** 协议厘清：**#22 拥有声音资产、混音总线、事件→声音触发映射；#19 拥有视觉 juice**。同一 gameplay 事件（如 `OnRentPaid`）由两 owner **各自独立订阅、各播各的**（VFX 播金币飞溅画面，音频播金币音效），**互不双触发、互不依赖**——这是声画分离的标准模式，不构成歧义。MVP 范围聚焦核心循环 SFX + 1–2 条 BGM + 基础混音总线（SFX/BGM/Master 三路音量）；动态音乐分层、自适应配乐、ducking、空间化 3D 音源等高级行为一律降 Alpha（OQ）。技术上 #22 是一组 UE5.7 Sound Cue / MetaSound 资产 + Sound Class / Submix 混音图，随机变体走 #22 自建独立非权威 `FRandomStream`（音频随机严禁复用骰子3 权威流，防联网重放污染，同 ai CR-5④ / VFX19 CR-9）。

---

## Player Fantasy

**核心感受："这一局不只看得见，还听得见——骰子在桌上滚出脆响，金币哗啦砸进账户，房子咔哒落地，对手破产时那声沉闷的'唉'；轻快的背景乐让整桌人放松地坐下来又坑又乐。"**

#22 服务 **MDA Sensation（感官愉悦，priority-4）**的**听觉**维度——它不加任何规则，却和 VFX19 一起决定核心循环"摸起来爽不爽"。听觉反馈的独特价值：

1. **掷骰的命运之声**（支柱③运气×策略）：骰子翻滚撞击的脆响 + 定格的"嗒"，把"再走一次"的期待变成可闻的张力；掷出双点有上扬的强化提示音，连掷三次双点入狱则切换为低沉警告音——庆祝与危险，闭着眼也分得清。
2. **收租 / 发薪的金币声**（支柱②社交博弈）："我的地替我赚钱"的满足靠金币哗啦声传递；付租方那一声"叮——"则是被坑的社交快感的听觉载体。
3. **建房 / 买地的获得感**（支柱①桌游忠实）：房子弹性"咔哒"落地、四栋合并成酒店的"轰"一声升起、买地契据"啪"地拍定——桌面实体感被声音放大。
4. **破产 / 胜利的情绪收束**：破产的沉闷下沉音 + BGM 短暂压低；胜者的欢快号角 + 全场欢呼——一局的情绪曲线在声音上两极收束。
5. **BGM 的桌面氛围**（支柱④易上手 + Submission priority-2）：一条轻快、低压力、不抢戏的背景乐，复刻"围坐一桌随时开一局"的休闲感，是放松基调的听觉锚。

*Design test（art-bible 支撑原则 2 听觉延伸）*：在"安静朴素"与"有声反馈"之间，核心循环动作永远选有声；混音预算/并发紧张需裁剪时，**优先保留掷骰 / 金币 / 建房 / 破产 / 胜利 SFX**，先裁背景环境层与非交互装饰音。但音频**永不阻塞**游戏推进——它"使能爽感"而非"拥有流程"，落后于回合时只丢过期声音、不回灌框架（同 HUD/VFX 非阻塞约定）。

> 注：`audio-director`/`creative-director` 未咨询（lean 模式）。production 前须人工复核情感措辞、sonic 基调与支柱对齐，并由 audio-director 确认混音电平基准。

---

## Detailed Rules

> 约定：#22 一律**呈现侧纯叶子**——订阅 owner 事件、播放声音；**绝不**写游戏状态、裁决结算或阻塞框架。owner 事件签名见 Dependencies 表（与 VFX19 / 各 owner GDD 逐字对齐）。

### CR-1 事件驱动播放（纯订阅）

#22 启动时订阅各 owner 的 multicast delegate；收到事件即触发对应 Sound Cue 播放，**单向消费**——不回调 owner、不写状态、不返回值影响结算。owner 事件是唯一触发源（#22 不轮询、不自驱）。**与 VFX19 关系**：同一事件 VFX19 与 #22 **各自独立订阅**（声画分离），#22 不经 VFX 转发、VFX 不经 #22 转发，两者互不持引用、互不依赖（CR-10 enable-not-own）。

### CR-2 声音→事件触发映射（MVP 核心循环）

下表是 MVP 必备的事件→声音映射。每条声音是一个 Sound Cue（含随机变体，CR-7）。**音频不解释 gameplay 语义**，只据 payload 字段做有限分支（如 `EChangeReason` 正/负向、`bIsDouble` 庆祝/常规）：

| 事件（owner） | 触发声音 | 总线 | 分支依据 |
|---|---|---|---|
| `OnDiceRolled`（骰子3） | 骰子翻滚撞击 + 定格"嗒" | SFX | `bIsDouble==true` → 叠加上扬强化提示音；定序语境抑制强化（CR-3） |
| `OnPawnMoveStarted` / `OnPawnLanded`（移动4） | 棋子逐格"哒哒"hop + 落地"咚" | SFX | hop 步进声节流（CR-6 同帧/连步泛滥防护） |
| `OnRentPaid`（经济5） | 金币哗啦（收租方视角）+ 付租"叮" | SFX | 单一事件单次播放，不按金额缩放音量（MVP） |
| `OnCashChanged`（经济5） | 现金数字跳动"嘀嗒" | SFX | 据 `EChangeReason` 选正向（金币系）/ 负向（扣钱系）音色（CR-4） |
| `OnOwnershipChanged`（地产6） | 买地契据"啪"拍定 | SFX | `Old==None && New==PlayerId` → 买地音；破产移交（`P→P`/`P→None`）→ 中性翻页音（CR-5） |
| `OnMortgageChanged`（地产6） | 抵押契据翻动"沙" | SFX | `bIsMortgaged==true` 抵押 / `false` 赎回，可同音不同 pitch |
| `OnBuildingChanged`（建房8） | 房子"咔哒"落地 / 酒店合并"轰" | SFX | 据本地 `house_count_cache` delta 派生 Build/Sell（CR-5，镜像 VFX19 CR-6） |
| `OnCardDrawn`（事件格7） | 抽牌翻面"唰" | SFX | — |
| `OnTileEventResolved`（事件格7） | 事件结算提示（中性"叮"） | SFX | 据 `ECardEffectType` 可选正/负音色（MVP 可统一中性） |
| `OnPlayerJailed`（事件格7） | 入狱铁门"哐当" | SFX | — |
| `OnPlayerReleased`（事件格7） | 出狱"叮——"释放音 | SFX | — |
| `OnBankruptcyDeclared`（经济5 广播 / 破产9 拥裁决） | 破产沉闷下沉音 + BGM 短压（CR-8） | SFX + BGM duck（**MVP 简化：仅 BGM 音量阶跃压低，非动态 ducking**） | — |
| `OnGameWon`（回合2） | 胜利号角 + 欢呼 + BGM 切胜利段（MVP：BGM 淡出，胜利 stinger 独立） | SFX + BGM | — |
| `OnTurnStarted`（回合2） | 轻微"轮到你"提示音（当前为人类玩家时；AI 回合静音或低音量，CR-9） | SFX | 据当前玩家是否人类分支 |
| UI 点击（菜单/按钮，#16/#20/#23 UI 层触发） | UI 点击"嗒" | UI（归 SFX 总线） | 按下/松开/禁用三态可同 Cue 不同变体 |

> **MVP 范围声明**：本表是 MVP 全集。**不在 MVP**（降 Alpha OQ）：动态音乐分层（按局势紧张度切换 BGM 层）、自适应配乐、真正的 sidechain ducking（语音/关键 SFX 压低 BGM 的实时包络）、3D 空间化音源定位、混响分区、命运之轮/俄罗斯轮盘/拍卖/策略卡专属音乐（特色机制本身 Alpha）。

### CR-3 掷骰 juice 音频（继承义务 dice OQ-T-Dice-4，听觉腿）

订阅骰子3 `OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})` 播翻滚→定格音。**双点强化提示音差异化**：与 VFX19 CR-3 同源——从 player-turn `OnPhaseChanged(FPhaseChangedInfo{...,ConsecutiveDoubles})` payload 读取 `ConsecutiveDoubles`（**非裸 `bIsDouble`、禁直接轮询 player-turn 属性**，时序不保证），1~2 连=上扬庆祝音；第 3 连（入狱）=低沉警告音。**定序阶段抑制**：player-turn 告知"当前=开局定序"语境（经 `OnPhaseChanged` 阶段枚举判定）时，#22 抑制双点强化音（定序掷骰不进双点链，dice OQ-T-Dice-1）。`OnTurnStarted` 时 #22 重置缓存 `ConsecutiveDoubles=0`（防上家旧值污染新玩家首掷，镜像 VFX19 R-3）。若 `OnDiceRolled` 与 `OnPhaseChanged` 时序未对齐或值缺失 → EC-3 降级为基础掷骰音（无强化/警告分支）。

### CR-4 经济现金音频（继承义务 economy Visual 听觉腿，Sensation priority-4）

订阅经济5 `OnRentPaid(Payer,Payee,Amount,TileIndex)` 播金币哗啦 + 付租"叮"；`OnCashChanged(Player,Old,New,EChangeReason)` 据 reason 选音色；`OnBankruptcyDeclared(Debtor,Creditor)` 播破产音。**`EChangeReason` 音色分类表（与 VFX19 CR-5 同源、镜像枚举）**：**正向音色**（`Salary` 发薪 / `RentReceived` 收租 / `PassGo` 过 GO）= 金币系上扬；**负向音色**（`RentPaid` 付租 / `Tax` 税 / `Purchase` 买地 / `JailFine` 保释 / `BuildCost` 建房支出）= 扣钱系下沉"叮"；**未知 reason** → **中性提示音，不臆断正负**（EC-5，防罚款误播奖励音）。经济5 新增 reason 时须回补此表（否则走 EC-5 中性兜底，不崩溃）。**MVP 不按金额缩放**（不做 `Amount→pitch/volume` 映射，降 Alpha OQ-Audio-3）。

### CR-5 建房 / 地产音频（enable-not-own，承接 #8 / #6）

订阅建房8 `OnBuildingChanged(tile, newCount)`（owner 2 字段）——**方向由 #22 本地从 `newCount` 单调性派生**（镜像 VFX19 CR-6 / 地产6 owner-delta 先例）：#22 自持 per-tile `house_count_cache[tile]`（呈现层 memo，**非游戏状态**，同自建 `FRandomStream` 类），`newCount > cache` → **Build 音**（咔哒落地；`newCount==5` 酒店合并"轰"），`newCount < cache` → **Sell/拆除 音**（较轻翻页/收回音，**无庆祝音**），`newCount == cache` → no-op。**方向只依赖 delta 符号、步长无关**（含破产清零 `h→0` 大跳仍 `<cache`=正确派生 Sell）。订阅地产6 `OnOwnershipChanged(TileIndex,Old,New)`：`None→PlayerId`=买地音；`P→P`/`P→None`（破产清算移交）=中性翻页音，并 **在每个 `OnOwnershipChanged` 上 re-seed `cache[tile]=GetHouseCount(tile)`**（镜像 VFX19 R-3 Finding A：封银行破产 `LiquidateAllBuildings` 逐格清零可能不广播 `OnBuildingChanged` 致 cache 漂移→该格回银行重买后首次建房误判 Sell 音）。`OnMortgageChanged(TileIndex,bIsMortgaged)` 播抵押翻动音。**缓存种子化（seed-before-first-event）**：#22 订阅建房8 时立即用 `GetHouseCount(tile)` 种子化 `house_count_cache`，首事件前缓存已映射真值。缓存 miss → EC-6 非方向中性兜底音。这些声音资产归 #22；#17/#19 不持音频引用。

### CR-6 同帧多事件 / 连步 SFX 节流（防泛滥）

同一帧/极短窗口内多个事件触发同类 SFX（如 hop 逐格连续 `OnPawnMoveStarted`、级联破产 N 格 `OnOwnershipChanged`），#22 须**节流防爆音**：① 每个 Sound Cue 有**最小重触发间隔** `T_sfx_min_retrigger`（同 Cue 在窗口内重触发则合并/丢弃后续实例，不叠 N 次同音）；② 全局**并发声音上限** `N_voice_max`（活跃 voice 数达上限时，新声音按优先级（CR-8）抢占最低优先级 voice，关键 beat 破产/胜利不被抢占）；③ hop 步进声以**节奏量化**播放（每 `T_hop_step` 一声，而非每事件一声，与 VFX19 hop 节奏对齐）。详见 F-3。

### CR-7 随机变体 RNG 隔离（继承义务 ai CR-5④ / VFX19 CR-9）

#22 一切声音随机（变体选择、pitch 抖动 ±、起播微延迟）**必走 #22 自建独立非权威 `FRandomStream`**，**严禁复用骰子3 权威流**（防联网重放污染）。变体池：每个核心 SFX 备 2~3 个变体样本，#22 从自建流随机选，避免重复听感疲劳。

### CR-8 声音优先级与抢占

并发达 `N_voice_max` 时按优先级表抢占（F-2）。优先级（高→低）：**Critical**（破产 `OnBankruptcyDeclared` / 胜利 `OnGameWon`，绝不被抢占、绝不丢弃，至少播完关键 beat）> **Core 循环**（掷骰 / 金币 / 建房 / 入出狱）> **次要**（UI 点击 / hop 步进 / 数字跳动）> **装饰**（环境/背景层）。低优先级被高优先级抢占时**静默丢弃当前实例**（不淡出排队，MVP 简化），不回灌框架。

### CR-9 非阻塞 + 追赶 / 跳过（同 HUD F-4 / VFX19 CR-8）

声音**永不阻塞**框架推进。若 #22 落后于回合（下一回合已开始 / 声音队列积压），**丢弃过期的中间声音、不补播**（如 hop 已结束则不再播剩余步进声），**不回灌框架、不要求 owner 等待**。关键 beat（破产/胜利）即使追赶也至少触发一次（CR-8 Critical 优先级）。**AI 回合音量**：AI 玩家回合的 `OnTurnStarted` 提示音静音或降至低音量（避免 AI 快速连续行动时提示音轰炸），核心结算 SFX（金币/建房）正常播放（保社交可闻性，支柱②——人类要听见 AI 在收钱建房）。

### CR-10 enable-not-own 边界（对 VFX19 / HUD16 / #17）

#22 拥 **声音资产 + Sound Cue/MetaSound + Sound Class/Submix 混音图 + 事件→声音映射**；VFX19 拥 **视觉 juice 资产 + Niagara/UMG 动画**。两者对**同一 gameplay 事件各自独立订阅 owner**（声画分离），#22 不经 VFX/HUD/#17 转发、不持其引用，VFX/HUD/#17 不持 #22 引用、不触发 #22 播放（UI 点击音例外：UI 层调 #22 暴露的 `PlayUISound(SoundId)` 接口——这是 UI 主动请求音，非状态事件订阅，见 CR-11）。

### CR-11 UI 点击音接口（#22 对 UI 层的唯一流出 API）

UI 屏（主菜单 #20 / HUD #16 操作栏 / 设置 #23）的按钮按下需即时点击音。**机制**：#22 暴露 `PlayUISound(EUISoundId id)` 蓝图可调函数，UI widget 在 `OnClicked` 中调用——这是 #22 唯一的**被调入**接口（非订阅状态事件），不构成 #22 写游戏状态（它只播音）。`EUISoundId` 枚举：`Click`/`Confirm`/`Cancel`/`Disabled`/`Hover`（Hover 音 MVP 可不实现，避 hover-only 依赖，技术偏好键鼠+手柄适配）。**节流**同 CR-6（连点防爆音）。

### CR-12 混音总线结构（MVP 三路）

#22 维护三级 Sound Class / Submix 层级：**Master**（总音量）← **SFX**（所有 gameplay + UI 音效）+ **BGM**（背景音乐）。每路独立音量参数 [0,1]，由设置屏 #23 / 主菜单读写（#22 供参数语义、不拥设置 UI）。MVP **不**做更细分组（不拆 UI/世界 SFX 子总线、不做混响 send、不做 ducking 自动化）。BGM 在 `OnBankruptcyDeclared` 短暂阶跃压低、`OnGameWon` 淡出切胜利 stinger（CR-2 表），均为简单音量包络，非动态 ducking（降 Alpha OQ-Audio-1）。

### CR-13 BGM 播放规则（MVP 1–2 条）

整局背景播 1 条主 BGM（循环无缝 loop point）；可选第 2 条用于主菜单/大厅（与对局区分氛围，art-bible 状态 A vs B）。BGM 经 BGM 总线，跟随 BGM 音量参数。**MVP 不做**：按局势强度切层、按回合阶段切段、战斗/紧张自适应（全降 Alpha OQ-Audio-1）。状态切换（主菜单→对局→胜利）用简单 crossfade（`T_bgm_crossfade`），非分层。

---

## Formulas

> 本系统**无平衡数值公式**——音量电平、并发上限是混音/性能参数（Tuning），不是 gameplay 平衡。本节收录**确定性混音/优先级/节流逻辑**。所有声音随机走 #22 自建 `FRandomStream`（CR-7，非权威流）。

### F-1 总线增益合成 `effective_gain` [Logic]

每个声音实例的最终增益 = 各级总线音量连乘（线性增益域）：

```
effective_gain(sound) = V_master × V_bus(sound) × G_cue(sound)
```

- **`V_master`**：Master 总线音量，范围 [0.0, 1.0]，默认 0.8（玩家可调，设置屏）
- **`V_bus(sound)`**：该声音所属总线音量；`V_sfx`（SFX 总线）或 `V_bgm`（BGM 总线），各 [0.0, 1.0]，默认 `V_sfx=1.0`、`V_bgm=0.6`（BGM 默认低于 SFX，不抢戏，art-bible 状态 A「不喧宾夺主」）
- **`G_cue(sound)`**：该 Cue 的设计基准增益（资产内固定，[0.0, 1.0]，asset-spec 定，非玩家可调），用于跨 Cue 响度平衡
- **`effective_gain`** ∈ [0.0, 1.0]

**示例**：金币收租音，`V_master=0.8`、`V_sfx=1.0`、`G_cue=0.9` → `effective_gain = 0.8×1.0×0.9 = 0.72`。玩家把 SFX 调到 0.5 → `0.8×0.5×0.9 = 0.36`。
**约束**：`effective_gain` 由乘法保证单调——任一总线降低则该路所有声音降低，无溢出（各因子 ≤1.0，积 ≤1.0）。

### F-2 声音优先级抢占判定 `should_preempt` [Logic]

当活跃 voice 数 `N_active == N_voice_max` 且新声音 `s_new` 到达：

```
should_preempt(s_new) =
    ∃ s_active ∈ ActiveVoices : priority(s_active) < priority(s_new)
victim = argmin_{s_active} priority(s_active)   // 同优先级取最早起播者
```

- **`priority(s)`** ∈ {Critical=3, Core=2, Secondary=1, Decorative=0}（CR-8，枚举 `EAudioPriority : uint8`）
- 若 `should_preempt==true` → 抢占 `victim`（静默停止该 voice）、播 `s_new`
- 若 `should_preempt==false`（无更低优先级可抢，即 `s_new` 是当前最低）→ **丢弃 `s_new`**（不播，不排队，CR-8 MVP 简化）
- **Critical 不变式**：`priority(s_new)==Critical` 时，由于 Critical=3 是最高，只要存在非 Critical voice 即可抢占；**两条 Critical 同时争最后 1 个 voice slot（极端）→ 均播放，临时超 `N_voice_max` 一个 slot（破产+胜利同帧的关键 beat 绝不丢，EC-7）**

- **`N_voice_max`**：全局并发声音上限，范围 [16, 64]，默认 32（Tuning，硬件相关）

**示例**：`N_voice_max=32` 已满，全为 Secondary（hop/UI）；破产音（Critical=3）到达 → 抢占最早的 hop voice 播破产音。

### F-3 同 Cue 节流 `allow_retrigger` [Logic]

同一 Sound Cue `c` 在短窗口内重复触发时：

```
allow_retrigger(c, t_now) = (t_now − t_last_play(c)) ≥ T_sfx_min_retrigger(c)
```

- **`t_now`**：当前触发时刻（秒，游戏时钟）
- **`t_last_play(c)`**：Cue `c` 上次起播时刻（#22 per-Cue 缓存）
- **`T_sfx_min_retrigger(c)`**：该 Cue 最小重触发间隔（秒），范围 [0.02, 0.30]，默认 0.06（约 60ms，防同帧叠 N 次同音爆响）；hop 步进音用更大值对齐 `T_hop_step`
- 若 `allow_retrigger==false` → **丢弃本次触发**（不播、不排队，合并到上一声的尾音），更新 `t_last_play` 不变
- 若 `true` → 播放并置 `t_last_play(c)=t_now`

**示例**：级联破产同帧 8 格 `OnOwnershipChanged` 同播翻页音，`T_sfx_min_retrigger=0.06`、帧间隔 0.016s → 仅首声播放，后 7 次落在 60ms 窗口内被丢弃（听感为单次而非 8 连爆）。

### F-4 BGM 状态切换增益 `bgm_target_gain` [Logic]

BGM 在游戏状态切换 / 破产压低时的目标增益（MVP 阶跃/线性 crossfade，非动态 ducking）：

```
bgm_target_gain(state) =
    state == Bankruptcy_active ? V_bgm × DUCK_FACTOR
  : state == Victory          ? 0.0    (淡出，胜利 stinger 独立播)
  : V_bgm                              (常态)
```

- **`DUCK_FACTOR`**：破产时 BGM 压低系数，范围 [0.2, 0.7]，默认 0.4（短暂压暗，突出破产 SFX，art-bible 状态 G「时间停滞」听觉）
- **`V_bgm`**：BGM 总线音量（F-1）
- crossfade 时长 `T_bgm_crossfade`，范围 [0.1, 2.0] 秒，默认 0.5；破产压低恢复用更短值（破产 beat 后约 `T_duck_release` 恢复常态）
- 输出 `bgm_target_gain` ∈ [0.0, `V_bgm`]

**示例**：`V_bgm=0.6`、`DUCK_FACTOR=0.4`、破产发生 → BGM 目标增益 `0.6×0.4=0.24`，0.5s crossfade 压低，破产 SFX 播完后恢复 0.6。

---

## Edge Cases

> 明确"发生什么"，非"优雅处理"。所有降级路径**不崩溃、不阻塞框架、不回灌**。

- **EC-1 锚点/资产解析失败**（Sound Cue 引用空 / 资产未加载）：**跳过该次声音**，记一条 Warning 日志，不崩溃、不阻塞。后续事件正常播放。
- **EC-2 同帧多事件触发同类 SFX**：经 F-3 节流——首声播放，窗口（`T_sfx_min_retrigger`）内后续同 Cue 触发被**丢弃**（合并听感），不叠 N 次爆音。不同类 SFX 各自独立计节流。
- **EC-3 `OnDiceRolled` 与 `OnPhaseChanged` 时序未对齐 / `ConsecutiveDoubles` 缺失**：掷骰音**降级为基础翻滚音**（无双点强化/警告分支），不阻塞、不报错。（镜像 VFX19 EC-9）
- **EC-4 并发声音超 `N_voice_max`**：经 F-2 抢占——低优先级 voice 被**静默停止**为新声让位；若新声是全局最低优先级则**丢弃新声**。Critical（破产/胜利）绝不被抢占（EC-7）。
- **EC-5 `OnCashChanged` 未知 `EChangeReason`**：播**中性提示音**（不臆断正负色），记 Warning。经济5 新增 reason 未回补 CR-4 分类表时走此兜底，不误播奖励/扣钱音。（镜像 VFX19 EC-17）
- **EC-6 `house_count_cache` miss**（种子化失败 / 首见新 tile）：播**非方向中性建房提示音**（不猜 Build/Sell），置 `cache[tile]=newCount`。后续该 tile 事件正常派生。（镜像 VFX19 EC-15）
- **EC-7 破产 + 胜利同帧**（最后一人破产触发胜利）：两条 Critical 声音**均播放**，临时超 `N_voice_max` 一个 slot（F-2 不变式）——关键情绪 beat 绝不因并发上限丢失。
- **EC-8 读档重建**（存档21）：#22 **先解绑再重绑**全部 owner delegate（防双绑致单事件双播 footgun，[[juice-leaf-event-double-subscribe]]），从快照 re-seed `house_count_cache`、重置 `ConsecutiveDoubles=0`、**不回放历史声音**（不补播读档前发生的掷骰/金币音）。BGM 据当前状态重新起播。
- **EC-9 BGM 资产缺失 / 加载失败**：**静默继续**（无 BGM），SFX 不受影响，记 Warning。BGM 是氛围非功能性，缺失不阻塞游戏。
- **EC-10 `reduce_motion` / 静音（mute）状态**：`V_master==0`（或玩家静音）时所有声音 `effective_gain==0`（F-1），voice 仍按逻辑分配（不省抢占判定）但实际无声输出。设置静音不影响 gameplay 事件订阅与节流逻辑。
- **EC-11 AI 快速连续回合**：AI 回合 `OnTurnStarted` 提示音静音/低音量（CR-9），但金币/建房等结算 SFX 正常播（经 F-3 节流防爆音）——保社交可闻性。
- **EC-12 hop 被 F-3 / 追赶跳过**：若 hop 步进声落在节流窗口或回合已推进（CR-9 过期），**不补播剩余步进声**，落地音（若未过期）仍触发；玩家听到的是压缩节奏而非每格一声，不影响 gameplay。

---

## Dependencies

> 方向约定：#22 是**呈现层叶子消费者**——`←` 订阅/读取 owner 事件；**唯一 `→`** 是 `PlayUISound` 被 UI 层调入（请求音，非写状态）。事件签名与各 owner GDD / VFX19 逐字对齐（双向一致性见末注）。

| 系统 | 数据流入 #22（#22 订阅/消费） | #22 流出 | owner |
|------|------------------------------|----------|-------|
| **骰子(3)** | `OnDiceRolled(FDiceRollResult{Die1,Die2,Total,bIsDouble})`——掷骰音；`bIsDouble` 触发强化音分支（CR-3） | 无 | #3 拥 RNG/掷骰结果 |
| **玩家回合(2)** | `OnPhaseChanged(FPhaseChangedInfo{...,ConsecutiveDoubles})`（双点强化差异化 + 定序抑制，**payload 取值非属性轮询**）+ `OnTurnStarted(FTurnStartedInfo{PlayerId,bIsAI})`（轮次提示音 + 重置 `ConsecutiveDoubles` 缓存；`bIsAI` 供 AI 回合静音/低音量分支 CR-9）+ `OnGameWon(WinnerId)`（胜利号角，CR-2/CR-8） | 无 | #2 拥流程/双点链/胜负广播 |
| **移动(4)** | `OnPawnMoveStarted(Player,FromIndex,ToIndex,Steps,PassedGo)`（hop 步进音）+ `OnPawnLanded(Player,TileIndex,EArrivalContext)`（落地音，CR-2/EC-12；`EArrivalContext` 即 `arrivalContext` 落地语境枚举） | 无 | #4 拥位移 |
| **经济(5)** | `OnRentPaid(Payer,Payee,Amount,TileIndex)`（金币音）+ `OnCashChanged(Player,Old,New,EChangeReason)`（据 reason 正/负音色，CR-4）+ `OnBankruptcyDeclared(Debtor,Creditor)`（破产音 + BGM duck） | 无 | #5 拥现金结算/事件 |
| **地产所有权(6)** | `OnOwnershipChanged(TileIndex,Old,New)`（买地/移交音 + 触发 cache re-seed=调建房8 `GetHouseCount`，CR-5）+ `OnMortgageChanged(TileIndex,bIsMortgaged)`（抵押翻动音） | 无 | #6 拥归属/抵押事实 |
| **建房升级(8)** | `OnBuildingChanged(tile, newCount)`（2 字段）+ `GetHouseCount(tile)` 种子化读——#22 本地从 `newCount` delta 派生 Build/Sell 音（CR-5） | 无 | #8 拥 house_count/建房 |
| **事件格(7)** | `OnCardDrawn(playerId, EEventDeck deck, cardId)`（抽牌音）+ `OnTileEventResolved(playerId, tileIndex, ECardEffectType effectType)`（结算提示）+ `OnPlayerJailed(playerId, EJailReason reason)`（入狱音）+ `OnPlayerReleased(playerId, EJailReleaseMethod releaseMethod)`（出狱音）——tile-events L109/L112 已列音频(22)为订阅方 | 无 | #7 拥事件结算/牌堆/入出狱规则 |
| **破产胜负(9)** | 破产音经经济5 `OnBankruptcyDeclared` 广播；胜负经回合2 `OnGameWon`（#22 不直接订阅 #9，经其 owner 广播路径，同 VFX19） | 无 | #9 拥破产裁决/清算编排 |
| **UI 层（主菜单20 / HUD16 / 设置23）** | — | **`PlayUISound(EUISoundId)`**（UI widget `OnClicked` 调入，CR-11，唯一被调接口） | UI 屏拥按钮，调 #22 播点击音 |
| **设置 & House Rules(23)（Alpha）** | 读写 `V_master`/`V_sfx`/`V_bgm` 总线参数（#22 供参数语义，#23 拥设置 UI；MVP 阶段音量调节可临时挂主菜单或 #23 占位） | 无（参数被 #23 读写） | #23 拥设置 UI；#22 拥混音参数 |

> **双向一致性核验**：
> ① **事件格(7)** L109/L112 已显式列「音频(22)」为 `OnCardDrawn`/`OnTileEventResolved`/`OnPlayerJailed`/`OnPlayerReleased` 订阅方——上游已声明，本 GDD 对齐，无反向缺口。
> ② **骰子3/回合2/移动4/经济5/地产6/建房8** 的事件已由 **VFX19（Approved）+ HUD16（Approved）** 作为消费方在各 owner GDD 登记（systems-index 继承义务表 line 86/87）；#22 复用同一组 owner 事件、**各订各播**（声画分离），不新增 owner 侧字段需求 → **无新 propagate 债**（与 VFX19 「双向一致性·已存在无需新 propagate」同理）。
> ③ **新增 propagate 债（producer，不阻开工）= OQ-Audio-2**：各 owner GDD 的「事件供 UI/VFX 订阅」措辞应补「音频(22)」为消费方之一（VFX19 已在 line 86 登记，#22 平行补登）；以及 `PlayUISound` 接口需在 UI 屏 GDD（#16/#20/#23，多数 Not Started）登记被调义务——待下游 UI GDD 撰写时落实。
> ④ **破产事件接缝继承**：`OnBankruptcyDeclared`（经济5，2 字段）vs `OnPlayerBankrupt`（破产9，3 字段）的 OQ-VFX-13 跨档接缝——#22 与 **VFX19 / HUD16（均订经济5 `OnBankruptcyDeclared`）作同样选择**（订经济5），维持一致（[[10px-floor-consistent-with-approved-sibling]]）；若 producer 裁定改订 `OnPlayerBankrupt`，#22 随 OQ-VFX-13 同步改订（登记 OQ-Audio-2）。

---

## Tuning Knobs

| 旋钮 | 默认 | 安全范围 | 影响 | 来源 |
|---|---|---|---|---|
| `V_master` | 0.8 | [0.0, 1.0] | Master 总音量；0=全静音，过高（接近 1.0）多声叠加可能削波 | F-1 |
| `V_sfx` | 1.0 | [0.0, 1.0] | SFX 总线音量；调低削弱获得感反馈 | F-1 |
| `V_bgm` | 0.6 | [0.0, 1.0] | BGM 总线音量；默认低于 SFX 不抢戏，>0.8 可能盖过 SFX | F-1 / art-bible 状态 A |
| `N_voice_max` | 32 | [16, 64] | 全局并发声音上限；过低（<16）级联场景丢声明显，过高增 CPU/内存开销 | F-2（硬件相关，目标硬件定后校准） |
| `T_sfx_min_retrigger`（per-Cue 基准） | 0.06s | [0.02, 0.30]s | 同 Cue 最小重触发间隔；过小同帧爆音，过大连续动作丢声 | F-3 |
| `T_hop_step` | 0.12s | [0.06, 0.30]s | hop 步进音节奏间隔；与 VFX19 hop 视觉节奏对齐 | CR-6 / F-3 |
| `DUCK_FACTOR` | 0.4 | [0.2, 0.7] | 破产时 BGM 压低系数；过低 BGM 几乎消失，过高压不出破产 beat | F-4 |
| `T_bgm_crossfade` | 0.5s | [0.1, 2.0]s | BGM 状态切换淡变时长；过短切换生硬，过长拖沓 | F-4 / CR-13 |
| `T_duck_release` | 1.0s | [0.3, 3.0]s | 破产 duck 后 BGM 恢复常态时长 | F-4 |
| `cue_variant_count`（per-SFX） | 3 | [1, 5] | 每核心 SFX 变体样本数；越多越不疲劳，增资产/内存 | CR-7 |
| `pitch_jitter_range` | ±0.05 | [0.0, ±0.15] | 变体 pitch 随机抖动幅度（半音比例）；过大失真 | CR-7（#22 自建 RNG） |

> 所有电平默认值为 **起草期 sound-designer 建议值，待 audio-director 混音 pass + 目标硬件性能校准后定稿**（同经济 faucet:sink 比例的「待平衡 pass」性质）。`N_voice_max`/`pitch_jitter_range` 等性能/听感旋钮在 Pre-Production 标定。

---

## Acceptance Criteria

> **类型标注**：[Logic]=可 headless 自动化（确定性逻辑/混音算法/状态机）；[Integration]=多系统集成（事件订阅链路）；[Advisory]=呈现层听感（须人工/playtest 签核，headless 测不了实际声音）。BLOCKING=上线前必过；Recommended=应过、不阻。
> **headless 纪律**：实际声音的"好不好听""音色是否匹配情绪"是 Advisory·playtest-signoff——headless 无音频探针、测不了波形听感；只有**触发逻辑/混音增益/优先级/节流的状态机**标 [Logic]（用 spy / mock audio 接口断言"是否调用了播放、用什么 SoundId、什么增益、是否被节流/抢占"，不断言实际声波）。

### [Logic] BLOCKING

- **AC-1** [Logic] 给定 `OnDiceRolled(bIsDouble=false)` 事件，#22 调用一次掷骰基础音播放（spy: `PlaySound` 调用计数==1，SoundId==DiceRoll）；`bIsDouble=true` 且非定序语境且 `ConsecutiveDoubles∈{1,2}` → 额外触发强化音（spy: 强化音 SoundId 调用==1）。
- **AC-2** [Logic] 给定定序语境（`OnPhaseChanged` 阶段枚举=定序）+ `OnDiceRolled(bIsDouble=true)`，#22 **抑制**双点强化音（spy: 强化音调用==0，基础音==1）。验证 CR-3 定序抑制（继承 dice OQ-T-Dice-1）。
- **AC-3** [Logic] 给定 `ConsecutiveDoubles==3` + `OnDiceRolled(bIsDouble=true)`，#22 触发**警告音**（spy: 警告 SoundId==1，庆祝强化音==0）。验证 CR-3 第3连入狱分支。
- **AC-4** [Logic] `OnTurnStarted` 到达后 #22 内部缓存 `ConsecutiveDoubles==0`；随后未收到 `OnPhaseChanged` 即收到新玩家首掷 `OnDiceRolled(bIsDouble=true)` → 走基础/庆祝（非警告，证缓存未残留上家值）。验证 CR-3 跨回合重置。
- **AC-5** [Logic] 给定 `OnCashChanged(EChangeReason=Salary)` → 正向音色 SoundId；`EChangeReason=Tax` → 负向音色 SoundId；`EChangeReason`=未注册枚举值 → 中性音 SoundId（spy 断言三分支各调对应 Id）。验证 CR-4 + EC-5。
- **AC-6** [Logic] 给定 `OnRentPaid(Payer,Payee,Amount,TileIndex)`，#22 触发收租金币音一次（spy: SoundId==Coin，计数==1），且**不读 `Amount` 做音量缩放**（spy: 传入 gain 与 `Amount` 无关，固定 `G_cue`）。验证 CR-4 MVP 不按金额缩放。
- **AC-7** [Logic] 种子化后 `house_count_cache[t]==2`，收到 `OnBuildingChanged(t, newCount=3)` → 派生 **Build**（spy: BuildSound==1，SellSound==0），cache 更新为 3；收到 `OnBuildingChanged(t, newCount=1)` → 派生 **Sell**（SellSound==1，BuildSound==0）。验证 CR-5 delta 派生。
- **AC-8** [Logic] `house_count_cache[t]==4`，收到 `OnBuildingChanged(t, newCount=5)` → 派生酒店合并音（spy: HotelMergeSound==1）。验证 CR-5 `newCount==5` 酒店分支。
- **AC-9** [Logic] `house_count_cache[t]==3`，收到 `OnOwnershipChanged(t, Old=P1, New=BANK/None)` → #22 re-seed `cache[t]=GetHouseCount(t)`（mock 返回 0）→ cache 变 0；随后 `OnBuildingChanged(t, newCount=1)` 派生 **Build**（非 Sell，证 re-seed 封漂移）。验证 CR-5 Finding A 镜像。
- **AC-10** [Logic] `OnBuildingChanged` 首次到达前 `house_count_cache` 已经 `GetHouseCount` 种子化（spy: 订阅时调 `GetHouseCount`）；缓存 miss（mock 返回未设值）→ 走 EC-6 中性兜底音（spy: NeutralBuildSound==1）。验证 CR-5 种子化 + EC-6。
- **AC-11** [Logic] `OnOwnershipChanged(t, Old=None, New=P1)` → 买地音（spy: BuySound==1）；`Old=P1, New=P2`（破产移交）→ 中性翻页音（NeutralTransferSound==1，BuySound==0）。验证 CR-5 移交分支。
- **AC-12** [Logic] `effective_gain` 计算：`V_master=0.8, V_sfx=1.0, G_cue=0.9` → 实例 gain==0.72（±1e-6）；`V_sfx=0.5` → 0.36。验证 F-1 增益合成（边界值 fixture）。
- **AC-13** [Logic] `V_master=0` → 任意声音 `effective_gain==0`（spy: 传入 gain==0，但订阅/抢占逻辑仍执行）。验证 F-1 + EC-10 静音不影响逻辑。
- **AC-14** [Logic] `N_voice_max=2`，2 个 Secondary voice 活跃，到达 1 个 Critical（破产）→ `should_preempt==true`，抢占最早 Secondary（spy: 被抢占 voice 停止==1，破产音播放==1）。验证 F-2 抢占。
- **AC-15** [Logic] `N_voice_max=2` 满且全为 Critical，到达 1 个 Secondary → `should_preempt==false` → 新声**丢弃**（spy: 新 Secondary 播放==0，无 Critical 被停）。验证 F-2 低优先级丢弃。
- **AC-16** [Logic] `N_voice_max=1` 满（1 个 Critical=破产），到达第 2 个 Critical（胜利）→ **两者均播放**，临时超上限（spy: 活跃 Critical==2，无 Critical 被停）。验证 F-2 Critical 不变式 + EC-7。
- **AC-17** [Logic] 同 Cue 在 `t=0` 播放，`t=0.03s`（< `T_sfx_min_retrigger=0.06`）再触发 → `allow_retrigger==false`，丢弃（spy: 该 Cue 播放计数==1）；`t=0.07s`（≥0.06）触发 → 播放（计数==2）。验证 F-3 节流（边界值）。
- **AC-18** [Logic] 同帧 8 个 `OnOwnershipChanged` 触发同翻页 Cue，`T_sfx_min_retrigger=0.06`、模拟同帧（Δt<0.06）→ 仅 1 次播放（spy: 计数==1，后 7 次被节流丢弃）。验证 F-3 + EC-2 级联防爆音。
- **AC-19** [Logic] `state=Bankruptcy_active`，`V_bgm=0.6, DUCK_FACTOR=0.4` → `bgm_target_gain==0.24`（±1e-6）；`state=Victory` → `bgm_target_gain==0.0`；`state=Normal` → `0.6`。验证 F-4 三分支。
- **AC-20** [Logic] UI 层调 `PlayUISound(Click)` → #22 播点击音一次（spy: SoundId==UIClick）；连续 `t=0,0.03,0.07` 三次调用、`T_sfx_min_retrigger=0.06` → 播放 2 次（第 2 次 0.03 被节流）。验证 CR-11 + F-3 连点节流。
- **AC-21** [Logic] 读档重建（EC-8）：#22 先解绑再重绑 owner delegate（spy: 每个 delegate `Unbind` 先于 `Bind`，无重复绑定）；单个 `OnRentPaid` 事件读档后只触发 1 次金币音（**非 2 次**，证无双绑）。验证 EC-8 防双订阅。
- **AC-22** [Logic] #22 一切声音随机走自建 `FRandomStream`（code-review checklist + spy：变体选择/pitch 抖动调用的 RNG 句柄 ≠ 骰子3 权威流句柄）。验证 CR-7 RNG 隔离（继承 ai CR-5④ / VFX19 CR-9）。

### [Integration] BLOCKING

- **AC-23** [Integration] 事件格7 广播 `OnPlayerJailed(playerId, EJailReason)` → #22 触发入狱音；`OnPlayerReleased(playerId, EJailReleaseMethod)` → 出狱音；`OnCardDrawn` → 抽牌音（三事件订阅链路端到端，证 #22 正确挂接 #7 owner delegate）。验证 Dependencies 事件格7 行（tile-events L109）。
- **AC-24** [Integration] 经济5 `OnBankruptcyDeclared(Debtor,Creditor)` → #22 触发破产音（Critical）+ BGM 据 F-4 压低至 `DUCK_FACTOR` 倍；回合2 `OnGameWon(WinnerId)` → 胜利音 + BGM 淡出（端到端，证 #22 订经济5/回合2 而非直订破产9，与 VFX19/HUD16 一致）。验证 CR-2/CR-8 + Dependencies ④。

### [Advisory] Recommended（playtest / code-review 签核，headless 测不了）

- **AC-25** [Advisory·playtest-signoff] 掷骰音"期待→翻滚→定格"节奏感与 VFX19 视觉三阶段对齐，双点庆祝音 vs 入狱警告音情绪可辨（盲听可分庆祝/危险）。承接继承义务 dice OQ-T-Dice-4 听觉腿。
- **AC-26** [Advisory·playtest-signoff] 收租金币音、建房咔哒、破产沉闷、胜利号角的情绪与 art-bible 对应状态（B/F/G）一致；BGM 轻快不抢戏（art-bible 状态 A「不喧宾夺主」）。
- **AC-27** [Advisory·playtest-signoff] 跨 Cue 响度平衡（`G_cue` 校准）：默认音量下无单一 SFX 明显过响/过弱；级联破产场景经节流后听感为可辨单次而非爆音泥团。
- **AC-28** [Advisory·code-review] 混音总线层级（Master ← SFX/BGM）正确搭建，三路独立音量参数可被设置屏读写；无 MVP 范围外的子总线/ducking 自动化（防 scope 蔓延）。验证 CR-12 MVP 边界。
- **AC-29** [Advisory·code-review] AI 回合 `OnTurnStarted` 提示音静音/低音量，但金币/建房结算 SFX 正常播放（CR-9/EC-11 社交可闻性）。

---

## Open Questions

> 均为 Alpha / 架构期 / Pre-Production 标定，**不阻 MVP 开工**。

- **OQ-Audio-1（Alpha）动态音乐 / 自适应配乐 / 真 ducking**：BGM 按局势紧张度分层切换、按回合阶段切段、sidechain 实时 ducking（关键 SFX 压 BGM 包络）。MVP 用简单 crossfade + 阶跃 duck（CR-12/CR-13/F-4）。Alpha 评估 MetaSound 动态参数 / Quartz 节拍同步。
- **OQ-Audio-2（producer，跨档 propagate，不阻）**：① 各 owner GDD（骰子3/回合2/移动4/经济5/地产6/建房8）「事件供 UI/VFX 订阅」措辞补「音频(22)」为消费方（VFX19 已 line 86 登记，#22 平行补）；② `PlayUISound` 被调义务登记到 UI 屏 GDD（#16/#20/#23，多 Not Started）；③ 破产事件接缝 OQ-VFX-13 裁定后 #22 随 VFX19/HUD16 同步订阅源。
- **OQ-Audio-3（Alpha / Pre-Production）按金额缩放音频反馈**：`OnRentPaid.Amount` / `OnCashChanged` delta 映射到 pitch/volume/层数（大额收租更"爽"）。MVP 固定音不缩放（CR-4），Alpha 评估。
- **OQ-Audio-4（架构期 ADR）BP vs C++ 边界 + 音频中间件**：#22 用纯 UE5.7 原生（Sound Cue / MetaSound / Submix）还是引入 Wwise/FMOD 中间件？事件订阅在 C++ 框架层还是 Blueprint？`FRandomStream` 封装位置（同 VFX19 OQ-VFX-2 / ai OQ-AI-3b 模式）。LLM 知识截至 ~5.3，UE5.7 MetaSound/Submix API 变更须架构期对照 docs。
- **OQ-Audio-5（架构期）非阻塞追赶的声音生命周期**：过期声音"丢弃 vs 淡出"、抢占"静默停止 vs 短淡出"在 MVP 取静默简化（CR-8），Pre-Production playtest 评估是否需短淡出避免咔哒切断（pop noise）。
- **OQ-Audio-6（架构期）`OnPawnLanded.arrivalContext` 利用**：落地音是否据 `arrivalContext`（普通落地 / SentToJail）分支（去坐牢落地用不同音）？MVP 统一落地音 + 入狱另由 `OnPlayerJailed` 覆盖；待确认是否需区分。
- **OQ-Audio-7（Pre-Production 标定）`N_voice_max` / 内存预算**：并发上限、变体样本数、BGM 流式 vs 全载——待目标硬件内存上限确定后校准（同 art-bible 性能预算「待目标硬件确定」）。

---

*骰子大亨 Audio GDD — 2026-06-06 — sound-designer（pending `/design-review`）*
