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
