---
name: vibe-coding-workflow
description: >-
  Runs a three-phase vibe coding workflow (clarify + confirm landing details →
  design doc → implement with progress logs and mandatory build/smoke-run before
  task close). Phase 1 requires explicit user sign-off on implementation landing
  details before writing Docs/[TaskName]_Plan.md. Use when the user says vibe
  coding, asks for this workflow, or works under Docs/.
---

# Vibe Coding Workflow

All paths are relative to the **workspace root**. Artifact directory: `Docs/`.

- **Active task only (WIP):** `Docs/{TaskName}_Plan.md`, `Docs/{TaskName}_Progress.md` at `Docs/` root — **at most one** pair at a time; see `Docs/README.md` → **Active now**
- **Closed tasks:** `Docs/Archived/plans/{TaskName}_Plan.md` + `_Progress.md` (closeout-only progress)
- Cross-task roadmap: `Docs/Active-Plan.md` (grep/read the relevant sprint section; do not read the full file by default). Completed lines: `Docs/Archived-Plan.md`
- Architecture intent: `Docs/EngineArchitecture.md` (read only when policy/boundaries change)

## Phase 1 — Clarify & confirm landing details

Goal: agree on a complete, unambiguous task spec **and** implementation landing details with the user before any plan file or code.

### 1a — Task spec

- Ask targeted questions until scope, inputs/outputs, constraints, acceptance criteria, and out-of-scope items are explicit.
- Do **not** assume unstated requirements; surface options and let the user choose.

### 1b — Landing details (落地细节)

After the spec is clear, **propose concrete landing details in chat** (do not write `*_Plan.md` yet) and get explicit user confirmation. Cover at least what will appear in the plan:

| Area | Confirm with user |
|------|-------------------|
| **Touch list** | Files/modules to add or change (and what stays untouched) |
| **Design choices** | APIs, data layout, shader/descriptor strategy, config paths — with 1–2 alternatives when trade-offs matter |
| **Implementation order** | Ordered steps / milestones (what ships first, dependencies) |
| **Verification** | Build config, smoke-run commands, log signals, or documented N/A |
| **Risks / rollback** | Anything that could break existing behavior or needs a revert path |

- If the user disagrees or is unsure on any item, revise the proposal and ask again; do **not** fold unconfirmed choices into the plan.
- Do **not** create `*_Plan.md` or write implementation code until the user confirms **both** the spec **and** the landing details (or explicitly asks to skip remaining questions / defer a specific item with a noted follow-up).

**Phase 1 exit gate (user must confirm):** “Spec + landing details are agreed; proceed to write `{TaskName}_Plan.md`.”

## Phase 2 — Design

Precondition: user has confirmed the clarified spec **and** the landing details from Phase 1b.

1. Pick a short `TaskName` (ASCII, no spaces; e.g. `add-login-form`).
2. Ensure `Docs/` exists.
3. Create `Docs/{TaskName}_Plan.md` from the **user-confirmed** spec and landing details only (no new design choices in the doc without returning to Phase 1). Include at minimum:
   - Problem statement and goals
   - Non-goals / out of scope
   - Design decisions (with alternatives considered if relevant)
   - File/module touch list (expected)
   - Step-by-step implementation plan (ordered, checkable)
   - **Build / smoke-run** plan (commands, expected signals, when N/A)
   - Risks and rollback notes if any

Do not contradict the confirmed spec or landing details in the plan; if something is still unclear or was not user-confirmed, return to Phase 1 (1a or 1b) before editing the plan further.

## Phase 3 — Implement

Precondition: `Docs/{TaskName}_Plan.md` exists and is current.

1. Treat the plan as the source of truth; if reality diverges, update the plan in the same edit session or after agreement with the user.
2. Execute the plan in order. After **each** logical step (or each coherent batch of edits), append a **checkpoint** to `Docs/{TaskName}_Progress.md` (see template below). Use **verbose** blocks only for deviations or non-obvious pitfalls.
3. If the project uses Perforce (P4), check whether current edits touch files that are already checked out. If yes, explicitly remind the user.
4. Debug until acceptance criteria in the plan are met; log each fix iteration in `_Progress.md` the same way.
5. Before marking the task done or moving its line to `Archived-Plan.md`, complete **Build / smoke-run** (below) unless the plan documents why it does not apply.

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

