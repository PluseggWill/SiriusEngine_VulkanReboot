# Scene JSON authoring (English stub)

**Full guide (canonical):** [`SceneJSON.md`](SceneJSON.md) (中文，完整 schema).

| Item | Rule |
|------|------|
| Location | `Data/Scenes/<name>.json` |
| Default | `sponza.json` (CI smoke: `stress.json`) |
| Version | `"version": 1` only |
| Paths | Repo-relative to **asset root** (`--asset-root` / `engine.json`) |
| Parser | `Gfx_LoadSceneDesc` → `Gfx_SceneApply` |
| Example | [`Data/Scenes/demo.json`](../Data/Scenes/demo.json) |
| Task log | [`Archived/plans/scene-load_Plan.md`](Archived/plans/scene-load_Plan.md) |

CLI: `VulkanDesktop.exe --scene Data/Scenes/your_scene.json` — see [`CLI.md`](CLI.md).
