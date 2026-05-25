# scene-soa — SoA columns + stable entity ids (S1)

## Problem statement and goals

Replace the temporary `std::vector<Gfx_SceneEntity>` with a **columnar scene store** (`Gfx_SceneSoA`) matching `EngineArchitecture.md` §4.1–§4.2 and SprintPlan S1 data-plane items:

- SoA columns: transform, bounds, mesh/material indices, layer/mask.
- Stable entity id: slot index + per-slot generation (stale handles fail `IsAlive`).

Extract reads SoA columns; still **no Vulkan** in Gfx.

## Non-goals

- GPU resource tables, instance slab, frustum cull, sort/batch, multi-mesh record.
- Hierarchy, physics writes, scene JSON load.

## Design decisions

| Topic | Choice |
|-------|--------|
| Storage | Parallel `std::vector` columns indexed by slot |
| Iteration | `myActiveSlots` list maintained on alloc/free |
| Bounds v0 | Recomputed from transform column (translation + axis lengths as half-extents) |
| `Gfx_SceneEntity` | Removed; `Gfx_StableEntityId` lives in `Gfx_SceneSoA.h` |
| Extract API | `Gfx_ExtractDrawInstances( const Gfx_SceneSoA&, … )` |

## File touch list

| Path | Action |
|------|--------|
| `VulkanDesktop/Gfx/Gfx_SceneSoA.h` / `.cpp` | New SoA + slot map |
| `VulkanDesktop/Gfx/Gfx_DrawExtract.h` / `.cpp` | Extract from SoA |
| `VulkanDesktop/RenderCore/Vk_Core.h` / `.cpp` | `mySceneSoA`, demo init |
| `VulkanDesktop/VulkanDesktop.vcxproj` / `.filters` | New compile entries |
| `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md` | Archive tasks + sync |
| `Docs/scene-soa_Progress.md` | Progress log |

## Implementation plan

1. Add `Gfx_SceneSoA` (columns, `AllocEntity`, `IsAlive`, active slot list).
2. Point extract at SoA columns.
3. Migrate `Vk_Core` demo scene; remove `mySceneEntities`.
4. vcxproj + docs archive (SoA columns, stable entity id, DrawObjects stub line).
5. Build Debug\|x64 + smoke-run; log `[SCENE]` active count.

## Build / smoke-run

MSBuild `VulkanDesktop.sln` Debug\|x64; 4s minimized run; expect `[EXTRACT] entities=2 visible=2 draws=2`.
