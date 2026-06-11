# Active Plan — SiriusEngine / VulkanDesktop

**Only open `[ ]` tasks** — queue, gates, hardening index.  
**Doc map:** `.cursor/rules/docs-roadmap-arch-sync.mdc` · **History:** [`Archived-Plan.md`](Archived-Plan.md) · **Staged S4+:** [`Wishlist.md`](Wishlist.md) · **Policies:** [`EngineArchitecture.md`](EngineArchitecture.md)

Done → move line to Archived-Plan; no `[x]` here.

---

## Queue

| # | Focus | Plan |
|---|--------|------|
| 1 | **S3** — hybrid deferred follow-ups | [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §A · FG v0 archived |
| 2 | **P4** — Vertical slice (parallel) | § [P4](#p4--vertical-slice-v0) below |

---

## Gates

| Gate | Criteria | Unlocks |
|------|----------|---------|
| **G0** ✓ | `Verify-CI.ps1` green | M2 merges |
| **G1** ✓ | CPU vs GPU cull parity *(2026-06-10)* | S3 FG v0 |
| **G2** | P4 complete | S8 — [`Wishlist.md`](Wishlist.md) |
| **G3** | [`content-pipeline_Plan.md`](content-pipeline_Plan.md) §A | S4 meshlets |
| **G4** | Stage 2 acceptance | Stage 3 DDGI |

Pass topology diagram: [`EngineArchitecture.md`](EngineArchitecture.md) §7.

---

## S3 — GPU-driven indirect + FG v0

M2 geometry closed via P2–P3 ([`Archived-Plan.md`](Archived-Plan.md)). Validate: [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) §S3.

- [ ] `LNK4098` linker warning *(optional)*

Compaction / MT v1 → [`Wishlist.md`](Wishlist.md) backlog.

---

## P4 — Vertical slice v0

- [ ] Play/benchmark scene + verified assets
- [ ] One objective with win/lose feedback
- [ ] Restart without process exit

---

## Hardening index

| # | Landing | Where | Plan |
|---|---------|-------|------|
| 1 | FG v0 slices 4–6 (transparent, bindless, specular) | S3 | [`Archived/plans/s3-fg-s6-forward-parity_Plan.md`](Archived/plans/s3-fg-s6-forward-parity_Plan.md) |
| 5 | Vertical slice = 3 tasks | P4 | § P4 |
| 18 | Bindless layout codegen | Wishlist | shader-bindless-policy |
| 19 | MeshImport v0 | Wishlist | content-pipeline §A |
| 24 | Material hot reload | Wishlist | content-pipeline §B |
| 26–27 | P0 validation debt | P0 | SprintOutcomeValidation §P0 |
| 29 | Slice = product priority | P4 | § P4 |
| 41 | WSI maintenance1 | Wishlist S7 | vulkan-rhi-hardening §RHI-D |

**#2–17, 20–25, 30–40, 42–43 closed** — see [`Archived-Plan.md`](Archived-Plan.md) and hardening notes in archived plans.

**Bindless maint (still applies):** [`shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance contract before changing scene passes / bindless / lit shaders.

---

## Closed (pointer only)

P0–P3, P1–P2 RHI, S0–S2 → [`Archived-Plan.md`](Archived-Plan.md) · design logs → [`Archived/plans/`](Archived/plans/)
