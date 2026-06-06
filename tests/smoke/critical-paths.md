# Smoke Test: Critical Paths

**Purpose**: 15 分钟内跑完这些检查，作 QA 交接前的关键路径门。
**Run via**: `/smoke-check`（读取本文件）
**Update**: 每实现一个核心系统就更新对应条目。

## Core Stability（始终跑）

1. 游戏启动到主菜单无崩溃
2. 主菜单可开新局（本地热座 2–8 玩家 + AI 难度配置）
3. 主菜单响应全部输入无卡死（Enhanced Input，ADR-0011）

## Core Mechanic（按 sprint 更新）

<!-- 每 sprint 实现的主机制加在这里 -->
4. [掷骰→移动→落地结算 核心循环——首个核心系统实现后更新]
5. [买地/收租——地产6/经济5 实现后更新]
6. [建房/租金升级——建房8 实现后更新]
7. [破产判定/胜负——破产9 实现后更新]
8. [AI 对手一回合自动决策不卡流程——AI10 实现后更新]

## Data Integrity

9. 存档完成无错误（存档21 实现后）
10. 读档还原正确状态——精确阶段 + 骰面 + 牌堆顺序（存档21 + ADR-0005 实现后）

## Performance

11. 目标硬件无可见掉帧（60fps 目标）
12. 5 分钟连续游玩无内存增长（核心循环实现后）

## Engine Verification（Sprint 0 blocking — /architecture-review CONCERNS）

13. **ADR-0012 CommonUI 插件 5.7 是否 Experimental**——若是须显式启用，否则 Shipping cook 全 UI 失效（specialist finding S-3，Sprint 0 首日核验）
14. **ADR-0008 `UUserWidget::NativeTick` 在 `-nullrhi` 是否触发**——决定 HUD 计时器 AC 是 [Logic] 还是 [Advisory]（ADR-0008 Verification Required ①）
