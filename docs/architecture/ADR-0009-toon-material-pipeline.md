# ADR-0009: 卡通材质管线（Legacy vs Substrate）

## Status

Accepted

## Date

2026-06-06

## Last Verified

2026-06-06

## Decision Makers

- msc（用户）+ Technical Director — 2026-06-06 Accepted（用户裁定全接受推荐选型：Legacy Unlit，关 Lumen/Nanite/Megalights；Substrate 迁移留 Alpha 评估）
- Art Director（视觉需求输入）
- Lead Programmer（实现可行性）

## Summary

本项目采用卡通 2.5D 风格，需要选定材质框架：Legacy Unlit/自定义节点（直接、团队熟悉、2.5D 完全够用）vs Substrate（UE 5.7 production-ready、面向未来一致性但卡通 Unlit 路径有 post-cutoff 知识盲区）。本 ADR 根据卡通描边/平涂/色阶材质需求、项目禁用 Lumen/Nanite/Megalights 的减法配置策略，以及 Substrate 卡通可行性核验结果，裁定 MVP 材质框架选 **Legacy Unlit + 自定义节点**（关 Lumen/Nanite/Megalights，显式禁用 Substrate）；Substrate 迁移列 Alpha 评估。

---

## Engine Compatibility

| 字段 | 值 |
|------|----|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Rendering |
| **Knowledge Risk** | HIGH — Substrate 在 5.7 production-ready，但模型训练数据约在 5.3，Substrate Slab/Unlit 卡通用法为 post-cutoff |
| **References Consulted** | `docs/engine-reference/unreal/modules/rendering.md`、`docs/engine-reference/unreal/deprecated-apis.md`、`docs/engine-reference/unreal/current-best-practices.md`、`design/art/art-bible.md`（§3/4/6/8）、`design/gdd/vfx-feedback.md`（CR-7/OQ-VFX-6）、`docs/architecture/architecture.md`（§Engine Integration 1.3） |
| **Post-Cutoff APIs Used** | Substrate Unlit 节点（若选 Substrate 路径）——`rendering.md` 仅记录 Substrate Slab/Blend/ThinFilm/Hair，**未确认 Substrate Unlit 节点存在**；Post Process Material 在 5.7 Substrate 体系下接口变化情况；Inverted Hull 描边在 Substrate 开启后是否需要额外配置 |
| **Verification Required** | 若选 Substrate：① 确认 UE 5.7 中 Substrate 是否提供 Unlit Slab 或等效节点；② 确认 Inverted Hull 描边材质在 Substrate 开启后正常工作；③ 确认 Post Process Material（胜利暖金 PP）在 Substrate 体系下 API 不变（OQ-VFX-6）。若选 Legacy：④ 确认 Legacy 材质与 Substrate 禁用状态下 Unlit 着色器不受 deprecated-apis 路线图影响（5.7 warning-only 还是 hard break） |

> **注意**：Knowledge Risk 标 HIGH。若选 Substrate 路径，实施前须对照 UE 5.7 官方文档（https://docs.unrealengine.com/5.7/en-US/substrate-materials-in-unreal-engine/）逐条核验上述 Verification Required 项，不可凭模型训练记忆臆造节点名称。

---

## ADR Dependencies

| 字段 | 值 |
|------|----|
| **Depends On** | ADR-0001（UObject 宿主）已 Accepted；ADR-0007（BP-vs-C++ 边界）Proposed（材质框架选定后，Post Process Material C++ 控制路径需对齐 ADR-0007 BP/C++ 边界） |
| **Enables** | 美术资产制作开工（棋子/建筑/棋盘材质、描边、VFX 材质）；ADR-0007 关联的 VFX #19 资产制作（胜利暖金 Post Process Material） |
| **Blocks** | 美术资产 Sprint（材质框架未定则所有 Material/MaterialInstance 资产无法开始，棋子描边/色阶材质/Post Process Volume 胜利暖金均阻） |
| **Ordering Note** | 本 ADR 可与 ADR-0010（音频架构）并行起草，不依赖彼此。须在美术资产 Sprint 开工前 Accepted。 |

---

## Context

### Problem Statement

UE 5.7 将 Substrate 标记为 production-ready，并在 `deprecated-apis.md` 中将 Legacy material nodes 列为"迁移到 Substrate"目标。项目同时明确了以下约束：

