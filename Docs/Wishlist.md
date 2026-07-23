# Wishlist — staged backlog (render-first)

**Not the execution queue.** Open `[ ]` only. Queue order: `[Active-Plan.md](Active-Plan.md)`.  
**Doc map:** `.cursor/rules/docs-roadmap-arch-sync.mdc`

**Shipped:** S0–S8, G4, RHI-E4 → `[Archived-Plan.md](Archived-Plan.md)`

**Principle:** **Gfx/Rhi ownership completion first** (Active-Plan #1) → then **S10** content pipeline → complex test scenes; then VFX/env (particles → water → terrain → hair); then scale/geometry. Sim / slice stay **parallel**.

---

## Index
| Sprint              | Theme                        | Deps                               | Status                                  |
| ------------------- | ---------------------------- | ---------------------------------- | --------------------------------------- |
| **S9**              | Temporal (MV + TAA)          | —                                  | **Done → G5** ✓                         |
| **S10**             | **Content pipeline**         | After ownership epic preferred     | Open → **G3** *(after queue #1)*        |
| **S11**             | **GPU particles**            | Depth + FG ✓; S10 scenes preferred | Open                                    |
| **S12**             | **Water**                    | Transparent + SSR ✓; S9 preferred  | Open                                    |
| **S13**             | Cascaded shadows             | S5 ✓                               | Open                                    |
| **S14**             | **Terrain**                  | S13 CSM preferred                  | Open                                    |
| **S15**             | **Hair / fur**               | G-buffer ✓; S9 preferred           | Open                                    |
| **S16**             | Occlusion + compaction       | S6 / S3 ✓                          | Open                                    |
| **S17**             | Meshlets                     | **G3**                             | After S10                               |
| **S18**             | Mesh shader + GPU mesh       | S17                                | Deferred                                |
| **S19**             | Materials + decals           | G-buffer ✓                         | Open                                    |
| **S20**             | Volumetrics + cinematic post | S7 ✓, **G5**                       | Open                                    |
| **S21**             | Render lab + RHI             | Ownership epic pulled to Active #1 | Parallel lab / WSI                      |
| **P-Sim / P-Slice** | Simulation / slice           | **G2** ✓                           | Parallel                                |
| **Backlog**         | Unscheduled                  | —                                  | Parking                                 |
---
## S9 — Temporal foundation

*AAA need: shared MV + TAA. **Gate G5** — closed 2026-07-14.* Plan: [`Archived/plans/temporal_Plan.md`](Archived/plans/temporal_Plan.md).

- [x] `temporal_Plan.md`: Halton jitter, history lifetime, disocclusion.
- [x] G-buffer **motion vectors**; consumers: TAA, AO temporal; SSR keeps hit-world × shared prev VP.
- [x] TAA v0.5 + ImGui/preset toggle; MV debug viz (history-weight view optional/deferred).
- [x] Unify temporal AO / SSR history onto shared lifetime (+ AO on shared MV).

**Acceptance (G5):** [x] Camera motion: aliasing ↓ (TAA); stress smoke green; validation layer N/A on close machine.

---



## S10 — Content pipeline *(moved early — rich test scenes)*

**Plan:** `[content-pipeline_Plan.md](content-pipeline_Plan.md)` · unlocks **G3**.

*Why here:* later particles/water/terrain/hair need multi-mesh interiors/exteriors beyond Sponza/stress.

### A — MeshImport v0 (**G3**)

- [ ] Offline/CLI: glTF (prefer) / OBJ → mesh blob (vb/ib/bounds/LOD).
- [ ] Scene JSON blob id; manifest hash + bounds verify.
- [ ] Reimport demo assets → identical draw counts vs legacy OBJ path.
### B — Material hot reload

- [ ] Material pool recreate without full `UnloadScene`; reload Set 1 only.
### C — Rich scene pack — **Bistro interior** (dogfood for S11+)

*Primary:* [Amazon Lumberyard Bistro](https://developer.nvidia.com/orca/amazon-lumberyard-bistro) (ORCA, CC-BY 4.0). **v0 = interior only**; exterior → Backlog. Depends on §A glTF MeshImport (not Sponza-style OBJ split).

- [ ] `Scripts/Fetch-BistroOrca.ps1` → `Data/Models/bistro/source/` (large assets not in git).
- [ ] Offline FBX → glTF (tool version pinned); MeshImport → blobs; textures downscaled (2k → 1k/512 default).
- [ ] `Scripts/Generate-BistroScene.ps1` → `Data/Scenes/bistro_interior.json` + `Config/engine.bistro.json`.
- [ ] Z-up remap + recenter + camera spawn preset(s); `Data/Models/bistro/CREDITS.md`.
- [ ] glTF PBR → engine materials; glass → transparent pass; import reports alpha stats.
- [ ] Document dogfood path in Active-Plan / README after close. **CI smoke stays** `stress.json`**.**

**Acceptance:** [ ] **G3** met; hot reload works; `bistro_interior.json` loads under HybridDeferred; metrics in closeout; glass readable; dev path documented (not Verify-Smoke).

---



## S11 — GPU particles

*AAA need: GPU emitters. Prefer dogfood on **S10** rich scene.*

**Deps:** Depth + FG ✓. S10 preferred (not hard block).

- [ ] `particles_Plan.md`: SoA buffer, emit/simulate compute, billboard VS/FS.
- [ ] Soft particles vs depth; lit/unlit; additive or sorted v0.
- [ ] Scene JSON emitter; FG after opaque; ImGui debug.
- [ ] Optional: depth collide kill/bounce v0.

**Acceptance:** [ ] ≥1 emitter in rich scene (or Sponza fallback); soft edges; preset on/off; G0-validation clean.

---



## S12 — Water

**Deps:** Transparent + SSR ✓. **S9 preferred**. S10 rich outdoor/courtyard preferred.

- [ ] `water_Plan.md`: plane/mesh; Gerstner or normal scroll v0.
- [ ] Refraction (scene color + depth); absorption; soft shoreline.
- [ ] Reflection: SSR → prefilter; foam v0; sun specular; shadow receive.
- [ ] Scene JSON water flags.

**Acceptance:** [ ] Readable refraction + reflection in dogfood scene; no validation spam.

---



## S13 — Cascaded shadow maps

**Deps:** S5 ✓. **Preferred before S14.**

- [ ] `csm_Plan.md`: 3–4 cascades, splits, matrix upload.
- [ ] Cascade select + PCF/PCSS v0; debug tint; bias controls.

**Acceptance:** [ ] Near + distant coverage on exterior path; cost documented.

---



## S14 — Terrain

**Deps:** **S13 CSM preferred**. Heightmap PNG/RAW OK without MeshImport; S10 helps props on terrain.

- [ ] `terrain_Plan.md`: heightmap; chunk/clipmap LOD; skirts.
- [ ] Splatmap 2–4 layers; receive CSM + IBL/AO.
- [ ] Scene JSON terrain; debug LOD tint; frustum cull chunks (*Hi-Z later S16*).

**Acceptance:** [ ] Outdoor patch + stable cascades; draw cost in notes.

---



## S15 — Hair / fur *(does not wait for S19)*

**Deps:** G-buffer ✓. **S9 preferred**. S10 helps character mesh import.

- [ ] `hair_Plan.md`: **hair cards** v0; denser strands → Backlog.
- [ ] Kajiya-Kay / Marschner-approx; alpha clip / A2C; shadow receive.
- [ ] Demo prop/character in scene JSON; document G-buffer aniso packing for S19.

**Acceptance:** [ ] Readable aniso highlight; stable under TAA; no validation Errors.

---



## S16 — Visibility (Hi-Z occlusion + compaction)

**Deps:** S6 Hi-Z ✓, S3 GPU cull ✓. Benefits from S10 dense scenes.

- [ ] Hi-Z occlusion for instance AABBs; optional GPU compaction.
- [ ] Stats + on/off preset; debug parity vs CPU.

**Acceptance:** [ ] Cull win on rich/stress scene; no false pop-in on agreed paths.

---



## S17 — Meshlets (M3)

**Deps:** **G3** (S10).

- [ ] Meshlet builder + asset format + upload + bounds debug.
- [ ] Optional meshlet LOD — *deps: S1 LOD chains*.

**M3 acceptance:** [ ] ≥1 production mesh correct segmentation.

---



## S18 — Mesh shader + GPU mesh tasks (M4–M5)

**Deps:** S17. Task shader → Backlog.

- [ ] Mesh shader probe + G-buffer/deferred parity vs VS.
- [ ] Meshlet frustum → `vkCmdDrawMeshTasksIndirectEXT`.
- [ ] Preset matrix: `Traditional` / `GpuIndirect` / `MeshShader` / `FullGpuMesh`.

**Acceptance:** [ ] Multi-object GPU-driven primary path; CPU record cost stable.

---



## S19 — Advanced materials + deferred decals

**Deps:** G-buffer ✓. Share aniso with S15; do not duplicate.

- [ ] Clearcoat; transmission / glass; emissive → bloom notes.
- [ ] Finish general-material aniso if S15 was hair-only.
- [ ] Deferred decal volumes; depth clip; debug bounds.

**Acceptance:** [ ] Clearcoat + decal + shared aniso; base PBR unchanged when off.

---



## S20 — Volumetrics + cinematic post

**Deps:** S7 ✓; **G5** for MB; S13 helps shafts.

- [ ] Height/exp fog + sun inscatter; optional volumetric shafts.
- [ ] DOF + motion blur (MV); color grading / auto-exposure v0.

**Acceptance:** [ ] Preset toggles; mood pass without validation spam.

---



## S21 — Render lab + RHI WSI *(parallel)*

**Plan:** [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md) · Records done: [`gfx-rhi-pass-migration_Plan.md`](Archived/plans/gfx-rhi-pass-migration_Plan.md)

**Promoted to Active-Plan #1** (not staged here): FG Begin/End peel · Rhi create · Init→Gfx · delete `Vk_*_Record` / pass Init — see [`gfx-rhi-ownership-completion_Plan.md`](gfx-rhi-ownership-completion_Plan.md).

- [ ] Presets; GPU timestamps; benchmark runbook; screenshots; FG transient RT pool.
- [ ] Bindless layout codegen; troubleshooting; licenses.
- [ ] **RHI-D1–D3** WSI path.
- [ ] MSAA vs TAA matrix; SSR follow-ups; pipeline_binary research.

---



## P-Sim — Simulation *(parallel)*

**Deps:** **G2** ✓.

- [ ] Physics / animation / AI min slice (see prior Wishlist detail).
- [ ] GPU skin align with S18 later.

**Acceptance:** [ ] Dynamic prop + skinned clip + chase agent.

---



## P-Slice — Vertical slice extras *(parallel)*

- [ ] HUD; pause / frame advance; controller / game-state / events.
- [ ] Interact / chase — *deps: P-Sim*.
- [ ] Load-smoke scene; missing-asset warn path.

---



## Backlog

- [ ] Multi-threading v1 → MT v2 with S18.
- [ ] Task shader amplification — *post-S18*.
- [ ] RHI-E1 / E2 / E3 / E5; VSM / RT shadows research.
- [ ] Denser strand hair; ocean FFT; virtual texturing terrain.
- [ ] Bistro exterior full · San Miguel (S10 lands interior only).
- [ ] Editor, networking, non-Windows, navmesh, full BT, audio.

**Parking:** property editor; cross-platform windowing; material graph.
