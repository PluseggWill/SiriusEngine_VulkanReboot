# Progress: rhi-camera-ubo

## Closeout — 2026-06-09

- **Outcome:** Camera UBO slices upload only in `PrepareFrameCpu` (`UpdateForView`). `DrawFrameGpu` calls `UpdateEnvironment` for env slice; `Update` is env-only delegate. `myViewWorldPos` still uses fly camera eye.
- **Verification:** `Scripts/Verify-CI.ps1` exit 0; `Scripts/Verify-Smoke.ps1` exit 0; log: `[APP] Engine exited run loop normally`.
- **Deviations:** A2-V2 manual PiP (`demo.json`) not run this session.
