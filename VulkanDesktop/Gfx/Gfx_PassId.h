#pragma once

#include <cstdint>

// Logical pass ids for HybridDeferred FG / Gfx_RenderPipeline (no Vulkan).
// Ordinal values must stay aligned with historical Vk_FrameGraphPassId.
enum class Gfx_PassId : uint8_t { Shadow = 0, GBuffer, ClusterBuild, DepthPyramid, SSR, SSAO, DdgiProbeUpdate, ShadowAoSoft, DeferredTransparent, Post, Count };
