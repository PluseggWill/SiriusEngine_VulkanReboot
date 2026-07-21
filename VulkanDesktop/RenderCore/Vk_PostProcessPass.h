#pragma once

#include <array>

#include "Vk_DataStruct.h"
#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

constexpr VkFormat kPostSceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

// HDR scene color + hybrid resolve RP/FB + tonemap/bloom (extent-sized; recreated on resize).
struct Vk_PostProcessState {
    Vk_TextureResource mySceneColor{};
    Vk_TextureResource myTaaResolved{};
    Vk_TextureResource myTaaHistory[ 2 ]{};
    uint32_t           myTaaHistoryWriteIndex = 0u;
    // False until at least one resolve has been copied into history (avoids sampling UNDEFINED on TAA toggle-on).
    bool               myTaaHistoryReady = false;
    Vk_TextureResource myBloomPing{};
    Vk_TextureResource myBloomPong{};

    VkRenderPass  myHybridRenderPass  = VK_NULL_HANDLE;
    VkFramebuffer myHybridFramebuffer = VK_NULL_HANDLE;
    VkSampler     mySceneSampler      = VK_NULL_HANDLE;

    VkPipeline            myTonemapPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      myTonemapPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myTonemapSetLayout      = VK_NULL_HANDLE;

    VkPipeline            myTaaResolvePipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      myTaaResolvePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myTaaResolveSetLayout      = VK_NULL_HANDLE;

    VkPipeline            myBloomThresholdPipeline       = VK_NULL_HANDLE;
    VkPipeline            myBloomBlurPipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      myBloomThresholdPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout      myBloomBlurPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myBloomThresholdSetLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myBloomBlurSetLayout           = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool               = VK_NULL_HANDLE;

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myTonemapDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myTaaResolveDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBloomThresholdDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBloomBlurHorizDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myBloomBlurVertDescriptorSets{};

    Vk_DeletionQueue myDeletionQueue;
    bool             myInitialized = false;
};

namespace Vk_PostProcessPass {

bool HasHybridResolve( const Vk_Renderer& aCore );

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

// Hybrid resolve RP ends with sceneColor in SHADER_READ_ONLY_OPTIMAL — keep CPU layout tracker in sync.
void MarkSceneColorShaderRead();

void RecordPost( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aImageIndex, uint32_t aFrameIndex );

}  // namespace Vk_PostProcessPass
