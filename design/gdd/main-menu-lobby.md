# GDD — 主菜单与房间大厅 (Main Menu & Lobby) · 系统 #20

> **Status**: Approved（R-1 fresh `/design-review` 2026-06-06 = APPROVED_WITH_FOLLOWUPS；遗留 OQ-Lobby-1..7 上游/产品决策 + UX-spec/Asset-spec flags 不阻开工，详见 reviews/main-menu-lobby-review-log.md）
> **Category**: UI（呈现/编排层）
> **Priority**: MVP
> **Author**: ux-designer
> **Created**: 2026-06-06
> **Depends On**: 2（玩家与回合管理）
> **Implements Pillar**: ④ 易上手轻松局（低摩擦快速开局）+ ②社交博弈（本地热座配置入口）
> **Engine**: Unreal Engine 5.7（UMG / Blueprint 为主）

---

## 1. Overview

主菜单与房间大厅（#20）是玩家进入一局对战前的**配置与编排入口**。它是一个**呈现/编排层纯叶子系统**——它收集本地热座（Pass'N Play）2–8 玩家的开局配置（玩家数、每位玩家名/棋子色/人类或 AI、AI 难度档），在"开始游戏"时把配置打包成一个**开局参数结构（`FGameSetupConfig`）传给玩家与回合管理（2）**，由回合 2 据此初始化 `PlayerState` 列表并执行开局定序。

本系统**不持有任何游戏运行态、不驱动游戏流程、不计算定序**（同 HUD CR-14 的纯叶子约束）：它只在"开局前"活动，把一组玩家意图转成回合 2 能消费的参数；一旦回合 2 接管，本系统退场。MVP 范围严格收敛为**本地热座配置 + AI 设置 + 席位/颜色分配 + 开始游戏**四件事。联网房间/匹配（#25 Full Vision）、House Rules 配置面板（#23 Alpha）、存档续局入口（#21，接口未定）均**不在 MVP 正文**，仅留 Open Question 占位，避免在 MVP 制造未兑现接缝（吸取 #10 过度规格化教训）。

## 2. Player Fantasy

**核心幻想：低摩擦快速开局——"想玩了，三秒就能开一局"。**

玩家从启动游戏到坐进对局之间的每一步都应是**明确、轻量、无挫败**的：一眼看懂有几个座位、谁是真人谁是 AI、AI 多强；调整全靠点击（增减座位、切颜色、换难度档），不需要填表单、不需要读说明。这呼应 concept 支柱④（易上手轻松局：随时和朋友开一局）与 Target Player "能随时和朋友开一局"的诉求，也呼应 concept "What would turn them away = 上手门槛高"。

情感基调取自 art-bible 状态 A（主菜单/房间大厅）：**期待感 + 轻松欢迎**——暖白光、棋盘缓慢呼吸、玩家"落座"的弹性入场动画，让配置过程本身就有"马上要开一局了"的愉悦预期，而不是一道冷冰冰的设置墙。

> **纯叶子的 Fantasy 边界（enable-not-own，吸取编排层教训）：** 本系统**不拥有**"对局爽感"——掷骰、收租、建房的 juice 归 #19/HUD#16/对局系统。本系统的 Fantasy 仅限**开局前的低摩擦预期与欢迎感**；它**使**对局得以快速开始（enable），不**拥有**对局体验本身。入场动画/呼叫感的视觉体验由 art-bible 状态 A 与 Pre-Production UX spec 兑现（见 Flags），本档登记体验契约、不在 AC 断言"感觉良好"。

## 3. Detailed Rules

### CR-1 系统定位（纯叶子编排）
- 本系统**只在 PreGame（开局前）阶段存在**：玩家在大厅配置 → 点"开始游戏" → 本系统构造 `FGameSetupConfig` → 调用回合 2 的开局入口 → 本系统**移交控制权并退场**。
- 本系统**不**持有 `PlayerState`、不写任何对局运行态、不计算 turn order、不掷骰。所有这些归回合 2（见 player-turn CR-2/F-4）。本系统只产出**配置数据**。
- 本系统**不被任何对局系统反向调用**（无环：#20 → #2 单向）。回合 2 不依赖本系统的运行态。

### CR-2 大厅配置项（MVP 范围）
大厅维护一份**席位列表 `SeatSlots`**（长度 = 当前玩家数 `P`，`P∈[P_min, P_max]`），每个席位含以下可配置项：

| 配置项 | 类型 | 可选值 | 默认 | 写入目标（回合2 字段） |
|---|---|---|---|---|
| 是否启用 | bool | 启用/空 | 前 `P_default` 个启用 | 决定 `P`（启用席位数） |
| 玩家名 | setup 层 FString（编辑态）→ 边界转 FText | 非空、≤`NAME_MAX_LEN` 字符 | `"Player N"` / `"AI N"` | `PlayerState.DisplayName`（FText，owner=回合2 L79；本档在 F-2 边界 `FText::FromString` 转换） |
| 棋子颜色 | EPlayerColor | 8 色枚举之一，**席位间互斥** | 按 art-bible §5.2 P1..P8 默认色 | `PlayerState.TokenColor` |
| 玩家类型 | bool `bIsAI` | 人类 / AI | 席位 0 人类、其余可切 | `PlayerState.bIsAI` |
| AI 难度 | EAIDifficulty | None / Casual / Normal / Sharp | 人类=None；AI 默认 Normal | `PlayerState.AIDifficulty` |

