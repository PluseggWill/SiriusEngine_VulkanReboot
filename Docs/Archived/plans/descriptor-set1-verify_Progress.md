# Progress: descriptor-set1-verify

## 2026-05-26 — P1–P6

- **Plan ref:** P1–P6
- **Files:** `Vk_Enum.h`, `TriangleFrag_Lit.frag`, `Vk_Core.h/cpp`, `Vk_ResourceTables.cpp`, `Gfx_ResourceManifest.cpp`, `Util_DemoAssets.h`, `Util_FrameStats.*`, `Util_StatsOverlay.cpp`, `Vk_Initializer.cpp`, docs
- **What changed:** Set 1 `COMBINED_IMAGE_SAMPLER` per material; Set 0 frame UBOs only. `CreateMaterialDescriptorSets` after texture load; bind Set 1 once per batch. Demo: material 0 viking texture, material 1 RedMoon; entities mesh0/mat0, mesh1/mat1.
- **Verification:** MSBuild Debug|x64 exit 0. Smoke 4s no crash. Log: `Material sets allocated: 2`, `BATCH runs=2`, textures loaded for id 0/1.
