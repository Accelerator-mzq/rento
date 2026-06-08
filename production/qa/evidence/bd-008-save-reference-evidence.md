# BD-008 证据 — 棋盘 DA 存档引用契约（BoardIdentifier）：存引用不存布局

> Story: `production/epics/board-data/story-008-board-id-save-reference.md`
> 日期: 2026-06-08 | 模式: Collaborative
> 验证者: 主会话独立核实（编译 + 跑 Automation，非信 agent 自报）

## 交付物

| 文件 | 作用 |
|---|---|
| `Source/rentoTests/Private/BoardIdSaveReferenceTest.cpp` | 板侧存档引用契约自动化测试（3 TC，类目 Rento.Board.SaveReference） |
| `production/qa/evidence/bd-008-save-reference-evidence.md` | Advisory code-review 签核表 + OQ-8 回链登记 + AC-28b deferral 记录 |

## AC-板存档供给 code-review 签核表

### 板侧契约描述

- **提供接口**：`GetBoardId() → FName`（`BoardDataAssetHost.h` L148-149，UFUNCTION BlueprintPure）
- **返回语义**：取 `UBoardDataAsset` 顶层 `BoardIdentifier` 字段（作者手填逻辑 ID，与资产路径 / PrimaryAssetId / 文件名解耦）；无板时返回 `NAME_None`
- **禁止的接口**：无「序列化全量布局」public UFUNCTION（SerializeLayout / SaveLayout / SerializeBoard / ExportLayout 等均不存在）
- **存档容器对棋盘**：仅持 `BoardIdentifier: FName`；全量布局由 DA 重建，存档不复制静态数据（ADR-0005 存DA引用不存布局）

### 自动化测试证据

| TC | Automation 类目 | 验证内容 | AC 映射 |
|---|---|---|---|
| TC1 | `Rento.Board.SaveReference.TC1_GetBoardIdReturnsTopLevelIdentifier` | GetBoardId()==BoardIdentifier 顶层字段；解耦守卫 GetBoardId()!=资产对象名（非 vacuous） | AC-板存档供给 / AC-28 |
| TC2 | `Rento.Board.SaveReference.TC2_GetBoardIdNoneWhenNoBoard` | 无板时 GetBoardId()==NAME_None（null 契约） | AC-板存档供给 |
| TC3 | `Rento.Board.SaveReference.TC3_NoLayoutSerializationOnBoardSide` | UBoardDataAsset 本类 0 个 CPF_SaveGame 字段；BoardIdentifier 字段存在且是 FNameProperty（非 vacuous）；board host 无全量布局序列化 UFUNCTION | AC-板存档供给 / ADR-0005 |

### code-review sign-off

| 检查项 | 状态 | Reviewer |
|---|---|---|
| GetBoardId() 返回顶层 BoardIdentifier 而非从路径派生 | PASS（TC1 + 变异坐实解耦守卫非-vacuous） | msc 2026-06-08 |
| 无全量布局序列化接口（SerializeLayout 等黑名单）均不存在 | PASS（TC3c UFUNCTION 黑名单扫描 0 命中） | msc 2026-06-08 |
| UBoardDataAsset 无 UPROPERTY(SaveGame) 字段 | PASS（TC3a 反射 SaveGameCount==0 + TC3b 锚点守卫） | msc 2026-06-08 |
| TC1/TC2/TC3 全绿（自动化运行结果） | PASS（主会话独立 3/3 Success + 全量 269/269 零回归） | msc 2026-06-08 |

---

## OQ-8 回链登记

**登记时间**: 2026-06-08
**来源**: story-008 §Acceptance Criteria OQ-8 / §Implementation Notes §5

### 强制要求

AC-28b（`GetBoardId` 跨会话/跨存档稳定性）的 **BLOCKING 测试须在存档系统(21) GDD §Acceptance Criteria 中出现显式实现 AC**，并注明「实现 board-data AC-28b」。

- `/design-review` 审查存档(21) GDD 时：核对此回链是否存在
- 缺失判据：存档(21) GDD §AC 中无任何条目引用 `board-data AC-28b` 或等效表述
- **缺失则存档(21) GDD 不得 APPROVED**（此为 design-review gate 硬线）

### 跨系统追踪

| 系统 | 责任 | 状态 |
|---|---|---|
| board-data (story-008，本文件) | 声明契约：`GetBoardId() → FName` 返回顶层 `BoardIdentifier`；板侧无全量布局序列化 | DONE（自动化 TC1/TC2/TC3） |
| save-load epic 21 | 实现 AC-28b：跨会话 round-trip BLOCKING 测试（存档写 BoardIdentifier → 重启读档 → 再调 GetBoardId() 一致） | **PENDING**（待存档21 GDD 撰写 + 回链登记） |

---

## AC-28b Deferral 记录

**Deferral 依据**: story-008 §Acceptance Criteria AC-28b / §Out of Scope

**原因**: 板侧无法跨进程验证。`GetBoardId()` 契约正确性（取顶层字段、返回稳定 FName）已由 TC1/TC2 在进程内验证。跨会话 round-trip（存档写入 → 进程退出 → 重启读档 → 再查 GetBoardId 一致性）属存档框架责任，板侧单元测试机制无法覆盖。

**归属**: `tests/integration/save-load/`（save-load epic 21 Integration 测试）

**测试 Gate**: BLOCKING（存档21 Integration，须在 CI 全量回归前实现）

**本系统状态**: Advisory（契约声明已自动化验证；跨进程验证 defer save-load 21）
