# Progress: draw-batch

## 2026-05-26 — Plan

- **Plan ref:** P0
- **Files:** `Docs/draw-batch_Plan.md`
- **Verification:** N/A

## 2026-05-26 — P1–P5 Implementation

- **Plan ref:** P1–P5
- **Files:** `Gfx_DrawBatch.h`, `Gfx_DrawBatch.cpp`, `Vk_Core.h`, `Vk_Core.cpp`, `VulkanDesktop.vcxproj`, `.filters`, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`, `Docs/README.md`
- **What changed:** `Gfx_BuildOpaqueDrawBatches` groups sorted draws by batch key (sort key without depth bucket). `RecordScenePass` binds set 0 once per pass; per batch binds pipeline once; per draw VB/IB + set 2 dynamicOffset + draw.
- **Verification:** MSBuild Debug|x64 exit 0. Smoke-run 4s no crash. Log: `BATCH runs=2 draws=2` (demo viking + monkey, same material).
