---
name: feedback-art-bible-open-obligations
description: art-bible v0.1 中已知但尚未落地的 propagate 债与视觉规格缺口
metadata:
  type: feedback
---

art-bible v0.1 有 2 条跨档 propagate 债尚未在 art-bible 正文中闭合:

**Why:** HUD(hud.md)和 property-card-ui 多轮 design-review 中均标注这些债归 art-director 闭合, 若 Pre-Production 开工仍未落, WBP TextBlock 无可绑字体、动效规格与 GDD 不一致。

**How to apply:** 每次被召唤处理 art-bible 修订时，优先检查这 2 条是否已落:

1. **等宽数字字体资产名(OQ-A11Y-5)**: §7.2 "Tabular Figures 等宽字体" 须替换为具体字体资产名(如 "Roboto Mono" 或等效), 否则 WBP TextBlock 无可绑字体。UE5.7 FSlateFontInfo 是否暴露 tnum API 须架构期核验; 保守方案=选字形本身等宽的字体资产。
2. **高亮曲线纠偏(§7.5 表格)**: §7.5 "面板弹出/收起"行仍写 "Ease-Out Bounce 或 Ease-Out Back"。HUD R-2 已裁定面板高亮须改为 Ease-Out Cubic(去超冲), Bounce 归 VFX juice。art-bible §7.5 正文应同步: 面板高亮过渡=Ease-Out Cubic; 弹性/超冲效果仅由 VFX(19)拥有。
