# Plan: render-m2-p3-a — entity AABB + draw template SSBO (sync SoA)

**Status:** Closed (2026-06-10)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P3** (first line)  
**Depends on:** P2 draw-template SSBO (closed), SoA world AABB (§F)

## Goal

Upload **per-entity-slot** cull input (world AABB + indexed-indirect template fields) to a GPU SSBO each frame, indexed by SoA slot — same layout the future compute cull pass will read. CPU draw path unchanged (post-cull templates + indirect).

## Non-goals

- Compute cull shader / GPU indirect fill (P3 next lines)
- Descriptor bind of entity SSBO to a pipeline (not needed until compute)
- `EngineArchitecture.md` policy change

## Touch list

| Area | Files |
|------|--------|
| Gfx contract | `Gfx_EntityGpuRecord.h/.cpp` |
| Frame buffers | `Vk_FrameData.h`, `Vk_Core.cpp` (`CreateEntityRecordBuffers`) |
| CPU fill | `Vk_FrameDrawPrep.h/.cpp`, `Vk_Core.cpp` (`PrepareFrameCpu`) |
| Policy constant | `Vk_DescriptorPolicy.h` |
| Tests | `GfxTests_Main.cpp`, `GfxTests.vcxproj` |
| Project | `VulkanDesktop.vcxproj` (+ filters) |

## Steps

1. `Gfx_EntityGpuRecord` — std430-friendly: bounds min/max, layer/render flags, mesh/material ids, `Gfx_DrawIndirectCommand`.
2. Per-frame `myEntityRecordBuffer` (STORAGE, CPU-mapped), capacity `kMaxEntitySlots` (= instance slab cap).
3. `FillEntityRecords` — iterate SoA slots; inactive slots get `layerMask=0`; mesh `myIndexCount` from resource tables.
4. Call once per frame in `PrepareFrameCpu` (before multi-view draw prep).
5. GfxTests: struct size + fill sync with SoA bounds/transform.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- `powershell -File Scripts/Verify-Smoke.ps1` exit 0
- Log: `Entity-record buffer: slots=…` · `FillEntityRecords: wrote … slot(s)`

## Risks

- Slot count > cap → fail-closed (same as slab overflow). Document in log.
