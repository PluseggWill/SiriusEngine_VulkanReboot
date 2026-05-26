# Plan: descriptor-set1-verify

**Sprint:** S1 — CPU draw stream (submission)  
**Status:** Open  
**SprintPlan:** Verify descriptor policy (Set 0/1 + Set 2) — Set 1 remaining.

## Problem

Set 2 dynamic instance slab is proven (`descriptor-set2-verify`). Set 0 still binds **texture from material 0 only** at init (`CreateDescriptorSets`). Multi-material scenes cannot show distinct textures; M1 descriptor sign-off is blocked.

## Goals

1. Set 1 layout: material UBO and/or `COMBINED_IMAGE_SAMPLER` per `Vk_DescriptorPolicy.h` / `eVk_*` bindings.
2. Per-material descriptor sets (or pool + alloc per material id) written from `Vk_ResourceTables::GetTextureIdForMaterial`.
3. `RecordScenePass`: bind **Set 1 once per batch** (batch key already groups by `materialId`); Set 0 holds camera + env only (move texture off Set 0 or duplicate only for demo fallback — document choice).
4. `TriangleFrag_Lit.frag`: sample albedo from Set 1 binding; rebuild SPIR-V.
5. Demo scene: ≥2 entities with **different** `materialId` + manifest textures; validation layers clean.
6. Log once: Set 1 binds per frame ≤ batch count.

## Non-goals

- Bindless / descriptor indexing (see Bindless v0 sprint block).
- Per-material pipeline variants (single forward pipeline for demo).
- Set 2 / push changes (Set 2 done).

## Files

| File | Change |
|------|--------|
| `VulkanDesktop/Shader/TriangleFrag_Lit.frag` | Set 1 texture binding |
| `VulkanDesktop/RenderCore/Vk_Enum.h` | Material bindings |
| `VulkanDesktop/RenderCore/Vk_Core.cpp` | Set 1 layout, pool, writes, record bind |
| `VulkanDesktop/Gfx/Gfx_ResourceManifest.*` | Second material + texture for demo |
| `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md` | sync on completion |

## Steps

- [ ] P1 — Set 1 descriptor layout + pipeline layout (non-empty set 1)
- [ ] P2 — Allocate/update per-material descriptor sets from resource tables
- [ ] P3 — Shader + SPIR-V; remove Set 0 texture from hot path (or keep env-only on Set 0)
- [ ] P4 — RecordScenePass: `vkCmdBindDescriptorSets` Set 1 at batch start
- [ ] P5 — Demo manifest two materials; smoke + validation
- [ ] P6 — Archive sprint line; update S1 implementation notes + M1 acceptance

## Build / smoke-run

MSBuild Debug|x64; smoke 4s; two meshes show **different** textures; log batch count vs Set 1 bind count.
