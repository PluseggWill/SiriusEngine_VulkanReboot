## Closeout — 2026-06-02

- **Plan ref:** `Docs/Archived/plans/renderdoc-drawcall-tags_Plan.md`
- **Scope delivered:**
  - Added startup-gated RenderDoc integration via `--renderdoc`.
  - Added F12 capture request path.
  - Added pass + per-draw command labels visible in RenderDoc (`Pass/Draw/Mesh/Material/Entity`).
  - Refactored RenderDoc runtime logic into `Vk_RenderDoc` helper and cleaned debug-era redundant capture plumbing.
  - Locked runtime behavior to passive attach (`GetModuleHandle("renderdoc.dll")`) for stability.
  - Added capture stability guard: force Batch material path when RenderDoc module is injected.
- **Verification:**
  - Build passed (`Debug|x64`).
  - Smoke run passed (`--no-validation --smoke-seconds 6 --renderdoc`).
  - Runtime logs confirmed startup gate and passive attach behavior.
- **Outcome:** Closed; artifacts moved to `Docs/Archived/plans/`.
