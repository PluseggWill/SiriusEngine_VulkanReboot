# Progress: shadow-clipz-vulkan

## 2026-07-13 — Plan + implement steps 1–3

- **Plan ref:** Steps 1–3 (matrix ZO, PbrShadow, GfxTests)
- **Files:** `VibeCoding/shadow-clipz-vulkan_Plan.md`, `VulkanDesktop/Gfx/Gfx_LightingMath.h`, `VulkanDesktop/Shader/PbrShadow.glsl`, `VulkanDesktop/GfxTests/GfxTests_Main.cpp`, regenerated `Shader_Generated/*` consumers of `PbrShadow.glsl`
- **What changed:** Shadow ortho → `glm::orthoRH_ZO` (reverse-Z far/near swap kept); compare depth = clip Z; map helper identity; comments + unit tests updated for Vulkan [0,1] contract.
- **Verification:** MSBuild Debug|x64 OK; `GfxTests: all passed`; shader regen + drift clean vs index.

## 2026-07-13 — Step 4 visual sign-off

- **Plan ref:** Step 4
- **What changed:** User confirmed shadow compare/lighting look correct after ZO + identity compare fix.
- **Verification:** manual Sponza / HybridDeferred (user).

## 2026-07-13 — Steps 5–8 hardening + residual sweep

- **Plan ref:** Steps 5–8
- **Files:** `.cursor/rules/vulkan-clip-depth.mdc`, `vulkan-render-pass-pitfalls.mdc`, `Gfx_LightingMath.h`, `Shader/ClipDepth.glsl`, `PbrShadow.glsl`, `GfxTests_Main.cpp`, `Vk_ShadowMapPass.cpp`, `Vk_Camera.cpp`, `AoCommon.glsl`, `DeferredLighting.frag`, `Docs/EngineArchitecture.md`, regenerated SPVs
- **What changed:** Named ZO/viewport helpers; Cursor rule; viewport anti-regression test; reverse-Z OOB border → black; fixed stale comments.
- **Verification:** `GfxTests: all passed`; `powershell -File Scripts/Verify-CI.ps1` exit 0.

## Closeout — 2026-07-13

- **Outcome:** Directional shadow write/compare aligned on Vulkan ZO `[0,1]` clip Z; helpers + GfxTests + Cursor rule guard against OpenGL `*0.5+0.5` Z remap regression. User visual sign-off on Sponza.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; GfxTests viewport contract + reverse-Z ZO samples.
- **Deviations:** Task lived under `VibeCoding/` (not Docs WIP slot). Main camera ZO migration deferred.
