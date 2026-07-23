#pragma once

#include "../Gfx/Gfx_AoPass.h"

#include <array>

#include <glm/mat4x4.hpp>

#include "Vk_FrameLimits.h"

#include "Vk_Types.h"

struct VkCommandBuffer_T;

using VkCommandBuffer = VkCommandBuffer_T*;

class Vk_Renderer;

// Screen-space ambient occlusion — pluggable algorithms (Classic SSAO, HBAO+, GTAO).

struct Vk_AoState {
    Gfx_AoPass::PassState myGfx{};

    // Non-owning Vk mirrors (lifetime owned by Gfx PassState).
    VkPipeline myClassicPipeline = VK_NULL_HANDLE;

    VkPipeline myHbaoPipeline = VK_NULL_HANDLE;

    VkPipeline myGtaoPipeline = VK_NULL_HANDLE;

    VkPipeline myUpsamplePipeline = VK_NULL_HANDLE;

    VkPipeline myBlurPipeline = VK_NULL_HANDLE;

    VkPipelineLayout myClassicPipelineLayout = VK_NULL_HANDLE;

    VkPipelineLayout myHalfResPipelineLayout = VK_NULL_HANDLE;

    VkPipelineLayout myUpsamplePipelineLayout = VK_NULL_HANDLE;

    VkPipelineLayout myBlurPipelineLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout myClassicSetLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout myHalfResSetLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout myUpsampleSetLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout myBlurSetLayout = VK_NULL_HANDLE;

    VkDescriptorPool myDescriptorPool = VK_NULL_HANDLE;

    VkSampler myGBufferSampler = VK_NULL_HANDLE;

    Vk_TextureResource myAoRaw{};

    Vk_TextureResource myAoHalf{};

    Vk_TextureResource myBentNormalHalf{};

    Vk_TextureResource myAoBlur{};

    Vk_TextureResource myAoHistory[ 2 ]{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myClassicDescriptorSets{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myHalfResDescriptorSets{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myUpsampleDescriptorSets{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurHorizDescriptorSets{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBlurVertDescriptorSets{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myTemporalDescriptorSets{};

    VkPipeline myTemporalPipeline = VK_NULL_HANDLE;

    VkPipelineLayout myTemporalPipelineLayout = VK_NULL_HANDLE;

    VkDescriptorSetLayout myTemporalSetLayout = VK_NULL_HANDLE;

    uint32_t myTemporalReadIndex    = 0u;
    bool     myTemporalHistoryReady = false;

    bool myInitialized = false;
};

namespace Vk_AoPass {

void Init( Vk_Renderer& aCore );

void Destroy( Vk_Renderer& aCore );

void RecreateForExtent( Vk_Renderer& aCore );

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

VkImageView GetRawAoImageView( const Vk_Renderer& aCore );

VkImageView GetBentNormalHalfView( const Vk_Renderer& aCore );

void NoteRawAoLayout( VkImageLayout aLayout );

VkImageLayout GetRawAoLayout();

}  // namespace Vk_AoPass
