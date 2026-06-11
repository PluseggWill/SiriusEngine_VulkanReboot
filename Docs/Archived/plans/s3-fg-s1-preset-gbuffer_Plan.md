# Plan: s3-fg-s1 — Preset hook + GBufferOpaque shell (slice 1)

**Status:** Closed (2026-06-11)  
**Parent roadmap:** [`s3-fg-v0_Plan.md`](../../s3-fg-v0_Plan.md) steps 1–2 (partial)  
**Epic:** [`hybrid-deferred-epic_Plan.md`](../../hybrid-deferred-epic_Plan.md) §A + §B (G-buffer only)  
**Branch:** `S3`

## Problem

Full FG v0 (`GBuffer → ClusterBuild → DeferredLighting`) is too large for one PR. Slice 1 delivers **preset plumbing + offscreen G-buffer raster + albedo composite to swapchain** so the hybrid path is dogfoodable before clustered lighting exists.

## Goal (slice 1 only)

| # | Deliverable | Done when |
|---|-------------|-----------|
| G1 | `HybridDeferred` preset parses (CLI/config) | `--render-preset HybridDeferred` boots; default `ForwardLit` unchanged |
| G2 | G-buffer targets + render pass | Resize recreates RT0/RT1 + depth; format locked in Progress |
| G3 | `GBufferOpaque` raster | Opaque draws write albedo + normal/roughness MRT (batch materials) |
| G4 | `CompositeAlbedo` to swapchain | Visible frame ≈ forward albedo (no lighting yet) |
| G5 | Pass chain log | Once: `[FG] HybridDeferred: GBufferOpaque -> CompositeAlbedo` |
| G6 | CI green | `Verify-CI` + `Verify-Smoke` on **ForwardLit** default |

## Non-goals (defer to slice 2+)

- `ClusterBuild`, `DeferredLighting`, HDR intermediate
- Transparent forward over deferred depth (log skip once)
- Bindless + HybridDeferred (batch only v1; bindless → forward fallback + log)
- Multi-view (view 0 only; warn if `aViewCount > 1`)
- `PermutationRegistry.json` / `EngineArchitecture.md` policy lock
- `Verify-Smoke` HybridDeferred pass (manual only this slice)

## G-buffer format v0 (slice 1 — Progress only, not Architecture)

| Target | Format | Content |
|--------|--------|---------|
| RT0 | `VK_FORMAT_R8G8B8A8_UNORM` | Albedo RGB (vertex color × tex × `baseColorFactor`) |
| RT1 | `VK_FORMAT_R16G16B16A16_SFLOAT` | World normal XYZ + roughness in W |
| Depth | `FindDepthFormat()` | Dedicated G-buffer depth (not swapchain depth) |

MSAA: **off** for G-buffer (1 sample); matches typical deferred; swapchain MSAA unchanged for composite pass.

## Touch list

| Area | Files |
|------|--------|
| Preset | `Gfx_RenderPreset.h/.cpp`, `Util_EngineConfig.cpp` *(no change if preset already flows)* |
| Shaders | `Shader/GBuffer.vert`, `GBuffer.frag`, `CompositeAlbedo.vert`, `CompositeAlbedo.frag`, `CompileShader_Glslc.bat` |
| G-buffer module | `RenderCore/Vk_GBufferPass.h/.cpp` *(new)* |
| Record | `Vk_ScenePasses.h/.cpp` — pipeline override + hybrid dispatch |
| Lifecycle | `Vk_Core.cpp` Load/Unload, `Vk_SwapchainHost.cpp` extent rebuild |
| Tests | `GfxTests_Main.cpp` — preset API |
| Docs | `CLI.md`, `s3-fg-v0_Plan.md` cross-link |
| Build | `VulkanDesktop.vcxproj` + `.filters` |

## Ordered steps

### 1. Preset API
- `Gfx_RenderPreset::IsHybridDeferred(name)`
- `ToShaderPermutationName("HybridDeferred")` → `"lit"` *(material table / scene load unchanged)*
- GfxTests: HybridDeferred recognized; unknown preset still throws

### 2. Shaders + compile
- `GBuffer.vert` — same bindings as `TriangleVertex.vert`
- `GBuffer.frag` — MRT 2; reuse lit material sampling (batch Set 1)
- `CompositeAlbedo.vert` — fullscreen triangle (`gl_VertexIndex`)
- `CompositeAlbedo.frag` — sample RT0 albedo
- Extend `CompileShader_Glslc.bat` → `GBufferVert.spv`, `GBufferFrag.spv`, `CompositeAlbedoVert.spv`, `CompositeAlbedoFrag.spv`

### 3. `Vk_GBufferPass` resources
- `Init` / `Destroy` / `RecreateForExtent`
- Offscreen render pass (2 color + depth), framebuffer, images on deletion queue
- G-buffer graphics pipeline: **batch layout** (`myPipelineLayout`), dynamic viewport/scissor
- Composite pipeline: single descriptor (combined image sampler), uses **swapchain render pass**

### 4. Record path
- `Vk_ScenePasses::RecordScene`: if `IsHybridDeferred` → `Vk_GBufferPass::RecordFrame(...)` else forward (unchanged)
- `RecordFrame`: GBuffer RP → opaque draws with `myGBufferPipeline` override → barrier → swapchain RP → composite tri → end
- Bindless + hybrid: fallback to forward + one-time warn

### 5. Lifecycle hooks
- `LoadSceneResources`: `Vk_GBufferPass::Init` after scene pipelines
- `UnloadScene`: `Vk_GBufferPass::Destroy`
- `RebuildExtentDependentResources`: `Vk_GBufferPass::RecreateForExtent`

### 6. Verification
- `Verify-CI.ps1` exit 0
- `Verify-Smoke.ps1` exit 0 (ForwardLit default)
- Manual: `--render-preset HybridDeferred --no-validation --smoke-seconds 4` → `[FG]` log + validation-clean exit

## Risks

- **Bindless dev path:** batch-only hybrid v1; document in closeout
- **Resize:** G-buffer tied to `RebuildExtentDependentResources` — soak manually if needed
- **Shader drift:** commit new `.spv` after compile

## References

- Roadmap: [`s3-fg-v0_Plan.md`](../../s3-fg-v0_Plan.md)
- Bindless maint: [`shader-bindless-policy_Plan.md`](shader-bindless-policy_Plan.md)
