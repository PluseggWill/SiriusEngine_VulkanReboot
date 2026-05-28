# Plan: vk-core-decomposition

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Done (2026-05-27)  
**Related:** [`SprintPlan.md`](SprintPlan.md) (open task: `Vk_Core` decomposition), [`EngineArchitecture.md`](EngineArchitecture.md) §3–4, §9, [`scene-load_Plan.md`](scene-load_Plan.md) (Phase D parallel), [`Archived/plans/resource-tables_Plan.md`](Archived/plans/resource-tables_Plan.md)

## Problem

`Vk_Core` (~2400 lines) still owns window/device/swapchain **and** scene CPU state, per-frame draw-stream preparation, and command record/submit. That blocks treating RenderCore as an **RHI-shaped backend**: narrow surface, consumes prepared GPU work, minimal gameplay/scene knowledge.

`Vk_ResourceTables` exists but loads through `friend Vk_Core` and `Vk_Core&` factories. Draw prep (extract → cull → LOD → sort → batch → instance slab) runs inside `DrawFrame` before `vkCmd*`.

## Goals

1. **Three incremental milestones** (M1 → M2 → M3), each build + smoke-verified, **no visual or log-regression** on default `demo.json`.
2. **M1 — Resource tables + RHI context:** `Vk_ResourceContext` holds device/allocator/buffer/image helpers; `Vk_ResourceTables::LoadFromManifest` uses context instead of `friend Vk_Core`.
3. **M2 — Draw-list build:** Gfx CPU path + RenderCore slab upload in dedicated types; **demo Z-spin** moves out of `Vk_Core` (Application hook or `Gfx_DemoSceneSim` in Gfx).
4. **M3 — Record/submit only in `DrawFrame`:** acquire → UBO upload → consume prepared frame → `RecordScenePass` → ImGui → submit/present; no direct `Gfx_ExtractDrawInstances` in `Vk_Core`.
5. **Direction:** Slim `Vk_Core` toward **RHI responsibilities** (device, swapchain, pipelines, descriptors, frame resources, command recording). Scene SoA/LOD stay on `Vk_Core` for this task; ownership peel is a follow-up.

## Non-goals

- Full cross-platform RHI abstraction or removing Vulkan types from RenderCore
- Frame graph, multi-view, moving SoA to Application
- `scene-load` Phase D1–D3 (GPU unload policy, `smoke.json`) — **parallel**, not a blocker; no fake hot-reload in this task
- Eliminating all `Vk_Core::GetInstance()` from `Util_Loader` / `Gfx_Mesh::BuildBuffers` (separate SprintPlan item)
- Shader/descriptor policy or layout changes (`Vk_DescriptorPolicy.h` unchanged unless bugfix)

## Design decisions

| Topic | Decision |
|-------|----------|
| Milestones | M1 → M2 → M3; separate commits preferred per milestone |
| Resource load | `Vk_ResourceContext` passed into `Vk_ResourceTables::LoadFromManifest`; remove `friend class Vk_Core` |
| Context contents (M1) | `VkDevice`, `VmaAllocator`, `Vk_QueueFamilyIndices`, graphics/transfer queues, command pools — factories moved from `Vk_Core` public helpers used at load time |
| Draw prep split | **Gfx:** `Gfx_BuildFrameDrawStream` — extract, cull, LOD, sort, batch (no Vulkan). **RenderCore:** `Vk_FrameDrawPrep` — owns `Gfx_FrameExtract`, batch vectors, calls `FillInstanceSlab` |
| Demo spin (M2) | New Gfx-side helper (e.g. `Gfx_ApplyDemoTransformAnimation`) called from **Application** before `Render()`, or thin `Gfx_DemoSceneSim::Tick(SoA, baseTransforms)`; `Vk_Core` drops `ApplyDemoTransformAnimation` / `ComputeDemoModelMatrix` |
| Prepared frame | `Vk_PreparedFrame` or `Vk_FrameDrawPrep` output: opaque/transparent extract, batch runs, `slabOk`, stats inputs for `Util_FrameStats` |
| `Vk_Core` after M3 | Owns: Vulkan device graph, `myResourceTables`, `myDrawPrep` result cache, `RecordScenePass`, `DrawFrame` orchestration. Does **not** call Gfx extract/cull directly |
| Phase D | Parallel: `UnloadScene` behavior unchanged except any M1 table clear hooks already present |

