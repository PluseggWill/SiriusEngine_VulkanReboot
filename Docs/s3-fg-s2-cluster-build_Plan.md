# Plan: s3-fg-s2 — ClusterBuild compute stub (slice 2)

**Status:** WIP *(kickoff 2026-06-11)*  
**Parent roadmap:** [`s3-fg-v0_Plan.md`](s3-fg-v0_Plan.md) step 3  
**Depends on:** [`Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md`](Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md) (closed)  
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

## Cluster grid v0 (Progress only)

| Param | Value | Notes |
|-------|-------|-------|
| Screen tile | 16×16 px | `kTileSize` |
| Depth slices | 24 | logarithmic spacing deferred to slice 3 lighting |
| Max lights (stub) | 1 | sun from `GpuEnvironmentData` |
| Max lights / cluster | 8 | SSBO headroom for slice 3 |

Cluster count = `tilesX(extent.w) × tilesY(extent.h) × kDepthSlices`.

## Touch list

| Area | Files |
|------|--------|
| Gfx contract | `Gfx/Gfx_ClusterLighting.h` *(new)* |
| Compute module | `RenderCore/Vk_ClusterBuildPass.h/.cpp` *(new)* |
| Shader | `Shader/ClusterBuild.comp`, `CompileShader_Glslc.bat` |
| Record | `Vk_GBufferPass.cpp` — dispatch between G-buffer and composite |
| Lifecycle | `Vk_Core.h/.cpp`, `Vk_SwapchainHost.cpp` |
| Tests | `GfxTests_Main.cpp` — grid math |
| Build | `VulkanDesktop.vcxproj` + `.filters` |

## Ordered steps

### 1. Gfx cluster constants
- `Gfx_ClusterLighting::TilesForExtent`, `ClusterCount`
- `GpuClusterLight` / `GpuClusterLightList` structs shared with shader comments

### 2. `ClusterBuild.comp`
- Workgroup 64; one invocation per cluster (capped by push `clusterCount`)
- Binding 0: readonly lights SSBO; binding 1: cluster light lists SSBO
- Stub: copy light indices `0..lightCount-1` into every cluster list

### 3. `Vk_ClusterBuildPass`
- Mirror `Vk_GpuCull` pattern: layout, pipeline, per-frame descriptor sets
- Extent-sized cluster-list buffers; host-mapped lights buffer (1 sun)
- `RecordDispatch` after G-buffer RP, before composite

### 4. Lifecycle + log
- Init/Destroy with hybrid scene load; `RecreateForExtent` on swapchain resize
- Update `[FG]` chain log string

### 5. Verification
- `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0
- Manual HybridDeferred: `[FG]` chain includes `ClusterBuild`; `[CLUSTER]` dispatch log once

## Risks

- Cluster buffer size scales with resolution — recreate on resize (same hook as G-buffer)
- Slice 3 must consume cluster SSBO layout — keep structs in `Gfx_ClusterLighting.h`

## References

- Roadmap: [`s3-fg-v0_Plan.md`](s3-fg-v0_Plan.md)
- Prior slice: [`Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md`](Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md)
