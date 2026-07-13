# Plan: s3-fg-s3 — DeferredLighting resolve (slice 3)

**Status:** Closed (2026-06-11)  
**Parent roadmap:** [`s3-fg-v0_Plan.md`](s3-fg-v0_Plan.md) step 4  
**Depends on:** [`s3-fg-s2-cluster-build_Plan.md`](s3-fg-s2-cluster-build_Plan.md) (closed)  
**Branch:** `S3`

## Problem

Slices 1–2 dogfood hybrid topology but the swapchain still shows raw G-buffer albedo (`CompositeAlbedo`). Slice 3 replaces that stub with a **fullscreen deferred resolve** that reads G-buffer + cluster light lists and writes lit opaque color to swapchain — completing FG v0 opaque chain.

## Goal (slice 3 only)

| # | Deliverable | Done when |
|---|-------------|-----------|
| G1 | `DeferredLighting` shaders | Fullscreen tri; sample albedo + normal/roughness; read cluster SSBO |
| G2 | `Vk_DeferredLightingPass` | Pipeline + per-frame descriptors (G-buffer samplers + cluster SSBOs) |
| G3 | Record order | `GBufferOpaque → ClusterBuild → DeferredLighting` (no CompositeAlbedo on hot path) |
| G4 | Lighting v0 | Blinn-Phong diffuse + ambient via cluster light loop (sun stub); no specular (no world-pos G-buffer) |
| G5 | Pass chain log | Once: `[FG] HybridDeferred: GBufferOpaque -> ClusterBuild -> DeferredLighting` |
| G6 | CI green | `Verify-CI` + `Verify-Smoke` on **ForwardLit** default |

## Non-goals (defer)

- Specular / full forward parity (needs world-position G-buffer or depth reconstruct)
- Depth-slice cluster lookup (screen-tile + slice 0 stub)
- Transparent forward; bindless hybrid; HDR intermediate
- `EngineArchitecture.md` policy lock

## References

- Roadmap: [`s3-fg-v0_Plan.md`](s3-fg-v0_Plan.md)
- Forward shading reference: `TriangleFrag_Lit.frag` (diffuse + ambient)
