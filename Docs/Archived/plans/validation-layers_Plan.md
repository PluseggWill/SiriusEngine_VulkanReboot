# Plan: validation-layers

**Sprint:** S0 — Foundation & tooling  
**Status:** Done (2026-05-22)

## Problem

Validation layer enablement is hard-coded in `VulkanDesktop.cpp` (`NDEBUG` only). Layer probing uses `std::cout` in `CheckValidationLayerSupport`, and there is no install/run documentation or runtime toggle.

## Goals

1. **Install guide** — README + `Docs/validation-layers.md` (Windows / Vulkan SDK).
2. **Startup discovery log** — enumerate instance layers via `UtilLogger` (`[VULKAN]`) before instance create; log requested layers and enable/disable decision.
3. **Runtime switch** — CLI `--validation` / `--no-validation`; optional `enableValidationLayers` in `Config/engine.json` (CLI overrides config; config overrides build default).

## Non-goals

- `VK_EXT_debug_utils` messenger (S0 Should: debug messenger).
- Migrating `CheckExtensionSupport` to logger (S0 Should; separate).

## Design

| Item | Decision |
|------|----------|
| Layer list | `VK_LAYER_KHRONOS_validation` (unchanged) |
| Default | Debug builds: on; Release: off (`NDEBUG`) |
| Precedence | CLI > `engine.json` > build default |
| Discovery | `UtilValidationLayers::LogInstanceLayerDiscovery()` — always before `vkCreateInstance` |
| Support check | `UtilValidationLayers::AreLayersAvailable()` — replaces cout-based `CheckValidationLayerSupport` body |

## Files

- `VulkanDesktop/Util/Util_ValidationConfig.{h,cpp}`
- `VulkanDesktop/Util/Util_ValidationLayers.{h,cpp}`
- `VulkanDesktop/Util/Util_AssetConfig.cpp` (usage text + forward unknown-args: validation parsed in main first)
- `VulkanDesktop/VulkanDesktop.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `Config/engine.json`
- `README.md`, `Docs/validation-layers.md`
- `Docs/validation-layers_Progress.md`, `Docs/SprintPlan.md`
- `VulkanDesktop.vcxproj`, `.filters`

## Steps

- [x] Plan
- [x] Util modules + config/CLI
- [x] Wire Vk_Core + main; README/docs
- [x] Build Debug\|x64 + smoke (`--help`, `--no-validation`, default Debug log lines)
- [x] Archive S0 in SprintPlan

## Verification

1. MSBuild Debug\|x64 exit 0.
2. Run: log lists instance layers; `validation=enabled|disabled` line before instance create.
3. `--no-validation`: layers listed, instance created without enabled layers.
4. `--help` documents validation flags.
