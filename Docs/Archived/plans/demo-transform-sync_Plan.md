# Plan: demo-transform-sync

**Sprint:** S1 — CPU draw stream (hygiene)  
**Status:** Done (2026-05-26)  
**SprintPlan:** Align SoA transform/bounds with instance slab before extract/cull.

## Problem

Demo Z-spin was applied only in `FillInstanceSlab` (`ComputeDemoModelMatrix`), while **extract/cull/sort** read static SoA transforms. Rendered mesh could diverge from cull bounds and opaque depth keys.

## Goals

1. Store per-slot **base** transforms at `InitDemoSceneEntities`.
2. Each frame **before extract**: apply `ComputeDemoModelMatrix` (or base when `ENABLE_ROTATE` false) via `Gfx_SceneSoA::SetTransform` (updates bounds).
3. `FillInstanceSlab` copies `GetTransform` only — no second spin path.

## Non-goals

- Moving spin to simulation module (S2/S8).
- Oriented world AABB (backlog).
- Removing `ComputeDemoModelMatrix` entirely (kept until gameplay writes transforms).

## Files

| File | Change |
|------|--------|
| `VulkanDesktop/RenderCore/Vk_Core.h` / `.cpp` | `myDemoBaseTransforms`, `ApplyDemoTransformAnimation`, slab fill |

## Steps

- [x] P1 — Base transforms at demo init
- [x] P2 — Pre-extract SoA update in `DrawFrame`
- [x] P3 — Slab uses SoA matrix; comment `ENABLE_ROTATE`
- [x] P4 — Build + smoke (both meshes visible, spin unchanged)

## Build / smoke-run

MSBuild Debug|x64; smoke 4s; cull log unchanged; meshes spin as before.
