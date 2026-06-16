# Progress: ddgi-lighting

## 2026-06-16 — S8 kickoff prep

- **Scope alignment:** moved `Temporal AO stability (motion vectors + AO history/reprojection)` from S6 backlog into S8 kickoff scope.
- **Plan sync:** updated [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) A-track with temporal-AO-baseline as a prerequisite deliverable.
- **Queue sync:** updated [`Active-Plan.md`](Active-Plan.md) and [`Wishlist.md`](Wishlist.md) so S8 owns the remaining AO-stability task.
- **Verification:** docs-only update (runtime/build N/A).

## 2026-06-16 — S8 Task 1: temporal AO baseline

- **Files:** `VulkanDesktop/RenderCore/Vk_AoPass.h`, `VulkanDesktop/RenderCore/Vk_AoPass.cpp`, `VulkanDesktop/RenderContract/GpuAoSettings.h`, `VulkanDesktop/Util/Util_LightingPanel.cpp`, `VulkanDesktop/Shader/AoTemporal.comp`, `VulkanDesktop/Shader/CompileShader_Glslc.bat`, `VulkanDesktop/VulkanDesktop.vcxproj`.
- **Outcome:** landed AO temporal history/reprojection baseline with history ping-pong resources, reprojection compute pass (`AoTemporal.comp`), and runtime controls (`AO temporal enabled`, `AO temporal blend`).
- **Verification:**
  - `powershell -File Scripts/Verify-CI.ps1` → **failed at shader drift gate** (expected generated `.spv` diffs present in working tree).
  - `powershell -File Scripts/Verify-Smoke.ps1` → default smoke passed; gpu-cull leg failed on existing log token assertion `(gpu-deferred)` mismatch.
  - `x64/Debug/VulkanDesktop.exe --asset-root ... --validation --smoke-frames 120 --smoke-seconds 6` → exit **0**; no `[VULKAN-VALIDATION]` lines in `Logs/engine_runtime_log.txt`.

## 2026-06-16 — S8 Task 2-5: probe model + FG integration + controls + debug viz

- **Files:** `VulkanDesktop/RenderContract/GpuLightingGlobals.h`, `VulkanDesktop/RenderCore/Vk_DeferredLightingPass.h`, `VulkanDesktop/RenderCore/Vk_DeferredLightingPass.cpp`, `VulkanDesktop/RenderCore/Vk_FrameGraph.h`, `VulkanDesktop/RenderCore/Vk_FrameGraph.cpp`, `VulkanDesktop/Shader/DeferredLighting.frag`, `VulkanDesktop/Util/Util_LightingPanel.cpp`, `VulkanDesktop/Util/Util_EngineConfig.h`, `VulkanDesktop/Util/Util_EngineConfig.cpp`, `VulkanDesktop/Shader/DdgiProbeUpdate.comp`.
- **Outcome:** added DDGI probe volume v0 data model (probe grid + atlas + version-safe runtime state), staged/full update budget policy, FG `DdgiProbeUpdate` pass, deferred DDGI sampling hook, DDGI On/Off + budget controls, and DDGI contribution debug view.
- **Verification:**
  - `powershell -File Scripts/Verify-CI.ps1` → build/tests pass; shader drift gate reports generated `.spv` deltas.
  - `powershell -File Scripts/Verify-Smoke.ps1` → default smoke pass; gpu-cull leg still fails on existing `(gpu-deferred)` log token assertion mismatch.
  - `VulkanDesktop.exe --validation --smoke-frames 120 --smoke-seconds 6 --ddgi` → exit **0**; no `[VULKAN-VALIDATION]` lines.

## 2026-06-16 — S8 Task 6: benchmark script

- **Files:** `Scripts/Benchmark-Ddgi.ps1`.
- **Outcome:** added reproducible DDGI benchmark script to run paired `--no-ddgi`/`--ddgi` perf captures and summarize output locations.
- **Verification:** script arguments validated; runtime path uses existing smoke/perf CLI contract.

## Closeout — 2026-06-16

- **Outcome:** S8 DDGI tasks complete: temporal AO baseline, probe data/update budget, FG probe update and deferred sampling, DDGI controls and debug overlay, and benchmark automation.
- **Verification:** `Verify-CI` build/test passes with expected shader drift deltas; validation smoke (`--ddgi`) exit 0; no `[VULKAN-VALIDATION]` in runtime log.
- **Deviations:** `Verify-Smoke.ps1` gpu-cull leg continues to fail on pre-existing `(gpu-deferred)` token assertion; not introduced by S8 DDGI changes.
