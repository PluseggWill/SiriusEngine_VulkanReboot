# Archived Plan — SiriusEngine / VulkanDesktop

Completed lines from [Active-Plan.md](Active-Plan.md). Task logs: [Archived/plans/](Archived/plans/).

**Format:** `[x] [Sx] description — date/notes`

---

## Completed tasks

### Toolchain & stability

- [x] **[S0]** Extension/layer probes via `UtilLogger` (instance discovery, device missing ext Warn, GPU alignment Debug) — 2026-05-23; `Docs/Archived/plans/extension-probes_Plan.md`.
- [x] **[S0]** Debug messenger (`VK_EXT_debug_utils` → `UtilLogger` `[VULKAN-VALIDATION]`) — 2026-05-23; `Docs/Archived/plans/debug-messenger_Plan.md`, `Util_DebugMessenger`.
- [x] **[S0]** Expand `README.md` + new-machine bootstrap (`Docs/bootstrap.md`, `Scripts/Verify-Bootstrap.ps1`) — 2026-05-23; `Docs/Archived/plans/bootstrap_Plan.md`.
- [x] **[S0]** Fix `VkInit::Pipeline_DynamicStateCreateInfo` dangling `pDynamicStates`; wire dynamic state in `Vk_PipelineBuilder` — 2026-05-22; `Docs/Archived/plans/pipeline-dynamic-state_Plan.md`.
- [x] **[S0]** Validation layers: install guide, startup layer discovery log, runtime on/off (`--validation`, `--no-validation`, `engine.json`) — 2026-05-22; `Docs/validation-layers.md`, `Util_ValidationConfig`, `Util_ValidationLayers`.
- [x] **[S0]** Pipeline creation diagnostics (`VkResult` + stage/layout summary) — 2026-05-22; `Util_VulkanResult`, `Vk_PipelineDiagnostics`, `Docs/Archived/plans/pipeline-diagnostics_Plan.md`.
- [x] **[S0]** Startup checks: required `Shader_Generated/*.spv`, demo models/textures exist before Vulkan init — 2026-05-22; `Util_StartupChecks`, `Util_DemoAssets.h` (temporary; migrate per `Archived/plans/scene-load_Plan.md`), `Docs/Archived/plans/startup-checks_Plan.md`.
- [x] **[S0]** Asset root configuration (`Config/engine.json` + `--asset-root` / `--config` CLI); repo-relative paths; heuristic `BuildCandidateBases` removed — 2026-05-22; `Docs/Archived/plans/asset-root_Plan.md`.
- [x] **[S0]** Deterministic glslc compile + VS Custom Build — 2026-05-22; `Docs/Archived/notes-2026-05-22-shader-debug.md`.
- [x] **[S0]** Repo-root `Logs/shader_compile_log.txt` + `engine_runtime_log.txt` — `ShaderBuild_Common.bat`, `UtilLogger`.
- [x] **[S0]** Shader scripts split; MSBuild stdout pitfall documented — `.cursor/rules/shader-build.mdc`.
- [x] **[S0]** Single GLSL source path (`TriangleVertex.vert` + `TriangleFrag_Lit.frag`); HLSL/dxc removed — 2026-05-22.
- [x] **[S0]** SPIR-V outputs under `Shader_Generated/` — 2026-05-22.

### Engine / hygiene