1. **art-bible 卡通视觉需求**：风格化手绘贴图、Unlit 或 Lit（无 PBR），禁止写实材质；棋子需要轮廓描边（outline stroke），art-bible §5.1 明确「UE5 Post Process Outline 或 Inverted Hull 方法」；胜利状态需要暖金 Post Process Material（VFX GDD CR-7，OQ-VFX-6）
2. **渲染减法配置**：architecture.md §1.3 已裁定 Lumen/Nanite/Megalights 全部关闭。卡通 2.5D 的 Sensation 来自 juice 动效而非渲染复杂度
3. **项目规模**：PC/Steam，60fps，低面数棋盘/棋子（LOD0 ≤ 2000 tris），无复杂光照需求
4. **团队背景**：Blueprint 为主的 indie 团队，材质编辑复杂度应与团队能力匹配
5. **后期迁移成本**：材质框架一旦定型，整个资产管线绑定——后期换框架需重建所有材质图，代价极高

**不决定的代价**：美术资产 Sprint 无法开工，棋子/房屋/酒店/棋盘材质全部阻塞。

### Current State

无已实现材质。项目处于设计/架构阶段，材质框架尚未选型，这是首次落地决策。

### Constraints

- **引擎版本**：UE 5.7，Substrate production-ready，Legacy 官方立场是"可用但建议迁移"
- **知识盲区**：Substrate 卡通 Unlit 路径模型不确定（post-cutoff HIGH risk），须核验
- **无 Lumen/Nanite/Megalights**：项目已显式关闭重型特性，材质框架选择不依赖这些特性
- **禁止写实 PBR**：art-bible §9.2 禁区 1，所有棋子/建筑材质为风格化哑光/半光泽
- **Post Process Outline 或 Inverted Hull**：描边是视觉身份的核心（art-bible §5.1），框架必须可靠支持
- **Post Process Material（胜利暖金）**：VFX GDD CR-7 + OQ-VFX-6 要求，须在所选框架下 API 稳定
- **团队规模**：Indie，Substrate 学习曲线须与收益匹配

### Requirements

- **R1 卡通 Unlit/平涂**：棋子、建筑、棋盘面、UI 道具支持 Unlit 或简单 Diffuse 着色，无物理光照计算
- **R2 轮廓描边**：棋子 1.5–2px 描边（art-bible §5.1），Post Process 或 Inverted Hull 方法，1080p 下稳定
- **R3 色阶材质**：支持 2–3 色调阶（卡通 cel-shading），实现方式为自定义着色节点或 Custom node
- **R4 Post Process Material**：胜利暖金、破产去饱和逐 pawn 材质参数（VFX GDD CR-7/States F/G），Post Process Volume `bUnbound=true` + `APostProcessVolume::Settings.bOverride_*` 接口在所选框架下可用
- **R5 动态材质实例**：`UMaterialInstanceDynamic::SetVectorParameterValue`/`SetScalarParameterValue` 运行时控制（破产去饱和、玩家颜色注入），C++ 接口在所选框架下稳定
- **R6 性能**：60fps @ PC，关闭 Lumen/Nanite/Megalights，低面数场景，材质 overdraw 控制
- **R7 资产命名**：符合 art-bible §8.2 规范（`M_`/`MI_`/`T_` 前缀），与框架无关

---

## Decision

### 选项评估（逐选项列代价）

---

#### 选项 A：Legacy Unlit + 自定义节点（已裁定 Accepted）

**描述**：使用 UE 传统材质系统，Unlit/Lit Blend Mode，卡通色阶通过 Custom HLSL 节点或 Ceil/Floor 节点实现，描边用 Inverted Hull（单独翻法线 mesh pass）或 Post Process 描边材质，Post Process Material 用标准 `APostProcessVolume`。

**技术路径**：
- 棋子/建筑/棋盘：Unlit Material，`BaseColor` 接贴图/颜色参数，无 Roughness/Metallic 计算
- 卡通色阶：`MaterialFunction` 封装 `Ceil(dot(N,L)*K)/K` 或 Custom HLSL（可复用）
- 描边：Inverted Hull（翻法线+放大+Unlit 单色 pass）—— 不依赖 Post Process，对卡通玩具风格最稳定
- Post Process Material（暖金/去饱和）：`APostProcessVolume`，`Settings.bOverride_ColorGradingIntensity` 等标准接口
- 动态材质实例：`UMaterialInstanceDynamic` C++ 接口稳定（pre-5.3，无变化风险）

