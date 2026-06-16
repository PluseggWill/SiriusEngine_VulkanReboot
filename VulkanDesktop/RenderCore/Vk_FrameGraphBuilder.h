#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "../Gfx/Gfx_FrameDebugToggles.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_RenderView.h"
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

// FG v1 pass ids for HybridDeferred preset (topological order enforced by Vk_FrameGraphBuilder).
enum class Vk_FrameGraphPassId : uint8_t { Shadow = 0, GBuffer, ClusterBuild, DepthPyramid, SSAO, ShadowAoSoft, DeferredTransparent, Post, Count };

// Per-frame inputs shared by all FG record callbacks.
struct Vk_FrameGraphContext {
    Vk_Renderer*                                                   myCore             = nullptr;
    const Gfx_FrameDebugToggles*                                   myToggles          = nullptr;
    VkCommandBuffer                                                myCommandBuffer    = VK_NULL_HANDLE;
    uint32_t                                                       myImageIndex       = 0;
    uint32_t                                                       myFrameIndex       = 0;
    const std::array< VkViewport, kGfxMaxRenderViews >*            myViewports        = nullptr;
    const std::array< VkRect2D, kGfxMaxRenderViews >*              myScissors         = nullptr;
    const std::array< VkDescriptorSet, kGfxMaxRenderViews >*       myFrameDescriptors = nullptr;
    uint32_t                                                       myViewCount        = 0;
    const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >* myViewPackets      = nullptr;
};

namespace Vk_FrameGraphBuilder {

// Registers nodes + deps, topologically sorts enabled passes, invokes Record(ctx).
void RecordHybridDeferred( Vk_FrameGraphContext& aCtx );

}  // namespace Vk_FrameGraphBuilder
