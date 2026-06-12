# Progress: khronos-shadow-map

## Closeout — 2026-06-12

- **Outcome:** Directional shadow map aligned with Khronos Vulkan-Samples (`multithreading_render_passes` / `shadows/*`): scene-load ortho cache, `vulkan_style_projection(ortho) * lightView`, raster depth bias `(-1.4, 0, -1.7)`, `GREATER_OR_EQUAL` compare + border white, single-sample hardware PCF in `PbrIbl.glsl`.
- **Files:** `Gfx_LightingMath.h`, `Vk_ShadowMapPass.*`, `Vk_Core.cpp`, `Vk_FrameUniformUploader.cpp`, `Gfx_LightingGlobals.h`, `PbrIbl.glsl`, generated lit/deferred SPIR-V.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` — MSBuild Debug|x64 exit 0; GfxTests exit 0; shader drift until SPIR-V committed (expected after GLSL change). Manual: RenderDoc ShadowMap pass — VS vertex input bound, depth fills scene, `lightViewProj` stable when fly camera moves.
- **Deviations:** Kept `VK_CULL_MODE_FRONT_BIT` (Z-up sun) vs Khronos back/none; ortho recomputed on scene load only (sun direction fixed from env at load).
