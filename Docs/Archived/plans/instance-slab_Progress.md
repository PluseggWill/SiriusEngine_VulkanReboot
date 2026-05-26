# Progress: instance-slab

## 2026-05-26 — Plan

- **Plan ref:** P0 design doc + SprintPlan S1 data plane
- **Files:** `Docs/instance-slab_Plan.md`
- **What changed:** Scoped instance ring UBO; FillInstanceSlab; record reads offsets only; Set 2 deferred.
- **Verification:** N/A (plan only)

## 2026-05-26 — P1–P5 Implementation

- **Plan ref:** P1–P5
- **Files:** `Vk_DescriptorPolicy.h`, `Vk_Camera.h`, `Vk_FrameData.h`, `Vk_Core.h`, `Vk_Core.cpp`, `Gfx_DrawExtract.cpp`, `Docs/EngineArchitecture.md`, `Docs/SprintPlan.md`
- **What changed:** Per-frame CPU-mapped instance ring UBO (`kMaxInstanceSlabEntries=256`, stride 64). `CreateInstanceSlabs` at init. `FillInstanceSlab` after cull/sort assigns aligned offsets and writes `GpuObjectData.model`. `RecordScenePass` reads slab via `myInstanceDataOffset` (no SoA). Extract leaves offset at 0 until fill.
- **Verification:** MSBuild Debug|x64 exit 0. Smoke-run 4s (force-stopped, no crash). Log: `Instance slab: entries=256 stride=64`, `FillInstanceSlab: wrote 2 instance(s)`; no new `[ERROR]` during init.
