#pragma once

#include <array>
#include <cstdint>

#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_RenderView.h"
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Core;
struct DebugUIState;

// Offscreen G-buffer + composite pass state (extent-sized; recreated on swapchain resize when hybrid active).
struct Vk_GBufferState {
    Gfx_Texture           myAlbedo;
    Gfx_Texture           myNormalRoughness;
    Gfx_Texture           myDepth;
    VkRenderPass          myRenderPass              = VK_NULL_HANDLE;
    VkFramebuffer         myFramebuffer             = VK_NULL_HANDLE;
    VkPipeline            myGBufferPipeline         = VK_NULL_HANDLE;
    VkPipelineLayout      myCompositePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myCompositeSetLayout      = VK_NULL_HANDLE;
    VkDescriptorPool      myCompositeDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       myCompositeDescriptorSet  = VK_NULL_HANDLE;
    VkPipeline            myCompositePipeline       = VK_NULL_HANDLE;
    VkSampler             myCompositeSampler        = VK_NULL_HANDLE;
    Vk_DeletionQueue      myDeletionQueue;
    bool                  myInitialized = false;
};

// FG v0 slice 1: offscreen G-buffer opaque + albedo composite to swapchain.
namespace Vk_GBufferPass {

bool IsActive( const Vk_Core& aCore );

void Init( Vk_Core& aCore );
void Destroy( Vk_Core& aCore );
void RecreateForExtent( Vk_Core& aCore );

void RecordFrame( Vk_Core& aCore, const DebugUIState& aDebugUI, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,
                  const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets );

}  // namespace Vk_GBufferPass
