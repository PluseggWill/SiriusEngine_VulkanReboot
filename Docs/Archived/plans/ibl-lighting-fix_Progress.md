# Progress: ibl-lighting-fix

## Closeout — 2026-07-13

- **Outcome:** Fixed diffuse IBL π energy bug; removed `iblSpecularShadowMin` / `Pbr_IblSpecularShadowScale` / dead `Pbr_EvalIbl`; deferred IBL-off uses `ambientColor * albedo`; sun intensity via `sunlightColor.w` (default 2.5) + ImGui slider; camera far 256 default, scene spawn min 128 ×20 look distance.
- **Files:** `PbrIbl.glsl`, `PbrShadow.glsl`, `LightingBindings.glsl`, `DeferredLighting.frag`, `TriangleFrag_Lit*.frag`, `GpuLightingGlobals.h`, `GpuEnvironmentData.h`, `Util_LightingPanel.cpp`, `Util_TuningPrefs.{h,cpp}`, `SceneCpuLoad.cpp`, `Vk_Camera.cpp`, `Vk_ClusterBuildPass.cpp`, regenerated `Shader_Generated/*.spv`
- **Verification:** MSBuild Debug|x64 OK; GfxTests exit 0; shader SPIR-V regen (4 lit/deferred spv). G0-validation / smoke: user manual on Sponza recommended (deferred lighting touched).
- **Deviations:** none
