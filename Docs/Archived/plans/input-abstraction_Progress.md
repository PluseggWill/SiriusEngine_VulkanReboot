# Progress: input-abstraction

**Plan:** [`input-abstraction_Plan.md`](input-abstraction_Plan.md)  
**Archived:** 2026-05-29 (docs hygiene batch)

## Closeout — 2026-05-27

- **Outcome:** `InputSystem` owns `Util_InputState` and ImGui-gated sampling; `Application` loop is platform → input → `ApplyCameraInput` → render; removed `Vk_Core::Update` and RenderCore `UtilInput` usage.
- **Verification:** MSBuild Debug|x64 exit 0; smoke graceful close, `entities=9 draws=9`; grep: no `UtilInput::Sample` under `RenderCore/`.
- **Deviations:** none.
