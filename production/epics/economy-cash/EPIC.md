# Epic: 经济与现金 Economy & Cash

> **Layer**: Core ⚠ **bottleneck**
> **GDD**: design/gdd/economy-cash.md
> **Architecture Module**: 经济现金（architecture §2.3）— 租金/抵押/建房公式发源地
> **Status**: Ready
> **Stories**: 10 stories created (2026-06-08) — see ## Stories

## Overview

经济现金是整个游戏的经济 bottleneck —— 租金、抵押、建房、发薪、税收、破产判据的所有公式（F-1..F-11）发源于此。它独占每玩家 `Cash` 写权威，经强封装的 `Credit`/`Debit` 受控写接口落地现金侧执行，对外暴露 `GetCash`/`is_insolvent`/F-9 `net_liquidation_value` 估值口径。架构裁定为消解环依赖：被地产6 调（6→5 执行现金侧）、被破产9 调，**绝不反调 6/9**；建房 `house_count` 归建房8 不归 5（防 5↔8 环）；AI 经只读 GameStateSnapshot 读估值（预聚合 NLV 由回合2 装配，AI 严禁自算）。MVP 聚焦本地 + AI 跑通核心经济循环。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0014: 整数确定性与溢出防护 | 金钱运算整数纯净无 float+显式取整(ceil/floor)+逐栋 floor 先于求和（位级一致 AC-32）；passed_go 运行时 clamp[0,1000]；SalaryAmount≤2e6/DICE_MULT_MAX=1e6 加载期 fatal | LOW |
| ADR-0007: BP-vs-C++ 边界 | 经济公式落 C++ 权威层（[Logic] headless 可测）；Credit/Debit/GetCash 标 UFUNCTION(BlueprintCallable) | LOW |
| ADR-0003: 事件总线 | 4 事件 owner-held DYNAMIC_MULTICAST_DELEGATE + BlueprintAssignable；方向由消费方派生；OnBankruptcyDeclared 现金侧切分 | LOW |
| ADR-0006: GameStateSnapshot | AI 经只读快照读经济估值，预聚合 NLV 回合2 装配期算（防 5→8 反向环） | LOW |
| ADR-0005: 存档契约 | 每玩家 Cash 序列化（CashByPlayer TMap）；读档经容器写回不经 Credit/Debit（OnCashChanged 0 次） | LOW |
| ADR-0002: 棋盘容器 | 经济读棋盘 base 金额（Price/RentTable/MortgageValue/TaxAmount/SalaryAmount）；F-2 数据包络加载期校验 | LOW※ |
| ADR-0001: UObject 宿主 | 经济服务对局期 UObject 宿主（Cash 权威服务实例随局生灭）；Cash 容器私有 + 强封装 | LOW |

> ※ ADR-0002 的 MEDIUM 引擎风险（UAssetManager 异步加载签名）已在 board-data Sprint0 验证；经济仅只读消费棋盘 base 金额，不新增引擎面风险。架构 §2.3 标本系统 risk = LOW。

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-econ-001 | 经济服务对局期 UObject 宿主（Cash 权威服务实例随局生灭） | ADR-0001 ⚠ Partial |
| TR-econ-002 | 受控写宿主形态：Cash 容器私有 + Credit/Debit 强封装，禁外部直写 | ADR-0007 ✅ |
| TR-econ-003 | 经济公式 F-1..F-11 落 C++ 权威层，[Logic] AC headless 可测 | ADR-0007 ✅ |
| TR-econ-004 | Credit/Debit/GetCash/GetUnmortgageCostForDisplay 标 UFUNCTION(BlueprintCallable) | ADR-0007 ✅ |
| TR-econ-005 | 4 事件 DYNAMIC_MULTICAST_DELEGATE + BlueprintAssignable + payload USTRUCT append-only | ADR-0003 ✅ |
| TR-econ-006 | 事件次数/payload spy 可断言（恰 1 次 OnRentPaid + 2 次 OnCashChanged 等） | ADR-0003 ✅ |
| TR-econ-007 | 方向由消费方派生（EChangeReason 无方向位，delta 符号 + Payer/Payee 视角） | ADR-0003 ✅ |
| TR-econ-008 | OnBankruptcyDeclared（经济5现金侧信号）与破产9 OnPlayerBankrupt 双事件源切分、禁双发 | ADR-0003 ✅ |
| TR-econ-009 | 每玩家 Cash 序列化（CashByPlayer TMap<int32,int32>，SaveGame） | ADR-0005 ✅ |
| TR-econ-010 | 读档 Cash 经容器写回、不经 Credit/Debit，OnCashChanged 触发 0 次 | ADR-0005 ✅ |
| TR-econ-011 | Raising Funds 瞬态不中途存档（瞬态债务不序列化） | ADR-0005 ⚠ Partial |
| TR-econ-012 | preaggregated_nlv（F-9/F-10）由回合2 装配期预聚合 6(MV)+8(sellback)，AI 严禁自算 | ADR-0006 ✅ |
| TR-econ-013 | AI 经只读 GameStateSnapshot 读经济估值，不持活指针 | ADR-0006 ✅ |
| TR-econ-014 | F-6/F-8/F-9 整数取整路径跨编译配置/两次冷启动位级一致、无 float | ADR-0014 ✅ |
| TR-econ-015 | int32 溢出硬防护：passed_go clamp[0,1000] + SalaryAmount≤2e6/DiceMultiplierTable 上界加载期校验 | ADR-0014 ✅（SalaryAmount 校验 board propagate 债） |
| TR-econ-016 | F-2 数据包络跨公式约束加载期校验（RentTable[1]≥RentTable[0]×2 等） | ADR-0002/0014 ⚠ Partial（warning，board propagate 债） |
| TR-econ-017 | 经济读棋盘 base 金额（GetTileData→Price/RentTable/MortgageValue/TaxAmount/SalaryAmount） | ADR-0002 ✅ |
| TR-econ-018 | 现金转移原子性（单一 R 局部变量 Debit+Credit，无造币/丢币）；破产移交锁 bIsMidBankruptcyTransfer | ADR-0007 ⚠ Partial |

