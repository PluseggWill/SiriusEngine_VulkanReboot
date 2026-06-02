# Docs — SiriusEngine / VulkanDesktop

## Active now

| Field | Value |
|-------|--------|
| **Task** | *(none — open a vibe task to start next WIP pair)* |
| **Recommended next** | **P1** — [`config-platform-hardening_Plan.md`](config-platform-hardening_Plan.md) or [`shader-bindless-policy_Plan.md`](shader-bindless-policy_Plan.md) per [`Active-Plan.md`](Active-Plan.md) |
| **Plan / Progress** | — |
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
| [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md) | **P0 done** — G0 CI, GfxTests, smoke, perf JSONL |
| [`Archived/plans/vk-core-world-peel_Plan.md`](Archived/plans/vk-core-world-peel_Plan.md) | **P1 peel done** — WorldState, ImGui, `Vk_*Context` |
| [`render-m2-prep_Plan.md`](render-m2-prep_Plan.md) | CPU indirect, record hygiene, M2 prep |
| [`shader-bindless-policy_Plan.md`](shader-bindless-policy_Plan.md) | Bindless decision, perm freeze |
| [`config-platform-hardening_Plan.md`](config-platform-hardening_Plan.md) | Config instance, VK recover, Windows scope |
| [`content-pipeline_Plan.md`](content-pipeline_Plan.md) | MeshImport v0, material hot reload |

## Lighting epics (reference, not WIP)

| File | Purpose |
|------|---------|
| [`forward-rendering-epic_Plan.md`](forward-rendering-epic_Plan.md) | Stage 1 — forward baseline (closed) |
| [`forward-stage1.md`](forward-stage1.md) | Stage 1 golden/perf, handoff, gaps (canonical) |
| [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) | Stage 2 — hybrid deferred |
| [`ddgi-epic_Plan.md`](ddgi-epic_Plan.md) | Stage 3 — DDGI |

---

## Archive

| Path | Contents |
|------|----------|
| [`Archived/`](Archived/) | Sprint retrospectives, notes, completed plan logs |
| [`Archived/plans/`](Archived/plans/) | Closed `{Task}_Plan.md` + `_Progress.md` |
