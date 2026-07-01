#pragma once

#include <array>

#include <glm/mat4x4.hpp>

#include "Vk_FrameLimits.h"

#include "Vk_Types.h"

struct VkCommandBuffer_T;

using VkCommandBuffer = VkCommandBuffer_T*;

class Vk_Renderer;

// Screen-space ambient occlusion — pluggable algorithms (Classic SSAO, HBAO+, GTAO).

// Contract: myAoRaw (R8 full-res, linear) is consumed by ShadowAoSoft and/or deferred resolve.

// Half-res methods (HBAO+, GTAO) write myAoHalf then reuse myUpsamplePipeline → myAoRaw.

struct Vk_AoState {

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

    Gfx_Texture myAoRaw{};  // full-res R8 — deferred / contact-soft input

    Gfx_Texture myAoHalf{};  // half-res R8 — HBAO+ / GTAO intermediate

    Gfx_Texture myBentNormalHalf{};  // half-res RG8 octahedral bent normal (GTAO only; deferred binding 18)

    Gfx_Texture myAoBlur{};  // classic SSAO separable blur ping-pong

    Gfx_Texture myAoHistory[ 2 ]{};  // temporal AO history ping-pong

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
    bool     myTemporalHistoryValid = false;

    glm::mat4 myPrevViewProj{ 1.0f };

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
