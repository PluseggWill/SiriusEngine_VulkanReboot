# Plan: vk-core-world-peel

**Status:** In progress (2026-06-02)  
**Progress:** [`vk-core-world-peel_Progress.md`](vk-core-world-peel_Progress.md)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P1**  
**Covers recommendations:** #3, #6, #8, #9  
**Prerequisite:** P0 closed — use [`Scripts/Verify-CI.ps1`](../Scripts/Verify-CI.ps1) + [`Scripts/Verify-Smoke.ps1`](../Scripts/Verify-Smoke.ps1) each phase.

**Related:** [`vk-core-decomposition` (archived)](Archived/plans/vk-core-decomposition_Plan.md) (M1–M3 done; **ownership** peel is this plan), [`config-platform-hardening_Plan.md`](config-platform-hardening_Plan.md), [`shader-bindless-policy_Plan.md`](shader-bindless-policy_Plan.md).

---

## Problem

`Vk_Core` (~1426 LOC `.cpp`, ~286 LOC `.h`) still **owns** scene CPU state, debug UI state, and exposes **9 `friend` classes** (+ shader layout test) that read/write `aCore.my*`. Prior decomposition moved **orchestration** (`Vk_FrameDrawPrep`, `Vk_ScenePasses`, …) but not **ownership** or **App↔RenderCore boundaries**.

**Symptoms today (grep baseline):**

| Signal | Current |
|--------|---------|
| `friend class` in `Vk_Core.h` | 9 modules + `VkShaderEffectMeta` test |
| `GetSceneSoA()` / `GetSceneTransformState()` | Public on `Vk_Core`; **Application** main loop uses both |
| Scene CPU members on `Vk_Core` | `mySceneSoA`, `myLodTable`, `myLodState`, `mySceneTransformState`, `mySceneIdTables`, `myLoadedScene`, … |
| ImGui in `DrawFrame` | `Util*Panel::Build` ×5 + inline **Multi-view** `ImGui::Begin` |
| Scene reload | `UtilScenePanel::State` on core → `TakePendingSceneReloadPath()` |
| `DrawFrame` LOC | ~155 (already under 250) — **misleading** as sole metric; `#include` + friends + hot-path getters matter more |

---

## Goal

**Application** owns world CPU state and builds debug UI inputs; **RenderCore** consumes **`const WorldState&`**, prepared **`Gfx_FrameRenderPacket[]`**, and GPU/session state only.

**Active-Plan acceptance (interpreted):**

- `GetSceneSoA()` / `GetSceneTransformState()` **removed from `Vk_Core` public API**; sim/resolve in `Application` via `WorldState`.
- `DrawFrame` stays a thin orchestrator (target ≤ ~250 LOC — already close; **do not** pad LOC to hit a number).
- `friend class` count **monotonically decreases** each context migration slice.

---

## Non-goals

- Full ECS library
- Removing `Vk_Core` singleton in one pass
- Frame graph (S7) / M2 GPU cull (P2–P3)
- Moving `Vk_ResourceTables` load off core (separate backlog; M1 done)
- Eliminating all `Vk_Core&` module entry points in one PR (Phase 4 is incremental)

---

## Critical review (反思)

### What the draft got right

- Phases 1→2→3 order matches risk: **ownership before UI before cosmetic slimming**.
- `WorldState` + context structs align with [`EngineArchitecture.md`](EngineArchitecture.md) §1 diagram and §3.1 boot loop.
- Explicit non-goals and “land before M2” merge guidance.

### Gaps fixed in this revision

