# Plan: m1-acceptance

**Sprint:** S1 — Milestone M1 (final acceptance line)  
**Status:** Done (2026-05-26)  
**SprintPlan:** Multi-mesh scene; draws scale with batches (not per-object Set 1 binds); frame time logged.

## Problem

S1 feature tasks are done; one **M1 acceptance** line remains: prove the demo multi-mesh path batches descriptor binds and exposes frame time for sign-off.

## Goals

1. **Scene metrics** on `Util_FrameStats`: active entities, visible opaque/transparent draws, opaque/transparent batch run counts (post-cull).
2. **ImGui** Performance overlay: show batch runs vs material set binds vs draw calls (batch path: material binds ≤ batch runs).
3. **Runtime log** once after warmup: `[PERF]` line with frame ms, FPS, draws, batch runs, material/pipeline binds (evidence for log file).
4. Archive M1 acceptance in `SprintPlan.md` + sync `EngineArchitecture.md`.

## Non-goals

- GPU timestamps / p95 benchmark runbook (S7).
- Changing batch algorithm or adding more demo entities.
- CSV export.

## Files

| File | Change |
|------|--------|
| `VulkanDesktop/Util/Util_FrameStats.h` / `.cpp` | draw-stream metric fields + setter |
| `VulkanDesktop/Util/Util_StatsOverlay.cpp` | display batch vs bind counts |
| `VulkanDesktop/RenderCore/Vk_Core.cpp` | set metrics after batch build; one-shot PERF log |
| `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`, `Docs/README.md` | M1 archive + arch note |

## Steps

- [x] P1 — `Util_FrameStats` scene/batch metrics + overlay
- [x] P2 — `DrawFrame` populate metrics; `[PERF]` log at frame 60
- [x] P3 — Build + smoke-run; archive sprint line; progress log

## Build / smoke-run

MSBuild Debug|x64; smoke ≥4s (past frame 60 if possible use 5s); log contains `[PERF]` with `batchRuns` and `frameMs`.

## Acceptance

- Demo: ≥2 meshes, ≥2 materials, ≥8 opaque draws → opaque batch runs < opaque draws OR material binds ≤ total batch runs (batch path).
- Frame ms visible in overlay and one `[PERF]` log line after warmup.
