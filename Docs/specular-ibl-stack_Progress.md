# Progress: specular-ibl-stack

**Plan:** [`specular-ibl-stack_Plan.md`](specular-ibl-stack_Plan.md)  
**Kickoff:** 2026-06-30

## A0 — Kickoff

- **Plan ref:** Phase A0
- **Files:** `Docs/specular-ibl-stack_Progress.md`, `Docs/README.md`, `Docs/specular-ibl-stack_Plan.md` (status)
- **What changed:** WIP kickoff; baseline = `d454d15` specular bypasses AO + prefilter mip0 only.
- **Verification:** N/A (docs only)

## 2026-06-30 — A1–A4 Phase A implementation

- **Plan ref:** A1 bake, A2 loader, A3 shaders, A4 UI
- **Files:** `Scripts/bake_default_ibl.py`, `Data/Environments/default/manifest.json`, `prefilter/mip00..07/*`, `Util_Loader.*`, `Vk_ResourceContext.*`, `Vk_IblResources.cpp`, `PbrIbl.glsl`, `DeferredLighting.frag`, `GpuLightingGlobals.h`, `Util_LightingPanel.cpp`, `Util_TuningPrefs.cpp`, `DeferredLightingFrag.spv`
- **What changed:** Offline GGX prefilter 8-mip chain (manifest v3); `LoadCubemapMipChainFromFaceDirectories`; Lagarde `Pbr_SpecularOcclusion` on deferred specular IBL (`shadowParams.y` toggle); ImGui checkbox + tuning prefs.
- **Verification:** MSBuild Debug|x64 OK; GfxTests OK; Verify-CI OK; G0-validation Sponza stress `--validation` exit 0.

## 2026-06-30 — A5 closeout + commit

- **Commit:** `3a044a9` — `[Lighting] Add GGX prefilter mips and Lagarde specular occlusion (Phase A).`
- **Review pass:** `GpuLightingGlobals` packing comment; `PrefilterMipSubdir()` helper; manifest layout reject; tuning prefs for specular occlusion.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0 post-commit.

## 2026-06-30 — B1–B3 Phase B

- **Plan ref:** B1 SSR scaffold, B2 Hi-Z trace v0, B3 deferred composite
- **Commit:** `87be12f` — `[Lighting] Add Hi-Z SSR pass and deferred prefilter blend (Phase B).`
- **Files:** `SsrCommon.glsl`, `SsrTrace.comp`, `Vk_SsrPass.*`, `Vk_FrameGraph.*`, `DeferredLighting.frag` binding 17, `GpuLightingGlobals.h` (`shadowParams.x`), `Util_LightingPanel.cpp`, `Util_TuningPrefs.cpp`, `SsrTrace.spv`, `DeferredLightingFrag.spv`
- **What changed:** Hi-Z SSR compute after DepthPyramid; `mix(prefilter, ssr, confidence) * specularOcc`; ImGui SSR toggles; default off.
- **Note:** v0 hit radiance = G-buffer albedo at hit UV (lit HDR / temporal reprojection follow-up).
- **Verification:** Verify-CI exit 0; G0-validation exit 0; FG: `DepthPyramid -> SSR -> AO -> ...`.
