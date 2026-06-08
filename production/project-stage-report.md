# Project Stage Analysis Report

**Generated**: 2026-06-09
**Stage**: **Production**（Core 层实现进行中）
**Stage Confidence**: PASS — 清晰检测（代码 11k 行 + 288 测试 + 15 ADR + 活跃 sprint）
**Analysis Scope**: Full project（无 role 过滤）

---

## Executive Summary

《骰子大亨》(Rento Fortune 复刻，UE5.7) 已走完 概念 → 引擎配置 → 系统拆解 → art-bible → **全 MVP GDD Approved** → **架构定稿(15 ADR)** → **Pre-Production 文档管线** → **Sprint0 工程脚手架**，目前处于 **Production 阶段**：逐 epic/story 实现 Core 玩法。

**Foundation 层 4 epic 全部完成**（framework/board-data/dice-rng/player-turn 共 **32/32 story**），**Core 层 economy bottleneck 推进中**（econ-001/002/003 done，3/10）。代码库 **11,149 行 C++**，**288 个自动化测试全绿**，CI/CD 已搭（GitHub Actions headless `-nullrhi`）。

**Current Focus**: Sprint 2 — Core economy 实现（mode-A workflow-assisted 串行）。刚完成 econ-003 发薪 F-1（PaySalary + clamp + 溢出 guard，2026-06-09）。
**Blocking Issues**: 无硬 blocker。
**Estimated Time to Next Stage**: 完成 Core 层 17 story（econ 剩 7 + movement 5 + property 7 + tile-events 7 = 26 story 待起，部分 Sprint 2）后接近功能完整，再 `/gate-check` Production → Polish。

> ⚠ **数据一致性已纠正**：`production/stage.txt` 本次由 `Pre-Production` 更新为 **`Production`**（之前的 `Concept`/`Systems Design`/`Pre-Production` override 滞后于真实进度）。

---

## Completeness Overview

### Design Documentation
- **Status**: 100% complete（MVP 范畴）
- **Files Found**: 19 GDD 文档于 `design/gdd/`（16 MVP 系统 + game-concept + systems-index + cross-review）
- **GDD 状态**: 全 16 MVP 系统 **Approved**（棋盘/玩家回合/骰子/移动/经济/地产/事件格/建房/破产/AI/HUD/地产卡UI/VFX/主菜单/存档/音频）
- **支撑产物**: `design/art/art-bible.md` ✓、`design/ux/`(3 spec + interaction-patterns + accessibility) ✓、`systems-index.md`(26 系统) ✓
- **Key Gaps**: 无 MVP 设计 gap。Alpha/Full 系统(拍卖/交易/House Rules 等)按计划留后。

### Source Code
- **Status**: ~55% complete（按 MVP epic 计：4/8 epic 完成 + 1 epic 部分）
- **Files Found**: 43 C++ 文件，**11,149 行**于 `Source/rento` + `Source/rentoTests`
- **Major Systems Implemented**:
  - ✅ **Foundation Framework** (7/7)：UMatchSubsystemBase 宿主、PIE 隔离、SaveGame 契约
  - ✅ **Board Data** (8/8)：UPrimaryDataAsset 容器、schema 校验、拓扑、UAssetManager 加载
  - ✅ **Dice RNG** (8/8)：FRandomStream 权威流、确定性、分布、序列化
  - ✅ **Player Turn** (9/9)：回合状态机、GameStateSnapshot、AI hook、强制清算、CI 权威纯净门
  - 🔄 **Economy Cash** (3/10)：Cash 受控写、事件契约、发薪 F-1（econ-001/002/003 done）
  - 📋 **Movement / Property Ownership / Tile Events**：未起（0/5, 0/7, 0/7）
- **Key Gaps**: Core 层剩余 26 story（econ 7 + move 5 + prop 7 + tile 7）

### Architecture Documentation
- **Status**: 100% complete（MVP 范畴）
- **ADRs Found**: **15 Accepted ADR**（ADR-0001~0015）+ master `architecture.md` + `tr-registry.yaml`(274 TR) + control-manifest + architecture-review
  - Foundation 5 (宿主/容器/事件总线/RNG/存档) + Core/Pres 7 (Snapshot/BP边界/HUD/材质/音频/输入/UI) + Gap ADR 3 (0013 地产所有权/0014 经济整数确定性/0015 掷骰上下文)
- **Coverage**: ✅ master 架构 ✅ 全技术决策 ✅ 需求追踪 ✅ 控制清单
- **Key Gaps**: 无。propagate 债登记在案（SalaryAmount board 校验等，随对应 epic 落地）

### Production Management
- **Status**: ~50% complete
- **Found**:
  - `sprint-status.yaml`：Sprint 2 活跃（3 done / 7 ready-for-dev / 7 backlog）
  - `production/epics/`：8 epic + index.md，**61 story 文件**
  - `production/qa/`：QA plan (sprint-2) + evidence
  - `production/gate-checks/`：pre-production gate 记录
  - `session-state/`：active.md 跨会话锚点 + LOCKED brief（mode-A 模板）
- **Key Gaps**: milestone/roadmap 文档未单列（进度跟踪在 sprint-status.yaml + active.md）

### Testing
- **Status**: ~60% coverage（Foundation + econ 已实现部分全覆盖）
- **Test Files**: 36 个 `*Test.cpp`，**288 个自动化测试**（类目 `Rento.[System].[Feature]`）
- **最新全量**: 287/287 Success, 0 Fail, 0 notRun, EXIT 0（econ-003 后基线，零回归）
- **机制**: headless `-nullrhi` UE Automation + 变异坐实非 vacuous + CI 权威纯净门（pt-009）
- **Key Gaps**: 未实现 epic 的测试随实现落地（TDD）

