---
Epic: player-turn
Status: Ready
Layer: Foundation
Type: Logic
Estimate: S
Manifest Version: 2026-06-06
Last Updated: 2026-06-06
---

# Story 005 — 受控写接口面（SetPosition/SetCash/SetJailState/SetBankrupt）C++ 强封装

## Context

- **GDD**: `design/gdd/player-turn.md` — CR-1 受控写接口面（具名清单 L92-99）、受控写强度待 OQ-1 ADR 裁定（L103）、CR-3.1 `SetCurrentRollContext`、AC-35/AC-35a（受控写契约门控）
- **Requirement TR-ID**: TR-turn-003（受控写接口面 SetPosition/SetCash/SetJailState/SetBankrupt 等，强度=C++ 强封装〔字段 private + C++ setter〕硬保证 vs BP 软约束）— **Partial**（强度待 OQ-1 ADR 细化，非 Gap）
- **ADR Governing**:
  - **ADR-0007（primary）— BP/C++ 权威边界**：写权威状态（`PlayerState`/`Cash`/`bIsInJail`/`bIsBankrupt`）→ C++；受控写强封装=字段 private + C++ setter + BP 只读（真硬保证）vs BP 约定 + code-review（软约束）。强度随 OQ-1 ADR 容器选择分叉。
  - **ADR-0001 — UObject 宿主与生命周期**：受控写接口面挂回合2 World Subsystem / PlayerState 容器；宿主不持状态写语义（字段值的改写规则归各 owner，本系统只暴露唯一合法写路径）。
- **Engine**: Unreal Engine 5.7（Blueprint 为主 + C++ 框架）— **Risk: LOW**（BP/C++ 互操作 `UFUNCTION`/`UPROPERTY`/`UCLASS` 是 UE 长期稳定基础设施）
- **Engine Notes（ADR-0007 Engine Compatibility）**:
  - Post-Cutoff APIs Used: **None** — 本 ADR 是组织/边界裁定，不引入 5.4+ 新 API。
  - Verification Required: 静态符号扫描门若采用，须在 CI 验证能解析 `uasset` 检出违规直写、能区分白名单（经 setter 的 `UFUNCTION`）。MVP 可先以「权威逻辑无 BP 类」目录级断言替代逐节点扫描。
  - **强度分叉（GDD L103 + OQ-1 因子⑨）**：UE 中 `BlueprintCallable` 无调用方限制，BP-primary 项目无法在引擎层硬性阻止任意持有引用的 BP 图调用 setter——只能靠 (a) C++ 强封装（字段 private + C++ setter，BP 只读=真硬保证）或 (b) BP 约定 + code-review（软约束）。容器选 C++ 强封装时须防 Blueprintable 子类遮蔽漏洞（`meta=(IsBlueprintBase=false)` 或选 UObject 而非 AActor）。
- **Control Manifest Rules（Core Layer）**:
  - **Required（Core）**: 写权威状态? → C++。任何写 `PlayerState`/owner map/`house_count`/`Cash`/牌堆顺序/状态机 phase 落 C++（source: ADR-0007）。BP↔C++ handoff → C++ 暴露 `BlueprintCallable`，BP 经此入口（可 spy）（source: ADR-0007）。
  - **Forbidden（Core）**: Never 让 BP 呈现层回写权威状态——BP 仅订阅·显示·转发（纯叶子）（source: ADR-0007）。Never 让权威逻辑出现 BP 派生类（source: ADR-0007）。Never 用纯 code-review 软约束禁随机——可被「C++ 文本 grep 硬门 + 目录级断言」廉价取代（source: ADR-0007，受控写同性质参照）。
  - **Guardrail**: 权威逻辑 C++ 热路径优于等价 BP VM，回合制低频差异非瓶颈；CPU 预算 16.6ms。

## Acceptance Criteria

- [ ] AC-1（TR-turn-003，GDD L92-99）: 对每个委派字段暴露**唯一具名**受控写接口，调用方严格受限——`SetPosition(index)`（唯一合法调用方=移动4）、`SetCash(value)`/`Debit`/`Credit`（经济5）、`SetJailState(bInJail, reason)`（事件格7）、`SetCurrentRollContext(FDiceRollResult)`（事件格7，仅事件额外掷骰）、`SetBankrupt(bool)`（本系统回合2，据破产9 return-only 写，9 不直写）。
- [ ] AC-2（GDD AC-35）: 当前稳定门控=[Advisory] code-review——受控写是架构约定，BP 无法引擎级硬拦截调用方（软约束）；首次实现保留 code-review checklist，记录于 `production/qa/evidence/`。
- [ ] AC-3（GDD AC-35a，条件门）: 若 OQ-1 ADR 选 **C++ 强封装**（字段 private + C++ setter + BP 只读），受控写升为**可回归硬门**——届时新增 `AC-35a [Logic]` 架构/静态测试：扫描非 `TurnManager`/`PlayerStateController` 文件，断言无对 `PlayerState` 字段（Cash/CurrentTileIndex/bIsInJail/bIsBankrupt）的直接赋值，仅允许经 setter。**在 ADR 签署前 AC-35a 不作确定性 gate 条目**（避免永远 pending 的 gate）；OQ-1 标 `resolved` 且容器选 C++ 强封装时**必须**同批新增此 AC，否则 ADR 不得标 resolved。
- [ ] AC-4（GDD L99/CR-6）: `SetBankrupt(bool)` 的唯一合法调用方=本系统（回合2），据破产9 `ResolveBankruptcy` 返回 `debtorEliminated=true` 写入；破产9 为 return-only 编排、**不直写本字段**（防 2↔9 环）。

