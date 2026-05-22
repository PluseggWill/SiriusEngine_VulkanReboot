# Progress: pipeline-dynamic-state

## 2026-05-22 — Plan

- **Plan ref:** Design + steps
- **Files:** `Docs/pipeline-dynamic-state_Plan.md`
- **Verification:** N/A

## 2026-05-22 — Fix dynamic state storage

- **Plan ref:** Implementation steps
- **Files:** `Vk_Pipeline.{h,cpp}`, `Vk_Initializer.{h,cpp}`, `Vk_Core.cpp`, `Vk_PipelineDiagnostics.cpp`, `Docs/SprintPlan.md`
- **What changed:** `Vk_PipelineBuilder::SetDynamicStates` keeps `myDynamicStatesStorage`; `pDynamicStates` points at stable vector data through `vkCreateGraphicsPipelines`. Replaced return-by-value `Pipeline_DynamicStateCreateInfo` with `Pipeline_FillDynamicStateCreateInfo(storage, out)` (reused by builder). Defaults via `SetDefaultDynamicStates`. Pipeline summary logs `dynamicState count=2 wired`.
- **Review (pre-commit):** Kept `Pipeline_FillDynamicStateCreateInfo` for reuse; `SetDynamicStates` delegates to it; centralized defaults in `Vk_Pipeline.cpp`.
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run OK; log line `dynamicState count=2 wired`.
