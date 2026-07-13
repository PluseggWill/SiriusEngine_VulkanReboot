# Plan: shadow-clipz-vulkan — Align directional shadow Z with Vulkan viewport

**Status:** Closed (2026-07-13)  
**Progress:** [`shadow-clipz-vulkan_Progress.md`](shadow-clipz-vulkan_Progress.md)

## Problem

Directional shadow used GLM OpenGL clip Z `[-1,1]` while Vulkan viewport writes `z_fb = z_ndc`. Sample path applied OpenGL `clipZ * 0.5 + 0.5`. Project has hit this class of bug repeatedly.

## Goals

- Shadow `lightViewProj` outputs **Vulkan [0,1] clip Z** (reverse-Z: near≈1, far≈0); compare depth = clip Z.
- **Hardening:** Cursor rule + named Gfx/GLSL helpers + GfxTest viewport contract so write/sample cannot silently diverge again.
- Sweep live code/comments that still encode the wrong OpenGL Z remap / border-lit contract.
- User visual confirm (done 2026-07-13).

## Non-goals

- Global `GLM_FORCE_DEPTH_ZERO_TO_ONE` / main-camera perspective ZO migration (still dual convention; documented)
- CSM / cascades / Contact-soft redesign
- Rewriting `Docs/Archived/**` history prose

## Design

| Decision | Choice |
|----------|--------|
| ZO matrix | `Gfx_MakeVulkanOrthoReverseZ` → `orthoRH_ZO(far,near)` + Y flip |
| Compare / FB depth | `Gfx_ClipZToFramebufferDepth` (= identity under viewport [0,1]) |
| Shader | `ClipDepth.glsl` — XY→UV vs Z→depth split |
| Border | reverse-Z `GREATER_OR_EQUAL` → `OPAQUE_BLACK` (OOB → lit) |
| Agent guard | `.cursor/rules/vulkan-clip-depth.mdc` |

## Touch list

- `VulkanDesktop/Gfx/Gfx_LightingMath.h`
- `VulkanDesktop/Shader/ClipDepth.glsl` (new), `PbrShadow.glsl`
- `VulkanDesktop/GfxTests/GfxTests_Main.cpp`
- `VulkanDesktop/RenderCore/Vk_ShadowMapPass.cpp`, `Vk_Camera.cpp`
- `VulkanDesktop/Shader/AoCommon.glsl` (comment)
- `.cursor/rules/vulkan-clip-depth.mdc`
- `Docs/EngineArchitecture.md` (locked shadow Z contract one-liner)
- regenerated SPVs consuming `PbrShadow.glsl`

## Steps

1. [x] Shadow matrix ZO + identity compare
2. [x] `PbrShadow.glsl` drop OpenGL Z remap
3. [x] GfxTests reverse-Z ZO samples
4. [x] User visual sign-off
5. [x] Cursor rule `vulkan-clip-depth.mdc`
6. [x] Gfx helpers + `ClipDepth.glsl`; wire `PbrShadow`
7. [x] GfxTest: Vulkan viewport `z_fb` == compare helper (anti OpenGL remap)
8. [x] Sweep residuals (border, comments, Architecture); Verify-CI

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- Manual Sponza / HybridDeferred shadows signed off

## Risks

- Bias may still need retune.
- Main camera remains OpenGL-style Z until a dedicated ZO migration.
