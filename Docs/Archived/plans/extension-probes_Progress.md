# Progress: extension-probes

## 2026-05-23 — Task 3 complete

- **Plan ref:** Steps 1–6; Build / smoke-run
- **Files:** `VulkanDesktop/RenderCore/Vk_Core.cpp`, `Docs/SprintPlan.md`
- **What changed:**
  - Instance `CheckExtensionSupport()`: `[VULKAN] Instance extension discovery (N):` + per-extension Info lines.
  - Device `CheckExtensionSupport(VkPhysicalDevice)`: Warn with comma-separated missing required extensions.
  - `CheckDeviceSuitable`: `minUniformBufferOffsetAlignment` → `[GPU]` Debug.
  - Removed `#include <iostream>`; no `std::cout` left under `VulkanDesktop/`.
- **Verification:**
  - MSBuild Debug|x64 exit 0.
  - 4s smoke: log contains `Instance extension discovery` and `minUniformBufferOffsetAlignment=`.
