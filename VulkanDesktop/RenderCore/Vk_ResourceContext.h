#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include "Vk_Types.h"

// Narrow GPU resource factory surface for load-time paths (meshes, textures, tables).
// Vk_Core fills this after device + VMA init; avoids friend access from Vk_ResourceTables.
// TODO(vk-core-peel): extend with CreateBuffer/Image/Copy/Transition/Mipmap from Vk_Core; Util_Loader + Gfx_Mesh take const Vk_ResourceContext&.
struct Vk_ResourceContext {
    VkDevice     myDevice     = VK_NULL_HANDLE;
    VmaAllocator myAllocator  = VK_NULL_HANDLE;
    VkPhysicalDevice myPhysicalDevice = VK_NULL_HANDLE;
    VkQueue myGraphicsQueue = VK_NULL_HANDLE;
    VkQueue myTransferQueue = VK_NULL_HANDLE;
    VkCommandPool myGraphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool myTransferCommandPool = VK_NULL_HANDLE;
    uint32_t myGraphicsQueueFamily = 0;
    uint32_t myTransferQueueFamily = 0;

    void Bind( VkDevice aDevice, VmaAllocator aAllocator, VkPhysicalDevice aPhysicalDevice, VkQueue aGraphicsQueue, VkQueue aTransferQueue,
               VkCommandPool aGraphicsCommandPool, VkCommandPool aTransferCommandPool, uint32_t aGraphicsQueueFamily, uint32_t aTransferQueueFamily );

    void CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool aIsExclusive ) const;
    void CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                      VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;
    void TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const;
    void CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const;
    void CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    void GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const;
    VkImageView CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel = 1 ) const;

private:
    VkCommandBuffer BeginSingleTimeCommands( VkCommandPool aCommandPool ) const;
    void            EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue ) const;
    static bool     HasStencilComponent( VkFormat aFormat );
};