**优点**：
- 团队熟悉，Blueprint 主导项目无额外学习曲线
- 卡通 Unlit 是传统 UE 成熟路径，Stack Overflow/资料丰富
- 与「禁用 Lumen/Nanite/Megalights」的减法策略完全对齐——Legacy 材质不依赖这些特性
- Inverted Hull 描边在 Legacy 体系下有大量 Unreal 社区验证案例
- Post Process Material API 在 pre-5.3 已稳定，模型训练数据可信度高
- **迁移风险低**：即使 Substrate 未来成为硬要求，Legacy→Substrate 迁移工具在 5.7 已存在（官方提供兼容模式）
- 2.5D 棋盘回合制无需 Substrate 的分层物理材质收益

**缺点**：
- deprecated-apis.md 将 Legacy material nodes 列为「建议迁移到 Substrate」，未来版本可能警告升级
- 长期来看不符合 UE 官方推荐方向（5.7 起 Substrate 为默认推荐）
- 若 Alpha/Full Vision 引入复杂材质特效（如命运之轮彩灯），Legacy 表现力略逊于 Substrate

**实现成本**：低（团队熟悉，无新系统学习）

---

#### 选项 B：Substrate 卡通路径（Substrate Unlit / Substrate Slab 限制使用）

**描述**：启用 Substrate（Project Settings > Engine > Substrate > Enable Substrate），使用 Substrate Unlit 节点（若存在）或受限 Substrate Slab（Roughness=1.0 / Metallic=0 近似 Unlit）实现平涂卡通，描边和 Post Process Material 在 Substrate 体系下实现。

**技术路径（⚠ 含知识盲区）**：
- **⚠ Substrate Unlit**：`rendering.md` 仅记录 Substrate Slab/Blend/ThinFilm/Hair 四个节点，**未记录 Unlit 节点**。若 UE 5.7 无 Substrate Unlit，卡通平涂须用 `Substrate Slab(Roughness=1, Metallic=0)` 模拟，仍有微量 GGX 高光计算——与 art-bible「禁止写实材质」的精神对齐但有轻微计算开销
- **⚠ Inverted Hull 在 Substrate**：Inverted Hull 用单独 mesh pass，理论上与 Substrate 兼容（材质模式独立），但 5.7 实测兼容性须验证（OQ-VFX-6 正式登记此盲区）
- **⚠ Post Process Material 接口**：`rendering.md` 的 Post Process 章节基于 pre-5.3 API，Substrate 开启后 PP Material 的 Blend Mode 选项是否变化须核验（OQ-VFX-6 范畴）
- 动态材质实例：API 在 Substrate 下与 Legacy 相同（`UMaterialInstanceDynamic` 接口不变），风险低

**优点**：
- 符合 UE 5.7 官方推荐方向，未来版本升级兼容性更好
- Substrate 的分层系统为 Alpha/Full Vision 阶段的复杂特效（命运之轮/俄罗斯轮盘材质）预留扩展
- 全项目材质体系统一，无 Legacy + Substrate 混用维护负担

**缺点**：
- **关键知识盲区（HIGH risk）**：Substrate Unlit 节点存在性未确认；Inverted Hull 兼容性未确认；PP Material 接口变化未确认
- Substrate 学习曲线对 Blueprint 主导团队不可忽视（节点体系不同）
- 卡通 2.5D 场景从 Substrate 获得的 PBR 分层收益接近于零——Substrate 的核心价值是物理精确分层，这与本项目「风格化哑光玩具感」需求不匹配
- 若 Substrate Unlit 不存在，须用 Slab 模拟，引入不必要的 GGX 计算（虽轻微）
- **迁移阻断**：一旦全项目用 Substrate 材质，引擎降级或协作工具（低版本）无法打开材质图

**实现成本**：高（新系统学习 + 知识盲区验证工作量 + 2.5D 收益存疑）

---

#### 选项 C：混合路径（Legacy 主体 + Substrate PP Material）

