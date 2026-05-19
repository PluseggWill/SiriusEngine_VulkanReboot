# Progress: vs-file-layout

## 2026-05-19 — Rewrite vcxproj.filters

- **Plan ref:** Steps — 重写 `.vcxproj.filters`
- **Files:** `VulkanDesktop/VulkanDesktop.vcxproj.filters`
- **What changed:** 移除 `Source Files` / `Header Files` 默认树；建立 **Util**（含 Editor、ThirdParty/imgui）、**RenderCore**、**Gfx**（含 Shader）；补全 vcxproj 中已有但未进 filters 的条目；`VulkanDesktop.cpp` 留在项目根。
- **Verification:** 需在 Visual Studio 中打开解决方案目视确认（本机未自动启动 VS）。
