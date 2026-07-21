# New machine bootstrap (Windows)

Step-by-step setup for building and smoke-testing **VulkanDesktop** on a clean Windows PC. For day-to-day run notes, see [`README.md`](../README.md).

## Prerequisites

| Component | Version / notes |
|-----------|-----------------|
| **OS** | Windows 10/11 x64 |
| **Visual Studio 2022** | Workload: **Desktop development with C++**; toolset **v143** (project default) |
| **Windows SDK** | 10.0 (VS installer default) |
| **GPU driver** | Vulkan 1.2+ capable (NVIDIA/AMD/Intel recent driver) |
| **Optional** | [Vulkan SDK](https://vulkan.lunarg.com/) for `vulkaninfo` / validation layers on the system (see [`validation-layers.md`](validation-layers.md)) |

This repo **vendors** headers/libs used by **Debug\|x64** and **Release\|x64** (repo-relative paths):

| Dependency | Path |
|------------|------|
| Vulkan headers/libs | `lib/VulkanSDK/1.2.182.0/` |
| GLFW 3.3.4 | `lib/VulkanSDK/glfw-3.3.4.bin.WIN64/` |
| glslc (shader compile) | `lib/VulkanSDK/1.2.182.0/Bin32/glslc.exe` |
| glm, stb, VMA, imgui, tinyobjloader | `lib/` |

> **Note:** Some legacy vcxproj configurations still reference `C:\VulkanSDK\...`. Use **Debug\|x64** or **Release\|x64** with the repo open from a clone — those configs use `..\lib\VulkanSDK\...`.

## 1. Clone and layout

```powershell
git clone <your-remote-url> SiriusEngine_VulkanReboot
cd SiriusEngine_VulkanReboot
```

Expected top-level entries:

- `VulkanDesktop.sln` — open this in Visual Studio
- `VulkanDesktop/` — engine source, shaders, `.vcxproj`
- `Data/` — demo meshes/textures
- `Config/engine.json` — central runtime config (window, vsync, asset root, scene, log level, validation, feature flags)
- `lib/` — third-party and Vulkan SDK payload
- `Logs/` — created at runtime (`engine_runtime_log.txt`, `shader_compile_log.txt`)

## 2. Build (Visual Studio)

1. Open `VulkanDesktop.sln`.
2. Configuration: **Debug** | **x64** (recommended for first build).
3. **Build** → **Build Solution** (or F5).

SPIR-V is produced by a **Custom Build** on `VulkanDesktop/Shader/TriangleVertex.vert` into `VulkanDesktop/Shader_Generated/`. If shaders fail, check `Logs/shader_compile_log.txt`.

**Command-line build** (same as verify script):

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msbuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
```

Output binary: `x64\Debug\VulkanDesktop.exe`.

## 3. Run

| Context | Asset root |
|---------|------------|
| **Local dev** (from `x64\Debug`) | Optional — auto-detect via `VulkanDesktop.sln` walk (deprecated; prefer explicit flag) |
| **Scripts / CI / agents** | **Required** `--asset-root <repo-root>` — see [`CLI.md`](CLI.md) |

Local quick start:

```powershell
.\x64\Debug\VulkanDesktop.exe --asset-root (Get-Location)
```

Or from `x64\Debug` without flag (legacy discovery):

```powershell
Set-Location x64\Debug
.\VulkanDesktop.exe
```

**CLI / config:** Full reference — [`CLI.md`](CLI.md). Quick: `.\VulkanDesktop.exe --help`.

At runtime, use the ImGui **Scene** window to switch between `Data/Scenes/*.json` without restarting.

## 4. Logs and success signals

| Log | Location |
|-----|----------|
| Runtime | `Logs/engine_runtime_log.txt` (fresh each run; repo root even when cwd is `x64\Debug`) |
| Shader compile | `Logs/shader_compile_log.txt` |
| History | `Logs/HistoryLogs/` — previous runs renamed to `<name>_yyyyMMdd_HHmmss.txt` (10 kept per log type) |

**Visual Studio F5** debugs `VulkanDesktop.exe` directly. Logs rotate via `RotateEngineLogs.bat` on each process start (`UtilLogger::Init`) and before **Build** (PreBuild event).

Manual rotation only:

```bat
call VulkanDesktop\Scripts\RotateEngineLogs.bat
```

After a normal startup (not `--help`), expect lines similar to:

```text
[INFO] [CONFIG] window=1600x1200 vsync=off
[INFO] [CONFIG] assetRoot=<canonical repo path>
[INFO] [STARTUP] Verifying scene asset manifest
[INFO] [VULKAN] Vulkan instance created.
```

Validation: [`validation-layers.md`](validation-layers.md).

## 5. Automated verify

From repository root (PowerShell 5.1+):

| Script | Gate | What it does |
|--------|------|----------------|
| [`Scripts/Verify-CI.ps1`](../Scripts/Verify-CI.ps1) | **G0** (blocks M2) | MSBuild Debug\|x64 → shader drift → `GfxTests.exe` |
| [`Scripts/Verify-Smoke.ps1`](../Scripts/Verify-Smoke.ps1) | G0-smoke | Graceful smoke (`sponza.json` + `engine.json`) + `Assert-SmokeLog.ps1` |
| [`Scripts/Verify-Bootstrap.ps1`](../Scripts/Verify-Bootstrap.ps1) | Bootstrap | Build → `--help` → smoke (same as Verify-Smoke) → startup log checks |

```powershell
powershell -ExecutionPolicy Bypass -File Scripts/Verify-CI.ps1
powershell -ExecutionPolicy Bypass -File Scripts/Verify-Smoke.ps1
```

Optional: `-RepoRoot <path>`, `-Configuration Debug`. Perf JSONL sample: [`CLI.md`](CLI.md) (`--perf-log`).

GitHub Actions mirrors **Verify-CI** on PRs; GPU smoke job is soft-fail v1 — see [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md).

## 6. Optional: system Vulkan check

If the [Vulkan SDK](https://vulkan.lunarg.com/) is installed:

```powershell
& "$env:VULKAN_SDK\Bin\vulkaninfo.exe" | Select-String "VK_LAYER_KHRONOS_validation"
```

Not required to compile (vendored SDK), but helps validation-layer troubleshooting.

## Troubleshooting

| Symptom | What to check |
|---------|----------------|
| MSBuild / vswhere not found | Install VS 2022 with C++ desktop workload |
| Link errors for `vulkan-1.lib` / `glfw3.lib` | Build **x64** Debug/Release (repo `lib/` paths), not Win32 |
| `[STARTUP] Missing required file` | Run from clone with `Data/` and `Shader_Generated/*.spv`; rebuild solution for SPIR-V |
| Black window / immediate exit | `Logs/engine_runtime_log.txt` for `[ERROR]`; GPU/driver Vulkan support |
| Wrong assets loaded | `[CONFIG] assetRoot=` in log; set `--asset-root` to repo root |
| Validation noise | `--no-validation` or Release build; see `validation-layers.md` |

Roadmap: [`Active-Plan.md`](Active-Plan.md) · [`Archived-Plan.md`](Archived-Plan.md). Architecture: [`EngineArchitecture.md`](EngineArchitecture.md).
