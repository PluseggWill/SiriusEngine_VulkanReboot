# Progress: imgui-tuning-prefs

## 2026-06-25 — Implement tuning persistence + tab layout

- **Plan ref:** Steps 1–4
- **Files:** `Util_TuningPrefs`, `Util_TuningPanel`, `DebugOverlay`, `Util_LightingPanel`, `Application`, `DebugUIState`, `SceneCpuLoad`
- **What changed:** Engine Debug tabs; `Config/user-tuning.json` save/load; removed legacy ImGui sliders.
- **Verification:** Verify-CI exit 0

## 2026-06-25 — Review: decouple layers

- **Plan ref:** Deviation (minor) — split JSON (`Util_TuningPrefs`) from App bridge (`Util_TuningPanel`); viewport overlays moved to `DebugUIState`.
- **Verification:** Verify-CI exit 0

## Closeout — 2026-06-25

- **Outcome:** ImGui 调参分 tab；`user-tuning.json` 持久化；移除 legacy UI；分层清晰（JSON / 面板 / App 状态）。
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0
- **Deviations:** Added `Util_TuningPanel` (plan touch list updated).
