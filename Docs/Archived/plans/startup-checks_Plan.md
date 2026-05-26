# Plan: startup-checks

**Sprint:** S0 — Foundation & tooling  
**Status:** Done (2026-05-22)

## Problem

Missing SPIR-V, meshes, or textures are only discovered mid-`InitVulkan()` (after instance/device work). That wastes GPU setup time and produces opaque loader errors deep in the render path.

## Goals

1. Verify all **demo-required** repo-relative assets exist as regular files under the configured asset root.
2. Run checks **after** `UtilAssetConfig::Initialize` and **before** `Vk_Core::Run()` / `vkCreateInstance`.
3. Log each path as `[STARTUP] OK …` or `[STARTUP] ERROR …` with logical + resolved paths.
4. Fail fast with `std::runtime_error` (caught in `main`, exit non-zero).

## Non-goals

- Validating file contents (OBJ/PNG decode, SPIR-V magic).
- Scene-file-driven asset lists — superseded by [`scene-load_Plan.md`](scene-load_Plan.md) Phase B.
- CI headless smoke (backlog).

## Design

| Item | Decision |
|------|----------|
| Path source | `Util_DemoAssets.h` — single list shared with `Vk_Core` demo globals |
| Resolution | `UtilLoader::ResolvePath` (same as runtime loads) |
| Required set | `TriangleVert.spv`, `TrianglePix.spv`, `viking_room.png`, `viking_room.obj`, `monkey_smooth.obj` |
| API | `UtilStartupChecks::VerifyRequiredAssets()` |
| Call site | `VulkanDesktop.cpp` after asset config, before `Run()` |

## Files

- `VulkanDesktop/Util/Util_DemoAssets.h` (new)
- `VulkanDesktop/Util/Util_StartupChecks.{h,cpp}` (new)
- `VulkanDesktop/VulkanDesktop.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `VulkanDesktop/VulkanDesktop.vcxproj`, `.filters`
- `Docs/startup-checks_Progress.md`, `Docs/SprintPlan.md`

## Steps

- [x] Plan
- [x] `Util_DemoAssets` + `Util_StartupChecks`
- [x] Wire `VulkanDesktop.cpp`; point `Vk_Core` paths at shared constants
- [x] Build Debug\|x64 + smoke-run + negative test (missing file)
- [x] Archive S0 line in `SprintPlan.md`

## Verification

1. MSBuild `VulkanDesktop.sln` Debug\|x64 — exit 0.
2. Smoke-run from `x64\Debug`: log shows `[STARTUP] OK` for five files before `[VULKAN]`.
3. Rename one `.spv` temporarily: startup fails before `Vulkan instance created`.
4. `--help` still exits 0 without asset checks.

## Risks

- Texture not in git clone → startup fails until asset is present (intended; document in README if needed).
