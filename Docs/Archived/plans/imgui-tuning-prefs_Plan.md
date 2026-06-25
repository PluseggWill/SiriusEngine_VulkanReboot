# Plan: imgui-tuning-prefs — ImGui 调参持久化

**Status:** Closed (2026-06-25)  
**Progress:** [`imgui-tuning-prefs_Progress.md`](imgui-tuning-prefs_Progress.md)

## Problem

Engine Debug 中 Lighting / DDGI / AO / Post / Camera 等调参仅存在于运行时；重启后回到代码默认值，需手改 `GpuAoSettings.h` 等。

## Goals

- 启动时自动加载 `Config/user-tuning.json`（若存在），覆盖 `engine.json` / 代码默认值。
- Engine Debug 窗口顶部提供 **保存调参**、**恢复默认**。
- 调参范围：Lighting（含 sun/ambient）、DDGI、AO（含 contact soft）、Post、Camera；不含 Render 调试、Scene、Views。

## Non-goals

- 不写回 `engine.json`；不持久化 debug view / skip pass / 场景路径。
- 不自动保存；不支持多 profile / CLI 路径（v1）。
- 不改 `EngineArchitecture.md`（非 locked policy）。

## Design

- 新模块 `Util_TuningPrefs`：`Capture` / `Apply` / `LoadFromFile` / `SaveToFile` / `ResetToBaseline`。
- 文件：`{assetRoot}/Config/user-tuning.json`，`version: 1`；加入 `.gitignore`。
- 加载顺序：代码默认 → `engine.json` lighting 子集（已有）→ `user-tuning.json`（在首场景 `App_InitScenePresentation` 之后应用，以便覆盖 env sun/ambient）。
- **恢复默认**：删除 `user-tuning.json`；重置 Ao/Post/Camera 为 struct 默认；Lighting 为 struct 默认 + `engine.json` lighting 覆盖；环境光/太阳调用 `App_ApplyDefaultEnvironment`；viewport gizmo 回默认。

## Touch list

| File | Change |
|------|--------|
| `Util/Util_TuningPrefs.{h,cpp}` | JSON 读写 + apply/capture |
| `App/SceneCpuLoad.{h,cpp}` | 抽出 `App_ApplyDefaultEnvironment` |
| `App/Application.cpp` | 场景激活后 auto-load |
| `App/DebugOverlay.cpp` | Save / Reset 按钮 |
| `Util/Util_LightingPanel.{h,cpp}` | gizmo toggle 访问器 |
| `VulkanDesktop.vcxproj` + `.filters` | 新源文件 |
| `.gitignore` | `Config/user-tuning.json` |
| `GfxTests` 或内联测试 | JSON round-trip（可选，优先 GfxTests 小用例） |

## Steps

1. [ ] `Util_TuningPrefs` snapshot + JSON v1 schema
2. [ ] `App_ApplyDefaultEnvironment` + startup load + reset baseline
3. [ ] DebugOverlay Save/Reset UI + gizmo accessors
4. [ ] vcxproj / gitignore / Verify-CI

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- Manual: 改 AO radius → 保存 → 重启 → 值保留；恢复默认 → 重启 → 回到默认

## Risks

- `myFogDistance.w` 为 debug view — 禁止写入 tuning 文件。
- 场景切换后 tuning 仍全局生效（v1 接受）。
