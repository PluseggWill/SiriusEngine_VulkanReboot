# Docs — SiriusEngine / VulkanDesktop

## Active now

| Field | Value |
|-------|--------|
| **Task** | *(none — pick next from [`Active-Plan.md`](Active-Plan.md))* |
| **Recommended next** | Stage 2 `hybrid-deferred-epic` or S2 `multi-view` / flat world matrices |
| **Plan / Progress** | Create `Docs/{TaskName}_Plan.md` + `_Progress.md` when starting |
| **Do not @** | Completed tasks under [`Archived/plans/`](Archived/plans/) unless debugging history |

*Update this table when opening or closing a vibe task.*

---

## Roadmap & architecture

| File | Purpose |
|------|---------|
| [`Active-Plan.md`](Active-Plan.md) | Open `[ ]` tasks, north star, sprint map, dependency graph, backlog |
| [`Archived-Plan.md`](Archived-Plan.md) | Completed `[x]` sprint lines |
| [`SprintPlan.md`](SprintPlan.md) | Index → Active / Archived split |
| [`EngineArchitecture.md`](EngineArchitecture.md) | Architecture intent, invariants, render/data-plane policy |
| [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) | Sprint close-out validation runbook |

## Lighting epics (reference, not WIP)

| File | Purpose |
|------|---------|
| [`forward-rendering-epic_Plan.md`](forward-rendering-epic_Plan.md) | Stage 1 — forward baseline (closed) |
| [`forward-stage1.md`](forward-stage1.md) | Stage 1 golden/perf, handoff, gaps (canonical) |
| [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) | Stage 2 — hybrid deferred + PBR |
| [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) | Stage 3 — optional DDGI |

## Guides

| File | Purpose |
|------|---------|
| [`CLI.md`](CLI.md) | CLI args, `engine.json`, examples |
| [`.cursor/rules/vulkan-smoke-test.mdc`](../.cursor/rules/vulkan-smoke-test.mdc) | Agent smoke-run commands |
| [`bootstrap.md`](bootstrap.md) | Toolchain, build, run, logs |
| [`validation-layers.md`](validation-layers.md) | Validation layers install + toggles |
| [`SceneJSON.en.md`](SceneJSON.en.md) / [`SceneJSON.md`](SceneJSON.md) | Scene JSON v1 authoring (EN / 中文) |

## Archived task logs

Completed vibe-coding **Plan** / **Progress** pairs: [`Archived/plans/`](Archived/plans/) — see [`Archived/README.md`](Archived/README.md).

**Workflow:** on task close, collapse Progress to **Closeout**, move both files to `Archived/plans/`, move the sprint line from **Active-Plan** → **Archived-Plan**, clear **Active now** above.
