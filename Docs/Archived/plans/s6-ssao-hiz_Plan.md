# Plan: S6 — SSAO + Hi-Z depth pyramid

**Status:** Closed (2026-06-15)  
**Progress:** [`s6-ssao-hiz_Progress.md`](s6-ssao-hiz_Progress.md)

## Problem

Stage 2 hybrid deferred lacks screen-space ambient occlusion and a Hi-Z depth pyramid. Contact crevice darkening and future GPU occlusion culling (Backlog) need these resources.

## Non-goals

- GTAO / temporal accumulation (backlog)
- GPU occlusion cull pass (Backlog, deps S6)
- Post / tonemap (S7)
- Frame graph builder (S7)

## Design

| Component | Approach |
|-----------|----------|
| **Hi-Z** | R32_SFLOAT mip chain; min-depth 2×2 reduction compute per mip; mip0 = G-buffer depth copy |
| **Mip policy** | `levels = min(8, floor(log2(max(w,h))) + 1)`; each level halves resolution; stores **closest** depth (min) for standard Z [0,1] |
| **SSAO** | View-space hemisphere (16 taps); normal-aware; separable 4-tap blur compute |
| **Composite** | AO modulates **ambient + IBL only** in `DeferredLighting.frag` (direct sun unchanged) |
| **Debug** | `Gfx_DebugViewMode_Ao`, `Gfx_DebugViewMode_HiZ` |

## Chain insertion

```
GBufferOpaque → ClusterBuild → [barriers] → DepthPyramid → SSAO → depth copy → HybridRP(Deferred → Transparent)
```

## Touch list

- `Gfx/Gfx_AoSettings.h`, `Gfx_MaterialTypes.h` — settings + debug modes
- `RenderCore/Vk_DepthPyramidPass.{h,cpp}`, `Vk_SsaoPass.{h,cpp}`
- `Shader/DepthPyramid.comp`, `Ssao.comp`, `SsaoBlur.comp`
- `Shader/DeferredLighting.frag`, `Vk_DeferredLightingPass.cpp`
- `RenderCore/Vk_GBufferPass.cpp`, `Vk_Core.{h,cpp}`, `Vk_SwapchainHost.cpp`
- `Util/Util_LightingPanel.cpp`, `Util/Util_RenderDebugPanel.cpp`
- `VulkanDesktop.vcxproj` + `.filters`, `CompileShader_Glslc.bat`

## Steps

1. **Gfx settings + debug modes** — `Gfx_AoSettings`, panel toggles, debug view enum
2. **Hi-Z pass** — pyramid image, compute reduce, mip views, barrier helpers
3. **SSAO pass** — compute AO + blur; R8_UNORM output
4. **Deferred composite** — binding 13 AO; indirect-only multiply; debug views
5. **Wire chain + lifecycle** — Init/Destroy/Recreate; `RecordFrame` insertion
6. **Verify** — `Verify-CI.ps1`; manual Sponza AO visible

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- Manual: Sponza HybridDeferred — crevice AO visible; debug AO / Hi-Z mip modes work

## Risks

- SSAO noise without temporal filter — acceptable v0; blur mitigates
- Hi-Z unused by cull this sprint — texture must be valid for capture / debug only
