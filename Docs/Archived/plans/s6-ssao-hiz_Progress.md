# Progress: S6 — SSAO + Hi-Z

## Closeout — 2026-06-15

- **Outcome:** Hi-Z min-depth pyramid (R32 mip chain, compute reduce), SSAO (16-tap view-space + separable blur), deferred AO composite on ambient/IBL only, debug views AO + Hi-Z mip.
- **Chain:** `GBuffer → ClusterBuild → [depth/color barriers] → DepthPyramid → SSAO → depth copy → Deferred → Transparent`.
- **Files:** `Vk_DepthPyramidPass`, `Vk_SsaoPass`, `Gfx_AoSettings`, shaders `DepthPyramid.comp` / `Ssao.comp` / `SsaoBlur.comp`, `DeferredLighting.frag` bindings 13–14, ImGui AO + debug panels.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` — build + GfxTests pass; shader SPIR-V regenerated (commit with code). Manual Sponza AO pending user.
- **Deviations:** Hi-Z mip0 is half-res (first reduce from full G-buffer depth); temporal AO deferred to backlog.
