# Epic: 骰子与确定性 RNG 服务

> **Layer**: Foundation
> **GDD**: design/gdd/dice.md
> **Architecture Module**: #3 骰子 Dice + 权威 RNG 流
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories dice-rng`
> **Manifest Version**: 2026-06-06

## Overview

实现项目的**单一确定性随机源**——零依赖的种子化 RNG 服务。`DiceService` 持唯一权威 `FRandomStream`，
所有需可重放的 gameplay 随机（骰点、牌堆 Fisher-Yates 洗牌、AI 决策扰动）走此流；表现层 juice
（VFX/Audio/HUD）与 AI 内部扰动各持**独立非权威流**，严禁复用权威流（否则污染确定性重放，联网 desync 根源）。
暴露 `RollDice()→FDiceRollResult{Die1,Die2,Total,bIsDouble}`、`RandomRange`、`RandomFloat01`、
`GetCurrentSeed()`（Full Vision），广播 `OnDiceRolled`。骰子点数由种子化 RNG 裁定（非 Chaos 物理，
物理仅做表现）。`RollDice` 执行序铁律：流抽→固化→广播→返回同一固化值（同步返回值 == 事件 payload）。

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0004: 确定性 RNG | 单权威 `FRandomStream`（DiceService 持）+ 各系统独立非权威流；退化安全（Min==Max early-return、Min>Max ensure 不交换）；Range < 2^24 | **MEDIUM**（`FRandomStream::RandRange` 是否仍走 FRand() 浮点中介、默认种子是否恒 0、是否新增官方确定性 RNG 子系统待验证）|
| ADR-0003: 事件总线 | `OnDiceRolled(FDiceRollResult)` `DYNAMIC_MULTICAST_DELEGATE` + USTRUCT payload | LOW |
| ADR-0007: BP-C++ 边界 | RNG 落 C++ + `UFUNCTION(BlueprintCallable)` 封装；CI 禁全局 RNG 静态扫描硬门 | LOW |
| ADR-0001: 宿主 | DiceService 与 player-turn 同生命周期层 | LOW |
| ADR-0005: 存档 | 当前骰序列化完整 `FDiceRollResult`；MVP 不存 Seed | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-dice-001 | DiceService 持唯一权威 `FRandomStream`，禁旁路引擎全局 RNG | ADR-0004 ✅ |
| TR-dice-002 | 种子初始化 `SetSeed` + 确定性（同种子+同抽取序列→同输出）| ADR-0004 ✅ |
| TR-dice-003 | `RandomRange` 退化契约（Min==Max 早退、Min>Max ensure 不交换）| ADR-0004 ✅ |
| TR-dice-004 | 跨平台浮点位级一致风险 → Full Vision 重放 desync 处置 | ADR-0004 ✅ |
| TR-dice-005 | `OnDiceRolled` 事件契约（DYNAMIC_MULTICAST + BlueprintAssignable）| ADR-0003 ✅ |
| TR-dice-006 | 广播/返回同源顺序（固化→广播→返回同值）| ADR-0003 ⚠ Partial |
| TR-dice-007 | 禁单线程重入（订阅者纯呈现，不在回调内二次抽取）| ADR-0004/0003 ⚠ Partial |
| TR-dice-008 | DiceService UE 宿主 + Seed 注入点（与 player-turn 同层）| ADR-0001 ✅ |
| TR-dice-009 | `FDiceRollResult` USTRUCT(BlueprintType) + UPROPERTY BlueprintReadOnly | ADR-0003/0007 ✅ |
| TR-dice-010 | RNG 原语须 C++ 封装为 `UFUNCTION(BlueprintCallable)` | ADR-0007 ✅ |
| TR-dice-011 | 确定性权威逻辑落 C++ + 禁全局 RNG 静态符号扫描硬门 | ADR-0007 ✅ |
| TR-dice-012 | 当前回合骰结果序列化完整 `FDiceRollResult`（读档不重掷）| ADR-0005 ✅ |
| TR-dice-013 | MVP 不序列化 Seed；Full Vision 经 GetCurrentSeed/SetSeed 显式存取 | ADR-0005/0004 ✅ |
| TR-dice-014 | lazy-init 兜底种子取非确定源（FPlatformTime::Cycles），禁默认构造 | ADR-0004 ⚠ Partial |
| TR-dice-015 | 通用原语 RandomRange/RandomFloat01 供 AI/Alpha 走权威流，juice 走独立流 | ADR-0004 ✅ |
| TR-dice-016 | 多线程并发抽取禁止：后台线程用独立流，不共享权威流 | ADR-0004 ✅ |
| TR-dice-017 | F-5 确定性种子序列 fixture（≥20 抽取逐字段精确相等）PR-gate headless | ADR-0004/0007 ⚠ Partial |

**Untraced Requirements**: None（17/17 有 ADR 覆盖；Partial = 跨 ADR 协同或测试前提，非 Gap）

## Definition of Done

This epic is complete when:
- All stories implemented, reviewed, closed via `/story-done`
- All acceptance criteria from `design/gdd/dice.md` verified
- F-5 确定性种子序列 fixture（≥20 抽取逐字段精确相等）headless 通过于 `tests/unit/dice/`
- CI 禁全局 RNG 静态扫描硬门生效（grep `FMath::Rand`/`FMath::RandRange`/`rand(`，白名单 `UDiceRngService` 内 `FRandomStream`）
- **Sprint0 引擎验证 ADR-0004 #4**：`FRandomStream::RandRange(int,int)` 是否仍走 `RandHelper` 浮点中介 + 默认构造种子是否恒 0 + 是否新增官方确定性/网络 RNG 子系统

## Next Step

Run `/create-stories dice-rng` to break this epic into implementable stories.
