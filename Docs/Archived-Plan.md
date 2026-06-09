# Archived Plan — SiriusEngine / VulkanDesktop

Completed lines from [Active-Plan.md](Active-Plan.md). Task logs: [Archived/plans/](Archived/plans/).

**Format:** `[x] [Sx] description — date/notes`

**Closed sprints:** **S0** and **S1 (M1)** — removed from [Active-Plan.md](Active-Plan.md); reference material below.

---

## S0 — Foundation & tooling *(closed 2026-05-23)*

| | |
|--|--|
| **Milestone** | — |
| **Outcome** | Toolchain + resources trustworthy; P0 blockers cleared |
| **Validation** | [`SprintOutcomeValidation.md`](./SprintOutcomeValidation.md) (S0 section) |

Completed **`[S0]`** tasks: **Toolchain & stability** below and **`[S0]`** lines under **Engine / hygiene**.

---

## S1 — CPU draw stream / milestone M1 *(closed 2026-05-26)*

| | |
|--|--|
| **Milestone** | **M1** — SoA → extract → sort → batch → record (VS/FS) |
| **Outcome** | CPU draw stream landed; descriptor Sets 0/1/2, transparency, LOD v0, bindless v0 |
| **Validation** | [`SprintOutcomeValidation.md`](./SprintOutcomeValidation.md) (S1 section) |
| **Retrospective (中文)** | [`Archived/S1-回顾总结.md`](Archived/S1-回顾总结.md) |

### S1 — implementation notes *(reference; update here if desktop path debt shifts)*

| Topic | State | Follow-up |
|-------|--------|-----------|
| Resource tables | Done — `Gfx_ResourceManifest`, `Vk_ResourceTables`, `RecordScenePass` | Scene manifest via scene-load Phase C |
| Per-draw `model` | Done — Set 2 `UNIFORM_BUFFER_DYNAMIC` + `dynamicOffset` | — |
| Record ↔ transforms | Done — SoA before extract; slab copies matrix | — |
| Instance slab | Done — overflow fail-closed | — |
| Set 0 / Set 1 | Done — batch + bindless | S7 preset toggle |
| Draw submission | Done — set 0 once/pass; set 1/batch; set 2/draw | — |
| RenderDoc capture path | Done — passive attach (`GetModuleHandle`) + draw/pass labels; injected sessions force Batch path for stability | If bindless+RenderDoc stability improves, revisit fallback in S7 lab |
| Transparency | Done — opaque + transparent lists | — |
| LOD v0 (CPU) | Done — `Gfx_LodTable` → resolved `meshId` | GPU LOD parity (**S3**) |

**Pitfall (2026-05-26):** Do not patch `model` in a shared per-frame camera UBO between draws on the same descriptor set — use dynamic offsets (`.cursor/rules/vulkan-descriptor-per-draw.mdc`, `EngineArchitecture.md` §6.1).

### S1 — completed tasks

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

## S2 — Engine layering & hygiene *(closed 2026-06-02)*

| | |
|--|--|
| **Milestone** | — |
| **Outcome** | Lifecycle/config/render-backend layering stabilized; scene-load A–D + shader stack landed; multi-view re-landed and stabilized. |
| **Validation** | [`SprintOutcomeValidation.md`](./SprintOutcomeValidation.md) (S2 section) |
| **Retrospective (中文)** | [`Archived/S2-回顾总结.md`](Archived/S2-回顾总结.md) |

## Completed tasks *(S2+ and `[S0]` checklist)*

### Toolchain & stability *(S0)*

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

