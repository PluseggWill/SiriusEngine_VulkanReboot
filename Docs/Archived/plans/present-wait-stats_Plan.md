# Present wait stats — Work ms / Present wait ms overlay

**Status:** Closed (2026-06-12)

## Problem

With vsync FIFO, wall-clock **Frame ms** mixes render work and **present blocking**; overlay misreads pacing spikes as GPU slowness. Need separate present wait timing and vsync-aware breakdown in Performance panel.

## Non-goals

- GPU timestamp queries.
- Changing 1% Low to use Work ms (follow-up).
- `EngineArchitecture.md` update.

## Touch list

| Area | Files |
|------|--------|
| Present timing | `Vk_SwapchainHost.cpp/.h` |
| Frame stats | `Util_FrameStats.h/.cpp` |
| Overlay | `Util_StatsOverlay.cpp/.h`, `DebugOverlay.cpp` |
| Frame loop | `Vk_Core.cpp` |

## Steps

1. Measure `vkQueuePresentKHR` wall time in `SubmitAndPresent` (out param).
2. `Util_FrameStats`: pending Work / Present / Fence; commit to ring on `PushFrameTime` (align with Frame ms).
3. Overlay: when `myVsyncFifo`, show Work ms + Present wait ms + plots; note Frame ms includes both.
4. Verify — `Verify-CI.ps1`, `Verify-Smoke.ps1`.

## Risks

- Work ms = loop start → present call (includes fence/acquire/CPU/submit), not GPU-only — document in overlay hint.
