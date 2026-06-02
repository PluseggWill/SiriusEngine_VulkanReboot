# Plan: input-abstraction

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Done (2026-05-27)  
**Related:** [`fps-camera_Plan.md`](fps-camera_Plan.md), [`application-lifecycle_Plan.md`](application-lifecycle_Plan.md), [`../../EngineArchitecture.md`](../../EngineArchitecture.md) §3.1

## Problem

`Vk_Core::BeginFrame` still owns GLFW sampling (`UtilInput::Sample`), persistent `Util_InputState`, and ImGui capture gating. Render backend should consume a snapshot, not poll devices.

## Goals

1. `App/InputSystem` owns `Util_InputState` and per-frame `Util_InputSnapshot`.
2. `Application` loop: platform tick → input sample → camera apply → render.
3. `Vk_Core` exposes `GetWindow()`, `BeginPlatformFrame`, `ApplyCameraInput`; no `UtilInput::Sample` in RenderCore.
4. `UtilInput::Sample` stays GLFW backend in `Util/` (unchanged contract).
5. Default fly camera + demo behavior unchanged.

## Non-goals

- Gamepad, action mapping, rebinding UI
- Player controller / gameplay consumers (expose snapshot only)
- Moving ImGui init out of `Vk_Core`

## Design

| Item | Decision |
|------|----------|
| App layer | `InputSystem` in `VulkanDesktop/App/` |
| ImGui capture | `InputSystem::Sample` reads `ImGuiIO` after `Vk_Core` calls `NewFrame` |
| Camera | `Vk_Camera::ApplyInput` still in `Vk_Core::ApplyCameraInput` |
| Future gameplay | Read `InputSystem::GetSnapshot()` from Application |

## Files

| Area | Paths |
|------|--------|
| New | `VulkanDesktop/App/InputSystem.{h,cpp}` |
| Change | `Application.{h,cpp}`, `Vk_Core.{h,cpp}`, docs |

## Steps

- [x] I0 — Plan + progress
- [x] I1 — `InputSystem` + Application loop wiring
- [x] I2 — Split `Vk_Core::BeginFrame` → `BeginPlatformFrame` + `ApplyCameraInput`
- [x] I3 — Docs sync + build/smoke-run

## Verification

1. MSBuild Debug|x64 — exit 0
2. Smoke: WASD + RMB look; ImGui panels work; `entities=9 draws=9`
3. Grep: no `UtilInput::Sample` in `RenderCore/`
