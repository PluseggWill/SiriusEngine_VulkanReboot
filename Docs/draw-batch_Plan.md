# Plan: draw-batch

**Sprint:** S1 — CPU draw stream (submission)  
**Status:** Done (2026-05-26)  
**SprintPlan:** Batch runs; `RecordScenePass` scans batch runs only (minimal `vkCmdBind*` per batch).

## Problem

After opaque sort, `RecordScenePass` iterates every `Gfx_DrawInstance` and binds **set 0** (frame) per draw. Sort already groups by `(perm, material, mesh, depth)`; we need explicit **batch runs** and record loop over runs.

## Goals

1. `Gfx_BuildOpaqueDrawBatches` — scan sorted draws, emit contiguous runs with same batch key (sort key without depth bucket).
2. `RecordScenePass` — bind **set 0 once** per pass; per **batch**: bind pipeline once; per **draw** in run: VB/IB + set 2 `dynamicOffset` + draw.
3. Log once: draw count + batch count.
4. No Vulkan in Gfx module.

## Non-goals

- Set 1 material descriptor sets (next task: verify Set 0/1).
- Instancing / multi-draw indirect.
- Transparent batching.

## Files

| File | Change |
|------|--------|
| `VulkanDesktop/Gfx/Gfx_DrawBatch.h` / `.cpp` | **New** batch builder |
| `VulkanDesktop/RenderCore/Vk_Core.h` / `.cpp` | batch vector, build hook, record loop |
| `VulkanDesktop.vcxproj` + `.filters` | register sources |
| `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`, `Docs/README.md` | sync |

## Steps

- [ ] P1 — `Gfx_DrawBatch` API + build from sorted draws
- [ ] P2 — `DrawFrame` build batches after sort; log counts
- [ ] P3 — `RecordScenePass` batch loop; set 0 once per pass
- [ ] P4 — Build + smoke-run
- [ ] P5 — Archive sprint line; update S1 notes

## Build / smoke-run

MSBuild Debug|x64; smoke 4s; log `BATCH runs=2 draws=2` (demo scene).
