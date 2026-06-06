---
Epic: foundation-framework
Status: Ready
Layer: Foundation
Type: Integration
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 004 — 跨局 UGameInstanceSubsystem 宿主框架（Save/Audio/Setup 入口）

## Context

- **GDD**: 无（TD-owned 引擎集成模块，源 `docs/architecture/architecture.md` §2.1 模块①——跨关卡持久宿主层）
- **Requirement TR-ID**: 框架面支撑下游 Save 框架（21）/ Audio（22）/ Setup·主菜单（20）的跨局宿主面（功能 owns 归各自 epic：Save→ADR-0005、Audio→ADR-0010）。本 epic 无独立 TR owns，仅提供跨局宿主分类框架。
- **ADR Governing**:
  - **ADR-0001（primary）— UObject 宿主与生命周期边界**：跨局持久服务（Save/Load 框架21、Audio22 资产/混音、Setup/主菜单20 入口）挂 `UGameInstanceSubsystem`；存档槽访问、音频总线、`StartNewGame(const FGameSetupConfig&)` 入口须跨局存活；`SaveGameToSlot` 调用方挂此处。`UAssetManager` 缓存 `DA_Board` 底层资产跨局不释放（OQ-6 reconcile）。
- **Engine**: Unreal Engine 5.7（C++ 框架）— **Risk: LOW**（`UGameInstanceSubsystem` UE4.24+ 稳定，无 post-cutoff API）
- **Engine Notes（ADR-0001 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None**。`UGameInstanceSubsystem`/`Initialize`/`Deinitialize` 自 UE4.24 稳定。
  - Verification: `StartNewGame` 入口跨 PIE 局存活（ADR-0001 Migration Plan #3 验证）。
- **Control Manifest Rules**:
  - **Required（Foundation/ADR-0001）**: 跨局持久服务挂 `UGameInstanceSubsystem`（Save/Load 框架21、Audio22 资产/混音、Setup/主菜单20 入口）。
  - **Forbidden（ADR-0001）**: Never 把 per-match 对局态全挂 `UGameInstanceSubsystem`（PIE 隔离破裂）——跨局宿主只承 Save/Audio/Setup 入口与跨局资产缓存，**不承对局态**。
  - **Required（Full Vision 预留）**: MVP 状态以普通 `UPROPERTY` 承载，字段命名结构对齐 `PlayerState`/`GameState`（source: ADR-0001）。
  - **Guardrail**: Subsystem 查找 O(1) 哈希，非热路径；跨局资产 AssetManager 缓存一份（不随局释放）；CPU 帧预算 16.6 ms（source: ADR-0001）。
  - **Global**: UObject 引用成员用 `TObjectPtr<T>` + `UPROPERTY()` 防 GC（source: current-best-practices）。

## Acceptance Criteria

- [ ] AC-1: 存在跨局 `UGameInstanceSubsystem` 宿主框架，承载 Save/Load 框架（21）、Audio（22）、Setup/主菜单（20）三类跨局服务的挂载点。
- [ ] AC-2: `StartNewGame(const FGameSetupConfig&)` 入口挂跨局宿主（GameSetup 分量）；该入口在 PIE 多局间存活（不随单局 World 销毁）。
- [ ] AC-3: `SaveGameToSlot` / `LoadGameFromSlot` 调用方（存档槽访问）挂跨局宿主，跨局可达（序列化契约/原子写盘细节归 ADR-0005，不在本 story）。
- [ ] AC-4: 跨局宿主**不承载任何 per-match 对局态字段**（无 `Cash`/`house_count`/owner map / 回合状态机字段）。
- [ ] AC-5: `UAssetManager` 缓存 `DA_Board` 底层资产跨局不释放（满足 OQ-6"避免反复磁盘 IO"）；当前局正在用哪块棋盘的引用 + tile 访问服务仍挂 per-match World Subsystem（story-001/003，不在本 story）。
- [ ] AC-6: 在 PIE Start→Stop→Start 三局中，跨局宿主实例恒为同一进程单例（不随局重建）。

