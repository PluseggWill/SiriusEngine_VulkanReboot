# ImGui layout — Progress

## Closeout — 2026-06-12

- **Outcome:** Scheme A — View menu; Performance top-right with collapsed vsync/latency/draw sections; Engine Debug bottom-left TabBar (Scene | Render | Camera | Views); Objective top-center HUD; Lighting moved to App overlay; `io.IniFilename` → `%LOCALAPPDATA%/SiriusEngine/imgui.ini`; R/F12 gated on `!WantCaptureKeyboard`.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; GfxTests all passed.
- **Deviations:** none.
