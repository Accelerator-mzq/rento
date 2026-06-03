---
name: feedback-review-failure-modes
description: This project's two named GDD-review failure modes and the convergence discipline expected at re-review
metadata:
  type: feedback
---

This project has two documented GDD-review failure modes that re-reviews must guard against — but without manufacturing trivial blockers.

1. **"Peeling seam" (剥面) structural non-convergence** — player-turn took 8 rounds because each re-review peeled a new sub-face off a gating seam instead of converging. If a re-review keeps surfacing NEW seams round after round, that itself is a non-convergence signal and the call may be to RETREAT to a softer presentation-layer convention rather than keep tightening. See user auto-memory [[gating-seam-keeps-peeling]].

2. **"Fix lands in comments not spec" (修法落注释不落规格)** — a fix written into prose/comments but not into the load-bearing spec surface (formula / AC / contract table) is NOT closed. Re-review must verify fixes char-by-char on the spec surface, not accept a comment's self-claim of closure. See user auto-memory [[gdd-fix-lands-in-comments-not-spec]].

**Why:** Both cost real rounds (player-turn x8). R2 reviewers explicitly warned R3 must converge.
**How to apply:** At re-review, (a) convergence is a VALID outcome — do not invent BLOCKING findings to look thorough; (b) tag each finding [NEW / R1-R2-residual / already-an-OQ] and [BLOCKING/RECOMMENDED/NICE]; (c) if the only remaining issues are already-registered OQs or cross-doc items the doc itself flagged, the doc has discharged its duty — say APPROVED/CONVERGED. Distinguish "the doc is wrong" from "a dependency the doc correctly flagged is unmet."
