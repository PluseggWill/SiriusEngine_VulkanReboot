# Progress: forward-pass-hardening

## Closeout — 2026-06-02

- **Outcome:** Stage 1 epic §B — `Vk_ScenePasses` forward opaque/transparent CONTRACT; Architecture §5.8 Stage 2 depth policy; `Util_RenderDebugPanel` (skip pass, Lit/Depth/Normals debug, preset readout); per-material `alphaMode` mask discard; bindless `GpuMaterialTableEntry.myAlphaMode`; `DrawFrame` order prep → debug panel → env UBO upload → record.
- **Verification:** `.\VulkanDesktop.exe --no-validation --smoke-seconds 6 --scene Data/Scenes/demo.json` exit **0**; log: `[CONFIG] renderPreset=ForwardLit`, `[SCENE] LoadSceneResources completed`, `[APP] Smoke dwell reached (6.000000s)`.
- **Deviations:** optional demo mask material skipped.
- **Plan:** [`forward-pass-hardening_Plan.md`](forward-pass-hardening_Plan.md)
