# frame-pacing-fix — Progress

Plan: [`frame-pacing-fix_Plan.md`](frame-pacing-fix_Plan.md)

## Closeout — 2026-06-12

- **Outcome:** `MAX_FRAMES_IN_FLIGHT` raised to 3 (aligned with 3-image swapchain via `Vk_FrameLimits.h`); frame-stats ring buffer fixed for Avg/1% Low; GPU fence wait history plot added; HybridDeferred prep clamped to view 0 (skip PiP cull waste).
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; `powershell -File Scripts/Verify-Smoke.ps1` exit 0; GfxTests all passed.
- **Deviations:** none.
