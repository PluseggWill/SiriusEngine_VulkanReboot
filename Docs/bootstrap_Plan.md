# Plan: bootstrap

**Sprint:** S0 — Foundation & tooling (Should complete #1)  
**Status:** Done (2026-05-23)

## Problem

New contributors lack a single path to install tools, build, and verify the repo on a clean Windows machine. `README.md` covers run/shaders briefly but not toolchain versions or automated checks.

## Goals

1. Expand **`README.md`** with a short “New machine” section linking to detailed bootstrap doc.
2. Add **`Docs/bootstrap.md`**: toolchain versions, repo layout, build/run, log locations, common failures.
3. Add **`Scripts/Verify-Bootstrap.ps1`**: environment checks + MSBuild `Debug|x64` + run `--help` + parse `Logs/engine_runtime_log.txt` for expected tokens.

## Non-goals

- CI/GitHub Actions (backlog).
- Fixing `VulkanDesktop.vcxproj` hardcoded `C:\VulkanSDK\...` on non-repo configs (document repo-relative `lib/` as the supported path).
- Linux/macOS bootstrap.

## Design decisions

| Item | Decision |
|------|----------|
| Doc split | README summary + `Docs/bootstrap.md` detail |
| Toolchain | VS 2022 (v143), Windows 10 SDK (`10.0`), x64 Debug default |
| Vulkan / GLFW | Vendored `lib/VulkanSDK/1.2.182.0`, `lib/VulkanSDK/glfw-3.3.4.bin.WIN64` (matches Debug\|x64 vcxproj) |
| glslc | `lib/VulkanSDK/1.2.182.0/Bin32/glslc.exe` (VS Custom Build) |
| GPU | Recent Vulkan-capable driver; optional `vulkaninfo` check documented |
| Verify script | Full smoke: vswhere → MSBuild → `VulkanDesktop.exe --help` → log grep |
| Script exit | Non-zero on any failed step; print summary table |

### Verify script checks (minimum)

| Step | Pass signal |
|------|-------------|
| Repo root | `VulkanDesktop.sln` exists from script cwd or `-RepoRoot` |
| MSBuild | vswhere finds MSBuild; build exit 0 |
| Binary | `x64\Debug\VulkanDesktop.exe` exists after build |
| `--help` | Process exit 0; stderr contains `Usage:` |
| Runtime log | `Logs/engine_runtime_log.txt` contains `[CONFIG] assetRoot=`, `[STARTUP] All required demo assets present.` (after a normal Debug run from `x64\Debug` **or** script runs exe with cwd `x64\Debug` for smoke) |

**Note:** `--help` exits before startup checks; script will run a second minimal invocation **without** `--help` (4s minimized window) from `x64\Debug` to populate startup log lines, unless plan revision prefers only build+help.

### Revised smoke sequence (confirmed for implementation)

1. Build solution.
2. `VulkanDesktop.exe --help` (exit 0).
3. Short run from `x64\Debug` (4s, kill if alive) for `[STARTUP]` / `[CONFIG]` log lines.
4. Grep log for keywords.

## Files (touch list)

- `README.md`
- `Docs/bootstrap.md` (new)
- `Scripts/Verify-Bootstrap.ps1` (new)
- `Docs/bootstrap_Progress.md` (new, during implement)
- `Docs/SprintPlan.md` (archive line when done)

## Implementation steps

- [x] 1. Draft `Docs/bootstrap.md` (prerequisites, clone, VS workload, open `VulkanDesktop.sln`, build, run, logs, validation link, troubleshooting).
- [x] 2. Update `README.md` — “New machine” bullet list + links to `Docs/bootstrap.md` and `Scripts/Verify-Bootstrap.ps1`.
- [x] 3. Implement `Scripts/Verify-Bootstrap.ps1` (`-RepoRoot` optional, `-Configuration Debug`, `-Platform x64`).
- [x] 4. Self-run script from clean shell; fix doc/script mismatches.
- [x] 5. Append `Docs/bootstrap_Progress.md`; move S0 README/bootstrap line to `SprintPlan.md` → Archived.

## Build / smoke-run

| Step | Command |
|------|---------|
| Build | `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64` |
| Script | `pwsh -File Scripts/Verify-Bootstrap.ps1` from repo root |
| Manual | Follow `Docs/bootstrap.md` checklist on a machine without prior build |

**Success:** script exit 0; README/bootstrap paths accurate.

## Risks

- Machine without VS: script fails at vswhere — documented in bootstrap.md.
- First run from wrong cwd: script forces `x64\Debug` for smoke run; docs state recommended cwd.

## Rollback

Remove `Scripts/`, revert README/bootstrap doc; no code changes in `VulkanDesktop/`.
