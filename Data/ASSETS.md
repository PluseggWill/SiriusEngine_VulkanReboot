# Data assets (CC0)

Curated test content for multi-mesh / LOD / batch experiments. All listed assets are **CC0** (public domain); attribution is optional but appreciated.

## Stress scene (`Data/Scenes/stress.json`) — asset map

| Role | Repo path | Source | License |
|------|-----------|--------|---------|
| Default test scene | `Data/Scenes/stress.json` | Procedural (`Scripts/Generate-StressScene.ps1`) | — |
| Longhouse mesh | `Data/Models/viking_room.obj` | Project demo mesh | existing |
| Mist debug prop | `Data/Models/monkey_smooth.obj` | Project demo mesh | existing |
| Kenney props / terrain | `Data/Models/kenney_*.obj` | [Kenney Nature Kit 2.1](https://kenney.nl/assets/nature-kit) · [OpenGameArt mirror](https://opengameart.org/content/nature-kit) | **CC0** |
| Albedo (grass / rock / wood / metal) | `Data/Textures/ph_*_diff_1k.jpg` | [Poly Haven](https://polyhaven.com/) | **CC0** |
| Longhouse albedo | `Data/Textures/viking_room.png` | Project demo | existing |

**Attribution (optional):** Kenney — [kenney.nl](https://kenney.nl) · Poly Haven — [polyhaven.com](https://polyhaven.com).

**Fetch / regenerate:**

```powershell
powershell -File Scripts/Fetch-KenneyNatureSubset.ps1   # CC0 OBJ subset -> Data/Models/
powershell -File Scripts/Generate-StressScene.ps1       # stress.json from layout script
```

Kenney `.mtl` solid colors are not shipped; stress materials use Poly Haven albedo × `baseColor` tuned to Nature Kit `Kd` values (see scene JSON `materials[]`).

## Models (`Models/`)

| File | Source | Notes |
|------|--------|--------|
| `viking_room.obj`, `monkey_smooth.obj` | Project demo / common test meshes | `demo.json` / Stage 1 golden |
| `kenney_*.obj` | Kenney Nature Kit 2.1 OBJ export | Renamed with `kenney_` prefix; see mapping below |

### Kenney Nature Kit — repo filename → source OBJ

Copied by `Scripts/Fetch-KenneyNatureSubset.ps1` from `Models/OBJ format/` inside [Nature Kit (2.1).zip](https://opengameart.org/sites/default/files/Nature%20Kit%20%282.1%29.zip):

| Repo file | Kenney source |
|-----------|---------------|
| `kenney_ground_grass.obj` | `ground_grass.obj` |
| `kenney_ground_river_straight.obj` | `ground_riverStraight.obj` |
| `kenney_ground_river_corner.obj` | `ground_riverCorner.obj` |
| `kenney_ground_path_straight.obj` | `ground_pathStraight.obj` |
| `kenney_cliff_rock.obj` | `cliff_rock.obj` |
| `kenney_cliff_large_rock.obj` | `cliff_large_rock.obj` |
| `kenney_cliff_corner_rock.obj` | `cliff_corner_rock.obj` |
| `kenney_cliff_waterfall.obj` | `cliff_waterfall_rock.obj` |
| `kenney_cliff_waterfall_top.obj` | `cliff_waterfallTop_rock.obj` |
| `kenney_bridge_side_stone.obj` | `bridge_side_stone.obj` |
| `kenney_bridge_center_stone.obj` | `bridge_center_stone.obj` |
| `kenney_tree_detailed.obj` | `tree_detailed.obj` |
| `kenney_tree_simple.obj` | `tree_simple.obj` |
| `kenney_tree_tall.obj` | `tree_tall.obj` |
| `kenney_tree_oak.obj` | `tree_oak.obj` |
| `kenney_tree_small.obj` | `tree_small.obj` |
| `kenney_tree_pine_a.obj` | `tree_pineDefaultA.obj` |
| `kenney_tree_pine_simple.obj` | `tree_pineSmallA.obj` |
| `kenney_bush_large.obj` | `plant_bushLarge.obj` |
| `kenney_bush_small.obj` | `plant_bushSmall.obj` |
| `kenney_rock_large_a.obj` | `rock_largeA.obj` |
| `kenney_rock_large_b.obj` | `rock_largeB.obj` |
| `kenney_rock_small_a.obj` | `rock_smallA.obj` |
| `kenney_campfire_stones.obj` | `campfire_stones.obj` |
| `kenney_tent_closed.obj` | `tent_detailedClosed.obj` |
| `kenney_log_stack.obj` | `log_stack.obj` |
| `kenney_stump_round.obj` | `stump_round.obj` |
| `kenney_path_stone.obj` | `path_stone.obj` |

**Also in repo (optional props, same CC0 pack):** `kenney_fence_simple.obj`, `kenney_flower_red.obj`, `kenney_mushroom_red.obj`, `kenney_stump_old.obj`, `kenney_tent_open.obj` — fetch script map can be extended.

Full pack (~330 models) is not in-repo.

## Textures (`Textures/`)

| File | Source | License |
|------|--------|---------|
| `viking_room.png`, `RedMoon.jpg` | Demo scene | Existing |
| `ph_rock_diff_1k.jpg` | [Poly Haven — rock_01](https://polyhaven.com/a/rock_01) | CC0 |
| `ph_grass_diff_1k.jpg` | [Poly Haven — aerial_grass_rock](https://polyhaven.com/a/aerial_grass_rock) | CC0 |
| `ph_metal_diff_1k.jpg` | [Poly Haven — metal_plate](https://polyhaven.com/a/metal_plate) | CC0 |
| `ph_wood_table_diff_1k.jpg` | [Poly Haven — wood_table_worn](https://polyhaven.com/a/wood_table_worn) | CC0 |

Diffuse-only 1K JPGs; sufficient for albedo sampling in the current lit shader.

## LOD chains

See [`LOD.md`](LOD.md) for demo + stress LOD chains.

## Scenes

| File | Purpose |
|------|---------|
| `Data/Scenes/stress.json` | **Default** valley test scene (cull / LOD / batch) |
| `Data/Scenes/demo.json` | Stage 1 forward golden baseline |
| `Data/Scenes/smoke.json` | CI minimal load |

## Usage

Paths are repo-relative under `Data/` (see `Util_AssetConfig` / `UtilLoader::ResolvePath`). New meshes are referenced from scene JSON manifests (not hard-coded startup lists).

Example manifest entries:

```text
Data/Models/kenney_tree_detailed.obj
Data/Textures/ph_rock_diff_1k.jpg
```

**Stress scene:** regenerate layout with `Scripts/Generate-StressScene.ps1` after editing placement logic; fetch new Kenney OBJs with `Scripts/Fetch-KenneyNatureSubset.ps1`.
