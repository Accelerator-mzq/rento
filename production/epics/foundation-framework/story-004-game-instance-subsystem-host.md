---
Epic: foundation-framework
Status: Complete
Layer: Foundation
Type: Integration
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-07
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
- **Path**: `Source/rentoTests/Private/GameInstanceSubsystemHostTest.cpp`（逻辑分类 `tests/integration/foundation/game_instance_subsystem_host_test.cpp`）
- **Status**: 已创建（headless 结构反射路径：代码 TC-1 继承链 / TC-2 入口存在性〔Save/Setup UFUNCTION + Audio 成员指针〕/ TC-3 反射无 per-match 字段 + Edge×2，5 测试全 Success）。**story-004 QA TC-1/TC-2/TC-3/TC-5（PIE 跨局存活/三局单例/DA_Board 跨局缓存）= DEFER ff-007 + bd-007**（headless `-nullrhi` 不可驱动 GameInstance PIE 生命周期 + 需真 DA_Board 资产）。
- **headless 验证边界（code-review 2026-06-07）**：AC-1/3/4 由结构反射覆盖；AC-2/5/6 运行时行为 defer ff-007/bd-007，登记 tech-debt-register（无 false-green 假断言）。

## Dependencies

- **Depends on**: story-001（与 per-match World Subsystem 宿主分类对照，OQ-6 资产缓存 vs World 实例拆分）。
- **Unlocks**: Save epic（ADR-0005 SaveGame 宿主分量）、Audio epic（ADR-0010 混音宿主）、Setup epic（`StartNewGame` 入口）；pt-001（PlayerState，回合 epic——填充 FGameSetupConfig + StartNewGame body）。

## Completion Notes
**Completed**: 2026-06-07
**Criteria**: 6/6 处理（AC-1/AC-3/AC-4 COVERED 结构反射；AC-2 结构 COVERED + 运行时 DEFER ff-007；AC-5 DEFER ff-007+bd-007；AC-6 DEFER ff-007）。独立重跑全量 Rento. 153/153 Success, 0 failed, EXIT 0（Saved/TestReport_ff004_final/index.json）。
**Deviations**: 无 BLOCKING。Advisory：① headless 验证边界——GameInstance::Init 在 -nullrhi 不可靠，AC-1/3 走结构反射退化路径（IsChildOf+FindFunctionByName+成员函数指针，非实例化），AC-2/5/6 运行时行为 DEFER ff-007/bd-007（无 false-green 假断言）；② StartNewGame 最小 seam（FGameSetupConfig 空 USTRUCT 占位→pt-001 填充，禁重声明）。
**Test Evidence**: Integration — `Source/rentoTests/Private/GameInstanceSubsystemHostTest.cpp`（Rento.Foundation.GameInstanceSubsystemHost，5 测试 TC1-3+Edge×2，全 Success）+ `Source/rento/GameSetupConfig.h`/`PersistentServicesSubsystem.h`+`.cpp`。
**Code Review**: Complete（本会话 /code-review = APPROVED；MF-1 Audio 挂载点覆盖 + R-1 ExcludeSuper 归属本类 + R-2 doc-sync + R-3 TC 编号消歧闭合；deflate qa BLOCKING-2 EFieldIterationFlags〔引擎源码 UnrealType.h:7041 坐实 None=ExcludeSuper〕+ unreal W-1 Super 顺序〔前提错，与 sibling 一致〕）。
**Tech debt logged**: 2 项入 register（AC-2/5/6 运行时验证 defer ff-007+bd-007 / StartNewGame+FGameSetupConfig 最小 seam pt-001 填充）。
