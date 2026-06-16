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

## 2026-06-16 — Step 3 (App_PlatformHost peel)

- **Plan ref:** Step 3.1–3.3
- **Files:** `Application.*`, `App_PlatformHost.*`, `Vk_Renderer.*`, `Vk_RenderDevice.*`, project files
- **What changed:**
  - New `App_PlatformHost`: owns GLFW window + poll/delta/close + `CreateSurface(Vk_RhiDevice&)`/`RecreateSurface`
  - `Application` main loop drives platform host (input sample / ImGui frame), renderer only consumes window + timestamps
  - `Vk_Renderer` no longer initializes/destroys GLFW; delegates surface create/recreate to platform host; exposes `BindPlatformHost`/`SetPlatformWindow`
  - Deleted `Vk_PlatformFrame.{h,cpp}`; platform frame orchestration now fully in App layer
- **Verification:** `Verify-CI.ps1` Debug exit 0

## 2026-06-16 — Step 4 (renderer skeleton + scene DTO + contexts)

- **Plan ref:** Step 4.1, 4.2, 4.3
- **Files:** `Gfx_SceneGpuLoadParams.h`, `Application.cpp`, `Vk_Renderer.h`, `Vk_Renderer.cpp`, `Vk_RendererContexts.h`, `Vk_ScenePasses.*`
- **What changed:**
  - `Vk_Renderer::LoadSceneGpuResources` signature changed to `LoadSceneGpuResources(const Gfx_SceneGpuLoadParams&)`
  - RenderCore scene-load path no longer depends on `WorldState`; consumes App-built scene DTO pointers
  - `Application` now builds and passes `Gfx_SceneGpuLoadParams` from `WorldState` before scene GPU activation
  - Added `Vk_RendererContexts` and switched scene/imGui pass entrypoints to consume contexts bundle
- **Verification:** `Verify-CI.ps1` Debug exit 0

## 2026-06-16 — Step 5–8 closeout (FG v2 + legacy deletion)

- **Plan ref:** Step 5, Step 6, Step 7, Step 8
- **Files:** `Vk_FrameGraph.*`, `Vk_FgResourceRegistry.*`, `Vk_FgBarrierCompiler.*`, `Gfx_FramePacketValidation.*`, `Vk_GBufferPass.cpp`, `Vk_ScenePasses.cpp`, `Vk_FrameDrawPrep.cpp`, `Vk_GfxPipelineCache.cpp`, docs + project files
- **What changed:**
  - Added FG v2 entrypoints: `Vk_FrameGraph::Execute` + resource registry + barrier compiler
  - Removed legacy FG v1 and packet validator wrappers: deleted `Vk_FrameGraphBuilder.*` and `Vk_RenderBackend.*`
  - Promoted packet validation to Gfx boundary helper: `Gfx_FramePacketValidation::*`
  - Removed global shader path variables; moved to `Vk_Renderer` session state (`mySceneVertShaderPath`/`mySceneFragShaderPath`/`mySceneBindlessFragShaderPath`)
  - Synced architecture + roadmap docs for de-singleton and FG v2 closeout
- **Verification:** `Verify-CI.ps1` Debug exit 0

## Remaining (plan steps 2.2 optional polish)

- `Vk_ResourceContext` → hold `Vk_RhiDevice&` (optional polish)
