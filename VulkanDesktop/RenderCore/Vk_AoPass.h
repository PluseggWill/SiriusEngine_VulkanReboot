#pragma once

#include <array>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Core;

// Screen-space ambient occlusion — pluggable algorithms (Classic SSAO, HBAO+).
// Contract: myAoRaw (R8 full-res, linear) is consumed by ShadowAoSoft and/or deferred resolve.
struct Vk_AoState {
    VkPipeline myClassicPipeline   = VK_NULL_HANDLE;
    VkPipeline myHbaoPipeline      = VK_NULL_HANDLE;
    VkPipeline myUpsamplePipeline  = VK_NULL_HANDLE;
    VkPipeline myBlurPipeline      = VK_NULL_HANDLE;

    VkPipelineLayout myClassicPipelineLayout  = VK_NULL_HANDLE;
    VkPipelineLayout myHbaoPipelineLayout     = VK_NULL_HANDLE;
    VkPipelineLayout myUpsamplePipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout myBlurPipelineLayout     = VK_NULL_HANDLE;

    VkDescriptorSetLayout myClassicSetLayout   = VK_NULL_HANDLE;
    VkDescriptorSetLayout myHbaoSetLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myUpsampleSetLayout  = VK_NULL_HANDLE;
    VkDescriptorSetLayout myBlurSetLayout      = VK_NULL_HANDLE;

    VkDescriptorPool myDescriptorPool = VK_NULL_HANDLE;
    VkSampler        myGBufferSampler = VK_NULL_HANDLE;

    Gfx_Texture myAoRaw{};   // full-res R8 — deferred / contact-soft input
    Gfx_Texture myAoHalf{};  // half-res R8 — HBAO+ intermediate
    Gfx_Texture myAoBlur{};  // classic SSAO separable blur ping-pong

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myClassicDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myHbaoDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myUpsampleDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurHorizDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurVertDescriptorSets{};

    bool myInitialized = false;
};

namespace Vk_AoPass {

void Init( Vk_Core& aCore );
void Destroy( Vk_Core& aCore );
void RecreateForExtent( Vk_Core& aCore );

void RecordCompute( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

// Raw linear AO (full-res R8) for ShadowAoSoft pack and deferred fallback binding.
VkImageView GetRawAoImageView( const Vk_Core& aCore );

}  // namespace Vk_AoPass
