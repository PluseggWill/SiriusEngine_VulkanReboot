# Docs — SiriusEngine / VulkanDesktop

## Living documents (edit with active work)

| File | Purpose |
|------|---------|
| [`SprintPlan.md`](SprintPlan.md) | Executable roadmap (S0–S8), open `[ ]` tasks, **Archived** history |
| [`EngineArchitecture.md`](EngineArchitecture.md) | Architecture intent, invariants, render/data-plane policy |
| [`input-abstraction_Plan.md`](input-abstraction_Plan.md) / [`input-abstraction_Progress.md`](input-abstraction_Progress.md) | S2 `InputSystem` + camera input path out of `Vk_Core` — **done** 2026-05-27 |
| [`central-config_Plan.md`](central-config_Plan.md) / [`central-config_Progress.md`](central-config_Progress.md) | S2 `Util_EngineConfig` + `Config/engine.json` — **done** 2026-05-27 |
| [`application-lifecycle_Plan.md`](application-lifecycle_Plan.md) / [`application-lifecycle_Progress.md`](application-lifecycle_Progress.md) | S2 Application lifecycle + Update/Render scheduler — **done** 2026-05-27 |
| [`scene-load_Plan.md`](scene-load_Plan.md) / [`scene-load_Progress.md`](scene-load_Progress.md) | S2 scene-load Phases A–D **done** (GPU unload, ImGui scene switch, `CLI.md`) |
| [`vk-core-decomposition_Plan.md`](vk-core-decomposition_Plan.md) / [`vk-core-decomposition_Progress.md`](vk-core-decomposition_Progress.md) | S2 `Vk_Core` RHI peel (context, draw prep, record/submit) — **done** 2026-05-27 |
| [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) | Sprint close-out validation runbook (S0–S8 acceptance checks) |
| [`forward-rendering-epic_Plan.md`](forward-rendering-epic_Plan.md) | Stage 1 lighting epic: complete forward baseline before deferred migration |
| [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) | Stage 2 lighting epic: full PBR with opaque deferred/clustered + transparent forward |
| [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md) | Stage 3 lighting epic: optional DDGI layer on top of hybrid renderer |
| [`SceneJSON.en.md`](SceneJSON.en.md) / [`SceneJSON.md`](SceneJSON.md) | **Scene JSON v1 authoring** (EN / 中文) |

## Guides (not vibe task logs)

| File | Purpose |
|------|---------|
| [`CLI.md`](CLI.md) | 命令行参数、`engine.json` 字段、示例与优先级 |
| [`.cursor/rules/vulkan-smoke-test.mdc`](../.cursor/rules/vulkan-smoke-test.mdc) | Agent/CI 冒烟测试用 CLI（与 vibe-coding smoke-run 一致） |
| [`bootstrap.md`](bootstrap.md) | New-machine toolchain, build, run, logs |
| [`validation-layers.md`](validation-layers.md) | Validation layer install + runtime toggles |
| [`SceneJSON.en.md`](SceneJSON.en.md) / [`SceneJSON.md`](SceneJSON.md) | Scene JSON v1 schema and authoring (EN / 中文) |

## Archived

Completed sprint vibe-coding **Plan** / **Progress** pairs, S1 retrospective, and session notes live under [`Archived/`](Archived/README.md).

**Current sprint (S2):** keep `{TaskName}_Plan.md` and `{TaskName}_Progress.md` at **`Docs/` root** until the sprint closes or docs are bulk-archived.

When starting a new task, add both files at `Docs/` root.
