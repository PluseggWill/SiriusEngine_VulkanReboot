# Sprint Outcome Validation

Close-out runbook for **active** sprints. Queue: [`Active-Plan.md`](Active-Plan.md). History: [`Archived-Plan.md`](Archived-Plan.md).

**Shipped S0–S8 / G4 / P0–P4:** [`Archived/plans/`](Archived/plans/). Day-to-day: `.cursor/rules/vulkan-smoke-test.mdc`.

## How to use

1. Finish or defer tasks explicitly.
2. Run checklist · record evidence · stub → Archived-Plan + archive Plan/Progress.

---

## Sprint index

| Sprint | Theme | Status |
|--------|--------|--------|
| S0–S8, G4, P0–P4 | Lighting + FG + RHI-E4 | **Shipped** |
| **S9 / G5** | Temporal / TAA | **Shipped** |
| **gfx-rhi-ownership** | Retire facades + Init→Gfx (O1–O5) | **Queue #1** |
| **S10** | Content pipeline (**G3** + rich scenes) | After ownership preferred |
| **S11** | GPU particles | Open |
| **S12** | Water | Open |
| **S13** | CSM shadows | Open |
| **S14** | Terrain | Open |
| **S15** | Hair / fur | Open |
| **S16** | Occlusion + compaction | Open |
| **S17–S18** | Meshlets → mesh shader / GPU mesh | After G3 |
| **S19** | Materials + decals | Open |
| **S20** | Volumetrics + cinematic | After G5 |
| **S21** | Lab + RHI (ownership pulled out) | Parallel |
| **P-Sim / P-Slice** | Simulation / slice | Parallel |

**Dev scene:** `sponza.json` until S10; then imported rich scene. **CI smoke:** `stress.json`.

---

## P0 / CI

- `Verify-CI.ps1` · `Verify-Smoke.ps1` · G0-validation for pass work

---

<a id="validation-s9"></a>
## S9 (temporal / G5)

- MV debug viz · TAA reduces aliasing · SSR/AO on shared MV (or exception) · no validation Errors

<a id="validation-s10"></a>
## S10 (content pipeline / G3)

- MeshImport draw-count parity · material hot reload without full unload >5s
- **`bistro_interior.json`** + `engine.bistro.json`: HybridDeferred load success; closeout records `entities` / `draws` / load time / rough VRAM
- Glass / transparent materials in transparent pass; preset camera: SSR/IBL/shadows visually readable; no new validation Errors
- Dogfood path documented for S11+; **CI smoke unchanged** (`stress.json` — Bistro not in `Verify-Smoke`)

<a id="validation-s11"></a>
## S11 (GPU particles)

- Emitter in dogfood/rich scene · soft vs depth · preset on/off · G0-validation clean

<a id="validation-s12"></a>
## S12 (water)

- Refraction + reflection readable · no validation spam

<a id="validation-s13"></a>
## S13 (CSM)

- Cascades near→far stable · debug tint · cost row

<a id="validation-s14"></a>
## S14 (terrain)

- Heightfield + splat · receives CSM · LOD/cull sane

<a id="validation-s15"></a>
## S15 (hair / fur)

- Cards with anisotropic highlight · stable under TAA · shadow receive

<a id="validation-s16"></a>
## S16 (occlusion)

- On/off parity · cull stats · no pop-in on agreed paths

<a id="validation-s17"></a>
## S17 (meshlets)

- ≥1 mesh correct segmentation + bounds viz

<a id="validation-s18"></a>
## S18 (mesh shader + GPU mesh)

- VS parity · `DrawMeshTasksIndirect` · preset fallback matrix

<a id="validation-s19"></a>
## S19 (materials + decals)

- Clearcoat + decal · aniso shared with S15 · base PBR unchanged when off

<a id="validation-s20"></a>
## S20 (volumetrics + cinematic)

- Fog/shafts/DOF/MB/grading toggles · MB uses S9 MVs

<a id="validation-gfx-rhi-ownership"></a>
## gfx-rhi-ownership (queue #1)

- O1: FG no longer owns GBuffer/hybrid Begin/End · smoke + G0-validation
- O2: Rhi create surface covered by GfxTests + Verify-CI
- O3–O5: no HybridDeferred `Vk_*_Record`; pass Init not in `Vk_*Pass.cpp`; Architecture migration note current

<a id="validation-s21"></a>
## S21 (lab + RHI)

- ≥1 lab deliverable · WSI probed or N/A

<a id="validation-p-sim"></a>
## P-Sim

- Sim writes SoA before extract · physics + anim + AI min slice

---

## Evidence checklist

Build + exit · smoke log · visuals when relevant · regression note
