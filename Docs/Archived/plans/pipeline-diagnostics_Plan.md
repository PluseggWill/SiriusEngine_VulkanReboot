# Plan: pipeline-diagnostics

**Sprint:** S0 — Foundation & tooling  
**Status:** Done (2026-05-22)

## Problem

`vkCreateGraphicsPipelines` and `vkCreatePipelineLayout` failures only surface as opaque `runtime_error` with a numeric `VkResult`, often after partial Vulkan setup. There is no consolidated log of shader stages, layout sets, render-pass subpass, or fixed-function state at creation time.

## Goals

1. Map `VkResult` to symbolic name + numeric code for logs and exceptions.
2. Before/after graphics pipeline creation, log a **summary**: shader stages (stage + entry), layout set/push counts, render pass subpass, extent, MSAA, topology, depth/blend flags, vertex binding layout.
3. On failure, re-log summary then throw with `UtilVulkanResult::Describe`.
4. Apply to demo path: `vkCreatePipelineLayout`, `vkCreateShaderModule`, `Vk_PipelineBuilder::BuildPipeline` (used by `CreateGfxPipeline`).

## Non-goals

- Validation layer messenger / `VK_EXT_debug_utils` (separate S0 task).
- Pipeline cache or derivative pipelines.
- ImGui pipeline (optional reuse of `ToString` only).

## Design

| Item | Decision |
|------|----------|
| VkResult helper | `UtilVulkanResult` in `Util/Util_VulkanResult.{h,cpp}` |
| Pipeline summary | `Vk_PipelineDiagnostics` + `Vk_GraphicsPipelineBuildInfo` in `RenderCore/` |
| Log category | `[PIPELINE]` summary; `[SHADER]` on module failure |
| Build API | `BuildPipeline(device, pass, &buildInfo)` — nullable info skips extra lines |

## Files

- `VulkanDesktop/Util/Util_VulkanResult.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_PipelineDiagnostics.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_Pipeline.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `VulkanDesktop/VulkanDesktop.vcxproj`, `.filters`
- `Docs/pipeline-diagnostics_Progress.md`, `Docs/SprintPlan.md`

## Steps

- [x] Plan
- [x] `Util_VulkanResult` + `Vk_PipelineDiagnostics`
- [x] Wire `Vk_PipelineBuilder`, `CreateGfxPipeline`, `CreateShaderModule`
- [x] Build Debug\|x64 + smoke-run; confirm `[PIPELINE]` summary in log
- [x] Archive S0 line in `SprintPlan.md`

## Verification

1. MSBuild Debug\|x64 exit 0.
2. Run app: log contains `pipeline summary` with vert/frag `main`, `layout setCount=1`, extent, `VK_SAMPLE_COUNT_1_BIT`.
3. (Optional) corrupt SPIR-V or mismatch entry point → `VkResult` name in error before exit.

## Risks

- Log verbosity on every swapchain rebuild — acceptable for S0; gate behind flag later if noisy.
