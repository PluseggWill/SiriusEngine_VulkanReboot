# Plan: s3-fg-s6 — Opaque forward parity (specular v0, slice 6)

**Status:** Closed (2026-06-11)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) § S3 hybrid follow-ups  
**Depends on:** [`Archived/plans/s3-fg-s5-bindless-hybrid_Plan.md`](s3-fg-s5-bindless-hybrid_Plan.md) (closing)  
**Branch:** `S3`

## Problem

`DeferredLighting` is diffuse + ambient only; hybrid opaque looks flat vs `ForwardLit` Blinn-Phong (specular missing).

## Goal (slice 6 v0)

| # | Deliverable | Done when |
|---|-------------|-----------|
| G1 | G-buffer depth sample | Depth `SAMPLED` usage; bound in deferred pass |
| G2 | World position | Reconstruct from depth + `invViewProj` push constant |
| G3 | Specular term | Match forward `fogDistances.xy` strength/shininess for sun light |
| G4 | CI green | `Verify-CI` + `Verify-Smoke` |

## Non-goals

- Full PBR / metallic workflow
- Parity checklist doc / perf A-B (epic §D)
- `EngineArchitecture.md` policy lock

## Touch list

`Gfx_ClusterLighting.h`, `DeferredLighting.frag`, `Vk_DeferredLightingPass.cpp`, `Vk_GBufferPass.cpp`

## Verification

- `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0
