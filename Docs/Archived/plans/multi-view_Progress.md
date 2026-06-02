# Progress: multi-view

## Closeout — 2026-06-02

- **Outcome:** Re-landed `Gfx_RenderView` v1 (swapchain viewport only) + per-view extract/record; scene `cameras[]`; fly primary + PiP scene camera; ImGui **Multi-view** panel; `demo.json` `overview` camera. Post-landing review fixed PiP-only flicker by isolating per-view instance-slab ranges and preventing PiP LOD hysteresis writes from mutating main-view state.
- **Verification:** Build `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64` exit **0**; smoke `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit **0**; log: `[SCENE] Parsed scene ... cameras=1`, `[EXTRACT] entities=9 draws=9`, `[PERF] ... drawCalls=18` (2 views).
- **Deviations:** v1 uses swapchain only (no offscreen RT); shared depth across PiP (debug-grade).
- **Plan:** [`multi-view_Plan.md`](multi-view_Plan.md)
