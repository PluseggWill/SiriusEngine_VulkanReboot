# Gfx test fixtures (P0)

GfxTests embeds the demo scene layout in `GfxTests_Main.cpp` (`PopulateDemoSceneSoA`) with the **overview** camera from `Data/Scenes/demo.json` (eye 10,8,10 → center 0,0,-4, FOV 50°, aspect 1600/1200).

Expected after extract → cull → sort → batch:

- `visibleDraws` = 9 (8 opaque + 1 transparent)
- `batchRuns` = 8 (7 opaque batch runs — two trees share mesh+material — plus 1 transparent)
