---
name: r2-blocker-stub-field-gap
description: AI对手(#10) R2四条BLOCKING均经fresh-grep确认真实存在于文档——AC-5/6/7 CI-stub GIVEN字段与CR-6字段清单不一致是"修复了散文层但未到test-spec层"的标准模式
metadata:
  type: feedback
---

AI对手 R2 四条 BLOCKING 经 R3 fresh-grep 逐条核实，全部仍然存在于文档中，未被修复。

**Why:** R1→R2 修复仅改了 CR-6 散文层声明（Rent_top1/top2/owner map 字段），但 AC-5/6/7 的 GIVEN fixture 字段列表未同步更新——典型"fix lands in comments not spec"失败模式。

**How to apply:** 对任何 CI-stub AC，验证其 GIVEN 字段列表是否与同文档的 CR/字段清单完全对齐；散文层修了但 AC GIVEN 没跟 = 仍是 BLOCKING。

具体四条：
1. Buffer_max clamp 复发：Sharp 1800 在中后期 MaxRentExposure>1200 仍被 clamp 钉死，档差语义丧失（F-7 L229 可计算证伪）
2. AC-5/6/7 GIVEN 字段缺 Rent_top1/Rent_top2（三 hook 全缺）、AC-5 缺 starting_cash、AC-7 缺 owner_map/board_tile_count_classic
3. AC-61 未拆 a/b——spy 在 VFX/HUD 未实现时 trivially pass（vacuous-pass 模式）
4. F-3 无补全格豁免分支（AC-60b 要求与 F-3 公式矛盾）；F-4 赎回级联规格不完整（联合现金门未定义）
