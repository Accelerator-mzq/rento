# HUD（玩家面板 / 现金 / 回合）

> **Status**: **Approved**(R-3 fresh-session design-review 2026-06-05 = NEEDS REVISION〔finishing-class,收敛〕→ 就地修订完成,**5 must-land 全闭合**,CD 裁定 finishing 末圈、无需 R-4 重审:① AC-57 永真谓词+T_min 旁路缺失→重写为配置参数读回、补旁路 ② V-Own-10/AC-21 Ease-Out Bounce→Cubic〔R-2 去超冲漏传播〕③ AC-12/24a/30 OQ-HUD-2 门控标到 AC 条目 ④ V-Own-07"底部操作栏或"内部矛盾→删 ⑤ EC-11/AC-48 BP `Bind Event` vs C++ `AddDynamic` 去重区分。+ Recommended 收口:F-4 T_stale 最坏旋钮角约束、E_cur 额外回合引 player-turn L197、critical 集合 Mortgage/Unmortgage exclude 裁定、tnum 5.7 核验标注、F-2 PlaybackSpeed 排除理由、AC-58 BP 引脚守护、net_worth fallback 安全前提。AC 61→62。详见 review-log R-3 条目)
> **R-1/R-2 history**: R-1(同会话自taint)+ R-2(fresh,3 blocker:state_gen 幻象/EActionType propagate/reduce-motion AC)均 NEEDS REVISION→就地修订;严重度逐轮单调下降(R-1 系统级核循环破损 → R-2 跨档幻象契约 → R-3 谓词/未传播兄弟/标签收口)=健康收敛。
> **Author**: msc + design-system agents
> **Last Updated**: 2026-06-05
> **Implements Pillar**: ④ 易上手（清晰可读的信息显示）+ ② 社交博弈（建房通告 beat / AI 行动可见性，MVP 单屏=持屏者感知，见 Player Fantasy 范围限定）+ Sensation（juice 反馈）
> **Design Review (R-1, 2026-06-05)**: NEEDS REVISION → 就地修订完成。Specialists: game-designer/systems-designer/qa-lead/ux-designer/ue-umg-specialist + creative-director。修订见各节 "R-1" 标注。

## Overview

HUD(抬头显示)是《骰子大亨》的呈现层信息中枢——它把游戏状态系统(玩家回合、经济现金、棋盘)持续广播的事件与快照,翻译成玩家一眼可读的屏幕信息:每位玩家的现金与资产、当前轮到谁、所处回合阶段、以及收租/发薪/建房/破产等关键时刻的即时反馈。作为**呈现层纯叶子**,HUD 只**读取**状态与**监听**事件、只负责**显示**;它从不写游戏状态、也不驱动流程——所有数值与裁决都由 owner 系统先算定,HUD 仅回放既定结果。对玩家而言,HUD 是整局游戏全程凝视的面板:它让"聚光灯落在我身上"(当前回合高亮)、"对手在跟我斗"(AI 行动逐步可见)、"有人盖楼了"(全员可见建房通告)这些瞬间有据可演,并以清晰、低压力、不跳动的数字呈现服务支柱④易上手与 Submission(放松)。没有它,核心循环的每一步都将失去可读性与反馈——玩家无法知道自己有多少钱、轮到谁、刚刚发生了什么。HUD 拥有**信息显示布局、UMG widget 结构与数据绑定**;juice 动画的视觉资产(金币飞溅、Niagara 特效)归 VFX(19)/音频(22),HUD 使能而不拥有这些体验(enable-not-own)。

## Player Fantasy

**核心感受:"一切尽在掌握,我不必费力就看懂全局。"**

HUD 服务的不是炫技的幻想,而是一种**安心的掌控感**——这是支柱④易上手与 MDA **Submission(放松,priority-2)** 的直接落点。玩家围坐一桌(或独自对 AI)开一局休闲博弈时,不应为"我有多少钱?""轮到谁了?""他刚才付了多少租?"而眯眼搜寻或心算;答案永远在屏幕固定位置、用大而稳的等宽数字、不跳动地呈现。这种"零认知负担的可读"本身就是放松体验的地基——*Design test(支柱④)*:在"增加策略深度"与"保持易懂不吓跑休闲玩家"之间,HUD 永远优先后者。

在放松的底色之上,HUD 点亮三个**支柱②社交博弈**的情感时刻:

1. **"聚光灯落在我身上"** —— 轮到自己时,我的玩家面板即时高亮(+15% 亮度、scale 1.05),全桌一眼知道该谁了;这种被点名的小仪式感让回合流转有节奏、有存在感。
2. **"对手在跟我斗"** —— AI 不是黑箱跳步,它的买地、建房、付租被逐步、可见地播放出来,我能看着它思考与行动,"它在跟我算计"的对抗感才成立。
3. **"有人盖楼了!"** —— 任何玩家盖楼都会触发通告 beat,即使不在我的回合——这恢复了"仅自己回合建房"削弱的社交引信,让"对手变强了,我得当心"的张力被看见。*Design test(支柱②)*:在"提升单人最优策略"与"制造玩家间互动/冲突"之间,HUD 的反馈设计选后者。

> **⚠ MVP 范围诚实限定(R-1,game-designer/CD)**:上述三个社交时刻在 **MVP 本地 Pass'N Play 单屏**下,"全员/传遍牌桌"实际兑现为**当前持屏者的感知**(他人不看屏)。故 MVP 支柱②的 HUD 兑现范围 = **当前玩家 vs AI 的观察对抗感 + 持屏者对"对手变强"的即时知晓**;**全桌同时社交感知**(多人各自盯屏)留 **Full Vision 多屏/在线**(OQ-HUD-5)。本节情感措辞不假装单屏已达成全桌同时感知。

而收租金币飞溅、发薪入账、破产清算的"获得感/肉痛感"juice,HUD **使能而不拥有**:它提供准确的事件锚点与数值,让 VFX(19)/音频(22)有据可演——HUD 负责"你看得懂发生了什么",juice 负责"你感觉到了爽/痛"。

> 注:`creative-director` 未咨询(lean 模式跳过非 D/H 节)。production 前须人工复核本节情感措辞与 player-turn OQ-8(Submission 维度)CD 复核闭合。

## Detailed Design

### Core Rules

> 约定:本节所有规则中,HUD 一律为**消费侧/呈现侧**——监听事件、读快照、显示;**绝不**写游戏状态或驱动流程。事件 payload 签名见 Interactions 表(权威 owner 已定)。

**布局结构(art-bible §7.1 权威)**
- **CR-1 三区布局**:顶部 HUD 栏(玩家面板×N + 回合信息)+ 中央棋盘区(≥65%,非 HUD 拥有)+ 底部操作栏(掷骰按钮 / 当前玩家 / 操作组 / 地产卡快览入口)。HUD 拥有顶部栏与底部操作栏的 widget 结构、布局与数据绑定。

**玩家面板(每玩家一块)**
- **CR-2 面板集**:为每位在局玩家(MVP 本地 2–4,Full Vision 至 8)渲染一块面板,绑定该玩家**只读**状态:`PlayerId` + 玩家色、现金、资产摘要(持有地产数 / 可选 net_worth)、当前回合高亮态、破产态。面板视觉规格遵 art-bible §7.4(a):圆角卡片 + 玩家色 4px 左边框。
- **CR-3 现金显示**:监听经济5 `OnCashChanged(Player,OldCash,NewCash,EChangeReason)` 更新对应面板现金;数字用 **Tabular Figures 等宽**防跳动(§7.2),`NewCash` 经 counter 动画(≤1.5s,§7.5)过渡;按 `EChangeReason` 着语义色(Salary 入账=金/正面,Rent/Tax 付出=红/肉痛,§4.3)。初始现金由经济5/棋盘1 供(`STARTING_CASH`=1500 标准 / 750 快速)。**实现约束指针(R-1,ue-umg-specialist P4):UE 不提供 OpenType `tnum` 特性 API**(`FSlateFontInfo` 无 feature 字段、HarfBuzz shaping 不对用户暴露 per-run feature tag),故"等宽数字"须选**数字字形本身等宽**的字体资产(不能靠渲染器激活 tnum)——此为 AC-10 的硬依赖,**propagate 给 art-director:art-bible §7.2 须指定具体等宽数字字体资产**(现仅写"Tabular Figures"属性)。**⚠ 核验标注(R-3,ue-umg):此结论基于 ≤UE5.3 知识,UE 5.7 未见暴露此 API 的 release note 迹象但须架构期 ue-umg-specialist 核验 `FSlateFontInfo`/HarfBuzz feature tag 暴露情况;若 5.7 确有此 API,art-bible §7.2 字体约束可放宽。即使结论错,fallback(等宽字形字体)仍可行,非 BLOCKING。**
- **CR-4 当前回合高亮**:监听回合2 `OnTurnStarted(FTurnStartedInfo{PlayerId,bIsAI})`,对该 `PlayerId` 面板施加高亮(+15% 亮度、scale 1.05,§7.4a);**起呈现 ≤100ms、完成视觉切换 ≤500ms**(player-turn L450 数值化标准)。`bIsAI` 供面板路由(如显示"AI 思考中"指示)。
- **CR-5 破产面板态**:监听经济5 `OnBankruptcyDeclared(Debtor,Creditor)`,对 `Debtor` 面板置破产态——去饱和 60% + 灰对角纹(§7.4a)。**终态**,不再退出。**HUD 内并发优先级(R-2,ux-designer G4):破产终态 > 进行中 counter**——若 `Debtor` 面板 counter 动画正在播放,收到 `OnBankruptcyDeclared` 时**立即以当前 displayed 值锁定为最终现金显示**(不再向 NewCash 插值),再起破产去饱和渐变;避免"灰化面板上数字仍在滚动"的状态读序矛盾。(此为 HUD 拥有的同源并发裁定;跨系统并发——HUD 去饱和 + VFX19 棋子消散 + 音频22 破产音——的注意力仲裁归 OQ-HUD-11。)

**回合 / 阶段呈现**
- **CR-6 阶段指示**:监听回合2 `OnPhaseChanged(FPhaseChangedInfo{OldPhase,NewPhase,ConsecutiveDoubles})`,据新旧阶段做过渡呈现;用 `ConsecutiveDoubles` 区分"首次 RollPhase"与"双点额外回合 RollPhase"(player-turn L129 重发语义),避免误显两次开局。
- **CR-7 回合切换动画**:监听回合2 `OnTurnEnded(FTurnEndedInfo{PlayerId,bGrantsExtraTurn})`,**两分支各有独立动画**:`bGrantsExtraTurn==true` → "同玩家继续"(双点额外回合,高亮停留 + 链提示);`==false` → "移交新玩家"(高亮转移过场)。
- **CR-8 Pass'N Play handoff 不叠帧**:本地轮流(Pass'N Play)正常人驱动切玩家时,新玩家 handoff 高亮**必须等前一回合 outro 动画完成才起**,不与前一回合 outro 叠帧(player-turn R3 B-2)。**唯一例外**:当**新权威状态到达**(读档 EC-11 / 框架快进)使 outro 未完即须切换时,状态正确优先(见 EC-4),此例外不构成正常路径的叠帧违规。
- **CR-9 席位裁定可见**:监听回合2 `OnTurnOrderResolved(FTurnOrderResult{OrderedPlayerIds,bResolvedBySeatTiebreak})`,**可见**呈现最终回合顺序(反聚光灯——即使 `bResolvedBySeatTiebreak==true` 由席位裁定决出,也不得静默,让玩家知道为何某人先手,player-turn L108)。

**AI 行动呈现**
- **CR-10 AI 行动逐步播放**:监听回合2 `OnAIActionExecuted(FAIActionDetails{ActionIndex,EActionType,ActingPlayerId,TargetTileIndex,Amount})`,**非阻塞**逐步呈现 AI 行动,**按 `ActionIndex` 升序**播放(0..M-1,M=实播数)。`EActionType` 供行动文案/图标(V-Own-07)与 critical 判定(F-4)。**`EActionType` 由 R-2 跨档 propagate 落定**(player-turn `FAIActionDetails`,OQ-HUD-12 ✅RESOLVED 2026-06-05)。
- **CR-11 框架不 gating(R6 RETREAT 关键简化)**:HUD **自主非阻塞**呈现 AI 行动,**不阻塞框架推进**;框架不再等 HUD 回放完成。**不设计** `OnAIActionPlaybackSegmentComplete` / 任何回放完成回调(已删除该义务)。若 HUD 呈现慢于框架推进,以最新状态为准、丢弃过期中间动画(不回灌框架)。

**社交 beat 与终局**
- **CR-12 建房通告 beat**:监听回合2 `OnBuildingAnnounced(TileIndex,NewHouseCount,ActingPlayerId)`,呈现建房通告(ease-out 上滑 200ms、延迟 2s 消失,§7.5),**无论当前是否 `ActingPlayerId` 的回合**——为**持屏玩家**恢复 CR-8「仅自己回合建房」削弱的支柱②社交引信(building OQ-Build-6)。**MVP 单屏下"可见"= 当前持屏者可见**;多屏全桌同时感知 deferred(OQ-HUD-5,Full Vision)。
  - > **⚠ MVP 单屏时序诚实限定(R-2,game-designer)**:Pass'N Play 下建房发生在 `ActingPlayerId` 自己的回合(此刻持屏者=建房者本人),banner 2.4s 生命周期在设备移交下一玩家**前**即已消失——故"对手盖楼了"对**下一持屏玩家实际不可见**。MVP 单屏 CR-12 的真实兑现 ≈ 建房者看到自己的通告 + 旁观者(若在场看屏)即时可见;**"回合开始时回看上一轮对手建房摘要"的补偿机制**(让轮到的玩家知道对手在其离屏期间盖了楼)归 **OQ-HUD-13**(Pre-Production `/ux-design`,非 MVP 阻塞,亦非纯 Full Vision——单屏下即存在)。
- **CR-13 胜利态**:监听回合2 `OnGameWon(winnerId)`,呈现胜利终局态。HUD 不独立判定胜负(口径归破产9/回合2)。

**输入与反馈锚点**
- **CR-14 输入意图路由**:HUD 拥有交互控件(掷骰 / 买地 / 建房 / 抵押 / 赎回等按钮);点击仅向 owner 系统的公开接口**转发玩家输入意图**,HUD **不自行裁决结果、不变更状态**。按钮启用/禁用是对**只读 affordance** 的呈现反映(如非自己 RollPhase 时掷骰按钮禁用),非 HUD 决策。避免 hover-only 交互(技术偏好,利手柄适配)。
- **CR-15 收租/数值锚点**:HUD 可显示经济5 `OnRentPaid(Payer,Payee,Amount,TileIndex)` 的收租数值信息;**金币飞溅 juice 归 VFX(19)**(VFX 直接订阅经济事件,不经 HUD,tile-events L272)。HUD 可显示骰子3 `Total` 数值;**骰面(Die1/Die2)滚动呈现归 VFX(19)**。
- **CR-16 juice RNG 隔离**:HUD 本地任何 juice 随机(如轻微抖动/粒子节奏)**必须走独立非权威 `FRandomStream`**;**严禁复用骰子3 权威流**(否则污染重放,ai-opponent CR-5④;dice 模型=各系统自建流,dice.md L187)。

### States and Transitions

**玩家面板状态机(每面板独立)**

