# Progress: forward-stage1-contracts

## Closeout — 2026-06-01

- **Outcome:** Stage 1 epic §A — frozen `GpuMaterialParams` / `GpuMaterialTableEntry` + GLSL layouts; scene JSON optional PBR fields; `ForwardLit` render preset → permutation `lit` via `engine.json` / `--render-preset`; `Gfx_ShaderFeature_Pbr` reserved; docs in `EngineArchitecture.md` §5.10 and `SceneJSON.md`.
- **Verification:** `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit **0**; log: `[CONFIG] renderPreset=ForwardLit`, `[SHADER-PERM] active permutation name=lit`, `[SCENE] LoadSceneResources completed`.
- **Deviations:** none.
- **Plan:** [`forward-stage1-contracts_Plan.md`](forward-stage1-contracts_Plan.md)