## Implementation Notes

> 逐字保真自 ADR-0007 Implementation Guidelines #1/#6 + GDD CR-1 受控写接口面，不改写语义。

1. **写权威状态？→ C++**（ADR-0007 边界判据 #1）：任何写 `PlayerState`/`Cash`/`bIsInJail`/`bIsBankrupt`/`CurrentTileIndex` 的逻辑必落 C++（原则 4 + 单一 owner 不变式）。
2. **BP↔C++ handoff → C++ 暴露 `BlueprintCallable`**（ADR-0007 #6）：BP 调用方经此入口（非 BP 直连 BP），使调用可 spy。
3. **受控写接口面（GDD L92-99，具名清单）**：每个委派字段唯一具名受控写接口 + 唯一合法调用方：`CurrentTileIndex`→`SetPosition(index)`/移动4；`Cash`→`SetCash(value)`/`Debit`/`Credit`/经济5；`bIsInJail`→`SetJailState(bInJail, reason)`/事件7；当前程掷骰上下文→`SetCurrentRollContext(FDiceRollResult)`/事件7（仅事件额外掷骰更新 holder）；`bIsBankrupt`→`SetBankrupt(bool)`/本系统回合2（据破产9 return-only 写，9 不直写）。
4. **强度待 OQ-1 ADR 裁定（GDD L103，R1 措辞校正）**：受控写接口是架构层约定，强制力取决于 OQ-1 ADR 容器实现选择，而非引擎级保证。目标是「明确唯一合法写路径」，而非声明一个 BP 兜不住的工程级防篡改保证。
5. **`SetPosition` 与移动4 `SetTileIndex` 关系（GDD L101，OQ-Move-3b）**：移动4 对其他系统暴露 `SetTileIndex`/`Advance`/`TeleportTo` 作位置写公开 API；本系统 `SetPosition` 是 `PlayerState.CurrentTileIndex` 字段级受控写，仅供移动4 内部最终落位调用。两层 vs 同层别名由 OQ-Move-3b ADR 裁定，实现期收敛为单一命名链；裁定前两档以交叉引用消歧。

## Out of Scope

- PlayerState 字段构成与默认值 → **story-001**（本 story 封装其委派字段的写路径）。
- `bIsBankrupt` 何时写（破产9 return-only 驱动 + F-1 移出）→ **story-003**（本 story 只封装 `SetBankrupt` setter，触发语义归 003）。
- `SetCurrentRollContext` 的当前程 `Total` PULL 消费 + 程间非重入 → **story-006**（本 story 只声明该具名 setter）。
- 移动4 `SetTileIndex` 公开 API 实现 → movement epic。
- 经济5 `Debit`/`Credit` 现金公式 → economy epic（本系统只暴露 `SetCash` 字段级 setter）。

## QA Test Cases

> 本 story 为 Logic（BoundaryEnforcement，封装强度）；每 AC 一条 Given/When/Then；headless `-nullrhi`。当前 MVP 门控为 code-review checklist（AC-2）；AC-35a 静态门仅在 OQ-1 选 C++ 强封装后激活。

- **TC-1 (AC-1)**: GIVEN PlayerState 委派字段集合，WHEN 检查受控写接口，THEN 每字段恰一个具名 setter（`SetPosition`/`SetCash`/`SetJailState`/`SetCurrentRollContext`/`SetBankrupt`），调用方文档化限定。
- **TC-2 (AC-2)**: GIVEN 首次实现，WHEN code-review，THEN 受控写 checklist 通过、记录于 `production/qa/evidence/`（当前稳定门=[Advisory] code-review，软约束）。
- **TC-3 (AC-3，条件门)**: GIVEN OQ-1 ADR 选 C++ 强封装，WHEN 新增 AC-35a [Logic]，THEN 扫描非 `TurnManager`/`PlayerStateController` 文件，断言无对 Cash/CurrentTileIndex/bIsInJail/bIsBankrupt 的直接赋值（仅经 setter）；ADR 签署前此 AC 不作确定性 gate。
- **TC-4 (AC-4)**: GIVEN 破产9 `ResolveBankruptcy` 返回 `debtorEliminated=true`，WHEN 本系统写 `bIsBankrupt`，THEN 经 `SetBankrupt(true)` 由本系统调用；断言破产9 不直写本字段（return-only）。
- **Edge cases**: 容器选 C++ 强封装时须防 Blueprintable 子类遮蔽（`meta=(IsBlueprintBase=false)` 或 UObject 而非 AActor）；`SetCurrentRollContext` 仅事件7 在事件额外掷骰时调用（非每程）；强度门在 OQ-1 未裁前保持 [Advisory]，勿写永远 pending 的 [Logic] gate。

## Test Evidence

- **Type**: Logic（强度门当前为 [Advisory] code-review，AC-35a [Logic] 静态门待 OQ-1 ADR 激活）
- **Path**: `tests/unit/player-turn/controlled_write_interface_test.cpp`（AC-35a 静态扫描）+ `production/qa/evidence/`（AC-35 code-review checklist）
- **Status**: 未创建（待 dev-story 实现；AC-35a 待 OQ-1 ADR 裁定后补）

## Dependencies

- **Depends on**: story-001（PlayerState 容器及其委派字段）。
- **Unlocks**: story-003（经 `SetBankrupt` 写 `bIsBankrupt`）、story-006（`SetCurrentRollContext` holder 更新）；移动4/经济5/事件7 epic 经各自具名 setter 写 PlayerState 委派字段。
