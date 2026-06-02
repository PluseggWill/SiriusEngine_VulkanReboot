# Plan: content-pipeline

**Status:** Planned (mostly **Wishlist-gated**)  
**Parent:** [`Wishlist.md`](Wishlist.md) + Active-Plan **P4**  
**Covers recommendations:** #19, #24

## Goal

Stop treating OBJ + hand-edited JSON as sufficient for meshlet/mesh-shader milestones.

---

## A. MeshImport v0 (#19) — gate for S4

**Landing:**

| Deliverable | Detail |
|-------------|--------|
| Offline CLI or MSBuild step | OBJ/glTF → engine mesh blob (vertices, indices, bounds, optional LOD chain refs) |
| Scene JSON | References blob id, not raw OBJ path in hot path |
| Verify | Manifest checks blob hash + bounds |

**Unlocks:** Wishlist S4 meshlets (see [`Wishlist.md`](Wishlist.md)).

---

## B. Material hot reload (#24) — post-M2 / pre-S7 presets

**Landing:**

| Step | Detail |
|------|--------|
| B.1 | Separate **material descriptor pool** recreate from full `UnloadScene` |
| B.2 | ImGui or CLI: reload textures / material table → rebuild Set 1 only |
| B.3 | Document: mesh GPU buffers unchanged |

**Why deferred:** HybridDeferred A/B needs preset toggle without process restart; not required for M2 parity.

---

## Non-goals

- Full DCC plugin
- Streaming / pak files

## Verification

- Change albedo PNG → see update without scene reload >5s
- MeshImport: reimport demo assets → identical draw counts
