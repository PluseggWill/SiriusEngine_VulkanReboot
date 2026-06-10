# Progress: render-m2-p3-a

**Plan:** [`render-m2-p3-a_Plan.md`](render-m2-p3-a_Plan.md)

## Closeout — 2026-06-10

- **Outcome:** `Gfx_EntityGpuRecord` (80-byte std430) per SoA slot; per-frame `myEntityRecordBuffer` + `FillEntityRecords` in `PrepareFrameCpu`; inactive slots `layerMask==0`. CPU post-cull draw path unchanged.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0; log: `Entity-record buffer: slots=256`, `FillEntityRecords: wrote 1 slot(s)`; GfxTests `TestEntityGpuRecordSync`.
- **Deviations:** none
