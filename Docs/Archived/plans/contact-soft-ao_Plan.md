# Plan: contact-soft-ao — Shadow/AO contact softening pass

**Status:** Closed (2026-06-16)  
**Progress:** [`contact-soft-ao_Progress.md`](contact-soft-ao_Progress.md)

## Problem

Classic SSAO blur plus live PCF sun shadow produced harsh blocky contact regions near geometry. Disabling AO still triggered validation errors (UNDEFINED image layouts). AO contrast was applied twice (SSAO compute + deferred).

## Non-goals

- Replacing SSAO core with GTAO/HBAO (follow-up: `hbao-plus_Plan.md`, `gtao_Plan.md`)
- Temporal AO

## Solution

New **ShadowAoSoft** FG pass: pack raw linear AO (R) + screen-space sun shadow (G) into RG8, depth-aware bilateral separable blur, deferred reads contact map when enabled.

Validation fixes: shadow depth layout when pass skipped; fallback 1x1 AO/contact textures; dual pack descriptor sets (AO on/off).

## Touch list

- `VulkanDesktop/Shader/ShadowAoPack.comp`, `ShadowAoBlur.comp`
- `VulkanDesktop/RenderCore/Vk_ShadowAoSoftPass.{h,cpp}`
- `VulkanDesktop/RenderCore/Vk_ShadowMapPass.cpp` — `CmdBarrierForDeferredRead`
- `VulkanDesktop/Shader/Ssao.comp`, `DeferredLighting.frag`, `Gfx_AoSettings.h`
- FG: `Vk_FrameGraphBuilder`, `Vk_Core`, ImGui lighting panel

## Verification

- `Verify-CI.ps1` exit 0
- `--validation --smoke-frames 120` → 0 `[ERROR] [VULKAN-VALIDATION]`
- Manual: toggle AO off with contact soft on — no validation spam
