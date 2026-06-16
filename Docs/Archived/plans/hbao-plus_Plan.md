# Plan: hbao-plus — Modular AO pass + HBAO+ v0

**Status:** Closed (2026-06-16)  
**Progress:** [`hbao-plus_Progress.md`](hbao-plus_Progress.md)

## Problem

Classic 16-tap SSAO shows blocky patches near surfaces. AO algorithms were hard-coded in `Vk_SsaoPass` with no swap path for GTAO/HBAO.

## Non-goals

- GTAO (see `gtao_Plan.md`)
- Temporal accumulation / motion vectors
- Hi-Z-guided horizon search (HBAO v1 backlog)

## Design

| Layer | Role |
|-------|------|
| `Gfx_AoMethod` | Enum + labels; add method = enum + shader + pipeline slot |
| `Vk_AoPass` | Owns `myAoRaw` (full R8); dispatches by `Gfx_AoSettings.myMethod` |
| `AoCommon.glsl` | Shared depth/view helpers for all AO shaders |
| `ShadowAoSoft` | Unchanged contract — reads `GetRawAoImageView()` |

**HBAO+ path:** half-res `HbaoPlus.comp` → `AoUpsample.comp` (depth bilateral) → `myAoRaw`.

## Touch list

- `Gfx/Gfx_AoMethod.h`, `Gfx_AoSettings.h`
- `RenderCore/Vk_AoPass.{h,cpp}` (replaces `Vk_SsaoPass`)
- `Shader/AoCommon.glsl`, `HbaoPlus.comp`, `AoUpsample.comp`
- `Util_LightingPanel.cpp` — method combo + HBAO tunables
- `CompileShader_Glslc.bat`, vcxproj

## Verification

- `Verify-CI.ps1` exit 0 (shader drift included)
- G0-validation smoke 120 frames → 0 errors
- ImGui: switch Classic SSAO ↔ HBAO+ at runtime

## Risks

- HBAO v0 uses view-space horizon scan without Hi-Z — may need radius tuning on large scenes
