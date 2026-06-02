# Progress: vk-core-decomposition

**Plan:** [`vk-core-decomposition_Plan.md`](vk-core-decomposition_Plan.md)  
**Archived:** 2026-05-29 (docs hygiene batch)

## Closeout — 2026-05-28

- **Outcome:** M1 `Vk_ResourceContext` + tables load peel; M2 `Gfx_FrameDrawStream` / `Vk_FrameDrawPrep` / `Gfx_DemoSceneSim`; M3 `DrawFrame` structure; phase-2 modules (`Vk_RenderDevice`, `Vk_SwapchainHost`, `Vk_DescriptorSystem`, `Vk_GfxPipelineCache`, `Vk_ScenePasses`, `Vk_FrameUniformUploader`, `Vk_SceneHost`, `Vk_PlatformFrame`) delegated from `Vk_Core` with acquire/present on swapchain host.
- **Verification:** MSBuild Debug|x64 exit 0 after each milestone; smoke `[EXTRACT] entities=9 draws=9`, `FillInstanceSlab: wrote 9`; delegated acquire/present diagnostics in log.
- **Deviations:** none on scope. Post-close readability passes (comments only) did not re-run full smoke per batch.
