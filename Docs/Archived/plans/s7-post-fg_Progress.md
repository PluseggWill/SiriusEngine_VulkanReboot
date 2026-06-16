# Progress: S7 — Post + frame graph v1

## Closeout — 2026-06-15

- **Outcome:** HDR scene target (`R16G16B16A16_SFLOAT`), tonemap (ACES/Reinhard + exposure), optional bloom, `Vk_FrameGraphBuilder` drives HybridDeferred pass order with topo sort and shadow/AO/bloom toggles.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; GfxTests pass; shader drift clean.
- **Deviations:** Lab presets/timestamps deferred; swapchain legacy hybrid RP fields kept unused pending cleanup.
