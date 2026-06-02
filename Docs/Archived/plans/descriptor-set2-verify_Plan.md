# Plan: descriptor-set2-verify

**Sprint:** S1 — CPU draw stream  
**Status:** Done (2026-05-26)  
**SprintPlan:** Verify descriptor policy (Set 2) — `UNIFORM_BUFFER_DYNAMIC` on instance slab; distinct `dynamicOffset` per draw.

## Problem

Instance slab exists and `FillInstanceSlab` writes `GpuObjectData`, but the GPU path still uses **push constants** for `mat4 model`. Set 2 layout, pool, descriptors, and shader binding are not wired — policy is unproven.

## Goals

1. Set 2 layout: `UNIFORM_BUFFER_DYNAMIC` → `GpuObjectData` at binding 0 (vertex).
2. Pipeline layout: sets **0 / 1 / 2** (set 1 = empty placeholder layout per `Vk_DescriptorPolicy.h`).
3. Per in-flight frame: allocate/update object descriptor → whole `myObjectBuffer`, `range = sizeof(GpuObjectData)`.
4. `RecordScenePass`: bind set 0 once per draw (unchanged); bind set 2 with **`dynamicOffset = draw.myInstanceDataOffset`**; **remove** model push constant.
5. `TriangleVertex.vert`: read `model` from `layout(set = 2, binding = 0)`; rebuild SPIR-V.
6. Log once per run: dynamic offsets used for 2+ draws.

## Non-goals

- Set 1 material batch / texture per material.
- Removing push constants globally from policy (Set 2 path proven; push remains valid per §5.3).
- Batch record.

## Files

| File | Change |
|------|--------|
| `VulkanDesktop/Shader/TriangleVertex.vert` | Set 2 UBO for model |
| `VulkanDesktop/RenderCore/Vk_Enum.h` | `eVk_ObjectBinding` |
| `VulkanDesktop/RenderCore/Vk_Core.h` | `myMaterialSetLayout`, `myObjectSetLayout` |
| `VulkanDesktop/RenderCore/Vk_Core.cpp` | layouts, pool, object descriptors, pipeline, record |
| `VulkanDesktop/RenderCore/Vk_DescriptorPolicy.h` | note Set 2 live |
| `Docs/EngineArchitecture.md`, `Docs/SprintPlan.md` | sync |

## Steps

- [ ] P1 — Descriptor layouts (frame + empty material + object dynamic)
- [ ] P2 — Pool, object descriptor writes, init order (pipeline after slabs)
- [ ] P3 — Pipeline layout 3 sets, no model push
- [ ] P4 — Shader + record bind with dynamicOffset
- [ ] P5 — Build, smoke-run, validation log check
- [ ] P6 — Archive SprintPlan line; update S1 implementation notes

## Build / smoke-run

MSBuild Debug|x64; smoke 4s; expect 2 meshes at ±4 with spin; log `Set 2 dynamicOffset` for 2 draws.
