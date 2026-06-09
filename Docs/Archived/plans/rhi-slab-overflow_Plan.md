# Plan: rhi-slab-overflow (RHI-A1)

**Status:** Closed (2026-06-09)  
**Parent:** [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md) §RHI-A1 · [`Active-Plan.md`](Active-Plan.md) P1  
**Hardening:** #33

## Problem

`Vk_FrameDrawPrep::Build()` returns `false` on instance slab overflow, but `Vk_Core::PrepareFrameCpu()` ignores the return and sets `aOut.myOk = true`. `DrawFrameGpu` still records with stale/wrong dynamic offsets.

Prior S1 fix (`instance-slab-overflow`) guarded an older `DrawFrame` path; peel to `PrepareFrameCpu` / `DrawFrameGpu` left the gap.

## Non-goals

- Dynamic slab growth
- GfxTests overflow injection (optional; manual dev verify only)
- `EngineArchitecture.md` edits (no policy change)

## Touch list

| File | Change |
|------|--------|
| `VulkanDesktop/RenderCore/Vk_Core.cpp` | Check `Build()` return; abort prep |
| `VulkanDesktop/RenderCore/Vk_Core.h` | Once-per-session overflow log flag |
| `VulkanDesktop/RenderCore/Vk_FrameDrawPrep.cpp` | Verify only (no change expected) |

## Steps

1. In `PrepareFrameCpu` per-view loop: `const bool slabOk = myDrawPrep.Build(...)`.
2. On `!slabOk`: log once `[RESOURCE] PrepareFrameCpu aborted: instance slab overflow`; `aOut.myOk = false`; `return false` (after acquire; no fence reset).
3. Verify: `Verify-CI.ps1` + `Verify-Smoke.ps1`.

## Verification

| ID | Criterion |
|----|-----------|
| A1-V1 | `Scripts/Verify-CI.ps1` exit 0 |
| A1-V2 | `PrepareFrameCpu` checks `Build()` return |
| A1-V3 | Manual: lower `kMaxInstanceSlabEntries` → no draw that frame; no crash |

## Risks

- Low — `Application` already skips `DrawFrameGpu` when `PrepareFrameCpu` returns `false`.