3. **Smoke-run** — use commands from **`.cursor/rules/vulkan-smoke-test.mdc`** and full flag reference **`Docs/CLI.md`**. At least one:

   | Check | Command (from `x64\Debug`) |
   |-------|----------------------------|
   | CLI only | `.\VulkanDesktop.exe --help` |
   | **Preferred — full lifecycle** | `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` |
   | Demo scene | add `--scene Data/Scenes/demo.json` (optional; default is demo) |
   | Minimal scene | add `--scene Data/Scenes/smoke.json` |
   | Asset root from odd cwd | `--asset-root <repo-root>` plus scene path above |
   | Fast CI (no 6s dwell) | `--smoke-frames 2` only |

   **Graceful smoke** (`--smoke-seconds N`) keeps the main loop running **N seconds after scene load** (`LoadSceneResources` completed), then exits through `UnloadScene` → `Shutdown`. Use this for task close, not only `Stop-Process` after a timed wait.

   Optional quick visual (does **not** replace graceful smoke):

   ```powershell
   Set-Location x64\Debug
   $p = Start-Process -FilePath ".\VulkanDesktop.exe" -ArgumentList "--no-validation" -PassThru -WindowStyle Minimized
   Start-Sleep -Seconds 6
   if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
   ```

4. **Signals of success** (adjust per task in the plan; see `vulkan-smoke-test.mdc`):

   - Build exit code 0; SPIR-V custom build OK when shaders touched.
   - Smoke process exit code **0** when using `--smoke-seconds` or `--smoke-frames`.
   - `[CONFIG] assetRoot=…` when asset root changed.
   - `[SCENE] LoadSceneResources completed`; `[APP] Smoke dwell reached`; `[SCENE] UnloadScene: GPU scene resources released` for lifecycle smoke.
   - `[SHADER]` / `[LOADER]` / `[RESOURCE-TABLE]` / `[EXTRACT]` lines match scene scope.
   - No new `[ERROR]` during init before intentional shutdown (`--no-validation` if layers not installed).

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

### `Active-Plan.md` / `Archived-Plan.md` maintenance

- **`Docs/Active-Plan.md`:** active sprints (S0–S8, parallel) list **only** open `[ ]` tasks.
- **`Docs/Archived-Plan.md`:** when a sprint line completes, **move** it from Active-Plan to Archived-Plan with sprint tag (`[S0]`…`[S8]`, `[parallel]`) and completion note/date.
- Do not leave `[x]` items in Active-Plan.

### Task close (mandatory when build/smoke passes)

1. Mark `Docs/{TaskName}_Plan.md` → **`Status: Closed (YYYY-MM-DD)`**.
2. **Collapse** `_Progress.md` to a single **`## Closeout`** section (≤ ~30 lines): outcome summary, final verification command + exit code, 2–3 log lines, deviations (if any). Remove intermediate checkpoints.
3. **Move** both files to **`Docs/Archived/plans/`** (same filenames).
4. **Move** the matching sprint line from **`Active-Plan.md`** → **`Archived-Plan.md`**.
5. Update **`Docs/README.md` → Active now** (clear WIP or set the next task).
6. Update **`Docs/Archived/README.md`** plans index if needed.
7. Fix cross-links in touched docs (`EngineArchitecture.md` only if policy changed — `docs-roadmap-arch-sync.mdc`).

**WIP limit:** only one `{TaskName}_*` pair at `Docs/` root. Starting a new task requires the previous pair archived.

**Sprint close (optional batch):** when an entire sprint is signed off, refresh `Docs/README.md` living/epic tables only — task plans should already be under `Archived/plans/`.

### Progress log templates

**Checkpoint** (default — append during implementation):

```markdown
## YYYY-MM-DD — [step id / short title]

- **Plan ref:** …
- **Files:** `path/a`, `path/b`
- **What changed:** one short paragraph
- **Verification:** build exit code; smoke command; 1–2 log lines (or N/A + reason)
```

**Verbose** (deviations, blocked steps, subtle pitfalls only):

```markdown
## YYYY-MM-DD — [title] (verbose)

- **Deviation / pitfall:** …
- **Impact:** …
- **Resolution:** …
```

**Comment-only / doc-only batches:** one inline line under the latest checkpoint (`Verification: N/A — comments only`); do **not** add a new `##` section.

**Closeout** (replaces all checkpoints at task close):

```markdown
## Closeout — YYYY-MM-DD

- **Outcome:** …
- **Verification:** `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0; log: `…`, `…`
- **Deviations:** none | …
- **Plan:** [`{TaskName}_Plan.md`]({TaskName}_Plan.md)
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
| Shader/UBO layout or §5.3 policy changes | Update `EngineArchitecture.md` + `Active-Plan.md` § S1 implementation notes in the **same** change set (`docs-roadmap-arch-sync.mdc`) |

After closing a render/data-plane task, skim `Docs/Active-Plan.md` **§ S1 — implementation notes** and trim or extend the table if debt shifted.

## Invocation

- User may say: “按 vibe coding 流程做” or `@vibe-coding-workflow`.
- If `Docs/{TaskName}_Plan.md` already exists, resume from the latest incomplete plan step unless the user requests replanning.
