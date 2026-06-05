---
name: feedback-fantasy-beat-gated-on-unmet-dep
description: Adversarial Fantasy-review test — a Fantasy-pledged/Design-Test-anointed juice gated behind an unmet cross-doc dep with a zero-floor degraded fallback is BLOCKING, not a followup
metadata:
  type: feedback
---

When adversarially reviewing a presentation/juice GDD against its Player Fantasy, check whether any Fantasy-pledged beat is **gated behind an unmet cross-doc dependency** AND whether its **degraded fallback has a felt floor**.

**The trap (seen in vfx-feedback #19 R-2, B-1):** CR-6 building juice depended on `EBuildDirection` from building-upgrade's `OnBuildingChanged`. Fresh-grep proved the owner still broadcasts the 2-arg `(tile,newCount)` — dep unmet (OQ-VFX-10 open). The degraded fallback (EC-15) was a **zero-celebration plain snap** — yet "房子咔哒落地" is a Fantasy beat AND art-bible Design Test explicitly anoints building-landing as "preserve-first when budget-tight." So the doc contradicted itself: a juice it pledged to cut LAST became the FIRST thing absent in the fallback.

**Why it's BLOCKING not followup:** This is not "feature deferred" (degraded-but-present) — it's "feature absent." Compare a sibling beat that degraded gracefully (hop kept a compressed-min form); the anointed beat kept NO min form. Inconsistent treatment + the doc's own "don't accept registered-OQ as closure" rule = cannot ship on a registered OQ.

**The fix pattern that usually unblocks (recommended over freezing):** the consumer can often **locally derive** the missing discriminator (e.g. infer Build vs Sell from `newCount` monotonicity vs a cached prior value) so the degraded state plays a *compressed* celebration instead of *zero*, keeping the beat always-present. The authoritative field later overrides the local inference. Mirrors [[caller-injected-param-resolves-overreach-and-freeze]]: local derivation > freezing on an unmet dep.

**Companion test — compressed/protected states need a felt floor, not just a logic floor.** A protection AC that only asserts "never teleport-snaps" (logic) without an experiential sign-off that the *compressed* form still delivers the Fantasy (e.g. "看着棋子逼近命运" at N≥20 fast-hotseat) leaves a tunable (T_hop_protect) with no Fantasy anchor — it can be tuned to "technically non-snap but perceptually a smear" and never FAIL. Same shape as [[ac-tune-until-pass-escape-clause-is-tautology]]. Route the existing playtest-signoff AC to cover the compressed state explicitly.

**How to apply:** distinct from [[feedback-review-failure-modes]] (which is about convergence discipline). This is an *adversarial Fantasy-anchored* test: (1) trace each Fantasy beat to its trigger event and check the owner-side contract by fresh-grep; (2) if gated on an unmet dep, read the degraded fallback and ask "does the pledged beat survive degradation, or vanish?"; (3) vanishing + Design-Test-anointed = BLOCKING; (4) prefer local-derivation fixes that keep the beat present-but-compressed over freezing the approve on the dependency's ticket.
