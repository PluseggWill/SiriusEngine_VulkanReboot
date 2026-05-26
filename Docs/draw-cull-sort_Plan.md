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
| Frustum | Six planes from `proj * view`, AABB positive-vertex test |
| Cull API | `Gfx_CullDrawInstancesInPlace(scene, params, extractResult)` |
| Sort API | `Gfx_SortOpaqueDrawInstances(draws)` — `std::sort` on `mySortKey` |
| Frame order | Extract → Cull → Sort → Record |

## Files

- `VulkanDesktop/Gfx/Gfx_DrawCullSort.h` / `.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.cpp` — wire frame loop
- vcxproj, `SprintPlan.md`, `EngineArchitecture.md`, progress log

## Verification

MSBuild Debug|x64; smoke-run; log `[CULL]` once; both demo meshes visible with default camera.
