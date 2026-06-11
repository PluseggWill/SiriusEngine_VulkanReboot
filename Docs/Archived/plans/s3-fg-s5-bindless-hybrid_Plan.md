# Plan: s3-fg-s5 — Bindless HybridDeferred path (slice 5)

**Status:** Closed (2026-06-11)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) § S3 hybrid follow-ups · [`shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance  
**Depends on:** [`Archived/plans/s3-fg-s4-forward-transparent_Plan.md`](Archived/plans/s3-fg-s4-forward-transparent_Plan.md) (closed)  
**Branch:** `S3`

## Problem

`HybridDeferred` on default **bindless** GPUs falls back to `ForwardLit` — smoke/CI never exercises the hybrid chain on the primary material path.

## Goal (slice 5 only)

| # | Deliverable | Done when |
|---|-------------|-----------|
| G1 | `GBufferFrag_Bindless.frag` | Bindless Set 1; MRT albedo + normal/roughness |
| G2 | `GBuffer.vert` | Pass `materialIndex` to frag (batch frag ignores) |
| G3 | `myGBufferPipelineBindless` | Built with `myBindlessPipelineLayout` |
| G4 | Record path | `RecordFrame` uses hybrid chain for bindless; no ForwardLit fallback |
| G5 | M8 parity | `RecordOpaque/TransparentPacketDraws` keyed by `Vk_RenderMaterialPath` |
| G6 | CI green | `Verify-CI` + `Verify-Smoke` (bindless default on capable GPU) |

## Non-goals

- Forward parity / specular / world-position deferred
- New descriptor contract JSON (reuse lit_bindless Set 1)
- `EngineArchitecture.md` policy lock

## Touch list

| Area | Files |
|------|-------|
| Shaders | `GBuffer.vert`, `GBufferFrag_Bindless.frag`, `CompileShader_Glslc.bat` |
| Pass | `Vk_GBufferPass.*`, `Vk_ScenePasses.*` |
| Build | `VulkanDesktop.vcxproj`, `.filters` |

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- `powershell -File Scripts/Verify-Smoke.ps1` exit 0
- Manual: `--render-preset HybridDeferred` without `FORCE_MATERIAL_BATCH` → no bindless fallback log

## Risks

- GBuffer vert must stay compatible with batch + bindless frags (shared `GBufferVert.spv`)
- Re-bind bindless Set 1 before transparent (deferred pass uses different layout)
