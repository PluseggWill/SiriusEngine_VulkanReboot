# Plan: fps-camera

**Status:** Done (manual verification passed 2026-05-20)

## Problem

`Vk_Camera` movement was tied to `glfwSetKeyCallback` (discrete key repeat), used **world axes** not view axes, and `Move()` did not update `myCenter` / `lookAt`. `Rotate()` was empty. No mouse look.

## Goals

- **WASD**: camera-relative strafe / forward-back on the horizontal plane (Z-up).
- **Q / E**: world-up / world-down along +Z / −Z.
- **Mouse**: yaw/pitch while **right mouse button** held; disabled when ImGui captures mouse/keyboard.
- **Smooth**: movement and rotation scaled by **per-frame delta time**.
- Keep `myEye` / env `myViewWorldPos` compatible for lighting shaders.

## Non-goals

- Gamepad, sprint, collision, camera smoothing/acceleration curves
- Changing default scene / demo mesh animation (`ENABLE_ROTATE` model spin unchanged)

## Design (final)

| Item | Decision |
|------|----------|
| Orientation | Yaw around world Z, pitch around camera right; clamp pitch ±89° |
| Input | `UtilInput::Sample(GLFW)` → `Util_InputSnapshot` (device-neutral) |
| Camera | `Vk_Camera::ApplyInput(Δt, snapshot, settings)` — no GLFW in RenderCore camera |
| Frame order | `Vk_Core::BeginFrame`: poll → Δt → ImGui `NewFrame` → sample → apply camera |
| Mouse | RMB + cursor disabled while looking |
| Tuning | `Util_CameraPanel` (ImGui): move speed, mouse sensitivity |
| Dead code | Removed `HandleInputCallback` / `glfwSetKeyCallback` |

## Files

- `VulkanDesktop/Util/Util_InputSnapshot.h`
- `VulkanDesktop/Util/Util_Input.{h,cpp}`
- `VulkanDesktop/Util/Util_CameraPanel.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_Camera.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_Core.{h,cpp}`
- `VulkanDesktop/VulkanDesktop.vcxproj`, `.filters`

## Steps

- [x] Plan
- [x] Yaw/pitch camera + delta-time WASD/QE + RMB mouse look
- [x] `BeginFrame()` frame order; remove key callback
- [x] Input decouple (`Util_InputSnapshot`, `ApplyInput`)
- [x] ImGui Camera panel
- [x] Manual run verification

## Verification

Rebuild `VulkanDesktop`, run: RMB + mouse look; WASD/QE fly relative to view; Lighting and Camera panels work when UI has focus.

## Controls (runbook)

| Input | Action |
|-------|--------|
| W / S | Forward / back (horizontal) |
| A / D | Strafe |
| Q / E | Down / up (world Z) |
| Hold RMB + mouse | Look |
| Camera panel | Move speed, mouse sensitivity |
