# Plan: khronos-shadow-map — Khronos-style directional shadow refactor

**Status:** Closed (2026-06-12)  
**Progress:** [`khronos-shadow-map_Progress.md`](khronos-shadow-map_Progress.md)

## Problem

S5 shadow v0 drifted from sample best-practice: camera/AABB-fit churn, shader-only bias, `LESS` compare with `orthoRH_ZO`, no raster depth bias. User asked to align with **Khronos Vulkan-Samples** (`multithreading_render_passes` / `shadows/*`).

## Non-goals

- CSM / cascade atlasing
- Scene-graph light node (vkb::sg)
- Changing IBL / deferred topology

## Khronos contract (target)

| Item | Khronos | Landing |
|------|---------|---------|
| Ortho | Fixed `OrthographicCamera` on light; `vulkan_style_projection(ortho) * view` | Scene-load ortho cache from opaque AABB in light space |
| Shadow pass bias | `vkCmdSetDepthBias(-1.4, 0, -1.7)` | Shadow pass record |
| Compare | `sampler2DShadow` + `GREATER_OR_EQUAL` + border white | `Vk_ShadowMapPass` sampler |
| Sample | `texture(shadow, vec3(uv, z))` — no large shader depth offset | `PbrIbl.glsl` |
| Matrix UBO | `light_matrix` per frame | `GpuLightingGlobals.myLightViewProj` |

## Steps

1. `Gfx_LightingMath` — Khronos ortho + `VulkanStyleProjection` + scene ortho cache helpers  
2. `Vk_ShadowMapPass` — GREATER_OR_EQUAL sampler, depth bias dynamic + `vkCmdSetDepthBias`, ortho refresh on scene load  
3. `PbrIbl.glsl` — Khronos PCF sample path  
4. `Vk_Core` / uploader — use cached ortho + sun light view  
5. Verify: `Verify-CI.ps1` (+ smoke N/A shader-only policy)

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0  
- Manual: RenderDoc ShadowMap pass — VS inputs valid, depth covers scene, matrix stable when fly camera moves

## Risks

- Sun direction change at runtime vs cached ortho — recompute ortho on scene load only (sun fixed from env at load)
