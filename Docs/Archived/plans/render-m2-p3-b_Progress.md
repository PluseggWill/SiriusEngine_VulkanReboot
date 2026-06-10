# Progress: render-m2-p3-b

**Plan:** [`render-m2-p3-b_Plan.md`](render-m2-p3-b_Plan.md)

## Closeout — 2026-06-10

- **Outcome:** `EntityCull.comp` + `Vk_GpuCull` compute pipeline; per-slot `myGpuCullIndirectBuffer` (instanceCount=0 when culled); dispatch in `DrawFrameGpu` when `--gpu-cull`. CPU extract/cull/record unchanged by default.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0; manual `--gpu-cull` log: `GPU cull dispatch: views=1 slots=1`.
- **Deviations:** none
