---
name: total-push-vs-param-seam
description: movement(4)→economy(5) dice Total contract is push-vs-parameter mismatch; economy F-4/AC-18 forbid caching pushed Total
metadata:
  type: project
---

movement(4) CR-3.1 frames the dice `Total` handoff to economy(5) as a **push/store-and-forward** ("移动 push Total, 经济在 ResolvePhase 算租时消费"). Economy F-4 + AC-18 define `dice_total` as a **resolution-time parameter that is explicitly NOT cached** (AC-18 is a [Logic] PR gate asserting no cached read). These two models are incompatible — implementing movement's push model breaks economy AC-18.

**Why:** Surfaced in movement re-review 2026-06-02 (economy-boundary adversarial pass). The salary push `(passedGo, SalaryAmount)` half of the contract IS consistent (gate `>0` both sides, closed). Only the `Total` half mismatches. Prior review's blocker #5 registered the obligation row (systems-index L66) but encoded movement's wrong push model, so registration ≠ consistency.

**How to apply:** The correct owner of "supply dice_total to F-4 at resolution time" is 回合(2)/ResolvePhase pulling the value, not movement pushing-and-storing. The OQ-T-Econ-3 event extra-roll ("前进到最近公用须额外掷骰") is a second writer to the same logical slot — economy F-4 must treat the event-supplied dice as the parameter when present; there must be a single-source rule, not a stored Total. When reviewing any system that claims to "push Total to economy," check it against economy AC-18 (no-cache). See [[obligation-registered-not-consistent]].

Also noted: economy F-4's `DiceMultiplierTable` multiplier has NO upper-bound / load-time guard (only the index clamp). OQ-Econ-10 bounds `SalaryAmount` but not the Utility multiplier — a real second unguarded int32 overflow, tracked as the economy "F-4 clamp" followup on systems-index L47.
