# rentoTests — UE Automation 测试模块

UE Automation Testing Framework（UE5.7）。测试代码 C++，随 `rento.uproject` 的 **rentoTests** 模块编译
（Editor target 专用，不进 Shipping）。模块已建立（2026-06-06），脚手架冒烟测试已激活。

## 运行

```bash
# 编辑器内：Session Frontend → Automation → 勾选 "Rento." 测试
# headless（CI）：
UnrealEditor rento.uproject -nullrhi -nosound \
  -ExecCmds="Automation RunTests Rento.; Quit" -log -unattended
```

## 命名

- 测试类：`F[System][Feature]Test`（简单）或 `[System][Feature]Spec`（BDD 风格，推荐复杂场景）
- Automation 类目：`Rento.[System].[Feature]`（如 `Rento.Economy.Rent`、`Rento.Dice.Determinism`）
- 文件：`[System][Feature]Test.cpp` / `[System][Feature]Spec.cpp`，放 `Source/rentoTests/Private/`

## 模块结构（已建立）

- `rentoTests.Build.cs` — 依赖 `rento`（被测主模块）+ `AutomationController`/`FunctionalTesting`
- `Private/rentoTests.cpp` — `IMPLEMENT_MODULE`
- `Private/SampleFormulaSpec.cpp` — 脚手架冒烟（`Rento.Sample.*`），story dev 时替换为真实公式断言
- `rentoEditor.Target.cs` 已注册 rentoTests；`rento.uproject` Modules 列 rentoTests（Type=Editor）

## story dev 时新建测试（对齐 32 story 的 test path）

各 story `## Test Evidence` 指定 path（如 `tests/unit/dice/determinism_fixture_test.cpp`）——
实现 sprint 时在 `Source/rentoTests/Private/` 下建对应 `[System][Feature]Spec.cpp`，
include 真实模块头，按 story `## QA Test Cases` 的 Given/When/Then 写确定性断言。

> 注：story 里 `tests/unit/...` 是逻辑分类路径；实际 UE Automation .cpp 落
> `Source/rentoTests/Private/`，用 Automation 类目（`Rento.Dice.*`）区分系统，非物理子目录。

## 与 GDD / ADR 的对齐

- 确定性 RNG：注入 ADR-0004 `FRandomStream`（固定 seed），禁全局 RNG
- 游戏时钟：注入 ADR-0008 `IGameClock`（`FMockGameClock` 手动推帧），HUD 计时 AC headless 可测
- Subsystem 宿主：ADR-0001 `UWorldSubsystem`，测试用 mock world / 直接实例化逻辑 UObject
- 渲染时刻 AC 标 `[Advisory]`（headless `-nullrhi` 无渲染探针），逻辑状态机标 `[Logic]`
