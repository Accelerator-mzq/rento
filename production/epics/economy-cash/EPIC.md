# Epic: 经济与现金 Economy & Cash

> **Layer**: Core ⚠ **bottleneck**
> **GDD**: design/gdd/economy-cash.md
> **Architecture Module**: 经济现金（architecture §2.3）— 租金/抵押/建房公式发源地
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories economy-cash`

## Overview

经济现金是整个游戏的经济 bottleneck —— 租金、抵押、建房、发薪、税收、破产判据的所有公式（F-1..F-11）发源于此。它独占每玩家 `Cash` 写权威，经强封装的 `Credit`/`Debit` 受控写接口落地现金侧执行，对外暴露 `GetCash`/`is_insolvent`/F-9 `net_liquidation_value` 估值口径。架构裁定为消解环依赖：被地产6 调（6→5 执行现金侧）、被破产9 调，**绝不反调 6/9**；建房 `house_count` 归建房8 不归 5（防 5↔8 环）；AI 经只读 GameStateSnapshot 读估值（预聚合 NLV 由回合2 装配，AI 严禁自算）。MVP 聚焦本地 + AI 跑通核心经济循环。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
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
| TR-econ-014 | F-6/F-8/F-9 整数取整路径跨编译配置/两次冷启动位级一致、无 float | ❌ No ADR |
| TR-econ-015 | int32 溢出硬防护：passed_go clamp[0,1000] + SalaryAmount≤2e6/DiceMultiplierTable 上界加载期校验 | ❌ No ADR |
| TR-econ-016 | F-2 数据包络跨公式约束加载期校验（RentTable[1]≥RentTable[0]×2 等） | ADR-0002 ⚠ Partial |
| TR-econ-017 | 经济读棋盘 base 金额（GetTileData→Price/RentTable/MortgageValue/TaxAmount/SalaryAmount） | ADR-0002 ✅ |
| TR-econ-018 | 现金转移原子性（单一 R 局部变量 Debit+Credit，无造币/丢币）；破产移交锁 bIsMidBankruptcyTransfer | ADR-0007 ⚠ Partial |

**覆盖**: 12 Covered / 4 Partial / **2 Gap（无 ADR：TR-econ-014/015）**。

> ⚠ **2 条 untraced TR**（TR-econ-014 整数确定性 / TR-econ-015 溢出防护）无 ADR。其 story 在 `/create-stories` 时标 **Blocked** 直到 `/architecture-decision` 补 ADR。二者均为实现期工程门（确定性回归 + 加载期数据校验），可在经济公式 story 推进期并行补 ADR。

## Definition of Done

This epic is complete when:
- All stories are implemented, reviewed, and closed via `/story-done`
- All acceptance criteria from `design/gdd/economy-cash.md` are verified
- All Logic and Integration stories have passing test files in `tests/`（physical：`Source/rentoTests/Private/`）
- 2 untraced TR（econ-014/015）的 ADR 已补齐并解除其 story 的 Blocked 状态
- 环依赖纪律经测试守护：6→5/9→5 单向，economy 不反调 6/9（spy 断言无反向环）

## Next Step

Run `/create-stories economy-cash` to break this epic into implementable stories.
