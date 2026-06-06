---
name: ac-count-row-misses-new-acs
description: 每轮 review 修订后合计行须枚举全部新增 AC，仅依赖注释中"新增 N 条"易漏计
metadata:
  type: feedback
---

AC 合计行散文里的"新增 N 条"注释不可信——ai-opponent R-3 修订新增了 AC-11b、AC-61a、AC-64、AC-65 共 4 条 Logic，但注释只写"含 AC-61a/AC-64/AC-65 新增 3 条"，漏掉 AC-11b，导致 Logic 声明 56 实际 57，BLOCKING 声明 64 实际 65。

**Why:** 修订 PR 由多条处方独立实施，不同处方新增的 AC 分布在不同段落，最后汇总合计行时容易只统计"有意识添加的"新 AC，遗漏在同一 PR 内由处方 #1 补的 AC。

**How to apply:** 复审公式/AC 文档时，独立逐条扫描所有 AC，自行计数后与合计行声明对比。若差值>0，寻找被遗漏计数的条目（通常是"字段补充类"AC，如比例守护、fixture 修复类条目）。
