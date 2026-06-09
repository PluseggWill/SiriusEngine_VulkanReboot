# LOD chains (v0 sample)

CPU LOD v0 uses a **logical mesh id** on each entity. `Gfx_LodTable` maps logical id → ordered physical mesh ids (resource table indices). Distance thresholds pick the level; hysteresis reduces flicker at boundaries.

## Demo tree chain

| LOD level | Physical mesh | Asset path | Switch |
|-----------|---------------|------------|--------|
| 0 (high) | mesh id `2` | `Data/Models/kenney_tree_detailed.obj` | eye distance &lt; 14 m (with hysteresis) |
| 1 (low) | mesh id `3` | `Data/Models/kenney_tree_simple.obj` | eye distance ≥ 14 m |

Logical id: `"tree"` in `Data/Scenes/demo.json` → `logicalMeshes` (runtime: `Gfx_BuildLodTableFromSceneDesc`).

## Stress scene LOD chains (`stress.json`)

Valley layout: north cliff + waterfall → river (south) → stone bridge → east longhouse / west forest.

| Logical id | High → low meshes | Threshold |
|------------|-------------------|-----------|
| `tree` | `kenney_tree_detailed` → `kenney_tree_simple` | 18 m |
| `tree_giant` | `kenney_tree_tall` → `kenney_tree_simple` | 20 m |
| `pine` | `kenney_tree_pine_a` → `kenney_tree_pine_simple` | 16 m |
| `tree_oak` | `kenney_tree_oak` → `kenney_tree_small` | 18 m |

Regenerate layout: `Scripts/Generate-StressScene.ps1`. Run with `Config/engine.stress.json` (`lodEnabled: true`).

## Adding a chain (scene JSON, shipped)

1. List physical meshes under `meshes` in the scene file.
2. Add a `logicalMeshes` entry with `lodMeshes` (and optional `lodDistances`).
3. Reference the logical id from `entities[].logicalMesh`.

Runtime LOD chains come from scene JSON via `Gfx_BuildLodTableFromSceneDesc` (see `Docs/SceneJSON.md`).

**Scene JSON:** define chains under `logicalMeshes` in `Data/Scenes/*.json` — see [`Docs/SceneJSON.md`](../Docs/SceneJSON.md).

## Hysteresis (v0)

At threshold `T` (demo tree: 14 m), switching uses a 15% margin to reduce flicker:

- **Coarser** (LOD0 → LOD1): when `distance >= T × 1.15`
- **Finer** (LOD1 → LOD0): when `distance < T × 0.85`

Optional per-entity `lodBias` is added to distance before level selection (`Gfx_SceneSoA::AllocEntity`).

## Verification

Smoke-run logs once per session (`[LOD]`): near demo tree ~12 m → `resolvedMeshId=2` (detailed); far tree ~25 m → `resolvedMeshId=3` (simple). Fly camera across the threshold to see swaps.

Future: scene JSON `lodChain` + paths per level (`scene-load`).
