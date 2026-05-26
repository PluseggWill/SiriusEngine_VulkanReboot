# Progress: validation-layers

## 2026-05-22 — Plan

- **Plan ref:** Design + steps
- **Files:** `Docs/validation-layers_Plan.md`
- **What changed:** Task plan for S0 validation layers.
- **Verification:** N/A (plan only)

## 2026-05-22 — Implement validation layers

- **Plan ref:** Steps — Util modules, wire main/Vk_Core, docs
- **Files:** `Util_ValidationConfig.{h,cpp}`, `Util_ValidationLayers.{h,cpp}`, `Util_AssetConfig.cpp`, `VulkanDesktop.cpp`, `Vk_Core.cpp`, `README.md`, `Docs/validation-layers.md`, `VulkanDesktop.vcxproj`, `.filters`, `Docs/SprintPlan.md`
- **What changed:** Layer discovery logged via `UtilLogger` before `vkCreateInstance`. Enablement: CLI `--validation` / `--no-validation` > `enableValidationLayers` in `engine.json` > Debug/Release default. `CheckValidationLayerSupport` delegates to `UtilValidationLayers`. Install/troubleshooting in `Docs/validation-layers.md`. AssetConfig ignores validation CLI flags (parsed earlier).
- **Verification:** MSBuild Debug|x64 exit 0. Log: `Instance layer discovery`, `validationLayers=enabled (build default)`, `Validation layers: enabled`. `--no-validation`: `validationLayers=disabled (CLI)`, `Validation layers: disabled`. `--help` lists validation flags.
