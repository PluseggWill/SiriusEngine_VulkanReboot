# Progress: multi-view

## Closeout — 2026-06-01

- **Outcome:** `Gfx_RenderView` + per-view extract/record; scene `cameras[]`; fly primary + PiP scene camera; ImGui **Multi-view** panel; `demo.json` `overview` camera.
- **Verification:** `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit **0**; log: `[MULTI-VIEW] activeViews=2`, `[SCENE] cameras=1`, `drawCalls=18` (2 views).
- **Deviations:** v1 uses swapchain only (no offscreen RT); shared depth across PiP (debug-grade).
- **Plan:** [`multi-view_Plan.md`](multi-view_Plan.md)
