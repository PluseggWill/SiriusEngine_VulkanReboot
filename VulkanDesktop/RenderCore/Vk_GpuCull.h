#pragma once

#include "Vk_FrameCpuPrepResult.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

struct Vk_GpuCullState {
    VkPipeline            myComputePipeline     = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool      = VK_NULL_HANDLE;
};

// P3 compute frustum cull: entity-record SSBO → per-slot indirect buffer (instanceCount = 0 when culled).
namespace Vk_GpuCull {

void Init( Vk_Renderer& aCore );

void CreateFrameBuffers( Vk_Renderer& aCore );

void RecordDispatches( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, const Vk_FrameCpuPrepResult& aPrep );

}  // namespace Vk_GpuCull
