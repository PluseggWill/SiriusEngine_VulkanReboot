# Wishlist — staged backlog (S4+)

**Not the execution queue.** Open `[ ]` for **S4–S8**, **Parallel** (beyond P4), and **Backlog** only.

**Active work:** [`Active-Plan.md`](Active-Plan.md) (S3, P4, gates). **Doc map:** `.cursor/rules/docs-roadmap-arch-sync.mdc`

**Promote:** copy `[ ]` lines into Active-Plan when a gate opens. **Ship:** move `[x]` to [`Archived-Plan.md`](Archived-Plan.md) — never duplicate done items here.

---

## Index

| Sprint | Milestone | Tasks |
|--------|-----------|--------|
| **S3** | M2 + FG v0 | → [**Active-Plan §S3**](Active-Plan.md#s3--gpu-driven-indirect--fg-v0) |
| **S4** | M3 meshlets | [below](#s4--meshlet-geometry-milestone-m3) · gate **G3** |
| **S5** | M4 mesh shader | [below](#s5--mesh-shader-pipeline-milestone-m4) |
| **S6** | M5 GPU mesh tasks | [below](#s6--gpu-driven-mesh-tasks-milestone-m5) |
| **S7** | M6 render lab + Stage 2/3 body | [below](#s7--rendering-lab--hardening-milestone-m6) |
| **S8** | Simulation | [below](#s8--simulation-physics--animation--ai) · gate **G2** |
| **Parallel** | Full slice | [below](#parallel--vertical-slice) · minimal subset in Active-Plan P4 |

**S0–S2, P0–P3** → [`Archived-Plan.md`](Archived-Plan.md).

**Lighting epics:** Stage 2 [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) · Stage 3 [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md). FG v0 spike = Active-Plan S3; full FG = S7.

**Bindless contract:** [`shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance · [`EngineArchitecture.md`](EngineArchitecture.md) §6.

---

## S4 — Meshlet geometry (milestone M3)

**Validation:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) (S4 section) · **Gate G3** (MeshImport v0)

### Open tasks

- [ ] Choose meshlet builder (e.g. meshoptimizer) + documented cluster params.
- [ ] Asset format: meshlet table + vertex/index views + per-meshlet bounds (import or offline step).
- [ ] Optional **meshlet LOD** cluster rules documented — *deps: S1 LOD asset chains*.
- [ ] Upload global vertex/index + meshlet metadata buffers.
- [ ] Debug draw: meshlet bounds (VS or compute viz) on test mesh.

### M3 acceptance

- [ ] At least one production mesh displays correct meshlet segmentation.

---

## S5 — Mesh shader pipeline (milestone M4)

*Vulkan 1.2 + `VK_EXT_mesh_shader`; no Task shader in v1.*

**Validation:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) (S5 section)

### Open tasks

- [ ] Device capability probe: mesh shader features; log + graceful disable.
- [ ] Enable extensions; mesh + fragment layout aligned with bindless / material tables (S1).
- [ ] Shaders: `Mesh` (+ adapt `TriangleFrag_Lit.frag`) → `Shader_Generated/`; `materialIndex` from tables.
- [ ] `vkCreateGraphicsPipeline` mesh stages; payload reads meshlet + instance from SSBO.
- [ ] RenderDoc / validation capture checklist in docs.

### M4 acceptance

- [ ] Single-object mesh-shader path matches VS path for geometry/pass-contract parity (forward + hybrid G-buffer within agreed tolerance).

---

## S6 — GPU-driven mesh tasks (milestone M5)

**Validation:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) (S6 section)

### Open tasks

- [ ] Compute: meshlet frustum cull (+ optional backface cone later).
- [ ] Compact visible meshlet list → indirect mesh-task buffer.
- [ ] `vkCmdDrawMeshTasksIndirectEXT`; mesh shader consumes compact list + instance table.
- [ ] **Fallback preset:** S3 VS + indirect when mesh shader unsupported; bindless-off → S1 batch path.
- [ ] Preset enum: `Traditional` / `GpuIndirect` / `MeshShader` / `FullGpuMesh`.
- [ ] **Multi-threading v2:** job system parallel cull/LOD/transform — *deps: MT v1, S1 LOD v0*.

### M5 acceptance

- [ ] Multi-object scene; primary submission GPU-driven; CPU record stable across instance count.

---

## S7 — Rendering lab & hardening (milestone M6)

*Frame graph (full), presets, benchmarks. **Lighting Stage 2** main body + Stage 2/3 gates.*

**Validation:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) (S7 section)

### Open tasks — Lighting acceptance gates

- [ ] **Lighting Stage 2 gate:** `GBufferOpaque + DeferredLighting` (clustered) opaque, `ForwardTransparent`, full PBR, `ForwardLit`/`HybridDeferred` parity — [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md).
- [ ] **Lighting Stage 3 gate:** DDGI preset on/off parity, fallback, benchmark deltas — [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) — *after G4*.

### Open tasks — frame graph

- [ ] `framegraph_Plan.md`: pass/resource nodes, transient RT pool, import/export rules.
- [ ] `FrameGraphBuilder`: topological sort + barriers; hybrid chain + `ForwardLit` baseline.
- [ ] **Transparent pass** as FG node (reads depth).
- [ ] Preset toggles FG topology (shadow/post) without breaking sort keys.

### Open tasks — infrastructure

- [ ] Presets `Low / Base / High / Custom` + permutation subset (S2 registry).
- [ ] GPU timestamp queries + CPU p50/p95 logging.
- [ ] Standard benchmark procedure (scene, camera path, warmup, CSV/JSON).
- [ ] Screenshot capture keyed to preset + pose.
- [ ] RenderDoc expectations per preset; preset changelog.
- [ ] Benchmark: cold vs warm pipeline cache load (S2 cache, done).
- [ ] Shader reflection-driven **layout codegen** — follow-up to closed 2b JSON path.
- [ ] `VK_KHR_pipeline_binary` disk cache research.
- [ ] **[Multi-view] Instance slab dynamic partition:** per-frame pre-count + prefix-sum offsets.

### Open tasks — Vulkan RHI production WSI *(epic §Track D)*

**Plan:** [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md)

- [ ] **RHI-D1** — `Vk_ProbeWsiCaps`; `VK_EXT_swapchain_maintenance1` when available
- [ ] **RHI-D2** — Present history + deferred old-swapchain destroy
- [ ] **RHI-D3** — Remove `vkDeviceWaitIdle` from hot swapchain recreate path

### Open tasks — feature experiments

- [ ] MSAA vs post AA vs none.
- [ ] Shadow map (single cascade).
- [ ] IBL / environment upgrade.
- [ ] Tonemap / exposure modes.
- [ ] Bloom (optional).
- [ ] Validation-friendly toggles; graceful GPU feature degradation.
- [ ] GPU occlusion / hierarchical Z — *post-M5*.
- [ ] **Task shader** for mesh amplification — *post-M5*.

### Open tasks — documentation

- [ ] Engine overview diagram (modules + data flow).
- [ ] “How to add a rendering experiment” checklist.
- [ ] Troubleshooting matrix (seed: `Docs/Archived/notes-2026-05-22-shader-debug.md`).
- [ ] Third-party / SDK license inventory.
- [ ] Log rotation; domain-split logs; crash summary on failure.
- [ ] DDGI production tuning after **Lighting Stage 3** acceptance.

### M6 acceptance

- [ ] Frame graph drives hybrid path + at least one extra pass (shadow or tonemap) on benchmark scene.
- [ ] Two **RenderView**s or FG multi-target in runbook; `ForwardLit`/`HybridDeferred` switch without validation errors.

---

## S8 — Simulation (Physics → Animation → AI)

*Gate **G2** (P4). Does not block S3–S6.*

**Validation:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) (S8 section)

### Open tasks — physics

- [ ] `physics_Plan.md`: library choice + collision layers.
- [ ] `PhysicsWorld::SimStep(fixed_dt)`; entity ↔ body mapping; no Vulkan in sim code.
- [ ] Write back SoA: `transform`, `bounds`.
- [ ] Scene JSON physics components; debug draw AABB.

### Open tasks — animation

- [ ] Skeleton import + clip playback v0.
- [ ] `AnimationSystem` before Extract: skin matrices → deform or CPU skinned path.
- [ ] Plan GPU skinning alignment with S5 (non-blocking for v0).

### Open tasks — AI

- [ ] Agent SoA columns: state, target, perception radius.
- [ ] v0 state machine or minimal BT (Idle / Chase / Flee); one enemy uses player position.
- [ ] Debug: ImGui agent state; optional tie to Parallel objective.

### S8 acceptance

- [ ] Dynamic props (physics); one skinned clip; one agent chases player in play scene.
- [ ] **Multi-threading v3 (optional):** render thread + command stream — *deps: S7 FG*.

---

## Parallel — Vertical slice

*Full scope. Minimal subset → Active-Plan **P4**.*

### Open tasks — scene & content

- [ ] Primary play/benchmark scene in `Data/`. *(P4)*
- [ ] All referenced assets present or substitute with logged warnings.
- [ ] Optional second tiny scene for load smoke tests.

### Open tasks — gameplay

- [ ] One **objective** (reach marker / collect / survive / toggle lights). *(P4)*
- [ ] Win/lose or completion feedback (HUD or log). *(P4)*
- [ ] **Restart** without process exit. *(P4)*

### Open tasks — presentation

- [ ] HUD: FPS, frame time, **active render preset** name.
- [ ] Pause + frame advance (dev).

### Open tasks — engine hooks

- [ ] Player controller contract (move, look, interact).
- [ ] Simple game state / mode stack (Play, Pause, Dev overlay).
- [ ] Event channel gameplay ↔ UI ↔ debug.

### Open tasks — simulation hooks *(tie to S8)*

- [ ] Interact / damage via physics overlap or ray — *deps: S8 Physics*.
- [ ] Enemy chase via **S8 AI** — *deps: S8 AI, Parallel objective*.

---

## Backlog (deferred / unscheduled)

- [ ] Optional GPU compaction pass (deferred from S3 M2).
- [ ] **Multi-threading v1:** thread model + frame SoA double-buffer.
- [ ] Editor, networking, non-Windows.

### Parking lot

- In-engine property editor (post slice).
- Cross-platform windowing / CMake — [`config-platform-hardening`](Archived/plans/config-platform-hardening_Plan.md) (archived).
- Navmesh / full behavior trees (post S8 AI v0).
- Material hot reload — [`content-pipeline_Plan.md`](content-pipeline_Plan.md) § B.
- MeshImport v0 — [`content-pipeline_Plan.md`](content-pipeline_Plan.md) § A (gate **G3**).

### Vulkan RHI — long-term *(epic §Track E)*

- [ ] **RHI-E1** — Instance/device `apiVersion` 1.2+
- [ ] **RHI-E2** — Timeline semaphores — *deps: S7 frame graph*
- [ ] **RHI-E3** — Dynamic rendering spike (`VK_KHR_dynamic_rendering`)
- [ ] **RHI-E4** — Injectable render device / de-singleton `Vk_Core` for headless CI
- [ ] **RHI-E5** — Dynamic MSAA sample count or remove dead MSAA branches
