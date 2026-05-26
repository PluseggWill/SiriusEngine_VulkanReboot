# Plan: vs-file-layout

## Problem

`VulkanDesktop.vcxproj.filters` 仍使用默认的 `Source Files` / `Header Files` 分层，且未收录 Editor、FrameStats、Util_Logger、imgui 等已在 `.vcxproj` 中的条目，Solution Explorer 与磁盘目录不一致。

## Goals

在 Visual Studio 中按三类组织 Solution Explorer：

| 过滤器 | 内容 |
|--------|------|
| **Util** | 功能性辅助：Logger、Loader、FrameStats、Editor（ImGui/面板）、第三方 imgui |
| **RenderCore** | Vulkan 核心：`Vk_*` |
| **Gfx** | 图形数据与着色器：`Gfx_ViewData`、Shader 目录 |

入口 `VulkanDesktop.cpp` 保留在项目根过滤器（不强行归入三类）。

## Non-goals

- 不重命名磁盘上的 `Graphics\` → `Gfx\`（仅 VS 显示为 Gfx）
- 不移动 `Editor\` 物理路径（归入 Util 过滤器）
- 不改 include 路径或编译选项

## Files

- `VulkanDesktop/VulkanDesktop.vcxproj.filters` — 重写过滤器树

## Steps

- [x] 编写本计划
- [x] 重写 `.vcxproj.filters`，覆盖全部 ClCompile / ClInclude / None
- [x] 在 VS 中目视核对三类 + Shader 子树

## Verification

打开 `VulkanDesktop.sln`，确认无文件落在 `Source Files`/`Header Files` 下，Editor 与 imgui 在 Util 下，Shader 在 Gfx\Shader 下。

## Risks

无构建影响（仅 filters）；若 GUID 冲突可重新生成。
