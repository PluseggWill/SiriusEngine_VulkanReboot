# Progress: vulkan-rhi-p2 (P2 open tasks)

**Plan:** [`vulkan-rhi-hardening-epic_Plan.md`](../vulkan-rhi-hardening-epic_Plan.md) §B2–C  
**Closed:** 2026-06-09

## Closeout — 2026-06-09

- **Outcome:** RHI-B2–C3 landed — three-layer `Recreate`, `SURFACE_LOST` recovery, `Verify-ResizeSmoke.ps1`, `Vk_ResourceContext` single owner + batched scene upload, manifest-sized descriptor pool.
- **Verification:** MSBuild Debug|x64 exit 0; `Verify-Smoke.ps1` exit 0; `Verify-ResizeSmoke.ps1` exit 0 (log: `[SWAPCHAIN] rebuild layer=wsi|extent|pipeline`, `LoadSceneResources upload waitMs=`, `pool from manifest`).
- **Deviations:** Upload batch initially batched all transfer ops (texture layout wrong) and destroyed staging before submit (mesh wrong) — fixed: only `CopyBuffer` batched, textures load before batch, `DestroyStagingBuffer` defers VMA free. Review pass added batch/deletor comments and `popSecondFromBackAndRun`. `Verify-CI.ps1` reflection path drift pre-existing; `vkDeviceWaitIdle` kept in recreate (D3 → Wishlist S7).
