---
name: obligation-registered-not-consistent
description: a registered cross-system obligation row can faithfully encode an upstream system's WRONG model; registration != bidirectional consistency
metadata:
  type: feedback
---

When verifying a "prior blocker closed" claim about a cross-system contract, do NOT accept "the obligation row was added to systems-index" as proof the contract is consistent. The inherited-obligations table can faithfully transcribe an *upstream system's incorrect model* of how the downstream system works.

**Why:** In movement(4) re-review 2026-06-02, prior blocker #5 was "closed" by adding an economy(5) obligation row. The row was present — but it encoded movement's push-Total model, which directly contradicts economy's own F-4/AC-18 (no-cache parameter). A future implementer reading the row would build the wrong thing. "Registered" looked closed; "consistent" was not. Mirrors the project's standing pattern that self-consistent invariants are tautologies (see auto-memory invariant-self-consistent-is-tautology).

**How to apply:** For any "system A pushes X, system B receives X" closure claim, diff the *actual formula/AC text* on BOTH sides per-payload (split multi-field payloads — here salary half was consistent, Total half was not). Verify the receiving system's GDD actually consumes X the way the sender describes, not just that a row mentioning X exists. See [[total-push-vs-param-seam]].
