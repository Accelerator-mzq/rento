# Gate Check: Technical Setup → Pre-Production

**Date**: 2026-06-06
**Checked by**: /gate-check pre-production（lean review mode，四 director 全跑）
**Verdict**: **CONCERNS**（可进 Pre-Production；CONCERNS 均已知、有据、归属 Pre-Production/Sprint0，无硬 FAIL）

---

## Required Artifacts: 13/13 present

- [x] 引擎 UE5.7（CLAUDE.md / technical-preferences）
- [x] technical-preferences 配置（命名/性能预算/Engine Specialists/ADR Log）
- [x] art-bible.md v0.1（§1–9）
- [x] ≥3 Foundation ADR — **5 个**（ADR-0001~0005 全 Accepted）
- [x] 引擎参考库（docs/engine-reference/unreal/）
- [x] tests/unit + tests/integration
- [x] CI workflow（.github/workflows/tests.yml，UE Automation -nullrhi）
- [x] ≥1 示例测试（Source/Tests/Private/SampleFormulaSpec.cpp 模板）
- [x] master 架构（docs/architecture/architecture.md v1.0 TD APPROVED）
- [x] 追溯索引（docs/architecture/requirements-traceability.md，274 TR）
- [x] /architecture-review 已跑（architecture-review-2026-06-06.md = CONCERNS）
- [x] design/accessibility-requirements.md（WCAG-AA）
- [x] design/ux/interaction-patterns.md（17 模式）

## Hard Gate Checks: 2/2 PASS

- [x] **Foundation 层零缺口** — PASS（board/turn/dice/save 矩阵 Gap=0；23 真 Gap 无一落 Foundation，最近 G-04/05 属 economy Core 层）
- [x] **ADR 无环** — PASS（12 ADR Depends-On 拓扑序 0001→...→0012 自洽，无反向边；0006→0007 方向经 CONFLICT-2 裁定）
- [x] 12 ADR 全 Accepted（grep Status 逐档确认）
- [x] 所有 ADR 含 Engine Compatibility + GDD Requirements Addressed
- [x] 所有 ADR 同引擎版本（UE5.7），无 stale

## Director Panel（lean）

| Director | Verdict | 要点 |
|---|---|---|
| **creative-director** | CONCERNS | 四支柱无漂移；**RETREAT 不伤核心体验**（砍 sophistication 上限非 competence 下限，F-4 贪心+F-5b 保留）；watch：支柱②社交"坑"半（交易/拍卖）在 Alpha、vs-AI MVP 只兑现"赢" |
| **technical-director** | READY | 11 类核心决策全有 owner ADR；Foundation coding 前置解除；engine Verification Required 正确划归 Sprint0 不阻进入 |
| **producer** | CONCERNS | 硬前置全 ✓；concerns=producer 债清单（4 GDD 债/25 Gap TR/8 specialist findings/Sprint0 engine 验证）须追踪 |
| **art-director** | CONCERNS | art-bible v0.1 足够开工；2 propagate 债（§7.2 Tabular Figures 等宽字体具体资产名、Substrate Unlit go/no-go Alpha） |

> 无 NOT READY → 不 FAIL；3 CONCERNS → 最低 CONCERNS。

## CONCERNS（均 Pre-Production / Sprint0 收口，不阻进入）

1. **engine Verification Required（Sprint0）**：ADR-0012 CommonUI Experimental 状态（漏配 Shipping cook 全 UI 失效，**Sprint0 blocking check**）；ADR-0008 NativeTick headless / ADR-0010 音频 Submix / ADR-0011 IMC 截获 / ADR-0004 RandRange / ADR-0002 async load。
2. **ADR-0009 Legacy material = deprecated API**（有意技术债，documented，Substrate 留 Alpha 评估）。
3. **per-screen UX spec 缺**（仅 interaction-patterns；hud/main-menu UX → Pre-Production /ux-design）。
4. **producer 债**：4 GDD 债（OQ-Audio-2/VFX-7/VFX-13/RepairFee owner）+ 25 Gap TR（5 需新 ADR 如 OQ-Prop-1，其余 propagate/UX/perf/asset）+ 8 specialist findings（→ /create-control-manifest）。
5. **art-bible propagate 债**：等宽字体资产名、Substrate Unlit go/no-go。
6. **支柱②社交 watch**：MVP vs-AI 只兑现"赢"半，"坑"（交易/拍卖/策略卡）全 Alpha（concept Scope Tiers 已诚实标注，非漂移）。

## Chain-of-Verification

5 问校验（≥2 TOOL ACTION）：① [TOOL] Foundation 零缺口经 traceability matrix 逐系统核实 Gap=0；② [TOOL] ADR 无环经 12 档 Depends-On grep 拓扑确认；③ 所有 CONCERN 是否可在 Pre-Production 收口=是（vertical-slice/per-screen UX/control-manifest/Sprint0 验证均 Pre-Production 内）；④ 是否软化了 FAIL=否（两硬门真 PASS，无 director NOT READY）；⑤ CONCERN 叠加是否成 blocker=否（彼此正交，均有归属与处方）。**Chain-of-Verification: 5 问校验 — verdict 不变（CONCERNS）。**

## Verdict: CONCERNS — 可进 Pre-Production

两硬门 PASS、无 FAIL 条件、无 director NOT READY。CONCERNS 全部已知/有据/归属 Pre-Production 或 Sprint0。gate 为 advisory，由用户裁定是否推进。

## Pre-Production 推荐顺序（technical-setup PASS 标准序）

1. `/create-control-manifest`（从 12 Accepted ADR 提层规则 + 收 8 specialist findings；epics 前置）
2. `/vertical-slice`（**先验核心循环好玩**，再写 epics——含 Sprint0 引擎验证 ADR-0012/0008）
3. playtest → `/playtest-report`（≥1 session 过 Pre-Production→Production 门）
4. `/ux-design [hud|main-menu|...]`（per-screen UX spec）
5. `/create-epics layer:foundation` → `layer:core` → `/create-stories [epic]`（引 tr-registry 274 TR）
6. `/sprint-plan new`