### Prototypes
- **Active Prototypes**: 0（复刻成熟玩法，概念原型有意跳过——已是知情决定）

---

## Stage Classification Rationale

**Why Production?**

项目满足 Production 全部入场条件并正在其中推进：
- ✅ 全 MVP GDD Approved（16 系统）
- ✅ 架构定稿（15 ADR + master 架构）
- ✅ 引擎配置 + Sprint0 脚手架编译通过
- ✅ CI/CD 搭建（GitHub Actions headless）
- ✅ Foundation 层 100% 实现（32 story）
- ✅ `src/` 远超 10 文件门槛（43 文件 / 11k 行）
- 🔄 Core 层活跃实现中（economy 3/10，活跃 Sprint 2）

**进入 Polish 的门槛**（未来）:
- [ ] Core 层 4 epic 全部实现（economy/movement/property/tile-events）
- [ ] MVP 功能完整（含 AI/HUD/VFX/存档实现）
- [ ] vertical-slice 验证（真 UE + playtest，留到功能近完整）
- [ ] `/gate-check` Production → Polish 门 PASS

---

## Gaps Identified (with Clarifying Questions)

### Important Gaps (affect velocity)

1. **Core 层依赖序列**
   - **Impact**: econ bottleneck 先行（其余系统依赖经济 API），movement 可并行
   - **Question**: 继续串行跑完 econ 还是并行起 movement（economy-independent）？
   - **Suggested Action**: 按 sprint-2-workflow-handoff 执行序，econ 优先

2. **跨 story 接缝验证**
   - **Impact**: 已 done 的 player-turn 用 economy mock 接缝，economy 实现后需核对真实接线
   - **Question**: econ epic 完成后是否跑 integration 测试核对 pt-007 ↔ economy 真实接线？
   - **Suggested Action**: econ 全完后 `/smoke-check sprint` + integration

### Nice-to-Have Gaps

3. **propagate 债集中核销**
   - **Impact**: SalaryAmount board 校验、CollectSalary 路径测试等登记在各 story Completion Notes
   - **Suggested Action**: 各 epic 实现时逐条 discharge（已有 economy 兜底防线）

---

## Recommended Next Steps

### Immediate Priority (Do First)
1. **继续 Sprint 2 economy** → econ-004 地产租 F-2 piecewise（依赖 001/002 ✓）
   - mode-A workflow-assisted，复用 econ-003 LOCKED brief 模板
   - Estimated effort: M（piecewise + mock 接缝建房8/所有权6）

### Short-Term (This Sprint)
2. **econ-005..010** → 车站公用租 / 抵押赎回 / 缴税买地 / NLV / 凑钱破产 / Cash 存档
3. **(可并行) movement-001..005** → economy-independent，可穿插

### Medium-Term (Next Milestones)
4. **property-ownership + tile-events epic**（econ 完成后解锁）
5. **AI/HUD/VFX/存档 实现 epic**（Presentation/AI 层）
6. **功能近完整后** → `/vertical-slice`（真 UE + playtest）→ `/gate-check` Production → Polish

---

## Follow-Up Skills to Run

- `/dev-story production/epics/economy-cash/story-004-property-rent-f2.md` — 下一条 econ
- `/sprint-status` — Sprint 2 进度快照
- `/smoke-check sprint` — econ epic 完成后的关键路径门
- `/story-readiness` — 起新 story 前校验
- `/gate-check` — 功能完整后的 Production → Polish 门

---

## Appendix: File Counts by Directory

```
design/
  gdd/           19 files（16 MVP 系统全 Approved + concept + index + cross-review）
  art/           art-bible.md
  ux/            3 spec + interaction-patterns + accessibility-requirements

Source/
  rento/         主模块（11,149 行含测试）
  rentoTests/    36 个 *Test.cpp（288 automation tests）

docs/
  architecture/  15 ADR (Accepted) + architecture.md + tr-registry.yaml(274 TR)
                 + control-manifest + architecture-review

production/
  epics/         8 epic + index.md（61 story）
  qa/            QA plan (sprint-2) + evidence
  gate-checks/   pre-production gate 记录
  sprint-status.yaml  Sprint 2（3 done / 7 ready / 7 backlog）
  stage.txt      = Production（本次更新）
  review-mode.txt = lean

tests/           逻辑路径映射（物理测试在 Source/rentoTests）
prototypes/      0（复刻类，有意跳过概念原型）
```

### 实现进度矩阵

| Epic | Layer | Stories | Done | Status |
|------|-------|---------|------|--------|
| foundation-framework | Foundation | 7 | 7 | ✅ 100% |
| board-data | Foundation | 8 | 8 | ✅ 100% |
| dice-rng | Foundation | 8 | 8 | ✅ 100% |
| player-turn | Foundation | 9 | 9 | ✅ 100% |
| economy-cash | Core | 10 | 3 | 🔄 30% |
| movement | Core | 5 | 0 | 📋 0% |
| property-ownership | Core | 7 | 0 | 📋 0% |
| tile-events | Core | 7 | 0 | 📋 0% |
| **Total** | | **61** | **35** | **57%** |

---

**End of Report**

*Generated by `/project-stage-detect` skill — 2026-06-09*
