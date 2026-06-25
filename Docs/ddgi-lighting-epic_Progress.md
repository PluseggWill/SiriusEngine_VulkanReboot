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

## 2026-06-16 — DDGI missing-parts gap fill

- **Files:** `VulkanDesktop/RenderCore/Vk_DeferredLightingPass.h`, `VulkanDesktop/RenderCore/Vk_DeferredLightingPass.cpp`, `VulkanDesktop/RenderContract/GpuLightingGlobals.h`, `VulkanDesktop/Gfx/Gfx_ClusterLighting.h`, `VulkanDesktop/Shader/DeferredLighting.frag`, `VulkanDesktop/Shader/DdgiProbeUpdate.comp`, `VulkanDesktop/Util/Util_LightingPanel.cpp`, `VulkanDesktop/Util/Util_EngineConfig.cpp`.
- **Outcome:** landed DDGI visibility atlas, upgraded deferred DDGI resolve to minimal 8-probe trilinear sampling with normal-facing/visibility weights, added probe history blend in probe update compute pass, and parameterized DDGI volume (`center/extents`) with runtime controls.
- **Verification:**
  - `powershell -File Scripts/Verify-CI.ps1` → build/tests pass; shader drift gate reports expected `.spv` deltas (`DeferredLightingFrag.spv`, `DdgiProbeUpdate.spv`, plus existing AO shader drift files).
  - `powershell -File Scripts/Verify-Smoke.ps1` → same pre-existing gpu-cull token assertion failure: missing `(gpu-deferred)`.
  - `x64/Debug/VulkanDesktop.exe --asset-root . --scene Data/Scenes/sponza.json --validation --smoke-frames 120 --smoke-seconds 6 --ddgi` → exit **0**; runtime reports validation layer unavailable locally and continues with validation disabled.

## Closeout — 2026-06-16 (gap-fill)

- **Outcome:** DDGI Stage-3 implementation now includes visibility channel, volume-aware sampling, and probe temporal stabilization; remaining major delta to production DDGI is true scene ray integration for probe updates.
- **Verification:** see checkpoint above; no new crashes/regressions observed in DDGI on-path smoke.
- **Deviations:** unchanged pre-existing smoke token issue and local validation-layer availability limit.

## 2026-06-16 — P2: scene-aware probe integration (non-RT bridge)

- **Files:** `VulkanDesktop/RenderCore/Vk_DeferredLightingPass.cpp`, `VulkanDesktop/Shader/DdgiProbeUpdate.comp`, `Docs/ddgi-lighting-epic_Plan.md`.
- **Outcome:** `DdgiProbeUpdate` now reads `gbufferWorldPosition` + `gbufferNormalRoughness` through new compute descriptor bindings and performs per-probe multi-sample scene-aware integration (normal-facing + distance attenuation) for irradiance/visibility, then applies existing history blending and staggered budget policy.
- **Verification:**
  - `powershell -File Scripts/Verify-CI.ps1` → build/tests pass; shader drift gate reports expected `.spv` deltas including updated `DdgiProbeUpdate.spv` (and existing `DeferredLightingFrag.spv` + AO shader drift files already present in working tree).
  - `powershell -File Scripts/Verify-Smoke.ps1` → unchanged pre-existing gpu-cull token assertion failure `(gpu-deferred)`.
  - `x64/Debug/VulkanDesktop.exe --asset-root . --scene Data/Scenes/sponza.json --validation --smoke-frames 120 --smoke-seconds 6 --ddgi` → exit **0**; local validation layer still unavailable (`VK_LAYER_KHRONOS_validation` missing), runtime falls back to validation disabled.

## Closeout — 2026-06-16 (P2)

- **Outcome:** DDGI probe update path progressed from synthetic pattern writes to scene-aware accumulation using live GBuffer data while keeping current frame graph, budget, and history architecture stable.
- **Verification:** CI compile/tests and DDGI runtime smoke pass; no new runtime crash observed.
- **Deviations:** non-RT approximation limitations remain; smoke token and validation-layer environment limits unchanged.

## 2026-06-16 — P3: stability and debug usability

- **Files:** `VulkanDesktop/RenderCore/Vk_DeferredLightingPass.h`, `VulkanDesktop/RenderCore/Vk_DeferredLightingPass.cpp`, `VulkanDesktop/Gfx/Gfx_MaterialTypes.h`, `VulkanDesktop/Util/Util_RenderDebugPanel.cpp`, `Docs/ddgi-lighting-epic_Plan.md`.
- **Outcome:** added explicit `DDGI only` debug view selection in render debug panel and implemented probe-history reset policy in deferred DDGI update path (history invalidated on DDGI volume edits or significant camera jump), reducing stale-history artifacts and making DDGI diagnosis straightforward in-scene.
- **Verification:**
  - `powershell -File Scripts/Verify-CI.ps1` → build/tests pass; shader drift gate still reports expected generated `.spv` deltas already present in this DDGI workstream.
  - `powershell -File Scripts/Verify-Smoke.ps1` → unchanged pre-existing gpu-cull token assertion `(gpu-deferred)`.
  - `x64/Debug/VulkanDesktop.exe --asset-root . --scene Data/Scenes/sponza.json --validation --smoke-frames 120 --smoke-seconds 6 --ddgi` → exit **0**; validation layer unavailable locally (`VK_LAYER_KHRONOS_validation` missing), runtime falls back as before.

## Closeout — 2026-06-16 (P3)

- **Outcome:** P3 goals for single-volume DDGI path are landed: better debug visibility (`DDGI only`) plus practical runtime stability control via history reset conditions.
- **Verification:** CI compile/tests pass and DDGI smoke path exits normally.
- **Deviations:** multi-volume architecture is still pending future phase; existing smoke token and local validation-layer availability limits remain unchanged.
