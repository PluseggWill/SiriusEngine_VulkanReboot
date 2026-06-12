# Progress: lighting-shadow-refactor

## 2026-06-13 — Review (Phase 1)

- **Scope:** Full audit of S5 shadow + IBL vs Khronos / LearnOpenGL / Epic split-sum.
- **Findings (P0):** frustum-culled shadow casters; prefilter 1 mip vs shader LOD 4; duplicate UBO matrix upload.
- **Plan:** [`lighting-shadow-refactor_Plan.md`](lighting-shadow-refactor_Plan.md)

## 2026-06-13 — Steps 1–4

- **Shadow:** `myShadowCasterPass`, direct indexed shadow draws, single `UpdateLightingGlobals` after pass, ForwardLit parity.
- **Shaders:** `PbrShadow.glsl` / `PbrIbl.glsl` split; `(1-F)*kD` IBL diffuse; `Pbr_EvalSceneAmbient` + sun-only deferred shadow.
- **IBL:** 5-level prefilter mipgen; `iblParams.z` max mip.
- **Verification:** MSBuild Debug|x64 exit 0; GfxTests exit 0.

## 2026-06-13 — Shadow visibility follow-up

- **Issue:** Debug showed depth map; lit only attenuated direct sun — IBL washed out shadows.
- **Fix:** `Pbr_ModulateAmbientForSunShadow`; deferred debug = compare visibility (parity with forward).

## Closeout — 2026-06-13

- **Outcome:** HybridDeferred + ForwardLit directional shadows stable on Sponza; IBL prefilter mips + energy-consistent diffuse; shader module split for maintainability.
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0; manual Sponza lit + shadow debug sign-off.
- **Deviations:** `PBR_SHADOWED_IBL_SCALE` (0.25) is a readability tuning knob, not full physical skylight bounce; binding 11 depth read kept for tooling.
