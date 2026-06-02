# Progress: permutation-registry

## Closeout — 2026-06-01

- **Outcome:** `PermutationRegistry.json` + `Gfx_ShaderPermutation`; `lit` / `lit_alpha_clip` SPIR-V; extract sets `myPipelinePermutationId` and encoded sort-key perm slot; `engine.json` / `--shader-permutation` select active variant.
- **Verification:** MSBuild Debug|x64 exit 0; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0; log: `shaderPermutation=lit`, `[SHADER-PERM] active permutation`, `[SCENE] LoadSceneResources completed`, `[APP] Smoke dwell reached`.
- **Deviations:** `Gfx_ShaderPermutation::Initialize` moved after `UtilLogger::Init` so registry lines appear in runtime log.
- **Plan:** [`permutation-registry_Plan.md`](permutation-registry_Plan.md)
