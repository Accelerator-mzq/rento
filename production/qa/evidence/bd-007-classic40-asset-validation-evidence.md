# BD-007 证据 — DA_Board_Classic40 资产生成 + [Config] 校验 harness + cook 门控

> Story: `production/epics/board-data/story-007-classic40-asset-config-harness.md`
> 日期: 2026-06-07 ｜ 模式: Automated（编辑器/commandlet 会话，全自动 CLI 驱动）
> 验证者: 主会话独立核实（编译 + 跑 commandlet + 跑 test + grep rento.log，非信 agent 自报）

## 交付物

| 文件 | 作用 |
|---|---|
| `Source/rento/Commandlets/GenerateClassic40Commandlet.h/.cpp` | 程序化生成 `DA_Board_Classic40`（CR-6 布局 + 公开参考值 temp-fill），`#if WITH_EDITOR` 包裹 |
| `Source/rento/Commandlets/BoardCookGateCommandlet.h/.cpp` | cook 前扫描 `bIsPlaceholderData`，true 则报错 + 退出码 1（AC-4c Alpha gate） |
| `Source/rentoTests/Private/Classic40AssetValidationTest.cpp` | [Config] 校验 harness，加载真磁盘资产验 AC-3~7b，oracle 独立取自 CR-6 GDD |
| `Source/rento/rento.Build.cs` | 改：`if (Target.bBuildEditor)` 条件加 `AssetRegistry` 依赖 |
| `Content/Boards/DA_Board_Classic40.uasset` | 生成的 temp-fill 资产（40 格，33448 字节，bIsPlaceholderData=true） |

## 验证链（命令 + 结果）

### 1. 编译（UBT）
```
Build.bat rentoEditor Win64 Development -Project=...rento.uproject -WaitMutex
→ Result: Succeeded（UHT processed 1 file）
```
修 1 真编译错误：`GenerateClassic40Commandlet.h` 方法签名用 `FBoardTileData` 等类型但头未 include `BoardDataAsset.h`（已补）。

### 2. 资产生成 commandlet
```
UnrealEditor-Cmd rento.uproject -run=GenerateClassic40 -unattended -nopause -nosplash -stdout
→ LogTemp: GenerateClassic40: 已生成 40 格，保存到 .../Content/Boards/DA_Board_Classic40.uasset，bIsPlaceholderData=true
→ 进程 EXIT: 0
→ 资产落盘确认：Content/Boards/DA_Board_Classic40.uasset (33448 bytes)
```

### 3. [Config] 校验 harness（AC-3~7b）
```
UnrealEditor-Cmd rento.uproject -nullrhi -nosound -unattended -ExecCmds="Automation RunTests Rento.Board.Classic40AssetValidation; Quit"
→ 8/8 Result={Success}, 0 Fail：
   TC_AC3_TileTypeCounts            (AC-3  类型精确计数 Go=1..Property=22)
   TC_AC4asset_PropertyMonetaryNonZero (AC-4-asset  22 Property 格 Price>0 && BuildCost>0)
   TC_AC4b_UtilitySchema            (AC-4b  Utility Price>0 && DiceMult==2 && Rent==0)
   TC_AC4c_PlaceholderFlagReadable  (AC-4c  bIsPlaceholderData 可读, MVP==true)
   TC_AC5_ArrayLengthsByType        (AC-5  Property Rent==6/Railroad==4/Utility DiceMult==2&Rent==0/非购买==0)
   TC_AC6_ColorGroupCounts          (AC-6  棕2/浅蓝3/粉3/橙3/红3/黄3/绿3/深蓝2 合计22)
   TC_AC7_LayoutMatchesOracle       (AC-7  逐格 (TileType,ColorGroup,TileIndex) 匹配 CR-6 oracle)
   TC_AC7b_GoTileSchema             (AC-7b  Go 格 Salary>0 && SpecialAction==CollectSalary)
```
非 vacuous 保证：oracle 独立硬编码自 CR-6 GDD（非 import generator 表）；资产缺失走 AddError→Fail（非静默 pass）；全 TestEqual 精确计数。