- [x] **[S0]** Descriptor strategy locked (static + dynamic UBO + push hybrid by frequency) — 2026-05-22; `Docs/Archived/plans/descriptor-strategy_Plan.md`, `EngineArchitecture.md` §6.1, `Vk_DescriptorPolicy.h`. Set 0 demo verified; Set 1/2 + push verification tracked in **S1** / **S2** tasks.
- [x] **[S2]** Shader reflection (2a): offline SPIRV-Reflect → `reflection_lit.json`; MSBuild validate vs `DescriptorContract_LitBatch.json` — 2026-06-01; [`Archived/plans/shader-reflection_Plan.md`](Archived/plans/shader-reflection_Plan.md).
- [x] **[S2]** Shader layout from reflection (2b): `ShaderEffectMeta` + layout hash cache from `reflection_lit.json`; lit batch `vkCreateDescriptorSetLayout`; M4 `--descriptor-layout-mismatch-test` — 2026-06-01; [`Archived/plans/shader-layout-from-reflection_Plan.md`](Archived/plans/shader-layout-from-reflection_Plan.md).
- [x] **[S2]** Shader reflection bindless verify (2d): `reflection_lit_bindless.json` + `DescriptorContract_LitBindless.json` MSBuild validate; runtime `VerifyLitBindlessReflectionContract` on bindless path — 2026-06-01; [`Archived/plans/shader-reflection-bindless-verify_Plan.md`](Archived/plans/shader-reflection-bindless-verify_Plan.md).
- [x] **[S2]** Permutation registry: `PermutationRegistry.json`, `Gfx_ShaderPermutation`, `lit_alpha_clip` SPIR-V, sort-key perm slot + `myPipelinePermutationId` — 2026-06-01; [`Archived/plans/permutation-registry_Plan.md`](Archived/plans/permutation-registry_Plan.md).
- [x] **[S2]** Pipeline cache: `Vk_DevicePipelineCache` + disk `Cache/pipeline_cache_v1.bin` (GPU UUID + driver + SPIR-V mtimes); wired into `vkCreateGraphicsPipelines` — 2026-06-01; [`Archived/plans/pipeline-cache-disk_Plan.md`](Archived/plans/pipeline-cache-disk_Plan.md).
- [x] **[S2]** Stage 1 forward contracts (epic §A): `GpuMaterialParams`/`GpuMaterialTableEntry` PBR fields, scene JSON, `ForwardLit` render preset, feature-bit reserve — 2026-06-01; [`Archived/plans/forward-stage1-contracts_Plan.md`](Archived/plans/forward-stage1-contracts_Plan.md).
- [x] **[S2]** Stage 1 forward pass hardening (epic §B): pass CONTRACT, Stage 2 depth policy, `Util_RenderDebugPanel` (skip pass, depth/normal debug), per-material `alphaMode` mask, bindless `alphaMode` — 2026-06-02; [`Archived/plans/forward-pass-hardening_Plan.md`](Archived/plans/forward-pass-hardening_Plan.md).
- [x] **[S2]** Stage 1 forward validation (epic §C + Acceptance): [`forward-stage1.md`](forward-stage1.md), `Config/engine.benchmark.json`, golden PNG; Stage 1 epic closed — 2026-06-02; [`Archived/plans/forward-stage1-validation_Plan.md`](Archived/plans/forward-stage1-validation_Plan.md).
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
- [x] **[S2]** Flat world matrices — flat transform source/resolved state in Gfx, explicit resolve before extract/render, `GetWorldTransform`/`SetWorldTransform` API, RenderCore base-transform ownership removed — 2026-06-02; `flat-world-matrices_Plan.md`, `flat-world-matrices_Progress.md`.
- [x] **[S2]** RenderDoc integration and drawcall tags — startup gate `--renderdoc`, passive `renderdoc.dll` attach, F12 capture trigger, pass/per-draw debug labels (`Pass/Draw/Mesh/Material/Entity`), and capture-stability Batch fallback when injected — 2026-06-02; `Archived/plans/renderdoc-drawcall-tags_Plan.md`, `Archived/plans/renderdoc-drawcall-tags_Progress.md`.
- [x] **[S2]** Multi-view re-landed — `Gfx_RenderView` v1 (swapchain), scene `cameras[]` parse, per-view extract+record with viewport/scissor, ImGui PiP camera panel, plus PiP flicker stabilization (per-view slab partition + main-view LOD-state isolation) — 2026-06-02; `Archived/plans/multi-view_Plan.md`, `Archived/plans/multi-view_Progress.md`.
- [x] **[S2]** Swapchain recreate — Khronos acquire OUT_OF_DATE/SUBOPTIMAL retry in-frame; `createInfo.oldSwapchain` handoff; fence reset unchanged in `DrawFrameGpu` — 2026-06-08; `Archived/plans/swapchain-recreation_Plan.md`, `Archived/plans/swapchain-recreation_Progress.md`.
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

---

## P1 — config-platform-hardening *(closed 2026-06-02)*

| | |
|--|--|
| **Outcome** | `Util_EngineConfig` instance on `Application`; `Docs/Platform.md`; `Vk_FrameResult` recoverable acquire/submit/present; device lost → graceful shutdown |
| **Validation** | `Scripts/Verify-CI.ps1`, `Scripts/Verify-Smoke.ps1`; GfxTests config precedence |
| **Plan log** | [`Archived/plans/config-platform-hardening_Plan.md`](Archived/plans/config-platform-hardening_Plan.md), [`config-platform-hardening_Progress.md`](Archived/plans/config-platform-hardening_Progress.md) |
| **Hardening** | #7, #30, #31 |

