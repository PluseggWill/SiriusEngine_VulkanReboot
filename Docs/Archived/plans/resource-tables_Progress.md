# resource-tables — Progress

## 2026-05-26 — Plan created

- **Plan ref:** SprintPlan S1 resource tables (demo manifest v0).
- **Verification:** N/A

## 2026-05-26 — Manifest + Vk_ResourceTables + Vk_Core wiring

- **Plan ref:** Steps 1–6.
- **Files:** `VulkanDesktop/Gfx/Gfx_ResourceManifest.h/.cpp`, `VulkanDesktop/RenderCore/Vk_ResourceTables.h/.cpp`, `VulkanDesktop/RenderCore/Vk_Core.h/.cpp`, `VulkanDesktop/RenderCore/Vk_Types.h`, vcxproj/filters, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:** Demo CPU manifest loads into dense GPU tables; removed `myMeshMap`/`myMaterialMap`/`myTextureMap`; descriptors resolve texture via material→texture id; record loops extract draws and resolves mesh/material from tables.
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run 4s OK; log `[RESOURCE-TABLE] meshes=2 materials=1 textures=1`, `[EXTRACT] entities=2 visible=2 draws=2`, mesh id=0/1 load lines.

## 2026-05-26 — Per-draw model (push constant) + demo layout

- **Plan ref:** Post-review alignment with `EngineArchitecture.md` §5.3 (not a plan deviation; demo observability).
- **Files:** `VulkanDesktop/Shader/TriangleVertex.vert`, `VulkanDesktop/RenderCore/Vk_Camera.h`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, `VulkanDesktop/RenderCore/Vk_Enum.h`, `Docs/EngineArchitecture.md`
- **What changed:** Removed `model` from frame UBO; pipeline layout push constant 64 B; `RecordScenePass` uses `vkCmdPushConstants` with SoA world × demo spin. Demo entities at x = ±4; camera pulled back. UBO per-draw patch was insufficient (overlapping meshes).
- **Verification:** Rebuild + smoke-run OK; two meshes visually separated.
- **Follow-up (S1):** Use `DrawInstance.myInstanceDataOffset` + instance slab; Set 1 per `myMaterialId`; sort/batch to reduce `vkCmdBindDescriptorSets` per draw.
