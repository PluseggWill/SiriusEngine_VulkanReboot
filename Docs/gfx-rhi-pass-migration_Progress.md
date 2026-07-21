# Progress: gfx-rhi-pass-migration

**Plan:** [`gfx-rhi-pass-migration_Plan.md`](gfx-rhi-pass-migration_Plan.md)

## 2026-07-21 — E0 policy + E1 Rhi surface

- **Files:** `Docs/EngineArchitecture.md`, `Docs/Active-Plan.md`, `Docs/README.md`, `Docs/Wishlist.md`, `Docs/gfx-rhi-pass-migration_Plan.md`, `.cursor/rules/vulkan-desktop-layout.mdc`, `VulkanDesktop/Rhi/*`, `VulkanDesktop/RenderCore/Vk_RhiBackend.*`, `VulkanDesktop.vcxproj(.filters)`, `GfxTests.vcxproj`, `GfxTests_Main.cpp`
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; `GfxTests: all passed`; smoke/validation N/A (no GPU pass change)
- **Rhi header gate:** `VulkanDesktop/Rhi/*.h` contain no `vulkan.h`

## 2026-07-21 — E1b Rhi surface expansion + ownership fixes

- **Review fixes:** move-only `Rhi_Device` / `Rhi_CommandList`; device/CL refcount so DeviceDestroy does not UAF live lists; no by-value handle copies in backend helpers
- **API:** `Rhi_Enums`; buffer/texture/shader create-destroy; CommandList barrier/bind/push/dispatch/draw; `RhiVulkan::DeviceWrap` + adopt helpers + debug-utils hooks
- **Tests:** refcount after DeviceDestroy; CreateBuffer fails closed without logical device
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; `GfxTests: all passed`
