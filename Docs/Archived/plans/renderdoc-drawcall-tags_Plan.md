# Plan: renderdoc-drawcall-tags

**Sprint:** S2 — Engine tooling / debug visibility  
**Status:** Closed (2026-06-02)  
**Task line:** RenderDoc integration + drawcall tags (startup-gated)

## Problem statement

Vulkan desktop runtime had no explicit RenderDoc integration toggle and no per-draw labels visible in Event Browser, which slowed draw-stream diagnosis during capture.

## Goals

1. Add a startup CLI option to enable RenderDoc integration path.
2. Only initialize RenderDoc API when startup option is present.
3. Add F12 hotkey to trigger capture in runtime.
4. Add RenderDoc-visible labels for pass + every drawcall in scene record path.
5. Keep behavior unchanged when RenderDoc option is absent.

## Final landing decisions

- `--renderdoc` is the only startup gate for RenderDoc integration.
- Runtime uses passive attach only (`GetModuleHandle("renderdoc.dll")`), no in-app `LoadLibrary` or downloader/bootstrap path.
- Capture trigger uses RenderDoc API `TriggerCapture` (requested from F12 edge).
- Command labels use `VK_EXT_debug_utils`:
  - Pass labels (`Pass=Opaque`, `Pass=Transparent`)
  - Per-draw labels (`Pass=<...> Draw=<...> Mesh=<...> Material=<...> Entity=<...>`)
- For injected RenderDoc sessions, renderer forces Batch material path for capture stability on current driver/runtime combinations.

## Touch list

- `VulkanDesktop/Util/Util_EngineConfig.h`
- `VulkanDesktop/Util/Util_EngineConfig.cpp`
- `VulkanDesktop/App/Application.h`
- `VulkanDesktop/App/Application.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.h`
- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `VulkanDesktop/RenderCore/Vk_ScenePasses.h`
- `VulkanDesktop/RenderCore/Vk_ScenePasses.cpp`
- `VulkanDesktop/RenderCore/Vk_Bindless.cpp`
- `VulkanDesktop/RenderCore/Vk_RenderDoc.h`
- `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp`
- `Docs/CLI.md`
- `Docs/renderdoc-drawcall-tags_Progress.md` (archived counterpart)

## Verification snapshot

- Build: `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` (pass).
- Smoke run: `.\VulkanDesktop.exe --no-validation --smoke-seconds 6 --renderdoc` (pass).
- Log expectations:
  - `renderdoc=enabled` when CLI gate is set.
  - Passive-mode warning when DLL is not injected.
  - `VK_EXT_debug_utils command labels enabled` when extension entry points are available.

## Notes

- This task is archived. Follow-up work should start a new task file pair under `Docs/` and reference this archive for integration history.
