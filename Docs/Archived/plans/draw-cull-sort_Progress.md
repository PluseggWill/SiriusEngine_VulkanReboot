# draw-cull-sort — Progress

## 2026-05-26 — Plan created

- **Verification:** N/A

## 2026-05-26 — Cull + sort implementation

- **Plan ref:** Steps 1–5.
- **Files:** `VulkanDesktop/Gfx/Gfx_DrawCullSort.h`, `VulkanDesktop/Gfx/Gfx_DrawCullSort.cpp`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, vcxproj/filters, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:** Frustum AABB cull + layer mask; opaque sort on `mySortKey`; frame order extract→cull→sort→record.
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run OK; `[CULL] in=2 out=2 (frustum+layer)`.

## 2026-05-26 — Parallel-array sort + unified view params

- **Plan ref:** Hygiene follow-up (review items).
- **Files:** `VulkanDesktop/Gfx/Gfx_DrawCullSort.h`, `VulkanDesktop/Gfx/Gfx_DrawCullSort.cpp`, `VulkanDesktop/Gfx/Gfx_DrawExtract.h`, `VulkanDesktop/Gfx/Gfx_DrawExtract.cpp`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, `Docs/draw-cull-sort_Plan.md`
- **What changed:** `Gfx_SortOpaqueDrawInstances(Gfx_ExtractResult&)` keeps `myVisibleEntityIndices[i]` paired with `myDrawInstances[i]`; removed `Gfx_ExtractViewParams`; single `Gfx_CullViewParams` in frame loop; removed unused include.
- **Verification:** MSBuild Debug|x64 exit 0.
