# P4 — Vertical slice v0

**Status:** Closed (2026-06-11)  
**Landing:** Active-Plan § P4 · Gate **G2**

## Problem

Ship a minimal playable loop: dedicated scene with verified assets, one win/lose objective with feedback, and restart without exiting the process.

## Goals

1. `Data/Scenes/slice.json` — primary play/benchmark camp scene (demo asset subset).
2. Optional scene `objective` block → reach target within radius before time limit.
3. HUD + log tokens `[SLICE] Objective won` / `[SLICE] Objective lost`.
4. **R** key + Scene panel **Restart current** → in-process reload of current scene.

## Non-goals

- S8 AI, enemies, physics, save games.
- New art assets; reuse existing `Data/` models/textures.
- Slice smoke in `Verify-Smoke.ps1` (GfxTests manifest verify only).

## Touch list

| Area | Files |
|------|--------|
| Scene data | `Data/Scenes/slice.json`, `Config/engine.slice.json` |
| Scene parse | `Gfx_SceneDesc.h`, `Gfx_SceneLoader.cpp` |
| Objective runtime | `Gfx_ObjectiveRuntime.{h,cpp}` |
| App loop | `Application.{h,cpp}`, `DebugOverlay.cpp` |
| Scene UI | `Util_ScenePanel.cpp` |
| Tests | `GfxTests_Main.cpp` |
| Project | `VulkanDesktop.vcxproj`, `.filters` |

## Steps

- [x] 1. Add `Gfx_SceneObjectiveDesc` + JSON parse; create `slice.json` + `engine.slice.json`.
- [x] 2. `Gfx_ObjectiveRuntime` tick/HUD/log; wire Application + DebugOverlay.
- [x] 3. Restart: R key + Scene panel button; reset objective on reload.
- [x] 4. GfxTests: slice JSON + on-disk asset verify.
- [x] 5. `Verify-CI.ps1` + `Verify-Smoke.ps1`; close task docs.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` → exit 0 (includes GfxTests slice test).
- Manual: `--config Config/engine.slice.json --asset-root <repo>` → fly to campfire → `[SLICE] Objective won`; wait past limit on fresh load → `[SLICE] Objective lost`; **R** → `[APP] Scene reload completed` without process exit.

## Risks

- Objective target must match campfire entity translation in `slice.json` (documented in scene JSON).
