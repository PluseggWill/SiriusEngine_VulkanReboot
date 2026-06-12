# Data/Scenes

Scene files for VulkanDesktop (JSON v1).

| File | Purpose |
|------|---------|
| [`demo.json`](demo.json) | Default Kenney camp + viking/monkey demo; **Stage 1 forward baseline** ([`forward-stage1.md`](../../Docs/forward-stage1.md) §1) |
| [`stress.json`](stress.json) | **Valley settlement** (~108 entities): grass meadow, north cliff + waterfall, river, stone bridge, east-bank longhouse, west forest; [`Config/engine.stress.json`](../../Config/engine.stress.json) |
| [`sponza.json`](sponza.json) | **McGuire Sponza** indoor benchmark (25 material splits); fetch [`Scripts/Fetch-SponzaMcGuire.ps1`](../../Scripts/Fetch-SponzaMcGuire.ps1) · [`Config/engine.sponza.json`](../../Config/engine.sponza.json) |
| [`smoke.json`](smoke.json) | Minimal load test (single viking entity) |

**Authoring:** [EN](../../Docs/SceneJSON.en.md) · [中文](../../Docs/SceneJSON.md)

**Run:**

```text
VulkanDesktop.exe --scene Data/Scenes/demo.json
VulkanDesktop.exe --scene Data/Scenes/stress.json --config Config/engine.stress.json
VulkanDesktop.exe --config Config/engine.sponza.json
VulkanDesktop.exe --scene Data/Scenes/smoke.json
```

Default scene path if `--scene` is omitted: `Data/Scenes/sponza.json` (`kGfxDefaultSceneLogicalPath`; requires [`Fetch-SponzaMcGuire.ps1`](../../Scripts/Fetch-SponzaMcGuire.ps1)). CI smoke still uses [`stress.json`](stress.json). Stage 1 golden baseline still uses [`demo.json`](demo.json).

### `stress.json` world layout (top-down, -Y = ahead of default fly camera)

Engine **Z-up**; walkable plane = **X-Y** at Z=0. Kenney meshes get R_x(-90°) in the generator.

```text
        N (-Y)  cliff row -9 + waterfall top row -10
              [C C W W C C C C C C C]
              . . . . R . . . . . .     river col 0
  west forest | | | | | | | | | | |  east: longhouse + tent
              . . . . . . . . . . .     bridge row -5 (deck raised)
              G G G G G G G G G G G     grass (one layer, no overlap)
        S (+Y)  spawn / default view
```

- **Ground:** `ground_grass` tiles (5 m grid)
- **Cliff / falls:** north rim; central `cliff_waterfall` + `cliff_waterfall_top`
- **River:** flows south from falls through col 0
- **Bridge:** stone span at row -4, crosses river east–west
- **House:** `viking_room` mesh on east bank (main hall)
- **Forest:** large trees (`tree_giant`, oak, pine) on west bank only

Regenerate: `Scripts/Generate-StressScene.ps1` · assets: `Scripts/Fetch-KenneyNatureSubset.ps1`
