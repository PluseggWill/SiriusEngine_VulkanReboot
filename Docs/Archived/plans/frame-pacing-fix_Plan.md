# Frame pacing fix — dense FPS wave / 1% Low drop

**Status:** Closed (2026-06-12)

## Problem

Runtime Performance overlay shows dense wave-shaped frame-ms plots; instantaneous FPS can read 100+ while 1% Low drops to ~40. Root cause: **2-slot GPU fence beat** (`MAX_FRAMES_IN_FLIGHT = 2` vs 3 swapchain images) plus wall-clock sampling at loop start. Secondary: **ring-buffer bug** in Avg / 1% Low aggregates; **HybridDeferred FG v0** still runs PiP view prep/cull though only view 0 is rendered.

## Non-goals

- GPU timestamp queries / separate GPU frame time metric (follow-up).
- Changing default vsync or render preset.
- `EngineArchitecture.md` policy update (no locked contract change).

## Touch list

| Area | Files |
|------|--------|
| Frame in-flight | `Vk_Core.h`, `Vk_ClusterBuildPass.h`, `Vk_DeferredLightingPass.h` |
| Hybrid view prep | `Vk_Core.cpp` |
| Frame stats | `Util_FrameStats.h/.cpp`, `Util_StatsOverlay.cpp` |
| Docs | `Docs/README.md`, `Docs/Active-Plan.md`, close → `Archived/plans/` |

## Steps

1. **Align in-flight frames with swapchain (3)** — `MAX_FRAMES_IN_FLIGHT = 3`; sync cluster/deferred per-frame arrays; invariant `<= swapchain imageCount` unchanged.
2. **Fix frame-stats aggregates** — iterate ring buffer chronologically for Avg FPS and 1% Low.
3. **Overlay diagnostics** — ring history + plot for GPU fence wait (verify pacing fix in-game).
4. **HybridDeferred PiP trim** — clamp active view prep/cull to 1 when G-buffer FG v0 active (matches `RecordFrame` contract).
5. **Verify** — `Scripts/Verify-CI.ps1`; `Scripts/Verify-Smoke.ps1` (runtime path touched).

## Risks

- Slightly higher GPU memory (third frame slot buffers/descriptors) — bounded, matches existing 3-image swapchain.
- Cluster/deferred descriptor arrays must stay in sync with `MAX_FRAMES_IN_FLIGHT`.

## Verification

```powershell
powershell -File Scripts/Verify-CI.ps1
powershell -File Scripts/Verify-Smoke.ps1
```

Manual: run with default `engine.json`; Performance overlay — frame-ms wave amplitude reduced; GPU fence wait plot no longer strong 0/spike alternation; 1% Low closer to Avg.
