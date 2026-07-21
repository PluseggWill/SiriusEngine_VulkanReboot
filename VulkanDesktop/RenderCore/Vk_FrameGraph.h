#pragma once

#include <array>
#include <cstdint>

#include "../Gfx/Gfx_FrameDebugToggles.h"
#include "../Gfx/Gfx_FramePlan.h"
#include "../Gfx/Gfx_PassId.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_RenderView.h"
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

// Historical name retained for call sites; ordinals match Gfx_PassId.
using Vk_FrameGraphPassId = Gfx_PassId;

struct Vk_FrameGraphContext {
    Vk_Renderer*                                                   myCore    = nullptr;
    const Gfx_FrameDebugToggles*                                   myToggles = nullptr;
    Gfx_PipelineEnableFlags                                        myEnable{};
    VkCommandBuffer                                                myCommandBuffer    = VK_NULL_HANDLE;
    uint32_t                                                       myImageIndex       = 0;
    uint32_t                                                       myFrameIndex       = 0;
    const std::array< VkViewport, kGfxMaxRenderViews >*            myViewports        = nullptr;
    const std::array< VkRect2D, kGfxMaxRenderViews >*              myScissors         = nullptr;
    const std::array< VkDescriptorSet, kGfxMaxRenderViews >*       myFrameDescriptors = nullptr;
    uint32_t                                                       myViewCount        = 0;
    const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >* myViewPackets      = nullptr;
};

namespace Vk_FrameGraph {

void Execute( Vk_FrameGraphContext& aCtx );

}  // namespace Vk_FrameGraph
