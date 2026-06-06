# ADR-0010: 音频架构（MetaSound vs Sound Cue + 中间件取舍）

## Status

Accepted

## Date

2026-06-06

## Last Verified

2026-06-06 — 对照 `design/gdd/audio.md`（Approved，R-1）/ `docs/engine-reference/unreal/modules/audio.md` /
`docs/engine-reference/unreal/deprecated-apis.md`（Audio 节）/ `architecture.md §1.4 + §8 ADR-0010 条目` /
ADR-0003（Event Bus）/ ADR-0004（确定性 RNG）/ ADR-0007（BP-vs-C++ 边界）逐条核实。

## Decision Makers

msc（用户）+ Technical Director（主笔）— 2026-06-06 Accepted（用户裁定全接受推荐选型：MetaSound 程序化音效 + 简单 SFX Sound Wave + 三路 Sound Class/Submix + C++ 事件订阅层 + #22 独立非权威 FRandomStream + MVP 纯原生中间件）。
下游消费方 owner：audio(22)；上游事件 owner：骰子(3) / 回合(2) / 移动(4) / 经济(5) / 地产(6) / 建房(8) / 事件格(7)。

## Summary

《骰子大亨》音频系统（#22）需要在 UE5.7 原生音频工具链内选定**程序化音效引擎**（MetaSound vs Sound Cue），
并确立事件订阅层（C++ vs BP）、音频随机流封装（独立非权威 `FRandomStream`）、三路混音总线结构。
本 ADR 裁定：**程序化音效选 MetaSound**（UE5.7 推荐，Sound Cue 复杂逻辑已弃）；
**基础 2D 播放用稳定的 `PlaySound2D` / `UAudioComponent`**；
**中间件 = 纯原生**（MVP，Wwise/FMOD 降 Full Vision 评估）；
**事件订阅层 = C++**（随 ADR-0007 强度，headless 可测硬前提）；
**音频随机走 #22 自建独立非权威 `FRandomStream`**（随 ADR-0004 流隔离纪律）。

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7（Blueprint 为主 + C++ 框架） |
| **Domain** | Audio（核心音频架构，横切 Presentation 层） |
| **Knowledge Risk** | 🟡 MEDIUM — MetaSound 在 UE 5.3 时已进入 Early Access，5.7 标注 production-ready（`audio.md` 核验：「MetaSounds: Node-based procedural audio (RECOMMENDED, production-ready)」）。但 5.4–5.7 MetaSound 参数化 API（`SetFloatParameter`/`SetIntParameter`/`SetBoolParameter`）、Quartz 节拍同步、`UAudioComponent` + MetaSound 组合的 5.7 最终形态**超出模型 ~5.3 记忆**，须实现期对照 5.7 文档核实。Sound Concurrency / Sound Class / Submix 结构为 pre-5.3 稳定（LOW）。 |
| **References Consulted** | `docs/engine-reference/unreal/modules/audio.md`（MetaSound + Sound Cue + Sound Class/Submix + Sound Concurrency + 基础播放 API，verified 2026-02-13）；`docs/engine-reference/unreal/deprecated-apis.md`（Audio 节：「Sound Cue (for complex logic) → MetaSounds」）；`docs/engine-reference/unreal/current-best-practices.md`；`docs/engine-reference/unreal/VERSION.md`（5.4–5.7 知识缺口）；`design/gdd/audio.md`（Approved，CR-1~CR-13 / F-1~F-4 / OQ-Audio-4）；`docs/architecture/architecture.md §1.4`（MetaSound + 三路混音总线裁定） |
| **Post-Cutoff APIs Used** | `UMetaSoundSource`（UE 5.7 production-ready，post-cutoff 正式落地）；`UAudioComponent::SetFloatParameter` / `SetIntParameter` / `SetBoolParameter`（MetaSound 参数化，5.4+ 形态须核实）；`UGameplayStatics::PushSoundMixModifier` / `PopSoundMixModifier`（pre-5.3 稳定，LOW）；`USoundConcurrency`（pre-5.3 稳定）；`UGameplayStatics::PlaySound2D`（pre-5.3 稳定） |
| **Verification Required** | 实现前须对 UE 5.7 文档/源码确认：① `UAudioComponent::SetFloatParameter(FName, float)` 在 5.7 的最终签名是否有变化（影响程序化骰点变体/金币递增参数化）；② MetaSound Source 与 `UAudioComponent` 组合播放在 5.7 是否仍支持 `PlaySound2D` 调起（见 audio.md 示例）；③ `USoundSubmix` + `PushSoundMixModifier` 在 5.7 是否被新的 Audio Modulation / Audio Bus 系统部分替代（影响三路混音设计）；④ MetaSound 在 `-nullrhi` headless 模式下参数注入是否可 mock（影响 [Logic] AC spy 可行性） |

