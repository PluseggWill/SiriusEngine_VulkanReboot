---
name: vibe-coding-workflow
description: >-
  Runs a three-phase vibe coding workflow (clarify → design doc → implement with
  progress logs). Creates and maintains Docs/[TaskName]_Plan.md and
  Docs/[TaskName]_Progress.md under the workspace. Use when the user says
  vibe coding, asks for this workflow, or works under Docs/.
---

# Vibe Coding Workflow

All paths are relative to the **workspace root**. Artifact directory: `Docs/`.

- Per-task: `Docs/{TaskName}_Plan.md`, `Docs/{TaskName}_Progress.md`
- Cross-task roadmap: `Docs/SprintPlan.md`
- Architecture intent: `Docs/EngineArchitecture.md`

## Phase 1 — Clarify

Goal: agree on a complete, unambiguous task spec with the user.

- Ask targeted questions until scope, inputs/outputs, constraints, acceptance criteria, and out-of-scope items are explicit.
- Do **not** assume unstated requirements; surface options and let the user choose.
- Do **not** create `*_Plan.md` or write implementation code until the user confirms the spec is sufficient to proceed (or explicitly asks to skip remaining questions).

## Phase 2 — Design

Precondition: user has confirmed the clarified spec.

1. Pick a short `TaskName` (ASCII, no spaces; e.g. `add-login-form`).
2. Ensure `Docs/` exists.
3. Create `Docs/{TaskName}_Plan.md` from the **confirmed** spec only. Include at minimum:
   - Problem statement and goals
   - Non-goals / out of scope
   - Design decisions (with alternatives considered if relevant)
   - File/module touch list (expected)
   - Step-by-step implementation plan (ordered, checkable)
   - Test/verification plan
   - Risks and rollback notes if any

Do not contradict the clarified spec in the plan; if something is still unclear, return to Phase 1.

## Phase 3 — Implement

Precondition: `Docs/{TaskName}_Plan.md` exists and is current.

1. Treat the plan as the source of truth; if reality diverges, update the plan in the same edit session or after agreement with the user.
2. Execute the plan in order. After **each** logical step (or each coherent batch of edits), append to `Docs/{TaskName}_Progress.md`:
   - Timestamp (ISO 8601 date, local reasoning ok)
   - Step reference (which plan section / checklist item)
   - Files changed (paths)
   - Summary of behavior change
   - How verified (commands run, manual checks)
3. If the project uses Perforce (P4), check whether current edits touch files that are already checked out. If yes, explicitly remind the user.
4. Debug until acceptance criteria in the plan are met; log each fix iteration in `_Progress.md` the same way.

### Deviation handling protocol (mandatory)

Apply this protocol whenever implementation diverges from `Docs/{TaskName}_Plan.md`.

Deviation triggers:
- New files/modules touched that are not in the plan touch list
- Step order changes or skipped steps
- Behavior/output different from accepted scope
- Extra risk discovered (performance, migration, compatibility, security)

Required actions:
1. Pause implementation and mark the current step as blocked in `_Progress.md`.
2. Record a short deviation note in `_Progress.md`:
   - What changed vs plan
   - Why the deviation is needed
   - Impact/risk
3. Resolve by one of:
   - **Minor deviation**: update `Docs/{TaskName}_Plan.md` immediately, then continue.
   - **Major deviation** (scope/acceptance/risk changes): ask user confirmation first, then update plan and continue.
4. Do not continue coding with silent deviation.

### `Docs/SprintPlan.md` maintenance

- Active sprints (S0–S7, parallel): **only** open `[ ]` tasks.
- When completed: **move** the line to **`## Archived`** with sprint tag (`[S0]`…`[S7]`, `[parallel]`); preserve completion notes/dates.
- Do not leave checked `[x]` items in active sprints.

### Progress log entry template

Append blocks like:

```markdown
## YYYY-MM-DD HH:MM — [step id / short title]

- **Plan ref:** …
- **Files:** `path/a`, `path/b`
- **What changed:** …
- **Verification:** …
```

## Subagents (Task tool)

Use subagents to keep the main thread focused; **paste the current `TaskName` and phase** into the subagent prompt.

| Situation | Subagent | Prompt essentials |
|-----------|----------|---------------------|
| Unknown codebase layout, “where is X?” | `explore` (readonly) | Goal, constraints, return file paths and short findings only |
| Build, tests, grep-heavy commands | `shell` | Exact commands, cwd, report stdout/stderr summary |
| Broader multi-file change while you coordinate plan/progress | `generalPurpose` | Bound scope, must not edit `_Plan`/`_Progress` unless instructed |

Rules for subagents:

- Readonly exploration must not modify files unless the task explicitly allows implementation.
- On return, integrate findings into the plan or implementation; log substantive outcomes in `_Progress.md`.

## Invocation

- User may say: “按 vibe coding 流程做” or `@vibe-coding-workflow`.
- If `Docs/{TaskName}_Plan.md` already exists, resume from the latest incomplete plan step unless the user requests replanning.
