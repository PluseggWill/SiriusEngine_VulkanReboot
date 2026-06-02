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
- **Next:** Phase 2 — `DebugUIState`, `PrepareFrameCpu` / `DrawFrameGpu`, panels out of `DrawFrame`
