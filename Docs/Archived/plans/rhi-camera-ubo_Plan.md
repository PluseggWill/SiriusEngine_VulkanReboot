# Plan: rhi-camera-ubo (RHI-A2)

**Status:** Closed (2026-06-09)  
**Parent:** [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md) §RHI-A2 · [`Active-Plan.md`](Active-Plan.md) P1  
**Hardening:** #34

## Problem

`PrepareFrameCpu` uploads per-view camera UBO via `UpdateForView`. `DrawFrameGpu` calls `Update`, which re-writes view 0 from `Vk_Core::myCamera`, diverging from `aViews[0].myCamera` used during draw prep when matrices differ.

## Non-goals

- Changing multi-view routing or PiP UI
- `EngineArchitecture.md` policy edits

## Touch list

| File | Change |
|------|--------|
| `Vk_FrameUniformUploader.h/.cpp` | Add `UpdateEnvironment`; `Update` env-only |
| `Vk_Core.cpp` | `DrawFrameGpu` calls `UpdateEnvironment` |
| `Util_RenderDebugPanel.h` | Comment contract |

## Steps

1. Extract env upload to `UpdateEnvironment` (`myViewWorldPos` from fly `myCamera.myEye`).
2. Remove `UpdateForView(0, myCamera)` from `Update`; `Update` delegates to `UpdateEnvironment`.
3. `DrawFrameGpu` calls `UpdateEnvironment` after debug panels patch `myEnvironmentData`.

## Verification

| ID | Criterion |
|----|-----------|
| A2-V1 | `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 |
| A2-V2 | Manual: `demo.json` PiP — view 0 stable when fly ≠ scene overview camera |

## Risks

- Low — camera upload path unchanged in `PrepareFrameCpu`.
