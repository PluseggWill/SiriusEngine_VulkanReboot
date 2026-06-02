# Progress: config-platform-hardening

- **Plan:** [`config-platform-hardening_Plan.md`](config-platform-hardening_Plan.md) (this folder)
- **Parent:** Active-Plan P1 → [`Archived-Plan.md`](Archived-Plan.md) § P1 config-platform-hardening

## Closeout — 2026-06-02

- **Outcome:** `Util_EngineConfig` instance on `Application` (`myConfig`); `Vk_Core::BindEngineConfig`; no file-static `g*` in `Util_EngineConfig.cpp`; GfxTests `TestConfigPrecedence` (JSON vs CLI); `Docs/Platform.md` + Architecture §10 platform lock; `Vk_FrameResult` + non-throwing submit/present/acquire (device lost → `RequestShutdown`).
- **Phase 0 baseline (pre-change, from plan):** ~20 file-static `g*` in `Util_EngineConfig.cpp`; ~15 external `UtilEngineConfig::Get*` sites; swapchain throws on submit/present; 3× `#include <Windows.h>`. **Post:** `g*` **0**; `UtilEngineConfig::Get*` **0**; swapchain `throw` only in `Create*` helpers (3 lines); Win32 files = Platform.md table (4 paths).
- **Verification:** `Scripts/Verify-CI.ps1` exit **0** (GfxTests incl. precedence); `Scripts/Verify-Smoke.ps1` exit **0**; log `[CONFIG] assetRoot=`, `[APP] Engine exited run loop normally`.
- **Acceptance:** P1-A1–A6, P2-D1–D3, P3-V1–V2, P3-V5 via code/grep; P3-V3 resize soak **not automated** (manual per plan; optional `Verify-ResizeSmoke.ps1` deferred).
- **Deviations:** `LoadFromArgv` throws on invalid input (plan non-goal unchanged); `UtilEngineConfig` namespace kept for `TryEarlyExitFromCli` / `PrintUsage` only.