**描述**：棋子/建筑/棋盘材质使用 Legacy Unlit；仅胜利暖金/破产去饱和 Post Process Material 尝试 Substrate PP 节点（若收益明显），其余材质 Legacy。

**优点**：
- 分散风险：核心卡通材质走熟悉路径，PP 特效可探索 Substrate 收益
- 允许渐进迁移

**缺点**：
- Legacy + Substrate 混用造成材质系统割裂，维护负担加倍
- PP Material 的 Substrate 知识盲区仍存在（等同选项 B 的 PP 验证成本）
- 比「完全 Legacy」或「完全 Substrate」都更复杂，却不获得完整收益
- art-bible 描边（Inverted Hull）仍在 Legacy，PP 描边只用 Substrate 的收益不清晰

**实现成本**：中（两套系统并行维护）

---

### 裁定决策（Accepted）

**裁定选项 A：Legacy Unlit + 自定义节点（MVP），Substrate 迁移列 Alpha 评估（2026-06-06 由 msc Accepted）**

**裁定理由**：

1. **需求对齐**：卡通 2.5D 平涂的核心是「Unlit + 色阶节点」，这是 Legacy 材质的传统强项。art-bible 明确「禁止写实 PBR」，Substrate 的物理分层收益在本项目几乎为零。
2. **已知可行 vs 盲区风险**：Legacy Unlit + Inverted Hull 描边有大量 UE 社区案例验证；Substrate Unlit 节点存在性、PP Material 接口变化、Inverted Hull 兼容性均是 post-cutoff 盲区——在 Verification Required 全部清零前不应在 MVP 中押注。
3. **减法配置一致性**：项目已关闭 Lumen/Nanite/Megalights，选 Substrate 却不用其物理分层能力，收益/成本比极低（学习 Substrate 节点体系 + 知识盲区验证 >> 零收益）。
4. **迁移可逆**：Legacy→Substrate 的官方迁移路径在 5.7 存在（`deprecated-apis.md` 只写"建议迁移"而非"硬删除"），MVP 后若 Substrate 收益明确可在 Alpha 期系统性迁移，不影响 MVP 交付。
5. **团队能力匹配**：Blueprint 为主的 indie 团队选择熟悉的 Legacy 体系，可以立即开工，不消耗学习预算。

**关于 deprecated-apis.md 的 Legacy 标注**：`deprecated-apis.md` 将 Legacy 材质列为「建议迁移」而非「已移除」，5.7 仍然完整支持 Legacy 材质系统（包括 Unlit/Custom HLSL/Inverted Hull），这是官方兼容路径。只要在 Alpha 评估节点（本 ADR 的 Migration Plan 中明确）重新评估，不会产生技术债积压。

---

### Architecture

```
卡通材质管线（Legacy 路径）

┌─────────────────────────────────────────────────────────────────┐
│  Material 资产层（Content Browser）                               │
│  M_toon_unlit_base.uasset       ← 卡通平涂基础材质函数/父材质       │
│  M_toon_cel_shade.uasset        ← 含色阶节点的 Lit 变体            │
│  M_inverted_hull_outline.uasset ← 描边 pass（翻法线+Unlit）        │
│  M_postprocess_victory_warm.uasset ← 胜利暖金 PP 材质             │
│  M_postprocess_bankrupt_desaturate.uasset ← 破产去饱和 PP 材质    │
└──────────────────────┬──────────────────────────────────────────┘
                       │  MaterialInstance + MaterialInstanceDynamic
┌──────────────────────▼──────────────────────────────────────────┐
│  运行时控制层（C++ / Blueprint）                                   │
│  UMaterialInstanceDynamic::SetVectorParameterValue()             │
│  └─ 玩家颜色注入（MI_char_token_P1..P4 运行时染色）                 │
│  UMaterialInstanceDynamic::SetScalarParameterValue()             │
│  └─ 破产去饱和强度（VFX #19 订阅 OnBankruptcyDeclared 触发）        │
│  APostProcessVolume::Settings.bOverride_*                        │
│  └─ 胜利暖金（VFX #19 订阅 OnGameWon，bUnbound=true 全局 PP）      │
└──────────────────────┬──────────────────────────────────────────┘
                       │  蓝图可调参数（Tuning Knobs）
┌──────────────────────▼──────────────────────────────────────────┐
│  DA_MaterialConfig（Data Asset，数据驱动，遵 ADR-0002 原则）        │
│  - 描边宽度旋钮（1.5–2px @ 1080p）                                 │
│  - 色阶档数 K（2–4）                                               │
│  - 暖金 ColorGrading 强度                                         │
│  - 去饱和强度曲线                                                  │
└─────────────────────────────────────────────────────────────────┘

Inverted Hull 描边流程：
  SM_char_token_* 棋子 Mesh
  ├─ Mesh Pass 1（正常渲染）: M_toon_unlit_base（玩家颜色参数）
  └─ Mesh Pass 2（描边 pass）: M_inverted_hull_outline
       - Two-Sided OFF，Cull Front ON（翻法线向外）
       - 世界空间沿法线方向外扩 Outline_Width
       - Unlit 单色（棋子色深色版本，非纯黑）

Post Process Volume 胜利暖金：
  APostProcessVolume (bUnbound=true)
  └─ Material = M_postprocess_victory_warm
       - BlendWeight = 0 (default) → 1.0 (OnGameWon)
       - 色温偏移 + Fortune Gold 色调叠加
```

