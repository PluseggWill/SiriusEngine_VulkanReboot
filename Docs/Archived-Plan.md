# Archived Plan — SiriusEngine / VulkanDesktop

Completed work from [`Active-Plan.md`](Active-Plan.md). Per-task logs: [`Archived/plans/`](Archived/plans/).

**Format for new closes:** one stub section (outcome + plan link); do **not** paste full `[x]` checklists here — those live in `Archived/plans/*_Plan.md`.

---

## S1 — implementation notes *(keep current — code-path reference)*

| Topic | State | Follow-up |
|-------|--------|-----------|
| Resource tables | Done — `Gfx_ResourceManifest`, `Vk_ResourceTables`, `RecordScenePass` | Scene manifest via scene-load Phase C |
| Per-draw `model` | Done — Set 2 `UNIFORM_BUFFER_DYNAMIC` + `dynamicOffset` | — |
| Record ↔ transforms | Done — SoA before extract; slab copies matrix | — |
| Instance slab | Done — overflow fail-closed | — |
| Set 0 / Set 1 | Done — batch + bindless | S13 preset toggle |
| Draw submission | Done — set 0 once/pass; set 1/batch; set 2/draw | — |
| RenderDoc capture path | Done — passive attach + labels; Batch path under injected sessions | Revisit bindless+RenderDoc in S13 lab |
| Transparency | Done — opaque + transparent lists | — |
| LOD v0 (CPU) | Done — `Gfx_LodTable` → resolved `meshId` | Geometry track |

**Pitfall:** Do not patch `model` in a shared per-frame camera UBO between draws — use dynamic offsets (`.cursor/rules/vulkan-descriptor-per-draw.mdc`, `EngineArchitecture.md` §6.1).

---

## Closed sprints / gates (stubs)

| Id | Closed | Outcome | Plan logs |
|----|--------|---------|-----------|
| **S0** | 2026-05-23 | Toolchain, validation, asset root, shader pipeline | `Archived/plans/` (bootstrap, validation-layers, …) |
| **S1 / M1** | 2026-05-26 | CPU draw stream (extract → sort → batch → record) | `draw-extract`, `scene-soa`, `bindless-v0`, … |
| **S2** | 2026-06-02 | Lifecycle/config/RHI peel; Stage 1 forward | `vk-core-decomposition`, `forward-stage1-*`, … |
| **P0** | 2026-06-02 | Verify-CI / GfxTests / smoke scripts | `ci-verification` |
| **P1–P2 RHI** | 2026-06-09 | Swapchain, camera UBO, slab overflow, WSI soft | `rhi-*`, `vulkan-rhi-p2` |
| **P2–P3 / S3** | 2026-06-11 | GPU indirect + HybridDeferred FG v0 slices | `render-m2-*`, `s3-fg-*`, `s3-fg-v0` |
| **P4** | 2026-06-11 | Vertical slice v0 | `p4-vertical-slice-v0` |
| **S4** | 2026-06-12 | PBR + G-buffer | `s4-pbr-gbuffer` |
| **S5** | 2026-06-12 | IBL + skybox + shadows | `s5-ibl-shadows`, `lighting-shadow-refactor` |
| **S6** | 2026-06-15/16 | SSAO + Hi-Z + HBAO+/GTAO/contact soft | `s6-ssao-hiz`, `hbao-plus`, `gtao`, `contact-soft-ao` |
| **S7** | 2026-06-15 | Post + frame graph v1 | `s7-post-fg` |
| **G4** | 2026-06-16 | Stage 2 hybrid acceptance on Sponza | [`hybrid-deferred-epic_Plan.md`](Archived/plans/hybrid-deferred-epic_Plan.md) |
| **S8** | 2026-06-16 | DDGI / Stage 3 | [`ddgi-lighting-epic_Plan.md`](Archived/plans/ddgi-lighting-epic_Plan.md) |
| **RHI-E4** | 2026-06-16 | `Vk_RhiDevice` + `Vk_Renderer` + `App_PlatformHost` | [`rhi-independence_Plan.md`](Archived/plans/rhi-independence_Plan.md) |
| **IBL fix** | 2026-07-13 | Diffuse IBL π energy, sun intensity, camera far | `ibl-lighting-fix` |
| **Specular IBL stack** | 2026-07-13 | Prefilter GGX mips + specular occlusion + SSR layering (+ C v0 cones/probe) | `specular-ibl-stack` |
| **S9 / G5** | 2026-07-14 | Halton + G-buffer MV + TAA v0.5; AO/SSR on shared temporal lifetime | [`temporal_Plan.md`](Archived/plans/temporal_Plan.md) · branch retro [`S8-S9-回顾总结.md`](Archived/S8-S9-回顾总结.md) |

Lighting stage epics (closed): Stage 1 [`forward-rendering-epic_Plan.md`](Archived/plans/forward-rendering-epic_Plan.md) · Stage 2 [`hybrid-deferred-epic_Plan.md`](Archived/plans/hybrid-deferred-epic_Plan.md) · Stage 3 [`ddgi-lighting-epic_Plan.md`](Archived/plans/ddgi-lighting-epic_Plan.md).

Pass topology (current): [`EngineArchitecture.md`](EngineArchitecture.md) §7.

---

*When closing a task: add one stub row (or short section), move Plan+Progress → `Archived/plans/`. Update `EngineArchitecture.md` only on locked policy change.*
