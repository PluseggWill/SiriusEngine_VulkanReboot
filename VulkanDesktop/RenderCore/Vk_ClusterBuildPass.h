#pragma once

#include "../Gfx/Gfx_ClusterBuildPass.h"

#include <array>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

// FG v0: compute pass filling per-cluster light index lists (sun-only v1 stub).
struct Vk_ClusterBuildState {
    Gfx_ClusterBuildPass::PassState myGfx{};

    // Non-owning Vk mirrors for deferred descriptor updates (destroyed via Gfx PassState).
    VkPipeline            myComputePipeline     = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool      = VK_NULL_HANDLE;

    Vk_AllocatedBuffer myLightsBuffer{};
    void*              myLightsMapped = nullptr;

    std::array< Vk_AllocatedBuffer, MAX_FRAMES_IN_FLIGHT > myClusterListBuffers{};
    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT >    myDescriptorSets{};

    uint32_t myClusterCount = 0;
    bool     myInitialized  = false;
};

namespace Vk_ClusterBuildPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordDispatch( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_ClusterBuildPass
