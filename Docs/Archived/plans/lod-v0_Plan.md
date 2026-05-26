# Plan: lod-v0

**Sprint:** S1 — CPU draw stream  
**Status:** Done (2026-05-26)  
**SprintPlan:** LOD v0 block (CPU distance LOD + hysteresis; resolved meshId in draw stream).

## Problem

Draw instances use the SoA mesh column directly as the GPU mesh id. M1 acceptance requires distance-driven LOD with a documented chain and logged `meshId` changes.

## Goals

1. **Asset / doc:** Document a sample LOD chain (tree: `kenney_tree_detailed` → `kenney_tree_simple`) in `Data/LOD.md`.
2. **SoA:** `logicalMeshId` column (rename semantics of mesh column) + `lodBias` per entity.
3. **LOD table:** CPU `Gfx_LodTable` maps logical id → ordered physical mesh ids + distance thresholds.
4. **Cull prep:** After frustum cull, resolve LOD from eye-space distance (bounds center) with hysteresis; write **resolved** `myMeshId` on `Gfx_DrawInstance` and refresh opaque sort keys.
5. **Demo:** Two entities share logical tree group (near + far); log once per session with `resolvedMeshId` + distance.
6. **Docs:** Archive SprintPlan LOD lines; update `EngineArchitecture.md` §5.x + S1 notes.

## Non-goals

- Screen-size metric, GPU LOD, JSON scene LOD (scene-load).
- Per-asset automatic LOD generation.

## Design

| Topic | Choice |
|-------|--------|
| Logical vs physical | SoA stores **logical** mesh id; `Gfx_DrawInstance.myMeshId` is **resolved** after `Gfx_ApplyLod` |
| Metric | Eye-space distance to **bounds center** (world) |
| Hysteresis | 15% margin on thresholds: coarser LOD when `dist >= threshold * 1.15`; finer when `dist < threshold * 0.85` |
| Order | Extract → Cull → **ApplyLod** → Sort → Batch |
| Demo chain | `kLogicalTree` → mesh 2 (detailed) / mesh 3 (simple) at 14 m |

## Files

| File | Change |
|------|--------|
| `Data/LOD.md` | Sample chain doc |
| `Gfx_Lod.h` / `Gfx_Lod.cpp` | Table, resolver, apply to frame extract |
| `Gfx_SceneSoA.*` | `lodBias`, logical mesh column accessors |
| `Gfx_DrawExtract.cpp` | Sort key uses logical id (unchanged column read) |
| `Util_DemoAssets.h` | `kLogical*` ids |
| `Vk_Core.*` | Lod table init, apply hook, demo entities, `[LOD]` log |
| `EngineArchitecture.md`, `SprintPlan.md`, `Docs/README.md` | Sync |

## Verification

1. MSBuild `Debug|x64` — exit 0.
2. Smoke-run ~4 s; log contains `[LOD]` with `resolvedMeshId=2` or `3` for tree entities.
3. Fly camera farther than ~14 m from back tree → log shows `resolvedMeshId=3` (simple).
