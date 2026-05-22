# Progress: fps-camera

## 2026-05-20 — Refactor Vk_Camera + MainLoop input

- **Plan ref:** Steps — camera refactor + Vk_Core input loop
- **Files:** `VulkanDesktop/RenderCore/Vk_Camera.h`, `Vk_Camera.cpp`, `Vk_Core.h`, `Vk_Core.cpp`
- **What changed:** Yaw/pitch camera with delta-time WASD/QE and RMB mouse look; ImGui `NewFrame` runs before input so capture flags apply; removed discrete key-repeat movement.
- **Verification:** MSBuild Debug|x64 succeeded (`VulkanDesktop.exe`).

## 2026-05-20 — Maintainability pass

- **Plan ref:** Review follow-ups (input decouple, BeginFrame, Camera panel, remove dead callback)
- **Files:** `Util_InputSnapshot.h`, `Util_Input.{h,cpp}`, `Util_CameraPanel.{h,cpp}`, `Vk_Camera.{h,cpp}`, `Vk_Core.{h,cpp}`, `VulkanDesktop.vcxproj`, `.filters`
- **What changed:** `UtilInput::Sample` produces `Util_InputSnapshot`; `Vk_Camera::ApplyInput` has no GLFW; `BeginFrame()` fixes poll → ImGui → sample → camera order; Camera ImGui panel for speed/sensitivity; removed `HandleInputCallback`.
- **Verification:** MSBuild Debug|x64 succeeded.

## 2026-05-20 — Manual verification + docs

- **Plan ref:** Verification / runbook
- **Files:** `Docs/fps-camera_Plan.md`, `Docs/TODOList.md`, `Docs/EngineArchitecture.md`
- **What changed:** Plan marked done; TODO and architecture notes updated.
- **Verification:** User confirmed runtime behavior; Camera panel tuning OK.