> **接口归属（与 player-turn / ai-opponent 双向一致）：** `EPlayerColor`、`EAIDifficulty`、`PlayerState` 字段语义（含字段名与类型）**均归玩家回合（2）**（player-turn L78-82/L268 枚举 append-only）；AI 难度的**行为语义**归 AI（10）F-7（ai-opponent L340 明确"难度选择 UI 归主菜单/房间大厅(20)，写 `PlayerState.AIDifficulty`"）。本系统**只写值、不定义枚举、不定义字段名/类型**。
>
> **玩家名字段对齐（B-1，owner=回合2 L79）：** owner 权威字段为 `PlayerState.DisplayName`、类型 `FText`（**非** `PlayerName`/`FString`）。本系统 setup 层为支持文本输入框逐字符编辑，内部用 `FString` 持名（编辑态自然），**仅在 F-2 `BuildSetupConfig` 边界**经 `FText::FromString` 转为 `FText` 写入 `FPlayerSetupEntry.DisplayName`，再由回合2 写入 `PlayerState.DisplayName`。字段名/类型对齐已与 owner 双向回链（见 OQ-Lobby-1 同批 producer propagate）。

### CR-3 席位增减与颜色互斥
- 玩家通过增/减座位调整 `P`，约束 `P∈[P_min, P_max]`（MVP `[2,4]`，接口预留 8）。达上下界时对应增/减控件**禁用**（不可越界）。
- 棋子颜色在已启用席位间**互斥**：选取某色后，该色在其余席位选择器中**置灰不可选**（art-bible §4.5（禁用态：去饱和至灰度+不透明度 50%）/§4.3（语义色：灰=失效/禁用））。释放（改席位为其他色/停用席位）后该色重新可选。
- 至少 1 个席位**不可停用**到 `P<P_min`：减座位控件在 `P==P_min` 时禁用。

### CR-4 AI 设置
- 任一席位可切换 `bIsAI`。切到 AI 时 `AIDifficulty` 从 `None` 变为默认 `Normal`，并显示三档难度选择器（Casual/Normal/Sharp）。
- 切回人类时 `AIDifficulty` 复位为 `None`（与 player-turn AC-1 默认一致：`bIsAI=false→AIDifficulty=None`）。
- 难度档**仅选择**，行为差异由 AI（10）F-7 实现；本系统不解释三档区别（可附 Tooltip 简述，文案非本档规格）。

### CR-5 开始游戏（构造开局参数 → 移交回合2）
- "开始游戏"按钮在配置**合法**时可用（见 F-1 合法性谓词），非法时禁用并就近提示原因（如"颜色重复""名字为空"）。
- 点击后本系统构造 `FGameSetupConfig`（见 F-2 结构），调用回合 2 的开局入口（接口名 `StartNewGame(FGameSetupConfig)`，**owner=回合2**，本档作为调用方 propagate 该入口具名——见 Dependencies / OQ-Lobby-1）。
- **本系统不掷骰、不定序**：开局回合定序（经典掷骰定序，CR-2/F-4）由回合 2 在收到配置后自行执行。本系统**不**提供"手动指定先后手"——MVP 先后手由回合 2 掷骰决定（手动 turn order = Alpha，见 OQ-Lobby-4），避免与回合 2 CR-2 定序权责冲突制造接缝。
- 移交后回合 2 广播 `OnTurnOrderResolved(FTurnOrderResult)`（player-turn L258/AC-41）通知呈现层最终顺序——本系统已退场，该事件由对局内呈现层（HUD#16/VFX#19）消费，**不**回流大厅。

### CR-6 主菜单顶层入口（MVP 最小）
主菜单提供以下顶层入口（MVP）：
- **开始游戏（New Game）** → 进入大厅配置流程（本档主体）。
- **退出（Quit）** → 退出应用。
- **设置（Settings）** → 占位入口；MVP 仅音量/分辨率等系统设置归通用设置，House Rules 配置 = Alpha（#23，OQ-Lobby-3）。
- **续局（Continue）** → 占位入口；存档续局读 #21，#21 接口未定 → MVP 此入口**禁用/隐藏**（OQ-Lobby-2）。
- **联机（Online）** → 占位入口；MVP **禁用/隐藏**，联网房间/匹配 = Full Vision（#25，OQ-Lobby-5）。

## 4. Formulas

> 本系统是纯配置层，无数值玩法公式。以下两条是**配置合法性判定**与**开局参数打包**——纯结构/谓词逻辑，可 headless 测试（[Logic]）。

### F-1 ConfigIsValid — 开局配置合法性谓词

