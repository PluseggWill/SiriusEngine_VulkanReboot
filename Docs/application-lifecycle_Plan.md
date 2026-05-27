# Plan: application-lifecycle

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Done (2026-05-27)  
**Related:** [`scene-load_Plan.md`](scene-load_Plan.md) Handoff, [`EngineArchitecture.md`](EngineArchitecture.md) §3, [`SprintPlan.md`](SprintPlan.md) § S2

## Problem

`VulkanDesktop::main` parses/verifies the scene before Vulkan, but `Vk_Core::Run()` still bundles window init, device init, **scene SoA + GPU tables inside `InitVulkan`**, and the frame loop. That blocks `UnloadScene`, scene reload (Phase D), and a clean Application layer.

## Goals

1. `App/Application` orchestrates lifecycle stages with `[APP]` logging.
2. `InitRenderDevice` vs `LoadSceneResources` split in `Vk_Core` (no SoA/manifest in device init).
3. `UnloadScene` partial teardown (CPU scene state only; GPU resource tables finalized in `Shutdown`).
4. Thin scheduler: `Update` (input/camera/demo) vs `Render` (acquire/record/present).
5. Default `demo.json` behavior unchanged.

## Non-goals

- scene-load Phase D (mid-run reload, `smoke.json`, strict/warn policy)
- Central config expansion, `UtilInput` move out of `Vk_Core`, full `Vk_Core` decomposition
- Multi-view, S3 GPU cull

## Design decisions (confirmed 2026-05-27)

| Item | Decision |
|------|----------|
| Module | `VulkanDesktop/App/Application.{h,cpp}` |
| Scheduler | Same task: `Update` / `Render` after lifecycle split |
| `UnloadScene` | Partial: clear SoA/LOD/extract/scene desc; keep GPU-backed resource tables until `Shutdown` (deletion queue frees) |
| `Vk_Core` | Keep singleton; Application orchestrates |
| Scene ownership | `Application` holds `Gfx_SceneDesc`; `main` only boots `Application::Run` |

## Target flow

```text
main → Application::Configure → Application::Run
  InitApp (config, validation on Vk_Core)
  LoadSceneDesc + VerifyManifest
  Vk_Core::InitWindow
  Vk_Core::InitRenderDevice
  Vk_Core::LoadSceneResources(scene)
  while (!ShouldClose) { Update(dt); Render(); }
  Vk_Core::UnloadScene
  Vk_Core::Shutdown
```

## Files

| Area | Paths |
|------|--------|
| New | `VulkanDesktop/App/Application.{h,cpp}` |
| Change | `VulkanDesktop/VulkanDesktop.cpp`, `RenderCore/Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `.filters` |
| Docs | `Docs/application-lifecycle_Progress.md`, `EngineArchitecture.md` §3.1, `SprintPlan.md` |

## Implementation steps

- [x] L0 — Plan + progress files
- [x] L1 — `Application` + thin `main`
- [x] L2 — Split `InitVulkan` → `InitRenderDevice` + `LoadSceneResources`
- [x] L3 — `UnloadScene` + `Shutdown`; removed public `SetLoadedScene` / `Run`
- [x] L4 — `Update` / `Render`; Application drives loop
- [x] L5 — Docs sync + build/smoke-run

## Verification

1. `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64` — exit 0
2. `x64\Debug\VulkanDesktop.exe --help`
3. Minimized ~4s run: `[APP]` lifecycle, `[SCENE]` SoA/tables after device init, `entities=9 draws=9`, `[APP] UnloadScene`
4. Grep: no `SetLoadedScene` / `InitVulkan` scene populate in public boot path

## Risks / rollback

- Init order regression → mirror pre-split `InitVulkan` line order in plan step L2
- Revert single PR restores Phase C boot path
