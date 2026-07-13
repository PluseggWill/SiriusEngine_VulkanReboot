---
name: vibe-coding-workflow
description: >-
  Vibe coding for tracked Docs/ work: clarify (or fast path) → Plan → implement
  with Progress → build/smoke → archive. Use when user says vibe coding, 三阶段,
  starts a P0–P4 task, or kicks off a roadmap *_Plan.md.
---

# Vibe Coding Workflow

Paths relative to **workspace root**. Artifacts live under **`Docs/`** (never `VibeCoding/`).

| Topic | Canonical |
|-------|-----------|
| Doc map / sync | `.cursor/rules/docs-roadmap-arch-sync.mdc` |
| Build / smoke | `.cursor/rules/vulkan-smoke-test.mdc` |
| RenderCore passes | `.cursor/rules/vulkan-render-pass-pitfalls.mdc` |
| Code-only (no Plan) | `.cursor/rules/vulkan-desktop-quick.mdc` |
| Commits | `.cursor/rules/git-commit-format.mdc` (user must confirm) |

## Modes

| Mode | When | Phase 1 |
|------|------|---------|
| **Full vibe** | New scope, policy, large refactor | Spec → **user confirms** → Plan |
| **Fast path** | 直接做 / existing `Docs/*_Plan.md` | That file = Plan; add `_Progress.md` |
| **Not vibe** | Small `VulkanDesktop/` fix | Quick rule only |

**Roadmap plan:** no Progress until kickoff. **Task WIP:** one Plan+Progress pair at `Docs/` root.

## Phase 2 — Plan

`Docs/{TaskName}_Plan.md`: problem, non-goals, touch list, ordered steps, verification (`Verify-CI` / `Verify-Smoke` / G0-validation or N/A), risks.

## Phase 3 — Implement

- Deviation → Progress block; minor = update Plan; major = ask user.
- Progress: ≤3 steps → **Closeout only**; larger → per-step checkpoints.
- Before close: `powershell -File Scripts/Verify-CI.ps1`; GPU → `Verify-Smoke.ps1`; pass/barrier/descriptor work → G0-validation — see `vulkan-smoke-test.mdc`. N/A → document in Closeout.

## Task close

1. Plan `Status: Closed (YYYY-MM-DD)`
2. Progress → single **Closeout** (≤30 lines)
3. Move Plan + Progress → `Docs/Archived/plans/`
4. Active-Plan line → `Archived-Plan.md` stub
5. `Docs/README.md` **Active now**
6. `EngineArchitecture.md` **only if locked policy changed**

## Progress templates

**Checkpoint:**

```markdown
## YYYY-MM-DD — [step]
- **Files:** …
- **Verification:** build/smoke/validation exit (or N/A); log line
```

**Closeout:**

```markdown
## Closeout — YYYY-MM-DD
- **Outcome:** …
- **Verification:** command + exit 0; log: …
- **Deviations:** none | …
```

## Render pitfalls

Descriptor: `vulkan-descriptor-per-draw.mdc` + Architecture §6. New `Vk_*Pass`: `vulkan-render-pass-pitfalls.mdc`. Desktop path debt → `Archived-Plan.md` § S1 notes.

## Invocation

「按 vibe coding 流程做」· `@vibe-coding-workflow` · resume open WIP Plan unless replan.
