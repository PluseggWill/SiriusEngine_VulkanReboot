# Plan: S7 — Post-processing + frame graph v1

**Status:** Closed (2026-06-15)  
**Progress:** [`s7-post-fg_Progress.md`](s7-post-fg_Progress.md)

## Problem

HybridDeferred wrote LDR directly to swapchain with a hand-written pass chain. Stage 2 needs HDR intermediate, tonemap/exposure, optional bloom, and a frame graph to manage pass order and preset toggles.

## Non-goals

- Rendering lab presets / GPU timestamps / benchmark CSV (Wishlist §S7 lab — backlog)
- Full transient RT pool / import-export (FG v2)
- DDGI (S8)

## Design

| Component | Approach |
|-----------|----------|
| **HDR** | `R16G16B16A16_SFLOAT` scene color; hybrid RP/FB owned by `Vk_PostProcessPass` |
| **Post** | Optional bloom (threshold + separable blur) → ACES/Reinhard tonemap + exposure → swapchain |
| **FG v1** | `Vk_FrameGraphBuilder`: pass nodes, deps, topo sort, preset skips (shadow/AO/bloom) |
| **Settings** | `Gfx_PostSettings` + ImGui in Lighting panel |

## Chain (HybridDeferred)

```
Shadow → GBuffer → ClusterBuild → DepthPyramid → SSAO → DeferredTransparent(HDR) → Post → ImGui
```

## Touch list

- `Gfx/Gfx_PostSettings.h`, `Vk_FrameGraphBuilder.*`, `Vk_PostProcessPass.*`
- `Shader/Tonemap.*`, `BloomThreshold.comp`, `BloomBlur.comp`
- `Vk_GBufferPass.cpp`, `Vk_Core.*`, `Vk_GfxPipelineCache.cpp`, `Vk_SwapchainHost.cpp`
- `Util_LightingPanel`, `DebugOverlay`

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- Manual: HybridDeferred Sponza — exposure/bloom/tonemap toggles visible

## Risks

- MSAA + HDR hybrid depth sharing — document; default 1× MSAA OK
