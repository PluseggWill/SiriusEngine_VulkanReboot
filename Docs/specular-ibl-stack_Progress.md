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
- **Files:** `Scripts/bake_default_ibl.py`, `Data/Environments/default/manifest.json`, `Data/Environments/default/prefilter/mip00..07/*`, `VulkanDesktop/Util/Util_Loader.*`, `VulkanDesktop/RenderCore/Vk_ResourceContext.*`, `VulkanDesktop/RenderCore/Vk_IblResources.cpp`, `VulkanDesktop/Shader/PbrIbl.glsl`, `VulkanDesktop/Shader/DeferredLighting.frag`, `VulkanDesktop/RenderContract/GpuLightingGlobals.h`, `VulkanDesktop/Util/Util_LightingPanel.cpp`, `VulkanDesktop/Shader_Generated/DeferredLightingFrag.spv`
- **What changed:** Offline GGX prefilter 8-mip chain (manifest v3); `LoadCubemapMipChainFromFaceDirectories`; Lagarde `Pbr_SpecularOcclusion` on deferred specular IBL (`shadowParams.y` toggle); ImGui checkbox.
- **Verification:** MSBuild Debug|x64 OK; GfxTests pending; shader drift = uncommitted `DeferredLightingFrag.spv` (expected pre-commit); G0-validation manual pending.