1. **Phase 4 was missing** — `Vk_*Context` appeared in the goal table but not in phased steps; friend removal needs a dedicated migration track.
2. **DrawFrame LOC** is already ~155; real work is **moving members**, **scene reload**, and **prep↔panel ordering** (`UtilRenderDebugPanel` needs post-prep draw counts).
3. **`WorldState` contents** were underspecified — must include `Gfx_SceneDesc` + id tables + reload metadata, not only SoA columns.
4. **Presentation vs world** — fly camera + env UBO stay session/GPU-adjacent; do not block Phase 1 on moving them to App.
5. **P1 parallel tracks** — config instance (#7) should land **before or with** Phase 1.1 so `Application` is the real root object graph.
6. **Verification** — must name G0 scripts, not hand-wavy “smoke 6s”.
7. **API shape for Phase 2** — splitting `Render()` into **CPU prep** vs **GPU record** avoids duplicating `Vk_FrameDrawPrep` in Application.

### Lessons from `vk-core-decomposition`

- Milestone-sized commits with **build + smoke each slice** prevented regressions.
- Deferring ownership peel was explicit; **this plan is that follow-up** — do not re-split draw prep across Gfx/Vk again.

---

## Target architecture

```text
Application
  ├─ Util_EngineConfig (instance — config-platform-hardening)
  ├─ WorldState { SoA, LOD, transforms, id tables, Gfx_SceneDesc, logical path }
  ├─ DebugUIState { panel states, MultiViewState, reload request }
  └─ RunMainLoop:
        sim tick → resolve transforms(WorldState)
        optional: core.PrepareFrameCpu(WorldState, DebugUIState) → FrameCpuPrepResult
        DebugOverlay::BuildPanels(prep stats, DebugUIState)   // no vk*
        core.DrawFrameGpu(FrameCpuPrepResult)                 // acquire → record → present

RenderCore / Vk_Core
  ├─ Vulkan device, swapchain, pipelines, descriptors, tables
  ├─ Vk_FrameDrawPrep (member OK — builds packets from WorldState pointers)
  └─ Vk_SceneHost::LoadCpuState(WorldState&, GpuSession&) — no Gfx headers in .h public API long-term
```

### Context structs — replace friend soup (#8)

**Landing:** Introduce narrow structs; peel modules take `(Vk_DeviceContext&, …)` instead of `friend` + full `Vk_Core&`. Migrate **one module per PR** (order: `Vk_SwapchainHost` → `Vk_FrameUniformUploader` → `Vk_ScenePasses` → caches → `Vk_SceneHost`).

| Struct | Owns (illustrative) |
|--------|---------------------|
| `Vk_DeviceContext` | `VkDevice`, queues, allocator, physical device props/features |
| `Vk_FrameContext` | `myCurrentFrame`, `myFrameDatas`, swapchain extent, fence/slot |
| `Vk_SceneGpuContext` | pipelines, descriptor sets/pool, `Vk_ResourceTables`, material path |

**Done when:** `grep -c "friend class" Vk_Core.h` → 0 (except delete shader test friend or move test to `GfxTests`).

---

## P1 coordination (parallel tracks)

| Track | Order vs peel | Note |
|-------|----------------|------|
| **config-platform-hardening** | **Before Phase 1.2** | `Application` holds `Util_EngineConfig`; avoids re-threading globals twice |
| **shader-bindless-policy** | Anytime; avoid large `Vk_GfxPipelineCache` edits during Phase 4 slices | Policy decision does not block WorldState |
| **render-m2-prep** | **After Phase 1** | M2 touches draw templates; cleaner if `WorldState` already in App |

---

## Phased steps

### Phase 0 — Baseline (no behavior change)

| Step | Landing |
|------|---------|
| 0.1 | Record in `_Progress.md`: friend count, `GetSceneSoA` call sites, `DrawFrame` LOC, `#include` list in `Vk_Core.h` |
| 0.2 | Add `_Progress.md` at kickoff; README **Active now** lists this pair |

**Verify:** `Verify-CI.ps1` exit 0.

---

### Phase 1 — WorldState extraction (#3, #9)

**PR A — type + ownership (behavior unchanged externally)**

| Step | Landing |
|------|---------|
| 1.1 | `App/WorldState.h` (+ `.cpp` if needed): `Gfx_SceneSoA`, `Gfx_LodTable`, `Gfx_LodState`, `Gfx_SceneTransformState`, `Gfx_SceneIdTables`, `Gfx_SceneDesc myLoadedScene`, `std::string myLogicalPath`, `uint32_t myLodDebugLogicalMeshId`; `Clear()` mirrors today’s `UnloadScene` CPU clears |
| 1.2 | `Application` owns `WorldState myWorld`; **config instance** owned here if #7 landed |
| 1.3 | `Vk_Core` holds `WorldState*` or reference set at init/load — temporary bridge; deprecate public getters |

**PR B — call sites**

| Step | Landing |
|------|---------|
| 1.4 | `Application::RunMainLoop`: `Gfx_TickDemoSceneTransforms(myWorld.myTransformState)` + `Gfx_ResolveFlatWorldTransforms(..., myWorld.mySceneSoA)` |
| 1.5 | `Vk_SceneHost::LoadCpuState(WorldState&)` — move SoA/LOD populate out of `aCore.my*` writes |
| 1.6 | `Vk_FrameDrawPrep::Build` — `prepParams.myScene = &world.mySceneSoA` (and LOD pointers); **no** `core.GetSceneSoA()` inside RenderCore |
| 1.7 | `LoadSceneResources` / `UnloadScene`: Application passes `WorldState`; core keeps GPU tables/deletion queues only |
| 1.8 | Remove `GetSceneSoA()` / `GetSceneTransformState()` from `Vk_Core.h` |

**Still on `Vk_Core` after Phase 1 (intentional):** `myCamera`, `myEnvironmentData`, `myResourceTables`, `myDrawPrep`, `myRenderObjects`, GPU objects, `Util_*Panel::State` until Phase 2.

**Done when:**

- `rg "GetSceneSoA|GetSceneTransformState" VulkanDesktop/RenderCore` → 0
- `Vk_Core.h` public section does not `#include` `Gfx_SceneSoA.h` / `Gfx_Lod.h` (forward declare or move includes to `.cpp`)
- `Verify-CI.ps1` + `Verify-Smoke.ps1`; scene reload via panel still works

---

### Phase 2 — Debug UI out of DrawFrame (#6)

**Problem:** `UtilRenderDebugPanel::Build` needs **opaque/transparent draw counts** produced during prep. Moving all `Build` calls blindly to Application duplicates prep.

**PR C — split CPU prep vs GPU record**

| Step | Landing |
|------|---------|
| 2.1 | `App/DebugUIState.h`: `MultiViewState`, `UtilScenePanel::State`, `UtilRenderDebugPanel::State`, `Util_CameraSettings` (if panels stay Util-owned) |
| 2.2 | Introduce `FrameCpuPrepResult` (or reuse struct): `viewPackets`, `activeViewCount`, draw/batch totals, `Util_FrameStats` inputs |
| 2.3 | `Vk_Core::PrepareFrameCpu(const WorldState&, const DebugUIState&, FrameCpuPrepResult&)` — everything currently in `DrawFrame` **before** `vkBeginCommandBuffer` (views, `Vk_FrameDrawPrep`, uniform updates that depend on prep, perf JSONL) |
| 2.4 | `Application`: after sim/resolve, call `PrepareFrameCpu`; then `Util*Panel::Build` using `FrameCpuPrepResult` + `DebugUIState`; then `DrawFrameGpu(prep)` |
| 2.5 | `DrawFrameGpu`: acquire → `RecordScene` → `ImGui::Render` + `RecordImGui` only (no `Util*::Build`, no inline Multi-view window) |
| 2.6 | Scene reload: `Application::TakePendingSceneReload()` reads `DebugUIState` / panel flag — remove `Vk_Core::TakePendingSceneReloadPath` |

**Done when:**

- `rg "ImGui::Begin" VulkanDesktop/RenderCore/Vk_Core.cpp` → 0
- `rg "Util.*Panel::Build" VulkanDesktop/RenderCore` → 0 (panels only App/Util, invoked from App)
- PiP toggle + secondary camera + render debug skips unchanged in smoke

---

### Phase 3 — DrawFrame delegate polish (#3)

| Step | Landing |
|------|---------|
| 3.1 | Move `BuildActiveRenderViews` to Gfx or App helper taking `WorldState` + `DebugUIState` + session camera — core receives resolved `ActiveRenderView[]` |
| 3.2 | Optional: fold `PrepareFrameCpu` + `DrawFrameGpu` into `Vk_PlatformFrame::RunFrame` static orchestrator |
| 3.3 | Track metrics in Progress: LOC, `#include` count, friend count (for Architecture §9 narrative only) |

**Done when:** `DrawFrame`/`DrawFrameGpu` reads as acquire → record → present; no scene JSON or SoA members referenced in `Vk_Core.cpp`.

---

### Phase 4 — `Vk_*Context` migration (#8)

**One module per PR**, each: build + `Verify-CI` + `Verify-Smoke`.

| Order | Module | Friend removed |
|-------|--------|----------------|
| 4.1 | `Vk_SwapchainHost` | ✓ |
| 4.2 | `Vk_FrameUniformUploader` | ✓ |
| 4.3 | `Vk_ScenePasses` | ✓ |
| 4.4 | `Vk_RenderDevice`, `Vk_DescriptorSystem` | ✓ |
| 4.5 | `Vk_GfxPipelineCache`, `Vk_DevicePipelineCache` | ✓ |
| 4.6 | `Vk_SceneHost` | ✓ — last; needs `WorldState` + `Vk_SceneGpuContext` |

Move `VkShaderEffectMeta::RunLitBatchLayoutMismatchValidationTest` to **GfxTests** or pass `Vk_SceneGpuContext` only.

---

## Touch list (expected)

| Area | Files |
|------|--------|
| App | `Application.{h,cpp}`, **new** `WorldState.*`, **new** `DebugUIState.h` |
| RenderCore | `Vk_Core.*`, `Vk_SceneHost.*`, `Vk_FrameDrawPrep.*`, `Vk_PlatformFrame.*`, peeled `Vk_*.cpp` (Phase 4) |
| Gfx | Optional `Gfx_ActiveViews.cpp` if `BuildActiveRenderViews` moves |
| Util | Panel callers only — panel implementations unchanged |
| Docs | `EngineArchitecture.md` §3.1 sequence (**policy**: WorldState owner) — update on Phase 1 close per sync rule |
| Tests | Extend `GfxTests` if pure helpers move (optional) |

---

## Verification (every PR)

| Check | Command / criterion |
|-------|---------------------|
| G0 | `powershell -File Scripts/Verify-CI.ps1` |
| G0-smoke | `powershell -File Scripts/Verify-Smoke.ps1` |
| Multi-view | Manual or smoke log: PiP + secondary camera path unchanged |
| Scene reload | `UtilScenePanel` reload → `Application` path still logs `[SCENE] LoadSceneResources completed` |
| Friends | `rg "friend class" VulkanDesktop/RenderCore/Vk_Core.h` — decreases per Phase 4 slice |
| RenderDoc | F12 capture still works (no regression vs P0) |

---

## Risks & mitigations

| Risk | Mitigation |
|------|------------|
| Single giant PR | **6 PRs minimum:** 1A, 1B, 2C, 3, 4×N modules |
| Prep/panel ordering bug | Phase 2.3 **must** land before moving `UtilRenderDebugPanel::Build` |
| Scene reload breaks | Move panel state in same PR as `TakePendingSceneReload` relocation |
| M2 merge conflicts | Finish Phase 1 before `render-m2-prep` GPU template work |
| False confidence from LOC | Track **getter/friend/include** counts, not LOC alone |

---

## Suggested kickoff checklist

- [x] User confirms kickoff → `vk-core-world-peel_Progress.md`, README Active now
- [x] Phase 0 baseline logged
- [ ] Land config instance (`config-platform-hardening`) before further App wiring
- [x] Phase 1 — WorldState + Application ownership (PR 1A+1B landed in tree)
- [x] Phase 2 — DebugUIState, PrepareFrameCpu / DrawFrameGpu (local)
