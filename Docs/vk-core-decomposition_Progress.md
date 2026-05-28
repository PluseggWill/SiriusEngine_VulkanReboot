# Progress: vk-core-decomposition

**Plan:** [`vk-core-decomposition_Plan.md`](vk-core-decomposition_Plan.md)

---

## 2026-05-27 — Phase 2: plan authored

- **Plan ref:** Full plan from confirmed Phase 1 landing (three milestones, M1 context, M2 spin out, Gfx+RenderCore, Phase D parallel, RHI direction)
- **Files:** `Docs/vk-core-decomposition_Plan.md`, `Docs/vk-core-decomposition_Progress.md`, `Docs/README.md`, `Docs/EngineArchitecture.md`
- **What changed:** Design doc only; no `VulkanDesktop/` code yet
- **Verification:** Build/smoke-run N/A — doc-only

---

## 2026-05-27 — M1: Vk_ResourceContext + tables load peel

- **Plan ref:** M1.0–M1.5
- **Files:** `RenderCore/Vk_ResourceContext.{h,cpp}`, `Vk_ResourceTables.{h,cpp}`, `Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `.filters`
- **What changed:** `Vk_ResourceContext` binds device/allocator and owns `CreateImageView` for table load. `Vk_ResourceTables::LoadFromManifest` takes context instead of `Vk_Core&`; removed `friend class Vk_Core`. Deletion-queue lambdas capture allocator/device by value. `SyncResourceContext()` after VMA init and before manifest load.
- **Verification:** MSBuild Debug|x64 exit 0; 4s smoke OK; log `[RESOURCE-TABLE] meshes=8 materials=7 textures=6`; `[EXTRACT] entities=9 draws=9`; grep: no `friend.*Vk_Core` in `VulkanDesktop/`

---

## 2026-05-27 — M2: draw stream prep + demo sim out of Vk_Core

- **Plan ref:** M2.0–M2.4
- **Files:** `Gfx/Gfx_FrameDrawStream.{h,cpp}`, `Gfx/Gfx_DemoSceneSim.{h,cpp}`, `RenderCore/Vk_FrameDrawPrep.{h,cpp}`, `Vk_Core.{h,cpp}`, `App/Application.cpp`, vcxproj/filters
- **What changed:** CPU draw-list build in Gfx (`Gfx_BuildFrameDrawStream`); GPU slab fill in `Vk_FrameDrawPrep`. Demo Z-spin via `Gfx_TickDemoSceneTransforms` from Application before `Render`. `Vk_Core::DrawFrame` delegates to `myDrawPrep.Build`; record path reads `myDrawPrep.myExtract` / batch runs.
- **Verification:** MSBuild Debug|x64 exit 0; smoke OK (`[EXTRACT] entities=9 draws=9`, `FillInstanceSlab: wrote 9`); grep `Vk_Core.cpp`: no `Gfx_ExtractDrawInstances`, `ApplyDemoTransformAnimation`, `FillInstanceSlab`

---

## 2026-05-27 — M3: DrawFrame structure + task close

- **Plan ref:** M3.1–M3.5
- **Files:** `Vk_Core.{h,cpp}`, `Docs/EngineArchitecture.md`, `Docs/SprintPlan.md`, `Docs/README.md`, `Docs/vk-core-decomposition_Plan.md`
- **What changed:** `DrawFrame` labeled sync / CPU prep / GPU record / submit-present; RHI module comments on `Vk_Core`. Sprint task archived; architecture §3.1/§9 updated.
- **Verification:** MSBuild Debug|x64 exit 0; smoke OK; `Vk_Core.cpp` ~2260 lines (was ~2393 pre-peel); grep: no Gfx extract/cull/sort/batch in `Vk_Core.cpp`

---

## 2026-05-28 — Phase 2 #1: Vk_ResourceContext v2

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 task ledger #1 (`Vk_ResourceContext` v2)
- **Files:** `RenderCore/Vk_ResourceContext.{h,cpp}`, `RenderCore/Vk_Core.cpp`, `RenderCore/Vk_ResourceTables.cpp`, `RenderCore/Vk_Types.{h,cpp}`, `Util/Util_Loader.{h,cpp}`, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:** Expanded `Vk_ResourceContext` to own load-time helper operations (`CreateBuffer`/`CreateImage`/copy/transition/mipmap); `SyncResourceContext` now binds full handles (device, allocator, queues, pools, queue-family ids, physical device). Loader and mesh upload chains now consume explicit context, removing `Vk_Core::GetInstance()` usage in this path. Phase 2 tracking docs consolidated under `vk-core-decomposition_{Plan,Progress}.md`.
- **Verification:** MSBuild Debug|x64 exit 0; 4s smoke-run OK; runtime log shows `LoadSceneResources completed`, `RESOURCE-TABLE meshes=8 materials=7 textures=6`, `EXTRACT entities=9 draws=9`, and no new `[ERROR]` in init path.

---

## 2026-05-28 — Phase 2 #2: Vk_RenderDevice

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 task ledger #2 (`Vk_RenderDevice`)
- **Files:** `RenderCore/Vk_RenderDevice.{h,cpp}`, `RenderCore/Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `VulkanDesktop.vcxproj.filters`
- **What changed:** Added `Vk_RenderDevice` module and moved part-1 device bootstrap orchestration from `Vk_Core::InitRenderDevice` into `Vk_RenderDevice::Init` (instance/surface/physical device/queue families/logical device/command pools/allocator + bindless path selection). `Vk_Core` now delegates this slice to the new module.
- **Verification:** MSBuild Debug|x64 exit 0; 4s smoke-run OK; runtime log shows `LoadSceneResources completed`, `EXTRACT entities=9 draws=9`, and no new `[ERROR]` during init path.

