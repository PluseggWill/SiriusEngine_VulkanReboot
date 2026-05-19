# Progress: naming-prefix

## 2026-05-19 — Prefix rename pass

- **Plan ref:** Changes table
- **Files:** Util/*, RenderCore/*, Gfx/*, VulkanDesktop.vcxproj(.filters)
- **What changed:** 统一 Util/Vk/Gfx 前缀；Editor 并入 Util 目录；Graphics 重命名为 Gfx；更新 vcxproj 路径。
- **Verification:** 修正 `pQueueFamilyIndices`、`offsetof(Gfx_Vertex)` 等误替换；MSBuild Debug|x64 编译通过。