- [x] **[S0]** Descriptor strategy locked (static + dynamic UBO + push hybrid by frequency) — 2026-05-22; `Docs/Archived/plans/descriptor-strategy_Plan.md`, `EngineArchitecture.md` §5.3, `Vk_DescriptorPolicy.h`. Set 0 demo verified; Set 1/2 + push verification tracked in **S1** / **S2** tasks.
- [x] **[S2]** Shader reflection (2a): offline SPIRV-Reflect → `reflection_lit.json`; MSBuild validate vs `DescriptorContract_LitBatch.json` — 2026-06-01; [`Archived/plans/shader-reflection_Plan.md`](Archived/plans/shader-reflection_Plan.md).
- [x] **[S2]** Shader layout from reflection (2b): `ShaderEffectMeta` + layout hash cache from `reflection_lit.json`; lit batch `vkCreateDescriptorSetLayout`; M4 `--descriptor-layout-mismatch-test` — 2026-06-01; [`Archived/plans/shader-layout-from-reflection_Plan.md`](Archived/plans/shader-layout-from-reflection_Plan.md).
- [x] **[S2]** Shader reflection bindless verify (2d): `reflection_lit_bindless.json` + `DescriptorContract_LitBindless.json` MSBuild validate; runtime `VerifyLitBindlessReflectionContract` on bindless path — 2026-06-01; [`Archived/plans/shader-reflection-bindless-verify_Plan.md`](Archived/plans/shader-reflection-bindless-verify_Plan.md).
- [x] **[S2]** Permutation registry: `PermutationRegistry.json`, `Gfx_ShaderPermutation`, `lit_alpha_clip` SPIR-V, sort-key perm slot + `myPipelinePermutationId` — 2026-06-01; [`Archived/plans/permutation-registry_Plan.md`](Archived/plans/permutation-registry_Plan.md).
- [x] **[S2]** Pipeline cache: `Vk_DevicePipelineCache` + disk `Cache/pipeline_cache_v1.bin` (GPU UUID + driver + SPIR-V mtimes); wired into `vkCreateGraphicsPipelines` — 2026-06-01; [`Archived/plans/pipeline-cache-disk_Plan.md`](Archived/plans/pipeline-cache-disk_Plan.md).
- [x] **[S2]** Stage 1 forward contracts (epic §A): `GpuMaterialParams`/`GpuMaterialTableEntry` PBR fields, scene JSON, `ForwardLit` render preset, feature-bit reserve — 2026-06-01; [`Archived/plans/forward-stage1-contracts_Plan.md`](Archived/plans/forward-stage1-contracts_Plan.md).
- [x] **[S2]** Stage 1 forward pass hardening (epic §B): pass CONTRACT, Stage 2 depth policy, `Util_RenderDebugPanel` (skip pass, depth/normal debug), per-material `alphaMode` mask, bindless `alphaMode` — 2026-06-02; [`Archived/plans/forward-pass-hardening_Plan.md`](Archived/plans/forward-pass-hardening_Plan.md).
- [x] **[S2]** Wire dynamic pipeline state (viewport + scissor + line width); pass-level `SetGraphicsDynamicState`; remove legacy `Pipeline_DynamicStateCreateInfo` — 2026-06-01; `Archived/plans/pipeline-dynamic-state-wire_Plan.md`.
- [x] **[S2]** Input abstraction: `App/InputSystem` owns sampling + `Util_InputState`; `Application` drives platform → input → `ApplyCameraInput` → render; no `UtilInput::Sample` in RenderCore — 2026-05-27; `Archived/plans/input-abstraction_Plan.md`.
- [x] **[S2]** Central config: `Util_EngineConfig` + `Config/engine.json` (window, vsync, asset root, scene, log level, validation, demoRotate/runtimeMipmap); thin `Util_AssetConfig` / `Util_ValidationConfig` delegates — 2026-05-27; `Archived/plans/central-config_Plan.md`.
- [x] **[S2]** Application lifecycle: `App/Application` orchestrates InitApp → scene verify → `InitRenderDevice` / `LoadSceneResources` / `Update`+`Render` / `UnloadScene` / `Shutdown`; peeled scene load out of `InitVulkan` — 2026-05-27; `Archived/plans/application-lifecycle_Plan.md`.
- [x] **[S2]** `Vk_Core` decomposition (incremental): `Vk_ResourceContext` + tables load (M1), `Gfx_FrameDrawStream` / `Vk_FrameDrawPrep` + `Gfx_DemoSceneSim` (M2), `DrawFrame` acquire/prep/record/present (M3) — 2026-05-27; `Archived/plans/vk-core-decomposition_Plan.md`.
- [x] **[S2]** `Vk_ResourceContext v2`: resource load/upload helpers moved to explicit context injection (`Util_Loader`, `Gfx_Mesh::BuildBuffers`) and no singleton access in this chain — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `Vk_RenderDevice`: phase-2 part-1 init orchestration peeled to `Vk_RenderDevice::Init` (instance/device/queues/VMA/command pools) with `Vk_Core::InitRenderDevice` delegation — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `Vk_SwapchainHost`: phase-2 part-2 swapchain-host init orchestration peeled to `Vk_SwapchainHost::Init` (swapchain/render pass/depth-color/framebuffers) with `Vk_Core::InitRenderDevice` delegation — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `Vk_DescriptorSystem`: descriptor/layout/pool/material-set orchestration peeled to `Vk_DescriptorSystem` and delegated from `Vk_Core` init/load-scene paths — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `Vk_GfxPipelineCache`: scene pipeline orchestration peeled to `Vk_GfxPipelineCache::InitScenePipelines` and delegated from load-scene + swapchain-recreate paths — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `Vk_ScenePasses`: frame pass record orchestration peeled to `Vk_ScenePasses::RecordFramePasses` with `DrawFrame` delegation — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `Vk_FrameUniformUploader`: per-frame UBO upload orchestration peeled to `Vk_FrameUniformUploader::Update` with `DrawFrame` delegation — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** Image queue sharing when transfer ≠ graphics family: `VkInit::FillImageSharingMode` applied to `Vk_ResourceContext::CreateImage` + `Vk_Core::CreateImage`; startup queue-family sharing log added — 2026-06-01; [`image-queue-sharing_Plan.md`](Archived/plans/image-queue-sharing_Plan.md).
- [x] **[S2]** Verify descriptor layout policy: `VkPipelineLayout` sets 0/1/2 vs `Vk_DescriptorPolicy.h`, `LogLayoutContract`, material rebuild contract — 2026-05-29; [`descriptor-layout-verify_Plan.md`](Archived/plans/descriptor-layout-verify_Plan.md).
- [x] **[S2]** Init hygiene: remove dead `Vk_Core` init forwards; `Vk_SceneHost::InitScenePresentation` for demo camera/env; delete `Gfx_BuildDemo*` / `Util_DemoAssets` — 2026-05-29; [`s2-init-hygiene_Plan.md`](Archived/plans/s2-init-hygiene_Plan.md).
- [x] **[S2]** Scene host peel: scene CPU-state setup (id tables/SoA/LOD/debug logical mesh) peeled to `Vk_SceneHost::LoadCpuState` from `LoadSceneResources` — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `Vk_PlatformFrame`: GLFW window init + per-frame poll/delta/ImGui orchestration peeled to `Vk_PlatformFrame` from `InitWindow`/`BeginPlatformFrame` — 2026-05-28; `Archived/plans/vk-core-decomposition_Plan.md`, `Archived/plans/vk-core-decomposition_Progress.md`.
- [x] **[S2]** `gfx-vk-decoupling`: render packet contract + `Vk_RenderBackend` boundary landed; `RenderCore` runtime consume path is packet-only and direct `Gfx_ExtractResult` dependency removed from `RenderCore/` — 2026-05-28; `Archived/plans/gfx-vk-decoupling_Plan.md`, `Archived/plans/gfx-vk-decoupling_Progress.md`.
- [x] **[S2]** Thin scheduler: `Vk_Core::Update` vs `Render` driven by `Application` main loop — 2026-05-27; with application-lifecycle.
- [x] **[S2]** Scene JSON author guide [`SceneJSON.md`](SceneJSON.md) + handoff pause notes — 2026-05-27; `Archived/plans/scene-load_Plan.md` Handoff.
- [x] **[S2]** Scene-load Phase C: scene-driven SoA/LOD/manifest + `Vk_ResourceTables::LoadFromManifest`; `SetLoadedScene` before `Run()` — 2026-05-27; `Archived/plans/scene-load_Plan.md` Phase C.
- [x] **[S2]** Scene-load Phase D: `mySceneDeletionQueue` + GPU `UnloadScene`, `assetVerify` strict/warn (`engine.json`), `Data/Scenes/smoke.json`, `--smoke-frames` dev exit — 2026-05-29; `Archived/plans/scene-load_Plan.md` Phase D.
- [x] **[S2]** ImGui Scene panel + in-process scene reload + `Docs/CLI.md` — 2026-05-29; `Util_ScenePanel`, `Application::TryProcessSceneReload`.
- [x] **[S2]** Scene-load Phase B: `Util_VerifyManifest` replaces `UtilStartupChecks` / `kRequiredFiles` — 2026-05-26; `Archived/plans/scene-load_Plan.md` Phase B.
- [x] **[S2]** Scene-load Phase A: `Data/Scenes/demo.json`, `Gfx_LoadSceneDesc`, `Util_CollectDependencies`, CLI `--scene` (parse only; GPU path unchanged) — 2026-05-26; `Archived/plans/scene-load_Plan.md` Phase A, nlohmann/json vendored.
- [x] **[S2]** Debug fly camera — `Docs/Archived/plans/fps-camera_Plan.md`; sampling still in `Vk_Core` until input module.
- [x] **[S0]** `Gfx_Mesh::LoadMesh` UV guard — `Vk_Types.cpp`.
- [x] **[S0]** `UtilLoader::LoadTexture` fail-fast — `Util_Loader.cpp`.
- [x] **[S0]** Queue family graphics-as-transfer fallback — `Vk_DataStruct.h`, `Vk_Core.cpp`.
- [x] **[S0]** Comment conventions — `.cursor/rules/cpp-comments.mdc`.

