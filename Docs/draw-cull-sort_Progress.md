# draw-cull-sort — Progress

## 2026-05-26 — Plan created

- **Verification:** N/A

## 2026-05-26 — Cull + sort implementation

- **Plan ref:** Steps 1–5.
- **Files:** `VulkanDesktop/Gfx/Gfx_DrawCullSort.h`, `VulkanDesktop/Gfx/Gfx_DrawCullSort.cpp`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, vcxproj/filters, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:** Frustum AABB cull + layer mask; opaque sort on `mySortKey`; frame order extract→cull→sort→record.
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run OK; `[CULL] in=2 out=2 (frustum+layer)`.
