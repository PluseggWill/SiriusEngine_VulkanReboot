# Progress: s2-init-hygiene

## Closeout — 2026-05-29

- **Outcome:** Removed dead `Vk_Core` init/record forwards (~15 one-line stubs); demo camera/env → `Vk_SceneHost::InitScenePresentation`; deleted `Gfx_BuildDemoResourceManifest`, `Gfx_BuildDemoLodTable`, `Util_DemoAssets.h`, `Gfx_ResourceManifest.cpp`; vcxproj/filters updated.
- **Verification:** MSBuild Debug|x64 exit 0; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0; log: `[EXTRACT] entities=9 draws=9`, `Smoke dwell reached`, `UnloadScene: GPU scene resources released`.
- **Deviations:** none
- **Plan:** [`s2-init-hygiene_Plan.md`](s2-init-hygiene_Plan.md)
