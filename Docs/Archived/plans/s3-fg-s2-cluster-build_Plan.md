# Plan: s3-fg-s2 — ClusterBuild compute stub (slice 2)

**Status:** Closed (2026-06-11)  
**Parent roadmap:** [`s3-fg-v0_Plan.md`](../../s3-fg-v0_Plan.md) step 3  
**Depends on:** [`s3-fg-s1-preset-gbuffer_Plan.md`](s3-fg-s1-preset-gbuffer_Plan.md) (closed)  
**Branch:** `S3`

## Problem

Slice 1 composites raw G-buffer albedo. Deferred lighting needs a **per-cluster light index list** built on GPU before `DeferredLighting` (slice 3). Slice 2 adds the compute pass shell and SSBO outputs without changing the visible image (still `CompositeAlbedo`).

## Goal (slice 2 only)

| # | Deliverable | Done when |
|---|-------------|-----------|
| G1 | Cluster grid constants | `Gfx_ClusterLighting` tile/slice math; GfxTests grid count |
| G2 | `ClusterBuild.comp` + pipeline | Dispatch fills per-cluster light lists |
| G3 | `Vk_ClusterBuildPass` module | Init/Destroy/RecreateForExtent; lights + cluster-list SSBOs |
| G4 | Record order | `GBufferOpaque → ClusterBuild → CompositeAlbedo` on hybrid path |
| G5 | Pass chain log | Once: `[FG] … GBufferOpaque -> ClusterBuild -> CompositeAlbedo (DeferredLighting deferred)` |
| G6 | CI green | `Verify-CI` + `Verify-Smoke` on **ForwardLit** default |

## Non-goals (defer to slice 3+)

- `DeferredLighting` resolve; composite still samples albedo RT0
- Frustum-accurate cluster AABB vs light bounds (stub assigns all lights to all clusters)
- Point/spot lights; multiple lights beyond sun stub
- Transparent pass; bindless hybrid path (unchanged from slice 1)
- `EngineArchitecture.md` policy lock

## References

- Roadmap: [`s3-fg-v0_Plan.md`](../../s3-fg-v0_Plan.md)
- Prior slice: [`s3-fg-s1-preset-gbuffer_Plan.md`](s3-fg-s1-preset-gbuffer_Plan.md)
