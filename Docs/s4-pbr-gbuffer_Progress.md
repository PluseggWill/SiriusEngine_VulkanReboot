# Progress: s4-pbr-gbuffer

**Plan:** [`s4-pbr-gbuffer_Plan.md`](s4-pbr-gbuffer_Plan.md)  
**Status:** Implementation complete (pending commit `.spv` + optional Sponza regen)

## 2026-06-12 — Steps 1–5

- **Files:** `GBuffer.frag`, `GBufferFrag_Bindless.frag`, `PbrDirect.glsl` (new), `DeferredLighting.frag`, `TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag`, `CompileShader_Glslc.bat`, `Util_LightingPanel.cpp`, `Vk_Types.h`, `Vk_DeferredLightingPass.cpp`, `Gfx_ClusterLighting.h`, `SceneJSON.md`, `forward-stage1.md`, `VulkanDesktop.vcxproj`
- **Verification:** MSBuild Debug|x64 OK; `GfxTests.exe` all passed

## 2026-06-12 — Step 6

- **Files:** `Scripts/Generate-SponzaScene.ps1` (`Get-SponzaMaterialMr` heuristics)
- **Verification:** `Generate-SponzaScene.ps1` blocked locally — missing `Data/Models/sponza/source/sponza.mtl` (run `Fetch-SponzaMcGuire.ps1` then regenerate). Existing `sponza.json` unchanged until fetch.

## 2026-06-12 — Step 7

- **Verification:**
  - `Verify-Smoke.ps1` exit 0
  - `Verify-CI.ps1` build + GfxTests OK; shader drift expected until `Shader_Generated/*.spv` committed (6 binaries updated)
  - `Verify-CI` full (clang-format): pre-existing failure in `ActiveViewsBuild.cpp` (unrelated)

## Closeout — 2026-06-12

- **Outcome:** G-buffer RT0.a = metallic, RT1.w = roughness; shared Cook-Torrance `PbrDirect.glsl` in deferred + forward; legacy spec/shininess ImGui disabled; Sponza MR generator ready.
- **Verification:** `powershell -File Scripts/Verify-Smoke.ps1` exit 0; MSBuild + GfxTests green.
- **Deviations:** full `Verify-CI` clang-format blocked by unrelated `ActiveViewsBuild.cpp`; manual Sponza visual parity optional post-push.

## 2026-06-12 — Assets vendored + pushed

- **Outcome:** McGuire Sponza (~109 MB) under `Data/Models/sponza/`; `.gitignore` un-ignores `sponza/**/*.obj`; `sponza.json` with MR heuristics; pushed `origin/S4` (`a0bba6d` data, `8454bce` lighting).
- **Verification:** `Fetch-SponzaMcGuire.ps1` + `Generate-SponzaScene.ps1` exit 0; `git push origin S4` OK.
