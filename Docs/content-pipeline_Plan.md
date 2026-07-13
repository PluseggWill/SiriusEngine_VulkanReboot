# Plan: content-pipeline

**Status:** Planned (Wishlist **S10**; gate **G3**)  
**Parent:** [`Wishlist.md`](Wishlist.md) §S10 · [`Active-Plan.md`](Active-Plan.md)  
**Covers:** hardening #19, #24 + rich scene pack for S11+ dogfood

## Goal

Stop treating OBJ + hand-edited JSON as sufficient for meshlet milestones **and** for complex visual test scenes (particles / water / terrain / hair).

---

## A. MeshImport v0 (#19) — gate **G3**

**Landing:**

| Deliverable | Detail |
|-------------|--------|
| Offline CLI or MSBuild step | glTF (prefer) / OBJ → engine mesh blob (vertices, indices, bounds, optional LOD chain refs) |
| Scene JSON | References blob id, not raw OBJ path in hot path |
| Verify | Manifest checks blob hash + bounds |

**Unlocks:** Wishlist **S17** meshlets; preferred assets for S11–S15.

---

## B. Material hot reload (#24)

| Step | Detail |
|------|--------|
| B.1 | Separate **material descriptor pool** recreate from full `UnloadScene` |
| B.2 | ImGui or CLI: reload textures / material table → rebuild Set 1 only |
| B.3 | Document: mesh GPU buffers unchanged |

---

## C. Rich scene pack — **Bistro interior** (primary dogfood)

**Primary asset:** [Amazon Lumberyard Bistro](https://developer.nvidia.com/orca/amazon-lumberyard-bistro) (NVIDIA ORCA, CC-BY 4.0).

**v0 scope:** **interior only** (~1.0M tris). Exterior (~2.8M tris) → Backlog. Sponza stays light IBL/SSR/TAA sign-off; `stress.json` stays **CI smoke** (Bistro does **not** enter `Verify-Smoke`).

**Dependency:** §C dogfoods **§A MeshImport v0** (glTF → blob). Do **not** use the Sponza-style OBJ split path for Bistro.

**Import chain:**

```text
ORCA FBX  →  offline FBX2glTF / Blender (tool + version pinned in script)  →  glTF  →  MeshImport blob  →  Scene JSON
```

| Step | Detail |
|------|--------|
| C.1 | `Scripts/Fetch-BistroOrca.ps1` — ORCA download → `Data/Models/bistro/source/` (zip **not** in git) |
| C.2 | Offline convert: FBX → glTF; document pinned tool version in script header |
| C.3 | MeshImport CLI: glTF → per-material blobs + texture manifest (default **downscale 2k → 1k/512**; configurable) |
| C.4 | `Scripts/Generate-BistroScene.ps1` → `Data/Scenes/bistro_interior.json` |
| C.5 | `Config/engine.bistro.json`; axis remap + recenter like Sponza (engine **Z-up**); 1–2 camera spawn presets |
| C.6 | `Data/Models/bistro/CREDITS.md` (CC-BY 4.0 citation) |
| C.7 | Document dogfood path in `Docs/README.md` **Active now** / Active-Plan default benchmark after close |

**Material v0:** glTF metallic-roughness → `roughness` / `metallic` / `baseColor`; glass → `transparent: true` + `alphaMode: blend`; mask foliage → `alphaMode: mask`. Import reports transparent / mask counts.

**What Bistro dogfoods (S11+):** SSR/IBL indoors, multi-material density, glass transparent pass, TAA on high-frequency detail, Hi-Z occlusion (S16). **Not** primary for CSM / large outdoor (S13/S14) or water shoreline (S12) — those keep `stress.json` or future exterior subset.

---

## Non-goals

- Full DCC plugin
- Streaming / pak files
- ORCA zip or raw 2k texture set committed to git
- Baked lightmaps from Bistro FBX
- glTF transmission / clearcoat (→ S19)
- Auto mesh instancing in v0 (high draw count acceptable initially)
- Bistro exterior full scene in S10 (→ Backlog)

## Verification

- Change albedo PNG → see update without scene reload >5s
- MeshImport: reimport demo assets → identical draw counts
- `bistro_interior.json` + `engine.bistro.json`: HybridDeferred `LoadSceneResources` success; documented `entities` / `draws` / load time / rough VRAM in Progress closeout
- Glass materials in transparent pass; no new validation Errors on documented camera preset
- Dev load path documented; **CI unchanged** (`stress.json` + `engine.stress.json`)
