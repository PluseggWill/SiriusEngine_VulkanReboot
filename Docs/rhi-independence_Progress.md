# Progress: rhi-independence

## 2026-06-16 — Step 1 + partial 4–6 + Step 5

- **Plan ref:** Steps 1, 4 (rename), 5, 8 (delete Vk_Core)
- **Files:** `RenderContract/*`, `Gfx/*` aliases, `Vk_Renderer.*`, `Application.*`, `Vk_FrameDrawPrep.*`, deleted `Vk_Core.*`
- **What changed:**
  - `RenderContract/` — `GpuAoSettings`, `GpuPostSettings`, `GpuLightingGlobals`, `GpuEnvironmentData`, `GpuAoMethod`
  - `Vk_Core` → `Vk_Renderer`; removed `GetInstance()`; `Application` owns renderer
  - CPU prep uplift: `Gfx_BuildViewFramePacket` only in App; `Vk_FrameDrawPrep::UploadFromPacket` in RenderCore
- **Verification:** `Verify-CI.ps1` exit 0

## 2026-06-16 — Step 2 (Vk_RhiDevice peel)

- **Plan ref:** Steps 2.1, 2.3 (2.2 deferred — `Vk_ResourceContext` still uses `Bind()` handles, not `Vk_RhiDevice&`)
- **Files:** `Vk_RhiDevice.{h,cpp}`, `Vk_Renderer.{h,cpp}`, all pass modules (`myRhi.myDeviceCtx`), `GfxTests_*`, `VulkanDesktop.vcxproj`
- **What changed:**
  - New `Vk_RhiDevice`: device context + resource context; VMA init, buffer/image/shader/barrier/format helpers
  - `Vk_Renderer` embeds `myRhi`; factory APIs forward to `myRhi.*`; `GetResourceContext()` → `myRhi.myResourceContext`
  - GfxTests: `TestRhiDeviceHeadlessConstruct` (headless `VkInstance` create/destroy)
- **Verification:** `Verify-CI.ps1` Debug exit 0

## Remaining (plan steps 2.2, 3, 7, partial 4/8)

- `Vk_ResourceContext` → hold `Vk_RhiDevice&` (optional polish)
- `App_PlatformHost` (GLFW/ImGui out of RenderCore)
- `LoadSceneGpuResources(Gfx_SceneGpuLoadParams)` — drop `WorldState&`
- FG v2 (`Vk_FrameGraph`, resource registry, barrier compiler)
- `EngineArchitecture.md` policy sync at closeout
