# Progress: forward-stage1-validation

## Closeout — 2026-06-02

- **Outcome:** Stage 1 epic §C + Acceptance closed — [`forward-stage1.md`](../forward-stage1.md), golden PNG, `engine.benchmark.json`; epic Status Stage 1 complete.
- **Verification:** `.\VulkanDesktop.exe --asset-root <repo> --config <repo>\Config\engine.benchmark.json --no-validation --smoke-seconds 6` exit **0**; `[PERF] frameMs=16.57 fps=60.35 visibleDraws=9 batchRuns=9`.
- **Deviations:** golden captured via screen CopyFromScreen (0,0)–1600×1200; not pixel CI.
- **Plan:** [`forward-stage1-validation_Plan.md`](forward-stage1-validation_Plan.md)