判定当前大厅配置是否可以开始游戏（决定"开始游戏"按钮可用性 + 回合2 防守层入参契约）。

```
ConfigIsValid =
    (P_min ≤ P ≤ P_max)                                  // C1 玩家数在界内
  ∧ (∀ s ∈ EnabledSeats: NameOf(s) ≠ "" ∧ Len(NameOf(s)) ≤ NAME_MAX_LEN)  // C2 名字非空且不超长（NameOf 取 setup 层 FString 编辑态；若对已转 FText 取值则先 .ToString() 再计字符——FText 无直接 Len()）
  ∧ (∀ s,t ∈ EnabledSeats, s≠t: ColorOf(s) ≠ ColorOf(t))                  // C3 颜色两两互异
  ∧ (∀ s ∈ EnabledSeats: (bIsAI(s) = false → AIDifficulty(s) = None)
                        ∧ (bIsAI(s) = true  → AIDifficulty(s) ∈ {Casual,Normal,Sharp}))  // C4 AI 档与类型一致
```

**变量定义：**

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `P` | int32 | 实际 `[0, P_max]`；合法要求 `[P_min, P_max]` | 当前**已启用**席位数 |
| `P_min` | int32 | 常量 = 2 | 最小玩家数（回合制对战下限，对齐 player-turn AC-27/L379） |
| `P_max` | int32 | 常量 = 4（MVP；接口预留 8） | 最大玩家数（对齐 player-turn AC-27 "P>4 拒绝"） |
| `EnabledSeats` | set\<Seat\> | 大小 = `P` | 已启用席位集合 |
| `NameOf(s)` | FString（setup 编辑态） | 0..`NAME_MAX_LEN` 字符 | 席位 s 玩家名（编辑态 FString；F-2 边界经 `FText::FromString` 转 `FText` 写入 `FPlayerSetupEntry.DisplayName`） |
| `NAME_MAX_LEN` | int32 | 常量 = 16（默认） | 玩家名最大长度 |
| `ColorOf(s)` | EPlayerColor | 8 枚举值之一 | 席位 s 棋子色 |
| `bIsAI(s)` | bool | true/false | 席位 s 是否 AI |
| `AIDifficulty(s)` | EAIDifficulty | None/Casual/Normal/Sharp | 席位 s AI 难度 |

**输出范围：** `bool`。`true` ⇒ "开始游戏"可用且 `BuildSetupConfig` 产出回合2 可接受的入参；`false` ⇒ 按钮禁用，且**即便绕过 UI**，回合2 防守层（player-turn AC-27/AC-28）将拒绝越界配置。

**示例计算：**
- **例 A（合法）：** `P=2`，席位 0=人类"Alice"红、席位 1=AI"Bot"蓝/Normal。C1: 2≤2≤4 ✓；C2: 两名非空 ✓；C3: 红≠蓝 ✓；C4: 人类→None ✓、AI→Normal∈{...} ✓ → `ConfigIsValid=true`。
- **例 B（颜色冲突→非法）：** `P=3`，席位 0/1 均选红。C3: ColorOf(0)=ColorOf(1)=红 → 违反 → `ConfigIsValid=false`（按钮禁用，提示"颜色重复"）。
- **例 C（越界→非法）：** `P=1`（仅 1 席启用）。C1: 1 < P_min=2 → 违反 → `false`（减座位控件本应在 P=2 时已禁用，F-1 是回合2 入参的二次防守）。

### F-2 BuildSetupConfig — 开局参数打包

把合法的大厅配置打包成传给回合 2 的 `FGameSetupConfig`。

```
BuildSetupConfig(EnabledSeats) → FGameSetupConfig {
    int32 PlayerCount = P                                  // = |EnabledSeats|
    TArray<FPlayerSetupEntry> Players = [                  // 按席位索引升序
        for each s ∈ EnabledSeats (ordered by SeatIndex):
            FPlayerSetupEntry {
                FText      DisplayName = FText::FromString(NameOf(s))  // B-1 对齐 owner PlayerState.DisplayName:FText(L79);setup FString 在此边界转 FText
                EPlayerColor TokenColor = ColorOf(s)
                bool       bIsAI       = bIsAI(s)
                EAIDifficulty AIDifficulty = AIDifficulty(s)
            }
    ]
}
```

**变量定义：**

| 变量 | 类型 | 范围 | 含义 |
|---|---|---|---|
| `FGameSetupConfig` | USTRUCT(BlueprintType) | — | 开局参数容器，回合2 `StartNewGame` 入参 |
| `PlayerCount` | int32 | `[P_min, P_max]` | 玩家数（= `Players` 数组长度，单源） |
| `Players` | TArray\<FPlayerSetupEntry\> | 长度 = `PlayerCount` | 每席位配置，**按 `SeatIndex` 升序**（确定性顺序） |
| `FPlayerSetupEntry` | USTRUCT(BlueprintType) | — | 单玩家配置：`DisplayName`(FText，对齐 owner L79)/`TokenColor`/`bIsAI`/`AIDifficulty`。名字段名/类型与 `PlayerState` 逐字对齐(B-1)，回合2 据此直填 `PlayerState.DisplayName` 无再转换。 |

