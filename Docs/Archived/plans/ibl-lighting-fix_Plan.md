# Plan: ibl-lighting-fix — IBL energy, sun tuning, camera far

**Status:** Closed (2026-07-13)  
**Progress:** [`ibl-lighting-fix_Progress.md`](ibl-lighting-fix_Progress.md)

## Problem

Diffuse IBL is ~π too bright (`Pbr_SampleIrradiance` multiplies baked E/π by π again). Deferred ambient fallback when IBL off is dead code. `iblSpecularShadowMin` is a redundant patch. Sun direct light is weak vs IBL. Camera far plane too short for large scenes. No ImGui sun intensity control.

## Goals

1. Fix diffuse IBL energy: baked map = cosine-weighted average radiance → `diffuse = sample * kD * albedo` (no extra π).
2. Deferred: IBL-off path uses `ambientColor * albedo`; remove dead `ambient` churn.
3. Remove `iblSpecularShadowMin` / `Pbr_IblSpecularShadowScale` / dead `Pbr_EvalIbl`; specular IBL uses `sunShadow` directly.
4. Sun intensity via `sunlightColor.w` (default 2.5); ImGui slider; cluster + forward apply.
5. Raise camera far: default 256; scene spawn min 128, scale `lookDistance * 20`.

## Non-goals

- Re-bake IBL assets
- Shadow map / CSM changes
- Diffuse IBL shadow coupling (shadow-audit-fix contract stands)
- EngineArchitecture policy edits

## Touch list

- `VulkanDesktop/Shader/PbrIbl.glsl`
- `VulkanDesktop/Shader/PbrShadow.glsl`
- `VulkanDesktop/Shader/LightingBindings.glsl`
- `VulkanDesktop/Shader/DeferredLighting.frag`
- `VulkanDesktop/Shader/TriangleFrag_Lit*.frag`
- `VulkanDesktop/RenderContract/GpuLightingGlobals.h`
- `VulkanDesktop/Util/Util_LightingPanel.cpp`
- `VulkanDesktop/Util/Util_TuningPrefs.{h,cpp}`
- `VulkanDesktop/App/SceneCpuLoad.cpp`
- `VulkanDesktop/RenderCore/Vk_Camera.cpp`
- `VulkanDesktop/RenderCore/Vk_ClusterBuildPass.cpp`
- `Shader_Generated/*` (recompile)

## Steps

1. Shader contract: irradiance sample, remove spec-shadow-min patch, simplify deferred ambient path.
2. CPU: remove `myIblSpecularShadowMin`, sun intensity defaults, far plane, cluster sun scale.
3. ImGui + tuning prefs cleanup.
4. Recompile shaders; `Verify-CI.ps1`.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- Manual: Sponza — shadowed dielectric less blown-out; sun slider brightens direct only; far clip reduced on large scenes

## Risks

- Removing π scale lowers overall ambient — compensated by higher default sun intensity
- Old configs with `iblSpecularShadowMin` ignored harmlessly
