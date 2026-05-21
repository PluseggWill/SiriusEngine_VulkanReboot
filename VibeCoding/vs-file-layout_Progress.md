# Progress: vs-file-layout

## 2026-05-19 — Rewrite vcxproj.filters

- **Plan ref:** Steps — 重写 `.vcxproj.filters`
- **Files:** `VulkanDesktop/VulkanDesktop.vcxproj.filters`
- **What changed:** 移除 `Source Files` / `Header Files` 默认树；建立 **Util**（含 Editor、ThirdParty/imgui）、**RenderCore**、**Gfx**（含 Shader）；补全 vcxproj 中已有但未进 filters 的条目；`VulkanDesktop.cpp` 留在项目根。
- **Verification:** 需在 Visual Studio 中打开解决方案目视确认（本机未自动启动 VS）。

## 2026-05-21 — VS 过滤器目视确认（用户）

- **Plan ref:** 在 VS 中目视核对三类 + Shader 子树
- **Verification:** Solution Explorer 中 Util / RenderCore / Gfx\Shader 布局确认通过。
