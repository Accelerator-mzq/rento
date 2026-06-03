---
name: project-board-data-gdd
description: Board/Tile Data GDD (board-data.md) 三轮对抗性审查结果 — 第三轮新增 2 个 BLOCKING（AC-37标注根本性错误、跨系统责任转移真空）
metadata:
  type: project
---

Board/Tile Data GDD 完成三轮对抗性审查（QA Lead 执行，最后一轮 2026-06-01）。

## 第一/二轮已修复（R2 版本中已应用）

前两轮识别的 4 个 BLOCKING（AC-24拆分、AC-32改标 [Logic]、新增 AC-34、AC-25改 Advisory code review）均已在 R2 应用。AC-34/35/36/37/38/39 为 R2 新增。

## 第三轮新增问题

### BLOCKING（2个）

**BLOCK-1: AC-37 测试分层标注根本性错误**
AC-37 第二部分"断言不存在独立的公开 PassedGoCount 接口"标注为 [Logic] BLOCKING，但"某函数不存在"这一断言在任何自动化测试框架中物理上不可执行。应拆分：
- AC-37a [Logic]：断言 AdvanceIndex 返回元组两分量均正确
- AC-37b [Advisory]：code review 守门，确认头文件不暴露独立接口

**BLOCK-2: 跨系统 BLOCKING 责任转移无追踪机制**
AC-38（归经济5）、AC-39（归事件格7）、AC-28b（归存档21）的责任转移是软承诺：下游 GDD 均尚未成形，无关闭条件、无回链要求、无门控。GDD 声称"避免责任真空"但无执行机制。需在每个责任转移 AC 处增加"关闭条件：下游 GDD 中必须存在回链本 AC 编号的等价 BLOCKING，qa-lead 审查时确认"，或在 Open Questions 建立对应 OQ 追踪条目。

### RECOMMENDED（4个）

- **REC-1: AC-34 THEN 缺返回值断言**：只断言 ensure 和日志，未断言越界时仍返回通用式数学正确结果。
- **REC-2: AC-27 THEN 缺返回值规格**：应明确返回零值 FBoardTileData{}。
- **REC-3: AC-39 非法测试分层标注**：`[Advisory→Integration]` 不是合法标注，应改为 `[Advisory]`。
- **REC-4: [Config] nightly gate 缺 devops sign-off 追踪要求**：传递给 devops-engineer 的行为本身无可追踪确认机制。

### 通过项

Edge Case 到 AC 覆盖全表检查：39 条规则全部有对应 AC，无遗漏。数学验算 AC-33/15/29/39 全部通过。

**Why:** Foundation 系统 #1，BLOCK-1 直接导致 CI gate 不可执行，BLOCK-2 导致发薪/传送/存档三个跨系统测试在项目生命周期内可能静默缺失。
**How to apply:** 实现开始前须修复两个 BLOCKING；下游 GDD（经济5、事件格7、存档21）审查时 qa-lead 须主动核查本 GDD 的责任转移承诺是否已履行。
