# Plan: shader-bindless-policy

**Status:** Planned  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P1** (policy decision) + Wishlist (expansion)  
**Covers recommendations:** #14, #15, #17, #18

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

Disk cache: keep as-is; no cold/warm benchmark until [`ci-verification_Plan.md`](ci-verification_Plan.md) JSONL perf exists.

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
