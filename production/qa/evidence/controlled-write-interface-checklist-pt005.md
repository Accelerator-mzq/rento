# Controlled Write Interface Checklist — pt-005
# AC-2（GDD AC-35）：[Advisory] code-review 受控写接口软约束核对
# 日期：2026-06-07
# Story：story-005（受控写接口面）

## 核对背景

本 checklist 对应 story pt-005 AC-2（GDD AC-35）：
> 当前稳定门控 = [Advisory] code-review — 受控写是架构约定，BP 无法引擎级硬拦截调用方（软约束）；
> 首次实现保留 code-review checklist，记录于 production/qa/evidence/。

受控写接口面封装强度状态（OQ-1 ADR 未裁前）：
- 字段保持 `UPROPERTY(BlueprintReadOnly)`（不改 private）
- AC-35a 静态扫描门 **deferred**（待 OQ-1 ADR 裁定后激活）
- 当前门控 = code-review 软约束（本文件）

---

## 受控写接口清单（GDD CR-1 L92-99）

| 字段 | setter 名称 | 唯一合法调用方 | 文件位置 |
|------|------------|--------------|---------|
| `CurrentTileIndex` | `SetPosition(int32 Index)` | 移动(4) 系统 | `Source/rento/RentoPlayerState.h/.cpp` |
| `Cash` | `SetCash(int32 Value)` | 经济(5) 系统 | `Source/rento/RentoPlayerState.h/.cpp` |
| `bIsInJail` | `SetJailState(bool, EJailReason)` | 事件格(7) 系统 | `Source/rento/RentoPlayerState.h/.cpp` |
| `CurrentRollContext` | `SetCurrentRollContext(const FDiceRollResult&)` | 事件格(7)，仅事件额外掷骰 | `Source/rento/RentoPlayerState.h/.cpp` |
| `bIsBankrupt` | `SetBankrupt(bool)` | 本系统回合(2)，经 `HandlePlayerBankruptcy` | `Source/rento/RentoPlayerState.h/.cpp` |

---

## Code-Review 核对项

### [x] CR-1：每个委派字段恰有唯一具名 setter（AC-1）
- SetPosition → CurrentTileIndex：存在，`UFUNCTION(BlueprintCallable)`，文档化唯一调用方
- SetCash → Cash：存在，`UFUNCTION(BlueprintCallable)`，文档化唯一调用方
- SetJailState → bIsInJail：存在，`UFUNCTION(BlueprintCallable)`，文档化唯一调用方
- SetCurrentRollContext → CurrentRollContext：存在，`UFUNCTION(BlueprintCallable)`，文档化唯一调用方
- SetBankrupt → bIsBankrupt：存在，`UFUNCTION(BlueprintCallable)`，文档化唯一调用方

### [x] CR-2：setter 文档注释明确唯一合法调用方（ADR-0007 BP↔C++ handoff）
- 每个 setter 的 `UFUNCTION` meta `DisplayName` 和 `ToolTip` 均注明唯一调用方
- `.h` 文件 doc 注释中逐条说明「唯一合法调用方」

### [x] CR-3：字段封装现状确认（OQ-1 ADR 未裁前的软约束状态）
- 字段保持 `UPROPERTY(BlueprintReadOnly)`（不改 private）
- **不触碰 pt-001/002/003 测试的直接字段读写**（如 `State->bIsInJail = true`）
- AC-3/AC-35a 静态扫描门标注为 deferred，无恒 pending 自动测试

### [x] CR-4：SetBankrupt 唯一路径迁移（AC-4）
- `PlayerTurnSubsystem.cpp::HandlePlayerBankruptcy` 中原直写 `DebtorState->bIsBankrupt = true`
  已迁移为 `DebtorState->SetBankrupt(true)`
- 破产(9) `IResolveBankruptcy` 接口为 return-only，物理阻断不直写字段（2↔9 防环）

### [x] CR-5：CurrentRollContext 正确定义（GDD CR-3.1）
- 新增 holder 字段 `UPROPERTY(BlueprintReadOnly) FDiceRollResult CurrentRollContext`
- setter 签名：`SetCurrentRollContext(const FDiceRollResult& RollResult)`
- PULL 消费 + 程间非重入语义 deferred 至 story-006（本 story 只声明 holder + setter）

### [x] CR-6：EJailReason 最小占位（非完整枚举）
- `EJailReason` 枚举声明在 `PlayerTurnTypes.h`，仅含 `None=0` 占位
- 完整 EJailReason 规则归事件格7 epic（append-only，从 ordinal 1 开始追加）
- `SetJailState` 接受 `EJailReason Reason = EJailReason::None`（默认参数，BP 可不传）

---

## 自动测试覆盖

| TC | 覆盖 AC | 测试文件 | 状态 |
|----|---------|---------|------|
| TC-1 | AC-1 | `Source/rentoTests/Private/ControlledWriteInterfaceTest.cpp` | 已实现 |
| TC-2 | AC-2 | 本 checklist 文件（Advisory，非自动测试） | 已完成（本文件） |
| TC-3 | AC-3 | deferred — OQ-1 ADR 未裁前不激活 | deferred |
| TC-4 | AC-4 | `Source/rentoTests/Private/ControlledWriteInterfaceTest.cpp` | 已实现 |

---

## Out of Scope 确认（未触碰项）

- `bIsBankrupt` 触发语义（破产9 return-only 驱动 + F-1 移出）→ story-003 边界
- `SetCurrentRollContext` 的 PULL 消费 + 程间非重入 → story-006
- 移动4 `SetTileIndex` 公开 API 实现 → movement epic
- 经济5 `Debit`/`Credit` 现金公式 → economy epic
- PlayerState 字段构成与默认值 → story-001 已落地，本 story 只扩展写路径

---

## 封装升级路径（OQ-1 ADR 裁定后执行）

1. OQ-1 ADR 签署，选择「C++ 强封装」方案
2. 委派字段改为 `private`（或保持 BlueprintReadOnly 但 C++ 访问限制）
3. 激活 AC-35a [Logic] 自动测试（静态扫描）：
   - grep 非白名单文件（TurnManager/PlayerStateController）
   - 断言无对 Cash/CurrentTileIndex/bIsInJail/bIsBankrupt 的直接赋值
4. pt-001/002/003 测试对应直接字段读写路径改为经 getter/setter
5. ADR-0007 标注受控写升为硬保证
