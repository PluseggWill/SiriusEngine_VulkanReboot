# Wishlist — staged backlog (S4+)

**Not the execution queue.** Open `[ ]` for **S4–S13**, **Parallel**, **Geometry track**, and **Backlog** only.

**Active work:** [`Active-Plan.md`](Active-Plan.md) (queue + gates). **Doc map:** `.cursor/rules/docs-roadmap-arch-sync.mdc`

**Promote:** copy `[ ]` lines into Active-Plan when a gate opens. **Ship:** move `[x]` to [`Archived-Plan.md`](Archived-Plan.md) — never duplicate done items here.

**Pivot:** Lighting / image-quality **S4–S8** is the active track. **Meshlet / mesh shader / GPU mesh** moved to [**§ Geometry track (S10–S12)**](#geometry-track--meshlet--mesh-shader-deferred) — gate **G3** applies to S10 only.

---

## Index

| Sprint | Milestone | Theme | Tasks |
|--------|-----------|--------|--------|
| **S3** | M2 + FG v0 | GPU indirect + hybrid shell | → [`Archived-Plan.md`](Archived-Plan.md) |
| **S4** | Lighting-1 | PBR + G-buffer contract | → [`Archived-Plan.md`](Archived-Plan.md) |
| **S5** | Lighting-2 | IBL, skybox, shadows | [below](#s5--environment--shadows-lighting-2) |
| **S6** | Lighting-3 | SSAO + Hi-Z | [below](#s6--screen-space--hi-z-lighting-3) |
| **S7** | Lighting-4 | Post + frame graph v1 | [below](#s7--post-processing--frame-graph-v1-lighting-4) |
| **S8** | Lighting-5 | DDGI / GI (Stage 3) | [below](#s8--global-illumination-ddgi--stage-3) · gate **G4** |
| **S9** | Simulation | Physics / anim / AI | [below](#s9--simulation-physics--animation--ai) · gate **G2** ✓ |
| **S10–S12** | M3–M5 | Geometry (deferred) | [Geometry track](#geometry-track--meshlet--mesh-shader-deferred) |
| **S13** | M6 infra | Render lab + RHI | [below](#s13--render-lab-infrastructure-deferred) |
| **Parallel** | Full slice | Vertical slice extras | [below](#parallel--vertical-slice) |

**Lighting epics:** Stage 2 [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) (S4–S7) · Stage 3 [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) (S8).

**Bindless contract:** [`shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance · [`EngineArchitecture.md`](EngineArchitecture.md) §6.

---

## S4 — PBR + G-buffer contract (Lighting-1) *(shipped — see Archived-Plan)*

Closed 2026-06-12 · Plan: [`Archived/plans/s4-pbr-gbuffer_Plan.md`](Archived/plans/s4-pbr-gbuffer_Plan.md)

---

## S5 — Environment + shadows (Lighting-2)

*Deps: **S4** (G-buffer + PBR BRDF).*

### Open tasks

- [ ] Skybox / background: cubemap pass or skydome draw (depth at far plane).
- [ ] IBL: environment cubemap + diffuse irradiance + specular prefilter (or split-sum approximation).
- [ ] Wire IBL into deferred lighting + transparent forward.
- [ ] Directional shadow map v0 (single cascade); stable ortho frustum from sun direction.
- [ ] Shadow map sample in deferred lighting (PCF v0).
- [ ] ImGui / config toggles: shadows on/off, IBL intensity.

### Acceptance

- [ ] Sponza: sun shadow on ground/arch; sky visible at openings; IBL fills interiors without blowing exposure.

---

## S6 — Screen-space + Hi-Z (Lighting-3)

*Deps: **S4** (depth/normal). **S5** recommended (shadowed AO comparison).*

### Open tasks

- [ ] Depth pyramid (Hi-Z) from G-buffer or resolve depth — document mip policy.
- [ ] SSAO (or GTAO v0) pass; composite into deferred lighting or pre-tonemap buffer.
- [ ] Optional: normal-aware radius; temporal stability = backlog.
- [ ] Debug viz: AO only, Hi-Z mip slice (dev overlay).

### Acceptance

- [ ] Contact shadows / crevice darkening visible on Sponza; Hi-Z texture valid in capture.

---

## S7 — Post-processing + frame graph v1 (Lighting-4)

*Deps: **S4–S6**. Contributes to **G4** Stage 2 acceptance.*

**Epic:** [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §A.

### Open tasks — frame graph

- [ ] `framegraph_Plan.md` (new): pass/resource nodes, transient RT pool, import/export rules.
- [ ] `FrameGraphBuilder`: topological sort + barriers; hybrid chain + shadow + AO + post nodes.
- [ ] Preset toggles FG topology (shadow / AO / bloom) without breaking sort keys.

### Open tasks — post

- [ ] HDR intermediate target (if not already); tonemap + exposure (ImGui tunable).
- [ ] Optional bloom (threshold + blur + composite).
- [ ] Validation-friendly toggles per pass.

### Open tasks — lab

- [ ] Presets `Low / Base / High / Custom` + permutation subset (S2 registry).
- [ ] GPU timestamp queries + CPU p50/p95 logging.
- [ ] Benchmark runbook: Sponza, fixed camera, warmup, CSV/JSON.
- [ ] Screenshot capture keyed to preset + pose.
- [ ] RenderDoc expectations per preset documented.

### Acceptance (G4 contributor)

- [ ] Frame graph drives hybrid + at least **shadow** and **one post pass** on Sponza.
- [ ] `ForwardLit` ↔ `HybridDeferred` switch validation-clean.

---

## S8 — Global illumination / DDGI (Stage 3)

*Deps: **G4** (Stage 2 accepted). **After S7** FG hooks stable.*

**Epic:** [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md).

### Open tasks

- [ ] Probe volume data model + update budget (full vs staggered).
- [ ] FG passes: probe update + sample hook in deferred lighting.
- [ ] Preset `DDGI On/Off` + performance guardrails.
- [ ] Debug viz: probe contribution overlay.
- [ ] Benchmark script: DDGI on/off deltas on interior scene (Sponza + optional San Miguel later).

### Acceptance

- [ ] DDGI preset improves interior bounce vs S7 baseline; non-DDGI presets unchanged.

---

## S9 — Simulation (Physics → Animation → AI)

*Gate **G2** ✓ (P4). **Does not block S4–S8**.*

**Validation:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) §S9.

### Open tasks — physics

- [ ] `physics_Plan.md`: library choice + collision layers.
- [ ] `PhysicsWorld::SimStep(fixed_dt)`; entity ↔ body mapping; no Vulkan in sim code.
- [ ] Write back SoA: `transform`, `bounds`.
- [ ] Scene JSON physics components; debug draw AABB.

### Open tasks — animation

- [ ] Skeleton import + clip playback v0.
- [ ] `AnimationSystem` before Extract: skin matrices → deform or CPU skinned path.
- [ ] Plan GPU skinning alignment with geometry track (non-blocking for v0).

### Open tasks — AI

- [ ] Agent SoA columns: state, target, perception radius.
- [ ] v0 state machine or minimal BT (Idle / Chase / Flee); one enemy uses player position.
- [ ] Debug: ImGui agent state; optional tie to Parallel objective.

### S9 acceptance

- [ ] Dynamic props (physics); one skinned clip; one agent chases player in play scene.

---

## Geometry track — Meshlet / mesh shader (deferred)

*Former Wishlist **S4–S6**. Promote **S10** only when lighting track **G4** met or explicitly parallelized.*

| Sprint | Was | Gate | Validation |
|--------|-----|------|------------|
| **S10** | S4 meshlets | **G3** MeshImport v0 | [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) §S10 |
| **S11** | S5 mesh shader | S10 | §S11 |
| **S12** | S6 GPU mesh tasks | S11 | §S12 |

### S10 — Meshlet geometry (M3)

- [ ] Choose meshlet builder (e.g. meshoptimizer) + documented cluster params.
- [ ] Asset format: meshlet table + vertex/index views + per-meshlet bounds (import or offline step).
- [ ] Optional **meshlet LOD** cluster rules — *deps: S1 LOD asset chains*.
- [ ] Upload global vertex/index + meshlet metadata buffers.
- [ ] Debug draw: meshlet bounds on test mesh.

**M3 acceptance:** [ ] At least one production mesh displays correct meshlet segmentation.

### S11 — Mesh shader pipeline (M4)

*Vulkan 1.2 + `VK_EXT_mesh_shader`; no Task shader in v1.*

- [ ] Device capability probe; log + graceful disable.
- [ ] Extensions; mesh + fragment layout aligned with bindless / material tables.
- [ ] Shaders: Mesh + adapt lit/G-buffer frags; `materialIndex` from tables.
- [ ] `vkCreateGraphicsPipeline` mesh stages; payload reads meshlet + instance from SSBO.
- [ ] Parity: mesh path matches VS path for **G-buffer + deferred** (not forward-only).

**M4 acceptance:** [ ] Single-object mesh-shader path matches VS hybrid parity.

### S12 — GPU-driven mesh tasks (M5)

- [ ] Compute: meshlet frustum cull (+ optional backface cone later).
- [ ] Compact visible meshlet list → indirect mesh-task buffer.
- [ ] `vkCmdDrawMeshTasksIndirectEXT`; mesh shader consumes compact list.
- [ ] Fallback preset matrix: `Traditional` / `GpuIndirect` / `MeshShader` / `FullGpuMesh`.
- [ ] **Multi-threading v2:** parallel cull/LOD/transform — *deps: MT v1*.

**M5 acceptance:** [ ] Multi-object scene; primary submission GPU-driven; CPU record stable across instance count.

---

## S13 — Render lab infrastructure (deferred)

*Remainder of old **S7** infra not absorbed into S4–S7. Does not block G4.*

### Open tasks — Vulkan RHI WSI

**Plan:** [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md)

- [ ] **RHI-D1** — `Vk_ProbeWsiCaps`; `VK_EXT_swapchain_maintenance1` when available
- [ ] **RHI-D2** — Present history + deferred old-swapchain destroy
- [ ] **RHI-D3** — Remove `vkDeviceWaitIdle` from hot swapchain recreate path

### Open tasks — tooling & docs

- [ ] Shader reflection-driven **layout codegen** — follow-up to closed 2b JSON path.
- [ ] `VK_KHR_pipeline_binary` disk cache research.
- [ ] **[Multi-view] Instance slab dynamic partition:** per-frame pre-count + prefix-sum offsets.
- [ ] Engine overview diagram (modules + data flow).
- [ ] “How to add a rendering experiment” checklist.
- [ ] Troubleshooting matrix (seed: `Docs/Archived/notes-2026-05-22-shader-debug.md`).
- [ ] Third-party / SDK license inventory.
- [ ] Log rotation; domain-split logs; crash summary on failure.

### Open tasks — experiments (backlog-friendly)

- [ ] MSAA vs post AA vs none.
- [ ] GPU occlusion cull using Hi-Z — *deps: S6*.
- [ ] **Task shader** for mesh amplification — *post-S12*.
- [ ] SSR, volumetrics, decals — backlog.

---

## Parallel — Vertical slice

*Full scope. Minimal subset → Archived P4.*

### Open tasks — scene & content

- [ ] Primary play/benchmark scene in `Data/`. *(P4 ✓ partial — Sponza default)*
- [ ] All referenced assets present or substitute with logged warnings.
- [ ] Optional second tiny scene for load smoke tests.

### Open tasks — gameplay

- [ ] One **objective** (reach marker / collect / survive / toggle lights). *(P4 ✓)*
- [ ] Win/lose or completion feedback (HUD or log). *(P4 ✓)*
- [ ] **Restart** without process exit. *(P4 ✓)*

### Open tasks — presentation

- [ ] HUD: FPS, frame time, **active render preset** name.
- [ ] Pause + frame advance (dev).

### Open tasks — engine hooks

- [ ] Player controller contract (move, look, interact).
- [ ] Simple game state / mode stack (Play, Pause, Dev overlay).
- [ ] Event channel gameplay ↔ UI ↔ debug.

### Open tasks — simulation hooks

- [ ] Interact / damage via physics overlap or ray — *deps: S9 Physics*.
- [ ] Enemy chase via **S9 AI** — *deps: S9 AI, Parallel objective*.

---

## Backlog (deferred / unscheduled)

- [ ] Optional GPU compaction pass (deferred from S3 M2).
- [ ] **Multi-threading v1:** thread model + frame SoA double-buffer.
- [ ] Editor, networking, non-Windows.
- [ ] San Miguel / Bistro benchmark scenes — *deps: MeshImport optional*.

### Parking lot

- In-engine property editor (post slice).
- Cross-platform windowing / CMake — [`config-platform-hardening`](Archived/plans/config-platform-hardening_Plan.md).
- Navmesh / full behavior trees (post S9 AI v0).
- Material hot reload — [`content-pipeline_Plan.md`](content-pipeline_Plan.md) §B.
- MeshImport v0 — [`content-pipeline_Plan.md`](content-pipeline_Plan.md) §A (gate **G3** → S10).

### Vulkan RHI — long-term

- [ ] **RHI-E1** — Instance/device `apiVersion` 1.2+
- [ ] **RHI-E2** — Timeline semaphores — *deps: S7 frame graph*
- [ ] **RHI-E3** — Dynamic rendering spike (`VK_KHR_dynamic_rendering`)
- [ ] **RHI-E4** — Injectable render device / de-singleton `Vk_Core` for headless CI
- [ ] **RHI-E5** — Dynamic MSAA sample count or remove dead MSAA branches
