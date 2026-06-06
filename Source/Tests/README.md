# Unreal Automation Tests

UE Automation Testing Framework（UE5.7）。测试代码是 C++，随 `.uproject` 的测试模块编译。

## 运行

```bash
# 编辑器内：Session Frontend → Automation → 勾选 "DiceTycoon." 测试
# headless（CI）：
UnrealEditor DiceTycoon.uproject -nullrhi -nosound \
  -ExecCmds="Automation RunTests DiceTycoon.; Quit" -log -unattended
```

## 命名

- 测试类：`F[System][Feature]Test`（简单）或 `[System][Feature]Spec`（BDD 风格，推荐复杂场景）
- Automation 类目：`DiceTycoon.[System].[Feature]`（如 `DiceTycoon.Economy.Rent`）
- 文件：`[System][Feature]Test.cpp` / `[System][Feature]Spec.cpp`

## 测试模块（实现阶段建立）

`.uproject` 尚未创建。首个实现 sprint 须：
1. 建 `DiceTycoon` game module + `DiceTycoonTests` 测试模块（`DiceTycoonTests.Build.cs`，依赖 `AutomationController`/游戏模块）
2. 把 `Private/SampleFormulaSpec.cpp`（本目录示例模板）接入测试模块、激活
3. `.uproject` 的 `AdditionalDependencies` / `.Target.cs` 注册测试模块（Editor + 可选 Functional target）

> 示例 `Private/SampleFormulaSpec.cpp` = UE Automation Spec **模板**，演示确定性公式测试模式
> （固定输入→断言输出，无 RNG/时间依赖，headless 可跑），待游戏模块建立后激活并替换为真实公式断言。

## 与 GDD / ADR 的对齐

- 确定性 RNG：注入 ADR-0004 `FRandomStream`（固定 seed），禁全局 RNG
- 游戏时钟：注入 ADR-0008 `IGameClock`（`FMockGameClock` 手动推帧），使 HUD 计时器 AC headless 可测
- Subsystem 宿主：ADR-0001 `UWorldSubsystem`，测试用 mock world / 直接实例化逻辑 UObject
- 渲染时刻 AC 标 `[Advisory]`（headless `-nullrhi` 无渲染探针），逻辑状态机标 `[Logic]`