> **USTRUCT 约束（UE/dynamic delegate 一致性，承 player-turn L257）：** `FGameSetupConfig`/`FPlayerSetupEntry` 须标 `USTRUCT(BlueprintType)`；含 `TArray` 字段须包在 USTRUCT 内（不可裸 `TArray` 作 BP 函数参数边界）。`StartNewGame(const FGameSetupConfig&)` 取常引用传入。

**前置条件：** 仅在 `ConfigIsValid=true` 时调用（CR-5）；否则不构造。

**输出范围：** 一个 `FGameSetupConfig`，其 `PlayerCount∈[P_min,P_max]`、`Players` 各项满足 F-1 的 C2/C3/C4。**该结构是回合2 `StartNewGame` 的唯一入参**——颜色互异、名字合法、AI 档一致性在此已保证，回合2 仍做防守校验（AC-27/AC-28，不静默接受越界）。

**示例计算：** 例 A 配置 → `FGameSetupConfig{ PlayerCount=2, Players=[ {"Alice",红,false,None}, {"Bot",蓝,true,Normal} ] }`。回合2 据此建 2 个 `PlayerState`（PlayerId 0/1 开局分配）并进入掷骰定序。

## 5. Edge Cases

> 每条明确"发生什么"（不"优雅处理"）。

- **EC-1 玩家试图把玩家数减到 1（`P < P_min`）：** 减座位控件在 `P==P_min`（=2）时**已置禁用**，点击无效（不响应）；即使通过外部/异常路径产生 `P<2` 配置，F-1 C1 判 `false` → "开始游戏"禁用；即使再绕过，回合2 防守层（player-turn L379 "P<2 拒绝开局"/AC-27）拒绝并返回错误，**不进入对局**。
- **EC-2 玩家试图把玩家数增到 >`P_max`：** 增座位控件在 `P==P_max`（MVP=4）时**已置禁用**，点击无效；绕过则 F-1 C1 判 `false`；再绕过则回合2 AC-27 "P>4 拒绝开局或返回特定错误码"。MVP 接口预留 8，但 `P_max=4` 是大厅硬上界。
- **EC-3 两席位选同一颜色：** 选色器在已被占用的颜色上**置灰不可选**（CR-3），正常路径不会发生；若异常产生重复（如读未来存档配置），F-1 C3 判 `false`、"开始游戏"禁用、提示"颜色重复"。**不**自动改色（避免静默篡改玩家意图）。
- **EC-4 玩家名留空或仅空白：** "开始游戏"禁用（F-1 C2），就近提示"名字不能为空"。**不**自动填默认名覆盖玩家已清空的输入（尊重玩家正在编辑）；但席位**初始**默认名为 `"Player N"`/`"AI N"`（非空），故未编辑的席位天然合法。
- **EC-5 玩家名超长（>`NAME_MAX_LEN`）：** 输入框在达到 `NAME_MAX_LEN` 时**阻止继续输入**（截断在输入层）；F-1 C2 作二次防守。**不**截断后静默提交一个与显示不一致的名字。
- **EC-6 全部席位都设为 AI（无人类）：** **允许**——AI vs AI 是合法配置（用于观战/调试/演示）。F-1 不要求至少 1 人类（MVP 不加此约束，避免过度规格化）。回合2 正常初始化全 AI 局。〔若产品决定禁止全 AI，归 Alpha 旋钮，见 OQ-Lobby-6〕
- **EC-7 玩家在配置中途点"返回主菜单"：** 当前大厅配置**丢弃**（不持久化草稿——MVP 不做配置草稿存档，避免接缝）；返回主菜单后再次进入大厅恢复默认配置。
- **EC-8 续局/联机入口被点击（MVP 占位）：** 这些入口在 MVP **禁用或隐藏**（CR-6）；若可见且被点击，弹出"即将推出"提示并**不**进入任何未实现流程（无半成品接缝）。续局接口待 #21（OQ-Lobby-2），联机待 #25（OQ-Lobby-5）。
- **EC-9 `StartNewGame` 调用后回合2 返回失败（防守层拒绝）：** 本系统**停留在大厅、不切场景、不退场**，并显示回合2 返回的错误（如越界 P 错误码）。本系统不重试、不修改配置（玩家须自行修正）。这保证大厅与回合2 防守层对齐（CR-5/F-1）而非各自为政。

## 6. Dependencies

> 双向一致：本系统依赖的每个系统，其 GDD 须在被依赖方向提及 #20（已核验，见下表"反向核验"列）。

### 6.1 上游依赖（本系统消费/调用）

