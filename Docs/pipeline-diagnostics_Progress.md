# Progress: pipeline-diagnostics

## 2026-05-22 — Plan

- **Plan ref:** Design + steps
- **Files:** `Docs/pipeline-diagnostics_Plan.md`
- **What changed:** Task plan for S0 pipeline creation diagnostics.
- **Verification:** N/A (plan only)

## 2026-05-22 — Implement pipeline diagnostics

- **Plan ref:** Steps — Util_VulkanResult, Vk_PipelineDiagnostics, wire create paths
- **Files:** `Util_VulkanResult.{h,cpp}`, `Vk_PipelineDiagnostics.{h,cpp}`, `Vk_Pipeline.{h,cpp}`, `Vk_Core.cpp`, `VulkanDesktop.vcxproj`, `.filters`, `Docs/SprintPlan.md`
- **What changed:** `UtilVulkanResult::Describe` / `ThrowOnFailure` for symbolic VkResult in logs and exceptions. `LogGraphicsPipelineSummary` logs shader paths, stages, layout counts, render pass, extent, formats, MSAA, topology, depth/blend, vertex layout before `vkCreateGraphicsPipelines`; on failure re-logs and throws. Pipeline layout and shader module creation use same helper.
- **Verification:** MSBuild Debug|x64 exit 0. Smoke-run: `[PIPELINE] --- basic-lit pipeline summary ---` through `basic-lit graphics pipeline created (VkResult=VK_SUCCESS)` in `Logs/engine_runtime_log.txt` (2026-05-22 21:14:34).
