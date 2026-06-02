# Plan: vk-core-world-peel

**Status:** Planned  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P1**  
**Covers recommendations:** #3, #6, #8, #9

## Problem

`Vk_Core` (~1150 LOC) still owns scene CPU state, ImGui panels, and exposes private members to **9 `friend` classes**. Peel to date moved orchestration but not **ownership** or **boundaries**.

## Goal

Render backend consumes **packets + contexts** only; world/scene CPU state and debug UI live outside `Vk_Core`.

## Non-goals

- Full ECS library
- Deleting `Vk_Core` singleton in one pass
- Frame graph (S7)

---

## Target architecture

```text
Application
  ├─ WorldState (SoA, LOD table, transform resolve, scene desc ids)
  ├─ DebugUIState (ImGui panel structs, built in App or Util overlay)
  └─ RenderSession
        └─ Vk_Core / Vk_RenderDevice … (Vulkan only)
              reads: const WorldSnapshot + FrameRenderPacket[]
```

### Context structs (replace friend soup) (#8)

**Landing:** Introduce plain structs passed into peel modules:

| Struct | Fields (illustrative) |
|--------|------------------------|
| `Vk_DeviceContext` | `VkDevice`, queues, allocator, physical device props |
| `Vk_FrameContext` | current frame index, frame datas, swapchain extent |
| `Vk_SceneGpuContext` | pipelines, descriptor sets, resource tables, material path |

Static functions take `(Vk_DeviceContext&, …)` instead of `friend` + `Vk_Core&`. Delete `friend` entries as each module migrates.

---

## Phased steps

### Phase 1 — WorldState extraction (#3, #9)

| Step | Landing |
|------|---------|
| 1.1 | New `App/WorldState.h` (or `Gfx/Gfx_WorldState.h`): move `Gfx_SceneSoA`, `Gfx_LodTable`, `Gfx_LodState`, `Gfx_SceneTransformState`, `Gfx_SceneIdTables`, loaded scene desc metadata |
| 1.2 | `Application` owns `WorldState`; `Vk_Core` accessors removed from public API |
| 1.3 | `Vk_FrameDrawPrep::Build` takes `const WorldState&` / pointers — no `core.GetSceneSoA()` |
| 1.4 | `Vk_SceneHost::LoadCpuState` writes into `WorldState&` owned by Application |

**Done when:** `Vk_Core.h` no longer `#include`s scene SoA headers in public section; grep `GetSceneSoA` → 0 outside World/load paths.

### Phase 2 — Debug UI out of DrawFrame (#6)

| Step | Landing |
|------|---------|
| 2.1 | `DebugOverlayState` struct: render debug skips, multi-view PiP flags, lighting/camera panel inputs |
| 2.2 | `Application`: after input, call `DebugOverlay::Build(state, worldStats)` **before** `core.Render(state, packets)` |
| 2.3 | Remove ImGui calls from `Vk_Core::DrawFrame`; RenderCore only runs ImGui **render pass** for already-built draw lists if needed, or App calls `Vk_ImGuiRecord` with prepared `ImDrawData*` |

**Done when:** `Vk_Core.cpp` contains zero `ImGui::Begin` except optional thin wrapper.

### Phase 3 — Slim DrawFrame (#3)

| Step | Landing |
|------|---------|
| 3.1 | `DrawFrame` ≤ ~250 LOC: acquire → prep delegate → record delegate → present |
| 3.2 | Metrics: track LOC + `#include` count in PR template |

---

## Touch list (expected)

- `App/Application.cpp`, new `App/WorldState.*`
- `RenderCore/Vk_Core.*`, `Vk_SceneHost.*`, `Vk_FrameDrawPrep.*`, `Vk_PlatformFrame.*`
- `Util/*Panel*` — callers moved to Application
- `Docs/EngineArchitecture.md` §3.1, §9

## Verification

- Build + smoke 6s unchanged behavior (PiP, scene reload, ForwardLit)
- `grep -c "friend class" Vk_Core.h` decreases each phase
- RenderDoc capture still works

## Risks

- Large single PR → land Phase 1 before M2 GPU work to avoid merge hell
- Scene reload path must update `WorldState` without touching Vulkan headers
