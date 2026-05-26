# Data assets (CC0)

Curated test content for multi-mesh / LOD / batch experiments. All listed assets are **CC0** (public domain); attribution is optional but appreciated.

## Models (`Models/`)

| File | Source | Notes |
|------|--------|--------|
| `viking_room.obj`, `monkey_smooth.obj` | Project demo / common test meshes | Existing demo scene |
| `kenney_*.obj` | [Kenney Nature Kit](https://kenney.nl/assets/nature-kit) (via [OpenGameArt](https://opengameart.org/content/nature-kit)) | Low-poly props; UVs present; solid-color materials in `.mtl` (not bundled) |

Kenney subset copied from Nature Kit 2.1 OBJ export:

- `kenney_tree_detailed.obj` — large tree (~42 KB)
- `kenney_tree_simple.obj` — smaller tree (LOD proxy)
- `kenney_rock_large_a.obj` — medium rock
- `kenney_stump_round.obj` — small prop
- `kenney_campfire_stones.obj` — small prop
- `kenney_tent_closed.obj` — medium prop

Full pack (~330 models) is not in-repo; re-download from Kenney/OpenGameArt if you need more.

## Textures (`Textures/`)

| File | Source | License |
|------|--------|---------|
| `viking_room.png`, `RedMoon.jpg` | Demo scene | Existing |
| `ph_rock_diff_1k.jpg` | [Poly Haven — rock_01](https://polyhaven.com/a/rock_01) | CC0 |
| `ph_grass_diff_1k.jpg` | [Poly Haven — aerial_grass_rock](https://polyhaven.com/a/aerial_grass_rock) | CC0 |
| `ph_metal_diff_1k.jpg` | [Poly Haven — metal_plate](https://polyhaven.com/a/metal_plate) | CC0 |
| `ph_wood_table_diff_1k.jpg` | [Poly Haven — wood_table_worn](https://polyhaven.com/a/wood_table_worn) | CC0 |

Diffuse-only 1K JPGs; sufficient for albedo sampling in the current lit shader.

## Usage

Paths are repo-relative under `Data/` (see `Util_AssetConfig` / `UtilLoader::ResolvePath`). New meshes are **not** wired into `Util_DemoAssets::kRequiredFiles` until a test scene or manifest references them.

Example manifest entries (when extending the demo):

```text
Data/Models/kenney_tree_detailed.obj
Data/Textures/ph_rock_diff_1k.jpg
```
