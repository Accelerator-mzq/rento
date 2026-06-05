---
name: vfx-perf-review
description: VFX Feedback (#19) GDD performance review history and open obligations for performance-analyst
metadata:
  type: project
---

VFX Feedback GDD (#19) completed R-7 fresh adversarial performance review on 2026-06-06. Result: **APPROVED-with-followups (performance axis, no new BLOCKING)**.

**Why:** Seven rounds of review. R-7 is the convergence-confirm fresh review after R-6 NEEDS REVISION (pawn registry phantom-spy + UMG pool mechanism). Performance axis: all performance debt verified as correctly externalized to OQ-VFX-3 Pre-Production calibration + AC-21 hard gate. No design-layer uncovered gap.

**How to apply:** In future sessions, recall that:

## R-7 findings (performance axis)

### BLOCKING: 0

All four R-7 focus areas verified clean against source:
1. **Cascade 2N + three-pool isolation**: F-4 body (L206-221) has N_active_general, N_active_hop, N_active_umg all as separate count gates. EC-3/EC-14/EC-18 cover eviction paths. AC-61 covers UMG overflow [Logic].
2. **N_active_umg pool (R-6 fix landed)**: F-4 L221 body, EC-18, AC-61, AC-21 three-pool invariant enumeration — all verified present.
3. **PP stack + CPU/GPU split**: OQ-VFX-3 lists three-category breakdown (Niagara + UMG overdraw + PP passes), P99 ≤33ms gate, game-thread CPU separate column, cascade frame CPU ≤Xms placeholder. All verified at L540.
4. **Cold-start spike**: OQ-VFX-3 warm vs cold-start declaration verified at L540.

### Deflate record — R-7 confirmed not re-raiseable

- "N_confetti_max not in AC-21 upgrade-closure": N_confetti_max is Niagara internal particle density cap (not a system-level active-instance scheduler), covered by OQ-VFX-3 hardest-frame calibration scenario. No fourth invariant AC needed.
- All R-5 deflated items remain closed (see R-5/R-6 entries below).

## R-5 deflated items — do NOT re-raise

These were verified against source docs in R-5 and remain closed:

- "3N/4N cascade via OnCashChanged" — bankruptcy AC-17: in-kind has no cash event per tile. Closed.
- "chain bankruptcy 4N events" — OQ-VFX-3 R-5 ① adds chain scenario to calibration set; OQ scope, not design-review BLOCKING.
- "CPU split missing" — OQ-VFX-3 R-5 ② adds game-thread CPU gate. OQ scope.
- "UMG unbounded" — N_max_umg name and placeholder added; behavior definition still missing (see current BLOCKING above).

## Confirmed OQ-VFX-3 items that are fully landed (R-3/R-4/R-5)

All verified against L527 in R-6:
- 2N worst-case event model (ownership N + building N)
- "last player bankrupt + OnGameWon same frame" hardest frame scenario
- P99 ≤33ms gate (not just 120-frame average)
- Three-category frame budget breakdown: (a) Niagara instances + (b) UMG overdraw + (c) PP passes
- PP stack sub-budget single-listed, OQ-VFX-6 art-director decision node
- N_max_umg soft cap placeholder in Tuning Knobs + OQ-VFX-3
- PP sub-budget provisional floor placeholder
- Chain bankruptcy as independent required calibration scenario
- game-thread CPU vs GPU separate measurement + cascade frame CPU ≤Xms placeholder gate
- Pool prewarming (warm vs cold-start) required

## Persistent obligations for performance-analyst

- Named owner of OQ-VFX-3 (Pre-Production hardware calibration)
- At calibration: must add [Logic] invariant ACs: N_active_general≤N_max, N_active_hop≤N_hop_max, N_active_umg≤N_max_umg (last one pending BLOCKING fix landing)
- AC-21 hard gate: #19 stories cannot be marked Done before calibration
- Pool prewarming scenario required in calibration test suite
- game-thread CPU column must be measured separately from total frame time in cascade frames