> **Note**: Knowledge Risk = MEDIUM → 若项目升级引擎版本，本 ADR 须重新验证上述 ①–④。
> 若 5.7 确认 Sound Mix 被 Audio Modulation 替代，则 CR-12/F-4 BGM duck 实现须更新——届时标 Superseded 重写。

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | **ADR-0003（事件总线与 Delegate 契约）** — 音频(22) 所有触发源均来自 owner multicast delegate；ADR-0003 规范化签名、payload 格式、防双订阅纪律，#22 实现前须已 Accepted。**ADR-0004（确定性 RNG 服务）** — 本 ADR 音频随机流（变体选择/pitch 抖动）须建立独立非权威 `FRandomStream`，其隔离纪律与语义直接继承 ADR-0004 的流隔离约定。**ADR-0007（BP-vs-C++ 边界）** — 事件订阅层归 C++ 还是 BP 须遵循 0007 强度裁定（本 ADR 沿用 C++ 权威订阅层）。 |
| **Enables** | 音频(22) 实现开工（audio.md OQ-Audio-4 自陈「ADR 收口前不开工」）；OQ-Audio-1（Alpha 动态音乐，评估 MetaSound 动态参数/Quartz 同步时参考本 ADR 原生工具链选型） |
| **Blocks** | 音频系统(#22) 任何实现 story；OQ-Audio-4 收口（audio.md 显式标注「架构期 ADR」门控） |
| **Ordering Note** | 依赖 ADR-0003/0004/0007 均须先 Accepted。ADR-0003/0004 属 Foundation 层，ADR-0001 Accepted 后已可并行起草；ADR-0007 属 Core/Presentation 层，可与 0010 同批起草。本 ADR 不改 audio(22) 对外接口签名（`PlayUISound(EUISoundId)` / delegate 订阅结构），只裁定**实现层工具链与隔离纪律**。 |

## Context

### Problem Statement

《骰子大亨》音频系统（#22）需要在设计期确定以下三类架构选型，否则实现期可能产生不可逆的工具链锁定、
测试死角、或联网重放污染：

1. **程序化音效引擎**：骰点变体（不同面值音色）、金币递增动画、pitch 抖动等程序化行为，需一个可参数化的音效引擎。
   UE5.7 提供两种方案：MetaSound（node-based，5.7 production-ready）和 Sound Cue（legacy，复杂逻辑已弃用）。
2. **音频中间件**：纯原生 UE 工具链 vs 引入 Wwise/FMOD。MVP 范围（2D 为主，约 15 个核心 SFX + 1-2 条 BGM，
   三路简单混音，无空间化）是否值得中间件引入成本。
3. **事件订阅层与 RNG 封装**：音频(22)订阅多个 owner delegate；随机变体走什么 RNG 流？
   订阅绑定/解绑在 C++ 还是 BP？这直接影响 headless 可测性（AC-21~AC-22）与联网重放安全性（audio CR-7）。

**不决策的代价**：若用 Sound Cue 实现程序化逻辑、后期发现 5.7 已弃，迁移成本高且资产无法 diff 审查；
若引入中间件后期才发现 MVP 用不到，引入了多余许可费 + 集成复杂度；
若音频随机复用骰子3 权威流，联网重放污染无法事后隔离。

### Current State

- 无任何音频实现。audio.md（GDD #22）已 Approved，OQ-Audio-4 明确标注「架构期 ADR，不阻 MVP 开工
  但音频实现前必须收口」。
- architecture.md §1.4 已做方向性裁定：「MetaSound 非 Sound Cue；三路混音总线；2D 为主无空间化；
  Sound Concurrency 防同帧叠音」。**本 ADR 将该方向裁定升格为可执行边界，钉死接口约束与实现守则。**

### Constraints

- **引擎约束**：UE5.7 `deprecated-apis.md` 明文：「Sound Cue (for complex logic) → MetaSounds」；
  `audio.md` 确认 MetaSound 为 RECOMMENDED production-ready。Sound Cue 简单场景仍可用，但复杂节点逻辑应迁 MetaSound。
- **BP-vs-C++ 边界约束**：ADR-0007 裁定「状态机/确定性逻辑归 C++，呈现/资产装配归 BP」。
  音频订阅绑定属于与状态机紧耦合的生命周期纪律（读档重绑 EC-8、防双绑 AC-21），归 C++ 层。
- **联网预留约束**：音频随机**不得污染骰子3 权威流**（audio CR-7 / ADR-0004 流隔离纪律），
  否则 Full Vision 联网重放状态将被音频抖动扰乱。
- **Headless 可测约束**：[Logic] AC（AC-1~AC-22）须在 `-nullrhi` headless 跑通；
  音频实际波形不可 headless 测，但**触发逻辑/增益/优先级/节流状态机**须可 spy mock 测。
  这要求订阅层与播放层之间存在可注入的「播放接口」（非直接调 `PlaySound2D`）。
- **MVP 范围约束**：约 15 个核心 SFX + 1-2 条 BGM；2D 为主无空间化；三路简单混音（SFX/BGM/Master）；
  无 ducking 自动化、无动态分层（降 Alpha）。规模不支持中间件引入成本。
- **平台约束**：PC / Steam，60 FPS，音频帧预算属次要关注（回合制，非高频音效密度游戏）。

### Requirements

- **R-1 [程序化音效]**：骰点变体（`bIsDouble` 分支）、金币递增（参数化增益/pitch）须在资产层实现程序化逻辑，
  不依赖 C++ 手写分支条件逐资产切换。
- **R-2 [基础播放稳定性]**：基础 2D 播放（掷骰/移动/UI点击）须走已验证的稳定 API；不引入 5.4-5.7 post-cutoff 风险。
- **R-3 [三路混音]**：Master / SFX / BGM 三路独立音量参数，支持玩家在设置屏调节；BGM 破产短暂压低（F-4 阶跃）。
- **R-4 [Voice 并发上限]**：全局 `N_voice_max`（默认 32）限制活跃 voice 数；Critical 优先级（破产/胜利）不被抢占（CR-8/F-2）。
- **R-5 [随机流隔离]**：音频变体/pitch 随机走 #22 独立 `FRandomStream`，与骰子3 权威流物理隔离（CR-7）。
- **R-6 [Headless 可测]**：AC-1~AC-22 [Logic] 须可在 `-nullrhi` 用 spy/mock 断言；播放接口须可注入。
- **R-7 [读档重绑安全]**：`OnGameLoaded` 后每个 delegate 先 Unbind 再 Bind（EC-8 / AC-21 防双绑）。
- **R-8 [MVP 中间件上限]**：MVP 不引入 Wwise/FMOD；纯原生工具链。

---

## Decision

### 选型总裁定

| 维度 | 裁定 | 理由摘要 |
|------|------|----------|
| **程序化音效引擎** | **MetaSound**（`UMetaSoundSource`） | 5.7 RECOMMENDED production-ready；Sound Cue 复杂逻辑已弃；node graph 可 diff |
| **基础播放 API** | **`PlaySound2D` / `UAudioComponent`** | pre-5.3 稳定；两者均支持 MetaSound 资产（audio.md 已核验） |
| **音频中间件** | **纯原生**（MVP） | MVP 规模不支持中间件引入成本；Wwise/FMOD 降 Full Vision Alpha 评估 |
| **事件订阅层** | **C++**（随 ADR-0007） | headless spy 可测；生命周期纪律（防双绑/读档重绑）在 C++ 精确控制 |
| **混音总线** | **Sound Class / Submix 三路**（Master/SFX/BGM） | pre-5.3 稳定 API；`PushSoundMixModifier` 支持 BGM duck |
| **Voice 并发管理** | **`USoundConcurrency`**（Stop Oldest）+ C++ priority preemption（F-2） | 资产层限制同 Cue 并发；C++ 层实现 Critical 不被抢占不变式 |
| **音频随机** | **#22 自建独立非权威 `FRandomStream`** | 继承 ADR-0004 流隔离纪律；防权威流污染 |
| **BGM 管理** | **`UAudioComponent`**（持久，`FadeIn`/`FadeOut`）| 无缝 loop + crossfade；`T_bgm_crossfade=0.5s` |

### Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  PLATFORM LAYER（UE 5.7 引擎音频）                                                      │
│  UMetaSoundSource（程序化音效）│ USoundWave（简单 SFX）│ UAudioComponent（BGM 持久）      │
│  USoundClass（Master/SFX/BGM）│ USoundSubmix │ USoundConcurrency │ USoundMix（duck）   │
└──────────────────────────────────────────────┬───────────────────────────────────────┘
                                               │ 引擎 Audio API（C++ 封装层调用）
┌──────────────────────────────────────────────┴───────────────────────────────────────┐
│  C++ 音频系统核心（UAudioSubsystem_Rento 或 UWorldSubsystem 挂载）                        │
│                                                                                      │
│  ┌─ 订阅管理器 ────────────────────────────────────────────────────────────────┐     │
│  │  BindAll() / UnbindAll()（读档重绑：先 Unbind 再 Bind，EC-8 / AC-21）          │     │
│  │  订阅列表：OnDiceRolled / OnPhaseChanged / OnTurnStarted / OnGameWon            │     │
│  │           OnPawnMoveStarted / OnPawnLanded                                   │     │
│  │           OnCashChanged / OnRentPaid / OnBankruptcyDeclared                   │     │
│  │           OnOwnershipChanged / OnMortgageChanged                              │     │
│  │           OnBuildingChanged / OnCardDrawn / OnTileEventResolved               │     │
│  │           OnPlayerJailed / OnPlayerReleased                                   │     │
│  └────────────────────────────────────────────────────────────────────────────┘     │
│                                                                                      │
│  ┌─ 事件处理器 → 播放决策层 ─────────────────────────────────────────────────────┐     │
│  │  各 OnXxx 回调 → CueSelector::Resolve(EventId, Payload) → FAudioPlayRequest   │     │
│  │  CueSelector：从 payload 字段（bIsDouble/EChangeReason/delta 等）选定 SoundId  │     │
│  │  含 CR-3 ConsecutiveDoubles 缓存（重置/读取）+ house_count_cache（CR-5 delta）  │     │
│  └────────────────────────────────────────────────────────────────────────────┘     │
│                                                                                      │
│  ┌─ 播放执行层（可注入 IAudioPlaybackInterface，供 headless mock）─────────────────┐   │
│  │  ThrottleGate（F-3 per-Cue 节流）→ VoiceManager（F-2 抢占/丢弃）               │   │
│  │  → IAudioPlaybackInterface::Play(SoundId, Gain, PitchMult)                    │   │
│  │  生产实现：UAudioPlayback_UE（调 PlaySound2D / UAudioComponent::SetSound）     │   │
│  │  测试实现：UAudioPlayback_Mock（spy 计数器，headless [Logic] AC）               │   │
│  └────────────────────────────────────────────────────────────────────────────┘     │
│                                                                                      │
│  ┌─ 随机服务 ──────────────────────────────────────────────────────────────────┐     │
│  │  FAudioRNG（持 FRandomStream，独立非权威，与骰子3 权威流物理隔离）                │     │
│  │  SelectVariant(pool) → index；PitchJitter(range) → float（CR-7）              │     │
│  └────────────────────────────────────────────────────────────────────────────┘     │
│                                                                                      │
│  ┌─ BGM 管理器 ────────────────────────────────────────────────────────────────┐     │
│  │  UAudioComponent* BGMComp（持久循环）；CrossfadeTo(asset, T_bgm_crossfade)    │     │
│  │  SetBGMGain(target, duration)（F-4 BGM duck / 胜利淡出）                      │     │
│  └────────────────────────────────────────────────────────────────────────────┘     │
│                                                                                      │
│  ┌─ 混音总线管理（Sound Class / Submix 三路）─────────────────────────────────────┐   │
│  │  SetBusVolume(EAudioBus::Master/SFX/BGM, float V)                             │   │
│  │  → UGameplayStatics::SetSoundMixClassOverride 或 Submix 参数写                │   │
│  └────────────────────────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────────────────────────┘
                                               ▲ PlayUISound(EUISoundId)
                               UI 层（#16/#20/#23）调入（CR-11，唯一被调接口）
```

### Key Interfaces

```cpp
// ─── 音频随机服务（C++，独立非权威流） ───────────────────────────────────────────
// 与骰子3 FRandomStream 物理隔离；#22 自建，生命周期随 Audio Subsystem
class FAudioRNG
{
public:
    void    Initialize(int32 Seed = 0);            // 开局/读档重建时注入种子
    int32   SelectVariant(int32 PoolSize);         // [0, PoolSize-1]，走独立流
    float   PitchJitter(float HalfRange);          // [-HalfRange, +HalfRange]
    // 禁调 FMath::RandRange / FMath::FRand（ADR-0004 流隔离）
private:
    FRandomStream Stream; // 独立非权威，仅供 #22 使用
};

// ─── 播放接口（可注入，供 headless mock）────────────────────────────────────────
UINTERFACE(MinimalAPI)
class IAudioPlaybackInterface : public UInterface { GENERATED_BODY() };

class IAudioPlaybackInterface
{
public:
    // 触发一次 2D 音效（已经过 ThrottleGate + VoiceManager 后才进入此层）
    virtual void Play(FName SoundId, float Gain, float PitchMultiplier) = 0;
    // BGM 操作
    virtual void PlayBGM(FName SoundId, float FadeDuration)            = 0;
    virtual void SetBGMGain(float TargetGain, float FadeDuration)      = 0;
};

// ─── 混音总线（设置屏写，音频系统持参数语义）────────────────────────────────────
UENUM(BlueprintType)
enum class EAudioBus : uint8 { Master, SFX, BGM };

UFUNCTION(BlueprintCallable, Category="Audio|Mix")
void SetBusVolume(EAudioBus Bus, float Volume); // [0.0, 1.0]；#23 调

// ─── UI 音接口（#22 对 UI 层的唯一 Exposes）──────────────────────────────────
UENUM(BlueprintType)
enum class EUISoundId : uint8 { Click, Confirm, Cancel, Disabled, Hover };

UFUNCTION(BlueprintCallable, Category="Audio|UI")
void PlayUISound(EUISoundId SoundId); // CR-11；F-3 节流内含

// ─── MetaSound 参数化（骰点变体/金币递增，生产实现细节）─────────────────────────
// UAudioComponent::SetFloatParameter / SetIntParameter 在资产内配置参数 Input 节点
// 示例：骰子音 MetaSound 接收 FName("IsDouble") = bool，驱动内部变体选择
// （注：5.7 签名须实现期对照 docs 核实，见 Engine Compatibility Verification Required ①）
```

### Implementation Guidelines

#### G-1 MetaSound 资产分工

- **程序化逻辑**（2 个 die 变体、bIsDouble 分支、pitch 抖动模板）：用 **MetaSound Source**。
  在 MetaSound node graph 内建 `Random Get`/`Trigger Repeat`/参数 Input 节点。
  C++ 通过 `UAudioComponent::SetBoolParameter(FName("IsDouble"), bIsDouble)` 等注入参数——
  **不在 C++ 手写 if-else 分支逐资产切换**（音效逻辑内聚在资产）。
- **简单 SFX**（UI点击、落地"咚"、入狱铁门等无程序化逻辑）：可用 **Sound Wave**（直接引用）
  或包装为简单 Sound Cue（随机选变体 Random 节点）。**禁用 Sound Cue 的 Concat/Mixer/Delay 等复杂节点**
  （复杂逻辑已弃用；改 MetaSound）。
- **BGM**：Sound Wave + Loop（Sound Wave 属性勾选 Looping）挂 `UAudioComponent`，
  通过 `FadeIn`/`FadeOut`/`SetVolumeMultiplier` 实现 crossfade 和 duck。

#### G-2 三路 Sound Class / Submix 结构

```
Sound Class 层级:
  SC_Master
    ├── SC_SFX        (所有 gameplay + UI 音效)
    └── SC_BGM        (背景音乐)

Sound Submix 层级（与 Sound Class 并行，供 Audio Modulation 扩展预留）:
  SSX_Master
    ├── SSX_SFX
    └── SSX_BGM

Sound Mix（BGM duck）:
  SM_BankruptcyDuck   → SC_BGM 音量 × DUCK_FACTOR，T_bgm_crossfade 时长

每个 SFX Sound Wave / MetaSound 资产：
  属性 Sound Class = SC_SFX；Submix Send = SSX_SFX
BGM Sound Wave：
  属性 Sound Class = SC_BGM；Submix Send = SSX_BGM
```

SetBusVolume 实现：
- SFX 音量 → `UGameplayStatics::SetSoundMixClassOverride(SM_SFXVolume, SC_SFX, Volume, 1.0f, 0.1f)`，
  或写 `USoundSubmix` 的 `OutputVolume`（5.7 须核实哪条路径更稳定，见 Verification Required ③）。

#### G-3 Voice 并发管理

- **Sound Concurrency 资产**（`SCY_SFX`）：`MaxCount=32`（即 `N_voice_max` 默认值），
  Resolution Rule = Stop Oldest（低优先级被抢占）。挂在 SC_SFX 所有音效。
- **Critical 不被抢占**（F-2 不变式）：破产/胜利音使用独立 `SCY_Critical`（`MaxCount=4`，无上限抢占）。
  实现期若发现引擎 Concurrency 资产不支持「永不被抢占」语义，则升到 C++ VoiceManager 层实现。
- **ThrottleGate**（F-3 节流）：C++ per-Cue `TMap<FName, float> LastPlayTime` 缓存；
  `(GetWorld()->GetTimeSeconds() - LastPlayTime[SoundId]) >= T_sfx_min_retrigger` 为 true 才通过。

#### G-4 事件订阅层（C++ 强制）

遵循 ADR-0007：事件订阅/解绑逻辑在 C++ 内精确控制。
- `BindAll()` / `UnbindAll()` 集中管理，在 Subsystem `Initialize` / `Deinitialize` / 读档重绑（`OnGameLoaded`）调用。
- 每个 delegate 用 `AddDynamic(this, &Handle)` 绑定；读档重绑时先 `Remove` 再 `AddDynamic`（防双绑 EC-8 / AC-21）。
- **禁止 BP Widget 直接订阅 gameplay delegate**（BP 仅通过 `PlayUISound` 调 #22）。

#### G-5 音频随机流注入

- `FAudioRNG` 在 Audio Subsystem `Initialize` 时以系统时间或固定 seed（测试模式）初始化。
- 读档重建（EC-8）：从存档快照 re-seed（MVP 音频不序列化 Seed，用新非确定值——同 ADR-0004 audio 流约定）。
- **静态分析门**（随 ADR-0007 强度）：在代码 review checklist 加「音频 `.cpp` 中 grep `FMath::RandRange` / `FMath::FRand` 出现则 BLOCK」，确保音频随机不旁路 `FAudioRNG`（AC-22 代码审查软约束）。

#### G-6 headless 测试可行性

- `IAudioPlaybackInterface` 注入点：测试时注入 `UAudioPlayback_Mock`，spy 计数器记录 `Play(SoundId, Gain, Pitch)` 调用。
- [Logic] AC（AC-1~AC-22）在 `-nullrhi` 通过 mock 断言，**不依赖实际音频设备**。
- MetaSound 在 headless 下参数注入须核实（Verification Required ④）；若不可测，对应 AC 降为 [Advisory·code-review]。

---

## Alternatives Considered

### Alternative 1: 全部用 Sound Cue（不引入 MetaSound）

- **Description**: 所有 SFX 用 Sound Cue（Random 节点选变体、Modulator 节点调 pitch），
  程序化逻辑也在 Sound Cue node graph 内处理；不使用 MetaSound。
- **Pros**: 团队对 Sound Cue 编辑器更熟悉（pre-5.3 学习材料多）；基础场景不需要程序化时完全够用。
- **Cons**: `deprecated-apis.md` 明文弃用「Sound Cue (for complex logic)」→ MetaSounds；
  Sound Cue 的 node graph 存储为 `.uasset` 二进制，**无法 git diff 审查**（ADR-0007 代码可审性要求）；
  未来迁移成本高；不符合 5.7 现代范式。
- **Estimated Effort**: 初始低（无迁移），但未来技术债高。
- **Rejection Reason**: 违反 `deprecated-apis.md` 强烈建议；复杂逻辑弃用有官方背书，
  选 Sound Cue 等同主动接受技术债；且 binary uasset diff 不可审与项目代码审查要求冲突。

### Alternative 2: 引入 Wwise（MVP 即集成中间件）

- **Description**: 用 Wwise（或 FMOD）作为中间件，所有音效资产在 Wwise 项目内管理；
  UE 集成插件触发播放；动态混音、参数化、自适应配乐均在 Wwise 内实现。
- **Pros**: 行业级混音工具；自适应配乐/Sidechain Ducking/RTPC 参数化能力远超原生；
  未来动态音乐（OQ-Audio-1）实现成本低。
- **Cons**: **MVP 规模不支持**：约 15 个 SFX + 1-2 条 BGM + 三路简单混音，原生工具链已足够；
  Wwise 集成引入额外许可费（商业发布后）；引擎插件集成复杂度（`.wnproj` 外部项目管理）；
  团队须学习 Wwise 工具链；`Wwise Unreal Integration` 在 5.7 的兼容性须单独核实（post-cutoff 风险）。
- **Estimated Effort**: 高（集成 + 团队培训 + 持续许可费）。
- **Rejection Reason**: MVP 需求规模与中间件引入成本不成比例；Reversibility 极低（迁入容易迁出极难）；
  留 Full Vision Alpha 阶段评估（OQ-Audio-1 动态音乐需求落地后再判断 ROI）。

### Alternative 3: BP 层订阅 owner delegate（事件订阅在 Blueprint）

- **Description**: 音频(22) 的 owner delegate 订阅在 Blueprint Widget 或 BP Actor 内 `AddDynamic`；
  C++ 仅暴露 `PlaySound2D` 封装；触发逻辑在 BP graph 内实现。
- **Pros**: 设计师可在 Blueprint 编辑器直接调整事件→音效映射，无需重编译。
- **Cons**: **违反 ADR-0007 C++ 权威逻辑原则**——事件订阅生命周期（`BindAll`/`UnbindAll`/防双绑）属于
  与状态机紧耦合的确定性纪律，不属于「呈现/资产装配」范畴；
  BP 节点 headless 测试困难（AC-21 防双绑 spy 在 BP 中无法精确注入 mock）；
  读档重绑（EC-8）的「先 Unbind 再 Bind」序列在 BP 中无法静态核实；
  BP `uasset` 二进制不可 diff（同 Alternative 1 可审性问题）。
- **Estimated Effort**: 初始低，测试/维护成本高。
- **Rejection Reason**: 违反 ADR-0007；headless 可测性缺口（AC-21 将变 false-green）；
  防双绑纪律在 C++ 更可靠。

---

## Consequences

### Positive

- **MetaSound 程序化逻辑内聚**：骰点变体、bIsDouble 分支、pitch 抖动逻辑在 MetaSound graph 内管理，
  C++ 层只注入参数，不手写分支——资产层与代码层各司其职。
- **5.7 现代范式对齐**：选 MetaSound 符合 `deprecated-apis.md` 和 `audio.md` 双重背书，
  消除后期迁移风险。
- **headless 可测**：`IAudioPlaybackInterface` 注入点使 AC-1~AC-22 [Logic] AC 可在 `-nullrhi` 跑通，
  触发逻辑/增益/优先级/节流全可 spy 断言。
- **RNG 流安全**：`FAudioRNG` 独立非权威流严格隔离权威流，联网重放安全基础就位。
- **MVP 成本最优**：纯原生工具链零额外许可费；三路 Sound Class/Submix 结构简单够用，
  Full Vision 动态音乐需求出现时再评估中间件 ROI。

### Negative

- **MetaSound 学习曲线**：若团队 Sound Cue 经验更深，需补 MetaSound node graph 熟悉度。
- **post-cutoff API 核实成本**：`UAudioComponent::SetFloatParameter` / Sound Mix vs Audio Modulation 路径
  须实现期对照 5.7 文档核实（Verification Required ①③），不能直接用模型记忆。
- **`IAudioPlaybackInterface` 抽象层**：增加了一层间接性（接口 + Mock + 生产实现），
  代码量比直接调 `PlaySound2D` 略多；但 headless 可测性收益覆盖此成本。
- **Sound Cue 简单资产的延续性**：对于无程序化逻辑的简单 SFX，Sound Cue 仍可用（弃用仅限复杂逻辑），
  但规则须明确（G-1 已划定边界）防止随时间混用导致不一致。

### Neutral

- **BGM duck 方式**：MVP 用 `PushSoundMixModifier` 阶跃（非 sidechain 实时 ducking），
  听感上是简单音量跳变而非平滑包络——这是 MVP 简化（audio CR-12/F-4），Alpha 可升级。
- **Wwise/FMOD 留空槽**：未来如果动态配乐 OQ-Audio-1 需求落地，中间件 ROI 才值得重新评估；
  当前选纯原生不锁死，但切换中间件仍有较大迁移成本。

---

## Risks

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| MetaSound 5.7 参数化 API 签名有细微变化（Verification Required ①） | 中 | 中（影响程序化逻辑接口封装） | 实现前查 5.7 官方文档核实；封装层隔离变化影响面 |
| Sound Mix 被 Audio Modulation 部分取代（Verification Required ③） | 低-中 | 中（影响三路混音 SetBusVolume 实现路径） | 实现前核实；若 Audio Modulation 更稳定则改走该路径，不影响对外接口 |
| MetaSound headless 参数注入不可测（Verification Required ④） | 低-中 | 中（影响 AC-1~AC-22 中依赖参数化的 Logic AC） | 若不可测，对应 AC 降为 [Advisory·code-review]，spy 只断言"调用了 Play"不断言参数值 |
| Sound Cue vs MetaSound 边界模糊导致团队混用 | 中 | 低-中（影响可审性和技术债） | G-1 明确规则：复杂逻辑 → MetaSound，无逻辑简单 SFX → Sound Wave；Sound Cue 仅限 Random 节点单一场景 |
| `FAudioRNG` 被误绕过（直接用 `FMath::RandRange`） | 低 | 高（联网重放污染） | 代码 review checklist 静态扫描门（G-5）；CI 可加 `grep -r "FMath::RandRange" Source/Audio/` |

---

## Performance Implications

| 指标 | 基准（无音频） | 预期（本 ADR 实现后） | 预算 |
|------|----------------|----------------------|------|
| CPU（音频线程） | 0 ms | ~0.5–1.0 ms（32 voice，回合制低密度） | 16.6 ms 帧预算内（音频线程独立，不占游戏线程） |
| 内存（声音资产） | 0 MB | ~20–40 MB（15 SFX × 约 100KB–500KB/样本 × 3变体 + 2 BGM 流式） | 未定（待目标硬件确定，BGM 用流式降峰值） |
| 加载时间（SFX） | — | 微秒级（SFX 全加载；BGM 流式不阻启动） | 不影响启动时间（SFX 体积小） |
| Voice 峰值（级联破产 N 格） | — | 节流后 ≤ 32 voice（F-3 + SCY_SFX MaxCount） | `N_voice_max=32` 默认上限，可调 [16,64] |

> BGM 使用 **流式加载**（Sound Wave 属性 Streaming=true）避免 PIE 启动内存峰值。
> SFX 样本体积小，全加载（`Streaming=false`）以零运行时 I/O 开销换取即触即播。

---

## Migration Plan

当前无音频实现，不存在旧系统迁移。以下为**从零到音频架构就位**的施工步骤：

1. **基础设施就位**：创建 Sound Class 层级（`SC_Master/SFX/BGM`）+ Submix（`SSX_Master/SFX/BGM`）+
   Sound Concurrency 资产（`SCY_SFX, SCY_Critical`）；在 Project Settings 配置默认 Sound Class。
   [验证：Project Settings > Audio > Default Sound Class 正确指向 SC_Master]

2. **C++ 框架**：实现 `IAudioPlaybackInterface`（接口）+ `UAudioPlayback_UE`（生产实现）+
   `UAudioPlayback_Mock`（测试 mock）+ `FAudioRNG` + `ThrottleGate` + 订阅管理器 `BindAll/UnbindAll`。
   [验证：headless `-nullrhi` 下 `UAudioPlayback_Mock` 可接收 `Play` 调用并 spy 计数]

3. **核心 SFX 资产**：创建 CR-2 事件映射表中 MVP 全部 15 个 SFX 的 MetaSound Source 资产，
   每个资产备 2–3 变体样本（`cue_variant_count` 默认 3）；骰子翻滚 MetaSound 接入 `IsDouble` 参数 Input。
   [验证：MetaSound 编辑器内变体选择节点连接正确]

4. **BGM 资产**：创建 1-2 条 BGM Sound Wave（Looping=true，Streaming=true，Sound Class=SC_BGM）；
   初始化 `UAudioComponent* BGMComp`，绑定 `OnGameWon`/`OnBankruptcyDeclared` 回调。
   [验证：PIE 下 BGM 自动起播，OnBankruptcyDeclared 触发时 BGM gain 降至 DUCK_FACTOR 倍]

5. **Logic AC 测试**：用 `UAudioPlayback_Mock` 注入，逐条跑 AC-1~AC-22 [Logic] 测试。
   [验证：`tests/unit/audio/` 全绿，无 headless 假阳性]

6. **Integration AC 测试**：端到端跑 AC-23/AC-24（事件格7 入狱音 + 破产音链路）。
   [验证：`tests/integration/audio/` 全绿]

**Rollback plan**: 如 MetaSound 5.7 API 核实后发现重大不兼容，可回退步骤 3 改用 Sound Wave + Sound Cue
（Random 节点仅限变体选择），程序化参数逻辑退化为 C++ 多资产切换。代价：C++ 需手写枚举分支，
资产层 diff 能力丧失（二进制 uasset）；但基础播放/混音/优先级体系不受影响（步骤 1/2 仍有效）。

---

## Validation Criteria

- [ ] **MetaSound 参数化接口核实**：`UAudioComponent::SetBoolParameter(FName("IsDouble"), bool)` 在 UE 5.7
  可通过 C++ 注入并在 MetaSound graph 正确读取（骰子变体分支可驱动）。
- [ ] **headless mock 可注入**：`UAudioPlayback_Mock` 在 `-nullrhi` 下 `Play()` 可被 spy 断言；
  AC-1~AC-22 [Logic] 全绿（含 F-1 增益计算、F-2 抢占、F-3 节流、F-4 BGM duck 逻辑）。
- [ ] **RNG 流隔离核实**：code-review checklist 扫描 `Source/Audio/` 目录，无 `FMath::RandRange` /
  `FMath::FRand` 直调；`FAudioRNG::Stream` 实例与 DiceService `FRandomStream` 实例地址不同（spy 断言）。
- [ ] **三路混音结构**：`SetBusVolume(SFX, 0.0)` 静音所有 SFX 但 BGM 不受影响（反向亦然）；
  `SetBusVolume(Master, 0.0)` 全部静音。
- [ ] **Critical 不被抢占**：`N_voice_max=1`（满 1 Secondary），破产音（Critical）到达 → Secondary 被抢占，
  破产音播放（AC-14）；反向不成立（AC-15）；双 Critical 同帧均播（AC-16/EC-7）。
- [ ] **读档重绑安全**：`OnGameLoaded` 后 `OnRentPaid` 事件仅触发 1 次金币音（非 2 次），
  证无双绑（AC-21）。
- [ ] **[Advisory·playtest-signoff]**：mastering pass 确认 `G_cue` 跨 Cue 响度平衡；
  双点庆祝音 vs 入狱警告音盲听可分。

---

## GDD Requirements Addressed

| GDD 文档 | 系统 | 需求 | 本 ADR 满足方式 |
|----------|------|------|----------------|
| `design/gdd/audio.md` | Audio #22 | **OQ-Audio-4**：BP vs C++ 边界 + 音频中间件 + `FRandomStream` 封装（架构期 ADR 门控） | 裁定：C++ 订阅层 + 纯原生工具链 + `FAudioRNG` 独立流；OQ-Audio-4 收口 |
| `design/gdd/audio.md` | Audio #22 | **CR-7** 随机变体走 #22 自建独立非权威 `FRandomStream`，禁复用骰子3 权威流 | `FAudioRNG` 独立 `FRandomStream` + G-5 静态扫描门 |
| `design/gdd/audio.md` | Audio #22 | **CR-12** 三路混音总线（Master/SFX/BGM）+ 设置屏读写 | Sound Class / Submix 三路结构 + `SetBusVolume(EAudioBus, float)` 接口 |
| `design/gdd/audio.md` | Audio #22 | **CR-6/F-2/F-3** Voice 并发上限 + 优先级抢占 + 节流 | `USoundConcurrency` + C++ `ThrottleGate` + `VoiceManager`（含 Critical 不变式） |
| `design/gdd/audio.md` | Audio #22 | **CR-8 Critical 不变式**：破产/胜利绝不被抢占、绝不丢弃 | `SCY_Critical` 独立 Concurrency + F-2 EC-7 双 Critical 临时超上限 |
| `design/gdd/audio.md` | Audio #22 | **EC-8** 读档重建先解绑再重绑，防双订阅 | `BindAll`/`UnbindAll` C++ 集中管理；`OnGameLoaded` 触发重绑序列（AC-21） |
| `design/gdd/audio.md` | Audio #22 | **AC-22** RNG 隔离 code-review 软约束（继承 ai CR-5④ / VFX19 CR-9） | G-5 静态分析门 + review checklist |
| `design/gdd/audio.md` | Audio #22 | **CR-11** `PlayUISound(EUISoundId)` 对 UI 层唯一 Exposes 接口 | `UFUNCTION(BlueprintCallable) PlayUISound(EUISoundId)` 接口定义 |
| `docs/architecture/architecture.md` §1.4 | Platform Audio | MetaSound + 三路混音 + Sound Concurrency（architecture 方向裁定） | 本 ADR 将方向裁定升格为可执行边界，钉死接口约束与实现守则 |
| `docs/architecture/architecture.md` §8 ADR-0010 条目 | Required ADR | 裁定 ①MetaSound vs Sound Cue ②中间件 ③订阅 C++/BP ④FRandomStream 封装 ⑤三路混音图 | 全部五项在本 ADR Decision 节收口 |

---

## Related

- **依赖**：ADR-0003（事件总线契约，音频 delegate 签名/防双绑基础）
- **依赖**：ADR-0004（确定性 RNG，`FAudioRNG` 独立流隔离纪律直接继承此 ADR）
- **依赖**：ADR-0007（BP-vs-C++ 边界，事件订阅层归 C++ 的根据）
- **Enables**：音频系统 #22 实现开工（本 ADR Accepted 后 OQ-Audio-4 收口）
- **协调**：ADR-0009（卡通材质，可与本 ADR 并行，不触同文件）
- **GDD**：`design/gdd/audio.md`（Approved，本 ADR 的设计需求来源）
- **架构总纲**：`docs/architecture/architecture.md §1.4 + §8 ADR-0010 条目`
