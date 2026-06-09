# Progress: render-m2-prep — §A CPU indirect

**Plan:** [`render-m2-prep_Plan.md`](render-m2-prep_Plan.md) §A only  
**Scope:** Draw template SSBO + CPU `DrawIndexedIndirect` (#13)

## Closeout — 2026-06-09

- **Outcome:** `Gfx_DrawTemplate` / `Gfx_DrawIndirectCommand` (Vulkan-compatible 20 B); `FillDrawTemplates` writes parallel indirect + SSBO slots per view partition; `Vk_Core::CreateDrawTemplateBuffers`; record path uses `vkCmdDrawIndexedIndirect` (default); `--legacy-direct-draw` restores `vkCmdDrawIndexed`.
- **Verification:** MSBuild Debug|x64 exit 0; GfxTests exit 0; `Verify-Smoke.ps1` exit 0 — log: `[RESOURCE] FillDrawTemplates: wrote 1 draw template(s)`, `[RENDER] Scene record using CPU vkCmdDrawIndexedIndirect`, `[PERF] drawCalls=1`.
- **Deviations:** `Verify-CI.ps1` shader-drift step fails on pre-existing absolute `spv` paths in reflection JSON (unrelated); build + GfxTests + smoke green.

**Next (same plan):** §B `myIndexCount`, §C RenderDoc tags, §D–F defaults/AABB.
