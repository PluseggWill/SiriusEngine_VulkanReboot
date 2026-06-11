# Active Plan — SiriusEngine / VulkanDesktop

**Only open `[ ]` tasks** — queue, gates, hardening index.  
**Doc map:** `.cursor/rules/docs-roadmap-arch-sync.mdc` · **History:** [`Archived-Plan.md`](Archived-Plan.md) · **Staged S4+:** [`Wishlist.md`](Wishlist.md) · **Policies:** [`EngineArchitecture.md`](EngineArchitecture.md)

Done → move line to Archived-Plan; no `[x]` here.

---

## Queue

| # | Focus | Plan |
|---|--------|------|
| 1 | **S4** — meshlet geometry (gate **G3**) | [`Wishlist.md`](Wishlist.md) § S4 · [`content-pipeline_Plan.md`](content-pipeline_Plan.md) §A |

---

## Gates

| Gate | Criteria | Unlocks |
|------|----------|---------|
| **G0** ✓ | `Verify-CI.ps1` green | M2 merges |
| **G1** ✓ | CPU vs GPU cull parity *(2026-06-10)* | S3 FG v0 |
| **G2** ✓ | P4 complete *(2026-06-11)* | S8 — [`Wishlist.md`](Wishlist.md) |
| **G3** | [`content-pipeline_Plan.md`](content-pipeline_Plan.md) §A | S4 meshlets |
| **G4** | Stage 2 acceptance | Stage 3 DDGI |

Pass topology diagram: [`EngineArchitecture.md`](EngineArchitecture.md) §7.

---

## Hardening index

| # | Landing | Where | Plan |
|---|---------|-------|------|
| 1 | FG v0 slices 4–6 (transparent, bindless, specular) | S3 | [`Archived/plans/s3-fg-s6-forward-parity_Plan.md`](Archived/plans/s3-fg-s6-forward-parity_Plan.md) |
| 5 | Vertical slice v0 (scene + objective + restart) | P4 | [`Archived/plans/p4-vertical-slice-v0_Plan.md`](Archived/plans/p4-vertical-slice-v0_Plan.md) |
| 18 | Bindless layout codegen | Wishlist | shader-bindless-policy |
| 19 | MeshImport v0 | Wishlist | content-pipeline §A |
| 24 | Material hot reload | Wishlist | content-pipeline §B |
| 26–27 | P0 validation debt | P0 | SprintOutcomeValidation §P0 |
| 29 | Slice = product priority | P4 | archived P4 plan |
| 41 | WSI maintenance1 | Wishlist S7 | vulkan-rhi-hardening §RHI-D |

**#2–17, 20–25, 30–40, 42–43 closed** — see [`Archived-Plan.md`](Archived-Plan.md) and hardening notes in archived plans.

**Bindless maint (still applies):** [`shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance contract before changing scene passes / bindless / lit shaders.

---

## Closed (pointer only)

P0–P4, P1–P2 RHI, S0–S3 → [`Archived-Plan.md`](Archived-Plan.md) · design logs → [`Archived/plans/`](Archived/plans/)
