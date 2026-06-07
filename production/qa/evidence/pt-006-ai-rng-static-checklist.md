# pt-006 AC-5 — AI RNG 走骰子3 权威流 静态约束 checklist（Advisory）

- **Story**: production/epics/player-turn/story-006-dicerollresult-consumption-ai-rng.md
- **AC**: AC-5（TR-turn-010，GDD CR-8 确定性 + OQ-T-3②）
- **Gate Level**: ADVISORY（BP-only 软约束，code-review 守；C++ 强封装静态硬门 deferred 待 OQ-1 ADR）
- **Date**: 2026-06-08
- **Verified by**: 主会话独立 grep（human-on-halt，非 agent 自报）

## 约束（control-manifest Foundation Layer / ADR-0004）

全部需重放的 gameplay 随机（骰点/牌堆洗牌/AI 决策扰动）必须走唯一权威
`UDiceRngService` 的 `RandomRange`/`RandomFloat01`；**禁**旁路引擎全局 RNG
（`FMath::Rand` / `FMath::RandRange` / `FMath::FRand` / `rand(` / `std::rand`）。
表现层 juice 随机各持独立非权威流（不复用权威流）。

## pt-006 编排路径随机审查

pt-006 新增/改动的权威 C++ 编排路径：
- `UPlayerTurnSubsystem::ConsumeRollResult`
- `UPlayerTurnSubsystem::GetCurrentRollContext` / `GetCurrentRollTotal`
- `UPlayerTurnSubsystem::OrchestrateMove` / `HandlePawnLanded` / `ResolveArrival`
- `ILandingResolver` / `IPawnMover`（DI seam 签名，本 story 不含 AI 决策算法）

**结论**：pt-006 编排路径**不含任何随机调用**——本 story 是 holder 消费 + 落地路由 +
程间编排 seam，AI 决策算法（F-3 Epsilon / tiebreak 扰动）归 ai epic。
未来 AI 在本系统 hook 内的随机扰动须经注入的 `UDiceRngService::RandomRange/RandomFloat01`。

## grep 静态检查命令与结果

对权威模块 `Source/rento/`（AI/经济/建房/破产/回合/RNG）grep 禁用符号：

```
$ grep -rnE 'FMath::(Rand|RandRange|FRand)|[^A-Za-z_]rand\(|std::rand' Source/rento/
（白名单：仅 UDiceRngService 内 FRandomStream 调用）
```

**结果**：pt-006 改动文件（PlayerTurnSubsystem.cpp/.h、PlayerTurnTypes.h、
RentoPlayerState.h、LandingResolverInterface.h、PawnMovementInterface.h）
**零命中**禁用符号。权威随机仍唯一收敛于 `UDiceRngService::AuthoritativeStream`。

## code-review checklist

- [x] pt-006 编排路径无 `FMath::Rand` / `FMath::FRand` / `rand(` / `std::rand`
- [x] DI seam（ILandingResolver/IPawnMover）签名不含随机、不绕过 DiceService
- [x] AI 决策算法不在本 story（归 ai epic）；本 story 不引入未经权威流的随机
- [x] C++ 强封装静态硬门（AI 模块禁用引擎 RNG 符号扫描）deferred 待 OQ-1 ADR——
      本 story **不**写永远 pending 的 [Logic] gate（story AC-5 明确）

## OQ 登记

- **OQ-T-3②**：AI 内部随机强度（BP 软约束 vs C++ 静态硬门）待 OQ-1 ADR 裁定。
- **OQ-3**：Full Vision 重放模式才真需要严格重放；MVP AI 简单，软约束足够。
