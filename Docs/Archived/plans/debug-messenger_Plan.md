# Plan: debug-messenger

**Sprint:** S0 — Foundation & tooling (Should complete #2)  
**Status:** Done (2026-05-23)

## Problem

Validation layers can be enabled, but messages only appear in the debugger console. `Docs/validation-layers.md` notes messenger is missing; `Vk_Core::InitVulkan` has `// TODO: Set up debug messenger`.

## Goals

1. Implement **`VK_EXT_debug_utils`** callback → **`UtilLogger`** (category `[VULKAN-VALIDATION]`).
2. Create messenger **only when** `myEnableValidationLayers == true` (after successful layer availability check).
3. Wire instance extensions + `VkInstanceCreateInfo::pNext` chain; destroy messenger before `vkDestroyInstance`.
4. Update **`Docs/validation-layers.md`** and README troubleshooting (messages now in `Logs/engine_runtime_log.txt`).

## Non-goals

- Messenger when validation disabled.
- `VK_EXT_debug_report` legacy path.
- Filtering presets / ImGui toggle (S7).

## Design decisions

| Item | Decision |
|------|----------|
| Module | `Util/Util_DebugMessenger.{h,cpp}` |
| When | `myEnableValidationLayers` true only |
| Instance ext | `VK_EXT_DEBUG_UTILS_EXTENSION_NAME` added to enabled instance extensions |
| Create | `UtilDebugMessenger::Setup(instance, createInfo)` — patches `createInfo.pNext` with messenger create info for validation at instance create **or** post-create `vkCreateDebugUtilsMessengerEXT` |
| Severity | **All** standard severities: VERBOSE, INFO, WARNING, ERROR → map to Debug/Info/Warn/Error |
| Message types | GENERAL, VALIDATION, PERFORMANCE → include type in log line |
| Destroy | `UtilDebugMessenger::Destroy(instance)` from `Vk_Core::Clear()` before `vkDestroyInstance` |
| Load PFNs | `vkGetInstanceProcAddr` for Create/DestroyDebugUtilsMessengerEXT |

### Log format (example)

```
[VULKAN-VALIDATION] ERROR validation: …
```

## Files

- `VulkanDesktop/Util/Util_DebugMessenger.{h,cpp}` (new)
- `VulkanDesktop/RenderCore/Vk_Core.cpp` (CreateInstance, Clear, InitVulkan — remove TODO)
- `VulkanDesktop/VulkanDesktop.vcxproj` + `.filters`
- `Docs/validation-layers.md`, `README.md` (short note)
- `Docs/debug-messenger_Progress.md`
- `Docs/SprintPlan.md` archive when done

## Implementation steps

- [x] 1. Implement `Util_DebugMessenger` (callback, create/destroy, extension name constant).
- [x] 2. `CreateInstance`: merge GLFW extensions + debug utils when validation on; call Setup on `VkInstanceCreateInfo`.
- [x] 3. `Clear`: Destroy messenger before instance destroy.
- [x] 4. Docs update; remove “messenger not implemented” wording.
- [x] 5. Build Debug|x64; smoke with validation on — trigger a known harmless validation message or confirm callback invoked; check log file.
- [x] 6. Progress log + SprintPlan archive.

## Build / smoke-run

- MSBuild Debug|x64.
- Run 4s from `x64\Debug` with validation enabled (default Debug).
- Grep `Logs/engine_runtime_log.txt` for `[VULKAN-VALIDATION]` (may be empty if no violations — document; optional: force a debug printf via layer config if needed).

## Risks

- Missing extension when layers enabled: fail-soft log Warn, continue without messenger (layers may still output to debugger).
- Duplicate `pNext` chain: use struct chaining helper pattern (document in code comment).

## Rollback

Remove Util module and revert `Vk_Core` instance create/clear paths.
