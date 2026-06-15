# Plan: gtao — Ground Truth Ambient Occlusion (roadmap)

**Status:** Open (roadmap — not kicked off)  
**Deps:** `hbao-plus_Plan.md` (modular `Vk_AoPass` + `Gfx_AoMethod`)

## Problem

HBAO+ v0 improves contact stability vs classic SSAO but lacks slice-based GTAO quality (thin geometry, multi-bounce approximation, industry parity with UE-style contact AO).

## Non-goals (v0)

- Bent normal output to G-buffer
- DDGI / S8 integration changes

## Target

Add `Gfx_AoMethod::Gtao` slot in existing modular chain:

```
Vk_AoPass (GTAO slices, optional Hi-Z horizon)
  → optional spatial denoise (reuse ShadowAoSoft bilateral or dedicated 4-tap cross)
  → ShadowAoSoft (unchanged)
  → Deferred (unchanged)
```

## Steps (when kicked off)

1. **Shader:** `Gtao.comp` — Jimenez slice integration; `#include "AoCommon.glsl"`; optional Hi-Z min-depth for step search
2. **Pass:** pipeline + descriptors in `Vk_AoPass`; half-res default + `AoUpsample.comp` reuse
3. **Settings:** `myGtaoSlices`, `myGtaoStepsPerSlice`, falloff/thickness in `Gfx_AoSettings`
4. **ImGui:** extend AO method combo; debug view AO-only already exists
5. **Temporal (backlog):** G-buffer velocity + history buffer + reproject pass (Wishlist S6 backlog)

## Touch list (estimate)

- `Shader/Gtao.comp`, `AoCommon.glsl` (falloff helpers)
- `RenderCore/Vk_AoPass.cpp` — fourth pipeline slot
- `Gfx_AoMethod.h`, `Gfx_AoSettings.h`, `Util_LightingPanel.cpp`
- Optional: `Vk_DepthPyramidPass` read in GTAO shader

## Verification

- `Verify-CI.ps1` exit 0
- G0-validation 120 frames
- Manual Sponza: compare HBAO+ vs GTAO at equal half-res; crevice stability, thin bars, camera-close detail
- Wishlist S6 line: SSAO or GTAO v0 — mark GTAO when accepted

## Risks

- Temporal GTAO deferred until motion vectors exist in G-buffer
- Tuning thickness/falloff scene-dependent — expose ImGui sliders early
