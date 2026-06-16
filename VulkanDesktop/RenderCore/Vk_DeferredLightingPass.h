#pragma once

#include <array>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

struct Vk_DeferredLightingState {
    VkPipeline            myPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mySetLayout      = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool = VK_NULL_HANDLE;
    VkSampler             myGBufferSampler = VK_NULL_HANDLE;

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myDescriptorSets{};

    bool myInitialized = false;
};

// FG v0: fullscreen deferred resolve to swapchain (G-buffer textures + cluster SSBO from Vk_ClusterBuildPass).
namespace Vk_DeferredLightingPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordDraw( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_DeferredLightingPass
