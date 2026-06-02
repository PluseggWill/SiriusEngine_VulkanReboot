# draw-cull-sort — CPU frustum cull + opaque sort (S1)

## Problem statement and goals

After **extract**, filter `Gfx_DrawInstance` list by **view frustum** (SoA `bounds` column) and **layer mask**, then **sort opaque** draws by packed `mySortKey` `(pipelinePerm, material, mesh, depthBucket)`.

No Vulkan in Gfx module. `RecordScenePass` consumes sorted `myExtractResult`.

## Non-goals

- LOD, transparency lists, batch runs, GPU cull.
- Moving record off SoA transform reads (instance slab debt stays).

## Design

| Topic | Choice |
|-------|--------|
| View params | Single **`Gfx_CullViewParams`** (view, proj, layer mask) shared by extract + cull |
| Frustum | Six planes from `proj * view`, AABB positive-vertex test |
| Cull API | `Gfx_CullDrawInstancesInPlace(scene, params, extractResult)` — parallel arrays stay paired |
| Sort API | `Gfx_SortOpaqueDrawInstances(extractResult)` — permute **`myDrawInstances`** and **`myVisibleEntityIndices`** together by `mySortKey` |
| Frame order | Extract → Cull → Sort → Record |

## Files

- `VulkanDesktop/Gfx/Gfx_DrawCullSort.h` / `.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.cpp` — wire frame loop
- vcxproj, `SprintPlan.md`, `EngineArchitecture.md`, progress log

## Verification

MSBuild Debug|x64; smoke-run; log `[CULL]` once; both demo meshes visible with default camera.

## Hygiene (2026-05-26 follow-up)

- Removed duplicate **`Gfx_ExtractViewParams`**; extract uses **`Gfx_CullViewParams`** from `Gfx_DrawCullSort.h`.
- Sort reorders **`Gfx_ExtractResult`** parallel arrays in lockstep (fixes index/draw mismatch after sort).
- Dropped unused `<cmath>` in `Gfx_DrawCullSort.cpp`.
- Demo Z-spin moved to SoA before extract — [`demo-transform-sync_Plan.md`](demo-transform-sync_Plan.md) (cull/bounds match rendered matrix).
