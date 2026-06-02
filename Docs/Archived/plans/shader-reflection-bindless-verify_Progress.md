# Progress: shader-reflection-bindless-verify

## Closeout — 2026-06-01

- **Outcome:** Offline `lit_bindless` SPIR-V reflection + MSBuild contract (`DescriptorContract_LitBindless.json` → `reflection_lit_bindless.json`); runtime `VerifyLitBindlessReflectionContract()` on bindless material path.
- **Verification:** MSBuild Debug|x64 exit 0; `[SHADER-REFLECT] lit_bindless OK`; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0; log: `[SCENE] LoadSceneResources completed`, `[APP] Smoke dwell reached`, `[SCENE] UnloadScene: GPU scene resources released`. (This GPU used `materialPath=Batch`; bindless verify runs when indexing path selects Bindless.)
- **Deviations:** none
- **Plan:** [`shader-reflection-bindless-verify_Plan.md`](shader-reflection-bindless-verify_Plan.md)
