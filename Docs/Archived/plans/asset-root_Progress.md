# Progress: asset-root

## 2026-05-22 — Plan + implementation

- **Plan ref:** Steps 1–5 (`Docs/asset-root_Plan.md`)
- **Files:** `Config/engine.json`, `VulkanDesktop/Util/Util_AssetConfig.{h,cpp}`, `Util_Loader.{h,cpp}`, `VulkanDesktop.cpp`, `Vk_Core.cpp`, `VulkanDesktop.vcxproj`, `.filters`, `README.md`, `Docs/SprintPlan.md`
- **What changed:** `UtilAssetConfig::Initialize` loads `Config/engine.json`, accepts `--asset-root` / `--config` / `--help`. `UtilLoader::ResolvePath` joins paths under the configured asset root (repo root by default via `VulkanDesktop.sln` walk). Demo paths are repo-relative (`Data/…`, `VulkanDesktop/Shader_Generated/…`). Removed `BuildCandidateBases` heuristic probing.
- **Verification:** MSBuild Debug|x64 OK; `--help` exits 0; run from `x64\Debug` logs `assetRoot` + resolves `VulkanDesktop/Shader_Generated/*.spv` and `Data/Models/*.obj` under repo root (`Logs/engine_runtime_log.txt` 2026-05-22 20:23:46).

## 2026-05-22 — Code review (close)

- **Review:** `UtilAssetConfig` precedence (CLI > config > `FindRepoRoot`) and directory check are correct; minimal JSON parser scoped to `assetRoot` only. `ResolvePath` is deterministic (asset root first, optional cwd fallback for absolute-adjacent paths). Demo paths repo-relative. No `BuildCandidateBases` left in code.
- **Follow-ups (out of scope):** share `FindRepoRoot` with `UtilLogger`; startup existence checks — done in `Docs/startup-checks_Plan.md`.
- **Archived:** `Docs/SprintPlan.md` → `## Archived` [S0] asset-root.
