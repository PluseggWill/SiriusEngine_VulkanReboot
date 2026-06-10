# Progress: render-m2-p3-g1

**Plan:** [`render-m2-p3-g1_Plan.md`](render-m2-p3-g1_Plan.md)

## Closeout — 2026-06-10

- **Outcome:** `Gfx_GpuCull.cpp` CPU reference mirrors `EntityCull.comp`; GfxTests compare visible entity slots vs CPU draw-stream cull (overview, tight FOV, layer mask). G1 satisfied in `Verify-CI.ps1`.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0; GfxTests `TestCpuGpuCullParity*`.
- **Deviations:** parity is CPU-vs-CPU-reference (no GPU buffer readback) — matches plan scope.
