# Progress: s5-ibl-shadows

**Plan:** [`s5-ibl-shadows_Plan.md`](s5-ibl-shadows_Plan.md)  
**Status:** In progress

---

## 2026-06-12 — Step 0

- **Files:** `Scripts/Generate-DefaultIblAssets.ps1`, `Data/Environments/default/**`, `Config/engine.stress.json` (lighting off)
- **Verification:** asset script exit 0; G0 N/A until loader lands (stress keeps IBL/shadow off)
- **Notes:** 32² irradiance, 128² prefilter/sky faces, 512² BRDF LUT placeholder

## 2026-06-12 — Step 2

- **Files:** `Vk_IblResources.*`, `Util_Loader` cubemap path, `Vk_ResourceContext` cubemap helpers, `Gfx_LightingGlobals.h`, `Gfx_LightingMath.h`, `Vk_Core` init hook
- **Verification:** MSBuild VulkanDesktop Debug|x64 exit 0
- **Notes:** IBL loads at `InitRenderDevice`; lighting globals UBO slab allocated (descriptor bind in Step 4)

*Append further checkpoints per Plan §16. Closeout ≤30 lines at task end.*
