# Progress: lighting-plus

## 2026-05-19 16:56 — Blinn-Phong + texture albedo + ImGui lighting panel

- **Plan ref:** Steps 2–5
- **Files:** `Vk_Types.h`, `Vk_Enum.h`, `Vk_Core.{h,cpp}`, `TriangleShader.hlsl`, `TriangleVertex.vert`, `TriangleFrag_Lit.frag`, `LightingPanel.{h,cpp}`, `VulkanDesktop.vcxproj`, SPIR-V outputs
- **What changed:**
  - Fragment: `mix(vertexColor, texture, blend)` albedo; ambient + Lambert diffuse + Blinn-Phong specular (`pow(N·H, shininess)`).
  - Env UBO: `myViewWorldPos` + `fogDistances` packs spec/shininess/tex blend; binding 2 `COMBINED_IMAGE_SAMPLER` for `viking_room.png`.
  - `LightingPanel` ImGui window edits `myEnvironmentData`; UBO upload after UI each frame.
  - `InitDefaultEnvironmentData()` sets defaults (spec 0.45, shininess 32, full texture).
- **Verification:** glslc compile OK. Rebuild in VS and run.
