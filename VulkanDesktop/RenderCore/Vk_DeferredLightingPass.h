#pragma once

#include <array>

#include "Vk_Types.h"

constexpr uint32_t kDeferredLightingFramesInFlight = 2u;

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Core;

struct Vk_DeferredLightingState {
    VkPipeline            myPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mySetLayout      = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool = VK_NULL_HANDLE;
    VkSampler             myGBufferSampler = VK_NULL_HANDLE;

    std::array< VkDescriptorSet, kDeferredLightingFramesInFlight > myDescriptorSets{};

    bool myInitialized = false;
};

// FG v0: fullscreen deferred resolve to swapchain (G-buffer textures + cluster SSBO from Vk_ClusterBuildPass).
namespace Vk_DeferredLightingPass {

void Init( Vk_Core& aCore );
void Destroy( Vk_Core& aCore );
void RecreateForExtent( Vk_Core& aCore );

void RecordDraw( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_DeferredLightingPass
