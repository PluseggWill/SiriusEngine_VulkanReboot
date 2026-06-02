# Plan: pipeline-dynamic-state

**Sprint:** S0 — Foundation & tooling  
**Status:** Done (2026-05-22)

## Problem

`VkInit::Pipeline_DynamicStateCreateInfo()` returns `VkPipelineDynamicStateCreateInfo` by value with `pDynamicStates` pointing at storage inside the function. That pattern is fragile (dangling if the array is ever non-static) and the builder ignored dynamic state (`pDynamicState = nullptr`).

## Goals

1. Store dynamic state enums in **caller-owned** storage (`Vk_PipelineBuilder::myDynamicStatesStorage`).
2. `pDynamicStates` remains valid through `vkCreateGraphicsPipelines`.
3. Wire `pipelineInfo.pDynamicState` when count > 0.
4. Replace return-by-value helper with `Pipeline_FillDynamicStateCreateInfo(storage, out)`; defaults via `SetDefaultDynamicStates`.

## Non-goals

- `vkCmdSetViewport` / dynamic viewport usage in the render loop (S2 / pipeline builder hygiene).

## Files

- `VulkanDesktop/RenderCore/Vk_Initializer.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_Pipeline.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `VulkanDesktop/RenderCore/Vk_PipelineDiagnostics.cpp`
- `Docs/pipeline-dynamic-state_Progress.md`, `Docs/SprintPlan.md`

## Steps

- [x] Plan
- [x] Vk_PipelineBuilder storage + SetDynamicStates; wire BuildPipeline
- [x] VkInit fill API; update CreateGfxPipeline
- [x] Build Debug\|x64 + smoke-run
- [x] Archive S0 SprintPlan line

## Verification

1. MSBuild Debug\|x64 exit 0.
2. Log: `dynamicState count=2 wired` in pipeline summary.
3. Pipeline create succeeds (unchanged visuals).
