# Docs — SiriusEngine / VulkanDesktop

## Active now

| Field | Value |
|-------|--------|
| **Task** | **P3** — M2 GPU cull + G1 parity |
| **Recommended next** | GPU indirect record path; G1 CPU vs GPU parity |
| **Plan / Progress** | [`Active-Plan.md`](Active-Plan.md) § P3 · last closeout [`Archived/plans/render-m2-p3-b_Progress.md`](Archived/plans/render-m2-p3-b_Progress.md) |
| **Do not @** | Completed tasks under [`Archived/plans/`](Archived/plans/) unless debugging history |

*Update this table when opening or closing a vibe task.*

### Plan types (agent rules)

| Type | Files | When |
|------|-------|------|
| **Roadmap plan** | `Docs/{topic}_Plan.md` only | Design reference; **multiple** allowed (table below) |
| **Task WIP** | Same `*_Plan.md` + `{topic}_Progress.md` | **One pair max** at Docs root during implementation |

Canonical rules: `.cursor/rules/docs-roadmap-arch-sync.mdc` · workflow: `.cursor/skills/vibe-coding-workflow/SKILL.md`

---

## Roadmap & architecture

| File | Purpose |
|------|---------|
| [`Active-Plan.md`](Active-Plan.md) | **Open P0–P4 `[ ]` only** — queue, gates, hardening index |
| [`EngineArchitecture.md`](EngineArchitecture.md) | **Diagrams + locked policies** — not tasks or changelogs |
| [`Wishlist.md`](Wishlist.md) | **Full S3–S8 + Parallel + Backlog** (staged); promote via Active-Plan gates |
| [`Archived-Plan.md`](Archived-Plan.md) | Completed `[x]` sprint lines |
| [`SprintPlan.md`](SprintPlan.md) | Index → Active / Architecture / Archived |
| [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) | Sprint close-out runbook |

**Split:** tasks → Active-Plan · architecture → EngineArchitecture · doc map → `docs-roadmap-arch-sync.mdc`

### Hardening plans *(reference — start vibe task to implement)*

| Plan | Topic |
|------|--------|
| [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md) | Vulkan RHI audit → WSI, upload, resize (P1–P2; D→S7) |
| [`Archived/plans/render-m2-prep_Plan.md`](Archived/plans/render-m2-prep_Plan.md) | P2 CPU path prep (closed); P3 GPU cull reference |
| [`Archived/plans/shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) | Bindless dogfood (closed) |
| [`content-pipeline_Plan.md`](content-pipeline_Plan.md) | Mesh import, hot reload |

Closed: [`Archived/plans/`](Archived/plans/) (incl. `ci-verification`, `vk-core-world-peel`, `config-platform-hardening`, `swapchain-recreation`).

---

## Onboarding

| File | Purpose |
|------|---------|
| [`bootstrap.md`](bootstrap.md) | Clone, build, smoke, CI scripts |
| [`CLI.md`](CLI.md) | `VulkanDesktop.exe` flags |
| [`Platform.md`](Platform.md) | Windows-only inventory |
