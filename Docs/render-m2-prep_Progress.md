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

**Next:** §D `demoRotate: false` default + §E `lodEnabled: false`.
