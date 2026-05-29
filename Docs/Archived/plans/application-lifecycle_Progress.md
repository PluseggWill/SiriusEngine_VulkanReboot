# Progress: application-lifecycle

**Plan:** [`application-lifecycle_Plan.md`](application-lifecycle_Plan.md)  
**Archived:** 2026-05-29 (docs hygiene batch)

## Closeout — 2026-05-27

- **Outcome:** `Application` orchestrates InitApp → verify → `InitRenderDevice` / `LoadSceneResources` / Update+Render / `UnloadScene` / `Shutdown`; `Vk_Core` split init vs scene load; removed `Run`, `SetLoadedScene`, `InitVulkan`, `MainLoop`.
- **Verification:** MSBuild Debug|x64 exit 0; smoke shows `[APP] InitRenderDevice` before `[SCENE] LoadSceneResources`, `entities=9 draws=9`, clean shutdown.
- **Deviations:** `UnloadScene()` CPU-only at close; GPU tables kept until `Shutdown()` for deletion-queue ordering (documented in plan).