---

## 2026-05-28 — Phase 2 #3: Vk_SwapchainHost

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 task ledger #3 (`Vk_SwapchainHost`)
- **Files:** `RenderCore/Vk_SwapchainHost.{h,cpp}`, `RenderCore/Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `VulkanDesktop.vcxproj.filters`
- **What changed:** Added `Vk_SwapchainHost` module and moved swapchain-host init orchestration from `Vk_Core::InitRenderDevice` into `Vk_SwapchainHost::Init` (`CreateSwapChain`, `CreateRenderPass`, descriptor-set layout creation, color/depth resources, framebuffers).
- **Verification:** MSBuild Debug|x64 exit 0; 4s smoke-run OK; no new `[ERROR]` in startup path.

---

## 2026-05-28 — Phase 2 #4: Vk_DescriptorSystem

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 task ledger #4 (`Vk_DescriptorSystem`)
- **Files:** `RenderCore/Vk_DescriptorSystem.{h,cpp}`, `RenderCore/Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `VulkanDesktop.vcxproj.filters`
- **What changed:** Added `Vk_DescriptorSystem` module and delegated descriptor orchestration from `Vk_Core`: device-time layouts (`CreateDescriptorSetLayout`, bindless set/pipeline layout) and scene-time descriptors (sampler, pool, set allocation, material/bindless resources).
- **Verification:** MSBuild Debug|x64 exit 0; 4s smoke-run OK; no new `[ERROR]` in startup path.

---

## 2026-05-28 — Phase 2 #5: Vk_GfxPipelineCache

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 task ledger #5 (`Vk_GfxPipelineCache`)
- **Files:** `RenderCore/Vk_GfxPipelineCache.{h,cpp}`, `RenderCore/Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `VulkanDesktop.vcxproj.filters`
- **What changed:** Added `Vk_GfxPipelineCache` module and delegated scene pipeline orchestration (`CreateGfxPipeline`, optional bindless pipeline creation) from `Vk_Core` scene load and swapchain-recreate paths.
- **Verification:** MSBuild Debug|x64 exit 0; 4s smoke-run OK; no new `[ERROR]` in startup path.

---

## 2026-05-28 — Phase 2 #6/#7/#8/#9: ScenePasses + FrameUniformUploader + SceneHost + PlatformFrame

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 task ledger #6/#7/#8/#9
- **Files:** `RenderCore/Vk_ScenePasses.{h,cpp}`, `RenderCore/Vk_FrameUniformUploader.{h,cpp}`, `RenderCore/Vk_SceneHost.{h,cpp}`, `RenderCore/Vk_PlatformFrame.{h,cpp}`, `RenderCore/Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `VulkanDesktop.vcxproj.filters`
- **What changed:** 
  - `Vk_ScenePasses`: `DrawFrame` now delegates scene+imgui pass record orchestration to `RecordFramePasses`.
  - `Vk_FrameUniformUploader`: `DrawFrame` now delegates per-frame UBO uploads to `Update`.
  - `Vk_SceneHost`: `LoadSceneResources` now delegates scene CPU-state setup (id tables, SoA, LOD, debug logical mesh id) to `LoadCpuState`.
  - `Vk_PlatformFrame`: `InitWindow` and `BeginPlatformFrame` now delegate GLFW/frame tick orchestration to module methods.
