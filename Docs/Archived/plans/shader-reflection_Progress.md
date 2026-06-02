# Progress: shader-reflection

## Closeout — 2026-06-01

- **Outcome:** Phase **2a** complete — `ShaderReflect` offline tool reflects `TriangleVert.spv` + `TrianglePix.spv`, writes `Shader_Generated/reflection_lit.json`, validates vs `Shader/DescriptorContract_LitBatch.json` on MSBuild. Runtime `Vk_DescriptorSystem` unchanged. Active-Plan **2b/2d** remain open.
- **Verification:** MSBuild `Debug|x64` exit **0**; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit **0**; `shader_compile_log.txt`: `[SHADER-REFLECT] lit_batch OK`; runtime: `[SCENE] LoadSceneResources completed`, `UnloadScene: GPU scene resources released`.
- **Deviations:** CLI uses `path@stage` instead of `path:stage` (Windows drive colon); contract `name` fields informational only (SPIR-V block names may differ in case).
- **Plan:** [`shader-reflection_Plan.md`](shader-reflection_Plan.md) (this folder)