---

### Key Interfaces

```cpp
// ── 材质实例动态控制（C++ 权威，ADR-0007 原则）──

// 玩家颜色注入（对局初始化时，每 pawn 一个 MID）
UMaterialInstanceDynamic* TokenMID =
    UMaterialInstanceDynamic::Create(TokenBaseMaterial, PawnActor);
// 注入玩家色（FLinearColor，来自 FPlayerSetupEntry.PlayerColor）
TokenMID->SetVectorParameterValue(TEXT("PlayerColor"), PlayerLinearColor);
// 注入描边色（玩家色暗色版本，运行时算）
TokenMID->SetVectorParameterValue(TEXT("OutlineColor"), DarkenColor(PlayerLinearColor, 0.4f));
PawnMeshComp->SetMaterial(0, TokenMID);

// 破产去饱和（VFX #19 响应 OnBankruptcyDeclared）
// 注：这是逐 pawn 材质参数，非全屏 PP（art-bible 状态 G，VFX GDD §CR-6/States）
TokenMID->SetScalarParameterValue(TEXT("DesaturationAmount"), 1.0f); // 0=彩色, 1=全灰

// 胜利暖金 Post Process（VFX #19 响应 OnGameWon）
void ActivateVictoryPostProcess(APostProcessVolume* PPV) {
    PPV->Settings.bOverride_ColorGradingIntensity = true;
    // 暖金色调：通过 PP Material 的 BlendWeight 参数控制强度
    PPV->BlendWeight = 1.0f;
    // PP Material 内部实现：暖金色温 + Fortune Gold (#F4C430) 叠加
}

// ── 材质参数数据资产（数据驱动，遵 ADR-0001 Data-Driven 原则）──
USTRUCT(BlueprintType)
struct FToonMaterialConfig {
    GENERATED_BODY()
    // 描边旋钮
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float OutlineWidth = 1.8f;           // 1.5–2.0px @ 1080p
    // 色阶旋钮
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CelShadeSteps = 3;             // 2–4
    // 胜利暖金强度
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float VictoryWarmToneIntensity = 0.8f; // 0.0–1.0
    // 破产去饱和速度（秒）
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float BankruptcyDesaturationDuration = 1.5f; // 0.5–3.0s
};
```

---

### Implementation Guidelines

1. **父材质 + 材质实例层级**：建 `M_toon_unlit_base` 作父材质（Unlit，`BaseColor` = 参数），派生 `MI_char_token_*` / `MI_bld_house_*` 等实例。只在父材质改节点图，实例只改参数值——遵 art-bible §8.2 命名约定（`M_`/`MI_` 前缀）。

2. **卡通色阶节点封装**：将 `Cel Shade` 逻辑封装为 `MaterialFunction` `MF_CelShade(BaseColor, LightDir, Steps)`，可被多个材质复用，改 `Steps` 参数控制卡通风格档数（2–4档）。

