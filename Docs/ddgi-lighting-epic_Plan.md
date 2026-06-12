# Epic Plan: ddgi-lighting

**Status:** Planned  
**Scope:** Stage 3 of lighting evolution (optional feature set)  
**Related:** [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md), [`Active-Plan.md`](Active-Plan.md), [`EngineArchitecture.md`](EngineArchitecture.md)

## Naming conventions

- **Stage:** `Stage 1 (Forward Baseline)`, `Stage 2 (Hybrid Deferred + PBR)`, `Stage 3 (Optional DDGI)`.
- **Preset:** `ForwardLit`, `HybridDeferred`.
- **Pass chain base:** `GBufferOpaque -> ClusterBuild -> DeferredLighting -> ForwardTransparent -> Post`.

## Goal

Add DDGI as an optional global illumination layer on top of the hybrid renderer without destabilizing baseline presets.

## Non-goals

- Making DDGI mandatory for all presets/platforms.
- Replacing direct lighting, shadowing, or IBL systems.
- Path-traced GI pipeline.

## Deliverables

- DDGI probe data model and update scheduling policy.
- Frame graph passes for probe update and sampling.
- Integration path in deferred opaque lighting, with optional transparent interaction policy documented.
- Preset-level feature toggle (`DDGI On/Off`) with performance guardrails.

## Dependency graph

```mermaid
flowchart LR
  STG_HYB_DONE[Stage 2 Hybrid Deferred Epic done]
  STG_DDGI[Stage 3 DDGI Epic]

  TASK_A[A. Probe system]
  TASK_B[B. Frame graph integration]
  TASK_C[C. Quality/performance controls]
  TASK_D[D. Stability and rollout]

  STG_HYB_DONE --> STG_DDGI
  STG_DDGI --> TASK_A --> TASK_B --> TASK_C --> TASK_D
```

## Work breakdown

### A. Probe system

**Deps:** Stage 2 hybrid deferred accepted; frame graph and deferred lighting contracts stable.

- [ ] Define probe volume placement, resolution tiers, and scene binding policy.
- [ ] Define probe textures/buffers and versioning for runtime updates.
- [ ] Define update budget model (full update vs staggered update).

### B. Frame graph integration

**Deps:** A complete; requires Stage 2 pass topology (`GBufferOpaque -> DeferredLighting`) in production shape.

- [ ] Add probe update pass(es) and dependencies in frame graph.
- [ ] Add lighting resolve hook to sample DDGI contributions in deferred opaque pass.
- [ ] Document resource barriers/import-export contracts for DDGI history/state.

### C. Quality/performance controls

**Deps:** B complete; benchmark and preset infra from S7 available.

- [ ] Add quality levels and fallback behavior for unsupported/slow hardware.
- [ ] Add debug visualization (probe occupancy/contribution overlays).
- [ ] Add benchmark script/checklist for DDGI on/off deltas.

### D. Stability and rollout

**Deps:** C complete; parity baselines from Stage 1 and Stage 2 retained for regression checks.

- [ ] Keep non-DDGI presets behaviorally unchanged.
- [ ] Define acceptance scenes for interior/exterior validation.
- [ ] Record known artifacts and mitigation policy in docs.

## Acceptance

- [ ] DDGI is selectable per preset and disabled by default unless explicitly chosen.
- [ ] Hybrid deferred remains stable and visually correct with DDGI off.
- [ ] DDGI on-path passes validation and documented perf thresholds on benchmark scenes (Sponza; see [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) §S8 after **G4**).

## Exit criteria

DDGI is treated as an optional lighting enhancement layer with clear operational limits, not a mandatory baseline requirement for engine bring-up.

