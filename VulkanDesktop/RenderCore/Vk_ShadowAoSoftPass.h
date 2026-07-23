#pragma once

#include "../Gfx/Gfx_ShadowAoSoftPass.h"

#include <array>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

// Contact softening: pack linear AO (R) + screen-space sun shadow (G), bilateral blur, deferred read.
// Independent of AO algorithm (Vk_AoPass); uses fallback 1x1 textures when AO pass is skipped.
struct Vk_ShadowAoSoftState {
    Gfx_ShadowAoSoftPass::PassState myGfx{};

    // Non-owning Vk mirrors for descriptor allocation/updates (destroyed via Gfx PassState).
    VkPipeline            myPackPipeline       = VK_NULL_HANDLE;
    VkPipeline            myBlurPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      myPackPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout      myBlurPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myPackSetLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myBlurSetLayout      = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool     = VK_NULL_HANDLE;
    VkSampler             myGBufferSampler     = VK_NULL_HANDLE;

    Vk_TextureResource mySoftPing{};
    Vk_TextureResource mySoftPong{};
    Vk_TextureResource myFallbackAo{};       // 1x1 R8 white — pack slot when AO pass skipped/disabled
    Vk_TextureResource myFallbackContact{};  // 1x1 RG8 (1,1) — deferred when no valid contact map

    // Pre-built descriptor sets (no vkUpdateDescriptorSets during record).
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myPackDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myPackNoAoDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurHorizDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurVertDescriptorSets{};

    bool myInitialized = false;
};

namespace Vk_ShadowAoSoftPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, bool aAoPassRan );

// Deferred binding 13 (aoMap): RG8 contact map, raw R8 AO, or identity fallback.
VkImageView GetDeferredContactMapView( const Vk_Renderer& aCore );

}  // namespace Vk_ShadowAoSoftPass
