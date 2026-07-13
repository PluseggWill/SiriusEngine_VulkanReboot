#pragma once

#include <array>
#include <cstdint>

#include "../Gfx/Gfx_FrameDebugToggles.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_RenderView.h"
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;
struct Gfx_FrameDebugToggles;

// Offscreen G-buffer pass state (extent-sized; recreated on swapchain resize when hybrid active).
struct Vk_GBufferState {
    Gfx_Texture      myAlbedo;
    Gfx_Texture      myNormalRoughness;
    Gfx_Texture      myWorldPosition;
    Gfx_Texture      myMotionVector;
    Gfx_Texture      myDepth;
    VkRenderPass     myRenderPass              = VK_NULL_HANDLE;
    VkFramebuffer    myFramebuffer             = VK_NULL_HANDLE;
    VkPipeline       myGBufferPipeline         = VK_NULL_HANDLE;
    VkPipeline       myGBufferPipelineBindless = VK_NULL_HANDLE;
    Vk_DeletionQueue myDeletionQueue;
    bool             myInitialized = false;
};

// HybridDeferred opaque path: offscreen G-buffer raster (resolve → Vk_DeferredLightingPass).
//
// Lifecycle:
//   Init / RecreateForExtent — extent-sized attachments + pipelines (uses scene batch/bindless pipeline layouts).
//   RecreatePipelines — pipelines only; call after Vk_GfxPipelineCache rebuilds scene layouts on swapchain recreate.
namespace Vk_GBufferPass {

bool IsActive( const Vk_Renderer& aCore );

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );
void RecreatePipelines( Vk_Renderer& aCore );

void RecordFrame( Vk_Renderer& aCore, const Gfx_FrameDebugToggles& aToggles, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,
                  const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets );

}  // namespace Vk_GBufferPass
