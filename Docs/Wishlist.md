# Wishlist — deferred / unscheduled

**Not open sprint work.** Items here are intentionally **out of `Active-Plan.md`** until explicit [unlock gates](Active-Plan.md#unlock-gates) in Active-Plan are met. Lighting epics remain reference docs; execution resumes only after gates.

---

## Unlock summary

| Gate | Unlocks |
|------|---------|
| **M2 parity** (automated CPU vs GPU cull) | S3 FG v0 spike, [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §A |
| **Vertical slice v0** (objective + restart) | S8 simulation, expanded gameplay hooks |
| **Stage 2 gate** | Stage 3 DDGI — [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) |
| **MeshImport v0** — [`content-pipeline_Plan.md`](content-pipeline_Plan.md) | S4 meshlets (M3) |

---

## Render milestones (S4–S7) — summary only

Full historical checklists lived in pre-2026-06-02 `Active-Plan.md`; restore detail when a gate opens.

| Milestone | Sprint | One-line outcome |
|-----------|--------|------------------|
| **M3** | S4 | Offline meshlets + GPU tables + debug viz |
| **M4** | S5 | Mesh shader path vs VS parity |
| **M5** | S6 | GPU mesh tasks + preset fallbacks |
| **M6** | S7 | Full frame graph, presets, benchmarks, Stage 2/3 acceptance |

**Stage 2 body** (G-buffer, clustered deferred, PBR, parity): primary home **S7** after M2 + FG v0; epic [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md).

---

## Simulation (S8)

Physics → Animation → AI; no Vulkan in sim code. **Deferred** until vertical slice v0 ships. See pre-cut Active-Plan S8 section in git history if needed.

---

## Rendering experiments (post-M6)

MSAA, shadow cascade, IBL upgrade, tonemap, bloom, GPU occlusion, Task shader, multi-threading v2/v3, instance slab dynamic partition (multi-view scale), material hot reload at runtime.

---

## Product / platform parking lot

- In-engine property editor
- Cross-platform windowing / CMake (Windows-only is explicit non-goal until product request — [`config-platform-hardening_Plan.md`](config-platform-hardening_Plan.md))
- Navmesh / full behavior trees
- Editor, networking
- DDGI production tuning (post Stage 3 gate)

---

## Maintenance backlog (non-blocking)

- `LNK4098` linker warning
- Log rotation; domain-split logs
- Third-party license inventory
- Engine overview diagram; “how to add a rendering experiment” checklist
- `VK_KHR_pipeline_binary` research
