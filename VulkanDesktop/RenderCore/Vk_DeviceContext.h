#pragma once

#include "Vk_Bindless.h"
#include "Vk_DataStruct.h"
#include "Vk_Enum.h"
#include "Vk_Types.h"

#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

// Device / queue / VMA slice — replaces Vk_Renderer friend access for init and resource factories.
struct Vk_DeviceContext {
    VkInstance       myInstance       = VK_NULL_HANDLE;
    VkPhysicalDevice myPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         myDevice         = VK_NULL_HANDLE;
    VkQueue          myGraphicsQueue  = VK_NULL_HANDLE;
    VkQueue          myPresentQueue   = VK_NULL_HANDLE;
    VkQueue          myTransferQueue  = VK_NULL_HANDLE;
    VkSurfaceKHR     mySurface        = VK_NULL_HANDLE;

    Vk_QueueFamilyIndices myQueueFamilyIndices{};

    // Filled in const Vk_Renderer::CheckDeviceSuitable during device pick.
    mutable VkPhysicalDeviceProperties myPhysicalDeviceProperties{};
    mutable VkPhysicalDeviceFeatures   myPhysicalDeviceFeatures{};

    VkCommandPool myGraphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool myTransferCommandPool = VK_NULL_HANDLE;

    VmaAllocator myAllocator = VK_NULL_HANDLE;

    Vk_DeletionQueue myDeletionQueue;

    std::vector< const char* > myValidationLayers;
    std::vector< const char* > myDeviceExtensions;
    bool                       myEnableValidationLayers = false;

    Vk_BindlessCapabilities myBindlessCaps{};
    Vk_RenderMaterialPath   myMaterialPath = Vk_RenderMaterialPath::Batch;

    // 1x1 RGBA uploaded at device init; bindless texture-array slots [sceneTextureCount, kMax) only (not scene texture 0).
    Gfx_Texture myBindlessDefaultTexture{};

    VkPipelineCache myPipelineCache = VK_NULL_HANDLE;
};
