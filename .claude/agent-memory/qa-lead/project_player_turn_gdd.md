---
name: project-player-turn-gdd
description: 玩家与回合管理 GDD (player-turn.md) AC 校验结果 — 10个BLOCKING缺口，2个标签错误，草案AC数25→35
metadata:
  type: project
---

玩家与回合管理 GDD 完成首轮对抗性 AC 校验（2026-06-01，QA Lead 执行）。
GDD 本身的 Acceptance Criteria 节标注"To be designed"，本轮审查针对主会话草案 AC-1~25。

## BLOCKING 缺口（10个）

- GAP-1 [Logic]: CR-3 在狱分支路由无 AC → 新增 AC-8
- GAP-2 [Logic]: ConsecutiveDoubles 行动权切换时归零无 AC → 新增 AC-17
- GAP-3 [Logic]: CR-5 在狱不跳过（在狱仍获回合）无 AC → 新增 AC-25
- GAP-4 [Logic]: Edge Case "双点出狱不进双点链" 无 AC → 新增 AC-15
- GAP-5 [Logic]: F-1 cur∉A 防御无 AC → 新增 AC-24
- GAP-6 [Logic]: F-2 APC==0 assert 无 AC → 新增 AC-28
- GAP-7 [Logic]: F-3 非双点归零无 AC → 新增 AC-12
- GAP-8 [Logic]: F-3 >=防御（异常高值）无 AC → 新增 AC-13
- GAP-9 [Logic]: "双点后即破产无额外回合" 无 AC → 新增 AC-16
- GAP-10 [Advisory]: 跨系统责任转移（存档21/AI10/HUD16/事件格7）无 OQ 追踪/关闭条件 → 新增 OQ-T-1~4

## 标签错误（2个）

- LABEL-1: 草案 AC-4 全标 [Advisory]，阶段序列本身 fixture 可跑 → 拆分为 AC-7[Logic]+AC-34[Advisory]
- LABEL-2: 草案 AC-23 错标 [Logic]，需存档系统(21)参与 → 拆分为 AC-32a[Logic]+AC-32b[Advisory]

## 措辞问题（需改写）

- 草案 AC-5/AC-6 缺少关键断言（ConsecutiveDoubles 状态/TurnOrderIndex 切换/SendToJail 意图）
- 草案 AC-25 AI 超时缺少 GIVEN/WHEN/THEN

## 最终 AC 结构

- [Logic] 30条（AC-1~31 含 AC-32a），全部 fixture 可跑，可作 PR gate
- [Advisory] 5条（AC-2/32b/33/34/35），code review/集成/OQ 守门

## 跨系统 OQ（须在 GDD Open Questions 节建立）

- OQ-T-1: 事件格(7) GDD 须包含"双点出狱不进双点链"集成测试，回链本 GDD AC-15
- OQ-T-2: 存档(21) GDD 须包含"中途还原精确阶段+ConsecutiveDoubles"集成测试，回链本 GDD AC-32b
- OQ-T-3: AI(10) GDD 须包含"PostRollAction AI 超时自动 EndTurn"集成测试，回链本 GDD AC-33
- OQ-T-4: HUD(16) GDD 须包含"OnPhaseChanged 驱动 UI 更新"集成测试，回链本 GDD AC-34

**Why:** Foundation 系统 #2，10个 BLOCKING 缺口若不补全，PR gate 将放行有回归风险的回合逻辑；
跨系统责任真空重蹈 board-data BLOCK-2 教训。
**How to apply:** 实现开始前须补全全部 [Logic] AC；下游 GDD（事件格7/存档21/AI10/HUD16）
审查时 qa-lead 须主动核查 OQ-T-1~4 是否已履行回链承诺。关联记忆：[[project-board-data-gdd]]
