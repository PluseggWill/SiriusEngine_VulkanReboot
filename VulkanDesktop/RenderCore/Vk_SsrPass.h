#pragma once

#include <array>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

struct Vk_SsrState {
    VkPipeline            myComputePipeline     = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool      = VK_NULL_HANDLE;
    VkSampler             myGBufferSampler      = VK_NULL_HANDLE;

    Gfx_Texture mySsrOutput{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myDescriptorSets{};

    bool myInitialized = false;
};

namespace Vk_SsrPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

VkImageView GetOutputImageView( const Vk_Renderer& aCore );

}  // namespace Vk_SsrPass
