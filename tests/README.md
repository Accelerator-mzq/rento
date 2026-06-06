# Test Infrastructure

**Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）
**Test Framework**: UE Automation System（Functional / Spec tests），headless `-nullrhi`
**CI**: `.github/workflows/tests.yml`
**Setup date**: 2026-06-06

## Directory Layout

```
tests/
  unit/           # 隔离单元测试清单/文档（逻辑、公式、状态机）——UE 测试代码落 Source/Tests/
  integration/    # 跨系统 + 存读档 round-trip 测试清单/文档
  smoke/          # 关键路径冒烟清单（/smoke-check 门读取）
  evidence/       # 截图日志 + 手动测试 sign-off 记录（Visual/Feel/UI 类）
Source/Tests/     # UE Automation 测试 C++ 代码（FAutomationTestBase / Spec）
```

> **UE 特例**：UE Automation 测试代码是 C++，须随 `.uproject` 的测试模块编译，故落 `Source/Tests/`；
> 本 `tests/` 目录持**跨引擎结构**（冒烟清单、证据、按系统的测试规划文档）。逻辑单测的**断言**在 Source/Tests/，
> 其**规划/清单**（哪些 AC 要测、test-spec）在 `tests/unit/[system]/`。

## Running Tests

```bash
# headless 跑全部 DiceTycoon 自动化测试（CI 用）
UnrealEditor DiceTycoon.uproject -nullrhi -nosound \
  -ExecCmds="Automation RunTests DiceTycoon.; Quit" -log -unattended

# 编辑器内：Session Frontend → Automation → 勾选 "DiceTycoon." 测试
```

> 注：`.uproject` 与测试模块尚未创建（实现阶段建立）。CI 占位项目名 = `DiceTycoon`。
> 首个实现 sprint 建模块时，激活 `Source/Tests/` 下示例测试模板。

## Test Naming（coding-standards 对齐）

- **C++ 测试类**：`F[System][Feature]Test` 或 Spec `[System][Feature]Spec`
- **Automation 类目**：`DiceTycoon.[System].[Feature]`（如 `DiceTycoon.Economy.Rent`）
- **文件**：`[system]_[feature]_test` 命名规范；函数 `test_[scenario]_[expected]`
- **示例**：`Source/Tests/Private/EconomyRentTest.cpp` → `DiceTycoon.Economy.Rent.MonopolyDoubling`

## 确定性纪律（ADR-0004 / coding-standards）

- 测试**确定性**：固定 seed、无时间依赖断言、无 `Math.random`/全局 RNG（走 ADR-0004 注入的 `FRandomStream`）。
- **隔离**：每个测试自建/拆 state，不依赖执行顺序。
- **无 I/O**：单元测试不碰外部 API/DB/文件——用依赖注入（ADR-0008 `IGameClock`、ADR-0001 Subsystem mock）。
- **headless 可测**：逻辑 AC 标 `[Logic]` 须在 `-nullrhi` 可跑（见各 GDD AC 分类；渲染时刻标 `[Advisory]`）。

## Story Type → Test Evidence（coding-standards）

| Story Type | Required Evidence | Location | Gate |
|---|---|---|---|
| **Logic**（公式/AI/状态机） | 自动化单测 — 须通过 | `Source/Tests/` + 规划 `tests/unit/[system]/` | BLOCKING |
| **Integration**（多系统） | 集成测试 OR 记录的 playtest | `Source/Tests/` + `tests/integration/[system]/` | BLOCKING |
| **Visual/Feel**（动画/VFX/手感） | 截图 + lead sign-off | `tests/evidence/` | ADVISORY |
| **UI**（菜单/HUD/屏） | 手动 walkthrough OR 交互测试 | `tests/evidence/` | ADVISORY |
| **Config/Data**（平衡调参） | 冒烟通过 | `production/qa/smoke-*.md` | ADVISORY |

## CI

每次 push 到 `main` + 每个 PR 自动跑测试（`.github/workflows/tests.yml`）。测试失败阻塞合并。

> ⚠ **UE CI 须 self-hosted runner**（装了 Unreal Editor）+ 设 `UE_EDITOR_PATH` 环境变量。
> 详见 `.github/workflows/tests.yml` 注释。云端 hosted runner 不含 UE，无法跑 UE Automation。