| 系统 | 数据流 / 接口 | 谁拥有 | 反向核验 |
|---|---|---|---|
| **玩家与回合管理（2）** | 本系统构造 `FGameSetupConfig` → 调回合2 开局入口 `StartNewGame(const FGameSetupConfig&)`，回合2 据此初始化 `PlayerState` 列表并定序（player-turn CR-2/F-4）。本系统**写**配置值（名/色/`bIsAI`/`AIDifficulty`）、**不**写运行态、**不**定序、**不**被回合2 反调。 | 回合2 拥有 `PlayerState`/枚举/定序/开局入口语义 | ✅ player-turn L251 "主菜单大厅(20) 开局传入玩家数/名/色/AI 档 → 本系统初始化 `PlayerState` 列表并定序"；L419 "硬(依赖)"；index 行 `20 → depends 2`。开局入口具名 `StartNewGame` 为本档 propagate（OQ-Lobby-1）。 |
| **AI 对手（10）**（间接，经回合2 字段） | 本系统**写** `PlayerState.AIDifficulty`（值），AI（10）**读**并按 F-7 实现行为。本系统不依赖 AI 运行态，仅借其难度枚举语义。 | 难度**字段**归回合2；难度**行为语义**归 AI（10）F-7 | ✅ ai-opponent L340 "难度选择(Casual/Normal/Sharp)UI 归主菜单/房间大厅(20)——写 `PlayerState.AIDifficulty`（字段归玩家回合2）；本系统供三档行为语义(F-7)，不拥有选择 UI/布局"；L342 AI 不触发 UX-spec flag（难度选择屏 UX 归 #20/#16）。 |

### 6.2 下游消费者（消费本系统产物的系统）

| 系统 | 关系 | 备注 |
|---|---|---|
| **玩家与回合管理（2）** | 消费 `FGameSetupConfig`（`StartNewGame` 入参） | 唯一直接下游；移交后本系统退场。 |

> **无下游运行态依赖：** 本系统在对局开始后不参与，不广播对局事件、不持有 registry 跨档事实（同 HUD 呈现层"无下游"性质）。`OnTurnOrderResolved` 等开局事件归回合2 广播、对局内呈现层（#16/#19）消费，**不**回流大厅（CR-5）。

### 6.3 延后/未定接口（OQ 占位，不在 MVP 正文）

| 系统 | 关系 | 状态 |
|---|---|---|
| **存档/读档（21）** | 主菜单"续局"入口拟读 #21 加载存档恢复对局 | #21 GDD 未撰写、接口未定 → **不臆造**，MVP 入口禁用/隐藏（OQ-Lobby-2）。 |
| **设置 & House Rules（23）** | 大厅拟提供 House Rules（局长/起始资金/`bEnableDoublesExtraTurn` 等）配置入口 | #23=Alpha → MVP 不暴露 House Rules 面板（OQ-Lobby-3）。player-turn L421/L432 已预留 `DOUBLES_JAIL_THRESHOLD`/`MAX_TIEBREAK_ROUNDS`/`bEnableDoublesExtraTurn` 为 House Rules(23) 接口。 |
| **联网（25）** | 主菜单"联机"入口拟提供房间/匹配 | #25=Full Vision → MVP 禁用/隐藏（OQ-Lobby-5）。 |

## 7. Tuning Knobs

| 旋钮 | 含义 | 安全范围 | 影响 |
|---|---|---|---|
| `P_min` | 大厅最小玩家数 | 2..2（**固定**，对齐 player-turn L379 对战下限） | <2 无法对战；本旋钮非真正可调，列出以示与回合2 契约一致。 |
| `P_max` | 大厅最大玩家数（MVP 上界） | 2..8（MVP 默认 4；接口预留 8） | MVP=4 对齐 player-turn AC-27 "P>4 拒绝"；调到 8 须回合2/经济/棋盘同步放开（联网 #25 阶段），否则回合2 防守层拒绝。**MVP 不超过 4**。 |
| `P_default` | 大厅打开时默认启用席位数 | `[P_min, P_max]`（默认 2） | 默认 2 = 最快开局路径（支柱④低摩擦）；调高则打开即多座位，增配置步骤。 |
| `NAME_MAX_LEN` | 玩家名最大字符数 | 8..24（默认 16） | 过短限制表达；过长 HUD/玩家面板（art-bible §7.4a）布局溢出。须 ≤ HUD 玩家名标签宽度容量。 |
| `DefaultAIDifficulty` | 席位切到 AI 时的默认难度 | {Casual, Normal, Sharp}（默认 Normal） | 默认 Normal = 中性体验（concept "AI 分级让休闲玩家也能赢"）；默认 Sharp 会让新玩家首局过难、违支柱④。 |
| `DefaultColorBySeat[8]` | 各席位默认棋子色 | EPlayerColor 8 值的一个**排列**（默认取 art-bible §5.2 P1..P8） | 须与 art-bible §5.2 玩家色一致且 8 色两两互异；乱序或重复破坏颜色互斥默认态（CR-3）。 |

> **本系统无玩法数值旋钮**——上述全为**配置上下界与默认值**，不影响对局平衡（对局平衡旋钮归经济5/AI10 等）。House Rules 玩法旋钮（局长/起始资金/双点规则）归 #23 Alpha，不在本档（OQ-Lobby-3）。

