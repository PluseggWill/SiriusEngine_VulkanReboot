# LOD chains (v0 sample)

CPU LOD v0 uses a **logical mesh id** on each entity. `Gfx_LodTable` maps logical id → ordered physical mesh ids (resource table indices). Distance thresholds pick the level; hysteresis reduces flicker at boundaries.

## Demo tree chain

| LOD level | Physical mesh | Asset path | Switch |
|-----------|---------------|------------|--------|
| 0 (high) | mesh id `2` | `Data/Models/kenney_tree_detailed.obj` | eye distance &lt; 14 m (with hysteresis) |
| 1 (low) | mesh id `3` | `Data/Models/kenney_tree_simple.obj` | eye distance ≥ 14 m |

Logical id: `UtilDemoAssets::kLogicalTree` (see `Gfx_BuildDemoLodTable`).

## Adding a chain (code until scene JSON)

1. Register each physical mesh in `Gfx_BuildDemoResourceManifest`.
2. `Gfx_LodTable::SetChain( logicalId, { meshIdLod0, meshIdLod1, ... }, { threshold0, ... } )`.
3. Spawn entities with the **logical** id, optional `lodBias` on `AllocEntity`.

## Hysteresis (v0)

At threshold `T` (demo tree: 14 m), switching uses a 15% margin to reduce flicker:

- **Coarser** (LOD0 → LOD1): when `distance >= T × 1.15`
- **Finer** (LOD1 → LOD0): when `distance < T × 0.85`

Optional per-entity `lodBias` is added to distance before level selection (`Gfx_SceneSoA::AllocEntity`).

## Verification

Smoke-run logs once per session (`[LOD]`): near demo tree ~12 m → `resolvedMeshId=2` (detailed); far tree ~25 m → `resolvedMeshId=3` (simple). Fly camera across the threshold to see swaps.

Future: scene JSON `lodChain` + paths per level (`scene-load`).
