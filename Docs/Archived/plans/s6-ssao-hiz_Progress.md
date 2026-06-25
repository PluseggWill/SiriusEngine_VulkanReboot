# Progress: S6 — SSAO + Hi-Z

## Closeout — 2026-06-15

- **Outcome:** Hi-Z min-depth pyramid (R32 mip chain, compute reduce), SSAO (16-tap view-space + separable blur), deferred AO composite on ambient/IBL only, debug views AO + Hi-Z mip.
- **Chain:** `GBuffer → ClusterBuild → [depth/color barriers] → DepthPyramid → SSAO → depth copy → Deferred → Transparent`.
- **Files:** `Vk_DepthPyramidPass`, `Vk_SsaoPass`, `Gfx_AoSettings`, shaders `DepthPyramid.comp` / `Ssao.comp` / `SsaoBlur.comp`, `DeferredLighting.frag` bindings 13–14, ImGui AO + debug panels.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` — build + GfxTests pass; shader SPIR-V regenerated (commit with code). 2026-06-16 manual smoke: `x64/Debug/VulkanDesktop.exe --asset-root <repo> --config Config/engine.stress.json --validation --smoke-frames 120 --smoke-seconds 6` exit 0 (local machine lacks `VK_LAYER_KHRONOS_validation`, runtime auto-fell back to validation off); AO/FG chain logs present.
- **Deviations:** Hi-Z mip0 is half-res (first reduce from full G-buffer depth); temporal AO deferred to backlog.