- [x] **[P1]** config-platform-hardening — config instance (no file-static globals); platform inventory; recoverable VK frame errors — 2026-06-02

---

## P1 — vk-core-world-peel *(closed 2026-06-02)*

| | |
|--|--|
| **Outcome** | `WorldState` / debug UI in App; `BuildActiveRenderViews`; `PrepareFrameCpu(views)`; `Vk_*Context` slices; 0 `friend` on `Vk_Core` |
| **Validation** | `Scripts/Verify-CI.ps1`, `Scripts/Verify-Smoke.ps1` after each phase |
| **Plan log** | [`Archived/plans/vk-core-world-peel_Plan.md`](Archived/plans/vk-core-world-peel_Plan.md), [`vk-core-world-peel_Progress.md`](Archived/plans/vk-core-world-peel_Progress.md) |
| **Hardening** | #3, #6, #8, #9 |

- [x] **[P1]** vk-core-world-peel — scene CPU + debug UI ownership in Application; active views build in App; context injection replaces `friend` — 2026-06-02; commits `24d73b3`, `1392869`, `e341b0e`

---

## P1 — shader-bindless-policy *(closed 2026-06-09)*

| | |
|--|--|
| **Outcome** | Option A dogfood: `BINDLESS_RENDERDOC_OK` gate (#14), unified `RecordPassDrawsFromPacket` (#15), M7 perm freeze (#17); #18 codegen → Wishlist |
| **Validation** | `Scripts/Verify-CI.ps1`, `Scripts/Verify-Smoke.ps1`; log `materialPath=Bindless` on RTX |
| **Plan log** | [`Archived/plans/shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md), [`shader-bindless-policy_Progress.md`](Archived/plans/shader-bindless-policy_Progress.md) |
| **Hardening** | #14, #15, #17 |

- [x] **[P1]** shader-bindless-policy — Option A implement (#14–#17) — 2026-06-09

---

## P1 — rhi-swapchain-create (RHI-B1) *(closed 2026-06-09)*

| | |
|--|--|
| **Outcome** | compositeAlpha fallback, triple-buffer image count, create log, `NeedsSwapchainRebuild` precheck |
| **Validation** | `Scripts/Verify-CI.ps1`, `Scripts/Verify-Smoke.ps1` |
| **Plan log** | [`Archived/plans/rhi-swapchain-create_Plan.md`](Archived/plans/rhi-swapchain-create_Plan.md), [`rhi-swapchain-create_Progress.md`](Archived/plans/rhi-swapchain-create_Progress.md) |
| **Hardening** | #35 |

- [x] **[P1]** RHI-B1 — swapchain create hygiene — 2026-06-09

---

## P1 — rhi-camera-ubo (RHI-A2) *(closed 2026-06-09)*

| | |
|--|--|
| **Outcome** | Camera UBO in `PrepareFrameCpu` only; `UpdateEnvironment` in `DrawFrameGpu`; no view-0 overwrite |
| **Validation** | `Scripts/Verify-CI.ps1`, `Scripts/Verify-Smoke.ps1` |
| **Plan log** | [`Archived/plans/rhi-camera-ubo_Plan.md`](Archived/plans/rhi-camera-ubo_Plan.md), [`rhi-camera-ubo_Progress.md`](Archived/plans/rhi-camera-ubo_Progress.md) |
| **Hardening** | #34 |

- [x] **[P1]** RHI-A2 — multi-view camera UBO consistency (`UpdateEnvironment` split) — 2026-06-09

---

## P1 — rhi-slab-overflow (RHI-A1) *(closed 2026-06-09)*

| | |
|--|--|
| **Outcome** | `PrepareFrameCpu` fail-closed on `DrawPrep::Build()` slab overflow; GPU frame skipped |
| **Validation** | `Scripts/Verify-CI.ps1`; manual smoke + `Assert-SmokeLog.ps1` |
| **Plan log** | [`Archived/plans/rhi-slab-overflow_Plan.md`](Archived/plans/rhi-slab-overflow_Plan.md), [`rhi-slab-overflow_Progress.md`](Archived/plans/rhi-slab-overflow_Progress.md) |
| **Hardening** | #33 |

- [x] **[P1]** RHI-A1 — instance slab overflow gate in `PrepareFrameCpu` — 2026-06-09

---

## P2 — render-m2-prep §A CPU indirect *(closed 2026-06-09)*

| | |
|--|--|
| **Outcome** | `Gfx_DrawTemplate` + per-frame indirect/template SSBO upload; record via `vkCmdDrawIndexedIndirect`; `--legacy-direct-draw` fallback |
| **Validation** | MSBuild Debug\|x64; GfxTests; `Verify-Smoke.ps1` — log: `FillDrawTemplates`, `vkCmdDrawIndexedIndirect`, `drawCalls=1` |
| **Plan log** | [`Archived/plans/render-m2-prep_Plan.md`](Archived/plans/render-m2-prep_Plan.md) · [`Archived/plans/render-m2-prep_Progress.md`](Archived/plans/render-m2-prep_Progress.md) |
| **Hardening** | #13 |

- [x] **[P2]** Draw template SSBO + CPU `DrawIndexedIndirect` — 2026-06-09

- [x] **[P2]** `Gfx_Mesh::myIndexCount` at buffer build; record/indirect use GPU count — 2026-06-09

- [x] **[P2]** Per-draw RenderDoc tags via `char[128]` + `snprintf`; no heap in record hot path — 2026-06-09

- [x] **[P2]** `demoRotate: false` default; opt-in Z spin; sim time reset on scene reload — 2026-06-09

- [x] **[P2]** `lodEnabled: false` default; ImGui **CPU LOD** + CLI; draw stream gated — 2026-06-09

- [x] **[P2]** Mesh local AABB × transform; opaque depthBucket + transparent sort from bounds-center eye Z — 2026-06-09

---

## P2 — vulkan-rhi-p2 (RHI-B2–C3, B4) *(closed 2026-06-09)*

| | |
|--|--|
| **Outcome** | Three-layer swapchain recreate; `SURFACE_LOST` recovery; resource layer merge + batched upload; manifest descriptor pool; `Verify-ResizeSmoke.ps1` |
| **Validation** | MSBuild Debug\|x64; `Verify-Smoke.ps1`; `Verify-ResizeSmoke.ps1` |
| **Plan log** | [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md) §B2–C, [`vulkan-rhi-p2_Progress.md`](Archived/plans/vulkan-rhi-p2_Progress.md) |
| **Hardening** | #36–40, #43 |

- [x] **[P2]** RHI-B2 — `Recreate` three-layer split (wsi / extent / pipeline) — 2026-06-09
- [x] **[P2]** RHI-B3 — `VK_ERROR_SURFACE_LOST_KHR` surface + swapchain recovery — 2026-06-09
- [x] **[P2]** RHI-B4 — `Scripts/Verify-ResizeSmoke.ps1` + `CLI.md` — 2026-06-09
- [x] **[P2]** RHI-C1 — `Vk_ResourceContext` single owner; `Vk_Core` thin forwards — 2026-06-09
- [x] **[P2]** RHI-C2 — batched scene upload (`BeginSceneUploadBatch`) — 2026-06-09
- [x] **[P2]** RHI-C3 — descriptor pool sizing from manifest + policy max — 2026-06-09

---

## P0 — Verify & measure *(closed 2026-06-02)*

| | |
|--|--|
| **Outcome** | G0 CI + GfxTests + smoke scripts + perf JSONL; GitHub Actions `vulkan-desktop.yml` |
| **Validation** | `Scripts/Verify-CI.ps1`, `Scripts/Verify-Smoke.ps1`; [`SprintOutcomeValidation.md`](./SprintOutcomeValidation.md) § P0 |
| **Plan log** | [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md), [`ci-verification_Progress.md`](Archived/plans/ci-verification_Progress.md) |

- [x] **[P0]** G0 — GHA + `Verify-CI.ps1` (MSBuild Debug\|x64, shader drift, GfxTests) — 2026-06-02
- [x] **[P0]** G0 — `GfxTests.exe` SoA + CPU cull/batch golden — 2026-06-02
- [x] **[P0]** CI smoke + `Assert-SmokeLog.ps1`; `Verify-Bootstrap` / `Verify-Smoke` aligned — 2026-06-02
- [x] **[P0]** Automation contract: CI/agents require `--asset-root`; `CLI.md` / `bootstrap.md` — 2026-06-02
- [x] **[P0]** `engine.benchmark.json` vsync off; `forward-stage1.md` note — 2026-06-02
- [x] **[P0]** `--perf-log` JSONL + `Perf-JsonlSummary.ps1` — 2026-06-02
- [x] **[P0]** Adversarial archived claim (#26) in Progress closeout — 2026-06-02

---

*When closing a task: move here, tag sprint (`[S0]`…`[S8]`, `[parallel]`), note verification. Update `EngineArchitecture.md` when boundaries or render target change.*
