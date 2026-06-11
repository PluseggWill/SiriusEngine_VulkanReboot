# Plan: render-m2-p3-close — P3 phase close (LOD GPU parity)

**Status:** Closed (2026-06-11)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P3** (remaining lines)  
**Depends on:** [`Archived/plans/render-m2-p3-g1_Plan.md`](Archived/plans/render-m2-p3-g1_Plan.md) (G1)

## Goal

Close **P3 — M2 GPU cull**: LOD-aware entity-record fill for `--gpu-cull` path when `lodEnabled`; GfxTests parity. Defer optional compaction to Wishlist.

## Non-goals

- GPU compaction pass (Wishlist S3)
- LOD in `EntityCull.comp` compute shader
- Multi-view per-view entity-record LOD (primary-view eye only)
- `EngineArchitecture.md` policy change

## Touch list

| Area | Files |
|------|--------|
| LOD resolve | `Gfx_Lod.h/.cpp` |
| Entity record | `Gfx_EntityGpuRecord.h/.cpp`, `Vk_FrameDrawPrep.*`, `Vk_Core.cpp` |
| Tests | `GfxTests_Main.cpp` |

## Steps

1. `Gfx_ResolveLodMeshIdForSlot` + `Gfx_ResolveEntityRecordMeshId` (lod-state snapshot).
2. `FillEntityRecords` uses resolved mesh `myIndexCount` when `lodEnabled` (primary-view eye).
3. GfxTests: CPU `Gfx_ApplyLodToFrameExtract` vs entity-record LOD resolve.
4. Archive P3; optional compaction → Wishlist only.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0 (`TestLodGpuEntityRecordParity`)
- `powershell -File Scripts/Verify-Smoke.ps1` exit 0

## Risks

- Entity-record LOD uses view-0 eye before per-view `Build`; matches single-view / primary camera dogfood.
