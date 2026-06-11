# P4 — Vertical slice v0 — Progress

## Closeout — 2026-06-11

- **Outcome:** `Data/Scenes/slice.json` + `Config/engine.slice.json`; scene `objective` JSON → reach campfire with 120s limit; ImGui HUD + `[SLICE]` log tokens; **R** / Scene panel restart without process exit; GfxTests `TestSliceSceneAssets`.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; `powershell -File Scripts/Verify-Smoke.ps1` exit 0.
- **Deviations:** GfxTests uses lightweight JSON asset walk (no `Gfx_SceneLoader` link in GfxTests.exe — avoids GLFW include chain).