## 8. Acceptance Criteria

> 类型：[Logic]=可 headless 单测的配置/谓词逻辑；[Integration]=跨系统（回合2 移交）；[Advisory·code-review]=渲染/UI 呈现层（headless 测不了，靠 code-review + 手动 UX 走查，见 Flags UX-spec）。
> 等级：BLOCKING=阻塞 story Done；Recommended=建议但不阻塞。
> **呈现层渲染时刻一律标 [Advisory·code-review]**（吸取 #16/#19 headless 陷阱教训），逻辑状态机/谓词才标 [Logic]。

### 配置合法性（F-1）
- **AC-1** [Logic·BLOCKING]（F-1 全合法）GIVEN `P=2`、两席位名非空互异色、人类→None/AI→Normal，WHEN 求 `ConfigIsValid`，THEN ==true。
- **AC-2** [Logic·BLOCKING]（F-1 C1 下界）GIVEN `P=1`（仅 1 席启用），WHEN 求 `ConfigIsValid`，THEN ==false。
- **AC-3** [Logic·BLOCKING]（F-1 C1 上界）GIVEN `P=5`（MVP `P_max=4`），WHEN 求 `ConfigIsValid`，THEN ==false。
- **AC-4** [Logic·BLOCKING]（F-1 C3 颜色互斥）GIVEN `P=3` 且席位 0/1 同色，WHEN 求 `ConfigIsValid`，THEN ==false。
- **AC-5** [Logic·BLOCKING]（F-1 C2 空名）GIVEN 某启用席位 `NameOf=""`，WHEN 求 `ConfigIsValid`，THEN ==false。
- **AC-6** [Logic·Recommended]（F-1 C2 超长）GIVEN 某启用席位名长度 = `NAME_MAX_LEN+1`，WHEN 求 `ConfigIsValid`，THEN ==false。
- **AC-7** [Logic·BLOCKING]（F-1 C4 类型-档一致）GIVEN 某席位 `bIsAI=false ∧ AIDifficulty=Normal`（非法组合），WHEN 求 `ConfigIsValid`，THEN ==false；且 GIVEN `bIsAI=true ∧ AIDifficulty=None`，THEN ==false。
- **AC-8** [Logic·BLOCKING]（全 AI 合法，EC-6）GIVEN `P=2` 两席位均 `bIsAI=true`/合法难度，WHEN 求 `ConfigIsValid`，THEN ==true（MVP 不强制人类）。

### 开局参数打包（F-2）
- **AC-9** [Logic·BLOCKING]（F-2 长度单源）GIVEN 合法配置 `P=3`，WHEN `BuildSetupConfig`，THEN `PlayerCount==3 ∧ Players.Num()==3`（PlayerCount 与数组长度同源）。
- **AC-10** [Logic·BLOCKING]（F-2 字段映射，含 B-1 名字段对齐）GIVEN 例 A 配置（"Alice"红人类 / "Bot"蓝 AI-Normal），WHEN `BuildSetupConfig`，THEN `Players[0]=={DisplayName=FText("Alice"),红,false,None} ∧ Players[1]=={DisplayName=FText("Bot"),蓝,true,Normal}`，且 `Players[0].DisplayName` 为 `FText`（经 `FText::FromString` 转换，`.ToString()=="Alice"`），字段名为 `DisplayName`（**非** `PlayerName`，对齐 owner player-turn L79）。
- **AC-11** [Logic·BLOCKING]（F-2 确定性顺序）GIVEN 同一组启用席位，WHEN `BuildSetupConfig` 调用两次，THEN 两次 `Players` 按 `SeatIndex` 升序且逐项相等（确定性，无随机）。
- **AC-12** [Logic·Recommended]（F-2 前置）GIVEN `ConfigIsValid==false`，WHEN 尝试构造，THEN 不产出 `FGameSetupConfig`（构造仅在合法时发生，CR-5/F-2 前置条件）。

