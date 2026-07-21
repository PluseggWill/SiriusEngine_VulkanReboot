#pragma once

#include <array>
#include <glm/vec3.hpp>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

struct Vk_DeferredLightingState {
    VkPipeline            myPipeline                      = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout                = VK_NULL_HANDLE;
    VkDescriptorSetLayout mySetLayout                     = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool                = VK_NULL_HANDLE;
    VkSampler             myGBufferSampler                = VK_NULL_HANDLE;
    VkPipeline            myDdgiProbeUpdatePipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      myDdgiProbeUpdatePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDdgiProbeSetLayout            = VK_NULL_HANDLE;
    VkDescriptorPool      myDdgiProbeDescriptorPool       = VK_NULL_HANDLE;

    Vk_TextureResource myDdgiProbeIrradianceAtlas{};
    Vk_TextureResource myDdgiProbeVisibilityAtlas{};
    uint32_t           myDdgiProbeCountX     = 12u;
    uint32_t           myDdgiProbeCountY     = 8u;
    uint32_t           myDdgiProbeCountZ     = 12u;
    uint32_t           myDdgiTotalProbeCount = 0u;
    uint32_t           myDdgiUpdateCursor    = 0u;
    bool               myDdgiAtlasReadable   = false;
    glm::vec3          myDdgiPrevVolumeCenter{ 0.0f };
    glm::vec3          myDdgiPrevVolumeExtents{ 0.0f };
    glm::vec3          myDdgiPrevCameraEye{ 0.0f };
    bool               myDdgiHistoryForceReset = true;

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myDescriptorSets{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myDdgiProbeDescriptorSets{};

    bool myInitialized = false;
};

// FG v0: fullscreen deferred resolve to swapchain (G-buffer textures + cluster SSBO from Vk_ClusterBuildPass).
namespace Vk_DeferredLightingPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordDraw( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );
void RecordDdgiProbeUpdate( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_DeferredLightingPass
