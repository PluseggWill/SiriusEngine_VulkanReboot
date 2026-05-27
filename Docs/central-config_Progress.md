# Progress: central-config

## 2026-05-27 — Plan (Phase 2)

- **Plan ref:** C0
- **Files:** `Docs/central-config_Plan.md`, `Docs/central-config_Progress.md`
- **What changed:** Plan from confirmed Q1A–Q6A landing details.
- **Verification:** N/A (documentation only)

## 2026-05-27 — C1–C5 implementation

- **Plan ref:** C1–C5
- **Files:** `Util_EngineConfig.{h,cpp}`, `Config/engine.json`, `Util_Logger.*`, `Util_AssetConfig.cpp`, `Util_ValidationConfig.cpp`, `Application.{h,cpp}`, `VulkanDesktop.cpp`, `Vk_Core.{h,cpp}`, `Util_Loader.cpp`, vcxproj/filters, `Docs/bootstrap.md`, `SprintPlan.md`, `EngineArchitecture.md`
- **What changed:**
  - Central `Util_EngineConfig` (nlohmann JSON) with CLI precedence; `[CONFIG]` summary log.
  - Logger init moved to `Application::InitApp` after config; min log level filter.
  - Window/vsync from config; `ChooseSwapPresentMode` honors vsync; `demoRotate` / `runtimeMipmap` feature flags.
  - Removed hard-coded `WIDTH`/`HEIGHT` and `ENABLE_ROTATE` / `USE_RUNTIME_MIPMAP` compile-time toggles.
- **Verification:**
  - Build Debug|x64 — exit 0
  - `--help` lists new flags; default run `[CONFIG] window=1600x1200 vsync=on`; `entities=9 draws=9`
  - Grep: no `ENABLE_ROTATE` / `USE_RUNTIME_MIPMAP` in `VulkanDesktop/`
- **Review fix:** Reordered `UtilLogger::LogLevel` (Debug=0 … Error=3) so min-level filtering hides DEBUG when `logLevel=info`.