3. **Inverted Hull 描边**：对棋子 mesh 使用双 pass 方案，不使用 Post Process 全屏描边（PP 描边会框住所有物体，Inverted Hull 只框指定 mesh，更精准）。`M_inverted_hull_outline` 的 Outline_Width 须随相机距离缩放（避免远景描边过粗），通过 Camera Distance 除法节点动态调整。

4. **Post Process Volume 管理**：关卡中放置一个 `APostProcessVolume`（`bUnbound=true`），VFX #19 持其引用（`TObjectPtr<APostProcessVolume>`，`UPROPERTY()`）。默认 `BlendWeight=0`，`OnGameWon` 时设为 1.0，`OnBankruptcyDeclared` 不触发全屏 PP（破产是逐 pawn 材质参数，非 PP，见 VFX GDD States G）。

5. **禁止 Nanite/Lumen 材质特性**：所有 Static Mesh 不勾选 Enable Nanite Support；材质不使用 Emissive 主驱动 Lumen（art-bible §8.5 明文规定）；若需发光效果用 Additive Blend 或 Post Process Material。

6. **Substrate 关闭**：确认 Project Settings > Engine > Substrate 为**禁用**（UE 5.7 默认开启，须显式关闭），避免 Legacy 材质与 Substrate 管线意外交叉。

7. **动态材质实例生命周期**：`UMaterialInstanceDynamic` 须用 `UPROPERTY()` 标记防 GC，在 pawn BeginPlay 时 `Create`，`EndPlay` / pawn 销毁时由 GC 回收（不手动 delete）。

8. **数据驱动旋钮**：描边宽度、色阶数、暖金强度、去饱和速度等全部置于 `DA_MaterialConfig` Data Asset，不硬编码——遵 ADR-0001 Architecture Principle 1（Data-Driven Gameplay）。

---

## Alternatives Considered

### Alternative 1：Substrate 全量（选项 B）

- **Description**：全项目材质使用 Substrate，卡通 Unlit 经 Substrate Unlit 节点或 Slab 限制实现
- **Pros**：符合 UE 5.7 官方推荐，未来版本升级兼容性佳；为 Alpha 复杂材质预留扩展
- **Cons**：Substrate Unlit 节点存在性是 post-cutoff 知识盲区（rendering.md 未记录）；卡通 2.5D 无法获得 PBR 分层的实质收益；Inverted Hull 和 PP Material 兼容性需验证；学习曲线与 indie 团队能力不匹配
- **Estimated Effort**：高（含知识盲区验证 + 新系统学习）
- **Rejection Reason**：在 Verification Required 全部清零前，高风险 + 低收益不适合 MVP；推迟到 Alpha 评估

### Alternative 2：混合路径（选项 C）

- **Description**：Legacy 主体 + Substrate Post Process 局部
- **Pros**：渐进迁移路径，分散风险
- **Cons**：两套系统并行维护，开销加倍；PP Material 盲区验证成本不减；割裂的材质体系增加 Technical Artist 负担
- **Estimated Effort**：中（但复杂度最高）
- **Rejection Reason**：混合路径同时继承两套系统的缺点而不能完全获得任一的优点；不如 Legacy 干净

---

## Consequences

### Positive

- 美术资产 Sprint 可立即开工，不等 Substrate 知识盲区验证
- 卡通 Unlit 材质制作路径清晰，社区资料丰富，Technical Artist 零学习曲线
- 与减法渲染配置策略（关 Lumen/Nanite/Megalights）完全一致
- Inverted Hull 描边在 Legacy 体系下稳定可预期
- 60fps 帧预算有余量（无重型材质计算）
- Legacy→Substrate 官方迁移路径存在，Alpha 评估有据可依

### Negative

- 与 UE 5.7 官方推荐方向（Substrate）短期背道而行，若引擎后续版本硬弃 Legacy 须全体迁移
- 若 Alpha/Full Vision 引入复杂视觉特效（如命运之轮彩灯 iridescence），需评估 Legacy 的表现力上限
- 不享受 Substrate 分层对未来扩展的架构便利

### Neutral

- Substrate 在 5.7 为"可用，推荐"而非"强制"；Legacy 仍完整支持，无编译警告或功能缺失
- Post Process Material API 在 Legacy 体系下 pre-5.3 稳定（模型训练数据可信），不依赖 post-cutoff 知识
- 资产命名（`M_`/`MI_`/`T_` 前缀）与框架无关，art-bible §8.2 规范不受此决策影响