### RHI-shaped boundary (target end state)

```text
Application
  → (optional) Gfx demo sim / future gameplay
  → Vk_Core::Render
       → Vk_FrameDrawPrep::Build (Gfx_* + FillInstanceSlab)
       → DrawFrame: acquire | UpdateUniformBuffer | record | present
```

`Vk_ResourceContext` is the long-term injection point for loaders (`Gfx_Mesh::BuildBuffers`) — M1 wires tables only; loader singleton peel stays backlog.

## File touch list

| Path | M1 | M2 | M3 |
|------|----|----|-----|
| `RenderCore/Vk_ResourceContext.h` / `.cpp` | Add | — | — |
| `RenderCore/Vk_ResourceTables.h` / `.cpp` | Change API | — | — |
| `RenderCore/Vk_FrameDrawPrep.h` / `.cpp` | — | Add | Refine |
| `Gfx/Gfx_FrameDrawStream.h` / `.cpp` (or extend `Gfx_DrawExtract`) | — | Add | — |
| `Gfx/Gfx_DemoSceneSim.h` / `.cpp` (name TBD at implement) | — | Add | — |
| `App/Application.cpp` | — | Call demo sim | — |
| `RenderCore/Vk_Core.h` / `.cpp` | Slim load path | Remove spin/prep from DrawFrame | DrawFrame record-only surface |
| `VulkanDesktop.vcxproj` / `.filters` | ✓ | ✓ | ✓ |
| `Docs/vk-core-decomposition_Progress.md` | ✓ | ✓ | ✓ |
| `Docs/EngineArchitecture.md` §3.1, §9 | On M3 close | | ✓ |
| `Docs/SprintPlan.md` | Archive line on task close | | |
| `Docs/README.md` | Add plan link | | |

**Untouched (unless compile forces include):** shaders, `Vk_DescriptorPolicy.h`, `Gfx_SceneLoader`, `Util_EngineConfig`

## Implementation plan

### M1 — `Vk_ResourceContext` + tables load peel

- [x] **M1.0** — Progress log + vcxproj entries for new files
- [x] **M1.1** — Add `Vk_ResourceContext` struct/class:
  - Holds device + allocator; `CreateImageView` for texture table load
  - Populated from `Vk_Core::SyncResourceContext` after `InitAllocator`
  - Member `myResourceContext` on `Vk_Core`
- [x] **M1.2** — Change `Vk_ResourceTables::LoadFromManifest( const Gfx_ResourceManifest&, const Vk_ResourceContext&, … )`; remove `friend Vk_Core`
- [x] **M1.3** — `CreateImageView` on context; `Vk_Core::CreateImageView` forwards (load helpers on Core unchanged — loader peel is backlog)
- [x] **M1.4** — `LoadSceneResources` uses context + tables; no behavior change to manifest → GPU tables
- [x] **M1.5** — Build Debug\|x64 + smoke-run; grep no `friend class Vk_Core` on `Vk_ResourceTables`

### M2 — Gfx draw stream + `Vk_FrameDrawPrep` + demo spin out

