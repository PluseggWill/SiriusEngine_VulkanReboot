#pragma once

#include "Vk_Bindless.h"
#include "Vk_DataStruct.h"
#include "Vk_Enum.h"
#include "Vk_Types.h"

#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

// Device / queue / VMA slice — replaces Vk_Core friend access for init and resource factories.
struct Vk_DeviceContext {
    VkInstance       myInstance       = VK_NULL_HANDLE;
    VkPhysicalDevice myPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         myDevice         = VK_NULL_HANDLE;
    VkQueue          myGraphicsQueue  = VK_NULL_HANDLE;
    VkQueue          myPresentQueue   = VK_NULL_HANDLE;
    VkQueue          myTransferQueue  = VK_NULL_HANDLE;
    VkSurfaceKHR     mySurface        = VK_NULL_HANDLE;

    Vk_QueueFamilyIndices myQueueFamilyIndices{};

    // Filled in const Vk_Core::CheckDeviceSuitable during device pick.
    mutable VkPhysicalDeviceProperties myPhysicalDeviceProperties{};
    mutable VkPhysicalDeviceFeatures   myPhysicalDeviceFeatures{};

    VkCommandPool myGraphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool myTransferCommandPool = VK_NULL_HANDLE;

    VmaAllocator myAllocator = VK_NULL_HANDLE;

    Vk_DeletionQueue myDeletionQueue;

    std::vector< const char* > myValidationLayers;
    std::vector< const char* > myDeviceExtensions;
    bool myEnableValidationLayers = false;

    Vk_BindlessCapabilities myBindlessCaps{};
    Vk_RenderMaterialPath   myMaterialPath = Vk_RenderMaterialPath::Batch;

    VkPipelineCache myPipelineCache = VK_NULL_HANDLE;
};
