# Plan: s3-fg-s4 — ForwardTransparent over deferred depth (slice 4)

**Status:** Closed (2026-06-11)  
**Parent:** [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §A · [`Active-Plan.md`](Active-Plan.md) § S3 hybrid follow-ups  
**Depends on:** [`Archived/plans/s3-fg-s3-deferred-lighting_Plan.md`](Archived/plans/s3-fg-s3-deferred-lighting_Plan.md) (closed)  
**Branch:** `S3`

## Problem

FG v0 opaque chain is complete but hybrid path **skips transparent draws** and clears swapchain depth — demo overlay monkey is invisible under `HybridDeferred`.

## Goal (slice 4 only)

| # | Deliverable | Done when |
|---|-------------|-----------|
| G1 | G-buffer depth persist | Depth `STORE`; transfer-src usage on G-buffer depth |
| G2 | Depth copy + hybrid RP | Copy G-buffer depth → swapchain depth; resolve RP depth `LOAD` |
| G3 | `ForwardTransparent` record | After `DeferredLighting`, batch transparent draws on swapchain (depth test on / write off) |
| G4 | Pass chain log | Once: `GBufferOpaque -> ClusterBuild -> DeferredLighting -> ForwardTransparent` |
| G5 | CI green | `Verify-CI` + `Verify-Smoke` on **ForwardLit** default |

## Non-goals (defer)

- Bindless hybrid path (still ForwardLit fallback)
- Full forward lighting parity / specular / world-position G-buffer
- HDR intermediate; separate transparent-only render pass module
- `EngineArchitecture.md` policy lock

## Touch list

| Area | Files |
|------|-------|
| Depth / RP | `Vk_GBufferPass.cpp`, `Vk_SwapchainHost.cpp`, `Vk_SwapchainContext.h` |
| Record | `Vk_ScenePasses.cpp`, `Vk_ScenePasses.h` |
| Tests | `GfxTests_Main.cpp` (optional chain helper) |

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- `powershell -File Scripts/Verify-Smoke.ps1` exit 0
- Manual: `--render-preset HybridDeferred` → transparent overlay visible; log chain once

## Risks

- Depth copy barriers must run **outside** swapchain RP; hybrid RP depth `LOAD` required
- Resize: G-buffer + swapchain depth recreated together (existing B2 path)
