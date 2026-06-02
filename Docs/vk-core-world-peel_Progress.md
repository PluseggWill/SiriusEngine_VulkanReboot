# Progress: vk-core-world-peel

**Plan:** [`vk-core-world-peel_Plan.md`](vk-core-world-peel_Plan.md)  
**Parent:** Active-Plan **P1**

---

## 2026-06-02 — Phase 0 baseline

| Signal | Value |
|--------|-------|
| `friend class` in `Vk_Core.h` | 9 + `VkShaderEffectMeta` test |
| `GetSceneSoA` / `GetSceneTransformState` call sites | `Application.cpp` (loop); public API on `Vk_Core.h` |
| `DrawFrame` LOC (`Vk_Core.cpp`) | ~156 (lines 617–773) |
| `Vk_Core.cpp` / `.h` LOC | 1426 / 286 |
| ImGui `Begin` in `Vk_Core.cpp` | 2 (Multi-view window) |
| `Util*Panel::Build` in `DrawFrame` | 5 |

- **Verification:** `powershell -File Scripts/Verify-CI.ps1` — exit 0 (pre-change)

---

## 2026-06-02 — Phase 1 (WorldState) ✓

- **Files:** `App/WorldState.{h,cpp}`, `Application.{h,cpp}`, `Vk_Core.{h,cpp}`, `Vk_SceneHost.{h,cpp}`, `Vk_SwapchainHost.cpp`, `VulkanDesktop.vcxproj` (+ filters)
- **Landing:** Application owns `WorldState`; `BindWorldState` + `HasLoadedScene()` on core; scene CPU moved off `Vk_Core`; `Vk_SceneHost::LoadCpuState(WorldState&, Vk_Core&)`; getters removed; `Vk_Core.h` drops `Gfx_Lod` / `Gfx_SceneTransform` / `Gfx_SceneDesc` includes (forward declare `Gfx_SceneDesc` only)
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0; log `[SCENE] LoadSceneResources completed`, `[APP] Engine exited run loop normally`
---

## 2026-06-02 — Phase 2 (Debug UI peel) ✓

- **Files:** `App/DebugUIState.h`, `App/DebugOverlay.{h,cpp}`, `RenderCore/Vk_FrameCpuPrepResult.h`, `Vk_Core.*`, `Vk_ScenePasses.cpp`, `Application.*`
- **Landing:** `PrepareFrameCpu` + App panels + `DrawFrameGpu`; scene reload via `Application::TakePendingSceneReloadPath`; `Vk_Core` no `ImGui::Begin` / no pre-record `Util*Panel::Build` (lighting panel still after `RecordScene`)
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0
---

## 2026-06-02 — Phase 3 (view build delegate) ✓

- **Files:** `App/ActiveViewsBuild.{h,cpp}`, `RenderCore/Vk_ActiveRenderView.h`, `Vk_Core.*`, `Application.cpp`, vcxproj
- **Landing:** `BuildActiveRenderViews` in App (WorldState + DebugUI + fly camera); `PrepareFrameCpu` takes pre-built `Vk_ActiveRenderView[]`; frame hot path in `Vk_Core.cpp` no longer reads scene JSON for PiP
- **Metrics:** `friend class` 9; `Vk_Core.cpp` 1354 LOC; `Vk_Core.h` 270 LOC; `PrepareFrameCpu` ~90 LOC; `DrawFrameGpu` ~49 LOC
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0
---

## 2026-06-02 — Phase 4 (`Vk_*Context` / friend removal) ✓

- **New:** `Vk_DeviceContext`, `Vk_SwapchainContext`, `Vk_FrameContext`, `Vk_SceneGpuContext`, `Vk_PlatformContext` — public members on `Vk_Core`
- **Landing:** All 9 `friend class` + shader-test friend removed; peel modules use `aCore.my*Ctx` + promoted public helpers (`CreateInstance`, `QuerySwapChainSupport`, `PadUniformBufferSize`, …)
- **`Vk_ScenePasses`:** `RecordScene` takes `const DebugUIState&` (no `Vk_Core::DebugUI()` friend)
- **Shader test:** `VkShaderEffectMeta_RunLitBatchLayoutMismatchValidationTest(device, scene, core)` — no `friend`
- **Metrics:** `friend class` in `Vk_Core.h` → **0**
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0
- **Next:** Task close (archive plan/progress) or parallel P1 tracks (`config-platform-hardening`, …)
