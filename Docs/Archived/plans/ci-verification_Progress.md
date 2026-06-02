# Progress: ci-verification

- **Plan:** [`ci-verification_Plan.md`](ci-verification_Plan.md)
- **Parent:** Active-Plan P0

## Closeout — 2026-06-02

- **Outcome:** G0 CI (`Verify-CI.ps1`: MSBuild + shader drift + `GfxTests`), G0-smoke (`Verify-Smoke.ps1` + `Assert-SmokeLog.ps1`), automation docs, `engine.benchmark.json` vsync off, `--perf-log` JSONL + `Perf-JsonlSummary.ps1`, GitHub Actions workflow.
- **Verification:** `Scripts/Verify-CI.ps1` exit **0**; `Scripts/Verify-Smoke.ps1` exit **0**; `GfxTests: all passed`.
- **Adversarial (#26):** Archived claim *「M1 batchRuns=9」* ([`forward-stage1.md`](forward-stage1.md) §1) — runtime sums opaque+transparent batch runs for **9 draws**; GfxTests CPU golden uses demo SoA and expects **8** batch runs because two trees share mesh+material batch key (`TestFixtures/Gfx/README.md`). Consistent.
- **Deviations:** `reflection_*.json` excluded from shader drift (machine-specific `spv` paths). JSONL `activeViews` field added in review (schema v1). Peel metrics (#27) deferred to P1 closeout per SprintOutcomeValidation.
