# Plan: asset-root

**Sprint:** S0 — Foundation & tooling  
**Status:** Done (2026-05-22)

## Problem

`UtilLoader::ResolvePath` walks multiple cwd-relative bases (`BuildCandidateBases`) so assets work when the process starts from different Visual Studio output directories. That is **non-deterministic**, hard to document, and blocks the next S0 task (startup existence checks).

## Goals

1. Replace heuristic probing with a single **asset root** directory.
2. Load asset root from **`Config/engine.json`** (committed default).
3. Override via CLI: `--asset-root <path>`, optional `--config <file>`.
4. Use **repo-relative logical paths** in code (`Data/...`, `VulkanDesktop/Shader_Generated/...`).
5. Log resolved asset root and config source at startup.

## Non-goals

- Startup file-existence checks (next S0 item).
- Full central config (window, vsync, flags) — S2.
- Refactoring `UtilLogger::FindRepoRoot` to share code with asset config (optional later).

## Design

| Item | Decision |
|------|----------|
| Asset root | Directory that contains `Data/` and `VulkanDesktop/` (repository root). |
| Default root | Walk parents from cwd for `VulkanDesktop.sln` or `VulkanDesktop/VulkanDesktop.vcxproj`; else cwd. |
| Config file | `Config/engine.json` under discovered repo root; `"assetRoot": ""` means use default discovery. |
| Precedence | CLI `--asset-root` > config file non-empty `assetRoot` > default discovery. |
| `ResolvePath` | Absolute existing path unchanged; else `weakly_canonical(assetRoot / relativePath)`; no multi-base probe. |
| Logical paths | Shaders: `VulkanDesktop/Shader_Generated/*.spv`; content: `Data/Models/*`, `Data/Textures/*`. |

### CLI

| Flag | Effect |
|------|--------|
| `--asset-root <dir>` | Sets asset root (absolute or relative to cwd at startup). |
| `--config <file>` | Loads `assetRoot` from JSON before CLI override. |
| `--help` | Prints usage; exit 0. |

## Files

- `Config/engine.json` (new)
- `VulkanDesktop/Util/Util_AssetConfig.{h,cpp}` (new)
- `VulkanDesktop/Util/Util_Loader.{h,cpp}`
- `VulkanDesktop/VulkanDesktop.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.cpp` (demo logical paths)
- `VulkanDesktop/VulkanDesktop.vcxproj`, `.filters`
- `README.md` (short run note)
- `Docs/asset-root_Progress.md`, `Docs/SprintPlan.md`

## Steps

- [x] Plan
- [x] `Util_AssetConfig` + `Config/engine.json`
- [x] Wire CLI in `VulkanDesktop.cpp`; init before `Vk_Core::Run`
- [x] Replace `BuildCandidateBases` in `Util_Loader`
- [x] Update demo paths in `Vk_Core.cpp`
- [x] Archive S0 line in `SprintPlan.md`
- [x] Build Debug\|x64 and smoke-run (see `_Progress.md`)

## Verification

1. Build `VulkanDesktop` Debug\|x64.
2. Run from `x64\Debug` with default config: log shows asset root + mesh/shader loads succeed.
3. Run with `--asset-root` pointing at repo root from another cwd: same behavior.
4. Grep confirms `BuildCandidateBases` removed.

## Risks

- Missing `Data/Textures/viking_room.png` in repo still fails at load (addressed by next S0 startup-check task).
- JSON parser is minimal (single key only); invalid JSON logs warning and falls back to default root.
