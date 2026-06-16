#pragma once

#include "Vk_DataStruct.h"
#include "Vk_DeviceContext.h"
#include "Vk_ResourceContext.h"

#include <string>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

// Low-level Vulkan device + resource factory (no passes, swapchain, or scene state).
class Vk_RhiDevice {
public:
    Vk_DeviceContext   myDeviceCtx;
    Vk_ResourceContext myResourceContext;

    void SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers );
    void SetRequiredExtension( std::vector< const char* > someDeviceExtensions );

    // Instance without GLFW (GfxTests / headless probe).
    bool CreateInstanceHeadless();

    void SyncResourceContext();
    void InitAllocator();

    void CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool aIsExclusive ) const;
    void CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                      Vk_AllocatedImage& anImage ) const;
    void CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                      VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;
    void CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                      VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;

    VkShaderModule CreateShaderModule( const std::vector< char >& someShaderCode ) const;

    VkImageView CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel = 1 ) const;

    void TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const;
    void CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const;
    void GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const;
    void CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    void CopyBufferOnGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;

    bool                  HasStencilComponent( VkFormat aFormat ) const;
    VkFormat              FindDepthFormat() const;
    VkFormat              FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const;
    VkSampleCountFlagBits GetMaxUsableSampleCount() const;
    size_t                PadUniformBufferSize( size_t anOriginalSize ) const;

    bool IsInstanceCreated() const {
        return myDeviceCtx.myInstance != VK_NULL_HANDLE;
    }

private:
    uint32_t FindMemoryType( uint32_t aTypeFilter, VkMemoryPropertyFlags someProperties ) const;
};
