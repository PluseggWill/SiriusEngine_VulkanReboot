# Plan: temporal

**Status:** Active (Wishlist **S9**; gate **G5**)  
**Parent:** [`Wishlist.md`](Wishlist.md) §S9 · [`Active-Plan.md`](Active-Plan.md)  
**Covers:** Halton jitter + motion vectors + TAA v0 + temporal history convergence

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

1. **S9.1 — Temporal skeleton and jitter**
   - Add Halton jitter sequence and per-frame camera projection offset.
   - Define temporal history lifetime/reset contract (resize, scene swap, camera cut).
   - Add debug readout for current jitter index and reset reasons.
2. **S9.2 — Motion vectors (MV)**
   - Implement MV output in primary path (G-buffer or compute fallback).
   - Include camera + object motion support; define static-object behavior explicitly.
   - Add MV visualization mode and validation-friendly resource barriers.
3. **S9.3 — TAA v0 / v0.5**
   - Add TAA resolve with history blend + neighborhood clamp/rejection baseline.
   - v0.5: Catmull-Rom history, YCoCg variance clip, velocity-adaptive blend, light sharpen (ImGui: blend / γ / sharpen).
   - Wire ImGui/preset toggle: `Off` / `TAA`; ensure no regressions in resize/reload/recreate flows.
4. **S9.4 — Shared temporal consumers**
   - Route temporal AO / SSR history logic to shared MV and lifetime rules where practical.
   - Keep fallback paths documented when a pass cannot fully migrate in this sprint.
   - Re-check pass ordering and hazard transitions after migration.
5. **S9.5 — Gate close and docs**
   - Run verification suite, capture stress observations, and finalize closeout notes.
   - If accepted, mark plan closed and archive Plan+Progress per workflow.

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