**覆盖**: 14 Covered / 4 Partial / **0 Gap**。

> ✅ **2 条 Gap TR 已解除（ADR-0014 Accepted，2026-06-08）**：TR-econ-014（整数确定性：num/den+显式取整+逐栋 floor+零 float→位级一致）/ TR-econ-015（溢出防护：passed_go 运行时 clamp[0,1000]+SalaryAmount≤2e6/DICE_MULT_MAX=1e6 加载期 fatal）。详见 `docs/architecture/ADR-0014-economy-integer-determinism-overflow.md`。
> ⚠ **propagate 债（board-data 已 Approved+实现）**：① SalaryAmount≤2e6 fatal 加载校验 board 现无（仅 >0/==0），须后续 board story 或 `/propagate-design-change` 新增；② RentTable 跨公式包络（OQ-Econ-9，warning，TR-econ-016 仍 Partial）board 现仅自身单调。DiceMult 上界已落 board AC-23j（DICE_MULT_MAX=1e6 对齐其建议初值）。

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Cash 服务 + Credit/Debit 受控写 + 守恒/不变式 | Logic | Ready | ADR-0001/0007/0014 |
| 002 | 经济事件契约 (4 delegate + EChangeReason) | Logic | Ready | ADR-0003/0007 |
| 003 | 发薪 F-1 (clamp + 溢出 guard + gate) | Logic | Ready | ADR-0014/0001 |
| 004 | 地产租金 F-2 (piecewise, ×2 仅无房 base, 抵押短路) | Logic | Ready | ADR-0014/0006/0007 |
| 005 | 车站/公用租金 F-3/F-4 (count guards + dice_total PULL) | Logic | Ready | ADR-0015/0014/0006 |
| 006 | 抵押/赎回现金腿 F-5/F-6 + 显示读接口 + 无套利 | Logic | Ready | ADR-0014/0007 |
| 007 | 缴税 F-7 + 买地现金腿 CR-4 | Logic | Ready | ADR-0007/0001 |
| 008 | NLV F-9 + F-10 is_insolvent + F-8 卖回 🔴最高风险 | Logic | Ready | ADR-0014/0006 |
| 009 | 凑钱状态机 CR-7 + 破产现金侧 F-11 | Integration | Ready | ADR-0001/0014/0003 |
| 010 | Cash 存档序列化 round-trip | Integration | Ready | ADR-0005/0003 |

> **依赖序**：001 → 002 → {003,004,005,006,007,008 并行} → 009(需 006/008) → 010(需 001/002)。8 Logic + 2 Integration，全 Ready（无 Blocked）。
> **TR 覆盖**：econ-001..018 全覆盖（econ-016 数据包络=board propagate 债非本 epic story；econ-013 AI snapshot 读经 story-008 暴露 + ADR-0006 装配）。AC-1..43（41 Logic + 3 Advisory：AC-4/40/41）分布于 10 story；AC-40/41 = Advisory 证据（playtest/smoke）非 dev story。

## Definition of Done

This epic is complete when:
- All stories are implemented, reviewed, and closed via `/story-done`
- All acceptance criteria from `design/gdd/economy-cash.md` are verified
- All Logic and Integration stories have passing test files in `tests/`（physical：`Source/rentoTests/Private/`）
- ✅ 2 untraced TR（econ-014/015）的 ADR 已补齐（ADR-0014 Accepted 2026-06-08）；SalaryAmount 加载校验 propagate 债待 board story 闭合
- 环依赖纪律经测试守护：6→5/9→5 单向，economy 不反调 6/9（spy 断言无反向环）

## Next Step

10 stories created. Run `/story-readiness production/epics/economy-cash/story-001-cash-service-controlled-write.md` then `/dev-story` to begin implementation. Work in dependency order (each story's `Depends on:` field gates start).
