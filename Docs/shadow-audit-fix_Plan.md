# Plan: shadow-audit-fix — Directional shadow audit & repair

**Status:** In progress (2026-06-13)  
**Progress:** [`shadow-audit-fix_Progress.md`](shadow-audit-fix_Progress.md)

## Problem

Incremental shadow tweaks after Khronos landing (`39f6ace`) broke Sponza: shadow direction reads wrong, contact shadows unreadable. Stack included BACK cull flip, frustum UV reject, IBL hemisphere gate, and compare-depth experiments layered without a single contract.

## Non-goals

- CSM / cascade atlasing
- IBL energy or prefilter changes
- SSAO / post (S6)

## Khronos contract (keep)

| Item | Target |
|------|--------|
| Light view | Eye on sun side: `focus + sunDir * reach`, look at scene center |
| Ortho | Scene opaque AABB in light space + scene-scaled padding + texel snap |
| Matrix | `vulkan_style_projection(ortho) * lightView` |
| Shadow pass | `GREATER` write, `GREATER_OR_EQUAL` compare, border white, **FRONT cull** (Z-up deviation per khronos-shadow-map closeout) |
| Bias | Khronos slope `-1.7`; constant scaled by ortho depth range |
| Sample | `texture(shadow, vec3(uv, compareDepth))`; UV `xy*0.5+0.5`; compareDepth viewport-normalized (`z*0.5+0.5`) for OpenGL NDC GLM |

## Touch list

- `VulkanDesktop/Gfx/Gfx_LightingMath.h` — verify light view + scene ortho (keep)
- `VulkanDesktop/Shader/PbrShadow.glsl` — remove hacks; keep viewport compare depth
- `VulkanDesktop/Shader/DeferredLighting.frag`, `LightingBindings.glsl`, `TriangleFrag_Lit*.frag`
- `VulkanDesktop/RenderCore/Vk_ShadowMapPass.cpp` — FRONT cull, scaled bias
- Keep validation fixes: `Vk_ResourceTables.cpp`, `Vk_DeferredLightingPass.cpp`

## Steps

1. **Audit** — map diffs vs `39f6ace` / khronos-shadow-map Plan; list regressions  
2. **Shader contract** — strip `Pbr_SunHemisphereIblScale`, frustum reject; restore border-white compare path  
3. **Pass contract** — restore `VK_CULL_MODE_FRONT_BIT`; keep scene-only matrix + scaled bias  
4. **Verify** — `powershell -File Scripts/Verify-CI.ps1`; manual Sponza (atrium sun, fly camera stable, debug shadow view)

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0  
- Manual: Sponza atrium receives directional sun; shadow debug view aligns with sun vector; no validation errors on exit

## Risks

- FRONT cull may show minor back-face leak on thin geo — acceptable per prior Khronos deviation note; revisit with depth bias tuning only if needed
