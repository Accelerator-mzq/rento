# 设计变更影响报告 — 地产所有权(6) Group B 传播

| 字段 | 值 |
|---|---|
| **源 GDD** | `design/gdd/property-ownership.md`(系统 #6) |
| **变更日期** | 2026-06-03 |
| **触发** | `/propagate-design-change`(producer 工单) |
| **源文档状态** | NEEDS REVISION,pending re-review(本传播为同批/推 Approved 前执行) |
| **执行人** | 主会话(claude) |

---

## 0. 前置:本项目无 ADR

`docs/architecture/` 下零 ADR(仅 `tr-registry.yaml`)。项目在设计阶段,architecture 未开。
`/propagate-design-change` 的标准「GDD→ADR」影响分析**无对象**——本次传播是 GDD→邻居 GDD
的**跨档接缝传播**(Group B),与项目 CD 流程约束(跨档接缝 single-doc 修订须同 PR grep 核实对端)一致。

## 1. 变更摘要

property-ownership.md 经 design-review full(2026-06-03)做出绑定裁定,产生 5 条跨 Approved 邻居的
**真矛盾**(Group B),要求 propagate 修正邻居本体:

- **事务归属**:买地/抵押/赎回事务归地产所有权(6),方向 **6→5**(6 调 economy `Debit`/`Credit`);
  economy **不反调 6**(解 5↔6 环,保 systems-index"无环"声明)。
- **house_count 归属**:归建房(8),6 不持(防 6↔8 环);6 快照 + 8 house_count 经回合2 `ResolvePhase` 聚合。
- **破产资产移交**:地产 owner in-kind 写归破产9 经所有权6/建房8;economy 只执行**现金侧**。
- **RentTable 档位查表**:归经济(5) F-3;6 只供 `station_count`(6 无 RentTable 访问权)。

## 2. 影响分析与处置(fresh-grep 重盘,不信旧登记行号)

> ⚠ **登记清单第三次漂移漏列**(项目已记录此模式):OQ-Prop-3 源文档登记 6 处,
> fresh-grep 实命中 **9 处**——新增 economy `104`/`265`/`470` 原漏列。

| 工单 | 错型 | 处置(命中文件:行) | 状态 |
|---|---|---|---|
| **OQ-Prop-2** | economy 反向调6 / F-11 越权写 owner / house_count 校验错置 | economy `60`(CR-4)/`63`/`64`(CR-5)/`104`(Interactions row6)/`70`/`87`/`106`/`118`/`241`(F-11)/`291`(house_count校验)/`441`(AC-30)/`442`(AC-31) | ✅ RESOLVED |
| **OQ-Prop-3** | 「house_count 由6供」(应归8) | economy `58`/`104`/`152`/`265`/`318`/`333`/`470` + systems-index `67` + entities `267`/`277` | ✅ RESOLVED(扩集 6→9) |
| **OQ-Prop-4** | index 缺 9→6 /(条件)16→6 | systems-index `28`(`2,5`→`2,5,6`) | ✅ 9→6 闭合;16→6 待 HUD GDD(用户裁定暂不补) |
| **OQ-Prop-6** | board 把 RentTable 档位错归6 | board-data `90`(拆两腿:count 归6、档位查表归经济5 F-3) | ✅ RESOLVED |
| **P5** | board row6 缺 TileType/GetTilesInGroup 且自相矛盾 | board-data `161`(Interactions)/`301`(下游依赖表)补 `TileType`+`GetTilesInGroup` | ✅ RESOLVED |

**合计 24 处编辑**,横跨 4 个邻居文件 + 源文档追踪表更新。

## 3. 不在本次传播范围(非 propagate 项)

- **OQ-Prop-1**(⚠ ADR):owner map 容器形态/受控写封装/UObject 宿主类型 — 架构决策,留
  `/architecture-decision`,下游 8/9 实现开工前,与 player-turn/board/economy/movement 同生命周期层 ADR 协调。
- **OQ-Prop-5**(🔴 BLOCKING):`is_monopoly` 抵押是否破坏垄断 ×2 — Rento Fortune 实测**事实核查**,
  非文档编辑,MVP 设计冻结前先于 AC-18 实现。

## 4. 验证证据(fresh-grep,0 残留)

- economy 反向「通知所有权(6)」:**0 命中** ✓
- board「选 RentTable…归所有权6」:**0 命中** ✓
- 全部 `house_count` 行已改归建房(8)✓
- 源文档 OQ-Prop-2/3/6 + P5 标 ✅ RESOLVED;OQ-Prop-4 标 PARTIAL(9→6 闭合,16→6 待 HUD)。

## 5. 已知残留(re-review pass 可折叠)

property-ownership.md 正文 CR-2/CR-3/CR-6 的 inline ⚠「真矛盾…须 propagate」rationale 注
仍为现在时措辞(作历史依据保留,已交叉引用到现 RESOLVED 的 OQ-Prop 表)。源文档 pending re-review,
其正文将在 re-review 时与 OQ 表一并 reconcile。**权威闭合状态以 OQ-Prop 表 + Status 行为准**。
