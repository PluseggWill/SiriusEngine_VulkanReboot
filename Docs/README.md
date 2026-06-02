# Docs — SiriusEngine / VulkanDesktop

## Active now

| Field | Value |
|-------|--------|
| **Task** | *(none — no active vibe task in Docs root)* |
| **Recommended next** | [`Active-Plan.md`](Active-Plan.md) **P0** → [`ci-verification_Plan.md`](ci-verification_Plan.md) |
| **Plan / Progress** | Roadmap plans at `Docs/*_Plan.md` (no WIP Progress until a vibe task starts) |
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
| [`Wishlist.md`](Wishlist.md) | Deferred S4–S8, experiments (unlock via Active-Plan gates) |
| [`Archived-Plan.md`](Archived-Plan.md) | Completed `[x]` sprint lines |
| [`SprintPlan.md`](SprintPlan.md) | Index → Active / Architecture / Archived |
| [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) | Sprint close-out runbook |

**Split:** tasks → Active-Plan · architecture → EngineArchitecture · doc map → `docs-roadmap-arch-sync.mdc`

### Hardening plans *(reference — start vibe task to implement)*

| Plan | Topic |
|------|--------|
| [`ci-verification_Plan.md`](ci-verification_Plan.md) | CI, tests, asset root, perf JSONL |
| [`vk-core-world-peel_Plan.md`](vk-core-world-peel_Plan.md) | WorldState, ImGui, context structs |
| [`render-m2-prep_Plan.md`](render-m2-prep_Plan.md) | CPU indirect, record hygiene, M2 prep |
| [`shader-bindless-policy_Plan.md`](shader-bindless-policy_Plan.md) | Bindless decision, perm freeze |
| [`config-platform-hardening_Plan.md`](config-platform-hardening_Plan.md) | Config instance, VK recover, Windows scope |
| [`content-pipeline_Plan.md`](content-pipeline_Plan.md) | MeshImport v0, material hot reload |

## Lighting epics (reference, not WIP)

| File | Purpose |
|------|---------|
| [`forward-rendering-epic_Plan.md`](forward-rendering-epic_Plan.md) | Stage 1 — forward baseline (closed) |
| [`forward-stage1.md`](forward-stage1.md) | Stage 1 golden/perf, handoff, gaps (canonical) |
| [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) | Stage 2 — hybrid deferred + PBR |
| [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) | Stage 3 — optional DDGI |

## Guides

| File | Purpose |
|------|---------|
| [`CLI.md`](CLI.md) | CLI args, `engine.json`, examples |
| [`vulkan-smoke-test.mdc`](../.cursor/rules/vulkan-smoke-test.mdc) | Smoke-run CLI |
| [`vulkan-desktop-quick.mdc`](../.cursor/rules/vulkan-desktop-quick.mdc) | Non-vibe code changes |
| [`bootstrap.md`](bootstrap.md) | Toolchain, build, run, logs |
| [`validation-layers.md`](validation-layers.md) | Validation layers install + toggles |
| [`SceneJSON.en.md`](SceneJSON.en.md) / [`SceneJSON.md`](SceneJSON.md) | Scene JSON v1 authoring (EN / 中文) |

## Archived task logs

Completed vibe-coding **Plan** / **Progress** pairs: [`Archived/plans/`](Archived/plans/) — see [`Archived/README.md`](Archived/README.md).

**Workflow:** on task close, collapse Progress to **Closeout**, move both files to `Archived/plans/`, move the sprint line from **Active-Plan** → **Archived-Plan**, clear **Active now** above.
