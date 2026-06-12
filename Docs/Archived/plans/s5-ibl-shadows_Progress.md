# Progress: s5-ibl-shadows

**Plan:** [`Archived/plans/s5-ibl-shadows_Plan.md`](Archived/plans/s5-ibl-shadows_Plan.md)  
**Status:** Closed (2026-06-12)

---

## 2026-06-12 — Step 0

- **Files:** `Scripts/Generate-DefaultIblAssets.ps1`, `Data/Environments/default/**`, `Config/engine.stress.json` (lighting off)
- **Verification:** asset script exit 0
- **Notes:** 32² irradiance, 128² prefilter/sky faces, 512² BRDF LUT placeholder

## 2026-06-12 — Steps 1–3

- **Files:** `PbrIbl.glsl`, `Vk_IblResources.*`, `Vk_ShadowMapPass.*`, shadow/IBL shaders + `.spv`
- **Verification:** MSBuild Debug|x64; shadow pass before GBuffer in `Vk_GBufferPass`

## 2026-06-12 — Steps 4–6

- **Files:** `LightingBindings.glsl`, lit/deferred frags, `Vk_DescriptorSystem`, `Vk_DeferredLightingPass`, `Vk_FrameUniformUploader`
- **Verification:** reflection_lit* 11 Set-0 bindings; MSBuild exit 0

## 2026-06-12 — Step 7

- **Files:** `Util_EngineConfig`, `Util_LightingPanel`, `Config/engine.json` / benchmark
- **Verification:** CONFIG + ImGui `[LIGHTING]` log; stress config shadows/IBL off

## Closeout 2026-06-12

- **Outcome:** Split-sum IBL + sky at depth ≥ 0.999 + 2048² directional shadow (PCF); toggles via `GpuLightingGlobals` (no new permutations). Chain: ShadowMapDirectional → GBufferOpaque → ClusterBuild → DeferredLighting → ForwardTransparent.
- **Verification:** `Verify-Smoke.ps1` exit 0 (stress); MSBuild solution Debug|x64; forward batch + bindless lit SPIR-V rebuilt.
- **Deviations:** Optional `Fetch-S5Environment.ps1` not added; placeholder cubemaps from `Generate-DefaultIblAssets.ps1`.
