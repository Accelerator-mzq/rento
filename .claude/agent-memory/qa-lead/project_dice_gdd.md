---
name: project-dice-gdd
description: 骰子 GDD 审查史 R1→R3——R3 散文收敛,唯一 BLOCKING 落 systems-index(5个OQ-T-Dice未登记继承义务表);propagate 未执行=实现阻塞
metadata:
  type: project
---

骰子 GDD (`design/gdd/dice.md`,systems-index #3,S 级零依赖纯 RNG 服务) 审查史。28 条 AC(扩自原 23),核心是 [Logic]/[Advisory] 分层是否真干净。

**Why:** 本项目 player-turn 经 8 轮复审,核心失效模式是"统计/频率断言被误当 [Logic] PR gate → CI flaky"+"修法落注释不落规格"。骰子比 player-turn 严谨。详见 [[gdd-fix-lands-in-comments-not-spec]] / [[gating-seam-keeps-peeling]]。

**How to apply:** R3 散文已收敛、design-APPROVABLE。下次审查时核对下方 BLOCKING 是否落地。骰子的 AC 分层是本项目的正面范本——勿无端推翻。

## R3 结论 (2026-06-02, qa-lead 对抗式)
散文收敛,AC 套件可测且分层正确(20 [Logic] 确定性 / 8 [Advisory] 统计,28 条 inline 标签与 L289 汇总全对齐,20/8 计数正确)。**M3 缺陷(概率断言误标 [Logic])不复发**——AC-9/AC-17b 均确定性单点断言(AC-17b = `InitialSeed != 0`)。"每次结果∈[Min,Max]"是确定性不变式(首个违例即失败),非统计——AC-1/2/7/12b/12c/13 均正确归此类;AC-12 边界可达性(真种子依赖)正确留 [Advisory]。

**唯一 BLOCKING (NEW,落 systems-index 非 dice 散文):** dice 的 5 个 OQ-T-Dice 移交未登记进 systems-index 继承义务表(L53-58)。该表全部行源自 `player-turn OQ-T-*`,无一条 dice 来源——正是该表(L51,board-data 责任真空教训)要防的失效模式下沉一层。尤其:存档(21)行仍写 `(点数,bIsDouble)`(M2 刚清掉的同款残留,索引是最后存活处),与 OQ-T-Dice-5 直接矛盾;OQ-T-Dice-4(CD 钦定 MUST-FULFILL 掷骰爽感 owner)若不在 VFX19 行,审 VFX19 的 qa-lead 看不到。修法:补/改 5 行,单 artifact 收口,不动 dice 文档=不构成剥面。

**M1 精确裁定:** OQ-T-Dice-5(player-turn AC-34 须从 `(点数,bIsDouble)` 扩为完整 `FDiceRollResult`)——经独立核 player-turn.md 确认 propagate **未执行**(AC-34 L471 + AC-33 L468 + 5 处契约行 L110/180/195/369 仍旧契约)。裁定:**不阻塞 dice 设计文档**(PENDING 标记是本层正确且充分动作,实际 /propagate 归 producer);**阻塞 dice 实现**(B1 序列化链依赖 player-turn 持 FDiceRollResult,现未兑现)。design-approvable / implementation-blocked。

**3 个 R2-NICE 均不升 BLOCKING:** (a) AC-8 fixture oracle 自生成 = RNG API 稳定回归门的正确范式,非 test-validity 洞,留;(c) AC-21 缺 GENERATED_BODY() 编译门必捕获,留;(b) AC-16 "恰滚动两步"措辞不可经 GetCurrentSeed 直接执行(LCG Seed 非步计数器),但 AC-16 是 Advisory 且意图已被 AC-10/AC-6 确定性覆盖 → RECOMMENDED 措辞清理,不阻塞。

**收敛判断:** 未触发结构性不收敛信号。dice 散文无新接缝;唯一 BLOCKING 是 B1 根的下游登记收尾(~5 行 systems-index 编辑),非级联剥面。

## 历史 (R1/R2 已闭合,留作追踪)
R1 (6 blocking+2 qa): B1 序列化丢 Die1/Die2 / B2 卡方数值 / B3 缺 RandomRange 范围不变式 AC / B4 lazy-init 默认种子=0 / B5 掷骰爽感无 owner / B6 定序双点反馈矛盾 / D-1 AC-9 概率 / D-2 AC-6 前提。
R2 (4 must-fix): M1 OQ-T-Dice-5 propagate 悬挂(标 PENDING)/ M2 L65/L200 契约表 (点数,bIsDouble) 残留 / M3 AC-17b 概率断言误标 [Logic]→改 InitialSeed!=0 / M4 F-2 溢出约束 <INT32_MAX 改 <2²⁴。四专家独立重算卡方全 PASS(df=1/10.83、df=10/29.59、df=5/20.52、df=25/52.62)。
