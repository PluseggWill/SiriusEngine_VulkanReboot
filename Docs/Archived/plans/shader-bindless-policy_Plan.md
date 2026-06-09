# Plan: shader-bindless-policy

**Status:** Closed (2026-06-09)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P1** (policy decision) + Wishlist (expansion)  
**Covers recommendations:** #14, #15, #17, #18

## Locked decision

**Option A — Dogfood bindless** (user 2026-06-09). Batch remains mandatory fallback when indexing unavailable, `FORCE_MATERIAL_BATCH`, or explicit test override.

## Maintenance contract *(read before touching Set 1 / record / materials)*

| # | Rule | Why |
|---|------|-----|
| M1 | **Dual-path parity:** batch and bindless must produce the same visible result for the same scene packet (sort order, depth, transparency). | Option A dogfoods bindless; batch is CI fallback and no-indexing hardware. |
| M2 | **Set 1 contract:** bindless changes need `TriangleFrag_Lit_Bindless.frag` + `DescriptorContract_LitBindless.json` + `reflection_lit_bindless.json` + `Vk_Enum.h` / `CreateBindlessMaterialSetLayout` in sync. Batch uses reflection codegen (#18 target). | Hand-written bindless layout drifts until #18. |
| M3 | **Per-draw Set 2:** both paths use `myInstanceDataOffset` into instance slab only — never patch shared frame UBO between draws. | [`EngineArchitecture.md`](EngineArchitecture.md) §6. |
| M4 | **Material table generation:** bindless Set 1 bind once per pass; `materialIndex` in draw/instance must match `Vk_ResourceTables` generation logged at scene load. | Stale table → wrong textures with no crash. |
| M5 | **RenderDoc:** today `renderdoc.dll` forces Batch (`Vk_Bindless.cpp`). Option A implementation must replace with fix or `BINDLESS_RENDERDOC_OK=1` gate — do not delete batch path. | Capture stability vs dogfood. |
| M6 | **Smoke/CI:** `Verify-Smoke.ps1` on RTX should keep `materialPath=Bindless` unless agent lacks indexing. | Regression signal for M1–M4. |
| M7 | **Freeze perm (#17):** no new `.spv` permutations until hybrid pass 2 — even when adding G-buffer shaders. | Shader stack already ahead of features. |
| M8 | **Record unification (#15):** new draw features go through shared semantics keyed by `Vk_RenderMaterialPath`; do not add a third record fork. | Two paths are already the maintenance tax. |

**Grep anchors:** `POLICY_BINDLESS`, `shader-bindless-policy`, `Vk_RenderMaterialPath`.

## Phase 1 — Decision brief *(2026-06-09)*

### Today (code truth)

| Item | State |
|------|--------|
| **Default on RTX + indexing** | `Vk_SelectRenderMaterialPath` → **Bindless** (smoke/CI already exercise this) |
| **RenderDoc injected** | Default **Batch**; `BINDLESS_RENDERDOC_OK=1` keeps bindless when capable |
| **Override** | `FORCE_MATERIAL_BATCH=1` → Batch |
| **Record** | `RecordPassDrawsFromPacket` keyed by `Vk_RenderMaterialPath` |
| **Shaders** | `TrianglePix.spv` (batch Set 1) + `TrianglePix_Bindless.spv` (texture array + material SSBO) |
| **Layouts** | Batch Set 1 from `reflection_lit.json`; bindless Set 1 **hand-written** in `Vk_DescriptorSystem` |
| **Architecture lock** | [`EngineArchitecture.md`](EngineArchitecture.md) §6 — batch fallback **always** supported regardless of A/B |

### What you are deciding

Not “bindless yes/no forever” — **which path is the daily dev default** and how much dual-path maintenance you carry until Stage 2 / S7.

### Option A — Dogfood bindless

**Means:** Capable GPUs use bindless every normal dev run; batch remains fallback for no-indexing hardware and explicit override only. Fix or narrowly gate RenderDoc+bindless instead of silent Batch switch.

| Future impact | |
|---------------|--|
| **P2 M2 prep** | Record-path unification (#15) pays off immediately — one mental model, bindless is “real” |
| **Stage 2 hybrid** | Many materials / G-buffer passes align with bindless Set 1; less rework before FG v0 |
| **S5 mesh shader** | Material tables already exercised; mesh path can assume SSBO table exists |
| **S7 lab** | Preset toggle “batch vs bindless” is a **test matrix**, not a rescue mission |
| **CI / smoke** | Keep running bindless on RTX agents; catches descriptor/table regressions early |
| **Cost** | Maintain two SPIR-V + two record branches until layouts merge (#18); RenderDoc investigation time |

**Implementation sketch:** RenderDoc gate env (`BINDLESS_RENDERDOC_OK` or fix root cause); document dev default; optional `engine.json` `materialPath` preference later; unify record (#15); freeze perm (#17).

### Option B — Defer bindless hot path

**Means:** Strip bindless from runtime init/record/pipelines; dev default = **Batch only**. Keep MSBuild `reflection_lit_bindless.json` + contract verify so SPIR-V/layout contract does not rot offline.

| Future impact | |
|---------------|--|
| **Near term** | Simpler `Vk_ScenePasses`, smaller init, RenderDoc always matches dev path |
| **P2 M2 prep** | CPU indirect + batch record only — less branching while landing M2 |
| **Stage 2** | Re-enable bindless becomes a **scheduled chunk** (likely S7 / material-heavy pass) |
| **S5 / S7** | Risk: bindless bugs rediscovered after months; `#18` layout codegen deferred |
| **CI / smoke** | Would run Batch unless you add a separate bindless-only job |
| **Cost** | `#ifdef` or delete paths — re-merge pain when hybrid needs many materials |

**Implementation sketch:** `Vk_SelectRenderMaterialPath` always Batch; skip `CreateBindlessGfxPipelines` / bindless descriptor init; keep ShaderReflect bindless contract in MSBuild; startup log once.

### Cross-cutting (both options — do regardless of A/B)

| Item | Why |
|------|-----|
| **#17 Freeze perm** | Only `lit` + `lit_alpha_clip` until hybrid pass 2 needs a branch — stops shader stack running ahead of features |
| **#15 Unify record semantics** | A needs it for maintainability; B still benefits if any bindless code remains for verify |
| **#18 Layout codegen** | **A:** do in this epic or next; **B:** Wishlist until re-enable |

### Pick guidance

| Your priority | Pick |
|---------------|------|
| RTX dev box, forward path stable, want Stage 2 bindless-ready | **A** |
| RenderDoc every week, minimal dual-path until hybrid epic | **B** |
| Unsure | **A** on your machine (bindless already green in smoke); gate RenderDoc only |

**Confirmed:** Option **A** (2026-06-09). Vibe Phase 3: RenderDoc gate, #15 record unify, #17 freeze enforcement.

## Problem

Shader stack (reflection, perm registry, disk cache, bindless verify) is **ahead of features** (2 permutations, 0 G-buffer passes). Bindless is **forced off** under RenderDoc, so the path is rarely dogfooded.

## Decision required (pick one — document in plan Progress when implementing)

| Option | Action |
|--------|--------|
| **A — Dogfood** | Fix RenderDoc + bindless stability; default dev config uses bindless when capable; batch = fallback only |
| **B — Defer** | Remove bindless pipeline objects from hot path; keep JSON verify only; shrink `Vk_Bindless` until S5 |

**Recommendation:** Option A if RTX dev machines are primary; Option B if capture stability matters more short-term.

---

## Work breakdown

### Freeze perm/cache expansion (#17)

**Landing:** No new permutations until **second hybrid pass** needs a branch. Registry stays `lit` + `lit_alpha_clip` only; shadow/IBL/PBR bits stay comments, not glslc builds.

Disk cache: keep as-is; no cold/warm benchmark until [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md) JSONL perf exists.

### Unify batch vs bindless record semantics (#15)

**Landing:** One record function keyed by `Vk_RenderMaterialPath`; sort key documents which fields apply. Bindless still respects opaque sort order for pipeline/state coherence.

### Bindless layout codegen (#18)

**Landing:** Extend ShaderReflect → generate bindless Set 1 layout **or** merge batch+bindless into one contract JSON. Eliminate hand-written bindless layout in `Vk_DescriptorSystem`.

### RenderDoc path (#14)

**Landing:** If Option A: investigate capture crash; remove auto Batch fallback or gate behind `BINDLESS_RENDERDOC_OK=1`. If Option B: log once at startup “bindless disabled this session” and delete misleading verify-on-bindless-path tests.

---

## Verification

- Single material path exercised every dev smoke run
- MSBuild reflection validate still green
- No new `.spv` variants without Active-Plan approval

## Non-goals

- PBR permutation
- Descriptor buffers / bindless v2
