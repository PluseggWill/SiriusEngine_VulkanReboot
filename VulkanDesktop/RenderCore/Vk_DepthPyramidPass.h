#pragma once

#include <array>
#include <cstdint>

#include "../Gfx/Gfx_AoSettings.h"
#include "../Rhi/Rhi_Handles.h"
#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

constexpr uint32_t kHiZMaxMipLevels = 8u;

// Hi-Z min-depth pyramid (R32_SFLOAT mip chain) from G-buffer depth.
// Used by SSR Hi-Z march + deferred Hi-Z debug. Not GPU occlusion culling (Wishlist S16).
struct Vk_DepthPyramidState {
    // Rhi-owned (create via myGfxRhiDevice); Vk mirrors below for SSR/deferred consumers.
    Rhi_Pipeline                                                                          myRhiPipeline{};
    Rhi_PipelineLayout                                                                    myRhiLayout{};
    Rhi_DescriptorSetLayout                                                               myRhiSetLayout{};
    Rhi_DescriptorPool                                                                    myRhiPool{};
    Rhi_Sampler                                                                           myRhiDepthSampler{};
    Rhi_Sampler                                                                           myRhiPyramidSampler{};
    std::array< std::array< Rhi_DescriptorSet, kHiZMaxMipLevels >, MAX_FRAMES_IN_FLIGHT > myRhiSets{};

    VkPipeline            myComputePipeline     = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool      = VK_NULL_HANDLE;
    VkSampler             myDepthSampler        = VK_NULL_HANDLE;
    VkSampler             myPyramidSampler      = VK_NULL_HANDLE;

    Vk_TextureResource                          myPyramid{};
    std::array< VkImageView, kHiZMaxMipLevels > myMipViews{};
    uint32_t                                    myMipLevelCount = 0;

    std::array< std::array< VkDescriptorSet, kHiZMaxMipLevels >, MAX_FRAMES_IN_FLIGHT > myDescriptorSets{};

    bool myInitialized = false;
};

namespace Vk_DepthPyramidPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordBuild( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

VkImageView GetMipView( const Vk_Renderer& aCore, uint32_t aMipLevel );
uint32_t    GetMipLevelCount( const Vk_Renderer& aCore );

}  // namespace Vk_DepthPyramidPass
