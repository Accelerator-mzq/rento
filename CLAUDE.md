# Claude Code Game Studios -- Game Studio Agent Architecture

Indie game development managed through 49 coordinated Claude Code subagents.
Each agent owns a specific domain, enforcing separation of concerns and quality.

## Technology Stack

- **Engine**: Unreal Engine 5.7
- **Language**: Blueprint (primary — gameplay/UI), C++ (framework & performance-critical systems)
- **Version Control**: Git with trunk-based development
- **Build System**: Unreal Build Tool (UBT)
- **Asset Pipeline**: Unreal Content Pipeline

> **Note**: Engine-specialist agents exist for Godot, Unity, and Unreal with
> dedicated sub-specialists. Use the set matching your engine.

## Project Structure

@.claude/docs/directory-structure.md

## Engine Version Reference

@docs/engine-reference/unreal/VERSION.md

## Technical Preferences

@.claude/docs/technical-preferences.md

## Coordination Rules

@.claude/docs/coordination-rules.md

## Collaboration Protocol

This project runs in **one of two modes**. **Collaborative Mode is the default**;
Automated Mode is opt-in per phase.

### 🤝 Collaborative Mode (default)

**User-driven collaboration, not autonomous execution.**
Every task follows: **Question -> Options -> Decision -> Draft -> Approval**

- Agents MUST ask "May I write this to [filepath]?" before using Write/Edit tools
- Agents MUST show drafts or summaries before requesting approval
- Multi-file changes require explicit approval for the full changeset
- No commits without user instruction

### 🤖 Automated Mode (opt-in, Workflow-driven)

For convergent, standards-bound execution phases (e.g. finishing a GDD batch),
the user may engage **Automated Mode**. The `Workflow` tool then runs the phase
end-to-end with **exception-based autonomy**: it auto-selects the recommended
option at every fork, lands convergent outputs, and **halts only on a
circuit-breaker** (non-convergence, unresolvable blocker, or vision-level fork).
A consolidated report is produced for **post-hoc** review. Work is governed by a
**three-tier autonomy model** (🟢 autonomous / 🟡 logged-decision / 🔴
circuit-breaker) and the cross-mode invariants (8 GDD sections, propagate +
fresh-grep on seam edits, no commits without instruction, falsifiable ACs).

See `docs/COLLABORATIVE-DESIGN-PRINCIPLE.md` for the full protocol, the
three-tier model, and the circuit-breaker specification.

> **First session?** If the project has no engine configured and no game concept,
> run `/start` to begin the guided onboarding flow.

## Coding Standards

@.claude/docs/coding-standards.md

## Context Management

@.claude/docs/context-management.md
