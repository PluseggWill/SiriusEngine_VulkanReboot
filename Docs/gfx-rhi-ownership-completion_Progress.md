# Progress — Gfx/Rhi ownership completion

**Plan:** [`gfx-rhi-ownership-completion_Plan.md`](gfx-rhi-ownership-completion_Plan.md)  
**Started:** 2026-07-23

## 2026-07-23 — O5 SSR + AO descriptor updates → Gfx
- **Gfx_SsrPass:** allocate sets in `CreatePipeline`; `UpdateDescriptors` (GBuffer/HiZ/history/output).
- **Gfx_AoPass:** `UpdateDescriptors` + `UpdateTemporalDescriptors` (classic/half/upsample/blur/temporal).
- **RC:** cross-pass adopt + SyncVkMirrors only; no `vkUpdateDescriptorSets` for Soft/SSR/AO.
- **Still pending:** Post descriptor writes + layouts/images; thinner shell retirement.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED.

## 2026-07-23 — O5 Soft descriptor alloc/update → Gfx
- **Gfx_ShadowAoSoftPass:** allocate pack/blur sets in `CreatePipelines`; `UpdateDescriptors` via `DeviceUpdateDescriptorImages/Buffers`.
- **RC:** SPIR-V load + SyncVkMirrors + cross-pass adopt (GBuffer/AO/Shadow/lighting); no `vkUpdateDescriptorSets` / `vkAllocateDescriptorSets`.
- **Rhi:** `DeviceUpdateDescriptorImages` resolves adopted samplers (pointer-id fallback).
- **Still pending:** AO/SSR/Post descriptor writes; Post layouts/images; retire thinner shells.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED.

## 2026-07-23 — O3 Init fan-out + Rhi correctness
- **Init→Gfx:** AO CreatePipelines + images; Soft/SSR CreateOrRecreateImages; Post bloom/TAA compute PSOs; ShadowMap CreateResources; Deferred+DDGI CreatePipeline.
- **Rhi fixes:** `DeviceCreateComputePipeline` uses `ResolvePipelineLayout` (adopted layouts); standard Gfx vertex attrs location/binding order; `DeviceUploadTexture2D` returns bool; Clear destroys ShadowMap+Rhi before VMA allocator flush.
- **Bugbot:** Soft fallback upload must succeed; deferred hybrid PSO sample count fixed to 1.
- **Still pending:** descriptor writes mostly RC; Post/Deferred layout+image shells; O5 retire empty `Vk_*Pass` shells.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (known `enabledLayerCount` only).

## 2026-07-23 — O3 AO + Soft/SSR images Init in Gfx
- **Gfx_AoPass:** `PassState`, `CreatePipelines` (6 compute PSOs, 5 set layouts, pool, sampler, 3×6 descriptor sets), `CreateOrRecreateImages` (R8/RG8 storage targets).
- **Gfx_ShadowAoSoftPass / Gfx_SsrPass:** `CreateOrRecreateImages` (RG8 ping-pong + R8/RG8 1×1 fallbacks; SSR RGBA16F output + history).
- **Rhi:** `RG8_Unorm`, `DeviceTransitionTextureLayout`, `DeviceUploadTexture2D`.
- **RC (unchanged role):** `vkUpdateDescriptorSets` for GBuffer/shadow/lighting/HiZ/temporal; Vk texture mirrors via `SyncVkMirrors`.
- **Verification:** `Verify-CI` PASSED.

## 2026-07-23 — O4 complete + O3 fan-out (Cluster/Soft/SSR)
- **O4:** Deleted remaining `Vk_*_Record.cpp` (Post, ShadowMap, Deferred+DDGI); all FG records via pass TU + `Vk_FrameCmd`.
- **O3:** ClusterBuild full Init in Gfx (buffers + map + descriptor buffer writes); Soft/SSR **CreatePipeline** in Gfx (images/descriptor writes still RC).
- **Rhi:** `DeviceUpdateDescriptorBuffers`, `DeviceMapBuffer`/`UnmapBuffer`.
- **Still pending:** AO/Post/Shadow/Deferred Init; Soft/SSR image Init; O5 shell retirement.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (known `enabledLayerCount` only).

## 2026-07-23 — O3 DepthPyramid Init + O4 Record peel + graphics create
- **DepthPyramid Init in Gfx:** `Gfx_DepthPyramidPass::{CreatePipeline,CreateOrRecreateImage,Destroy*}`; RC thin SPIR-V load + Vk mirrors for SSR/deferred.
- **Deleted Record facades:** DepthPyramid, ClusterBuild, AO, Soft, SSR — FG/pass TUs call Gfx via `Vk_FrameCmd`.
- **Rhi graphics create:** `DeviceCreateRenderPass` / `Framebuffer` / `GraphicsPipeline`; Post hybrid RP/FB + tonemap PSO use them.
- **Still pending:** Post/Shadow/Deferred Record peel; more Init moves (O3/O5).
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (known benign noise only if present).

## 2026-07-23 — kickoff
- **Files:** Progress created; README Active now → this WIP
- **Next:** O1 FG Begin/End peel; O2 Rhi create surface; O3/O4 pilot then fan-out
- **Verification:** pending

## 2026-07-23 — Hi-Z correctness fix (not occlusion cull)
- **What it is:** Min-depth pyramid for **SSR** + debug; **S16** is GPU occlusion cull (future).
- **Bugs fixed:** (1) mip0 only wrote half-res into full-res image → garbage UV sampling; now mip0 = full-res depth copy. (2) Sample view was single-mip → `textureLod`/`Hi-Z` debug broken; now full mip-chain view. (3) Debug remaps ZO depth for visibility; ImGui tooltip clarifies role.
- **Files:** `DepthPyramid.comp`, `Gfx_DepthPyramidPass.cpp`, `Vk_DepthPyramidPass.cpp`, `DeferredLighting.frag`, `Util_RenderDebugPanel.cpp`, regenerated `.spv`
- **Verification:** build OK; `Verify-Smoke.ps1` PASSED; sponza `--validation` exit 0 (known `enabledLayerCount` only). `Verify-CI` will pass once regenerated `.spv` are committed.