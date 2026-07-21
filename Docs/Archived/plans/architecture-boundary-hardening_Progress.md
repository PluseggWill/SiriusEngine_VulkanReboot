# Progress: architecture-boundary-hardening

**Plan:** [`architecture-boundary-hardening_Plan.md`](architecture-boundary-hardening_Plan.md) (this folder)

## Closeout — 2026-07-21

- **Outcome:** ABH-1..7 already landed (2026-07-15 commits); closeout fixes DDGI atlas VMA leak on shutdown, restores GpuCull smoke `(gpu-deferred)` Info token, locks RenderContract/Platform boundaries in Architecture + layout rules, archives this plan.
- **Verification:**
  - `powershell -File Scripts/Verify-CI.ps1` → exit 0 (`Verify-CI: PASSED`)
  - `powershell -File Scripts/Verify-Smoke.ps1` → exit 0 (`Smoke log OK (Default)` + `Smoke log OK (GpuCull)` + `Verify-Smoke: PASSED`)
  - Log: `[APP] Engine exited run loop normally`; `[CORE] Resource cleanup completed` (no VMA assert)
- **Deviations:** Pre-existing shutdown VMA leak (DDGI irradiance atlas never destroyed) and Debug-level `(gpu-deferred)` vs stress `logLevel=info` fixed during closeout verification — outside original ABH touch list but required for V-4/V matrix.