---

## Risks

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| UE 未来版本将 Legacy 材质升为 hard-deprecated（强制报错而非警告） | 低（5.7 为 warning/建议，非 hard break） | 高（需全量迁移） | Alpha 评估节点重新审视；监控 UE 官方 release notes；若出现 hard deprecation 警告，提前立项 Substrate 迁移 Sprint |
| Substrate Unlit 节点在 5.7 不存在，导致「想迁」时需大量 Slab 模拟工作 | 中（rendering.md 未记录） | 中（迁移成本高于预期） | 本 ADR 标 Verification Required；Alpha 评估前先做 Substrate 卡通 POC |
| Inverted Hull 描边在特定棋子 mesh 上出现接缝/z-fight | 中（常见问题，有缓解方案） | 低（视觉瑕疵，不影响功能） | 描边 pass 增加 Depth Bias；或备选 Post Process 描边（Custom Depth Stencil 方法）|
| PP Material 胜利暖金在 Substrate 禁用后行为不符预期 | 低（PP Material 不依赖 Substrate 开关） | 中（胜利状态视觉） | ADR Accepted 后 VFX Technical Artist 立即做 PP Material POC 验证 |
| art-bible §5.1「10px 标签最小字号」与 Legacy 材质无关，但描边宽度单位（px vs world unit）在不同分辨率下漂移 | 中 | 低 | 描边宽度参数用相机距离驱动的世界空间-屏幕空间换算；QA 在 720p/1080p/1440p 三档验证 |

---

## Performance Implications

| 指标 | 实现前 | 预计实现后 | 预算 |
|------|--------|------------|------|
| 棋子材质 CPU 帧时间（per draw call） | — | < 0.1ms（Unlit，无光照计算） | 16.6ms 总预算 |
| Inverted Hull 描边额外 draw call | — | +N（N = 同屏棋子数，最多 4，< 1ms） | 可接受 |
| Post Process Volume（Victory PP） | — | < 0.5ms（full-screen quad，Unlit） | 可接受 |
| 破产去饱和（逐 pawn MID 参数） | — | CPU: < 0.01ms（参数设值），GPU: 零增量（着色器已在 MID 编译时固定） | 可忽略 |
| 材质内存（MI 池） | — | < 10 个 MID（4 玩家 × 棋子 + 建筑） | 可忽略 |

> 注：无 Lumen/Nanite/Megalights，本项目渲染瓶颈预期在 Niagara 粒子（VFX #19）和 UMG overdraw（HUD #16），非材质着色。具体帧分项数据须 Pre-Production 阶段 Unreal Insights 实测。

---

## Migration Plan

**MVP（当前）**：实施 Legacy Unlit + 自定义节点，完成卡通描边/色阶/PP Material 全套。

**Alpha 评估节点**（进入 Alpha Sprint 前）：
1. 整理 Legacy 材质实现的技术债清单（Substrate 迁移候选列表）
2. 创建 Substrate 卡通 POC（1 个棋子材质 + 1 个描边）验证：
   - Substrate Unlit 节点是否存在（对照 UE 5.7 官方文档 https://docs.unrealengine.com/5.7/en-US/substrate-materials-in-unreal-engine/）
   - Inverted Hull 在 Substrate 开启后是否正常工作
   - PP Material 接口是否变化
3. 若 POC 通过且 Substrate 收益明确（如命运之轮 iridescence 等 Alpha 特效需求），立项「材质管线迁移 Sprint」，用 UE 官方 Legacy→Substrate 迁移工具（Shader Convert Tool）批量迁移
4. 若 POC 失败或收益不足，维持 Legacy，在此 ADR 基础上写 ADR-0009a 记录决策

**回滚计划**：Legacy 材质框架无需回滚（本决策即从 Legacy 出发）。若 Alpha 评估后迁 Substrate 却遇问题，可回退到本 ADR 对应的 Legacy 资产 Git tag，代价是丢弃 Substrate 迁移工作量。

---

## Validation Criteria

