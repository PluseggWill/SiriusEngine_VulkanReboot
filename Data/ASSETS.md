# Data assets (CC0)

Curated test content for multi-mesh / LOD / batch experiments. All listed assets are **CC0** (public domain); attribution is optional but appreciated.

## Stress scene (`Data/Scenes/stress.json`) — asset map

| Role | Repo path | Source | License |
|------|-----------|--------|---------|
| Default test scene | `Data/Scenes/stress.json` | Procedural (`Scripts/Generate-StressScene.ps1`) | — |
| Longhouse mesh | `Data/Models/viking_room.obj` | Project demo mesh | existing |
| Mist debug prop | `Data/Models/monkey_smooth.obj` | Project demo mesh | existing |
| Kenney props / terrain | `Data/Models/kenney_*.obj` | [Kenney Nature Kit 2.1](https://kenney.nl/assets/nature-kit) · [OpenGameArt mirror](https://opengameart.org/content/nature-kit) | **CC0** |
| Albedo (grass / foliage / cliff / rock / path) | `Data/Textures/ph_*_diff_1k.jpg` | [Poly Haven](https://polyhaven.com/) — see table below | **CC0** |
| Longhouse albedo | `Data/Textures/viking_room.png` | Project demo | existing |

**Attribution (optional):** Kenney — [kenney.nl](https://kenney.nl) · Poly Haven — [polyhaven.com](https://polyhaven.com).

**Fetch / regenerate:**

```powershell
powershell -File Scripts/Fetch-KenneyNatureSubset.ps1        # CC0 OBJ -> Data/Models/
powershell -File Scripts/Fetch-PolyHavenStressTextures.ps1   # CC0 JPG -> Data/Textures/
powershell -File Scripts/Generate-StressScene.ps1            # stress.json
```

Kenney `.mtl` solid colors are not shipped. Stress materials use dedicated Poly Haven diffuse maps (see below); water/tent keep light `baseColor` tints.

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

### Stress scene (Poly Haven CC0, 1K diffuse JPG)

| Repo file | Poly Haven id | Used by (`stress.json`) |
|-----------|---------------|-------------------------|
| `ph_grass_ground_diff_1k.jpg` | [sparse_grass](https://polyhaven.com/a/sparse_grass) | `mat_grass` — ground tiles |
| `ph_foliage_diff_1k.jpg` | [forest_leaves_03](https://polyhaven.com/a/forest_leaves_03) | `mat_foliage` — trees / bushes |
| `ph_cliff_rock_diff_1k.jpg` | [cliff_side](https://polyhaven.com/a/cliff_side) | `mat_cliff` — cliff / waterfall |
| `ph_rock_face_diff_1k.jpg` | [rock_face](https://polyhaven.com/a/rock_face) | `mat_rock`, `mat_bridge`, `mat_stone`, river tint base |
| `ph_grass_path_diff_1k.jpg` | [grass_path_3](https://polyhaven.com/a/grass_path_3) | `mat_path` — footpath |
| `ph_wood_table_diff_1k.jpg` | [wood_table_worn](https://polyhaven.com/a/wood_table_worn) | `mat_wood`, `mat_tent` |
| `ph_metal_diff_1k.jpg` | [metal_plate](https://polyhaven.com/a/metal_plate) | (reserved / props) |

Fetch: `Scripts/Fetch-PolyHavenStressTextures.ps1` (grass/cliff/rock/path/foliage subset).

### Demo / legacy (still used by `demo.json`)

| File | Source | License |
|------|--------|---------|
| `viking_room.png`, `RedMoon.jpg` | Project demo | existing |
| `ph_rock_diff_1k.jpg` | [Poly Haven — rock_01](https://polyhaven.com/a/rock_01) | CC0 |
| `ph_grass_diff_1k.jpg` | [Poly Haven — aerial_grass_rock](https://polyhaven.com/a/aerial_grass_rock) | CC0 |

Diffuse-only 1K JPGs; lit shader samples albedo only (`textureBlend` from env UBO). Enable `runtimeMipmap` in `engine.json` for ground viewing angles.

## LOD chains

See [`LOD.md`](LOD.md) for demo + stress LOD chains.

## Sponza benchmark (`Data/Scenes/sponza.json`) — fetch-on-demand

| Role | Repo path | Source | License |
|------|-----------|--------|---------|
| Scene manifest | `Data/Scenes/sponza.json` | `Scripts/Generate-SponzaScene.ps1` | — |
| Source mesh + textures | `Data/Models/sponza/source/` | [McGuire Graphics Archive — Crytek Sponza](https://casual-effects.com/data/) | **CC BY 3.0** (© Frank Meinl, Crytek) |
| Split OBJ parts (25 materials) | `Data/Models/sponza/parts/` | Generated from source | — |

**Not in git** (`Data/Models/sponza/` is gitignored, ~80 MB). Fetch once:

```powershell
powershell -File Scripts/Fetch-SponzaMcGuire.ps1        # download + split + sponza.json
powershell -File Scripts/Generate-SponzaScene.ps1       # regenerate after editing generator
```

**Run:**

```powershell
VulkanDesktop.exe --config Config/engine.sponza.json
# or
VulkanDesktop.exe --scene Data/Scenes/sponza.json --config Config/engine.sponza.json
```

**Attribution (recommended):** Morgan McGuire, [Computer Graphics Archive](https://casual-effects.com/data), 2017.

## Scenes

| File | Purpose |
|------|---------|
| `Data/Scenes/sponza.json` | **Default** indoor lighting benchmark (McGuire Sponza; requires fetch) |
| `Data/Scenes/stress.json` | Valley test scene (cull / LOD / batch; CI smoke) |
| `Data/Scenes/demo.json` | Stage 1 forward golden baseline |
| `Data/Scenes/smoke.json` | Minimal load (optional) |

## Usage

Paths are repo-relative under `Data/` (see `Util_AssetConfig` / `UtilLoader::ResolvePath`). New meshes are referenced from scene JSON manifests (not hard-coded startup lists).

Example manifest entries:

```text
Data/Models/kenney_tree_detailed.obj
Data/Textures/ph_rock_diff_1k.jpg
```

**Stress scene:** regenerate layout with `Scripts/Generate-StressScene.ps1` after editing placement logic; fetch new Kenney OBJs with `Scripts/Fetch-KenneyNatureSubset.ps1`.
