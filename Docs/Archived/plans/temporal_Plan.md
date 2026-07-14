# Plan: temporal

**Status:** Closed (2026-07-14)  
**Parent:** [`Wishlist.md`](../Wishlist.md) §S9 · [`Active-Plan.md`](../Active-Plan.md)  
**Covers:** Halton jitter + motion vectors + TAA v0.5 + temporal history convergence

## Problem

Current frame-to-frame stability is not strong enough for high-frequency geometry and post effects. We need a shared temporal foundation so TAA and history-based consumers (SSR/AO now, particles/hair later) use one consistent motion model.

## Non-goals

- Motion blur / cinematic post pipeline finalization (S20)
- Production-grade TAA sharpening or advanced reconstruction variants
- Complete rewrite of post stack ordering
- Asset/content pipeline changes (S10 scope)

## Touch list

- `VulkanDesktop` camera jitter injection and projection update path
- G-buffer / compute motion vector production + debug visualization
- TAA resolve pass, history management, disocclusion/rejection logic
- Shared temporal utilities consumed by SSR/AO where feasible
- ImGui toggles/presets and developer diagnostics
- Docs closeout artifacts (`temporal_Progress.md`, Active now pointer, archive on close)

## Ordered tasks

1. **S9.1 — Temporal skeleton and jitter** ✓
2. **S9.2 — Motion vectors (MV)** ✓
3. **S9.3 — TAA v0 / v0.5** ✓
4. **S9.4 — Shared temporal consumers** ✓
5. **S9.5 — Gate close and docs** ✓

### S9.4 consumer contract

| Consumer | Motion | Lifetime |
|----------|--------|----------|
| **TAA** | G-buffer MV | `Vk_TemporalState::myHistoryValid` ∧ `myTaaHistoryReady` |
| **AO temporal** | G-buffer MV (`prevUv = uv - mv`) | shared valid ∧ `myTemporalHistoryReady` |
| **SSR** | Hit-world × shared `prevViewProj` (surface MV wrong for radiance history) | shared valid ∧ `myHistoryReady` |

Shared reset (resize / scene swap / camera cut / manual) clears all three “buffer ready” flags. Pass-owned history textures remain local.

## Verification

- **Verify-CI:** `powershell -File Scripts/Verify-CI.ps1` (required)
- **Verify-Smoke:** `powershell -File Scripts/Verify-Smoke.ps1` (required for render path changes)
- **G0-validation:** Required (no new validation Errors on representative cameras)
- **G5 acceptance:** Camera motion aliasing visibly reduced; no new validation Errors; stress smoke green

## MV convention (S9.2)

- **Texture**: `GBuffer.myMotionVector` (`RG16F`)
- **Definition**: \(mv = currUv - prevUv\) where `uv = clip.xy / clip.w * 0.5 + 0.5`
- **Units**: normalized UV in \([0,1]\) (not pixels). Debug view uses bipolar `mv * 32 + 0.5` (gray = stationary).
- **Sign**: `+x` = moving right on screen; `+y` = moving down (matches Vulkan NDC Y flip and screen-space UV).
- **Production**: Vertex outputs `currClip`/`prevClip`; fragment reconstructs UV after perspective-correct interpolate (do not lerp UV-space MV). Camera UBO remains VERTEX-only.
- **Invalidation**: on resize / scene swap / camera cut / manual reset → history invalid and `prevViewProj = currViewProj` (MV becomes ~0).

## Risks and mitigations

- **Ghosting on disocclusion:** start with conservative rejection/clamp, expose tunables in ImGui.
- **MV correctness drift across passes:** keep one shared MV convention and add debug overlays early.
- **Barrier/layout regressions:** apply existing render-pass/barrier rules and verify with validation enabled.
- **Scope creep into cinematic post:** keep motion blur and filmic polish explicitly out of S9.
