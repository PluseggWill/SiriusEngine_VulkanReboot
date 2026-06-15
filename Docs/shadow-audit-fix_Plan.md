# Plan: shadow-audit-fix — Directional shadow + IBL lighting contract

**Status:** In progress (2026-06-15)  
**Progress:** [`shadow-audit-fix_Progress.md`](shadow-audit-fix_Progress.md)

## Problem

Shadow + IBL were incorrectly coupled (`PBR_SHADOWED_IBL_SCALE` crushed ambient in sun shadow), producing near-black masks and chaotic contrast. Shadow compare was active at invalid sun elevations (below horizon), inverting upper/lower scene shading. Prefilter used GPU box-filter mips (not GGX roughness mips), muddying specular IBL.

## Non-goals

- CSM / cascade atlasing
- Offline multi-roughness prefilter bake (follow-up; mip0-only until then)
- SSAO / post (S6)

## Target contract (landed)

| Layer | Rule |
|-------|------|
| **IBL / ambient** | Independent of shadow map — full split-sum contribution always |
| **Direct sun** | `Pbr_EvalDirect(..., sunColor * sunShadow)` only |
| **Shadow compare** | Active when `shadowsEnabled && sunDir.z > 0.08` (Z-up overhead sun) |
| **Shadow sample** | 3×3 PCF; `shadowParams.w = 1/mapSize` |
| **Shadow pass** | Khronos GREATER + BACK cull + fixed bias; scene AABB ortho |
| **Prefilter** | mip0 only (`maxMip=0`); no GPU box mips on prefilter |

## Touch list

- `VulkanDesktop/Shader/PbrShadow.glsl` — PCF; remove IBL modulation
- `VulkanDesktop/Shader/LightingBindings.glsl`, `DeferredLighting.frag`, `TriangleFrag_Lit*.frag`
- `VulkanDesktop/Gfx/Gfx_LightingMath.h` — sun elevation gate
- `VulkanDesktop/Gfx/Gfx_LightingGlobals.h` — shadowParams.w, gated compare
- `VulkanDesktop/RenderCore/Vk_FrameUniformUploader.cpp`, `Vk_ShadowMapPass.cpp`, `Vk_GBufferPass.cpp`, `Vk_ScenePasses.cpp`
- `VulkanDesktop/RenderCore/Vk_IblResources.cpp` — prefilter mip0
- `VulkanDesktop/GfxTests/GfxTests_Main.cpp`

## Steps

1. **Decouple IBL from shadow map** — delete `Pbr_ModulateAmbientForSunShadow`  
2. **Sun elevation gate** — skip shadow pass + compare when sun below horizon  
3. **PCF + prefilter mip fix** — 3×3 taps; prefilter mip0 only  
4. **Verify** — `powershell -File Scripts/Verify-CI.ps1`; manual Sponza default sun

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0  
- Manual: default sun — contact shadows readable, no IBL black masks; below-horizon sun — no inverted shadow chaos

## Risks

- Softer contact shadows without IBL crush — intentional; tune sun intensity / future AO if needed
