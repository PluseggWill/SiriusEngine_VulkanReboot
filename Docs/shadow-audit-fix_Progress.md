# Progress: shadow-audit-fix

## 2026-06-13 — Implement

- **Removed:** `Pbr_SunHemisphereIblScale`, frustum UV reject, `VK_CULL_MODE_BACK_BIT`.
- **Kept:** light eye `focus + sunDir * reach`, scene-scaled ortho + texel snap, viewport compare depth (`z*0.5+0.5`), scaled depth bias.
- **Fixed:** lookAt alternate up when `|dot(+Z, sunDir)| >= 0.85`; **BACK cull** (FRONT cull caused light leak via back-face depth); Khronos fixed depth bias `-1.4` (scaled bias was ~0 for Sponza).
- **Files:** `PbrShadow.glsl`, `DeferredLighting.frag`, `LightingBindings.glsl`, `TriangleFrag_Lit*.frag`, `Vk_ShadowMapPass.cpp`, `Gfx_LightingMath.h`.
- **Verification:** `GfxTests.exe` exit 0; `Verify-Smoke.ps1` exit 0; shader SPIR-V regenerated (commit with code). Manual Sponza + shadow debug view pending user.
