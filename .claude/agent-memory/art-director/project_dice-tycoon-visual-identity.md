---
name: project-dice-tycoon-visual-identity
description: 骰子大亨核心视觉定位、材质管线裁定状态与关键跨档接缝
metadata:
  type: project
---

骰子大亨(Rento 复刻)视觉身份: 卡通 2.5D 棋盘, 明亮玩具感, UE5.7, PC/Steam。

**Why:** art-bible v0.1 是 Technical Setup→Pre-Production gate 必检档案，需掌握当前视觉定位以快速审核后续资产。

**How to apply:** 审核任何视觉资产或设计文档时以 art-bible §1-9 为权威源。

## 材质管线裁定(ADR-0009, Accepted 2026-06-06)
- MVP: Legacy Unlit + 自定义节点 + Inverted Hull 描边
- Substrate: 显式禁用; Alpha 阶段 POC 评估
- 理由: Substrate Unlit 节点存在性是 post-cutoff 知识盲区 + indie 团队 + 无 PBR 收益

## 关键跨档接缝(art-director 需跟进)
- **OQ-A11Y-5**: art-bible §7.2 须落具体等宽数字字体资产名(HUD CR-3 + property-card V-Own-04 propagate 债)——Pre-Production asset-spec 前必须闭合
- **F-3 propagate**: HUD CR-3/art-bible §7.5 高亮曲线须从 Ease-Out Bounce 改为 Ease-Out Cubic(R-2 已裁定，art-bible §7.5 动效表尚未同步更新)
- **Substrate Alpha go/no-go**: 须将"Substrate Unlit 节点存在性"列为 Alpha 评估前置 go/no-go(ADR-0009 S-4)
