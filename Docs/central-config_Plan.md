# Plan: central-config

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Done (2026-05-27)  
**Related:** [`application-lifecycle_Plan.md`](application-lifecycle_Plan.md), [`EngineArchitecture.md`](EngineArchitecture.md) §3, [`SprintPlan.md`](SprintPlan.md) § S2

## Problem

Runtime settings are split across `VulkanDesktop.cpp` (window size), `Util_AssetConfig` / `Util_ValidationConfig` (hand-rolled JSON), `Vk_Core.h` (compile-time feature flags), and `ChooseSwapPresentMode` (implicit vsync). No single config surface for tools, presets, or scene-load Phase D2.

## Goals

1. Single `Config/engine.json` v1 schema (window, vsync, asset root, scene, log level, validation, feature flags).
2. `Util_EngineConfig` loads JSON via nlohmann, merges CLI overrides, logs `[CONFIG]` summary.
3. Wire Application + `Vk_Core` + `UtilLogger` + demo flags.
4. Keep existing CLI flags working (`--asset-root`, `--config`, `--scene`, `--validation`, `--log-level`).
5. Default behavior matches today (`demo.json`, 1600×1200, vsync on, demo rotate on).

## Non-goals

- scene-load strict/warn asset policy (JSON placeholder optional only)
- S7 presets, bindless force override, hot reload

## Design (confirmed Q1A–Q6A)

| Item | Decision |
|------|----------|
| Module | `Util_EngineConfig.{h,cpp}`; `Util_AssetConfig` / `Util_ValidationConfig` thin delegates |
| JSON | nlohmann/json (same as scene-load) |
| Log level | Min-level filter; logger init after config in `Application::InitApp` |
| Vsync | `true` → FIFO; `false` → MAILBOX → IMMEDIATE → FIFO |
| Features v1 | `demoRotate`, `runtimeMipmap`, `enableValidationLayers` |
| Precedence | CLI > config file > code defaults |

## Files

| Area | Paths |
|------|--------|
| New | `VulkanDesktop/Util/Util_EngineConfig.{h,cpp}` |
| Change | `Config/engine.json`, `Application.{h,cpp}`, `VulkanDesktop.cpp`, `Util_Logger.*`, `Util_AssetConfig.*`, `Util_ValidationConfig.*`, `Vk_Core.*`, `Util_Loader.cpp`, vcxproj/filters |
| Docs | `Docs/central-config_Progress.md`, `bootstrap.md`, `SprintPlan.md`, `EngineArchitecture.md` |

## Steps

- [x] C0 — Plan + progress
- [x] C1 — `Util_EngineConfig` + `engine.json`
- [x] C2 — Logger min level + init order
- [x] C3 — Window / vsync / features wiring
- [x] C4 — Thin AssetConfig / ValidationConfig delegates
- [x] C5 — Docs sync + build/smoke-run

## Verification

1. MSBuild Debug|x64 — exit 0
2. `--help` lists config flags
3. Default run: `[CONFIG]` width/height, vsync, logLevel, assetRoot, scene, features; `entities=9 draws=9`
4. Invalid JSON → clear error before Vulkan

## Risks

- Early `UtilLogger` callers before InitApp — `main` must not log before config (except `--help` on stderr)
