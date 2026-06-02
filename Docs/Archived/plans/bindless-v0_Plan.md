# Plan: bindless-v0

**Sprint:** S1 — CPU draw stream  
**Status:** Done (2026-05-26)  
**SprintPlan:** Bindless v0 block.

## Problem

Set 1 binds one descriptor set per material batch. S5/S6 need a signed-off material indexing model with a validation-friendly fallback.

## Goals

1. **Decision (locked):** **Hybrid** — Set 0 frame + Set 2 instance unchanged; Set 1 either **batch** (per-material descriptors) or **bindless** (texture array + material SSBO) when `VK_EXT_descriptor_indexing` is available.
2. **Probe:** log capability; auto-select path; `FORCE_MATERIAL_BATCH=1` env forces batch fallback.
3. **GPU material table:** SSBO `GpuMaterialTableEntry { textureIndex, alpha }`; fragment uses `materialIndex` from `GpuObjectData`.
4. **Sort key:** `myMaterialTableGeneration` on resource tables (v0: 0 at load); packed into opaque sort key `pipelinePermutationId` field.
5. **Docs:** `EngineArchitecture.md` §5.3 decision; SprintPlan archive.

## Non-goals

- Descriptor buffers, bindless mesh/scene data, runtime table reload, ImGui toggle (S7 preset).

## Design

| Topic | Choice |
|-------|--------|
| Bindless Set 1 | `sampler2D u_Textures[kMaxBindlessTextures]` + `readonly buffer` material table |
| materialIndex | `GpuObjectData` (Set 2), passed VS→FS as `flat uint` |
| Fallback | Batch path unchanged when indexing missing or env override |
| Record | Bindless: Set 1 once per pass; per-draw Set 2 only (no per-batch material bind) |

## Files

| File | Change |
|------|--------|
| `Vk_Bindless.h/cpp` | Probe, path enum |
| `Vk_DescriptorPolicy.h` | `kMaxBindlessTextures` |
| `Vk_Types.h` | `GpuMaterialTableEntry` |
| `Vk_Camera.h` | `GpuObjectData.materialIndex` |
| `TriangleVertex.vert`, `TriangleFrag_Lit_Bindless.frag` | materialIndex + nonuniform texture fetch |
| `CompileShader_Glslc.bat` | `TrianglePix_Bindless.spv` |
| `Vk_Core.*` | bindless layout, descriptors, pipelines, record branch |
| `Vk_ResourceTables.*` | `myMaterialTableGeneration` |
| `Gfx_DrawExtract.cpp` | generation in sort key |
| Docs | architecture, sprint, progress |

## Verification

1. MSBuild Debug|x64 — exit 0; bindless frag compiles.
2. Smoke-run: `[BINDLESS] path=Bindless` or `path=Batch`; draws succeed.
3. Log `materialTableGeneration=0`; `materialSetBinds=0` when bindless.
