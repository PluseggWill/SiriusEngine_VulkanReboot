# Plan: render-m2-prep

**Status:** Closed (2026-06-09)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P2 → P3**  
**Covers recommendations:** #1, #11, #12, #13, #21, #23

## Goal

Make the **CPU draw path** use the same buffers and record pattern that M2 GPU cull will consume — so M2 adds a compute pass, not a second parallel renderer.

## Non-goals

- FG v0 / G-buffer (#1 explicitly deferred until M2 parity automated — see Active-Plan gates)
- GPU meshlets / mesh shader

---

## Work breakdown

### A. Draw template + instance SSBO (#13)

**Landing:** Each logical draw has a **CPU/GPU-shared template**:

```text
indexCount, firstIndex, vertexOffset, instanceCount, firstInstance
+ instanceId → slab offset / material / mesh ids
```

| Step | Landing |
|------|---------|
| A.1 | `Gfx_DrawTemplate` struct; fill during extract/batch |
| A.2 | Upload templates to SSBO each frame (or static per mesh + instance buffer) |
| A.3 | Record: **`vkCmdDrawIndexedIndirect`** with **one** indirect buffer built on CPU first (same layout as future GPU output) |

**Done when:** CPU path issues indirect draw(s); per-draw `vkCmdDrawIndexed` loop gone from hot path.

### B. Index count on GPU mesh (#12)

**Landing:** `Gfx_Mesh` stores `myIndexCount` at buffer build; record uses `mesh.myIndexCount`, not `myIndices.size()`.

### C. Record path hygiene (#11)

**Landing:** RenderDoc labels via fixed `char[128]` scratch or `#ifdef VKDESKTOP_RENDERDOC_TAGS`; **no per-draw `std::string`** in default builds.

### D. Demo sim cleanup (#10 — coordinated with config)

**Landing:**

- Default `engine.json`: **`demoRotate: false`**
- Rename or narrow `Gfx_DemoSceneSim` → explicit opt-in scene component or debug flag only
- Reset sim time on scene reload

### E. LOD default off (#21)

**Landing:** `engine.json` feature `lodEnabled: false` default; demo scene uses single LOD level unless stress scene loaded. ImGui toggle for dev.

### F. Cull/sort correctness (#23)

**Landing:**

- Tighter world AABB on transform update (mesh local bounds × world matrix)
- Opaque `depthBucket` from **bounds center** eye-space Z (document in sort key)
- Add CPU unit test: rotating entity → transparent order stable vs reference

---

## M2 proper (P3 — after A–C)

Moved to Active-Plan **P3**; depends on draw template SSBO:

- Compute frustum cull → visible indices → fill indirect buffer
- Parity test vs CPU cull (automated — [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md))
- Optional compaction pass

**FG v0:** gate until parity job green.

---

## Verification

- Demo scene: indirect CPU path, same visual as today
- `[PERF]` drawCalls unchanged; record LOC down
- Unit tests for sort/cull from ci-verification plan

## Risks

- Indirect draw debugging harder → keep `--legacy-direct-draw` fallback flag one sprint