| 当前态 | 触发 | 新态 | 备注 |
|--------|------|------|------|
| Normal | `OnTurnStarted(PlayerId==本面板)` | Active | 高亮 +15%/scale1.05,≤100ms 起/≤500ms 完成(CR-4) |
| Active | `OnTurnEnded(bGrantsExtraTurn==false)` 且移交他人 | Normal | 移交过场(CR-7) |
| Active | `OnTurnEnded(bGrantsExtraTurn==true)` | Active | 同玩家继续,高亮停留(CR-7 链提示) |
| Active | `OnTurnStarted(PlayerId≠本面板)` | Normal | 他玩家激活 → 本面板退 Normal(**Active 互斥不变式落地**,R-2 补;消除"仅靠 OnTurnEnded 退出"的状态表洞) |
| Normal / Active | `OnBankruptcyDeclared(Debtor==本面板)` | **Bankrupt** | 去饱和60%+灰对角纹;**终态**(CR-5) |
| Bankrupt | —(无) | Bankrupt | 终态,无退出转换 |

**HUD 全局状态**

| 当前态 | 触发 | 新态 | 备注 |
|--------|------|------|------|
| InGame | `OnGameWon(winnerId)` | GameOver | 胜利终局呈现(CR-13) |

**AI 行动呈现态(非 gating,纯呈现内部态)**

| 当前态 | 触发 | 新态 | 备注 |
|--------|------|------|------|
| Idle | `OnAIActionExecuted(ActionIndex)` | Playing | 入队按 ActionIndex 升序非阻塞播放(CR-10) |
| Playing | `OnAIActionExecuted(ActionIndex)`(续) | Playing | **追加入队**(不重置态);重复 ActionIndex 去重(幂等,防事件重传,缺陷14) |
| Playing | 队列**全部**处理完(播出或判 expired)/ 下一回合开始(`E_cur` 推进) | Idle | **全部**动作处理完毕才归 Idle(非单条 expired 即归,避免漏处理后续未过期动作);不回调框架;过期动画丢弃(CR-11) |

> **状态机不变式(R-1 补,systems-designer 缺陷13/15):**
> - **面板 Active 互斥**:任意时刻**至多一块**面板处于 Active。若同帧误收多个 `OnTurnStarted`(owner bug/测试 harness),仅最后一个生效、其余忽略(权威当前玩家唯一,回合2 拥)。
> - **Bankrupt + GameOver 组合**:面板 Bankrupt(去饱和60%+灰对角纹)为终态;收到 `OnGameWon` 进 GameOver 时,Bankrupt 面板**不叠加二次灰化**——GameOver 覆盖层的"非胜者灰/低透明"(V-Own-09)对已 Bankrupt 面板**取已有破产视觉**(不二次去饱和),避免双重灰化。
> - **GameOver 时面板逻辑终态(R-2 补,systems-designer 缺陷15)**:收到 `OnGameWon` 后,各面板**冻结当前逻辑态**(Active/Normal/Bankrupt)、**不再响应**后续 `OnTurnStarted`/`OnCashChanged`/`OnTurnEnded`;视觉由 GameOver 覆盖层(V-Own-09)接管,面板 Active/Normal 区分在覆盖层下可省略。GameOver 为 HUD 全局终态(MVP 无局内重开;若 Full Vision 支持重开,复用 EC-11 读档式重建路径,不在此状态机内逆转)。

### Interactions with Other Systems

> 方向约定:HUD 是**纯叶子消费者**——只 `←` 接收/读取,**无** `→` 写回任何系统。所有事件由各 owner 广播,HUD 订阅。

| 系统 | 数据流入 HUD(HUD 消费) | HUD 流出 | owner |
|------|------------------------|----------|-------|
| **玩家回合(2)** | `OnTurnStarted{PlayerId,bIsAI}` / `OnPhaseChanged{OldPhase,NewPhase,ConsecutiveDoubles}` / `OnTurnEnded{PlayerId,bGrantsExtraTurn}` / `OnTurnOrderResolved{OrderedPlayerIds,bResolvedBySeatTiebreak}` / `OnAIActionExecuted(FAIActionDetails{ActionIndex,EActionType,...})` / `OnBuildingAnnounced(TileIndex,NewHouseCount,ActingPlayerId)` / `OnGameWon(winnerId)` | 无(输入意图经 CR-14 转发回合2 公开接口,非事件) | 回合2 拥有全部广播 + 流程;HUD 拥呈现;**`EActionType` 为 R-2 propagate 请求(OQ-HUD-12)** |
| **经济(5)** | `OnCashChanged{Player,OldCash,NewCash,EChangeReason}` / `OnRentPaid{Payer,Payee,Amount,TileIndex}` / `OnBankruptcyDeclared{Debtor,Creditor}`;查询 `GetCash` | 无 | 经济5 拥结算+事件;HUD 拥现金/应付额显示(economy L110/L377) |
| **棋盘(1)** | 读 tile 数据供资产摘要;`SalaryAmount`(=200)显示 | 无 | 棋盘1 拥地图/字段 |
| **骰子(3)** | 可读 `FDiceRollResult.Total` 显数值 | 无 | 骰子3 拥 RNG;**骰面 Die1/Die2 呈现归 VFX19** |
| **AI 对手(10)** | **间接**:AI 不自 emit;回合2 执行 AI 动作时广播 `OnAIActionExecuted`,HUD 据此非阻塞呈现 | 无 | AI10 只供决策质量;HUD 拥呈现;**CR-5④ juice RNG 须独立流** |
| **破产胜负(9)** | 间接:胜负经回合2 `OnGameWon` 呈现;若显局末排名用 `net_worth`(=Cash+net_liquidation_value)只读口径 | 无 | 9 拥破产判定+排名口径 |
| **VFX(19)** | **边界厘清(enable-not-own)**:HUD 拥**信息显示布局 + UMG widget + 数据绑定**;VFX 拥 juice 视觉资产/Niagara/世界空间金币飞溅。VFX **直接订阅经济/骰子事件**,不经 HUD 转发。HUD 提供准确事件锚点与数值,使能 juice 而不拥有 | 无(不向 VFX 推送) | 各拥各域,无重叠 |
| **地产卡与卡牌 UI(17)** | **边界**:HUD 拥玩家面板/现金/回合/操作栏;**地产卡详情面板**(地块详情、租金表、建房选项明细)归 #17。HUD 操作栏仅含"地产卡快览入口" | 无 | #17 拥地产卡详情 UI |

> 注:`ux-designer` / `ue-umg-specialist` 未咨询(lean 模式)。widget 层级、数据绑定形式(BindWidget / MVVM)、事件订阅-解绑生命周期 → 架构期 ADR + Pre-Production `/ux-design`。

## Formulas

> **边界声明**:HUD 是呈现层纯叶子,本节公式**仅限呈现层时序/动画参数化**。所有游戏数值(现金、租金、net_worth、概率)由 owner 系统(经济5/破产9 等)算定,HUD 仅显示其结果——本节不含任何游戏逻辑/经济/概率公式。

### F-1 现金 Counter 动画时长

`T_counter = clamp( K_dur × log10(|ΔCash| + 1) , T_min , T_max )`

**变量:**
| 变量 | 符号 | 类型 | 范围 | 描述 |
|------|------|------|------|------|
| 现金变化 | ΔCash | int | −∞~+∞(实际约 −5000~+5000) | `NewCash − OldCash`;绝对值驱动时长,正负不影响 |
| 时长系数 | K_dur | float | 0.3~0.7(推荐 0.55) | 时长缩放系数,主要调参 |
| 时长下界 | T_min | float | 0.2~0.4s(推荐 0.3) | 极小金额最短动画,防转瞬即逝 |
| 时长上界 | T_max | float | 1.0~1.5s(推荐 1.5) | art-bible §7.5 硬约束上界 |
| 动画时长 | T_counter | float | T_min~T_max | 本次 counter 动画实际时长(秒) |

**Output Range:** 0.3s(小额)~1.5s(大额被 clamp)。ΔCash=0 时 log10(1)=0→T_min(但 amount==0 事件经济侧已早返不发,economy AC-5);无论多大金额 clamp 到 1.5s 不破 §7.5。
**Example:** ΔCash=+200→clamp(0.55×log10(201),0.3,1.5)=clamp(1.267,...)=**1.267s**;ΔCash=−2000→clamp(0.55×3.301,...)=**1.5s**(上界 clamp);ΔCash=−30→**0.820s**。

### F-2 现金 Counter 插值(逐帧显示值)

`displayed(t) = OldCash + (NewCash − OldCash) × ease_out(t / T_counter)`
`ease_out(u) = 1 − (1 − u)^E_pow`  (u∈[0,1] → [0,1])

**变量:**
| 变量 | 符号 | 类型 | 范围 | 描述 |
|------|------|------|------|------|
| 已播时间 | t | float | 0~T_counter(秒) | 动画已播放时间 |
| 动画总时长 | T_counter | float | T_min~T_max(F-1 结果) | 本次动画总时长 |
| 起点现金 | OldCash | int | 0~game_max | `OnCashChanged` payload |
| 终点现金 | NewCash | int | 0~game_max | `OnCashChanged` payload |
| 缓动幂次 | E_pow | float | 1.5~3.0(推荐 2.0) | ease-out 幂次,2.0=平方缓出 |
| 显示值 | displayed(t) | float→round int | OldCash~NewCash | 当前帧显示数字(渲染前 round 整数) |

**Output Range:** t=0→OldCash(精确);t=T_counter→NewCash(精确,无误差);中间快进-慢出。E_pow=1 退化线性;>3 末段过缓不推荐。等宽 Tabular Figures(§7.2)在字体层保障横向不跳动。
**不变式(R-1 补,systems-designer 缺陷3/5):** ① `T_min > 0`(故 T_counter≥T_min>0,杜绝 `t/T_counter` 除零)——T_min 安全下界 0.15s,实现层须 assert T_min>0 且 `T_min ≤ T_max`(F-1);② `E_pow ≥ 1`(<1 时 ease_out 退化为 ease-in,违语义)——实现层 clamp/assert。
**ΔCash=0 路径(EC-2 实现澄清):** ΔCash==0 时**早返特判,不调 F-1/F-2**(非靠 F-2 幅度为0静默运行),避免依赖 T_counter 非零的隐式假设。
**Example:** OldCash=1000,NewCash=1200,T_counter=1.267s,E_pow=2.0:中点(u=0.5)ease=0.75→显 **1150**;t=1.0s(u=0.789)ease=0.955→显 **1191**;终点→**1200**。
**实现约束指针(R-1,ue-umg-specialist P1;R-3 精化排除理由):** F-1/F-2 要求**运行时逐帧动态求值**(时长随 ΔCash 变),**不可用设计时烘焙固定时长的 UWidgetAnimation 实现**;具体机制(NativeTick / Timeline / C++ 侧驱动)锁定归 OQ-HUD-2 架构期 ADR。**⚠ 精确排除理由(R-3,ue-umg):即便用 `PlaybackSpeed` 缩放 UWidgetAnimation 改变有效时长,仍不可行——根因是 UMG sequencer **无整数文本插值轨道**,滚动数字必须每帧 `SetText(FText::AsNumber(round(displayed(t))))`,本质即 NativeTick/Timeline 逐帧驱动;排除理由不是"时长烘焙",勿让工程师误试 PlaybackSpeed 路径。** 本约束须在 ADR 显式登记,否则实现走错路。

### F-3 当前回合高亮时序约束(可测参数化)

`t_onset ≤ T_ONSET_MAX`  ∧  `t_complete ≤ T_COMPLETE_MAX`
其中 `t_onset = t_highlight_first_frame − t_event_received`,`t_complete = t_highlight_full_scale − t_event_received`

**变量:**
| 变量 | 符号 | 类型 | 范围 | 描述 |
|------|------|------|------|------|
| 事件接收时刻 | t_event_received | float(ms) | 游戏内壁钟 | HUD 收到 `OnTurnStarted` 的时刻 |
| 起呈现延迟 | t_onset | float(ms) | 0~T_ONSET_MAX | 接收到首帧变化的延迟 |
| 完成延迟 | t_complete | float(ms) | 0~T_COMPLETE_MAX | 接收到动画完成的总延迟 |
| 起呈现上限 | T_ONSET_MAX | float | **≤100ms**(player-turn L450,固定) | 超出破坏聚光灯感 |
| 完成上限 | T_COMPLETE_MAX | float | **≤500ms**(player-turn L450,固定) | 超出节奏断裂 |
| 高亮动画时长 | D_highlight | float | 100~500ms(推荐 300) | 过渡时长,受双约束 |

**约束推论:** **上界 400ms** 由约束反推(`complete=onset+D≤500`,onset≤100→D≤400,AC-55 守护);**下界 200ms 是仪式感最小时长的设计选择**(非约束推导——低于 200ms 高亮太快缺被点名感),可调。`T_ONSET_MAX=100ms`/`T_COMPLETE_MAX=500ms` 是 AC FAIL 边界(不可放宽,非 clamp 对象);**该约束推论仅在非 FAIL 场景成立**(onset>100ms 即 EC-9 FAIL 路径,此时窗口失效)。
**Output Range:** 60FPS:onset≈16.6ms(一帧)、D=300ms、complete≈317ms 满足双约束;30FPS:complete≤500ms 仍可满足但 D_highlight≤467ms;UI 线程阻塞致 t_onset>100ms = AC FAIL。
**Example:** 事件 t=0 → 起帧 16.6ms(t_onset=16.6≤100 ✓)→ D=300ms(**Ease-Out Cubic/Quart——R-2 改:去超冲,对齐 Player Fantasy"大而稳不跳动"Submission 定位;超冲/弹性归 VFX 拥有的 juice,非面板高亮**;propagate art-bible §7.5 高亮曲线选型)→ 完成 316.6ms(t_complete=316.6≤500 ✓)。

### F-4 AI 行动逐步播放节奏 + 追赶/跳过规则

基础节奏:`T_slot(i, M) = clamp( T_base − K_compress × (M − 1) , T_slot_min , T_base )`

过期判定(**R-2 修订:主判据改为 HUD 可观察的回合代次,消除幻象框架契约**):
`expired(i) = turn_advanced_past(i)  OR  unpaused_elapsed(i) > T_stale`
- **主判据 `turn_advanced_past(i)`**:HUD 观察到**比动作 i 所属回合更新的回合已开始**(以 **HUD 本地回合代次 `turn_epoch`** 计:`E_cur > E_action(i)`)。`turn_epoch` 是 **HUD 自持**的单调计数器:每收到一次 `OnTurnStarted`(新当前玩家回合开始)即 +1;`E_action(i)` = 动作 i 的 `OnAIActionExecuted` 到达时刻的 `E_cur`;读档重建(EC-11)落到更新代次同样触发。**此判据仅依赖 HUD 已订阅的 `OnTurnStarted`/`OnTurnEnded`/读档事件——不依赖任何框架广播的"状态代次"**:player-turn R6 RETREAT 后框架即时 EndTurn、不阻塞、亦**不提供**此类计数(player-turn L187:"AI 动画可能被下一回合呈现覆盖,由 HUD 设计缓解,非框架契约";`FAIActionDetails` 仅含 per-回合自增的 `ActionIndex`,无跨回合 state_gen)。这正是 R6 RETREAT 的真实意图——**"下一回合已开始"才丢弃上一 AI 回合的中间动画**,而非"播得慢"就丢弃。
  - **同回合内 M 条动作共享同一 `E_action`**(均在下一 `OnTurnStarted` 之前到达)→ 主判据**绝不**在正常路径内误判同回合任一条过期。**注:框架 `ActionIndex` 每回合从 0 自增、不跨回合区分(player-turn L200),故不可用作过期判据;回合区分由 HUD 的 `turn_epoch` 独立承担**(消除"per-action state_gen 解读会让前 M-1 条瞬间过期"的击穿风险)。
  - **双点额外回合链的 E_cur 安全性(R-3 补依据,防跨档静默漂移)**:`E_cur` 每收一次 `OnTurnStarted` +1;**player-turn L197 权威定义"双点额外回合链 `OnTurnStarted` 整链只发一次、不在每次额外回合重发"**(额外回合靠 `OnPhaseChanged(RollPhase)` 重发)→ 故同一玩家的双点额外回合链内 **`E_cur` 不递增**,整链 AI 动作共享同一 `E_action`,**不会被误判过期**。⚠ 此安全性**依赖 player-turn L197 不重发语义**;若 player-turn 未来改为每额外回合重发 `OnTurnStarted`,本判据须同步修订(登记为跨档接缝依赖)。
