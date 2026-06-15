#pragma once

#include <array>
#include <cstdint>

#include "../Gfx/Gfx_AoSettings.h"
#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Core;

constexpr uint32_t kHiZMaxMipLevels = 8u;

// Hi-Z min-depth pyramid (R32_SFLOAT mip chain) built from G-buffer depth.
struct Vk_DepthPyramidState {
    VkPipeline            myComputePipeline     = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool      = VK_NULL_HANDLE;
    VkSampler             myDepthSampler        = VK_NULL_HANDLE;
    VkSampler             myPyramidSampler      = VK_NULL_HANDLE;

    Gfx_Texture                                 myPyramid{};
    std::array< VkImageView, kHiZMaxMipLevels > myMipViews{};
    uint32_t                                    myMipLevelCount = 0;

    std::array< std::array< VkDescriptorSet, kHiZMaxMipLevels >, MAX_FRAMES_IN_FLIGHT > myDescriptorSets{};

    bool myInitialized = false;
};

namespace Vk_DepthPyramidPass {

void Init( Vk_Core& aCore );
void Destroy( Vk_Core& aCore );
void RecreateForExtent( Vk_Core& aCore );

void RecordBuild( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

VkImageView GetMipView( const Vk_Core& aCore, uint32_t aMipLevel );
uint32_t    GetMipLevelCount( const Vk_Core& aCore );

}  // namespace Vk_DepthPyramidPass