- [x] **M2.0** — `Gfx_BuildFrameDrawStream` in `Gfx_FrameDrawStream.{h,cpp}` (extract → cull → LOD → sort → batch + one-time `[EXTRACT]`/`[CULL]`/`[BATCH]`/`[TRANSP]`/`[LOD]` logs)
- [x] **M2.1** — `Vk_FrameDrawPrep::Build` calls Gfx builder + `FillInstanceSlab` (moved from `Vk_Core`)
- [x] **M2.2** — `Gfx_TickDemoSceneTransforms` in `Gfx_DemoSceneSim.{h,cpp}`; **Application** calls before `Render()`; `Vk_Core::GetSceneSoA()` / `GetDemoBaseTransforms()`
- [x] **M2.3** — Removed `ApplyDemoTransformAnimation` / `ComputeDemoModelMatrix` / `FillInstanceSlab` from `Vk_Core`; `DrawFrame` uses `myDrawPrep.Build` only
- [x] **M2.4** — Build + smoke; `entities=9 draws=9`; no `Gfx_Extract*` / `FillInstanceSlab` in `Vk_Core.cpp`

### M3 — `DrawFrame` = record/submit + doc close

- [x] **M3.1** — `DrawFrame` sections: sync/acquire → CPU prep (`UpdateUniformBuffer` + `myDrawPrep.Build`) → GPU record → submit/present
- [x] **M3.2** — Grep: no `Gfx_ExtractDrawInstances` / cull / sort / batch in `Vk_Core.cpp` (M2)
- [x] **M3.3** — Module comments on `Vk_Core.{h,cpp}` (RHI scope + delegated modules)
- [x] **M3.4** — `EngineArchitecture.md`, `README.md`, SprintPlan **Archived** `[S2]`
- [x] **M3.5** — Final build + smoke + log tail

