# Progress: transparency

## 2026-05-26 — Plan

- **Plan ref:** P0
- **Files:** `Docs/transparency_Plan.md`
- **Verification:** N/A

## 2026-05-26 — P1–P6

- **Plan ref:** P1–P6
- **Files:** `Gfx_SceneSoA.*`, `Gfx_DrawExtract.*`, `Gfx_DrawCullSort.*`, `Gfx_ResourceManifest.*`, `Vk_Core.*`, `Vk_ResourceTables.*`, `Vk_Types.h`, `Vk_Enum.h`, `Vk_Initializer.*`, `TriangleFrag_Lit.frag`, docs
- **What changed:** Dual extract lists; transparent sort by eye Z; `myTransparentPipeline` with alpha blend; Set 1 `GpuMaterialParams.alpha`; center transparent monkey (material 2).
- **Verification:** MSBuild Debug|x64 OK. Smoke 4s. Log: `TRANSP runs=1 draws=1`, `opaque=2 transparent=1`, transparent pipeline `depthWrite=0 colorBlendEnable=1`.
