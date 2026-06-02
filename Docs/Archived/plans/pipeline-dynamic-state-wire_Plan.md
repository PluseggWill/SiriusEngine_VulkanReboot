# Plan: pipeline-dynamic-state-wire

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Closed (2026-06-01)  
**Roadmap:** moved to [`Archived-Plan.md`](../../Archived-Plan.md) `[S2]`  
**Prior art:** S0 [`Archived/plans/pipeline-dynamic-state_Plan.md`](pipeline-dynamic-state_Plan.md)

## Problem

Scene graphics pipelines declared dynamic viewport/line width with static scissor and per-batch cmd sets; legacy `Pipeline_DynamicStateCreateInfo()` remained unused.

## Goals (completed)

1. Default dynamic states: VIEWPORT + SCISSOR + LINE_WIDTH.
2. `SetGraphicsDynamicState` sets viewport, scissor, line width.
3. Once per scene render pass in `RecordScene`.
4. Remove `Pipeline_DynamicStateCreateInfo()`.
5. Diagnostics: `dynamicState count=3 wired`.
6. `EngineArchitecture.md` §5.2 policy bullet.

## Implementation plan

- [x] **1. Defaults** — `Vk_Pipeline.cpp`
- [x] **2. Command recording** — `Vk_Core.cpp`
- [x] **3. Pass-level call site** — `Vk_ScenePasses.cpp`
- [x] **4. Remove legacy API** — `Vk_Initializer.{h,cpp}`
- [x] **5. Diagnostics** — count=3 in pipeline summary
- [x] **6. Architecture note** — `EngineArchitecture.md` §5.2
- [x] **7. Verification** — Debug build + smoke exit 0

## Verification (recorded)

- MSBuild `Debug|x64` exit 0.
- `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0.
- Log: `dynamicState count=3 wired` on pipeline create.
