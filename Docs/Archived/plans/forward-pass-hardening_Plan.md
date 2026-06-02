# Plan: forward-pass-hardening

**Sprint:** S2 — Stage 1 forward epic §B  
**Status:** Closed (2026-06-02)  
**Roadmap:** [`Active-Plan.md`](../Active-Plan.md) — epic §C remains open  
**Epic:** [`forward-rendering-epic_Plan.md`](../forward-rendering-epic_Plan.md) § B  
**Depends on:** [`forward-stage1-contracts_Plan.md`](forward-stage1-contracts_Plan.md) (§A closed 2026-06-01)

## Problem statement

Stage 1 §A froze material/preset contracts and S1 already splits opaque vs transparent in Gfx + record. Epic §B is still open: pass boundaries are not documented at the record site, Stage 2 depth consumption rules are not written down, and there are no runtime debug hooks for forward parity work.

## Goals

1. **Pass-level contracts** — document and comment the forward opaque → transparent record flow (`Gfx_FrameRenderPacket` → `Vk_ScenePasses`).
2. **Stage 2 depth policy** — document transparent pass depth test ON / write OFF / reads opaque depth; map to future `ForwardTransparent` FG node.
3. **Debug views** — ImGui panel: skip opaque/transparent, **Depth** and **World normal** visualization modes, read-only preset/permutation display.
4. **Material alpha** — per-material `alphaMode == mask` discard in batch + bindless lit frags; bindless `GpuMaterialTableEntry` gains `alphaMode` (symmetry with batch UBO).
5. **Gaps note** — short Architecture subsection for items still deferred to Stage 2 / epic §C (no golden captures in this task).

## Non-goals

- Epic §C: golden screenshots, perf baseline CSV, full deferred migration checklist → **`forward-stage1-validation`** (separate task).
- PBR shading (`Gfx_ShaderFeature_Pbr`), `HybridDeferred` preset, frame graph pass split, runtime preset/pipeline hot-reload.
- Multi-view, new render passes, descriptor layout changes beyond `EnvironmentData.fogDistances.w` debug packing.

## Design decisions (user-confirmed 2026-06-02)

| Topic | Choice |
|-------|--------|
| Debug scope | **B:** skip-pass toggles + **Depth** + **World normal** shader modes (not runtime preset switch). |
| `alphaMode` shader | **B:** `mask` → `discard` when sampled alpha &lt; 0.5 (batch + bindless); keep `ForwardLitAlphaClip` global perm. |
| Bindless table | **A:** add `alphaMode` to `GpuMaterialTableEntry` + upload + GLSL struct. |
| Debug mode transport | Pack into `GpuEnvironmentData.myFogDistance.w` as `float` (0=Lit, 1=Depth, 2=Normals). **std140 layout unchanged** (same vec4 size). |
| Depth viz | Fragment: `gl_FragCoord.z` grayscale (parity-oriented, not linear eye-Z). |
| Normal viz | Fragment: `normalize(inWorldNormal) * 0.5 + 0.5`. |
| Skip flags | Session-only `bool` on `Vk_Core`, driven by ImGui (not persisted to `engine.json`). |

## Touch list

| Area | Files |
|------|--------|
| Pass contracts | `Vk_ScenePasses.cpp/.h`, `Gfx_RenderPacket.h` |
| Debug UBO | `Vk_Types.h`, `TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag` |
| Material / bindless | `Vk_Types.h`, `Vk_DescriptorSystem.cpp` |
| Record flags | `Vk_Core.h`, `Vk_ScenePasses.cpp` |
| UI | `Util_RenderDebugPanel.*`, `Vk_Core.cpp`, `VulkanDesktop.vcxproj` |
| Config display | `Util_EngineConfig.h/.cpp` |
| Docs | `EngineArchitecture.md`, `forward-rendering-epic_Plan.md` |

## Implementation steps

1. [x] Pass contracts (docs + comments).
2. [x] Debug view enum + UBO + lit frags.
3. [x] SPIR-V regen (MSBuild).
4. [x] Per-material mask discard.
5. [x] Bindless `alphaMode`.
6. [x] Record skip flags (`myRenderDebugState`).
7. [x] `Util_RenderDebugPanel` (prep → panel → upload order).
8. [x] `GetRenderPresetName()`.
9. [x] Architecture gaps subsection.
10. [x] Optional demo — skipped (no mask-friendly asset in demo).
11. [x] Epic §B checkboxes.
12. [x] Build + smoke.

## Verification

`.\VulkanDesktop.exe --no-validation --smoke-seconds 6 --scene Data/Scenes/demo.json` exit **0**; log: `renderPreset=ForwardLit`, `LoadSceneResources completed`, `Smoke dwell reached`.

## Follow-up

- **`forward-stage1-validation`** — epic §C + Acceptance.
