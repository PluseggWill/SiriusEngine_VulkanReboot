# Progress: startup-checks

## 2026-05-22 — Plan

- **Plan ref:** Design + steps
- **Files:** `Docs/startup-checks_Plan.md`
- **What changed:** Task plan for S0 startup existence checks.
- **Verification:** N/A (plan only)

## 2026-05-22 — Implement startup checks

- **Plan ref:** Steps — Util modules, wire main, verify
- **Files:** `VulkanDesktop/Util/Util_DemoAssets.h`, `Util_StartupChecks.{h,cpp}`, `VulkanDesktop.cpp`, `RenderCore/Vk_Core.cpp`, `VulkanDesktop.vcxproj`, `.filters`, `Docs/SprintPlan.md`
- **What changed:** `UtilStartupChecks::VerifyRequiredAssets()` resolves and verifies five demo files via `UtilLoader::ResolvePath` after asset-root init. `VulkanDesktop.cpp` calls it before `Vk_Core::Run()`. Demo paths centralized in `Util_DemoAssets.h`.
- **Verification:** MSBuild Debug|x64 exit 0. Smoke-run from `x64\Debug`: `[STARTUP] OK` ×5 before `[VULKAN] Vulkan instance created`. `--help` exit 0 (no checks). Renamed `TriangleVert.spv` → startup error, no Vulkan instance in log, exit non-zero; file restored.

## 2026-05-22 — Review + README

- **Plan ref:** Verification / risks (README note)
- **Files:** `VulkanDesktop.cpp`, `Util_DemoAssets.h`, `README.md`
- **What changed:** Wrap asset config + startup checks in the same `try` as `Run()` so missing assets log `[APP]` and return `EXIT_FAILURE` instead of terminating uncaught. Document startup checks in README; note `Util_DemoAssets` is temporary (scene-load).
- **Verification:** Rebuild Debug|x64 exit 0; smoke-run OK.

## 2026-05-26 — Superseded by scene-load Phase B

- **Plan ref:** `Docs/scene-load_Plan.md` Phase B; archived note in `startup-checks_Plan.md` Non-goals
- **Files:** Removed `Util_StartupChecks.{h,cpp}`; startup verify is `Util_VerifyManifest(Util_CollectDependencies(scene))` in `VulkanDesktop.cpp`
- **What changed:** Hard-coded `UtilDemoAssets::kRequiredFiles` retired; manifest from `Data/Scenes/*.json` is the boot-time asset closure.
- **Verification:** N/A (tracked in `Docs/scene-load_Progress.md`)
