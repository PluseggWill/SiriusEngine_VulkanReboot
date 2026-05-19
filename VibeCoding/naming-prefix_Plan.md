# Plan: naming-prefix

## Goals

按既有约定统一 `Util_` / `Vk_` / `Gfx_` 前缀：

- **文件**：`Module_Name.{h,cpp}`，与模块目录一致
- **类型**：主类/结构体带模块前缀（`Vk_Core`、`Gfx_Mesh`、`Util_FrameStats`）
- **工具 API**：命名空间 `UtilLogger`、`UtilLoader`、`UtilLightingPanel`、`UtilStatsOverlay`（仿 `VkInit`）
- **GPU UBO**：保留 `Gpu*` 前缀（`GpuCameraData`、`GpuEnvironmentData`）

## Changes

| 原 | 新 |
|----|-----|
| `FrameStats` | `Util_FrameStats` |
| `LightingPanel_*` | `Util_LightingPanel` + `UtilLightingPanel::Build` |
| `StatsOverlay_*` | `Util_StatsOverlay` + `UtilStatsOverlay::Build` |
| `Vk_ImGuiLayer` | `Util_ImGuiLayer` |
| `Gfx_Camera` | `Vk_Camera` |
| `FrameData` | `Vk_FrameData` |
| `PipelineBuilder` | `Vk_PipelineBuilder` |
| `QueueFamilyIndices` 等 | `Vk_*` |
| `Vertex`/`Mesh`/… | `Gfx_*` |
| `Graphics/` | `Gfx/` |
| `Editor/*` | `Util/Util_*` |

## Verification

- [x] MSBuild（若本机有 VS）
- [ ] 用户本地打开解决方案确认