### 与回合2 移交（CR-5）— 继承义务回链
- **AC-13** [Integration·BLOCKING]（开局移交，回链 player-turn L251/L419）GIVEN 合法 `FGameSetupConfig`（`P=2`），WHEN 调 `StartNewGame(config)`，THEN 回合2 初始化 `PlayerCount==2` 的 `PlayerState` 列表，各 `PlayerState` 的 `DisplayName/TokenColor/bIsAI/AIDifficulty` 等于 config 对应项（字段名/类型逐字对齐 owner player-turn L78-82：名为 `DisplayName:FText`，**非** `PlayerName/FString`；`PlayerState.DisplayName.EqualTo(config.Players[i].DisplayName)`）。**继承义务：player-turn 主菜单大厅(20) 依赖行 + Interactions L251**。
- **AC-14** [Integration·BLOCKING]（默认值对齐 player-turn AC-1）GIVEN 某席位 `bIsAI=false`，WHEN 移交后查回合2 `PlayerState`，THEN `AIDifficulty==None`（对齐 player-turn AC-1 "bIsAI=false→AIDifficulty=None"）。
- **AC-15** [Logic·BLOCKING]（防守层对齐，回链 player-turn AC-27）GIVEN 越界 `P`（<2 或 >4）经异常路径到达 `StartNewGame`，WHEN 回合2 防守校验，THEN 拒绝/返回错误码且**不**初始化对局（大厅 F-1 与回合2 AC-27 双层防守一致；本档断言"大厅不静默接受越界 P"，回合2 侧由 player-turn AC-27 覆盖）。
- **AC-16** [Logic·BLOCKING]（不掷骰不定序，CR-5/CR-1）GIVEN 本系统 `StartNewGame` 路径，WHEN code-review/调用图核查，THEN 本系统**不**调用骰子(3) `RollDice`、**不**计算 `TurnOrderIndex`（定序归回合2 CR-2/F-4；可由"本系统调用面无掷骰/定序符号"spy/审查证伪）。
- **AC-17** [Logic·BLOCKING]（移交后退场，CR-1 无环）GIVEN 移交完成，WHEN 回合2 进入对局，THEN 回合2 不回调本系统任何接口（#20→#2 单向无环；可由回合2 调用图无 #20 符号证伪）。

### 大厅交互约束（CR-3，逻辑可测部分）
- **AC-18** [Logic·BLOCKING]（席位增减边界）GIVEN `P==P_min`，WHEN 求"可否减座位"，THEN false；GIVEN `P==P_max`，WHEN 求"可否增座位"，THEN false（边界谓词，与控件禁用状态同源）。
- **AC-19** [Logic·BLOCKING]（颜色可选集）GIVEN 席位 0/1 已占红/蓝，WHEN 求席位 2 的"可选颜色集"，THEN 不含红、不含蓝（颜色互斥谓词，CR-3）。
- **AC-20** [Logic·Recommended]（切 AI 难度复位）GIVEN 某席位 `bIsAI` 由 true 切 false，WHEN 应用切换，THEN `AIDifficulty` 复位为 `None`；由 false 切 true，THEN `AIDifficulty` 置 `DefaultAIDifficulty`（CR-4）。

### 呈现/渲染层（Advisory — headless 测不了，靠 UX 走查 + code-review）
- **AC-21** [Advisory·code-review]（按钮可用性绑定）"开始游戏"按钮的 `bIsEnabled` 绑定到 `ConfigIsValid`（F-1）；非法时禁用并就近显示原因文案。逻辑侧由 AC-1..AC-8 覆盖，**渲染呈现**靠 UX 走查。
- **AC-22** [Advisory·code-review]（禁用色置灰）已占用颜色在其余席位选择器中以去饱和/置灰呈现（art-bible §4.5（禁用态：去饱和至灰度+不透明度 50%）/§4.3（语义色：灰=失效/禁用））。逻辑侧"可选集"由 AC-19 覆盖。
- **AC-23** [Advisory·code-review]（占位入口处理）续局/联机入口在 MVP 禁用或隐藏；若可见且被点击，显示"即将推出"且不进入未实现流程（EC-8）。
- **AC-24** [Advisory·playtest-signoff]（art-bible 状态 A 体验契约）大厅呈现暖白光 + 棋盘呼吸 idle + 玩家落座弹性入场动画（art-bible §2 状态 A），传达"期待感 + 轻松欢迎"。属体验义务，须 Pre-Production UX spec + playtest/lead 签核兑现（见 Flags），本档不断言"感觉良好"。

### 继承义务覆盖核对（供 `/design-review` qa-lead 核对）
- 本系统**作为下游**无 systems-index "Inherited Test Obligations" 表中登记给 #20 的条目（该表当前无 #20 行）。
- 本系统**作为上游契约的履行方**：player-turn 主菜单大厅(20) 依赖（L251/L419 开局传参+初始化）→ **AC-13/AC-14/AC-15** 覆盖；ai-opponent L340 难度选择 UI 归 #20 写 `PlayerState.AIDifficulty` → **AC-7/AC-10/AC-14/AC-20** 覆盖；player-turn AC-27 越界 P 契约对齐 → **AC-2/AC-3/AC-15** 覆盖。

## 9. Open Questions

