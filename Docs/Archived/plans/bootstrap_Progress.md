# Progress: bootstrap

## 2026-05-23 — Task 1 complete

- **Plan ref:** Implementation steps 1–5; Build / smoke-run
- **Files:** `README.md`, `Docs/bootstrap.md`, `Scripts/Verify-Bootstrap.ps1`, `Docs/bootstrap_Plan.md`, `Docs/SprintPlan.md`
- **What changed:**
  - Added new-machine section to README with links to bootstrap doc and verify script.
  - Added `Docs/bootstrap.md` (VS 2022/v143, vendored `lib/VulkanSDK`, build/run, logs, troubleshooting).
  - Added `Scripts/Verify-Bootstrap.ps1` (vswhere/MSBuild, `--help` via `cmd /c` to avoid stderr terminating, 4s smoke from `x64\Debug`, log keyword grep).
  - Documented `powershell` invocation (pwsh not required on host).
- **Verification:**
  - `powershell -ExecutionPolicy Bypass -File Scripts/Verify-Bootstrap.ps1` → exit **0**; all 7 steps PASS (build, --help, smoke, `[CONFIG] assetRoot=`, `[STARTUP] All required demo assets present.`).
