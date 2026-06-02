---
name: vibe-coding-workflow
description: >-
  Vibe coding for tracked Docs/ work: clarify (or fast path) → Plan → implement
  with Progress → build/smoke → archive. Use when user says vibe coding, 三阶段,
  starts a P0–P4 task, or kicks off a roadmap *_Plan.md.
---

# Vibe Coding Workflow

Paths relative to **workspace root**.

| Topic | Canonical rule |
|-------|----------------|
| **Doc map, plan types, sync** | `.cursor/rules/docs-roadmap-arch-sync.mdc` |
| **Build / smoke** | `.cursor/rules/vulkan-smoke-test.mdc`, `Docs/CLI.md` |
| **Code-only (no Plan)** | `.cursor/rules/vulkan-desktop-quick.mdc` |
| **Commits** | `.cursor/rules/git-commit-format.mdc` (user must confirm) |

---

## Modes

| Mode | When | Phase 1 |
|------|------|---------|
| **Full vibe** | New scope, policy, large refactor | Spec + landing details → **user confirms** → Plan |
| **Fast path** | User says 直接做 / points at existing `Docs/*_Plan.md` | Skip chat; that file = task Plan; add `_Progress.md` |
| **Not vibe** | Small `VulkanDesktop/` fix | Quick rule only |

**Roadmap plan** (`ci-verification_Plan.md`, …): no Progress until kickoff. **Task WIP:** one Plan+Progress pair at `Docs/` root — see sync rule.

---

## Phase 2 — Plan

`Docs/{TaskName}_Plan.md`: problem, non-goals, touch list, ordered steps, verification (build/smoke commands or N/A), risks.

## Phase 3 — Implement

- Execute plan; **deviation** → block in Progress; minor = update Plan; major = ask user.
- **Progress:** ≤3 plan steps → **Closeout only**; larger → checkpoints per step (templates below).
- **Before close:** MSBuild Debug\|x64 + graceful smoke (`--no-validation --smoke-seconds 6`, prefer `--asset-root <repo>`) unless N/A.

## Task close

1. Plan `Status: Closed (YYYY-MM-DD)`
2. Progress → single **Closeout** (≤30 lines)
3. Move Plan + Progress → `Docs/Archived/plans/`
4. Active-Plan line → `Archived-Plan.md` (`[P0]`…`[P4]`)
5. `Docs/README.md` **Active now**
6. `EngineArchitecture.md` **only if locked policy changed** (sync rule decision tree)

---

## Progress templates

**Checkpoint** (large tasks):

```markdown
## YYYY-MM-DD — [step]

- **Files:** …
- **Verification:** build exit; smoke exit; log line (or N/A)
```

**Closeout**:

```markdown
## Closeout — YYYY-MM-DD

- **Outcome:** …
- **Verification:** command + exit 0; log: …
- **Deviations:** none | …
```

---

## Render pitfalls

Per `.cursor/rules/vulkan-descriptor-per-draw.mdc` (EngineArchitecture **§6**). Desktop path change → `Archived-Plan.md` § S1 notes. Policy narrative → Architecture + sync rule.

## Invocation

「按 vibe coding 流程做」· `@vibe-coding-workflow` · resume from open WIP Plan unless user asks replan.