- **Verification:** MSBuild Debug|x64 exit 0; 4s smoke-run OK; no new `[ERROR]` in startup path.

---

## 2026-05-28 — Phase 2 continuation: body migration + DrawFrame swapchain host tightening

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 continuation record (2026-05-28)
- **Files:** `RenderCore/Vk_Core.cpp`, `RenderCore/Vk_SwapchainHost.{h,cpp}`, `RenderCore/Vk_DescriptorSystem.{h,cpp}`, `RenderCore/Vk_GfxPipelineCache.{h,cpp}`, `RenderCore/Vk_ScenePasses.{h,cpp}`, `RenderCore/Vk_FrameUniformUploader.cpp`
- **What changed:**
  - Migrated remaining implementation bodies from `Vk_Core.cpp` into module owners (`Vk_SwapchainHost`, `Vk_DescriptorSystem`, `Vk_GfxPipelineCache`, `Vk_ScenePasses`, `Vk_FrameUniformUploader`), leaving `Vk_Core` wrappers as thin delegation points.
  - Moved `DrawFrame` swapchain acquire/present flow to `Vk_SwapchainHost::AcquireNextImage` and `Vk_SwapchainHost::SubmitAndPresent`, including out-of-date/suboptimal handling.
  - Added one-time smoke diagnostics in `Vk_SwapchainHost` to confirm delegated runtime path (`Acquire path delegated...`, `Present path delegated...`).
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run reached main loop and first perf snapshot; runtime log contains both delegation diagnostics and no new render-path `[ERROR]`.

---

## 2026-05-28 — Phase 2 readability pass: module cleanup + comments

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 continuation quality follow-up (readability/maintainability)
- **Files:** `RenderCore/Vk_SwapchainHost.cpp`, `RenderCore/Vk_DescriptorSystem.cpp`, `RenderCore/Vk_ScenePasses.cpp`, `RenderCore/Vk_FrameUniformUploader.cpp`
- **What changed:**
  - Restructured dense one-line logic in `Vk_SwapchainHost::CreateSwapChain` into explicit staged assignments (capabilities -> create info -> image views -> deferred cleanup), reducing cognitive load for swapchain lifecycle changes.
  - Standardized several descriptor-path declarations and error branches in `Vk_DescriptorSystem` to make ownership/capture boundaries clearer for deletion-queue maintenance.
  - Added high-signal contract comments in descriptor layout setup and per-draw dynamic-offset binding sites to document shader/descriptor coupling and instance-slab usage.
  - Added env-UBO slab slicing note in `Vk_FrameUniformUploader::Update` to clarify per-frame offset behavior.
- **Verification:** MSBuild `Debug|x64` exit 0; 4s smoke-run on `x64\Debug\VulkanDesktop.exe` exited cleanly; runtime log includes delegated acquire/present lines plus normal extract/batch/instance-slab signals and no new `[ERROR]`.

---

## 2026-05-28 — Phase 2 readability pass: Vk_Core comment hygiene

- **Plan ref:** `vk-core-decomposition_Plan.md` -> Phase 2 continuation quality follow-up (`Vk_Core` maintainability)
- **Files:** `RenderCore/Vk_Core.cpp`
- **What changed:**
  - Replaced legacy tutorial-style/step-number comments with module-oriented intent comments (render orchestration, ownership boundaries, queue/present contracts).
  - Removed stale or low-signal TODO/comment noise and clarified teardown/initialization/descriptors/swapchain notes to match current decomposition state.
  - Added key contract comments in `Render`/`DrawFrame` around frame-slot rotation and `Vk_FrameDrawPrep` output consumption boundary.
- **Verification:** Lints clean on `Vk_Core.cpp`; build/smoke-run not re-executed in this comment-only batch.
