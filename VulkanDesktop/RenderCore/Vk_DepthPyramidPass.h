#pragma once

#include <array>
#include <cstdint>

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_DepthPyramidPass.h"
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
    Gfx_DepthPyramidPass::PassState myGfx{};

    // Non-owning Vk mirrors for SSR / deferred descriptor updates (destroyed via Gfx PassState).
    VkPipeline myComputePipeline = VK_NULL_HANDLE;
    VkSampler  myDepthSampler    = VK_NULL_HANDLE;
    VkSampler  myPyramidSampler  = VK_NULL_HANDLE;

    Vk_TextureResource                          myPyramid{};
    std::array< VkImageView, kHiZMaxMipLevels > myMipViews{};
    uint32_t                                    myMipLevelCount = 0;

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
