# Plan: architecture-layer-hygiene

**Status:** Closed (2026-06-13)  
**Related:** [`EngineArchitecture.md`](EngineArchitecture.md) §1, archived [`gfx-vk-decoupling_Plan.md`](Archived/plans/gfx-vk-decoupling_Plan.md), [`naming-prefix_Plan.md`](Archived/plans/naming-prefix_Plan.md)

## Problem

Disk modules (App / Gfx / RenderCore) have documented boundaries, but:

1. **Inverted dependencies:** `Gfx_ViewData` includes `Vk_Types.h`; `RenderCore` `#include`s `App/DebugUIState.h` and `App/WorldState.h`.
2. **Type ownership:** `Gfx_Mesh` / `Gfx_Material` / `Gfx_Texture` live in `Vk_Types.h` with Vulkan handles — violates layout rule (“new Gfx types → Gfx/”).
3. **Orchestration:** `Vk_FrameDrawPrep::Build` runs Gfx extract/batch inside RenderCore; scene CPU populate lives in `Vk_SceneHost`.
4. **Naming:** six `Vk_Frame*` concepts; folder `RenderCore/` vs prefix `Vk_`; dead `Gfx_ViewData` stub.

## Goals

1. Restore strict dependency direction: App → Gfx → RenderCore; no RenderCore → App includes.
2. Split CPU scene resource types (Gfx) from GPU resident types (RenderCore `Vk_*Resource`).
3. Gfx owns extract→batch packet build; RenderCore owns slab/template upload + record.
4. Add Frame* glossary to `EngineArchitecture.md` §1.

## Non-goals

- Renaming disk folder `RenderCore/` → `Vk/` (file prefix already `Vk_`).
- Shader / descriptor policy changes.
- Removing all RenderCore → Gfx includes (packets, lighting, presets remain valid downstream consumption).

## Phases

### P0 — Coupling guardrails

- [x] Delete dead `Gfx_ViewData`.
- [x] Add `Gfx_FrameDebugToggles`; pass recording uses toggles not `DebugUIState`.
- [x] Remove `#include "../App/*"` from RenderCore `.cpp` files.

### P1 — Scene load + type split

- [x] Move `Vk_SceneHost` CPU/presentation helpers → `App/SceneCpuLoad`.
- [x] `Gfx_FramePrepInput` replaces `WorldState&` on `PrepareFrameCpu`.
- [x] Split `Vk_Types.h`: Gfx CPU types in Gfx/; `Vk_MeshResource` / `Vk_MaterialResource` / `Vk_TextureResource` in RenderCore.

### P2 — CPU prep ownership

- [x] `Gfx_BuildViewFramePacket` — extract/batch in Gfx.
- [x] `Vk_FrameDrawPrep::Build` — slab/template/entity upload only.

### P3 — Docs + verify

- [x] `EngineArchitecture.md` §1 glossary + boundary table update.
- [x] `Verify-CI.ps1` green.

## Verification

```powershell
powershell -File Scripts/Verify-CI.ps1
powershell -File Scripts/Verify-Smoke.ps1
```

## Risks

| Risk | Mitigation |
|------|------------|
| Large rename churn | Keep getter names on `Vk_ResourceTables`; phased commits |
| Record path regressions | G0 + G0-smoke after each phase |
