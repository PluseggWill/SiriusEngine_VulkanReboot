# Progress: m1-acceptance

## 2026-05-26 — Plan

- **Plan ref:** Goals
- **Files:** `Docs/m1-acceptance_Plan.md`, `_Progress.md`
- **Verification:** N/A

## 2026-05-26 — P1–P3 implementation

- **Plan ref:** P1–P3
- **Files:** `Util_FrameStats.*`, `Util_StatsOverlay.cpp`, `Vk_Core.*`, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`, `Docs/README.md`
- **What changed:** Draw-stream metrics on frame stats; Performance overlay shows entities, batch runs vs material binds; one-shot `[PERF]` log after frame 60; M1 acceptance archived.
- **Verification:** MSBuild Debug|x64 exit 0; smoke 5s; log `[PERF] frameMs=… batchRuns=9 materialSetBinds=9 drawCalls=9` (batch path).
