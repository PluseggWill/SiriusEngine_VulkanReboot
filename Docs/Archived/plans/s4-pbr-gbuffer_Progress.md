# Progress: s4-pbr-gbuffer

**Plan:** [`Archived/plans/s4-pbr-gbuffer_Plan.md`](Archived/plans/s4-pbr-gbuffer_Plan.md)

## Closeout — 2026-06-12

- **Outcome:** G-buffer RT0.a = metallic, RT1.w = roughness; `PbrDirect.glsl` (clamp + Cook-Torrance + forward sun helper); deferred + forward parity; McGuire Sponza vendored; post-landing refactor (shared API, legacy push cleanup).
- **Verification:** `Verify-Smoke.ps1` exit 0; MSBuild Debug|x64 + GfxTests green; `Fetch-SponzaMcGuire.ps1` + `Generate-SponzaScene.ps1` exit 0 after vendor.
- **Deviations:** full `Verify-CI` clang-format fails on pre-existing `ActiveViewsBuild.cpp`; manual Sponza visual parity notes optional (§S4).
