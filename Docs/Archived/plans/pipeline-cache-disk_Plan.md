# Plan: pipeline-cache-disk

**Sprint:** S2 — Shader systems  
**Status:** Closed (2026-06-01)  
**Roadmap:** [`Active-Plan.md`](../../Active-Plan.md)

## Goals

1. Create `VkPipelineCache` at device init; pass to all `vkCreateGraphicsPipelines` in scene path.
2. Persist cache blob to `{assetRoot}/Cache/pipeline_cache_v1.bin` on shutdown.
3. Reload blob when header fingerprint matches (GPU `pipelineCacheUUID` + driver ids + active permutation SPIR-V mtimes).
4. Log load hit / miss / invalidate / save under `[PIPELINE-CACHE]`.

## Non-goals

- `VK_KHR_pipeline_binary` research; multi-scene shader paths beyond active permutation registry.
- Cold/warm benchmark runbook (S7).
- Compute pipeline cache.

## Design

| Piece | Choice |
|-------|--------|
| Module | `Vk_DevicePipelineCache` (distinct from `Vk_GfxPipelineCache` scene orchestration) |
| Disk path | `Cache/pipeline_cache_v1.bin` under `UtilEngineConfig::GetAssetRoot()` |
| Invalidate | Header mismatch → empty in-memory cache; stale file left until next successful save |
| Shader fingerprint | FNV-1a over registry + active perm vert/frag (+ bindless frag when bindless path) file mtimes |

## Touch list

| File | Change |
|------|--------|
| `RenderCore/Vk_DevicePipelineCache.h/.cpp` | New — create/load/save/destroy |
| `RenderCore/Vk_RenderDevice.cpp` | Call `Create` after logical device |
| `RenderCore/Vk_Core.h/.cpp` | `myPipelineCache`; save in `Clear` before `vkDestroyDevice` |
| `RenderCore/Vk_Pipeline.h/.cpp` | Pass cache into `vkCreateGraphicsPipelines` |
| `RenderCore/Vk_GfxPipelineCache.cpp` | Forward `aCore.myPipelineCache` |
| `VulkanDesktop.vcxproj` + `.filters` | New sources |
| `Cache/.gitkeep` | Directory anchor |
| `.gitignore` | Ignore `Cache/*.bin` |

## Verification

MSBuild Debug|x64; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6`; log `[PIPELINE-CACHE]` disk miss then hit on second run.
