# Progress: debug-messenger

## 2026-05-23 — Task 2 complete

- **Plan ref:** Steps 1–6; Build / smoke-run
- **Files:** `VulkanDesktop/Util/Util_DebugMessenger.{h,cpp}`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, `VulkanDesktop.vcxproj`, `.filters`, `Docs/validation-layers.md`, `README.md`, `Docs/SprintPlan.md`
- **What changed:**
  - Added `UtilDebugMessenger`: `VK_EXT_debug_utils` callback → `UtilLogger` (`[VULKAN-VALIDATION]`), VERBOSE–ERROR severities.
  - `Vk_Core::CreateInstance`: merge GLFW + debug utils extension; chain messenger on `VkInstanceCreateInfo`; post-create handle.
  - `Vk_Core::Clear`: destroy messenger before `vkDestroyInstance`.
  - Docs: validation messages now in `engine_runtime_log.txt`.
- **Verification:**
  - MSBuild Debug|x64 exit 0.
  - 4s smoke: log contains `Debug utils messenger created` and `[VULKAN-VALIDATION]` validation errors (dynamic viewport VUID; pre-existing pipeline issue).