### S1 — CPU draw stream (M1)

- [x] **[S1]** Resource tables: mesh/material/texture ids → `Vk_ResourceTables`; demo `Gfx_ResourceManifest`; `RecordScenePass` resolves `Gfx_DrawInstance` ids — 2026-05-26; [`resource-tables_Plan.md`](Archived/plans/resource-tables_Plan.md) (scene JSON manifest: `scene-load` Phase C).
- [x] **[S1]** Extract phase: `Gfx_DrawInstance` + `Gfx_ExtractDrawInstances` (sort key, mesh/material ids, instance offset); demo entities; no Vulkan in Gfx module — 2026-05-25; [`draw-extract_Plan.md`](Archived/plans/draw-extract_Plan.md), `Gfx_DrawExtract.*`, `Vk_Core` frame hook; removed dead `DrawObjects` stub.
- [x] **[S1]** SoA columns + stable entity id: `Gfx_SceneSoA` (transform, bounds, mesh/material, layer mask; slot + generation); extract reads columns — 2026-05-25; [`scene-soa_Plan.md`](Archived/plans/scene-soa_Plan.md), `Gfx_SceneSoA.*`.
- [x] **[S1]** Finish or delete `DrawObjects` stub; `RecordScenePass` documented as live Vulkan path — 2026-05-25 (with extract task).
- [x] **[S1]** CPU frustum cull + opaque sort by `mySortKey` — 2026-05-26; [`draw-cull-sort_Plan.md`](Archived/plans/draw-cull-sort_Plan.md), `Gfx_DrawCullSort.*`.
- [x] **[S1]** Per-frame instance slab (ring UBO, `FillInstanceSlab`, record via `myInstanceDataOffset`) — 2026-05-26; [`instance-slab_Plan.md`](Archived/plans/instance-slab_Plan.md).
- [x] **[S1]** Verify descriptor policy (Set 2): `UNIFORM_BUFFER_DYNAMIC` on instance slab, distinct `dynamicOffset` per draw — 2026-05-26; [`descriptor-set2-verify_Plan.md`](Archived/plans/descriptor-set2-verify_Plan.md).
- [x] **[S1]** Batch runs: `Gfx_BuildOpaqueDrawBatches`, `RecordScenePass` scans batch runs; set 0 once per pass — 2026-05-26; [`draw-batch_Plan.md`](Archived/plans/draw-batch_Plan.md).
- [x] **[S1]** Instance slab overflow fail-closed — 2026-05-26; [`instance-slab-overflow_Plan.md`](Archived/plans/instance-slab-overflow_Plan.md).
- [x] **[S1]** Demo transform/cull sync (SoA updated before extract; slab uses same matrix) — 2026-05-26; [`demo-transform-sync_Plan.md`](Archived/plans/demo-transform-sync_Plan.md).
- [x] **[S1]** Verify descriptor policy (Set 0/1 + Set 2): Set 1 per-material texture, batch bind; demo viking + RedMoon materials — 2026-05-26; [`descriptor-set1-verify_Plan.md`](Archived/plans/descriptor-set1-verify_Plan.md).
- [x] **[S1]** Transparency: dual extract/sort lists, eye-space Z sort, opaque then transparent record, demo overlay monkey — 2026-05-26; [`transparency_Plan.md`](Archived/plans/transparency_Plan.md).
- [x] **[S1]** LOD v0 (CPU): logical mesh + `Gfx_LodTable`, distance LOD + hysteresis, resolved meshId in draw/batch, demo tree chain — 2026-05-26; [`lod-v0_Plan.md`](Archived/plans/lod-v0_Plan.md), `Data/LOD.md`.
- [x] **[S1]** Bindless v0: hybrid batch/bindless paths, indexing probe, material SSBO table + `materialIndex`, sort-key table generation — 2026-05-26; [`bindless-v0_Plan.md`](Archived/plans/bindless-v0_Plan.md).
- [x] **[M1]** Transparent object over opaque (order + blend): demo overlay monkey; opaque behind visible — visual sign-off 2026-05-26; [`transparency_Plan.md`](Archived/plans/transparency_Plan.md).
- [x] **[M1]** Multi-mesh batch scaling + frame time: demo 9 entities / 8+1 batch runs; ImGui + `[PERF]` log after warmup — 2026-05-26; [`m1-acceptance_Plan.md`](Archived/plans/m1-acceptance_Plan.md).

---

*When closing a task: move here, tag sprint (`[S0]`…`[S8]`, `[parallel]`), note verification. Update `EngineArchitecture.md` when boundaries or render target change.*
