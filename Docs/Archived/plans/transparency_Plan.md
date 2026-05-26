# Plan: transparency

**Sprint:** S1 — CPU draw stream  
**Status:** Done (2026-05-26)  
**SprintPlan:** Transparency block (opaque + transparent lists, record two passes).

## Problem

Only opaque forward pass exists. M1 requires at least one transparent object with correct blend order over opaque geometry.

## Goals

1. **Render flags** on SoA entity (`opaque` / `transparent`).
2. **Extract** → `Gfx_FrameExtract` with separate opaque + transparent `Gfx_ExtractResult` lists (no Vulkan in Gfx).
3. **Transparent sort:** back-to-front by **eye-space Z** (view matrix); tie-break `materialId`, then `entityIndex`.
4. **Record:** opaque pass (depth write, no blend) then transparent pass (depth test on, depth write off, alpha blend) in same render pass.
5. **Set 1** material `alpha` uniform; transparent demo entity in front of opaque scene.
6. Log once: opaque/transparent draw counts + batch runs.

## Non-goals

- Order-independent transparency, separate FG node, dual-source blend.
- LOD / bindless changes.

## Design

| Topic | Choice |
|-------|--------|
| Flags | `Gfx_RenderFlags` column on `Gfx_SceneSoA`; default opaque |
| Material | `myIsTransparent` + `myAlpha` on manifest → `GpuMaterialParams` Set 1 binding 1 |
| Pipelines | `myBasicPipeline` (opaque), `myTransparentPipeline` (blend on, depth write off) |
| Batch | Reuse `Gfx_BuildOpaqueDrawBatches` for both lists |
| Slab | Single slab: opaque draws first, then transparent (`FillInstanceSlab`) |

## Tie-break (EngineArchitecture §5.8)

Sort transparent by ascending eye-space Z (farther first). Equal Z: lower `materialId`, then lower `entityIndex`.

## Files

| File | Change |
|------|--------|
| `Gfx_SceneSoA.*` | `myRenderFlags`, `AllocEntity` param |
| `Gfx_DrawExtract.*` | `Gfx_FrameExtract`, split extract |
| `Gfx_DrawCullSort.*` | `Gfx_SortTransparentDrawInstances`, eye Z |
| `Gfx_ResourceManifest.*` | transparent material + alpha |
| `Vk_Types.h` / `Vk_Enum.h` | `GpuMaterialParams`, Set 1 binding |
| `TriangleFrag_Lit.frag` | material.alpha output |
| `Vk_Initializer.*` | alpha blend + depth write off helpers |
| `Vk_Core.*` | transparent pipeline, dual extract, record passes |
| `Vk_ResourceTables.*` | per-material pipeline + alpha buffer |
| Docs | SprintPlan, EngineArchitecture, README |

## Steps

- [x] P1 — SoA render flags + manifest material alpha/transparent
- [x] P2 — Gfx extract/sort split (opaque + transparent)
- [x] P3 — Transparent pipeline + Set 1 alpha binding + shader
- [x] P4 — Record opaque then transparent; slab + demo entity
- [x] P5 — Build + smoke-run
- [x] P6 — Archive SprintPlan transparency lines; sync docs

## Build / smoke-run

MSBuild Debug|x64; smoke 4s; center monkey visible through blend; log `TRANSP` counts.
