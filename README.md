# VulkanDesktop

Vulkan learning / engine reboot sandbox (SiriusEngine).

## New machine (Windows)

First-time setup: install **Visual Studio 2022** (Desktop C++ / v143), clone the repo, build **Debug|x64**, run a quick verify.

| Resource | Purpose |
|----------|---------|
| [`Docs/bootstrap.md`](Docs/bootstrap.md) | Toolchain versions, repo layout, build/run, logs, troubleshooting |
| [`Scripts/Verify-Bootstrap.ps1`](Scripts/Verify-Bootstrap.ps1) | Automated: MSBuild → `--help` → short run → check `Logs/engine_runtime_log.txt` |

```powershell
powershell -ExecutionPolicy Bypass -File Scripts/Verify-Bootstrap.ps1
```

Vendored **Vulkan 1.2.182** + **GLFW 3.3.4** under `lib/VulkanSDK/` (no system SDK required to compile). Optional system Vulkan SDK for `vulkaninfo` / validation layers: [`Docs/validation-layers.md`](Docs/validation-layers.md).

## Run (Windows)

Build `VulkanDesktop.sln` (Debug|x64), run `x64\Debug\VulkanDesktop.exe`.

**Asset root:** relative paths (`Data/…`, `VulkanDesktop/Shader_Generated/…`) resolve under the repository root. Default: auto-detect via `VulkanDesktop.sln` walk from cwd. Override in `Config/engine.json` (`"assetRoot"`) or CLI `--asset-root <dir>`. Optional `--config <file>`.

**Scene & startup:** Default scene `Data/Scenes/stress.json` (`--scene <path>` to override; Stage 1 golden uses `demo.json`). Before Vulkan init, the app parses the scene and verifies all referenced assets exist (`[STARTUP]`). **Scene JSON authoring:** [`Docs/SceneJSON.md`](Docs/SceneJSON.md). Lifecycle/handoff: [`Docs/Archived/plans/scene-load_Plan.md`](Docs/Archived/plans/scene-load_Plan.md).

**Validation layers:** Khronos validation (`VK_LAYER_KHRONOS_validation`) is on in Debug builds by default, off in Release. Override with `Config/engine.json` (`enableValidationLayers`) or `--validation` / `--no-validation`. Layer messages go to `Logs/engine_runtime_log.txt` as `[VULKAN-VALIDATION]` when the debug utils messenger is active. Install and troubleshooting: [`Docs/validation-layers.md`](Docs/validation-layers.md).

## Shaders (GLSL → SPIR-V)

Sources under `VulkanDesktop/Shader/` → SPIR-V in `Shader_Generated/` via vendored **glslc** (VS Custom Build / `CompileShader.bat`). Details: [`Docs/bootstrap.md`](Docs/bootstrap.md). Logs: `Logs/engine_runtime_log.txt`, `Logs/shader_compile_log.txt`.

## Debug camera

| Input | Action |
|-------|--------|
| W / S / A / D | Move on horizontal plane (view-relative) |
| Q / E | Down / up (world Z) |
| Hold **RMB** + mouse | Look around |
| ImGui **Camera** panel | Move speed, mouse sensitivity |

Details: `Docs/Archived/plans/fps-camera_Plan.md`. Roadmap: `Docs/Active-Plan.md`. Architecture: `Docs/EngineArchitecture.md` §3.1.
