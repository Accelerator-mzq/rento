---
name: project-dice-gdd-review
description: Dice GDD (骰子大亨 / Rento Fortune clone) is an S-level zero-dependency pure RNG service; key settled calls and the live cross-doc gap
metadata:
  type: project
---

Dice GDD (`design/gdd/dice.md`, systems-index #3) is a zero-dependency Foundation pure-RNG service for 骰子大亨 (a Rento Fortune / Monopoly clone, UE5.7 Blueprint-primary). It claims NO independent player fantasy — only an invisible "fairness/trust floor" serving Pillar ③ (luck×strategy).

**Settled call (R2, do not re-litigate):** The signature 掷骰爽感 (dice-roll juice, Sensation = game-concept priority-4, start of the 30-second core loop) is owned end-to-end by VFX(19) GDD as a MUST-FULFILL handoff (OQ-T-Dice-4). game-designer (me) argued in R2 that this doc should carry presentation-layer hooks/AC; CD + qa + unreal overruled (forcing presentation AC into the RNG layer = layer violation). User adopted CD's PASS. The dice GDD's obligation boundary: authoritative result + `OnDiceRolled` event + result produced before animation starts.

**Why:** Forcing experiential AC back into a pure-RNG service is a layer violation; the carrier exists, just not in this doc.
**How to apply:** Do not re-open the VFX19 owner assignment unless a genuinely NEW gap appears (e.g., VFX19 GDD never gets written / qa-lead has no checklist to enforce the MUST-FULFILL at VFX19 review time).

**Live cross-doc gap (as of 2026-06-02, R3):** Dice B1 requires player-turn to serialize the full `FDiceRollResult` (Die1/Die2), but player-turn AC-34 AND its 存档(21) Dependency row STILL say `(点数,bIsDouble)` — propagate NOT executed. Dice GDD honestly marks this `propagate 状态: PENDING (实现前必须完成)`. This is a real downstream blocker for IMPLEMENTATION, owned by producer coordination, but the dice GDD itself has discharged its duty by flagging it. See [[feedback-review-failure-modes]].

Pillar priorities (game-concept MDA): Fellowship=1, Submission=2, Challenge=3, Sensation=4, Fantasy=5, Expression=6. Design Risks: "运气占比过高让偏策略玩家挫败" + "后期翻盘无望拖沓" — these motivate OQ-5 (anti-tilt variance compression).
