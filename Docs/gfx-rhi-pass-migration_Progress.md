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

## 2026-07-21 — E2 AO Record via Gfx + Rhi

- **Files:** `Gfx/Gfx_AoPass.{h,cpp}`, `RenderCore/Vk_AoPass_Record.cpp` (thin facade), `Vk_Renderer` DeviceWrap, `Rhi_CommandList` CopyImage
- **Behavior:** AO compute record orchestration lives in Gfx (no vulkan.h); Init/descriptors/temporal descriptor writes stay in RenderCore
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0; G0-validation attempted (`--validation`) exit 0 but host lacks `VK_LAYER_KHRONOS_validation` (logged ERROR once at init — no runtime pass spam)

## 2026-07-21 — E2 closeout (sponza G0-validation)

- **Fixes (same branch):** DDGI atlas order/push-range, depth layout tracking, UNDEFINED→SHADER_READ_ONLY for Soft/SSR/bent/DDGI, PostProcess TAA leak on RebuildResources, smoke default→sponza, CULL once-per-scene
- **Verification:** sponza `--validation --smoke-frames 120 --smoke-seconds 6` exit 0; only remaining ERROR is benign `vkCreateDevice enabledLayerCount != 0`
- **E2 outcome:** Record pilot accepted; Init peel deferred to E4

## 2026-07-21 — E3 FramePlan + Gfx_RenderPipeline (start)

- **Files:** `Gfx/Gfx_PassId.h`, `Gfx/Gfx_FramePlan.h`, `Gfx/Gfx_RenderPipeline.{h,cpp}`, `Vk_FrameGraph` consumes Plan, GfxTests `TestHybridDeferredFramePlan`
- **Behavior:** Hybrid topology + enable bits built in Gfx; RC still owns Record switch / enable sampling from Vk state
- **Verification:** `Verify-CI.ps1` exit 0 (`GfxTests: all passed`); `Verify-Smoke.ps1 -SkipGpuCull` exit 0 (sponza)
