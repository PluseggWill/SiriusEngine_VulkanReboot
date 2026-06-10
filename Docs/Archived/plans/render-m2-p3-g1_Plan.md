# Plan: render-m2-p3-g1 — automated CPU vs GPU cull parity (G1)

**Status:** Closed (2026-06-10)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P3** (G1 gate)  
**Depends on:** [`Archived/plans/render-m2-p3-c_Plan.md`](Archived/plans/render-m2-p3-c_Plan.md)

## Goal

**G1:** GfxTests compares CPU frustum-cull visible entity slots vs CPU reference of `EntityCull.comp` rules (fixed cameras). Runs in `Verify-CI.ps1` — no Vulkan GPU readback.

## Non-goals

- GPU buffer readback / integration test exe
- Compaction, LOD GPU parity
- `EngineArchitecture.md` policy change (CPU remains source of truth until this job is green — now green)

## Touch list

| Area | Files |
|------|--------|
| Gfx reference | `Gfx_GpuCull.h`, new `Gfx_GpuCull.cpp` |
| Tests | `GfxTests_Main.cpp`, `GfxTests.vcxproj` |
| Build | `VulkanDesktop.vcxproj` (+ filters) |

## Steps

1. `Gfx_IsEntitySlotVisibleForGpuCull` — mirror EntityCull.comp (layer + frustum, inactive slot = culled).
2. `Gfx_CollectCpuCullVisibleSlots` / `Gfx_CollectGpuCullVisibleSlots` + equality helper.
3. GfxTests: demo overview (all visible), culled camera, layer-mask reject.
4. Wire `Gfx_GpuCull.cpp` into GfxTests + VulkanDesktop projects.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0 (GfxTests parity cases)
- `Verify-Smoke.ps1` exit 0 (unchanged default path)

## Risks

- Demo SoA without mesh bounds uses unit AABB — parity still valid; stress scene not required for G1 v0.
