# Progress: architecture-layer-hygiene

## Closeout — 2026-06-13

- **Outcome:** P0–P3 complete — removed RenderCore→App includes; split CPU/GPU resource types; Gfx owns view packet build; App owns scene CPU load; EngineArchitecture glossary updated.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; `powershell -File Scripts/Verify-Smoke.ps1` exit 0.
- **Deviations:** Kept `Gfx_Mesh`/`Gfx_Material`/`Gfx_Texture` typedef aliases on `Vk_*Resource` for minimal record-path churn.
