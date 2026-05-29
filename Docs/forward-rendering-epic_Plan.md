# Epic Plan: forward-rendering

**Status:** Planned  
**Scope:** Stage 1 of lighting evolution  
**Related:** [`Active-Plan.md`](Active-Plan.md), [`EngineArchitecture.md`](EngineArchitecture.md), [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md)

## Naming conventions

- **Stage:** `Stage 1 (Forward Baseline)`, `Stage 2 (Hybrid Deferred + PBR)`, `Stage 3 (Optional DDGI)`.
- **Preset:** `ForwardLit`, `HybridDeferred`.
- **Pass chain target (Stage 2+):** `GBufferOpaque -> ClusterBuild -> DeferredLighting -> ForwardTransparent -> Post`.

## Goal

Stabilize a complete forward rendering baseline before deferred migration:

1. PBR-ready material/lighting data contracts in CPU and shader interfaces.
2. Reliable opaque + transparent forward path with clear pass boundaries.
3. Feature toggles and verification hooks that make deferred migration low risk.

## Non-goals

- Full deferred or clustered deferred lighting.
- DDGI probe update/integration.
- Replacing GPU-driven and mesh-shader roadmap milestones.

## Deliverables

- Forward path supports production-oriented controls (opaque + transparent).
- Material schema and descriptor contracts support later PBR upgrade without breaking compatibility.
- Render preset includes a stable `ForwardLit` baseline for parity checks.
- Benchmark and capture checklist for baseline visual/perf comparisons.

## Dependency graph

```mermaid
flowchart LR
  STG_FWD[Stage 1 Forward Epic]
  STG_HYB[Stage 2 Hybrid Deferred Epic]
  STG_DDGI[Stage 3 DDGI Epic]

  TASK_A[A. Contracts and data model]
  TASK_B[B. Forward pass hardening]
  TASK_C[C. Validation and migration gates]

  STG_FWD --> TASK_A --> TASK_B --> TASK_C
  TASK_C --> STG_HYB --> STG_DDGI
```

## Work breakdown

### A. Contracts and data model

**Deps:** `S1 M1` draw stream done, `S2` shader reflection/permutation scaffolding in progress.

- [ ] Freeze per-material data layout for forward path (`baseColor`, roughness/metallic factors, alpha mode contract).
- [ ] Keep transparent policy explicit (`opaque` vs `transparent`) and document sorting contract.
- [ ] Define shader feature bits that will carry into deferred (`SHADOWS`, `IBL`, `ALPHA_CLIP`, `PBR`).

### B. Forward pass hardening

**Deps:** A complete; requires current transparent policy from S1 and descriptor contracts (`Set 0/1/2`) locked in docs.

- [ ] Separate forward opaque and forward transparent record flow clearly in pass-level docs.
- [ ] Ensure transparent pass policy stays compatible with later deferred depth consumption.
- [ ] Add debug views/preset switches required for future parity checks.

### C. Validation and migration gates

**Deps:** A and B complete; benchmark/preset infra from `Active-Plan.md` S7 can be reused for parity captures.

- [ ] Capture golden screenshots and perf baseline in one benchmark scene.
- [ ] Add acceptance checklist for deferred migration handoff.
- [ ] Document known gaps that are intentionally deferred to Stage 2.

## Acceptance

- [ ] Default scene renders correctly with forward opaque + transparent policy.
- [ ] Material and permutation contracts are documented and reused by Stage 2 plan.
- [ ] `ForwardLit` baseline can be selected for A/B comparison after deferred path lands.

## Exit criteria for Stage 2

Forward remains a working fallback/reference path while Stage 2 introduces opaque deferred. No hard dependency from gameplay/simulation code into forward-only shader behavior.

