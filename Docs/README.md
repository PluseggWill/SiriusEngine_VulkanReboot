# Docs — SiriusEngine / VulkanDesktop

## Living documents (edit with active work)

| File | Purpose |
|------|---------|
| [`SprintPlan.md`](SprintPlan.md) | Executable roadmap (S0–S8), open `[ ]` tasks, **Archived** history |
| [`EngineArchitecture.md`](EngineArchitecture.md) | Architecture intent, invariants, render/data-plane policy |
| [`scene-load_Plan.md`](scene-load_Plan.md) / [`scene-load_Progress.md`](scene-load_Progress.md) | S2 scene manifest + lifecycle (in progress) |
| [`descriptor-strategy_Plan.md`](descriptor-strategy_Plan.md) / [`descriptor-strategy_Progress.md`](descriptor-strategy_Progress.md) | Locked Set 0/1/2 + push policy (referenced by S1 descriptor tasks) |
| [`instance-slab_Plan.md`](instance-slab_Plan.md) / [`instance-slab_Progress.md`](instance-slab_Progress.md) | S1 instance ring UBO (done; plan stays here for the sprint) |
| [`draw-extract_Plan.md`](draw-extract_Plan.md), [`scene-soa_Plan.md`](scene-soa_Plan.md), [`draw-cull-sort_Plan.md`](draw-cull-sort_Plan.md), [`resource-tables_Plan.md`](resource-tables_Plan.md) | S1 completed tasks — `_Progress.md` pairs at `Docs/` root |

## Guides (not vibe task logs)

| File | Purpose |
|------|---------|
| [`bootstrap.md`](bootstrap.md) | New-machine toolchain, build, run, logs |
| [`validation-layers.md`](validation-layers.md) | Validation layer install + runtime toggles |

## Archived

Older sprint vibe-coding **Plan** / **Progress** pairs (e.g. S0) and session notes live under [`Archived/`](Archived/README.md).

**Current sprint:** keep `{TaskName}_Plan.md` and `{TaskName}_Progress.md` at **`Docs/` root** for the whole sprint — including after a task line moves to `SprintPlan.md` → **Archived**. Do **not** move active-sprint plan/progress into `Archived/plans/` on task close.

When starting a new task, add both files here (root).