## Implementation Notes

> 逐字保真自 ADR-0001 Decision 表 + OQ-6 reconcile + Migration Plan #3，不改写语义。

1. **跨局持久服务挂 `UGameInstanceSubsystem`**（Save/Load 框架21、Audio22 资产/混音、Setup/主菜单20 入口）；存档槽访问、音频总线、`StartNewGame(const FGameSetupConfig&)` 入口须跨局存活；`SaveGameToSlot` 调用方挂此处。
2. **OQ-6 张力 reconcile**：`DA_Board`（不可变静态资产）由 `UAssetManager`/`FStreamableManager` 缓存，跨局复用、不随局释放底层资产；但"当前局正在用哪块棋盘"的引用 + 解析后的 tile 访问服务挂 per-match World Subsystem。即：**资产缓存在 GameInstance 级 AssetManager，棋盘服务实例在 World 级**。二者不矛盾。
3. **接口名冻结**：`StartNewGame(const FGameSetupConfig&)` 入口名冻结 + `FGameSetupConfig` USTRUCT 归属随回合2，本 ADR/story 只定宿主（不定签名内部、不定字段语义）。
4. **Full Vision 迁移预留**：MVP 状态以普通 `UPROPERTY` 承载、字段对齐 GameState/PlayerState。

## Out of Scope

- Save 序列化契约、四重完整性门、原子写盘 → Save epic（ADR-0005）。
- Audio 混音总线 / MetaSound 资产 / `BindAll`/`UnbindAll` → Audio epic（ADR-0010）。
- `FGameSetupConfig` 字段定义、主菜单流程 → Setup/UI epic。
- per-match World Subsystem 宿主（回合/经济/棋盘服务实例）→ **story-001**。

## QA Test Cases

> 📋 已同步 QA Plan：`production/qa/qa-plan-sprint-0-2026-06-06.md`（2026-06-06）——测试规格以本节为权威，plan 为汇总索引。

> Integration（跨局宿主生命周期）；headless `-nullrhi`。

- **TC-1 (AC-1/AC-6)**: GIVEN `-nullrhi` GameInstance，WHEN PIE Start→Stop→Start 三局，THEN 跨局 `UGameInstanceSubsystem` 实例指针三局恒同一（单例，不重建）。
- **TC-2 (AC-2)**: GIVEN 跨局宿主，WHEN 第一局调 `StartNewGame(config)` 后 Stop 再 Start，THEN 入口仍可达（跨局存活）。
- **TC-3 (AC-3)**: GIVEN 跨局宿主，WHEN 查询 `SaveGameToSlot`/`LoadGameFromSlot` 调用方归属，THEN 挂跨局宿主、跨局可达。
- **TC-4 (AC-4)**: GIVEN 跨局宿主 reflection 字段表，WHEN 枚举，THEN 无 per-match 对局态字段（`Cash`/`house_count`/owner map / 回合 phase）。
- **TC-5 (AC-5)**: GIVEN 连续两局加载同一 `DA_Board`，WHEN 检查 AssetManager 缓存，THEN 底层资产仅加载一次（跨局复用，无第二次磁盘 IO）；当前局棋盘引用仍在 World Subsystem。
- **Edge cases**: 进程内首次无 GameInstance 时入口安全兜底；Audio/Save/Setup 任一未实现时宿主框架仍可独立实例化（分量解耦）。

## Test Evidence

- **Type**: Integration
- **Path**: `tests/integration/foundation/game_instance_subsystem_host_test.cpp`
- **Status**: 未创建（待 dev-story 实现）

## Dependencies

- **Depends on**: story-001（与 per-match World Subsystem 宿主分类对照，OQ-6 资产缓存 vs World 实例拆分）。
- **Unlocks**: Save epic（ADR-0005 SaveGame 宿主分量）、Audio epic（ADR-0010 混音宿主）、Setup epic（`StartNewGame` 入口）。
