#pragma once

#include <array>
#include <cstdint>

#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_RenderView.h"
#include "Vk_DataStruct.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
struct VkPipeline_T;
using VkPipeline = VkPipeline_T*;
class Vk_Core;
struct Gfx_FrameDebugToggles;

// Vk_ScenePasses: scene forward pass record (opaque then transparent in one render pass).
class Vk_ScenePasses {
public:
    // CONTRACT: see RecordScene in Vk_ScenePasses.cpp (Stage 1 forward sub-passes).
    // aDebugUI: render-debug pass skips (opaque/transparent); avoids Vk_Core::DebugUI() after friend removal.
    static void RecordScene( Vk_Core& aCore, const Gfx_FrameDebugToggles& aToggles, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,
                             const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                             const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                             const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets );

    // Stage 1 forward path — single swapchain render pass (opaque + transparent).
    static void RecordForwardLit( Vk_Core& aCore, const Gfx_FrameDebugToggles& aToggles, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,
                                  const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets );

    // Hybrid G-buffer opaque: aGBufferPipeline = batch or bindless MRT pipeline (see RecordPassDrawsFromPacket / M8).
    static void RecordOpaquePacketDraws( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, uint32_t aDrawBufferBaseIndex,
                                         VkBuffer aIndirectBuffer, bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels, VkPipeline aGBufferPipeline );

    // HybridDeferred transparent sub-pass; reads swapchain depth after G-buffer copy.
    static void RecordTransparentPacketDraws( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, uint32_t aDrawBufferBaseIndex,
                                              VkBuffer aIndirectBuffer, bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels );

    // HybridDeferred PiP: forward-lit overlay for views 1..N-1 inside the hybrid resolve render pass.
    static void RecordHybridPiPViews( Vk_Core& aCore, const Gfx_FrameDebugToggles& aToggles, VkCommandBuffer aCommandBuffer,
                                      const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                                      const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                                      const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets );

    static void RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
};
