# Progress: application-lifecycle

## 2026-05-27 — Plan (Phase 2)

- **Plan ref:** L0
- **Files:** `Docs/application-lifecycle_Plan.md`, `Docs/application-lifecycle_Progress.md`
- **What changed:** Plan from confirmed Q1A–Q5A landing details.
- **Verification:** N/A (documentation only)

## 2026-05-27 — L1–L5 implementation

- **Plan ref:** L1–L5
- **Files:** `VulkanDesktop/App/Application.{h,cpp}`, `VulkanDesktop/VulkanDesktop.cpp`, `RenderCore/Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `.filters`, `Docs/application-lifecycle_Plan.md`, `Docs/application-lifecycle_Progress.md`, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:**
  - `Application` orchestrates lifecycle with `[APP]` logs; `main` only configures window size/extensions.
  - `Vk_Core`: `InitRenderDevice` (device/swapchain/frame infra) vs `LoadSceneResources` (SoA, LOD, pipelines, manifest); `Update`/`Render`/`ShouldClose`/`UnloadScene`/`Shutdown`.
  - Removed `Run()`, `SetLoadedScene()`, `InitVulkan()`, `MainLoop()`.
- **Note:** `UnloadScene()` is CPU-only; `myResourceTables` are intentionally kept until `Shutdown()` so `DeletionQueue` can free GPU buffers/images.
- **Verification:**
  - Build: MSBuild Debug|x64 — exit 0
  - Smoke: `--help`; graceful close ~3s — `[APP] InitRenderDevice` before `[SCENE] LoadSceneResources`; `entities=9 draws=9`; `[APP] UnloadScene` + `Shutdown`
  - Grep: no `SetLoadedScene` / `InitVulkan` in `VulkanDesktop/`
