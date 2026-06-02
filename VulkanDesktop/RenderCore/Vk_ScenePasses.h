#pragma once

#include <cstdint>
#include <array>

#include "../Gfx/Gfx_RenderView.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "Vk_DataStruct.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
struct VkPipeline_T;
using VkPipeline = VkPipeline_T*;
class Vk_Core;

// Vk_ScenePasses: scene forward pass record (opaque then transparent in one render pass).
class Vk_ScenePasses {
public:
    // CONTRACT: see RecordScene in Vk_ScenePasses.cpp (Stage 1 forward sub-passes).
    static void RecordScene( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex, const std::array< VkViewport, kGfxMaxRenderViews >& aViewports,
                             const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                             const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                             const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets );
    static void RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    static void RecordDrawBatchesFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, const char* aPassName );
    static void RecordDrawBatchesBindlessFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, VkPipeline aPipeline,
                                                     const char* aPassName );
};
