#pragma once

#include <array>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Core;

// Screen-space ambient occlusion (compute + separable blur); R8_UNORM output sampled by deferred resolve.
struct Vk_SsaoState {
    VkPipeline            mySsaoPipeline       = VK_NULL_HANDLE;
    VkPipeline            myBlurPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      mySsaoPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout      myBlurPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mySsaoSetLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myBlurSetLayout      = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool     = VK_NULL_HANDLE;
    VkSampler             myGBufferSampler     = VK_NULL_HANDLE;

    Gfx_Texture myAoRaw{};
    Gfx_Texture myAoBlur{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > mySsaoDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurHorizDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurVertDescriptorSets{};

    bool myInitialized = false;
};

namespace Vk_SsaoPass {

void Init( Vk_Core& aCore );
void Destroy( Vk_Core& aCore );
void RecreateForExtent( Vk_Core& aCore );

void RecordCompute( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_SsaoPass
