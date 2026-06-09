# Progress: rhi-swapchain-create

## Closeout — 2026-06-09

- **Outcome:** Khronos-style `compositeAlpha` fallback, triple-buffer image count (`max(min, 3)`), create log, `NeedsSwapchainRebuild` extent precheck in `Recreate`.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0; log: `imageCount=3 compositeAlpha=OPAQUE extent=1600x1200`.
- **Deviations:** B1-V2 manual resize soak not run; full recreate still on suboptimal path (B2 will split layers).
