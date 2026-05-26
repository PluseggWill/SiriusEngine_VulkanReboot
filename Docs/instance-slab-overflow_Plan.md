# Plan: instance-slab-overflow

**Sprint:** S1 — CPU draw stream (hygiene)  
**Status:** Done (2026-05-26)  
**SprintPlan:** Fail-closed when draw count exceeds `VkDescriptorPolicy::kMaxInstanceSlabEntries`.

## Problem

`FillInstanceSlab` logged overflow but still wrote `min(drawCount, max)` entries and `RecordScenePass` ran — silent missing draws.

## Goals

1. `FillInstanceSlab` returns `false` on overflow; no partial slab write.
2. `DrawFrame` skips `RecordScenePass` when slab fill fails; ImGui pass still runs.
3. One-time warn log per run (in addition to error).

## Non-goals

- Growing slab dynamically.
- GPU validation of draw count.

## Files

| File | Change |
|------|--------|
| `VulkanDesktop/RenderCore/Vk_Core.h` / `.cpp` | `FillInstanceSlab` → `bool`; skip record |

## Steps

- [x] P1 — Return false on overflow; remove partial `std::min` write path
- [x] P2 — `DrawFrame` guard around `RecordScenePass`
- [x] P3 — Build + smoke (normal scene unchanged)

## Build / smoke-run

MSBuild Debug|x64; smoke 4s; demo scene still renders (2 draws ≪ 256 cap).
