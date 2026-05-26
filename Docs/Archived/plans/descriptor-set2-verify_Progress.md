# Progress: descriptor-set2-verify

## 2026-05-26 — Plan

- **Plan ref:** P0
- **Files:** `Docs/descriptor-set2-verify_Plan.md`
- **What changed:** Scoped Set 2 UNIFORM_BUFFER_DYNAMIC verification.
- **Verification:** N/A

## 2026-05-26 — P1–P6 Implementation

- **Plan ref:** P1–P6
- **Files:** `TriangleVertex.vert`, `Vk_Enum.h`, `Vk_DescriptorPolicy.h`, `Vk_Core.h`, `Vk_Core.cpp`, `Vk_Camera.h`, `Vk_FrameData.h`, `Docs/EngineArchitecture.md`, `Docs/SprintPlan.md`, `Docs/README.md`
- **What changed:** Set 2 layout (UNIFORM_BUFFER_DYNAMIC), empty set 1 placeholder, pipeline layout 3 sets (no model push). Object descriptor per frame → instance slab. Record binds set 0 + set 2 with per-draw `dynamicOffset`. Vertex shader reads `objectData.model` from set 2.
- **Verification:** Rebuild Debug|x64 exit 0; SPIR-V rebuilt. Init order fix: `CreateDescriptorSets` after `LoadFromManifest`. Smoke-run 4s no crash. Log: `Set 2 dynamicOffsets (sample): 0, 64`; `pipeline layout: setCount=3`.
