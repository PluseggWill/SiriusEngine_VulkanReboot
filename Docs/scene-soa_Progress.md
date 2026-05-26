# scene-soa — Progress

## 2026-05-25 — Plan created

- **Plan ref:** SprintPlan S1 SoA columns + stable entity id.
- **Verification:** N/A

## 2026-05-25 — Gfx_SceneSoA + extract migration

- **Plan ref:** Steps 1–5.
- **Files:** `VulkanDesktop/Gfx/Gfx_SceneSoA.h`, `VulkanDesktop/Gfx/Gfx_SceneSoA.cpp`, `VulkanDesktop/Gfx/Gfx_DrawExtract.h`, `VulkanDesktop/Gfx/Gfx_DrawExtract.cpp`, `VulkanDesktop/RenderCore/Vk_Core.h`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, vcxproj/filters, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:** Columnar `Gfx_SceneSoA` with slot/generation ids; extract reads active slots; demo uses `AllocEntity`; removed `Gfx_SceneEntity` vector. Archived SoA, stable id, and DrawObjects sprint lines.
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run OK; `[SCENE] Demo scene SoA active entities: 2`, `[EXTRACT] entities=2 visible=2 draws=2`.
