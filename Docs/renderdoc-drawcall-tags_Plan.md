# Plan: renderdoc-drawcall-tags

**Sprint:** S2 — Engine tooling / debug visibility  
**Status:** In Progress (2026-06-02)  
**Task line:** RenderDoc integration + drawcall tags (startup-gated)

## Problem statement

Current Vulkan desktop runtime has no explicit RenderDoc integration toggle and no per-draw labels visible in Event Browser. This slows draw-stream diagnosis during frame capture.

## Goals

1. Add a startup CLI option to enable RenderDoc integration path.
2. Only initialize RenderDoc API when startup option is present.
3. Add F12 hotkey to trigger capture in runtime.
4. Add RenderDoc-visible labels for pass + every drawcall in scene record path.
5. Keep behavior unchanged when RenderDoc option is absent or RenderDoc is not installed.

## Non-goals

- No hard dependency on RenderDoc SDK at build/link time.
- No changes to render output, scene format, or descriptor policy.
- No GPU-marker requirements beyond Vulkan `debug_utils` label support.

## Design decisions

- Use dynamic runtime loading (`GetModuleHandleA`/`LoadLibraryA`) of `renderdoc.dll`; do not add link-time dependency.
- Add CLI switch `--renderdoc` in `Util_EngineConfig`.
- Initialize RenderDoc in app init only when `--renderdoc` is set.
- Trigger frame capture via F12 edge detection in main loop.
- Label granularity: pass + each drawcall, with detailed content:
  - `Pass=<Opaque|Transparent> Draw=<index> Mesh=<id> Material=<id> Entity=<index>`
- Use `vkCmdBeginDebugUtilsLabelEXT` / `vkCmdEndDebugUtilsLabelEXT` when function pointers are available.

## Touch list

- `VulkanDesktop/Util/Util_EngineConfig.h`
- `VulkanDesktop/Util/Util_EngineConfig.cpp`
- `VulkanDesktop/App/Application.h`
- `VulkanDesktop/App/Application.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.h`
- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `VulkanDesktop/RenderCore/Vk_ScenePasses.h`
- `VulkanDesktop/RenderCore/Vk_ScenePasses.cpp`
- `Docs/CLI.md`
- `Docs/renderdoc-drawcall-tags_Progress.md`

## Implementation steps

1. Add config/CLI support for `--renderdoc` and expose query API.
2. Add RenderDoc integration state/API inside `Vk_Core`:
   - startup-gated initialization
   - F12-triggered capture
   - debug label helpers
3. Wire startup-gated init from `Application::InitApp`.
4. Add F12 edge-trigger in main loop.
5. Add pass/draw labels in `Vk_ScenePasses` record loops.
6. Update `Docs/CLI.md` with new switch.
7. Build and run smoke verification.

## Verification plan

- Build `Debug|x64`:

```powershell
& "<MSBuild.exe>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
```

- Smoke-run:

```powershell
Set-Location x64\Debug
.\VulkanDesktop.exe --no-validation --smoke-seconds 6
```

- RenderDoc run checks:
  - Start with `--renderdoc`.
  - Press F12 in runtime and verify capture is triggered.
  - In RenderDoc Event Browser, verify pass and per-draw labels appear.

## Risks and rollback

- Risk: `VK_EXT_debug_utils` labels unavailable on some setups.
  - Mitigation: guard via function pointers; no-op if unavailable.
- Risk: per-draw string formatting overhead in debug capture mode.
  - Mitigation: only emits command labels; acceptable for debug flow.
- Rollback: disable `--renderdoc` usage; labels and capture path stay dormant.