- [ ] 棋子 `SM_char_token_*` 在所有玩家颜色（P1–P4）下 Inverted Hull 描边显示正确，1080p 下无接缝/z-fight
- [ ] 4 种颜色棋子同屏时 60fps 帧率维持（Unreal Insights stat unit ≤ 16.6ms）
- [ ] 卡通色阶材质（`M_toon_cel_shade`）2–3 色阶在 1080p 下视觉可辨，符合 art-bible §3/4 风格方向
- [ ] `M_postprocess_victory_warm` 在 `APostProcessVolume(bUnbound=true)` 下全屏正确渲染暖金效果，`BlendWeight` 0→1 过渡无闪烁
- [ ] 破产去饱和通过 `UMaterialInstanceDynamic::SetScalarParameterValue("DesaturationAmount", 1.0f)` 正确应用到单个 pawn 材质（逐 pawn 非全屏）
- [ ] 所有材质资产命名符合 art-bible §8.2（`M_`/`MI_`/`MI_*_P[n]`），Content Browser 无违规命名
- [ ] Substrate 已在 Project Settings 显式关闭（确认材质编辑器无 Substrate 节点提示）
- [ ] Lumen/Nanite/Megalights 关闭状态下材质渲染无异常（对照 architecture.md §Engine Integration 1.3）

---

## GDD Requirements Addressed

| GDD 文档 | 系统 | 需求 | 本 ADR 如何满足 |
|----------|------|------|-----------------|
| `design/art/art-bible.md` §3/4/5.1/6.4/8.5 | 材质视觉风格 | 风格化手绘贴图、Unlit/哑光、无 PBR、棋子描边 1.5–2px、禁 Nanite/Lumen | Legacy Unlit + Inverted Hull 描边，显式关闭 Nanite/Lumen/Substrate |
| `design/art/art-bible.md` §2（状态 F/G） | 胜利/破产视觉 | 胜利暖金 Post Process、破产去饱和 | `M_postprocess_victory_warm`（PP Volume）+ 逐 pawn `DesaturationAmount` MID 参数 |
| `design/gdd/vfx-feedback.md` CR-7 / OQ-VFX-6 | VFX #19 游戏反馈 | 胜利暖金 PP Material；OQ-VFX-6 登记「5.7 Substrate 是否改 PP Material 接口=LLM 盲区」| Legacy PP Material 接口 pre-5.3 稳定；OQ-VFX-6 盲区在本 ADR Engine Compatibility Verification Required 中显式登记 |
| `design/gdd/vfx-feedback.md` States G / art-bible §2 状态 G | VFX #19 破产去饱和 | 破产玩家去饱和为逐 pawn 材质（非全屏 PP） | 逐 pawn `UMaterialInstanceDynamic`，区分于全屏 PP（正确区分 CR-6/States G 语义） |
| `docs/architecture/architecture.md` §Engine Integration 1.3 | 渲染减法配置 | Lumen=OFF / Nanite=OFF / Megalights=OFF / 卡通材质→ADR-0009 | 本 ADR 裁定 Legacy Unlit，与减法配置完全对齐；Project Settings 配置指引在 Implementation Guidelines |

> **注（Foundational 补充）**：本 ADR 同时服务于 `architecture.md §Engine Integration 1.3` 中「卡通材质 legacy-vs-Substrate→ADR」的架构待裁项，属 Presentation 层美术资产基础设施决策，不归属任何单一 GDD 系统，而是横跨 HUD(16)/ VFX(19)/ 地产卡(17)/ 主菜单(20) 全部呈现层资产。

---

## Related

- `docs/architecture/ADR-0001-uobject-host-lifecycle.md`（宿主框架，已 Accepted）
- `docs/architecture/ADR-0007-bp-vs-cpp-boundary.md`（BP/C++ 边界，Proposed；Post Process C++ 控制路径须对齐）
- `docs/engine-reference/unreal/modules/rendering.md`（Substrate Slab 节点参考；PP Volume API）
- `docs/engine-reference/unreal/deprecated-apis.md`（Legacy material → Substrate 迁移建议）
- `design/art/art-bible.md`（视觉风格权威源，§3/4/5.1/6.4/8.5）
- `design/gdd/vfx-feedback.md`（CR-7 胜利 PP，OQ-VFX-6 Substrate 盲区登记）
- `docs/architecture/architecture.md` §Engine Integration 1.3（渲染减法配置裁定）
