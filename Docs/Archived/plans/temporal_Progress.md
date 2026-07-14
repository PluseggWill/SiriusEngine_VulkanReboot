# Progress: temporal

## Closeout — 2026-07-14
- **Outcome:** S9 / G5 closed. Shared `Vk_TemporalState` (Halton jitter + reset), G-buffer MV, TAA v0.5, AO temporal on shared MV, SSR on shared `prevViewProj` + lifetime. History-weight debug view deferred (optional).
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1 -SkipGpuCull` exit 0; `--validation` requested but `VK_LAYER_KHRONOS_validation` missing on this machine (graceful disable, exit 0, no `[VULKAN-VALIDATION]` lines). Stress HybridDeferred Bindless path green.
- **Deviations:** SSR does not sample surface MV (hit-world reprojection kept — documented). Machine lacks Khronos validation layer → G0-validation N/A beyond soft check.

## 2026-07-13 — S9.1 temporal skeleton and jitter
- **Files:** `Gfx/Gfx_TemporalJitter.h`, `RenderCore/Vk_Temporal*`, `Util/Util_TemporalPanel.*`, …
- **Notes:** Halton(2,3) jitter; shared history reset; Engine Debug → Temporal tab.

## 2026-07-13 — S9.2 motion vectors baseline
- **Notes:** G-buffer RG16F MV; deferred debug view; `GpuCameraData` prev/curr VP.

## 2026-07-13 — S9.3 TAA v0.5
- **Notes:** Catmull-Rom + YCoCg variance clip + velocity deadzone + sharpen; ImGui tunables.

## 2026-07-14 — S9.4 shared temporal consumers
- **Notes:** Shared reset clears TAA/AO/SSR ready flags; AO uses G-buffer MV; SSR uses shared prev VP.
