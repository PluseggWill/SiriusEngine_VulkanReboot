# Docs — SiriusEngine / VulkanDesktop

## Living documents (edit with active work)

| File | Purpose |
|------|---------|
| [`SprintPlan.md`](SprintPlan.md) | Executable roadmap (S0–S8), open `[ ]` tasks, **Archived** history |
| [`EngineArchitecture.md`](EngineArchitecture.md) | Architecture intent, invariants, render/data-plane policy |
| [`scene-load_Plan.md`](scene-load_Plan.md) / [`scene-load_Progress.md`](scene-load_Progress.md) | S2 scene manifest + lifecycle (in progress) |
| [`descriptor-strategy_Plan.md`](descriptor-strategy_Plan.md) / [`descriptor-strategy_Progress.md`](descriptor-strategy_Progress.md) | Locked Set 0/1/2 + push policy (referenced by S1 descriptor tasks) |
| [`instance-slab_Plan.md`](instance-slab_Plan.md) / [`instance-slab_Progress.md`](instance-slab_Progress.md) | S1 instance ring UBO (done; plan stays here for the sprint) |
| [`descriptor-set2-verify_Plan.md`](descriptor-set2-verify_Plan.md) / [`descriptor-set2-verify_Progress.md`](descriptor-set2-verify_Progress.md) | S1 Set 2 UNIFORM_BUFFER_DYNAMIC verification |
| [`draw-batch_Plan.md`](draw-batch_Plan.md) / [`draw-batch_Progress.md`](draw-batch_Progress.md) | S1 opaque batch runs + record (done) |
| [`descriptor-set1-verify_Plan.md`](descriptor-set1-verify_Plan.md) / [`descriptor-set1-verify_Progress.md`](descriptor-set1-verify_Progress.md) | S1 Set 1 material/texture per batch (done) |
| [`transparency_Plan.md`](transparency_Plan.md) / [`transparency_Progress.md`](transparency_Progress.md) | S1 opaque + transparent passes (done) |
| [`instance-slab-overflow_Plan.md`](instance-slab-overflow_Plan.md) / [`instance-slab-overflow_Progress.md`](instance-slab-overflow_Progress.md) | S1 slab overflow fail-closed (done) |
| [`demo-transform-sync_Plan.md`](demo-transform-sync_Plan.md) / [`demo-transform-sync_Progress.md`](demo-transform-sync_Progress.md) | S1 SoA/cull vs instance matrix (done) |
| [`lod-v0_Plan.md`](lod-v0_Plan.md) / [`lod-v0_Progress.md`](lod-v0_Progress.md) | S1 CPU distance LOD + hysteresis (done) |
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
