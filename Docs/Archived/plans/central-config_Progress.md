# Progress: central-config

**Plan:** [`central-config_Plan.md`](central-config_Plan.md)  
**Archived:** 2026-05-29 (docs hygiene batch)

## Closeout — 2026-05-27

- **Outcome:** `Util_EngineConfig` + `Config/engine.json` with CLI precedence; logger init after config; window/vsync/demoRotate/runtimeMipmap from config; removed compile-time `ENABLE_ROTATE` / `USE_RUNTIME_MIPMAP`.
- **Verification:** MSBuild Debug|x64 exit 0; `--help` lists new flags; default run `[CONFIG] window=1600x1200 vsync=on`, `entities=9 draws=9`.
- **Deviations:** reordered `UtilLogger::LogLevel` enum so min-level filtering works with `logLevel=info`.