### 4. cook 门控 commandlet（AC-4c Alpha gate）
```
UnrealEditor-Cmd rento.uproject -run=BoardCookGate -unattended -nopause -nosplash -stdout
→ LogTemp: BoardCookGate: 共找到 1 个 UBoardDataAsset 资产
→ LogTemp Error: [PLACEHOLDER] 资产 /Game/Boards/DA_Board_Classic40 的 bIsPlaceholderData==true
→ 进程 EXIT: 1（Alpha gate 阻断；MVP/nightly CI 可忽略此退出码）
```
gate 真读字段并分支（打印精确资产路径），非 vacuous。MVP 阶段占位资产触发门控=预期行为。

### 5. 全量回归
```
Automation RunTests Rento.
→ 161/161 Result={Success}, 0 Fail（基线 153 + 新增 8 Classic40 = 161，零回归）
```

## AC 覆盖表

| AC | 状态 | 覆盖方式 |
|---|---|---|
| AC-3 类型精确计数 | COVERED | TC_AC3（自动） |
| AC-4-asset temp-fill 已填 | COVERED | TC_AC4asset（自动） |
| AC-4b Utility schema | COVERED | TC_AC4b（自动） |
| AC-4c temp-fill 硬门控 | COVERED | TC_AC4c（字段可读）+ BoardCookGate commandlet（gate 机制，自动） |
| AC-5 数组语义长度 | COVERED | TC_AC5（自动） |
| AC-6 色组成员 | COVERED | TC_AC6（自动） |
| AC-7 经典布局精确匹配 | COVERED | TC_AC7（自动，独立 oracle） |
| AC-7b Go SalaryAmount schema | COVERED | TC_AC7b（自动） |
| Harness 就位 | COVERED | Classic40AssetValidationTest + BoardCookGate commandlet 均可 CLI 执行 |

## code-review 闭环（2026-06-07）

unreal-specialist + qa-tester 并行审（均 schema-less，主会话逐条独立裁定 + 防截断 resume）。0 真 BLOCKING。应用 3 should-fix + 1 验证驱动新发现：

| Fix | 来源 | 修法 | 验证 |
|---|---|---|---|
| F-1 cook-gate fail-closed | qa BLOCKING-1 + unreal W-3 | 无资产从 return 0 改 Error+return 1（gate 不得 fail-open） | 占位→EXIT 1 ✓；无资产→EXIT 1 ✓（两路径实测）|
| F-2 MarkPackageDirty | unreal W-2 | `SetDirtyFlag(true)`→`MarkPackageDirty()` | gen EXIT 0 |
| F-3 TC_AC6b | qa GAP-2 + unreal I-3 | 新增经 `GetTilesInGroup` 验真资产的集成 test（保留直接遍历 oracle） | Classic40 9/9 含 TC_AC6b |
| **BONUS 幂等覆盖崩溃** | 验证驱动主会话抓 | 真因=第二次覆盖时 SavePackage `appError:资产只加载一部分`（`SAVE_NoError` 曾掩盖成 code1）；修 `CreatePackage`后`FullyLoad()`+`FindObject`复用清空 | 连跑 gen ×2 EXIT 0 幂等 |

**最终全绿**：编译 Succeeded ｜ generator 幂等 EXIT 0 ｜ Classic40 **9/9 Success** ｜ cook-gate 两路径 EXIT 1 ｜ 全量回归 **162/162 Success, 0 Fail**（153+9 零回归）。

deflate/登记 tech-debt（非 BLOCKING）：W-1（WITH_EDITOR 包裹 UCLASS→Shipping 前迁 rentoEditor 模块）/ W-4（Main 345 行可选重构）/ I-2（LogTemp→LogRento 项目级）/ GAP-1·3（AC-4c false 路径覆盖留经济 epic）/ INFO-4（多 board gate 场景）/ CI 排序（cook-gate 须 GenerateClassic40 后跑）。详见 tech-debt-register。

## 遗留 Advisory（非阻塞）

- **TR-board-015 / ADR-0002 VC#1**：编辑器 Details 面板手工录入 TArray 多值的截图证明（证"策划可经 Details 录入"GUI 路径）——commandlet 程序化生成证明了 schema 能承载多值，但未证 GUI 录入路径。此为 [Advisory] 手动项（BD-001 已记 tech-debt line 6），可推迟，须人工开编辑器截图。
