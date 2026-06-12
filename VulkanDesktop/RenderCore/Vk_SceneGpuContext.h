#pragma once

#include "Vk_FrameDrawPrep.h"
#include "Vk_ResourceTables.h"
#include "Vk_Types.h"  // Gfx_RenderObject

#include <vector>
#include <vulkan/vulkan.h>

// Scene-scoped GPU: pipelines, descriptors, resource tables, draw prep outputs.
struct Vk_SceneGpuContext {
    VkDescriptorSetLayout myGlobalSetLayout           = VK_NULL_HANDLE;
    VkDescriptorSetLayout myMaterialSetLayout         = VK_NULL_HANDLE;
    VkDescriptorSetLayout myBindlessMaterialSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myObjectSetLayout           = VK_NULL_HANDLE;

    VkDescriptorPool myDescriptorPool = VK_NULL_HANDLE;

    VkPipelineLayout myPipelineLayout         = VK_NULL_HANDLE;
    VkPipelineLayout myBindlessPipelineLayout = VK_NULL_HANDLE;

    VkPipeline myBasicPipeline               = VK_NULL_HANDLE;
    VkPipeline myTransparentPipeline         = VK_NULL_HANDLE;
    VkPipeline myBasicPipelineBindless       = VK_NULL_HANDLE;
    VkPipeline myTransparentPipelineBindless = VK_NULL_HANDLE;
    // Forward lit in HybridDeferred resolve RP (depth LOAD; incompatible with myRenderPass pipelines).
    VkPipeline myBasicPipelineHybridResolve               = VK_NULL_HANDLE;
    VkPipeline myBasicPipelineBindlessHybridResolve       = VK_NULL_HANDLE;
    VkPipeline myTransparentPipelineHybridResolve         = VK_NULL_HANDLE;
    VkPipeline myTransparentPipelineBindlessHybridResolve = VK_NULL_HANDLE;

    VkDescriptorSet                   myBindlessDescriptorSet = VK_NULL_HANDLE;
    Vk_AllocatedBuffer                myMaterialTableBuffer;
    std::vector< Vk_AllocatedBuffer > myMaterialParamBuffers;
    std::vector< VkDescriptorSet >    myMaterialDescriptorSets;

    VkSampler myTextureSampler        = VK_NULL_HANDLE;
    uint32_t  myTextureImageMipLevels = 0;

    Vk_ResourceTables               myResourceTables;
    std::vector< Gfx_RenderObject > myRenderObjects;
    Vk_FrameDrawPrep                myDrawPrep;

    Vk_DeletionQueue mySceneDeletionQueue;
};
