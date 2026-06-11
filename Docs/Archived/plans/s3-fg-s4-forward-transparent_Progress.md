# Progress: s3-fg-s4-forward-transparent

**Plan:** [`s3-fg-s4-forward-transparent_Plan.md`](s3-fg-s4-forward-transparent_Plan.md)  
**Branch:** `S3`

## Closeout — 2026-06-11

- **Outcome:** HybridDeferred batch path copies G-buffer depth to swapchain, uses `myHybridResolveRenderPass` (depth LOAD), records `ForwardTransparent` after `DeferredLighting`. Chain log includes ForwardTransparent.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 (ForwardLit). Manual: `FORCE_MATERIAL_BATCH=1` + `--render-preset HybridDeferred --scene demo.json` exit 0; log chain + DeferredLighting resolve; no `[ERROR]`.
- **Deviations:** bindless hybrid still ForwardLit fallback; multiview view-0-only unchanged.
