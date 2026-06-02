# Progress: vk-core-world-peel

- **Plan:** [`vk-core-world-peel_Plan.md`](vk-core-world-peel_Plan.md)
- **Parent:** Active-Plan P1 → [`Archived-Plan.md`](../../Archived-Plan.md) § P1 peel

## Closeout — 2026-06-02

- **Outcome:** `WorldState` + `DebugUIState` owned in **App**; `BuildActiveRenderViews` in App; `PrepareFrameCpu` takes pre-built `Vk_ActiveRenderView[]`; ImGui panels in `Application` (not `DrawFrame`); public `Vk_DeviceContext` / `Vk_SwapchainContext` / `Vk_FrameContext` / `Vk_SceneGpuContext` / `Vk_PlatformContext`; **0** `friend` on `Vk_Core`.
- **Commits:** `24d73b3` (phase 1), `1392869` (phase 2), `e341b0e` (phases 3–4).
- **Verification:** `Scripts/Verify-CI.ps1` exit **0**; `Scripts/Verify-Smoke.ps1` exit **0** after each phase.
- **Peel metrics (#27):** `friend` 9→0; scene CPU getters off `Vk_Core` public API; PiP / secondary camera JSON stays in App (`ActiveViewsBuild`).
- **Intentional debt:** peel modules still take `Vk_Core&`; session camera/env on core; `World()` / `DebugUI()` remain private on core — follow-up tracks: config-platform-hardening, shader-bindless-policy.
