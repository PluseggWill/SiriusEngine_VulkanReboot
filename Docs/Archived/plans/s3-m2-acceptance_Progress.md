# Progress: s3-m2-acceptance

**Plan:** [`s3-m2-acceptance_Plan.md`](s3-m2-acceptance_Plan.md)  
**Branch:** `S3`

## Closeout — 2026-06-11

- **Outcome:** `Verify-Smoke.ps1` runs two passes (CPU indirect + `--gpu-cull` on stress scene); `Assert-SmokeLog.ps1 -Profile GpuCull` asserts M2 log tokens; §S3 evidence in `SprintOutcomeValidation.md`; `CLI.md` documents gpu-cull smoke.
- **Verification:** `Verify-CI.ps1` exit 0 (GfxTests G1 parity); `Verify-Smoke.ps1` exit 0 (~23s both passes); GpuCull tokens present in pass-2 log.
- **Audit:** `Vk_ScenePasses.cpp` — `gpuCullRecord` uses `vkCmdDrawIndexedIndirect` only; `vkCmdDrawIndexed` gated on `--legacy-direct-draw`.
- **Deviations:** none. CI workflow unchanged (already calls `Verify-Smoke.ps1`, which now includes gpu-cull pass).
