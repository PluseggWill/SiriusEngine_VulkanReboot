# ImGui layout — Tab merge + Performance collapse

**Status:** Closed (2026-06-12)  
**Scope:** App/Util ImGui panels only; no RenderCore draw-path change.

## Problem

Seven independent ImGui windows share left-column `(10, y)` anchors; Performance and Objective both use `(10,10)`, Camera and Render Debug both use `(10,420)`. Overlap makes debug UI unusable. Lighting panel builds inside `Vk_Core::DrawFrameGpu` while other panels build in Application.

## Non-goals

- ImGui docking branch / `DockSpaceOverViewport`
- Render pass or descriptor changes
- New debug features beyond layout

## Touch list

| Area | Files |
|------|--------|
| Orchestration | `App/DebugOverlay.cpp`, `App/DebugUIState.h`, `App/Application.cpp` |
| Panels | `Util_*Panel.cpp/h`, `Util_StatsOverlay.cpp/h` |
| Objective HUD | `Gfx/Gfx_ObjectiveRuntime.cpp/h` |
| Init | `Util/Util_ImGuiLayer.cpp` |
| RenderCore peel | `RenderCore/Vk_Core.cpp` (remove Lighting Build) |

## Steps

1. Add `DebugUIState::PanelVisibility` + View menu (`BeginMainMenuBar`).
2. Split each panel into `BuildContents` (no `Begin`/`End`); DebugOverlay owns windows.
3. **Engine Debug** (bottom-left, resizable): TabBar — Scene | Render (debug + lighting) | Camera | Views.
4. **Performance** (top-right): `CollapsingHeader` for vsync / latency / draw sections (default collapsed).
5. **Objective** (top-center HUD); respect visibility toggle.
6. Move Lighting + Render Debug into `BuildDebugOverlayPanels`; remove from `DrawFrameGpu`.
7. Set `io.IniFilename` under `%LOCALAPPDATA%/SiriusEngine/`; gate `R` / `F12` with `!io.WantCaptureKeyboard`.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- Manual: panels no longer overlap at first launch; View menu toggles work; RMB fly camera + Scene reload OK

## Risks

- Lighting env UBO was one frame late when built in `DrawFrameGpu`; moving to Application aligns with Render Debug (same-frame patch before `UpdateEnvironment`).
