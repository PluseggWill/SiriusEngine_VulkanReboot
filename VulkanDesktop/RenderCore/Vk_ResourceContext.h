#pragma once

#include "Vk_Types.h"
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

// Narrow GPU resource factory surface for load-time paths (meshes, textures, tables).
// Vk_Renderer fills this after device + VMA init; single owner for upload/barrier/alloc helpers.
//
// Scene upload batch (RHI-C2): call BeginSceneUploadBatch before mesh loads, EndSceneUploadBatch after.
// Only mesh CopyBuffer (transfer queue) is batched into one submit + wait. Image layout transitions,
// CopyBufferToImage, and GenerateMipmaps always submit immediately — textures must load before the batch.
// DestroyStagingBuffer defers staging VMA frees until EndSceneUploadBatch when a batch is active.
struct Vk_ResourceContext {
    VkDevice         myDevice              = VK_NULL_HANDLE;
    VmaAllocator     myAllocator           = VK_NULL_HANDLE;
    VkPhysicalDevice myPhysicalDevice      = VK_NULL_HANDLE;
    VkQueue          myGraphicsQueue       = VK_NULL_HANDLE;
    VkQueue          myTransferQueue       = VK_NULL_HANDLE;
    VkCommandPool    myGraphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool    myTransferCommandPool = VK_NULL_HANDLE;
    uint32_t         myGraphicsQueueFamily = 0;
    uint32_t         myTransferQueueFamily = 0;

    void Bind( VkDevice aDevice, VmaAllocator aAllocator, VkPhysicalDevice aPhysicalDevice, VkQueue aGraphicsQueue, VkQueue aTransferQueue, VkCommandPool aGraphicsCommandPool,
               VkCommandPool aTransferCommandPool, uint32_t aGraphicsQueueFamily, uint32_t aTransferQueueFamily );

    void BeginSceneUploadBatch() const;
    void EndSceneUploadBatch() const;

    void CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool aIsExclusive ) const;
    void CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                      VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;
    void CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                      VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;
    void CreateCubemapImage( VkExtent2D anExtent, VkFormat aFormat, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                             Vk_AllocatedImage& anImage ) const;
    void TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const;
    void TransitionCubemapLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const;
    void CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const;
    void CopyBufferToCubemapFace( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight, uint32_t aFaceIndex, uint32_t aMipLevel = 0 ) const;
    void CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    // Staging teardown after CopyBuffer; deferred until EndSceneUploadBatch when batch active.
    void        DestroyStagingBuffer( Vk_AllocatedBuffer& aBuffer ) const;
    void        CopyBufferOnGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    void        GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const;
    void        GenerateCubemapMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aFaceWidth, int32_t aFaceHeight, uint32_t aMipLevel ) const;
    VkImageView CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel = 1 ) const;
    VkImageView CreateCubemapImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const;

private:
    struct UploadBatchState {
        bool                              myActive                = false;
        VkCommandBuffer                   myTransferCommandBuffer = VK_NULL_HANDLE;
        bool                              myTransferRecording     = false;
        std::vector< Vk_AllocatedBuffer > myPendingStagingDestroys{};
    };

    mutable UploadBatchState myUploadBatch{};

    VkCommandBuffer BeginSingleTimeCommands( VkCommandPool aCommandPool, bool aDeferTransferCopyToBatch ) const;
    void            EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue, bool aDeferTransferCopyToBatch ) const;
    static bool     HasStencilComponent( VkFormat aFormat );
};
