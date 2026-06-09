# Progress: render-m2-prep

**Plan:** [`render-m2-prep_Plan.md`](render-m2-prep_Plan.md)  
**Scope:** P2 render path prep (§A–F)

## Closeout — §A CPU indirect (2026-06-09)

- **Outcome:** `Gfx_DrawTemplate` + CPU `vkCmdDrawIndexedIndirect`; `--legacy-direct-draw` fallback.
- **Verification:** GfxTests + `Verify-Smoke.ps1` exit 0.
- **Deviations:** none

## Closeout — §B myIndexCount (2026-06-09)

- **Outcome:** `Gfx_Mesh::myIndexCount` set in `BuildIndexBuffer`; draw prep/legacy record use GPU count.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0.
- **Deviations:** none

## Closeout — §C RenderDoc tags (2026-06-09)

- **Outcome:** Per-draw labels use stack `char[128]` + `snprintf`; gated on `AreCommandDebugLabelsEnabled()` (no per-draw `std::string`). Pass-level labels remain string literals.
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0.
- **Deviations:** none

## Closeout — §D demo sim defaults (2026-06-09)

- **Outcome:** `demoRotate` default `false` (config + `FeatureFlags`); `Gfx_TickDemoSceneTransforms` pass-through when off; `Gfx_ResetDemoSceneSimTime()` on scene reload.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0.
- **Deviations:** none

## Closeout — §E LOD default off (2026-06-09)

- **Outcome:** `lodEnabled` default `false`; draw stream skips `Gfx_ApplyLodToFrameExtract` unless enabled; ImGui **CPU LOD** session toggle + CLI `--lod-enabled` / `--no-lod-enabled`.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0.
- **Deviations:** none

## Closeout — §F AABB + depth bucket (2026-06-09)

- **Outcome:** Mesh `myLocalBounds` at load; SoA world AABB = local × transform; opaque `depthBucket` + transparent sort use bounds-center eye Z; GfxTests `TestTransparentSortStableUnderRotation`.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0; log: `Applied mesh local bounds to SoA`.
- **Deviations:** none
