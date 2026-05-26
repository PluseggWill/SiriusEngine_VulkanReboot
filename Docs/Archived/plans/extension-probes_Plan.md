# Plan: extension-probes

**Sprint:** S0 — Foundation & tooling (Should complete #3)  
**Status:** Done (2026-05-23)

## Problem

`Vk_Core::CheckExtensionSupport()` (instance) and device suitability still use `std::cout`. Layer discovery already uses `UtilValidationLayers` + `UtilLogger`; extension probes should match.

## Goals

1. Replace **all** `std::cout` in extension/GPU probe paths with **`UtilLogger`**.
2. Instance enumeration: **`UtilLogger::Info`** listing every instance extension (same verbosity as today).
3. Device required extensions: on failure, **`UtilLogger::Warn`** listing **missing** extension names.
4. `CheckDeviceSuitable`: min uniform buffer alignment → **`UtilLogger::Debug`**.
5. Remove `#include <iostream>` from `Vk_Core.cpp` if no longer needed.

## Non-goals

- New `Util_VulkanProbes` module (stay in `Vk_Core` unless file grows too large).
- Renaming overloads `CheckExtensionSupport` (keep names; optional comment distinguishing instance vs device).
- Logging every device extension when check passes (noise).

## Design decisions

| Item | Decision |
|------|----------|
| Instance dump | `[VULKAN] Instance extension discovery (N):` + per-line `extensionName` at **Info** |
| Call site | Keep `#ifdef _DEBUG` before `vkCreateInstance` (behavior unchanged, sink changed) |
| Device check | On `requiredExtensions` non-empty after scan → Warn with comma-separated missing list |
| Alignment line | Debug: `[GPU] minUniformBufferOffsetAlignment=…` |
| Category | `[VULKAN]` for extensions; `[GPU]` for alignment |

## Files

- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `Docs/extension-probes_Progress.md`
- `Docs/SprintPlan.md` archive when done

## Implementation steps

- [x] 1. Rewrite instance `CheckExtensionSupport()` logging.
- [x] 2. Add missing-extension Warn in device `CheckExtensionSupport(VkPhysicalDevice)`.
- [x] 3. Replace alignment `std::cout` with `UtilLogger::Debug`.
- [x] 4. Remove unused iostream include.
- [x] 5. Build Debug|x64; `_DEBUG` run — confirm log lines in `engine_runtime_log.txt`.
- [x] 6. Progress + SprintPlan archive.

## Build / smoke-run

- MSBuild Debug|x64.
- Debug run 4s; grep `[VULKAN] Instance extension discovery` and `[GPU] minUniformBufferOffsetAlignment`.

## Risks

- Large extension list on some ICDs — acceptable (user chose full Info list).

## Rollback

Revert `Vk_Core.cpp` logging only.