## Build / smoke-run

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
& "<MSBuild.exe>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
Set-Location x64\Debug
$p = Start-Process -FilePath ".\VulkanDesktop.exe" -PassThru -WindowStyle Minimized
Start-Sleep -Seconds 4
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Get-Content ..\..\Logs\engine_runtime_log.txt -Tail 60
```

**Success signals (all milestones):**

- MSBuild exit code 0
- No new `[ERROR]` during init through shutdown
- `[EXTRACT]`, `[CULL]`, `[BATCH]` present (first-frame logs)
- `[RESOURCE] FillInstanceSlab` or equivalent
- Default scene: 9 entities / 9 draws (or current demo.json counts)
- M3: `Vk_Core.cpp` line count materially lower than ~2393 (informal target &lt; ~2100)

## Acceptance criteria

| # | Criterion |
|---|-----------|
| A1 | `Vk_ResourceTables` loads without `friend Vk_Core` |
| A2 | Demo spin runs from Application/Gfx, not `Vk_Core` |
| A3 | `DrawFrame` does not call Gfx extract/cull/sort/batch directly |
| A4 | Visual parity on `Data/Scenes/demo.json` (opaque + transparent tree) |
| A5 | Descriptor record path unchanged (Set 0/1/2, instance slab offsets) |
| A6 | Docs: plan complete, progress logged per milestone, SprintPlan archived |

## Risks and rollback

| Risk | Mitigation |
|------|------------|
| `FillInstanceSlab` frame index mismatch | `Vk_FrameDrawPrep::Build` takes explicit in-flight frame index; same `MAX_FRAMES_IN_FLIGHT` |
| Spin moved but SoA not updated before extract | Application order: `DemoSim::Tick` → `Render` → prep uses updated transforms |
| Load helpers duplicated between Core and Context | M1: move only load-path helpers; record path stays on Core until later peel |
| Phase D unload incomplete | Document parallel; M1 must not assume GPU table destroy on `UnloadScene` unless already safe |

**Rollback:** Revert per milestone commit; M2 depends on M1 only for compile hygiene (context can land alone).

## Confirmed landing (Phase 1 — 2026-05-27)

- Three milestones; **Vk_ResourceContext in M1**
- **M2** moves demo spin out of `Vk_Core`
- **scene-load Phase D parallel**
- **Gfx + RenderCore** split for draw-list build
- Maximize `Vk_Core` slimming toward **RHI-shaped** backend

---

## Phase 2 backlog (next sprint slices)

Tracked in `Docs/SprintPlan.md` § **`Vk_Core` decomposition — phase 2**. Code search: `TODO(vk-core-peel)`.

### Phase 2 task ledger (consolidated)

All `Vk_Core decomposition — phase 2` tasks are documented in this plan and `Docs/vk-core-decomposition_Progress.md` only.  
`SprintPlan.md` remains the roadmap checklist; do not create separate `Docs/*_Plan.md` / `Docs/*_Progress.md` files for individual phase 2 slices.

| # | Module | Status | Notes |
|---|--------|--------|-------|
| 1 | `Vk_ResourceContext` v2 | Done (2026-05-28) | Extend context helper surface; remove `GetInstance()` from loader/mesh upload chain |
| 2 | `Vk_RenderDevice` | Done (2026-05-28) | Init orchestration peeled to `Vk_RenderDevice::Init` for instance/device/queues/VMA/command pools |
| 3 | `Vk_SwapchainHost` | Done (2026-05-28) | Swapchain-host init orchestration peeled to `Vk_SwapchainHost::Init` |
| 4 | `Vk_DescriptorSystem` | Done (2026-05-28) | Descriptor/layout/pool/material-set orchestration peeled to `Vk_DescriptorSystem` |
| 5 | `Vk_GfxPipelineCache` | Done (2026-05-28) | Scene pipeline orchestration peeled to `Vk_GfxPipelineCache::InitScenePipelines` |
| 6 | `Vk_ScenePasses` | Done (2026-05-28) | Frame pass record orchestration peeled to `Vk_ScenePasses::RecordFramePasses` |
| 7 | `Vk_FrameUniformUploader` | Done (2026-05-28) | Per-frame UBO upload orchestration peeled to `Vk_FrameUniformUploader::Update` |
| 8 | Scene host (App/Gfx) | Done (2026-05-28) | Scene CPU-state load orchestration peeled to `Vk_SceneHost::LoadCpuState` |
| 9 | `Vk_PlatformFrame` | Done (2026-05-28) | GLFW window and per-frame poll/delta/ImGui orchestration peeled to `Vk_PlatformFrame` |

### Phase 2 #1 completion record (`Vk_ResourceContext` v2)

Date: 2026-05-28

**Problem focus**
- Resource load/upload paths still used `Vk_Core::GetInstance()` in `Util_Loader` and `Gfx_Mesh`, which blocked clean RHI boundary peeling.

**Implementation steps completed**
- [x] Baseline gap check: confirmed remaining singleton usage in texture and mesh upload paths.
- [x] Context v2 surface: extended `Vk_ResourceContext` with load-time helper ops (`CreateBuffer`, `CreateImage`, `TransitionImageLayout`, `CopyBufferToImage`, `CopyBuffer`, `GenerateMipmaps`) and required handles.
- [x] Core bind update: `Vk_Core::SyncResourceContext` now binds full context (device/allocator/queues/pools/queue-family ids/physical device).
- [x] Loader/mesh migration: `UtilLoader::LoadTexture` and `Gfx_Mesh::BuildBuffers` switched to explicit `const Vk_ResourceContext&`.
- [x] Call-site cleanup: `Vk_ResourceTables` passes context through upload chain.

**Verification (passed)**
- Build: `Debug|x64` MSBuild exit code 0.
- Smoke-run: `x64\\Debug\\VulkanDesktop.exe` short run (~4s) exited cleanly.
- Logs: `LoadSceneResources completed`, `RESOURCE-TABLE meshes=8 materials=7 textures=6`, `EXTRACT entities=9 draws=9`, `FillInstanceSlab` wrote 9 instances, no new `[ERROR]` on init path.

**Notes / risks handled**
- A compile break from missing `Vk_Types.h` include in `Vk_ResourceContext.h` was fixed in-task.
- Scope remained limited to context + resource-load chain; no descriptor/pipeline policy changes.

**End state:** `Vk_Core` (or `Vk_RenderBackend`) orchestrates subsystems; no scene semantics, no GLFW, minimal public API.
