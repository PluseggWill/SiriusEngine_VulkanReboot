# Progress: input-abstraction

## 2026-05-27 — Plan (Phase 2)

- **Plan ref:** I0
- **Files:** `Docs/input-abstraction_Plan.md`, `Docs/input-abstraction_Progress.md`
- **Verification:** N/A

## 2026-05-27 — I1–I3 implementation

- **Plan ref:** I1–I3
- **Files:** `App/InputSystem.{h,cpp}`, `Application.{h,cpp}`, `Vk_Core.{h,cpp}`, vcxproj/filters, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:**
  - `InputSystem` owns `Util_InputState` and ImGui-gated `UtilInput::Sample`.
  - Main loop: `BeginPlatformFrame` → `InputSystem::Sample` → `ApplyCameraInput` → `Render`.
  - Removed `Vk_Core::Update`, `myInputState`, and RenderCore `UtilInput` include.
- **Verification:**
  - Build Debug|x64 — exit 0
  - Smoke: graceful close; `entities=9 draws=9`
  - Grep: no `UtilInput::Sample` under `RenderCore/`
