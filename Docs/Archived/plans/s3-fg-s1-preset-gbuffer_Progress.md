# Progress: s3-fg-s1-preset-gbuffer

**Plan:** [`s3-fg-s1-preset-gbuffer_Plan.md`](s3-fg-s1-preset-gbuffer_Plan.md)  
**Branch:** `S3`

## Closeout — 2026-06-11

- **Outcome:** `HybridDeferred` preset + offscreen G-buffer MRT (albedo + normal/roughness) + `CompositeAlbedo` to swapchain; `ForwardLit` default unchanged. `Vk_GBufferPass` lifecycle on scene load/unload and swapchain resize.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 (ForwardLit). Manual: `FORCE_MATERIAL_BATCH=1` + `--render-preset HybridDeferred` → exit 0, `[FG]` chain log, `drawCalls=21`. Commit `2e23764`.
- **Deviations:** Bindless + hybrid → forward fallback (batch-only v1); transparent pass skipped; multi-view uses index 0 only.
