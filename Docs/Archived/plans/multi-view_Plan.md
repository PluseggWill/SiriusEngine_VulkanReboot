# Plan: multi-view

**Sprint:** S2 — Multi-view  
**Status:** Closed (2026-06-02)  
**Roadmap:** [`Active-Plan.md`](../../Active-Plan.md) § Multi-view

## Goals

1. **`Gfx_RenderView`** — camera source (fly vs scene), normalized viewport, layer mask; optional RT = swapchain only (v1).
2. **Scene `cameras[]`** — parse eye/center/up/fov/viewport; `demo.json` overview PiP camera.
3. **Per-view extract + record** — rebuild draw stream and instance slab per view; patch Set 0 camera/env eye before each view's draws.
4. **Debug** — ImGui toggle PiP + secondary camera combo; dynamic viewport/scissor per view.

## Non-goals

- Offscreen render targets / frame graph (S7).
- Multiple swapchain images per view.

## Verification

- Debug\|x64 build (`MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64`) exit 0.
- Smoke: `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0.
- Log: `[SCENE] Parsed scene ... cameras=1`, `[EXTRACT] entities=9 draws=9`, `[PERF] ... drawCalls=18` when PiP enabled.