- **辅判据(软上界,防 HUD 自身卡死队列)`unpaused_elapsed(i) > T_stale`**:`unpaused_elapsed` 用**游戏时间**(暂停不计)、**读档时重置**——杜绝壁钟方案在暂停/读档后差值爆炸致全队列误判过期(systems-designer 缺陷10)。

追赶规则:`if expired(i): 直接应用 snapshot_state(i)(不播动画)并 proceed; else: 播 T_slot(i,M) 动画`(不回灌框架,CR-11)。
**M=0(本轮 AI 无实播动作)**:不进入播放循环,直接保持 Idle,F-4 不被调用(systems-designer 缺陷8)。

**critical 动作保护(R-1 新增,守支柱②;R-2:criticality 由 payload `EActionType` 判定)**:动作的 `is_critical(i)` **由 `OnAIActionExecuted` payload 的 `EActionType` 派生**(改变博弈格局者:买地 `BuyProperty` / 建房 `BuildHouse` / 破产 `Bankruptcy` = critical),**非 HUD 臆断**——**critical 集合裁定(R-3,game-designer 提 Mortgage/Unmortgage,CD 裁定 exclude-with-justification):`Mortgage`/`Unmortgage` 改变"哪块地收租"但其结果状态(抵押标记)**持久显示在棋盘/地产卡 UI(#17)**,即使 expired 跳过瞬时通告 beat,玩家仍可从棋盘读到该状态——故风险类别低于建房(banner 是一次性社交 beat,错过即丢),裁定 `Mortgage`/`Unmortgage` **非** critical(可整条跳过);若 Pre-Production playtest 显示玩家确实漏判对手抵押动机,再升 critical(挂 OQ-HUD-10 随 #17 设计复核)。`PayRent` 同理非 critical(金额由现金 counter 覆盖)**——HUD 纯叶子,criticality 须由 owner 经 payload 供(player-turn `FAIActionDetails` 扩 `EActionType`,见 Interactions 表 + 跨档 propagate OQ-HUD-12)。critical 动作即使被判 expired,**也不得静默跳过**——expired 路径下至少**呈现其最终态**(如建房通告 banner、买地归属变更),仅可省略中间过场动画。非 critical 动作(移动、过路结算等)expired 时可整条跳过。回链 AC-56。

**变量:**
| 变量 | 符号 | 类型 | 范围 | 描述 |
|------|------|------|------|------|
| 动作索引 | i | int | 0~M−1 | `ActionIndex`,升序 |
| 实播动作数 | M | int | 1~10(MVP 推荐≤5) | 本轮 AI 实播动作总数 |
| 基准时长 | T_base | float | 0.8~1.5s(推荐 1.0) | 单条动作基准显示时长 |
| 压缩量 | K_compress | float | 0.05~0.15s/条(推荐 0.08) | 每多一条的压缩量 |
| 单条下界 | T_slot_min | float | 0.4~0.6s(推荐 0.5) | 单条最短显示,保证看清 |
| 单条时长 | T_slot(i,M) | float | T_slot_min~T_base | 第 i 条实际显示时长 |
| 当前回合代次 | E_cur (turn_epoch) | int | HUD 本地单调递增 | HUD 自持:每收 `OnTurnStarted` +1(**非**框架广播) |
| 动作回合代次 | E_action(i) | int | HUD 本地单调递增 | 动作 i 的 `OnAIActionExecuted` 到达时的 `E_cur`;同回合 M 条共享同值 |
| 回合已推进 | turn_advanced_past(i) | bool | t/f | `E_cur > E_action(i)`(主判据,HUD 可观察) |
| 未暂停经过时长 | unpaused_elapsed(i) | float(ms) | 游戏时间,暂停不计、读档重置 | 动作 i 自入队起的有效经过(辅判据用) |
| 过期软上界 | T_stale | float | **3.5**~6.0s(推荐 4.0) | **辅判据**软上界(防 HUD 卡死);主判据是回合推进。须 ≥ 预期最大队列总时长(MVP M≤5 时 ≈3.4s),故安全下界钉 3.5s |
| 是否过期 | expired(i) | bool | t/f | 主判据(回合推进)∨ 辅判据(软上界);中间态无意义 |
| critical 动作 | is_critical(i) | bool | t/f | 由 payload `EActionType` 派生(买地/建房/破产=critical);expired 时仍须呈现最终态 |

**Output Range:** M=1→1.0s;M=3→0.84s;M=5→0.68s;M=10→0.5s(下界保护)。M=0→不进播放循环。追赶:**下一回合已开始**(主判据 `E_cur>E_action`)时过期动作跳过中间动画,critical 动作仍呈现最终态,不回灌框架。
**Example(正常 M=3,R-2 修订后):** 购地/建房/付税各 0.84s,共≈2.52s,三条均属同一 AI 回合(共享 `E_action`)。框架虽非阻塞已即时 EndTurn,但只要**下一玩家 `OnTurnStarted` 尚未到达**(`E_cur` 未越过 `E_action`),三条均**不**被判 expired→完整顺序播放(旧壁钟模型会在 t>2.0s 误判第3条;旧幻象 state_gen 模型在 per-action 解读下会误判前 2 条——R-2 均已修)。辅判据 T_stale=4.0s > 2.52s 队列总时长,亦不误伤。
**Example(真追赶):** 下一玩家回合开始(`OnTurnStarted` 到达使 `E_cur` 越过)/ 读档重建到更新代次 → 上一 AI 回合未播完的动作过期、跳过中间动画;其中 critical(如建房)仍呈现最终态 banner,非 critical(移动)整条跳过;直显最终态无残影,不回灌框架。

### F-5 通告 Beat 生命周期时序

`T_beat_total = T_slide_in + T_dwell + T_slide_out`
并发堆叠:`Y_offset(k) = k × (H_banner + GAP_banner)`(k=0 最新/最上)

**变量:**
| 变量 | 符号 | 类型 | 范围 | 描述 |
|------|------|------|------|------|
| 入场时长 | T_slide_in | float | **固定 200ms**(§7.5) | ease-out 上滑入场,不可调 |
| 停留时长 | T_dwell | float | 1.5~3.0s(推荐 2.0) | 可读停留,§7.5"延迟 2s 消失" |
| 出场时长 | T_slide_out | float | **固定 200ms**(§7.5) | ease-in 上滑消失,不可调 |
| 总生命周期 | T_beat_total | float | in+dwell+out | 单条完整生命周期 |
| 堆叠序号 | k | int | 0~N_max−1 | 并发队列堆叠序号 |
| banner 高 | H_banner | int | 40~60px(推荐 48) | 1080p 基准单条高度 |
| banner 间距 | GAP_banner | int | 4~8px(推荐 6) | 相邻 banner 间距 |
| 垂直偏移 | Y_offset(k) | int | 0~N_max×(H+GAP) | 第 k 条相对通告区顶部偏移 |
| 同屏上限 | N_max | int | 3~5(推荐 4) | 同时可见上限,超出最旧提前 fade-out |

**Output Range:** 单条 200+2000+200=**2400ms**;T_dwell=1.5s→1900ms;T_dwell=3.0s→3400ms;并发 k=3(H=48,GAP=6)→Y_offset=162px 最大,超出**驱逐最旧(k 最大,与 EC-8/AC-37 一致)**。
**驱逐期 k 槽规则(R-1 补,systems-designer 缺陷11):** 被驱逐 banner 进入 slide_out 时**立即释放 k 槽**(不再计入 k 编号),新 banner 入 k=0、其余 +1 重排;正在 slide_out 的旧 banner 走**独立 render 层**(不占 k 槽、不与新 banner Y_offset 重叠),其消散动画与新布局并行。避免"slot 已满但有条正在消失"的 Y_offset 重叠。
**Example(单条):** t=0 收 `OnBuildingAnnounced`→0~200ms 上滑入场→200~2200ms 停留 2s(读"玩家B在蓝色组建第2栋")→2200~2400ms 滑出→2400ms 结束。**Example(并发):** k=0 新建房 Y=0;k=1 前一收租 Y=54px 下偏一 slot,各自独立倒计时。

### 公式-约束遵从汇总

| 公式 | 约束来源 | 遵从方式 |
|------|---------|---------|
| F-1 | §7.5 max 1.5s | T_max≤1.5s,clamp 硬保证 |
| F-2 | §7.2 等宽字体 | 输出整数 + 字体层防跳动 |
| F-3 | player-turn L450 ≤100/≤500ms | 双参数作 FAIL 边界,不放宽 |
| F-4 | CR-11 R6 RETREAT 非阻塞 | expired 跳过,不回灌框架 |
| F-5 | §7.5 入场200/延迟2s | T_slide_in/out 固定不可调 |

## Edge Cases

> 注:HUD 为呈现层纯叶子,以下边界均为**呈现正确性**问题,绝不涉及游戏状态写入。`systems-designer` 未咨询(lean 模式),Pre-Production `/ux-design` 阶段复核。

- **EC-1 现金 counter 动画进行中又收到 `OnCashChanged`**:中断当前动画,以**当前显示值**为新 `OldCash`、新事件 `NewCash` 为目标,重启 F-1/F-2(不排队叠加、不丢目标值)。保证最终静止值始终等于最新权威 `NewCash`。
- **EC-2 收到 `OnCashChanged` 但 `NewCash==OldCash`(ΔCash=0)**:不播 counter 动画(F-1 退化 T_min 但视觉无变化),直接保持。注:经济5 `amount==0` 已早返不广播(economy AC-5),此为防御性处理。
- **EC-3 破产玩家面板后续仍收到其 `OnCashChanged`/`OnTurnStarted`**:面板处 Bankrupt 终态,**忽略**后续现金动画与高亮(不退出破产态);若误收到该玩家 `OnTurnStarted` 仅记录不高亮(权威轮转已由回合2 跳过破产玩家,此为防御)。
- **EC-4 同帧/紧邻收到 `OnTurnEnded` 后 `OnTurnStarted`(回合切换)**:**正常路径**(人驱动,`OnTurnStarted` 经正常回合流到达)严格串行——新玩家 handoff 高亮**等前一回合 outro 动画完成才起**(CR-8 不叠帧);Pass'N Play 本地轮流尤其严格(AC-24a)。**被迫切换路径**(**触发条件明确且仅限**:状态**重建**到新当前玩家——读档 EC-11 重建 / 框架快进重建——导致 outro 未完即须反映新当前玩家;**正常回合流的 `OnTurnStarted` 不属此列**):**丢弃 outro 余帧、直接起新高亮,状态正确优先于动画完整**(AC-24b)。两路径不矛盾:正常路径满足 CR-8 不叠帧不变式;被迫路径是 CR-8 显式登记的唯一例外(非违规)。判别口径:被迫 ⇔ 新当前玩家来自重建/快进而非顺序回合推进。
- **EC-5 AI 行动队列播放中下一回合已开始(过期)**:按 F-4 `expired(i)`(主判据 `E_cur>E_action`)判定,跳过过期中间动画、直接应用最终快照,**不回灌框架**(CR-11)。HUD 落后不阻塞游戏。
- **EC-6 玩家主动跳过/加速 AI 行动播放**:HUD 立即将队列剩余动作视为过期、直跳最终状态(等价 F-4 追赶路径)。这是呈现加速,不改变任何游戏结果。
- **EC-7 `OnBuildingAnnounced` 的 `ActingPlayerId` 恰为当前回合玩家**:**仍呈现**全员可见通告 beat(CR-12 义务是"无论是否其回合"——自己回合建房同样触发通告,保持一致性,不做特例抑制)。
- **EC-8 通告并发超过 `N_max`(雪崩)**:驱逐**最旧**(k 最大)通告,令其立即进入 fade-out(跳过剩余 dwell),新通告入 k=0;同屏永不超 N_max 条(F-5)。
- **EC-9 高亮 onset > 100ms(UI 线程阻塞)**:仍呈现高亮(不崩溃、不跳过),但此为 F-3 约束违约 = 对应 AC 的 **FAIL** 信号(性能问题,须 perf 优化,非 HUD 逻辑容错点)。
- **EC-10 `OnGameWon(winnerId)` 中 `winnerId` 为 `INDEX_NONE`(退化局 APC==0)**:呈现中性 game-over(无明确赢家),**不崩溃、不臆造赢家**;退化局检测/fallback 全归破产9(player-turn AC-20/bankruptcy AC-29),HUD 仅按收到的 winnerId 呈现。
- **EC-11 读档后(存档21)HUD 状态重建**:读档完成后 HUD 须**重绑全部事件 delegate 监听器** + 从快照**重读**所有显示值(现金/当前玩家/阶段/面板态)重建显示,**不回放**读档前已发生的动画(直接呈现最终态)。与存档21 协同(systems-index 存档行义务)。**⚠ BP/C++ 绑定路径区分(R-3,ue-umg):本项目 Blueprint 为主,Widget 若用 BP `Bind Event to [Delegate]` 节点绑定——其底层是 `FMulticastScriptDelegate::Add`(**非** `AddUnique`),重绑前不显式 `Unbind Event from` 解绑会**重复注册→重复触发**;而 C++ `AddDynamic`(宏展开为 `AddUnique`,按 UObject+FName 去重)无此风险。故重建路径**无论 BP 或 C++ 实现,均须先显式解绑再重绑**(BP 用 `Unbind`,C++ 用 `RemoveDynamic`),不可依赖"绑定天然幂等"。**实现期 code-review 须核验绑定机制**(AC-48)。
- **EC-12 玩家数边界(2~8)**:面板集布局须适配 MVP 本地 2~4、Full Vision 至 8 玩家;2 玩家时不留空位拉伸,8 玩家时面板紧凑但满足 §7.2 最小 10px 字号可读。超出 8 不支持(棋盘1/回合2 已限上界)。
- **EC-13 玩家色重复/缺失**:HUD 仅渲染**回合2/棋盘1 给定**的玩家色;若给定色缺失/重复,呈现 fallback 默认色 + 记录告警,不自行分配色(色分配非 HUD 职责)。
- **EC-14 juice 随机流被误接骰子3 权威流**:设计/实现层禁止——HUD juice 必走独立 `FRandomStream`(CR-16);若 code-review 发现复用骰子流即为缺陷(ai CR-5④ 重放污染)。

## Dependencies

> HUD 是**呈现层纯叶子**:所有依赖均为**单向读取/订阅**(HUD ←),无任何系统依赖 HUD(呈现层无下游)。双向一致性已核验:棋盘1(L14/165/304/315)、经济5(L327)、玩家回合2(Dependencies HUD 行)均回提 HUD(16)。

### 硬依赖(系统无法运作)

| 系统 | 依赖性质 | 接口/数据 |
|------|---------|-----------|
| **玩家回合(2)** | 硬 | 订阅 `OnTurnStarted` / `OnPhaseChanged` / `OnTurnEnded` / `OnTurnOrderResolved` / `OnAIActionExecuted` / `OnBuildingAnnounced` / `OnGameWon`;输入意图经回合2 公开接口转发(CR-14)。回合2 拥有全部广播 + 流程 |
| **经济(5)** | 硬 | 订阅 `OnCashChanged` / `OnRentPaid` / `OnBankruptcyDeclared`;查询 `GetCash`。经济5 拥结算 + 事件(economy L327/L377) |
| **棋盘(1)** | 硬 | `GetTileData(index)` 读 `DisplayName` / `ColorGroup` 色 / `PurchasePrice` / `RentTable` 供资产摘要与显示(board-data L165/L304);`SalaryAmount`(=200)。棋盘1 拥数据契约,UI 拥呈现 |

### 软/间接依赖(经回合2 中转,非直连)

| 系统 | 依赖性质 | 说明 |
|------|---------|------|
| **AI 对手(10)** | 软(间接) | AI 不自 emit;其行动经回合2 `OnAIActionExecuted(FAIActionDetails)` 中转,HUD 据此非阻塞呈现。HUD 不直接调 AI。约束:juice RNG 须独立流(ai CR-5④);**`FAIActionDetails` 须含 `EActionType`(R-2 propagate OQ-HUD-12)供 V-Own-07 文案 + F-4 critical 判定** |
| **破产胜负(9)** | 软(间接) | 胜负经回合2 `OnGameWon(winnerId)` 呈现;局末排名若显示用 `net_worth`(9-owned 只读口径)。HUD 不直接调9 |
| **骰子(3)** | 软 | 可读 `FDiceRollResult.Total` 显数值;**骰面 Die1/Die2 滚动呈现归 VFX19**,非 HUD |

### 呈现层边界(平级,非依赖关系)

| 系统 | 边界 |
|------|------|
| **游戏反馈 VFX(19)** | enable-not-own:HUD 拥信息显示布局 + UMG widget + 数据绑定;VFX 拥 juice 视觉资产/Niagara/世界空间金币飞溅。VFX **直接订阅经济/骰子事件**,不经 HUD 转发(tile-events L272)。互不依赖 |
| **地产卡与卡牌 UI(17)** | HUD 拥玩家面板/现金/回合/操作栏;**地产卡详情面板**归 #17。HUD 操作栏仅含地产卡快览入口 |

### 横切消费方(依赖 HUD 行为,非 HUD 依赖它)

| 系统 | 关系 |
|------|------|
| **存档/读档(21)** | 读档后 HUD 须重绑 delegate 监听器 + 从快照重读重建显示(EC-11)。存档21 序列化游戏状态,HUD 不被序列化(纯呈现,从快照重建) |

## Tuning Knobs

> 全部为**呈现层数据驱动旋钮**(designer 可调,无需改代码)。固定值(`T_ONSET_MAX`=100ms / `T_COMPLETE_MAX`=500ms / `T_slide_in`=`T_slide_out`=200ms)是 player-turn L450 / art-bible §7.5 硬约束,**非旋钮**,改动须同步源文档。视觉量(高亮 +15% 亮度 / scale 1.05 / 去饱和 60% / 4px 边框)归 **art-bible §7.4** 拥有,HUD 不复制为旋钮,引用源真值。

| 旋钮 | 来源 | 默认 | 安全范围 | 调太高 | 调太低 |
|------|------|------|---------|--------|--------|
| `K_dur` | F-1 | 0.55 | 0.3~0.7 | 现金动画拖沓 | 大额也一闪而过,缺获得感 |
| `T_min` | F-1 | 0.3s | 0.15~0.5s | 小额动画过久 | 小额闪过易漏看 |
| `T_max` | F-1 | 1.5s | 1.0~**1.5**s | **不得超 1.5s**(§7.5 硬线) | 大额缺戏剧性 |
| `E_pow` | F-2 | 2.0 | 1.0~3.0 | 末段过缓,数字停中途久 | 趋近线性,缺"稳稳落下"感 |
| `D_highlight` | F-3 | 300ms | 200~400ms | 逼近 500ms 完成上限,易违约 | 高亮太快缺仪式感 |
| `T_base` | F-4 | 1.0s | 0.8~1.5s | AI 行动整体拖沓 | AI 行动太快看不清 |
| `K_compress` | F-4 | 0.08s/条 | 0.05~0.15 | 长队列压缩过度(都触底 T_slot_min) | 长队列不提速,多动作回合冗长 |
| `T_slot_min` | F-4 | 0.5s | 0.4~0.6s | 长队列总时长偏长 | <0.4s 玩家读不懂 AI 做了啥 |
| `T_stale` | F-4 | **4.0s** | **3.5~6.0s** | 追赶迟钝,HUD 久落后于回合推进 | **<队列总时长(MVP M≤5≈3.4s)则正常路径末条被辅判据误判过期;故安全下界钉 3.5s(R-2:主判据已改 HUD 回合代次 `E_cur`,T_stale 仅软上界)** |
| `T_dwell` | F-5 | 2.0s | 1.5~3.0s | 通告堆积久占屏 | 阅读慢的玩家读不完 |
| `N_max` | F-5 | 4 | 3~5 | 通告雪崩占屏 | 频繁驱逐,漏看社交 beat |
| `H_banner` | F-5 | 48px | 40~60px | 占屏多 | <40px 可读性下降 |
| `GAP_banner` | F-5 | 6px | 4~8px | 通告区拉长 | 条目挤压难分辨 |
| `reduce_motion` | 全局 a11y(R-2) | Off | {Off,On} 布尔模式 | —(开=降幅/关非必要动画,见 AC-57) | —(关=保留全部超冲/滑动动画) | **非 F-1~F-5 内联数值旋钮**,布尔模式开关:On 时 counter **旁路 F-1 的 `T_min` 下界**(有效时长 ≤200ms,否则 T_min=0.3s 使 AC-57① 永不可 PASS)、高亮去超冲(见下 Ease-Out Cubic)、banner `T_slide_in`/`out` 可降至 0、AI 行动改 Instant、**操作栏阶段切换(V-Own-10)去动效**。具体降幅参数归 OQ-HUD-9 `/ux-design`;**存在性 + 参数抑制 = MVP 硬要求(AC-57 守)** |

### 旋钮交互(相互影响)

- **`T_max` ↔ `K_dur`**:`K_dur` 调大但 `T_max` 不变时,中大额金额提前触顶 1.5s,`K_dur` 对它们失效——大额区域由 `T_max` 主导。
- **`D_highlight` ↔ 固定阈值**:`D_highlight` 受 `T_COMPLETE_MAX`(500ms)− onset 约束;调至 >400ms 时若 onset 抖动易越 500ms 违约。
- **`K_compress` × `M` × `T_slot_min`**:三者共同决定长队列节奏;`K_compress` 调大到使 `T_base − K_compress×(M−1) < T_slot_min` 时,后续动作全部触底,`K_compress` 再调大无效。
- **`T_stale` ↔ 队列总时长(R-2 修订;R-3 补最坏旋钮角约束)**:expired 主判据已改为 **HUD 本地回合代次推进**(`E_cur>E_action`,见 F-4),T_stale 退为**辅判据软上界**(防 HUD 自身卡死)。软上界须 `≥ Σ T_slot(i,M)` 预期最大队列总时长。**⚠ 安全下界 3.5s 是按默认 `T_base=1.0` 算的(M=5 队列 ≈3.4s);但在合法旋钮最坏角(`T_base=1.5` 上界 / `K_compress=0.05` 下界 / M=5)队列总时长 = 5×clamp(1.5−0.05×4, 0.6, 1.5)=5×1.3=6.5s,超出 `T_stale` 安全范围上界 6.0s** → 此角下默认 T_stale=4.0s 会在正常路径末条因软上界误判过期(B-1 类回归)。**约束(R-3):调高 `T_base`/调低 `K_compress` 时,`T_stale` 须同步满足 `T_stale ≥ Σ T_slot(i, M_max)`(按实际配置算,非默认);或限定 `M_max` 随 `T_base` 收紧。** 此角降级温和(主判据 `E_cur` 仍是真守护,辅判据只是提前丢弃中间动画、critical 仍呈最终态),非击穿支柱②,故列 Recommended;实现期阈值冻结须校验此联动。`unpaused_elapsed` 用游戏时间(暂停不计、读档重置),不再用壁钟。
- **`T_dwell` ↔ `N_max`**:`T_dwell` 调大 + 通告频繁时更快撞 `N_max` 驱逐;二者共同决定通告区信息密度。

## Visual/Audio Requirements

> **边界声明**:本节严格遵守 enable-not-own 原则。HUD 拥有信息显示布局、UMG widget 结构、数据绑定及其状态视觉(面板/数字/高亮/通告 banner);juice 视觉资产(金币飞溅、Niagara 粒子、世界空间特效)归 **VFX(19)**,音效归 **音频(22)**。本节区分两类条目:HUD 拥有(V-Own)与 HUD 使能但不拥有(V-Enable)。

### 1. HUD 拥有的视觉需求

以下视觉均为 HUD 布局层、状态视觉、数字动效的规格,不涉及 juice 粒子资产。

#### V-Own-01 玩家面板基础样式
**依据**: art-bible §7.4(a)、§3.4、§4.3
- 每块面板为圆角矩形卡片,左侧 4px 纯色边框对应该玩家的指定玩家色(§5.2 玩家色表,P1 暖红 `#E03C31`…P8 棕色 `#7B3F00`)。
- 面板背景 Off White `#FAFAF5`,软阴影(blur ≥8px,偏移 ≤4px,阴影色为面板主色暗色版,§3.4)。
- 信息层级遵 §7.2:玩家名 Level 3(14–18px SemiBold)、现金数字 Level 2(20–28px SemiBold,Tabular Figures)、次要信息(地产数/资产摘要)Level 4(10–12px Regular,Dark Ink 60%)。
- 所有数字用 Tabular Figures 等宽——4 位变 5 位时面板宽度不变、不抖动(§7.2)。

#### V-Own-02 当前回合高亮态
**事件**: `OnTurnStarted(PlayerId, bIsAI)` | **依据**: §7.4(a)、F-3
- 激活面板:亮度 +15%、scale 1.05,投影加深(§7.4a)。曲线 **Ease-Out Cubic/Quart(R-2 改,去超冲——超冲与"不跳动"Submission 定位矛盾;弹性 juice 归 VFX19,非面板高亮自身)**,时长 D_highlight∈[200,400ms],onset≤100ms、完成≤500ms(F-3)。`reduce_motion=On` 时进一步去动效(瞬切或更短线性,AC-57)。
- 非激活面板恢复亮度与 scale 1.0,同曲线时长过渡。
- `bIsAI==true` 时面板内显"AI 思考中"标签(Level 4,Dark Ink 60%),随高亮态同步出现/消失,不用独立弹窗。

#### V-Own-03 现金 Counter 动画着色
**事件**: `OnCashChanged(Player, OldCash, NewCash, EChangeReason)` | **依据**: §4.3、§7.2、F-1、F-2
- 数字从 OldCash ease-out 插值滚动至 NewCash,时长由 F-1/F-2 决定(≤1.5s,§7.5)。
- 动画期按 `EChangeReason` 切语义色(§4.3):Salary/正向=Fortune Gold `#F4C430`;Rent/Tax/负向=Property Red `#E03C31`;其他中性=Dark Ink `#1A1A2E`。
- 颜色不单独编码信息——必须同时有数字增减方向冗余(§4.6,见第 3 节)。
- 动画完成数字恢复 Dark Ink;静止态永远 Dark Ink,不残留语义色。

#### V-Own-04 破产面板态
**事件**: `OnBankruptcyDeclared(Debtor, Creditor)` | **依据**: §7.4(a)、§4.3、§2 状态 G
- `Debtor` 面板进入终态:去饱和 60%(HSV 饱和度 ×0.4)+ 灰对角线纹理叠加(20–30% 不透明度)(§7.4a);面板不透明度降至 60%。
- 4px 玩家色左边框同步去饱和(保留位置,颜色变灰)——位置识别保留,状态语义清晰。
- 终态:不再响应 `OnTurnStarted` 高亮、不再播 counter 动画(EC-3)。过渡 Ease-Out 200–300ms 去饱和渐变,不跳切。

#### V-Own-05 回合切换过场动画(两分支)
**事件**: `OnTurnEnded(PlayerId, bGrantsExtraTurn)` | **依据**: §7.5、CR-7、F-3
- **分支 A 同玩家继续(`==true`)**:面板高亮停留不转移;面板内显"双点——继续!"链式 badge(Level 3,Fortune Gold 底,约 1s 后消失);高亮不闪烁/抖动。
- **分支 B 移交新玩家(`==false`)**:旧面板高亮 Ease-Out 退场后新面板高亮入场(CR-8:新起帧严格等旧 outro 完成,不叠帧)。
- 两分支视觉必须明显差异——玩家不应混淆"我继续"与"轮到下一个"。

#### V-Own-06 席位裁定顺序可见呈现
**事件**: `OnTurnOrderResolved(OrderedPlayerIds, bResolvedBySeatTiebreak)` | **依据**: §7.4(a)、CR-9、§4.3
- 顶部 HUD 栏或独立通告区**可见**呈现最终顺序(N1→N2→…,玩家色标签)。
- `bResolvedBySeatTiebreak==true` 时须有辅助文字"席位决定先手"(Level 4),不得静默(CR-9)。呈现持续至首个 `OnTurnStarted` 后自动消失,不阻塞流程。

#### V-Own-07 AI 行动逐步呈现
**事件**: `OnAIActionExecuted(FAIActionDetails{ActionIndex,EActionType,...})` | **依据**: CR-10、CR-11、F-4
- **HUD 专属区(非底部操作栏——守 UI Requirements「AI 行动区不挤占操作栏」MVP 硬约束,R-3 消歧:原"底部操作栏或"措辞与该硬约束矛盾,删除)** 按 `ActionIndex` 升序逐步呈现 AI 行动(文字+图标,如"AI 购入X地产");**文案/图标由 `EActionType` 派生**(`BuyProperty`/`BuildHouse`/`Mortgage`… 各映射文案模板与图标)——HUD 不从 `ActionIndex` 臆断动作类型(R-2,propagate OQ-HUD-12)。每条短暂高亮后淡出,节奏遵 F-4(T_slot 0.5–1.0s/条)。
- 非阻塞:呈现落后框架时过期条目跳过动画、直显最终快照(CR-11),不影响游戏状态。
- AI 行动文字中性 Dark Ink;涉资金变动时金额数字按 V-Own-03 语义色标注。

#### V-Own-08 建房通告 Banner
**事件**: `OnBuildingAnnounced(TileIndex, NewHouseCount, ActingPlayerId)` | **依据**: §7.5、F-5、CR-12、§4.3
- Banner 从屏幕上方 ease-out 上滑入场(200ms,§7.5),停留 2s,ease-in 上滑消失(200ms)。
- 内容:发起玩家色小圆点 + 玩家名 + 建房地产名 + 新建筑数("玩家B 在[地产名]建第N栋");文字 Level 3;背景 Off White 圆角,左 3px 发起玩家色线;不遮挡棋盘操作。
- 并发垂直堆叠(F-5:Y_offset=k×(48+6)px),最新在最上,同屏最多 N_max=4,超出驱逐最旧(EC-8)。
- 全员可见(**MVP 单屏=当前持屏者可见**,CR-12,恢复支柱②社交引信;多屏全桌同时感知 deferred OQ-HUD-5),无论当前活动玩家。

#### V-Own-09 胜利终局态
**事件**: `OnGameWon(winnerId)` | **依据**: §2 状态 F、§4.3
- HUD 呈现胜利终局覆盖层:胜者名 + 总资产(Level 1,Fortune Gold)。非胜者面板灰/低透明(幅度略浅于单个破产渐进式)。
- `winnerId==INDEX_NONE` 退化局:中性 game-over 文字,不臆造赢家(EC-10)。覆盖层粒子彩带/金色高光归 VFX19(见 V-Enable-01)。

#### V-Own-10 阶段指示 / 操作栏状态
**事件**: `OnPhaseChanged(OldPhase, NewPhase, ConsecutiveDoubles)` | **依据**: §7.1、§4.5、CR-6
- 底部操作栏掷骰按钮/操作组按当前 Phase 切 affordance:`RollPhase` 掷骰按钮激活(Fortune Gold 底,Dark Ink 文字,§4.5 主 CTA);非 `RollPhase` 禁用(去饱和灰,不透明度 50%,§4.5)。
- `ConsecutiveDoubles` 计数显于按钮旁或当前玩家信息区(Level 4),区分首次 RollPhase 与双点额外回合 RollPhase(CR-6)。切换过渡 **Ease-Out Cubic/Quart 200–300ms(R-3 改:去超冲,对齐 V-Own-02 面板高亮的同类 R-2 决策——Bounce 弹性与「不跳动」Submission 定位矛盾,弹性 juice 归 VFX19;R-2 去超冲漏传播至本操作栏切换,R-3 补)**,不跳切。`reduce_motion=On` 时进一步去动效(瞬切,AC-57⑤)。

### 2. HUD 使能但不拥有的 Juice(VFX19 / 音频22 事件锚点)

HUD 提供准确事件锚点与数值,实际视觉/音效资产由 VFX(19)/音频(22) 独立实现。**HUD 不向 VFX/音频推送额外信号——VFX/音频直接订阅各 owner 系统事件。**

| 编号 | 触发事件 | HUD 提供的锚点 | juice 资产归属 |
|------|---------|--------------|--------------|
| V-Enable-01 | `OnGameWon(winnerId)` | 胜者 id、覆盖层位置 | 彩带/烟花 confetti、金色高光爆炸 — VFX19 |
| V-Enable-02 | `OnRentPaid(Payer,Payee,Amount,TileIndex)` | TileIndex 世界坐标、Amount | 金币飞溅(抛物线 Niagara)— VFX19(CR-15) |
| V-Enable-03 | `OnBankruptcyDeclared(Debtor,Creditor)` | Debtor 棋子坐标 | 棋子倒下粒子消散、地产卡飞出 — VFX19 |
| V-Enable-04 | `OnBuildingAnnounced(...)` | TileIndex 坐标、新等级 | 建房落地弹性动效、星星粒子(§6.3) — VFX19 |
| V-Enable-05 | `OnTurnStarted(PlayerId,bIsAI)` | 激活棋子坐标 | 棋子 MyTurn 动画、rim light(§5.3) — VFX19/技术美术 |
| V-Enable-06 | `OnGameWon(winnerId)` | 胜者棋子坐标 | 棋子 Win 动画(旋转+上升+王冠,§5.3) — VFX19 |
| V-Enable-07 | `OnCashChanged` Salary | 发薪数值 | 金币入账音(正向 chime) — 音频22 |
| V-Enable-08 | `OnCashChanged` Rent/Tax | 扣款数值 | 支付音(钱币划走/负向) — 音频22 |
| V-Enable-09 | `OnTurnStarted` | 玩家身份 | 回合开始提示音(区分 AI/人类) — 音频22 |
| V-Enable-10 | `OnBuildingAnnounced` | House vs Hotel | 建房落地音(等级不同音色) — 音频22 |
| V-Enable-11 | `OnBankruptcyDeclared` | Debtor 身份 | 破产滑稽失败音(§2 状态 G) — 音频22 |
| V-Enable-12 | `OnGameWon` | 胜者身份 | 胜利号角/欢呼 stinger — 音频22 |

> VFX(19) 和音频(22) **直接订阅经济5/回合2 事件源**,不经 HUD 转发。HUD 不持有 VFX/音频引用或回调(enable-not-own 实现约定)。

### 3. 色盲安全 / 可读性
**依据**: §4.6、§7.2、§4.3

#### CA-01 语义色不可单独编码信息(§4.6 核心)
| HUD 场景 | 颜色信号 | 必须搭配的冗余信号 |
|---------|---------|----------------|
| 现金 counter 着色 | Gold/Red | 数字增减方向(±符号/箭头)+ `EChangeReason` 文字标签 |
| 回合高亮 | 亮度差异 | scale 1.05 尺寸差 + 玩家 P1–P8 编号标签恒显(§5.2) |
| 破产面板 | 灰去饱和 | 对角线纹理 + "破产"文字/骷髅×图标 |
| 掷骰按钮启用/禁用 | 金/灰 | 不透明度 50% + 图标实心↔空心骰子 |
| 回合切换分支 | 动画路径差异 | "双点——继续!"文字 badge,不依赖颜色 |
| 玩家色左边框 | 玩家专属色 | 玩家 P1–P8 圆形编号标签(§5.2) |

#### CA-02 红绿对最高风险区(§4.6 Deuteranopia)
- 现金动画红(扣)vs 金(收)非红/绿配对,风险低,仍须 ± 数值方向冗余。
- 确认(绿)用 `✓`、危险/拒绝(红)用 `✗`/`⚠`(§4.6);操作栏买入确认/拒绝遵此。
- P1 暖红 vs P3 饱和绿(§5.2 备注):并排时各加"1"/"3"编号标;HUD 任何分辨率不可省玩家编号标签。

#### CA-03 最小字号与紧凑约束(§7.2)
- 游戏内文字 ≥10px(1080p 基准)。EC-12:8 玩家紧凑布局现金可缩至 Level 3(14–18px),不低于 10px Level 4 底线。
- 通告 Banner(48px)内文字 Level 3。所有数字 Tabular Figures 等宽,宽度不随位数变化(§7.2)——布局稳定根基。

#### CA-04 UI QA 色盲检查清单(§4.6,Coblis/等效模拟 Deuteranopia/Protanopia)
1. 现金 counter 金 vs 红 色盲下可经数字方向区分;2. P1(红)vs P3(绿)面板可经编号标签区分;3. 掷骰按钮启用(金)/禁用(灰)可经不透明度+图标形状区分;4. 确认(绿)/危险(红)按钮可经 ✓/✗ 图标区分。

### 4. 音频锚点(归音频22)

HUD 仅持事件触发时刻与上下文;音效资产/SFX 文件/音量混响参数全归**音频(22)**。HUD 不持 Sound Cue 引用——音频22 直接订阅事件源。

| 编号 | 触发时刻 | HUD 提供上下文 | 音效需求 |
|------|---------|--------------|---------|
| A-01 | `OnTurnStarted(PlayerId,bIsAI)` | 玩家编号、是否 AI | 回合开始提示音:轻快短促(≤0.5s),AI 回合可略低沉区分,不打断 BGM |
| A-02 | `OnCashChanged`(Salary/正向) | ΔCash | 金币入账:明亮 chime,金额大则音调略高/更多层 |
| A-03 | `OnCashChanged`(Rent/Tax/负向) | ΔCash | 支付:钱币划走下行音调,金额大则更低沉 |
| A-04 | `OnBuildingAnnounced(...)` | NewHouseCount、是否酒店 | 建房:房屋咔哒落地;升酒店改厚重铿锵合并音(§6.3) |
| A-05 | `OnBankruptcyDeclared(Debtor,Creditor)` | Debtor | 破产:卡通滑稽失败音("哇哦"下行,§2 状态 G),喜感不悲怆 |
| A-06 | `OnGameWon(winnerId)` | 胜者 | 胜利 stinger:欢快号角/彩带音,≤3s 后渡入胜利 BGM 或 BGM 淡出 |
| A-07 | `OnTurnEnded(bGrantsExtraTurn==true)` | 双点链信号 | 双点继续音:短促清脆,与普通回合开始音音色差异 |
| A-08 | `OnPhaseChanged(NewPhase=RollPhase)` | Phase | 掷骰就绪提示音(可选):极轻微 UI 音效,可与 A-01 合并 |
| A-09 | `OnRentPaid(Amount,TileIndex)` | 金额、地块 | 收租落地音(与 A-03 分层:A-09 可为收方金币落袋音),与 VFX19 金币飞溅同步触发 |

> 所有音效音量平衡、混响分组(SFX/UI bus 分离)、平台适配由音频22 自主决策。每条锚点对应一个可独立关闭的 Sound Cue(`SC_` 前缀,§8.2)。

> **📌 Asset Spec Flag**:Visual/Audio 需求已定义。art-bible approved 后,运行 `/asset-spec system:hud` 从本节生成逐资产视觉描述、尺寸与生成 prompt。

## UI Requirements

> HUD 是项目核心 UI 系统。本节定义**屏上元素清单 + 信息架构 + 输入要求**(需求层);详细交互规格(布局尺寸、状态流转图、focus 顺序、手柄导航)归 Pre-Production `/ux-design`(见末尾 UX Flag)。

### HUD 贡献的屏上元素(WBP 候选)

| 元素 | 内容 | 始终可见? | 来源 CR |
|------|------|-----------|---------|
| **顶部 HUD 栏** | 玩家面板×N(现金/玩家色/资产摘要/高亮态/破产态)+ 回合信息 | 是 | CR-1/2/3/4/5 |
| **底部操作栏** | 掷骰按钮 + 当前玩家 + 操作组(买地/建房/抵押/赎回)+ 地产卡快览入口 | 是(按钮 affordance 随 Phase 变) | CR-1/14/CR-6 V-Own-10 |
| **回合/阶段指示** | 当前玩家 + 当前阶段 + ConsecutiveDoubles | 是 | CR-6 V-Own-10 |
| **通告区** | 建房通告 banner(堆叠≤4)+ 席位裁定顺序 | 上下文(事件触发) | CR-9/12 V-Own-06/08 |
| **AI 行动区** | AI 行动逐步呈现(文字+图标)+ "AI 思考中"指示 | 上下文(AI 回合) | CR-10 V-Own-02/07 |
| **胜利终局覆盖层** | 胜者 + 总资产 + 非胜者灰态 | 上下文(OnGameWon) | CR-13 V-Own-09 |

### 信息架构(可读性优先,支柱④)

- **永远可读**:每位玩家现金、当前轮到谁、当前阶段、自己的可用操作。这四项不得隐藏在 hover/二级菜单后(支柱④易上手 + Submission 放松)。
- **上下文呈现**:通告 beat、AI 行动、席位裁定、胜利态——事件驱动出现,自动消失,不常驻占屏。
- **层级**:核心数值用大字号 Level 1-2,辅助信息 Level 3-4(§7.2);通告与 AI 行动不遮挡棋盘操作区(≥65% 中央留给棋盘)。

### 输入要求

- **主交互鼠标点击**:掷骰、买地、建房、抵押、赎回均为按钮点击(CR-14 转发意图)。
- **禁止 hover-only**:所有信息与可用操作非 hover 即可读/可达(技术偏好,利后续手柄适配)。
- **按钮 affordance**:启用/禁用反映只读 Phase/资金状态(如非自己 RollPhase 掷骰禁用,V-Own-10);禁用态须色 + 不透明度 + 图标三重冗余(CA-01)。
- **手柄适配预留**:布局采用可聚焦导航结构(CommonUI 候选),具体导航顺序留 `/ux-design` + 架构期 ADR。

### 边界

- 地产卡**详情**面板(地块详情/租金表/建房选项明细)归 **#17 地产卡 UI**;HUD 操作栏仅含"快览入口"。
- 主菜单/房间大厅归 **#20**,非 HUD(HUD 是局内 UI)。
- **AI 行动区位置约束(R-1,ux-designer 1-B)**:AI 行动区**不得挤占"始终可见"的底部操作栏空间**(操作栏 affordance 须常在)——AI 行动区应为独立专属区(如棋盘上方浮层或侧栏),具体位置/尺寸归 OQ-HUD-6 `/ux-design`,但"不挤占操作栏"是 MVP 硬约束,非可延后。
- **缺失可读性信息归属(R-1,game-designer)**:**监狱状态**(owner=玩家回合2/事件7,HUD 待其暴露字段后在面板呈现,见 OQ-HUD-10)、**垄断组完成 / 抵押状态**(归 #17 地产卡 UI 详情域,HUD 面板"资产摘要"是否含摘要计数待 OQ-HUD-10)。**债务关系不补**——大富翁即时结算(付不出即破产),无持续债务账,系他游模型误投射。

> **📌 UX Flag — HUD(#16)**:本系统有 UI 需求。Pre-Production(Phase 4)须运行 `/ux-design` 为以下屏/HUD 元素各写 UX spec:**顶部 HUD 栏、底部操作栏、通告区、AI 行动区、胜利终局覆盖层**。引用 UI 的 story 须引 `design/ux/[screen].md`,**非直接引本 GDD**。建议在 systems-index 为 #16 标注此 UX-spec 待办。

## Acceptance Criteria

> **类型说明**:[Logic]=确定性可单测(headless 自动化,BLOCKING);[Integration]=多系统事件驱动(集成测试或文档化 playtest,BLOCKING);[Advisory]=纯视觉/feel 或 code-review 软约束(截图/签核,非阻塞门)。HUD 为呈现层纯叶子,所有 Logic 测的是**状态机转换/事件计数/payload 校验/公式计算**,非游戏逻辑本身。
> **R-1 诚实化裁定(qa-lead)**:**渲染层时刻**(高亮首帧 onset、banner 动画实录时序)在 `-nullrhi` 无渲染、且帧抖动使 ±10ms 级确定性断言不可行 → 拆为**逻辑状态机转换 [Logic]**(可 headless,如 AC-12/AC-24a/AC-36)+ **渲染侧时序 [Advisory]**(profiler/截图,如 AC-12b/AC-36b)。**负向断言**("不调某接口/不阻塞")须经 DI mock/spy 包裹能真违规的接口边界,否则 vacuous-pass(AC-26/29/40 已补 spy 锚点)。**code-review 软约束**(AC-47)诚实标 [Advisory] 非 [Logic]。

### A. 面板集与现金显示(CR-2 / CR-3 / F-1 / F-2 / EC-1 / EC-2 / EC-12)

- **AC-54** [Advisory](CR-1 三区布局比例)GIVEN 1920×1080;WHEN HUD 初始化;THEN 截图量测:中央棋盘区占屏 ≥65%,顶部/底部 HUD 栏不入侵棋盘区(无像素重叠)。证据 `production/qa/evidence/hud-layout-proportion.png`。
- **AC-1** [Logic](CR-2 面板数量×PlayerId 绑定)GIVEN N 名玩家(N∈{2,3,4})启动;WHEN HUD 初始化;THEN 精确渲染 N 块面板,各绑定唯一 `PlayerId`,无重复/无空位;N=2 无拉伸,N=4 均匀分布。
- **AC-2** [Logic](CR-2 Full Vision 8 人上界接口预留)GIVEN N=8 启动;WHEN 初始化;THEN 渲染 8 块面板不越界崩溃,PlayerId 绑定无误。(MVP 实际上界 4;8 人不崩是接口边界。)
- **AC-3** [Logic](CR-3 OnCashChanged 触发)GIVEN 面板正常态;WHEN 收到 `OnCashChanged(P,1000,1200,Salary)`;THEN 面板 P 现金从 1000 启动 counter,t=0 显 1000、结束精确 1200,其他面板不变。
- **AC-4** [Logic](CR-3/F-1 counter 双边界,R-1 由 [Integration] 改 [Logic];R-2 补下界)GIVEN 任意 ΔCash(含极大值);WHEN 调 F-1 计算 `T_counter`;THEN `T_min ≤ T_counter ≤ 1.5s`(**双边 clamp 硬保证**:上界防超 §7.5,下界防 `t/T_counter` 除零——T_min>0;ΔCash=0 时 `T_counter==T_min` 精确);**动画实际播放时长 ≤1.5s 的渲染侧验证归 [Advisory]**(profiler,与 AC-12b 同类)。
- **AC-5** [Logic](F-1 时长公式精度)GIVEN K_dur=0.55,T_min=0.3,T_max=1.5;WHEN 三变体:①|ΔCash|=200→≈1.267s ②=2000→=1.5s(clamp)③=30→≈0.820s;THEN 误差<1ms,②验 clamp。
- **AC-6** [Logic](F-2 插值起终点精确)GIVEN OldCash=1000,NewCash=1200,T=1.267s,E_pow=2.0;WHEN t=0/t=T/u=0.5;THEN t=0→1000、t=T→1200(精确)、u=0.5→1150(误差≤1)。
- **AC-7** [Logic](F-2 单调性防反向跳变)GIVEN 任意合法 Old<New;WHEN 逐帧推进;THEN displayed(t) 单调不递减;Old>New 时单调不递增(方向与 ΔCash 符号一致)。
- **AC-8** [Logic](EC-1 counter 中途重定向最新权威)GIVEN 面板 P 动画中(t=0.5s,displayed=1100);WHEN 再收 `OnCashChanged(P,1200,800,Rent)`;THEN 以当前显值 1100 为新 Old、800 为 New 重启,最终==800,不排队不丢目标。
- **AC-9** [Logic](EC-2 ΔCash=0 防御)GIVEN 面板显 1000;WHEN 收 `OnCashChanged(P,1000,1000,...)`;THEN 无动画,保持 1000,不崩溃。
- **AC-10** [Advisory](CR-3 等宽防跳动)GIVEN 4 位变 5 位(9999→10000);WHEN 跨边界;THEN 面板宽度恒定,无横向抖动。证据 `production/qa/evidence/hud-tabular-figures.png`。
- **AC-11** [Advisory](CR-3 语义色)GIVEN Salary/Rent;WHEN 动画期;THEN Salary≈Fortune Gold、Rent≈Property Red,结束归 Dark Ink。证据 `hud-cash-color.png`。

### B. 当前回合高亮(CR-4 / F-3 / EC-3 / player-turn AC-36 回链)

- **AC-12** [Logic·门控 OQ-HUD-2](CR-4 高亮**逻辑状态机转换**,headless 可测**前提 OQ-HUD-2 ADR 产出状态机抽独立 UObject,否则 UMG NativeTick 在 -nullrhi 不触发→false-green**,**回链 player-turn AC-36 / OQ-T-4**)GIVEN 已订阅 OnTurnStarted,InGame;WHEN t=0 广播 `OnTurnStarted(P,bIsAI=false)`;THEN 面板 P 逻辑态在 **≤1 游戏帧内**转 Active(状态机转换,spy 断言转换帧数==1),其余面板退 Normal。(渲染层 onset/complete 见 AC-12b。)
- **AC-12b** [Advisory](CR-4/F-3 **渲染侧时序**,profiler/帧分析证据)GIVEN AC-12 逻辑转换已发生;WHEN 录帧测量;THEN 高亮**首帧像素变化** `t_onset≤100ms`、视觉切换完成 `t_complete≤500ms`(player-turn L450 目标)。`-nullrhi` 无渲染不可 headless 断言,故为 Advisory 截图门;违约=perf 优化项(EC-9)。证据 `production/qa/evidence/hud-highlight-onset.png`。
- **AC-13** [Logic](CR-4 仅正确 PlayerId 高亮)GIVEN N=4,当前高亮 P1;WHEN 收 `OnTurnStarted(P2)`;THEN 仅 P2 进 Active,P1 退 Normal,P3/P4 不变,不多选。
- **AC-14** [Logic](CR-4 bIsAI=true 显标签)GIVEN 广播 `OnTurnStarted(P,bIsAI=true)`;WHEN 更新面板 P;THEN 显"AI 思考中"标签(Level 4),高亮态与 bIsAI=false 完全相同(onset/complete 约束均适用)。
- **AC-55** [Logic](F-3 D_highlight 双边界合规;R-2 补下界)GIVEN `D_highlight` 配置值(非 reduce_motion 模式);WHEN 读取参数;THEN `200ms ≤ D_highlight ≤ 400ms`(硬断言,违约=FAIL;上界推论:`complete = onset + D_highlight ≤ 100 + 400 = 500 ≤ T_COMPLETE_MAX`;下界=仪式感最小时长 F-3,防高亮太快缺被点名感)。`reduce_motion=On` 时下界豁免(可瞬切,AC-57)。
- **AC-15** [Logic](EC-3 破产面板忽略后续 OnTurnStarted)GIVEN 面板 P 已 Bankrupt 终态;WHEN 收 `OnTurnStarted(P)`(防御);THEN 保持 Bankrupt 不高亮不退出,静默吞收不崩溃。

### C. 破产面板态(CR-5 / EC-3)

- **AC-16** [Logic](CR-5 进入终态)GIVEN 面板 P 为 Normal/Active;WHEN 收 `OnBankruptcyDeclared(P,C)`;THEN P 切 Bankrupt(去饱和60%+灰对角纹),转为**终态**;之后 `OnCashChanged(P)`/`OnTurnStarted(P)` 均不触发 counter/高亮。
- **AC-17** [Logic](EC-3 破产 counter 不触发)GIVEN 面板 P Bankrupt 终态;WHEN 收 `OnCashChanged(P,500,0,Rent)`;THEN 无 counter,显值不变,不崩溃。
- **AC-18** [Advisory](CR-5 破产视觉)GIVEN `OnBankruptcyDeclared(P)`;WHEN 切换完成;THEN 截图验去饱和60%+灰对角纹+边框去饱和,Ease-Out 200–300ms 非跳切。证据 `hud-bankrupt-panel.png`。

### D. 阶段指示(CR-6 / EC-4 / player-turn AC-5c 回链)

- **AC-19** [Logic](CR-6 OnPhaseChanged 切换)GIVEN 已订阅;WHEN 收 `OnPhaseChanged(RollPhase,MovePhase,0)`;THEN 掷骰按钮切禁用,ConsecutiveDoubles 显 0,不崩溃。
- **AC-20** [Logic](CR-6 ConsecutiveDoubles 区分首次 vs 双点,**回链 player-turn AC-5c**)GIVEN 两变体:①`(...,RollPhase,CD=0)` ②`(...,RollPhase,CD=1)`;WHEN 处理;THEN ①显 0/不显计数 ②显 1 且有视觉区分(双点提示),两路 HUD 状态不相同(防误显两次开局)。
- **AC-21** [Advisory](CR-6 阶段过渡 feel)GIVEN 任意合法过渡;WHEN affordance 切换;THEN 验 Ease-Out Cubic/Quart 200–300ms(R-3:去超冲,对齐 V-Own-02/V-Own-10)非跳切,按钮三重冗余可区分。证据 `hud-phase-transition.png`。

### E. 回合切换动画(CR-7 / CR-8 / EC-4 / player-turn AC-7b / B-2 回链)

- **AC-22** [Logic](CR-7 bGrantsExtraTurn=true 分支,**回链 player-turn AC-7b**)GIVEN 面板 P Active;WHEN 收 `OnTurnEnded(P,true)`;THEN P 高亮保持不转移,显"双点——继续!"badge,不触发移交过场,其他面板不变。
- **AC-23** [Logic](CR-7 bGrantsExtraTurn=false 分支)GIVEN P1 Active、P2 Normal;WHEN 收 `OnTurnEnded(P1,false)` 完成 outro 后再收 `OnTurnStarted(P2)`;THEN P1 退 Normal(outro),P2 进 Active(入场),不叠帧(见 AC-24)。
- **AC-24a** [Logic·门控 OQ-HUD-2](CR-8 正常路径串行不叠帧,**回链 player-turn B-2**;headless 前提同 AC-12 依赖 OQ-HUD-2 ADR)GIVEN Pass'N Play 正常人驱动,P1 outro 进行中(`bP1OutroComplete==false`);WHEN 收 `OnTurnStarted(P2)` **经正常回合流到达**(非读档/快进重建,见 EC-4 判别口径,测试以 mock 标记事件来源=Sequential);THEN P2 Active 切换**入队等待**,P2 状态机不在当前帧切换,直至 `bP1OutroComplete==true` 后才切;断言无任何帧出现 P1/P2 同时 Active 或同帧高亮交叠。(纯逻辑状态机,headless 可测,不依赖渲染时刻。)
- **AC-24b** [Logic](EC-4 被迫切换路径)GIVEN P1 outro 进行中,**状态重建到新当前玩家**(mock 事件来源=Reconstruct:读档重建 / 框架快进重建);WHEN HUD 据重建状态须反映 P2 为当前;THEN P1 outro 余帧被丢弃、P1 退 Normal、P2 起 Active,任一帧无两面板同 Active;终态正确(当前玩家==P2)。(状态正确优先动画完整;此为 CR-8 登记的唯一例外,非违规。两路径以事件来源标记隔离,测试可干净构造。)
- **AC-25** [Advisory](CR-7 两分支视觉差异)GIVEN 两分支各播一次;WHEN 观察;THEN "双点继续"badge 与移交过场有明显差异,玩家不混淆。证据 `hud-turn-ended-branches.png`。

### F. 席位裁定可见(CR-9 / player-turn AC-41 回链)

- **AC-26** [Integration](CR-9 可见呈现,**回链 player-turn AC-41①**)GIVEN 定序完成 + HUD 经 DI 注入回合2 mock;WHEN 收 `OnTurnOrderResolved([P2,P0,P1,P3],false)`;THEN 可见呈现最终顺序(玩家色标签)不静默,首个 OnTurnStarted 后自动消失;**spy 断言 HUD 不调用回合2 的任何状态写入/流程推进接口(命名黑名单:`AdvanceTurn`/`EndTurn`/`SetCurrentPlayer` 等),只读查询(`GetCurrentPlayer` 等)不计入**(R-2 精化:消"任何调用==0"误把合法只读当违规的假阳性)。
- **AC-27** [Logic](CR-9 tiebreak 不静默,**回链 player-turn AC-41②**)GIVEN `bResolvedBySeatTiebreak=true`;WHEN 收事件;THEN 呈现含"席位决定先手"文字(Level 4),不得静默(player-turn L108/L451)。
- **AC-27a** [Advisory](CR-9 席位顺序 feel;原 AC-53,R-1 重排消乱序)GIVEN N=4 收到事件;WHEN 呈现;THEN 验 N1→N4 顺序含玩家色标签可辨,tiebreak 时辅助文字可读。证据 `production/qa/evidence/hud-turn-order.png`。

### G. AI 行动逐步播放(CR-10 / CR-11 / F-4 / EC-5 / EC-6 / player-turn AC-44 回链)

- **AC-28** [Logic](CR-10 按 ActionIndex 升序非阻塞,**回链 player-turn AC-44①**)GIVEN AI 回合发 M=3 条 `OnAIActionExecuted`(Index=0,1,2);WHEN 逐条处理;THEN 队列按 0→1→2 升序播放,各有可见呈现,全程不阻塞框架(HUD 不调回调回框架、不等播完才移交)。
- **AC-29** [Logic](CR-11 R6 RETREAT 负向:不 gating、过期跳过不回灌)GIVEN ActionIndex=1 动画播放中 + HUD 经 DI 注入**呈现→框架边界 spy**(监控 HUD 对框架方向的**任何**推进/回放完成调用,非仅单一命名回调,防换接口绕过);WHEN 下一回合 `OnTurnStarted` 到达(`E_cur` 越过 action 1 的 `E_action`,主判据 expired);THEN 判 expired、立即应用快照丢剩余帧(critical 动作仍呈最终态,AC-56),**spy 记录的对框架任何回放完成/推进回调次数==0**(断言"无回调"可 FAIL),框架未受阻塞,不崩溃。(HUD→框架方向不存在 `OnAIActionPlaybackSegmentComplete`/任何回放完成回调。)
- **AC-30** [Logic·门控 OQ-HUD-2](F-4 过期判定:回合推进主判据 + 软上界辅判据;`E_cur` 由 mock `OnTurnStarted` 驱动、`unpaused_elapsed` 走可注入 `IGameClock`,headless 前提依赖 OQ-HUD-2 ADR 产出 IGameClock DI + 状态机独立 UObject)GIVEN T_stale=4.0s;WHEN 四变体:① `E_cur` 未越过 `E_action` ∧ unpaused_elapsed=2.5s → **未过期**(主、辅判据均未触发)→ 播;② mock 注入下一回合 `OnTurnStarted` 使 `E_cur` 越过 → **过期**(主判据)→ 跳;③ 回合未推进但 unpaused_elapsed=4.5s>T_stale → **过期**(辅判据软上界)→ 跳;④ **读档重建后 `unpaused_elapsed` 重置 0**(模拟 EC-11)→ 旧累积时长不致误判;THEN 四变体判定正确,**正常 M=3 同回合队列总时长 2.52s 下第3条不被误判**(回归守护 B-1)。**暂停 30s 后** unpaused_elapsed 不计暂停 → 不误判过期(壁钟回归守护)。
- **AC-31** [Logic](F-4 T_slot 压缩精度)GIVEN T_base=1.0,K_compress=0.08,T_slot_min=0.5;WHEN 三变体:①M=1→1.0s ②M=3→0.84s ③M=10→0.5s(触底);THEN 误差<1ms,③验 clamp。
- **AC-32** [Logic](EC-6 玩家加速=追赶路径)GIVEN 玩家触发跳过,队列 Index=0,1,2 待播;WHEN 加速生效;THEN 全部视为 expired 直跳最终快照,不改游戏结果,不回调框架。
- **AC-56** [Logic](F-4 critical 动作 expired 不静默,**守支柱②,B-1 ③;依赖 `EActionType` payload——OQ-HUD-12 ✅RESOLVED,player-turn `FAIActionDetails` 已扩**)GIVEN mock `OnAIActionExecuted` 队列含 `EActionType∈{BuyProperty,BuildHouse}`×N_crit + `EActionType=Move`×N_move,全部被判 expired(`E_cur` 越过)+ HUD 经 DI 注入呈现层 spy;WHEN 追赶跳过;THEN **critical 动作仍呈现最终态**——spy 断言 HUD 的"critical 最终态呈现接口"(如 `ShowCriticalFinalState(EActionType)`)被调用恰 N_crit 次、`Move` 调用 0 次(criticality 由 `EActionType` 派生,非 HUD 臆断 `ActionIndex`);玩家不错过对手关键行动。
- **AC-58** [Advisory·code-review](OQ-HUD-12 `FAIActionDetails` 字段变更防 BP 引脚静默断线,R-3——原仅 OQ-HUD-12 末注无 AC 守护)GIVEN `FAIActionDetails` struct 字段发生任何变更(增/改名/改类型);WHEN 实现期代码变更提交;THEN reviewer 须打开所有绑定 `OnAIActionExecuted` 的下游 BP 图,确认 `EActionType`/`ActionIndex` 引脚无灰色断线(UE 按 pin 名称匹配,字段改名致引脚静默断线、runtime 读默认值如 `EActionType=0` 误判为 Move,**不报编译错**,见 player-turn L264-265)——任一灰色引脚视为 S2 缺陷。证据:reviewer 签字 + commit hash。
- **AC-33** [Advisory](CR-10 AI 行动 feel)GIVEN M=3(购地/建房/付税);WHEN 非阻塞播放;THEN 验各条文字+图标、节奏 0.5–1.0s/条可读清楚,涉资金着语义色。证据 `hud-ai-action-display.png`。

### H. 建房通告 Beat(CR-12 / F-5 / EC-7 / EC-8 / building OQ-Build-6 回链)

- **AC-34** [Logic](CR-12 全员可见无论是否其回合,**回链 building OQ-Build-6**)GIVEN 当前玩家 P1,广播 `OnBuildingAnnounced(5,2,P2)`(非当前玩家建房);WHEN 处理;THEN 通告 banner 全员可见出现,不因 ActingPlayerId≠当前玩家而抑制。
- **AC-35** [Logic](EC-7 自己回合建房同样触发不抑制)GIVEN 当前 P1,广播 `OnBuildingAnnounced(5,1,P1)`(自己回合);WHEN 处理;THEN banner 同样出现(CR-12 无例外),行为同 AC-34。
- **AC-36** [Logic](F-5 总时长公式)GIVEN T_slide_in=200/T_dwell=2000/T_slide_out=200;WHEN 调 `T_beat_total` 计算;THEN `T_beat_total==2400ms`(纯公式无帧依赖,headless 可测)。
- **AC-36c** [Advisory·code-review](F-5 固定参数不可改——R-2 由 AC-36 后半拆出)GIVEN HUD 实现提交;WHEN reviewer 静态扫描;THEN 确认 `T_slide_in`/`T_slide_out` 为固定常量(不暴露为可调 knob)。**BP 无语言级 const 语义** → code-review 软约束(同 AC-47);若架构期 ADR 选 C++ `constexpr`,可升 [Logic] `static_assert`(随 OQ-HUD-2/3)。证据:reviewer 签字 + commit hash。
- **AC-36b** [Advisory](F-5 实录入场动画时序)GIVEN 录帧工具;WHEN 播单条 banner;THEN 实录入场完成约 200ms、总时长约 2400ms(实际帧容差 ±20ms,非 headless 断言;UMG 动画系统帧抖动使 ±10ms 级确定性断言不可行)。证据 `production/qa/evidence/hud-banner-timing.png`。
- **AC-37** [Logic](F-5 N_max 驱逐最旧)GIVEN N_max=4,已 4 条在屏(k=0..3);WHEN 第 5 条到达;THEN k=3(最旧)立即 fade-out(跳剩余 dwell),新 banner 入 k=0,同屏永不超 4,k 编号正确重排。
- **AC-38** [Logic](F-5 Y_offset 精度,H=48/GAP=6)GIVEN k=0..3 同屏;WHEN 计算 Y 偏移;THEN k=0→0、k=1→54、k=2→108、k=3→162px,无 off-by-one。
- **AC-39** [Advisory](CR-12 banner 视觉,**回链 building OQ-Build-6 履行证据**)GIVEN `OnBuildingAnnounced`;WHEN 入场;THEN 验 ease-out 上滑 200ms+停 2s 可读+ease-in 滑出、左 3px 玩家色线、不遮挡棋盘。证据 `hud-building-announce.png`。

### I. 胜利态(CR-13 / EC-10 / player-turn AC-40a 回链)

- **AC-40** [Logic](CR-13 不独立判胜只呈现)GIVEN 收 `OnGameWon(P1)` + HUD 经 DI 注入破产9/回合2 接口 mock;WHEN 处理;THEN 呈现胜利覆盖层(胜者名+总资产),**mock 断言对破产9/回合2 的胜负确认接口调用次数==0**(spy 验空,口径完全由 payload 驱动),非胜者面板变灰。
- **AC-41** [Logic](EC-10 winnerId==INDEX_NONE 中性,**回链 player-turn AC-40a②**)GIVEN 收 `OnGameWon(INDEX_NONE)`(退化局);WHEN 处理;THEN 呈现中性 game-over(无赢家文字),不崩溃,不臆造赢家,退化局检测全归破产9。
- **AC-42** [Advisory](CR-13 胜利视觉)GIVEN `OnGameWon(P1)`;WHEN 覆盖层完成;THEN 验胜者名+总资产 Level 1 Fortune Gold、非胜者灰、彩带/高光归 VFX19。证据 `hud-victory-screen.png`。

### J. 输入意图路由(CR-14)

- **AC-43** [Logic](CR-14 不自行裁决状态)GIVEN 玩家点"买地"(ResolvePhase 合法)+ **HUD 经 DI 注入回合2/经济5 mock**;WHEN 按钮触发;THEN 仅向回合2 公开接口发"买地意图"(单次),**spy 断言无对回合2/经济5 任何写接口(状态变更/结算)的调用**(R-2 精化:点明注入哪些系统 mock + 监控写接口边界,否则负向断言 vacuous;路由意图是唯一允许的调用)。
- **AC-44** [Logic](CR-14 非自己 RollPhase 掷骰禁用=只读 affordance)GIVEN 当前 P1,P2 非活动;WHEN 访问 P2 掷骰按钮;THEN 禁用态(去饱和/半透明/图标变形),P2 不发掷骰意图,禁用由只读 Phase 驱动不写状态。
- **AC-45** [Logic](CR-14 禁 hover-only)GIVEN 鼠标不悬停(或手柄导航);WHEN 检查所有可用操作;THEN 掷骰/买地/建房/抵押/赎回 启用禁用态非 hover 即可读/可达,无信息仅 hover 显示。

### K. VFX 边界 / juice RNG 隔离(CR-15 / CR-16 / ai-opponent AC-61 回链)

- **AC-46** [Logic](CR-15 VFX 不经 HUD 转发)GIVEN 广播 `OnRentPaid(P1,P2,200,5)`;WHEN 处理;THEN HUD 仅显收租数值(可选),金币飞溅 juice 不由 HUD 触发,spy 断言无向 VFX19 推送调用(VFX 直订经济5)。
- **AC-47** [Advisory·code-review](CR-16 juice RNG 独立流,**回链 ai-opponent AC-61**)GIVEN HUD 实现提交 code-review;WHEN reviewer 静态扫描 HUD juice 随机调用路径;THEN 确认所有随机源自 HUD 自建独立 `FRandomStream`,无对骰子3 权威 RNG 流实例的引用/别名(防重放污染);复用即升 S2 缺陷。证据 `production/qa/evidence/hud-rng-isolation-review.md`(reviewer 签字 + commit hash)。**注:[Advisory] 因 BP 环境无语言级拦截、code-review≠自动单测(诚实降级);若架构期 ADR 选 C++ 强封装(私有 FRandomStream + DI spy),可升 [Logic] 静态断言(OQ-HUD-3)。**

### L. 读档重建(EC-11)

- **AC-48** [Integration](EC-11 重绑监听器+快照重建)GIVEN 进行中(P1 Active、P2 Bankrupt、现金各异);WHEN 读档完成 HUD 重建;THEN **先显式解绑旧 delegate 再重绑全部**(防旧 Widget 实例 dangling 回调/重复触发)——**C++ 路径用 `RemoveDynamic`,BP `Bind Event` 路径用 `Unbind Event from`(R-3:BP 绑定非 `AddUnique`,不解绑会重复注册,见 EC-11)**;从快照重读所有面板值直接呈现最终态,不回放历史动画,`E_cur` 重置为重建代次;重建后再收正常事件面板正常响应,**spy 断言旧 Handler 不被重复调用**(R-2,ue-umg R-2)。**测试须覆盖 BP 绑定路径的重复注册场景**(否则 mock 仅建模 C++ AddUnique 路径会漏掉 BP 双绑,R-3)。

### M. 色盲安全(CA-01..04)

- **AC-49** [Advisory](CA-01 现金方向冗余)GIVEN Salary/Rent 两场景;WHEN 动画;THEN Coblis 模拟(Deuteranopia/Protanopia)金 vs 红可经 ±数字方向区分。证据 `hud-colorblind-cash.png`。
- **AC-50** [Advisory](CA-01 玩家编号标签恒显)GIVEN N=4,P1(红)P3(绿)并排;WHEN 任意态;THEN 验各面板 P1..P4 圆形编号标签任意分辨率可见,色盲滤镜下编号足以区分 P1/P3。证据 `hud-colorblind-players.png`。
- **AC-57** [Logic·门控 OQ-HUD-2](reduce-motion 存在性 + 参数抑制 = MVP a11y 硬要求,**守 Submission/前庭安全 WCAG 2.3.3,R-2 blocker③**;**R-3 修订:谓词由"渲染层视觉"改为"动画有效配置参数读回",消除原②"scale 不越 1.05"的永真陷阱——Cubic 本无超冲,该谓词不测任何东西;并补 counter 的 T_min 旁路使①可达**)GIVEN `reduce_motion` 模式开关存在且置 `On`;WHEN 查询各动画的**有效配置参数**(非渲染输出);THEN 断言:① counter **旁路 F-1**——`reduce_motion=On` 时有效时长不受 `T_min` 下界约束、被覆盖为瞬切(0)或 ≤200ms 线性,断言读回的有效 counter 时长 ≤200ms(**否则 `T_min=0.3s` 默认使 `T_counter≥0.3s>200ms`,①永不可 PASS——R-3 修复:reduce_motion=On 旁路 T_min**);② 高亮 `D_highlight` 有效值被覆盖为瞬切或 ≤200ms 线性、**缓动曲线在 [0,1] 上单调不超出终值(无超冲采样)**——断言"曲线无 overshoot 段",**非**检查恒为 1.05 的终值本身(原永真谓词);③ banner `T_slide_in`/`T_slide_out` 有效值 == 0(瞬现/瞬隐);④ AI 行动有效呈现模式 == Instant;⑤ **操作栏阶段切换(V-Own-10)有效曲线去动效**(瞬切;补 R-3 V-Own-10 Bounce→Cubic 后的 reduce-motion 覆盖)。**本 AC 守"开关存在 + On 时各动画有效配置参数被抑制"的逻辑可测契约**(读**配置参数** headless 可断言、不依赖渲染输出;具体降幅数值由 OQ-HUD-9 `/ux-design` 填充,但"存在性 + 参数被覆盖为抑制值"为 MVP 硬门)。**渲染侧实录(无超冲视觉确认)归 [Advisory] 截图(覆盖缺口#6)**;本 AC 的"有效配置可读回"前提同 AC-12/24a/30 依赖 **OQ-HUD-2 ADR**(动画驱动/状态机抽独立 UObject + IGameClock DI,否则 UMG `NativeTick` 下配置读回 false-green)。

### N. 玩家数边界(EC-12)

- **AC-51** [Logic](EC-12 N=2 最小局)GIVEN N=2;WHEN 初始化;THEN 精确 2 块面板无空位拉伸,正常响应 OnCashChanged/OnTurnStarted,不崩溃。

### O. 玩家色缺失(EC-13)

- **AC-52** [Logic](EC-13 fallback 不自行分配色)GIVEN 给定玩家色 null/重复;WHEN 渲染;THEN 用 fallback 默认色(不自分配唯一色),记录告警,不崩溃,不修改色分配数据。

---

### 覆盖矩阵

> **R-1 修订(2026-06-05 design-review):** 渲染层时刻拆逻辑/渲染(AC-12→AC-12+12b、AC-24→AC-24a+24b、AC-36→AC-36+36b);AC-47 降 [Advisory·code-review];新增 AC-54(布局)/AC-55(D_highlight≤400)/AC-56(critical 动作);AC-53→AC-27a(消乱序);AC-26/29/40 补 spy 锚点。
> **R-3 修订(2026-06-05,finishing-class):** AC-57 重写(永真谓词→无超冲采样、补 T_min 旁路、加⑤操作栏)；AC-12/24a/30/57 标 `[Logic·门控 OQ-HUD-2]`；V-Own-10/AC-21 Bounce→Cubic；AC-48 补 BP 双绑覆盖；新增 AC-58(FAIActionDetails BP 引脚守护,Adv·code-review)。下方各行 AC-12/24a/30 的 Logic 标签均隐含 OQ-HUD-2 门控(见汇总表)。

| 需求来源 | AC 号 | 类型 |
|---|---|---|
| CR-1 三区布局 | AC-54 | Adv |
| CR-2 面板集 | AC-1, AC-2 | Logic |
| CR-3 现金+counter+着色 | AC-3..9 (Logic) / AC-10,11 (Adv) | |
| CR-4 高亮(逻辑/渲染拆分) | AC-12(Logic 状态机) / AC-13, AC-14, AC-55(Logic) / **AC-12b(Adv 渲染时序)** | Logic/Adv |
| CR-5 破产终态 | AC-16, AC-17 / AC-18(Adv) | Logic |
| CR-6 阶段/ConsecutiveDoubles | AC-19, AC-20 / AC-21(Adv) | Logic |
| CR-7 两分支动画 | AC-22, AC-23 / AC-25(Adv) | Logic |
| CR-8 Pass'N Play 不叠帧 | AC-24a(正常) / AC-24b(被迫) | Logic |
| CR-9 席位不静默 | AC-26, AC-27 / AC-27a(Adv) | Integration/Logic |
| CR-10 AI 升序非阻塞 | AC-28 | Logic |
| CR-11 R6 RETREAT 负向 | AC-29(spy) | Logic |
| CR-12 建房通告(单屏=持屏者) | AC-34, AC-35 / AC-39(Adv) | Logic |
| CR-13 胜利/不判胜 | AC-40(spy), AC-41 / AC-42(Adv) | Logic |
| CR-14 输入路由/禁 hover | AC-43, AC-44, AC-45 | Logic |
| CR-15 VFX 不经 HUD | AC-46 | Logic |
| CR-16 RNG 独立流 | AC-47 | **Adv·code-review** |
| F-1 时长 clamp | AC-4, AC-5 | Integration/Logic |
| F-2 插值精确 + 不变式 | AC-6, AC-7 | Logic |
| F-3 高亮时序 + D≤400 | AC-12(逻辑) / AC-12b(渲染 Adv) / AC-55(D≤400) | Logic/Adv |
| F-4 节奏+expired(**回合代次 `E_cur`**)+critical | AC-29, AC-30, AC-31, AC-56 | Logic |
| F-5 通告时序+驱逐 | AC-36(公式), AC-37, AC-38 / AC-36b(实录 Adv) / AC-36c(const·code-review) | Logic/Adv |
| **reduce-motion a11y(R-2;R-3 重写谓词+T_min 旁路)** | AC-57 | Logic·门控 OQ-HUD-2 |
| EC-1 重定向 | AC-8 | Logic |
| EC-3 破产忽略后续 | AC-15, AC-17 | Logic |
| EC-4 被迫切换 | AC-24b | Logic |
| EC-6 加速 | AC-32(状态正确;触发形式 OQ-HUD-9) | Logic |
| EC-10 winnerId NONE | AC-41 | Logic |
| EC-11 读档重建 | AC-48 | Integration |
| EC-12 N=2/N=8 | AC-51 / AC-2 | Logic |
| EC-13 色缺失 | AC-52 | Logic |
| **回链 player-turn AC-36**(OQ-T-4) | AC-12(逻辑) + AC-12b(渲染) | Logic/Adv |
| **回链 player-turn AC-41**(席位) | AC-26, AC-27 | Integration/Logic |
| **回链 player-turn AC-44**(非阻塞 AI) | AC-28, AC-29 | Logic |
| **回链 player-turn AC-7b**(bGrantsExtraTurn) | AC-22, AC-23 | Logic |
| **回链 player-turn AC-5c**(ConsecutiveDoubles) | AC-20 | Logic |
| **回链 building OQ-Build-6**(建房通告) | AC-34, AC-35, AC-39 | Logic/Adv |
| **回链 ai-opponent AC-61**(权威 RNG 隔离) | AC-47 | Adv·code-review |
| **支柱② critical 动作守护(R-1)** | AC-56 | Logic |
| **FAIActionDetails BP 引脚静默断线守护(R-3)** | AC-58 | Adv·code-review |

### BLOCKING / Advisory 汇总(R-3 修订后,共 62 条 AC)

- **[Logic] BLOCKING(43)**:AC-1,2,3,4,5,6,7,8,9,12,13,14,15,16,17,19,20,22,23,24a,24b,27,28,29,30,31,32,34,35,36,37,38,40,41,43,44,45,46,51,52,55,56,**57**
  - **其中 ADR 门控(R-3 标到条目)**:**AC-12 / AC-24a / AC-30 / AC-57** 标 `[Logic·门控 OQ-HUD-2]`——headless 可跑**前提**为 OQ-HUD-2 ADR 产出"状态机/动画驱动抽独立 UObject + IGameClock DI";ADR 未产出前,实现期勿按裸 headless 写测试(否则 UMG `NativeTick` 在 `-nullrhi` 不触发→false-green)。
- **[Integration] BLOCKING(2)**:AC-26(席位呈现,spy)/ AC-48(读档重建,**R-3 补 BP `Bind Event` 双绑测试覆盖**)。
- **[Advisory](17)**:AC-10,11,12b,18,21,25,27a,33,36b,**36c(code-review)**,39,42,47(code-review),49,50,54,**58(code-review·BP 引脚守护)**。
- **R-2 变更**:新增 AC-57;AC-36 拆出 AC-36c;AC-4/AC-55 补下界;AC-29/30 spy 范围精化 + 改 HUD 回合代次 `E_cur`;AC-56 依赖 `EActionType`(OQ-HUD-12 ✅RESOLVED)。
- **R-3 变更(finishing-class,5 must-land + Recommended 收口)**:① **AC-57 重写**——原②"scale 不越 1.05"永真谓词改为"缓动曲线无超冲采样"、①补 `reduce_motion=On` 旁路 F-1 `T_min`(否则 T_min=0.3s 使①永不可 PASS)、加⑤操作栏切换去动效;② **V-Own-10/AC-21 Ease-Out Bounce→Cubic**(R-2 去超冲漏传播至操作栏切换);③ **AC-12/24a/30/57 ADR 门控标签上条目**(原仅藏覆盖缺口#8);④ **V-Own-07 删"底部操作栏或"**(与 UI-Req"不挤占操作栏"矛盾);⑤ **EC-11/AC-48 BP/C++ 绑定去重区分**(BP `Bind Event` 非 AddUnique,重绑须显式解绑)。+ Recommended:F-4 T_stale 最坏旋钮角约束、E_cur 额外回合引 player-turn L197、critical 集合 Mortgage/Unmortgage exclude-with-justification、CR-3 tnum 5.7 核验标注、F-2 PlaybackSpeed 排除理由精化、**新增 AC-58**(FAIActionDetails BP 引脚静默断线守护)、OQ-HUD-8 net_worth fallback 安全前提。
- **诚实说明**:渲染时刻(AC-12b/36b/36c/AC-4 实录侧/AC-57 渲染确认)全归 Advisory;[Logic] BLOCKING 项均为可 headless 的状态机/公式/spy 断言(**ADR 门控四项 AC-12/24a/30/57 前提见上,余项无门控**)。

### 覆盖缺口(诚实标注,R-2 更新)

1. **EC-6 加速触发 UI 形式**:AC-32 验结果状态正确,触发形式(按键/点击)留 OQ-HUD-9 `/ux-design`——该 AC 实现依赖 OQ-HUD-9 解出,sprint 规划须登记"待解冻"。**且触发形式须不绕过 handoff outro 窗口(防 EC-6 全屏点击加速误触 AC-24a 不叠帧)**。
2. **CR-16 / AC-47 RNG 隔离**:BP 无语言级拦截 = code-review 软约束(已诚实标 [Advisory]);架构选 C++ 强封装可升 [Logic](OQ-HUD-3)。
3. **CR-12 单屏社交 beat 时序**:MVP 单屏 = 持屏者可见(AC-34/35 验逻辑触发);**Pass'N Play 下下一玩家看不到对手建房 banner——补偿机制归 OQ-HUD-13**;多屏/在线全桌同时感知归集成 playtest(OQ-HUD-5)。
4. **V-Own 视觉规格 + 渲染时序**:V-Own-01..10 + AC-12b/36b 渲染时刻无法纯自动断言(像素/动画曲线/帧时刻)——HUD 呈现层诚实局限,截图+lead 签核。
5. **"AI 思考中"标签生命周期**:AC-14 验出现,消失由 AC-22/23 回合结束路径覆盖。
6. **a11y(R-3 更新)**:**reduce-motion 存在性 + "On 时各动画有效配置参数被抑制"已升 MVP 硬要求(AC-57 守——R-3 重写谓词为配置参数读回、消除原"scale 不越 1.05"永真陷阱、补 T_min 旁路;`reduce_motion` 旋钮登记)**;**AC-57 渲染侧无超冲视觉确认归 [Advisory] 截图(headless 仅断言配置参数,不断言渲染输出)**;具体降幅参数 + 最小点击目标/键盘导航/handoff 输入锁定留 OQ-HUD-9 `/ux-design`。
7. **AC-56 / V-Own-07 动作类型**:依赖 `EActionType` payload——OQ-HUD-12 ✅RESOLVED(2026-06-05 propagate 已落 player-turn `FAIActionDetails`);实现期遵 player-turn L264 BP 引脚核验。
8. **AC-12/24a/30/57 headless 前提(R-3:门控标签已上 AC 条目,非仅藏此处)**:依赖 OQ-HUD-2 ADR 产出"状态机/动画驱动抽独立 UObject + IGameClock DI",否则 UMG `NativeTick` 在 `-nullrhi` 不触发致名义可测实则 false-green。四条 AC 已标 `[Logic·门控 OQ-HUD-2]`,实现期遇此标签须先确认 ADR 产出再写 headless 测试。
9. **AI 行动区布局可行性(R-3)**:V-Own-07 已删"底部操作栏"歧义、统一为"HUD 专属区(非操作栏)";1920×1080 下顶部栏/操作栏/棋盘≥65%/通告区/AI 行动区的具体高度数字化归 OQ-HUD-6 `/ux-design`,"不挤占操作栏"硬约束须在该阶段验可满足(ux-designer R-3 F4-A 提示布局预算紧)。
10. **GameOver 事件忽略列表 + 退出路径(R-3,ux-designer)**:状态机不变式列了 GameOver 冻结 `OnTurnStarted`/`OnCashChanged`/`OnTurnEnded`,但未覆盖 `OnPhaseChanged`/`OnBankruptcyDeclared`(末玩家破产与 OnGameWon 极近到达);GameOver→主菜单退出路径归 #20,HUD 状态机仅需在 Pre-Production 补"GameOver 后操作栏全禁用 + 忽略剩余局内事件"——非 MVP 阻塞,登记待 `/ux-design`。

## Open Questions

| ID | 问题 | Owner | 目标解决期 |
|----|------|-------|-----------|
| **OQ-HUD-1** | Player Fantasy 的 Submission(放松)维度措辞须 CD 复核(lean 模式跳过 creative-director;承接 player-turn OQ-8"HUD 开工前 CD 复核 Submission") | creative-director | Production 前 |
| **OQ-HUD-2** | 数据绑定形式(`BindWidget` 直绑 vs MVVM ViewModel)+ 事件 delegate 订阅/解绑生命周期(读档重绑 EC-11)。**R-2 必产 ADR 输出(否则 BLOCKING AC 名义可测实则 false-green,ue-umg B-1/B-4)**:① **面板/AI 行动状态机须抽为独立 `UObject`**(非 `UUserWidget` 内,后者 `NativeTick` 在 `-nullrhi` 不触发→AC-12/24a/30 无法 headless);② **游戏时间源须 DI 化 `IGameClock` 接口**(AC-30 `unpaused_elapsed` 可注入);③ **MVVM 不解 F-1/F-2 逐帧积分**——per-frame easing 须 `NativeTick` 驱动 + `SetTickEnabled(false/true)` 仅动画期开,MVVM 仅作数据绑定结构化,ADR 勿误选 MVVM 当 F-1/F-2 方案 | technical-director / ue-umg-specialist | 架构期 ADR |
| **OQ-HUD-3** | juice RNG 隔离(CR-16/AC-47)的拦截强度:BP 环境为 code-review 软约束;若架构选 C++ 强封装独立 `FRandomStream`,AC-47 可升静态扫描 [Logic] | technical-director | 架构期 ADR(随 OQ-HUD-2) |
| **OQ-HUD-4** | "加速/跳过 AI 行动"(EC-6/AC-32)的具体 UI 输入形式(按键/按钮/点击任意处) | ux-designer | Pre-Production `/ux-design` |
| **OQ-HUD-5** | 建房通告社交 beat(CR-12)在多屏 Pass'N Play / 在线环境的实际可见性验证(MVP 本地单屏已由 AC-34/35 覆盖逻辑触发;多屏/在线归集成 playtest) | qa-lead | 集成 playtest(Full Vision 联网阶段) |
| **OQ-HUD-6** | 各屏/HUD 元素 UX spec(顶部 HUD 栏 / 底部操作栏 / 通告区 / AI 行动区 / 胜利终局覆盖层)布局尺寸、focus 顺序、手柄导航 | ux-designer | Pre-Production `/ux-design`(UX Flag 已登记) |
| **OQ-HUD-7** | UMG 可行性复核(`ux-designer`/`ue-umg-specialist` lean 模式未咨询):widget 层级、Invalidation Box 性能、CommonUI input-aware 适配是否满足 60FPS 预算 | ue-umg-specialist | Production 前 |
| **OQ-HUD-8** | 玩家面板"资产摘要"是否常显 `net_worth`(=Cash+net_liquidation_value)还是仅持有地产数。**R-1 重分类(game-designer/CD):此为 MVP 信息架构核心决策,非纯 feel 调参**——玩家判断"对手是否快破产、是否该逼他"需 net_worth 而非仅现金,直接服务支柱②互坑决策可读性;但与认知负担(支柱④放松)有真权衡。**Pre-Production `/ux-design` 拍板信息架构,playtest 仅校准具体呈现**。**R-2 防 stall 预承诺(CD/disagreement 裁定 defer 保留):若 playtest 不决,MVP 落"仅持有地产数",net_worth 留 Full Vision**——避免 Pre-Production 悬而不决卡死。**R-3 补 fallback 安全性前提(game-designer):"仅持有地产数 + 现金"是否足以支撑支柱②"判断对手是否快破产"决策可读性,取决于棋盘/#17 UI 在 MVP 是否已清晰呈现地产抵押/建设状态(若是则 fallback 安全;否则须重评)——此为设计协议问题,非纯 playtest,Pre-Production 拍板时须先核此前提** | game-designer / ux-designer | **Pre-Production `/ux-design`**(playtest 校准) |
| **OQ-HUD-9** | **reduce-motion 细化参数 + 其余 a11y**(R-1/R-2):**reduce-motion 存在性 + "On 时去超冲"已升 MVP 硬要求(AC-57 守、`reduce_motion` 旋钮已登记)**;本 OQ 仅负责**具体降幅参数化**(各动画 On 时的目标时长/是否瞬切)。此外:最小点击目标尺寸(WCAG 2.5.5 ≥24×24px)、键盘 Tab 导航顺序、买地/建房模态确认 vs 直接触发(影响 CR-14 fire-and-forget vs request-confirm 接口语义)、**handoff outro 窗口输入锁定**(P1 outro 期 P2 接设备点击的路由语义,防误触金钱操作 + 防 EC-6 加速触发绕过 AC-24a)、瞬态元素(banner/AI 行动区)消失时手柄焦点逃逸归宿——均 ux-designer 点名 | ux-designer | Pre-Production `/ux-design` |
| **OQ-HUD-10** | **缺失可读性信息归属确认**(R-1,game-designer):监狱状态(待玩家回合2/事件7 暴露字段→HUD 面板呈现)、垄断组完成/抵押状态(归 #17)。债务关系不补(已裁定无持续债务账)。**🔶 垄断/抵押部分 = provisional-RESOLVED(2026-06-05,#17 R-2 propagate 回标)**:#17 已设计常驻棋盘叠加(`property-card-ui.md` CR-10 + V-Own-07/08 拥垄断完成环圈 + 抵押灰化斜纹常驻视觉)兑现归属 → **F-4 把 AI `Mortgage`/`Unmortgage` 判非 critical(可整条跳过)的前提"结果状态持久可读"已由 #17 设计满足**。⚠ 仅 **provisional**:叠加 widget owner/实现通道待架构 ADR(#17 OQ-PC-4(iv) = 本档 OQ-HUD-2 同一 ADR)兑现;**ADR 落定前 F-4 非-critical 裁定不可视作硬闭合**(避免据尚无 owner 的通道虚假闭合)。监狱状态部分仍 open(待 player-turn/7 暴露字段) | game-designer / #17 owner | Pre-Production(监狱部分);垄断/抵押部分随架构 ADR(OQ-HUD-2/OQ-PC-4) |
| **OQ-HUD-11** | **跨系统并发动画仲裁**(R-1,ux-designer 5-A):破产场景三路并发(HUD 去饱和 + VFX19 棋子消散 + 音频22 破产音)等**跨系统**注意力仲裁。(**HUD 同源内并发——破产终态 > 进行中 counter——已在 CR-5 钉死,本 OQ 仅余跨系统**) | ux-designer / VFX19 / 音频22 | Pre-Production `/ux-design` |
| **OQ-HUD-12** | **✅ RESOLVED(2026-06-05,同 PR propagate 已落):player-turn `FAIActionDetails` 已扩 `EActionType`**(枚举:`BuyProperty`/`BuildHouse`/`Mortgage`/`Unmortgage`/`Move`/`PayRent`/`Bankrupt`…,player-turn L263 + 修订日志 propagate 条目)——HUD V-Own-07 文案/图标 + F-4 critical 判定(AC-56)依赖已兑现;owner=player-turn(2)。**实现期遵 player-turn L264:核验绑定 `FAIActionDetails` 的下游 BP 图引脚防静默断线** | ✅ 已闭合(fresh-grep 核验 player-turn 落地) | RESOLVED |
| **OQ-HUD-13** | **Pass'N Play 建房通告时序补偿**(R-2,game-designer):单屏下建房 banner 2.4s 在设备移交前消失→下一玩家看不到对手建房。设计"回合开始回看上一轮对手关键行动摘要"机制(让轮到者知晓离屏期间对手盖楼)。非 MVP 阻塞、非纯 Full Vision(单屏即存在) | ux-designer / game-designer | Pre-Production `/ux-design` |
