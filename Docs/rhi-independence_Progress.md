# Progress: rhi-independence

## 2026-06-16 — Step 1 + partial 4–6 + Step 5

- **Plan ref:** Steps 1, 4 (rename), 5, 8 (delete Vk_Core)
- **Files:** `RenderContract/*`, `Gfx/*` aliases, `Vk_Renderer.*`, `Application.*`, `Vk_FrameDrawPrep.*`, deleted `Vk_Core.*`
- **What changed:**
  - `RenderContract/` — `GpuAoSettings`, `GpuPostSettings`, `GpuLightingGlobals`, `GpuEnvironmentData`, `GpuAoMethod`
  - `Vk_Core` → `Vk_Renderer`; removed `GetInstance()`; `Application` owns renderer
  - CPU prep uplift: `Gfx_BuildViewFramePacket` only in App; `Vk_FrameDrawPrep::UploadFromPacket` in RenderCore
- **Verification:** `Verify-CI.ps1` exit 0

## Remaining (plan steps 2–3, 7, partial 4/8)

- `Vk_RhiDevice` peel from `Vk_Renderer`
- `App_PlatformHost` (GLFW/ImGui out of RenderCore)
- `LoadSceneGpuResources(Gfx_SceneGpuLoadParams)` — drop `WorldState&`
- FG v2 (`Vk_FrameGraph`, resource registry, barrier compiler)
- GfxTests headless `Vk_RhiDevice` construct
- `EngineArchitecture.md` policy sync at closeout
