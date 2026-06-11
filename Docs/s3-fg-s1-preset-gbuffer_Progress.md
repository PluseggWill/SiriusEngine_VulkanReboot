# Progress: s3-fg-s1-preset-gbuffer

**Plan:** [`s3-fg-s1-preset-gbuffer_Plan.md`](s3-fg-s1-preset-gbuffer_Plan.md)  
**Branch:** `S3`

## 2026-06-11 — Kickoff

- **Scope:** Slice 1 — HybridDeferred preset + GBufferOpaque MRT + albedo composite; no ClusterBuild/DeferredLighting.
- **G-buffer format:** RT0 RGBA8 albedo, RT1 RGBA16F normal+roughness, dedicated depth (documented in Plan).
- **Next:** Steps 1–6 implementation.

## 2026-06-11 — Steps 1–6 implementation

- **Files:** `Gfx_RenderPreset.*`, `Vk_GBufferPass.*`, `Vk_ScenePasses.*`, `Vk_Core.*`, `Vk_SwapchainHost.cpp`, GBuffer/CompositeAlbedo shaders + `.spv`, GfxTests, `CLI.md`, vcxproj.
- **Fix:** `RecreateForExtent` no longer gates on `myInitialized` (Init was marking initialized without creating RTs).
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 (ForwardLit default). Manual: `FORCE_MATERIAL_BATCH=1` + `--render-preset HybridDeferred` → exit 0, `[FG]` chain log, `drawCalls=21`.
- **Deviation:** Bindless default → forward fallback (planned); hybrid G-buffer path requires batch (`Docs/Platform.md` `FORCE_MATERIAL_BATCH=1` for manual dogfood).

## 2026-06-11 — Review polish

- **Refactor:** `RebuildResources` vs `RecreateForExtent` — resize skips work when hybrid never inited (ForwardLit default).
- **Comments:** module header, preset/G-buffer split, pipeline override, CLI dogfood note.
- **Fix:** removed erroneous `(void)aDebugUI` in `RecordForwardLit`.
