# Progress: pipeline-cache-disk

## Closeout — 2026-06-01

- **Outcome:** Added `Vk_DevicePipelineCache` with versioned `Cache/pipeline_cache_v1.bin` (GPU UUID, driver ids, active permutation SPIR-V mtimes). `Vk_PipelineBuilder` and scene pipelines use `myPipelineCache`; save on shutdown.
- **Verification:** `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0; run1 `[PIPELINE-CACHE] disk miss` + `saved … blobBytes=29557`; run2 `[PIPELINE-CACHE] disk hit … bytes=29557` + `loaded VkPipelineCache`.
- **Deviations:** none
- **Plan:** [`pipeline-cache-disk_Plan.md`](pipeline-cache-disk_Plan.md)
