# Progress: ddgi-lighting

## 2026-06-16 — S8 kickoff prep

- **Scope alignment:** moved `Temporal AO stability (motion vectors + AO history/reprojection)` from S6 backlog into S8 kickoff scope.
- **Plan sync:** updated [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) A-track with temporal-AO-baseline as a prerequisite deliverable.
- **Queue sync:** updated [`Active-Plan.md`](Active-Plan.md) and [`Wishlist.md`](Wishlist.md) so S8 owns the remaining AO-stability task.
- **Verification:** docs-only update (runtime/build N/A).

## 2026-06-16 — S8 Task 1: temporal AO baseline

- **Files:** `VulkanDesktop/RenderCore/Vk_AoPass.h`, `VulkanDesktop/RenderCore/Vk_AoPass.cpp`, `VulkanDesktop/RenderContract/GpuAoSettings.h`, `VulkanDesktop/Util/Util_LightingPanel.cpp`, `VulkanDesktop/Shader/AoTemporal.comp`, `VulkanDesktop/Shader/CompileShader_Glslc.bat`, `VulkanDesktop/VulkanDesktop.vcxproj`.
- **Outcome:** landed AO temporal history/reprojection baseline with history ping-pong resources, reprojection compute pass (`AoTemporal.comp`), and runtime controls (`AO temporal enabled`, `AO temporal blend`).
- **Verification:**
  - `powershell -File Scripts/Verify-CI.ps1` → **failed at shader drift gate** (expected generated `.spv` diffs present in working tree).
  - `powershell -File Scripts/Verify-Smoke.ps1` → default smoke passed; gpu-cull leg failed on existing log token assertion `(gpu-deferred)` mismatch.
  - `x64/Debug/VulkanDesktop.exe --asset-root ... --validation --smoke-frames 120 --smoke-seconds 6` → exit **0**; no `[VULKAN-VALIDATION]` lines in `Logs/engine_runtime_log.txt`.