| ID | 问题 | 归属/触发阶段 |
|---|---|---|
| **OQ-Lobby-1** | 回合2 开局入口具名 + B-1 名字段对齐：本档假定 `StartNewGame(const FGameSetupConfig&)`（owner=回合2）。player-turn 当前 Dependencies 行（L251）描述了数据流但**未具名入口函数**。须 producer `/propagate-design-change` 在 player-turn 落定入口签名 + `FGameSetupConfig`/`FPlayerSetupEntry` USTRUCT 归属（建议归回合2 与其他 PlayerState 构造逻辑同处），并双向回链。**同批并入 B-1 决议**：本档已采 **Option A** 全档对齐 owner（玩家名字段 `PlayerName→DisplayName`、类型 setup 层 FString → F-2 边界 `FText::FromString` → `PlayerState.DisplayName:FText`），与 player-turn L79 逐字一致；此 propagate 批次须确认 player-turn L79 字段名/类型未变并双向回链 `FPlayerSetupEntry.DisplayName:FText` 接口。〔可逆：若产品后确认 setup/PlayerState 统一用 `PlayerName/FString`，切 Option B，由 producer 改 player-turn L79 owner 并回链——本档默认不锁死。〕**本档不臆造为已定**——AC-13 的接口名待此 OQ 闭合后冻结。 | producer / 设计冻结前 |
| **OQ-Lobby-2** | 续局入口读 #21 存档：#21 GDD 未撰写、加载接口未定。MVP 续局入口**禁用/隐藏**。#21 设计时须定义"从存档恢复对局"入口（如 `LoadGame(SaveSlot)→对局态`），届时大厅续局入口接入。**不臆造 #21 接口**。 | #21 设计阶段 |
| **OQ-Lobby-3** | House Rules 配置入口（局长/起始资金/`bEnableDoublesExtraTurn`/`DOUBLES_JAIL_THRESHOLD`/`MAX_TIEBREAK_ROUNDS`）：#23=Alpha。MVP 大厅**不暴露** House Rules 面板。player-turn L421/L432 已预留这些旋钮为 #23 接口（开局锁定、局中不可改）。Alpha 时大厅增 House Rules 配置区，写入回合2 开局锁定旋钮。 | #23 Alpha |
| **OQ-Lobby-4** | 手动指定先后手（turn order）：MVP 先后手由回合2 掷骰定序（CR-2/F-4），大厅**不**提供手动指定（避免与回合2 定序权责冲突）。若产品需要"固定座位先后手"选项 = Alpha House Rules（#23），届时回合2 须提供"跳过掷骰定序、按大厅指定顺序"的开局变体入口。 | #23 Alpha |
| **OQ-Lobby-5** | 联网房间/匹配/邀请：#25=Full Vision。MVP 联机入口**禁用/隐藏**。联网房间是独立大厅形态（房间码/匹配/准备状态/重连），与本地热座大厅共享配置 UI 但增网络态 → Full Vision 重新设计。 | #25 Full Vision |
| **OQ-Lobby-6** | 是否禁止"全 AI"局：MVP **允许**全 AI（EC-6，AI vs AI 观战/调试）。若产品决定至少需 1 人类，为 Alpha 旋钮（如 `bRequireHumanPlayer`），届时加入 F-1 一条约束 + AC。 | Alpha / 产品决策 |
| **OQ-Lobby-7** | `EPlayerColor` 是否已含 8 色枚举值：player-turn L80 "预留 8 色枚举"。本档默认 `DefaultColorBySeat[8]` 取 art-bible §5.2 P1..P8。须确认回合2 `EPlayerColor` 枚举值与 art-bible §5.2 玩家色一一对应（命名/顺序），避免大厅默认色与回合2 枚举不匹配。属轻量核对，归 producer。与 player-turn OQ-2（`EPlayerColor` 调色板与 `EColorGroup` 分离待 art-bible 确认）同源，两未决项交叉回链。 | producer / 设计冻结前 |

## 10. Flags

- 📌 **UX-spec flag（触发）：** 本系统拥有自有屏（主菜单屏 + 大厅配置屏），属 UI 系统。Pre-Production 须运行 `/ux-design` 为 **主菜单屏** 与 **房间大厅配置屏**（席位列表/颜色选择器/AI 难度选择器/开始按钮/占位入口）各写 UX spec 到 `design/ux/main-menu.md` 与 `design/ux/lobby.md`；story 引用 `design/ux/[screen].md`，**不**直引本 GDD。AC-21/22/23/24 的呈现层验收在 UX spec 阶段细化。难度选择屏 UX 归本系统（ai-opponent L342 已明确 AI 不拥有该屏 UX）。
- 📌 **Asset-spec flag（触发）：** art-bible approved 后运行 `/asset-spec system:main-menu-lobby`，产出主菜单/大厅所需 UI 资产规格（背景、按钮 `WBP_`/`T_ui_*`、席位卡片、颜色选择器、难度档图标、占位入口禁用态），遵 art-bible §7（UI 视觉方向）/§8（资产标准命名 `WBP_ui_*`/`T_ui_*`）。状态 A 的棋盘呼吸/落座入场动画资产联动 art-bible §2 状态 A。

---

> **MVP-minimal 声明：** 本档刻意将联网房间/匹配（#25）、House Rules 面板（#23）、手动 turn order（#23）、存档续局（#21）、全 AI 禁止（Alpha）全部降为 OQ 占位，**不**写入 MVP 正文，避免在尚未成形的下游系统上制造未兑现接缝（吸取 #10 过度规格化与第二代残渣教训）。MVP 正文仅含：本地热座 2–4 配置 + AI 设置 + 席位/颜色分配 + 开始游戏构造参数移交回合2。
