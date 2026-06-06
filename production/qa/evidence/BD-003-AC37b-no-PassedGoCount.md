# BD-003 AC-37b 静态守门证据

**Story**: BD-003 — UBoardMathLibrary：F-1/F-2/F-3 环路拓扑 + AdvanceIndex 原子返回
**AC**: AC-37b（[Advisory] code-review + 编译期守门）
**Date**: 2026-06-06
**Verifier**: unreal-specialist

## AC 原文

> **AC-37b（无独立 PassedGoCount 公开接口）** [Advisory] code-review + 编译期守门：
> 代码库不存在独立公开 `PassedGoCount(from, steps)` 可被分离调用。

## 验证方法

运行时断言无法证明函数不存在（无法断言「某函数不可调用」），
因此 AC-37b 采用 **code-review + 静态grep** 守门。

### Grep 证据

搜索整个 Source/ 目录，确认不存在独立公开的 `PassedGoCount` 符号：

```
grep -r "PassedGoCount" Source/
```

**结果：零命中（无任何匹配）**

搜索 UBoardMathLibrary 的全部公开 UFUNCTION：

```
grep -A2 "UFUNCTION" Source/rento/BoardMathLibrary.h
```

**结果：仅两个公开函数：**
- `AdvanceIndex(int32 From, int32 Steps, int32 N)` → 返回 `FBoardAdvanceResult`
- `StepsBetween(int32 From, int32 Target, int32 N)` → 返回 `int32`

无 `PassedGoCount` 或任何独立 F-2 接口。

## 原子性机制说明

`FBoardAdvanceResult` 是 USTRUCT(BlueprintType)，包含 `NewIndex` 和 `PassedGo` 两个字段。
`AdvanceIndex` 返回完整 struct，两值由同一次函数调用填充并原子返回（AC-37a）。

Blueprint 侧使用 Break Struct 节点拆分两值，两值必然来自同一个 struct 实例，
不存在「单独调用 PassedGoCount 取 PassedGo」的路径——这是类型层面的强制，非纪律约束。

## 约束文件

| 文件 | 内容 |
|------|------|
| `Source/rento/BoardMathLibrary.h` | 仅定义 `AdvanceIndex` 和 `StepsBetween` 两个公开 UFUNCTION |
| `Source/rento/BoardMathLibrary.cpp` | F-2 PassedGo 作为 AdvanceIndex 内部计算分量，无独立导出 |
| `ADR-0002 §Key Interfaces` | 注释明确「无独立公开 PassedGoCount 接口（AC-37b）」 |
| `control-manifest §数据容器` | Forbidden：不暴露独立公开 PassedGoCount(from, steps) 接口 |

## 结论

AC-37b **PASS**（Advisory，code-review 守门）：
- 代码库不存在 `PassedGoCount` 符号
- UBoardMathLibrary 公开接口仅两个（AdvanceIndex + StepsBetween）
- F-2 PassedGo 仅为 AdvanceIndex 返回 struct 的分量，无法分离调用
