# Progress: scene-load

**Plan:** [`scene-load_Plan.md`](scene-load_Plan.md)  
**Archived:** 2026-05-29 (docs hygiene batch)

## Closeout — 2026-05-29

- **Outcome:** Phases A–D: scene JSON v1 + manifest verify + scene-driven SoA/LOD/tables + GPU `UnloadScene` / `assetVerify` / `smoke.json` / `--smoke-frames` / ImGui Scene panel / `Docs/CLI.md` / `SceneJSON` guides.
- **Verification:** `VulkanDesktop.exe --no-validation --smoke-frames 2 --scene Data/Scenes/smoke.json` exit 0; log `UnloadScene: GPU scene resources released`, `Engine exited run loop normally`.
- **Deviations:** Phase D deferred until application-lifecycle landed (2026-05-27 handoff); then completed 2026-05-29.
