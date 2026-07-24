# Progress — Gfx/Rhi ownership completion

**Plan:** [`gfx-rhi-ownership-completion_Plan.md`](gfx-rhi-ownership-completion_Plan.md)  
**Started:** 2026-07-23

## 2026-07-24 — O5 Soft/SSR/AO Sync cleanup + ShadowMap + Architecture lock
- **Soft/SSR/AO:** state is `myGfx` (+ SSR/AO CPU history flags) only; removed SyncVkMirrors and all pipeline/set/texture Vk mirrors.
- **ShadowMap:** Record via `myGfx`; barriers use `TextureGetVkImage(myGfx.myDepth)`; kept `myDepthLayout` + light bias CPU; **DescriptorSystem** binds shadow via `SamplerGetVk` / `TextureGetVkView` on `myGfx`.
- **Architecture:** migration note locked — HybridDeferred Init+Record in Gfx; RC shells = SPIR-V / WSI hybrid RP-FB / cross-pass adopt / layout trackers.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (only known device-layer `enabledLayerCount` ERROR).

## Closeout — 2026-07-24
- **Outcome:** gfx-rhi-ownership-completion O1–O5 done. HybridDeferred pass Init+Record live in `Gfx_*Pass` via Rhi; `Vk_*_Record` deleted; remaining `Vk_*Pass` TUs are thin orchestration shells (SPIR-V load, WSI hybrid RP/FB, GBuffer/IBL adopt, CPU history/layout trackers).
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0; sponza `--validation` exit 0 (no new pass/descriptor validation ERROR).
- **Deviations:** GBuffer/IBL texture ownership and Forward path remain RC (non-goal). Soft/SSR/AO/ShadowMap header mirrors retired in final O5 slice after Post/Deferred/Cluster/Hi-Z.

## 2026-07-24 — O5 thin RC shells (Record via myGfx, drop redundant mirrors)
- **Post:** Bloom/TAA Record use `myGfx` directly (match tonemap); dropped all pipeline/set/texture Vk mirrors; TAA history CPU flags moved into `Gfx_PostProcessPass::PassState`; hybrid RP/FB remain RC (WSI).
- **SSR:** Trace + history Record via `myGfx`; history copies from `myPostProcessState.myGfx.mySceneColor`; removed dead `GetOutputImageView`.
- **Deferred/DDGI:** Record via `myDeferredGfx`/`myDdgiGfx`; removed SyncVkMirrors + pipeline/set mirrors from state.
- **Cluster / DepthPyramid:** state is `myGfx` + `myInitialized` only; Record already `MakeGpuResources`; removed Sync + dead `GetMipView`.
- **Soft / AO:** Record builds GpuResources from `myGfx` (GBuffer still adopt); removed dead `GetDeferredContactMapView` / `GetRawAoImageView` / `GetBentNormalHalfView`. Soft/SSR/AO header Sync mirrors still present (non-Record leftovers); ShadowMap untouched (DescriptorSystem consumer).
- **Still pending for O5 close:** Soft/SSR/AO Sync header cleanup; optional ShadowMap Record→myGfx; decide Architecture locked note vs WIP.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (only known device-layer `enabledLayerCount` ERROR).

## 2026-07-24 — O5 Deferred + DDGI layouts/descriptors/atlas → Gfx
- **Gfx_DeferredLightingPass:** `PassState` now owns the 20-binding set layout, pipeline layout, pool, sets[], G-buffer sampler, and graphics PSO; `CreatePipeline` builds the full stack from SPIR-V + hybrid RP (sampleCount=1); `UpdateDescriptors` writes all ~20 bindings via `DeviceUpdateDescriptorImages`/`Buffers` with albedo-fallback for optional CIS inputs (shadow/IBL/AO/HiZ/DDGI/SSR/bent-normal); `UpdateAoBinding` keeps the cheap per-draw binding-13 rebind.
- **Gfx_DdgiPass:** `PassState` now owns the 4-binding compute set layout, pipeline layout, pool, sets[], sampler, and irradiance/visibility atlas images; `CreateOrRecreateImages` (re)allocates atlases sized `probeCountX*probeCountY × probeCountZ` and transitions them to `ShaderReadOnly`; `UpdateDescriptors` binds atlases + G-buffer worldPos/normalRoughness for all frames.
- **RC (`Vk_DeferredLightingPass`):** kept SPIR-V file load, `SyncVkMirrors`, cross-pass `TextureAdopt`/`BufferAdopt`/`SamplerAdopt` (G-buffer, shadow map, IBL, cluster SSBOs, lighting globals UBO) into `DescriptorUpdateDesc`, `RecordDraw`/`RecordDdgiProbeUpdate` orchestration, `BuildPushConstants`, and DDGI history-cursor CPU state. Removed `CreateDdgiAtlas`/`DestroyDdgiAtlas` VMA calls, the 20-binding + 4-binding `vkCreate*`/`vkAllocateDescriptorSets`/`vkUpdateDescriptorSets` blocks, and the deletion-queue captures for layouts/pool/sampler now owned by Gfx `Destroy`.
- Fixed a duplicate `Destroy( Rhi_Device&, PassState& )` definition left over in `Gfx_DdgiPass.cpp` (the correct one calls `DestroyImages` + `DestroyPipeline`).
- **Still pending:** Soft/SSR/AO Sync header leftovers; ShadowMap optional; O5 Architecture lock.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (only known device-layer `enabledLayerCount` ERROR; no pass/descriptor validation ERROR).

## 2026-07-24 — O5 tonemap layouts/descriptors/PSO → Gfx
- **Gfx_PostProcessPass:** `CreateTonemapResources` / `CreateOrRecreateTonemapPipeline` / `UpdateTonemapDescriptors` / `DestroyTonemapResources`; PassState owns tonemap set layout, pipeline layout, pool, sets, PSO (swapchain RP adopted at create).
- **RC:** SPIR-V load + SyncVkMirrors + hybrid RP/FB only; no `vkCreate*` / `vkUpdateDescriptorSets` for tonemap; Record uses Gfx handles directly.
- **Still pending (at time of write):** Deferred/DDGI peel — **done in later checkpoint**.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (no `[VULKAN-VALIDATION]` ERROR).

## 2026-07-24 — O5 Post images + compute Init/descriptors → Gfx
- **Gfx_PostProcessPass:** `PassState` owns HDR/TAA/bloom images, compute set layouts/layouts/PSOs/pool/sampler/sets; `CreateOrRecreateImages`, `CreateComputeResources`, `UpdateComputeDescriptors` (per-frame filter for TAA history ping-pong).
- **RC:** SPIR-V load + SyncVkMirrors; tonemap layout/pool/sets + hybrid RP/FB remain RC; no VMA create/destroy for post images; no `vkUpdateDescriptorSets` for bloom/TAA.
- **Still pending:** tonemap descriptors/layouts → Gfx; thinner DepthPyramid/Cluster shells; O5 shell retirement.
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED.

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