---
name: vibe-coding-workflow
description: >-
  Runs a three-phase vibe coding workflow (clarify → design doc → implement with
  progress logs and mandatory build/smoke-run before task close). Creates and
  maintains Docs/[TaskName]_Plan.md and Docs/[TaskName]_Progress.md under the
  workspace. Use when the user says vibe coding, asks for this workflow, or works
  under Docs/.
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
   - **Build / smoke-run** plan (commands, expected signals, when N/A)
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
5. Before marking the task done or archiving a `SprintPlan.md` line, complete **Build / smoke-run** (below) unless the plan documents why it does not apply.

### Build / smoke-run (mandatory before task close)

Run after implementation (or after each batch that changes compile/runtime behavior). Record commands and outcomes in `_Progress.md` under **Verification**.

| Step | When required | Action |
|------|----------------|--------|
| **Build** | Any change under `VulkanDesktop/`, shaders, or `.vcxproj` | Compile the solution; fix errors before closing the task. |
| **Smoke-run** | Runnable app affected (startup, assets, render loop, CLI) | Short run; confirm no crash during init / one frame / `--help`. |
| **Log check** | Smoke-run or loader/config changes | Tail `Logs/engine_runtime_log.txt` (and `shader_compile_log.txt` if shaders changed) for expected categories/paths. |

**Do not** archive a sprint item or mark the plan “Done” on build/smoke-run failure without fixing or documenting a blocked deviation.

#### VulkanDesktop (default for this repo)

1. **Locate MSBuild** (if not on PATH):

   ```powershell
   & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
   ```

2. **Build** (from workspace root):

   ```powershell
   & "<MSBuild.exe>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
   ```

   Use `Release|x64` only when the plan or user asks for it.

3. **Smoke-run** (pick what fits the task; at least one):

   ```powershell
   & "x64\Debug\VulkanDesktop.exe" --help
   ```

   ```powershell
   Set-Location x64\Debug
   $p = Start-Process -FilePath ".\VulkanDesktop.exe" -PassThru -WindowStyle Minimized
   Start-Sleep -Seconds 4
   if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
   ```

   For asset/config work, also run from a **different cwd** with `--asset-root <repo-root>` and confirm resolution in the log.

4. **Signals of success** (adjust per task in the plan):

   - Build exit code 0; SPIR-V custom build OK when shaders touched.
   - `[CONFIG] assetRoot=…` when asset root changed.
   - `[SHADER]` / `[LOADER]` / `[RESOURCE]` lines show resolved paths under the repo.
   - No new `[ERROR]` during init before intentional shutdown.

#### When build/smoke-run is N/A

Doc-only tasks, comment-only edits, or archived notes: state **“Build/smoke-run: N/A — …”** in the plan verification section and `_Progress.md`. Do not skip silently.

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
- **Verification:** build command + exit code; smoke-run steps; log lines checked (or N/A + reason)
```

## Subagents (Task tool)

Use subagents to keep the main thread focused; **paste the current `TaskName` and phase** into the subagent prompt.

| Situation | Subagent | Prompt essentials |
|-----------|----------|---------------------|
| Unknown codebase layout, “where is X?” | `explore` (readonly) | Goal, constraints, return file paths and short findings only |
| Build, smoke-run, tests, grep-heavy commands | `shell` | MSBuild path, `Debug|x64`, smoke-run + log tail; stdout/stderr summary |
| Broader multi-file change while you coordinate plan/progress | `generalPurpose` | Bound scope, must not edit `_Plan`/`_Progress` unless instructed |

Rules for subagents:

- Readonly exploration must not modify files unless the task explicitly allows implementation.
- On return, integrate findings into the plan or implementation; log substantive outcomes in `_Progress.md`.

## VulkanDesktop pitfalls (from shipped tasks)

Apply when a task touches **draw stream**, **descriptors**, or **multi-entity demos** (see also `.cursor/rules/vulkan-descriptor-per-draw.mdc`):

| Pitfall | Do instead |
|---------|------------|
| Per-draw data written into a **shared frame UBO** between draws | Push constants or `UNIFORM_BUFFER_DYNAMIC` + offset (§5.3) |
| Demo meshes overlap | Fix transform path first; then adjust SoA spacing |
| Plan task done + **descriptor policy fix** in same session | Prefer **two commits** (implementation vs policy/shader); one Progress entry can cover both |
| Shader/UBO layout or §5.3 policy changes | Update `EngineArchitecture.md` + `SprintPlan.md` S1 notes in the **same** change set (`docs-roadmap-arch-sync.mdc`) |

After closing a render/data-plane task, skim `Docs/SprintPlan.md` **§ S1 — implementation notes** and trim or extend the table if debt shifted.

## Invocation

- User may say: “按 vibe coding 流程做” or `@vibe-coding-workflow`.
- If `Docs/{TaskName}_Plan.md` already exists, resume from the latest incomplete plan step unless the user requests replanning.
