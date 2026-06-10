# Progress: render-m2-p3-c

**Plan:** [`render-m2-p3-c_Plan.md`](render-m2-p3-c_Plan.md)

## Closeout — 2026-06-10

- **Outcome:** `--gpu-cull` record path uses `myGpuCullIndirectBuffer` with `Gfx_ComputeEntityIndirectSlot(viewBase, entitySlot)`; CPU frustum cull skipped when gpu cull on (`instanceCount=0` from compute). Default path unchanged.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0; manual `--gpu-cull` logs: `CULL … (gpu-deferred)`, `GPU cull dispatch`, `Scene record using GPU-filled slot indirect`.
- **Deviations:** none
