---
name: cascade-spec-update-variable-table
description: 公式级联扩展（如新增联合现金门）引入新操作数时必须同步更新变量表，否则规格不完整
metadata:
  type: feedback
---

当一条公式（如 F-4 贪心循环）通过"级联规格化"扩展出新计算路径（如赎回-解锁-建房联合现金门），新路径引入的操作数（`unmortgage_cost(A)`、`BuildingCost(首栋)`）必须补进该公式的变量表。

**Why:** ai-opponent R-3 修订将 F-4 的赎回-解锁级联从散文升为规格，但变量表只覆盖核心 BuildScore 公式的变量，未列联合现金门所需的新操作数，违反 Design Document Standards 第 4 节 Formula 要求。

**How to apply:** 审核"规格化"类修订时，逐行对照新公式/谓词中出现的每个符号，确认变量表有对应行；新引入但未列表的符号=规格缺口，标 BLOCKING。
