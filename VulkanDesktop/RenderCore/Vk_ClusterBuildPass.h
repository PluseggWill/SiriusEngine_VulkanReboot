#pragma once

#include <array>

#include "Vk_Types.h"

// Must match MAX_FRAMES_IN_FLIGHT in Vk_Core.h (avoid circular include).
constexpr uint32_t kClusterBuildFramesInFlight = 2u;

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Core;

struct Vk_ClusterBuildState {
    VkPipeline            myComputePipeline     = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool      = VK_NULL_HANDLE;

    Vk_AllocatedBuffer myLightsBuffer{};
    void*              myLightsMapped = nullptr;

    std::array< Vk_AllocatedBuffer, kClusterBuildFramesInFlight > myClusterListBuffers{};
    std::array< VkDescriptorSet, kClusterBuildFramesInFlight >    myDescriptorSets{};

    uint32_t myClusterCount = 0;
    bool     myInitialized  = false;
};

// FG v0 slice 2: compute stub filling per-cluster light index lists (sun-only v1).
namespace Vk_ClusterBuildPass {

void Init( Vk_Core& aCore );
void Destroy( Vk_Core& aCore );
void RecreateForExtent( Vk_Core& aCore );

void RecordDispatch( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_ClusterBuildPass
