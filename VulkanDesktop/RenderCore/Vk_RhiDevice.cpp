// Module: Vk_RhiDevice — Vulkan instance/device slice + resource factory (no scene passes).
#include "Vk_RhiDevice.h"

#include "../Util/Util_Logger.h"
#include "../Util/Util_ValidationLayers.h"

#include <stdexcept>

void Vk_RhiDevice::SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers ) {
    myDeviceCtx.myEnableValidationLayers = aEnableValidationLayers;
    myDeviceCtx.myValidationLayers       = std::move( someValidationLayers );
}

void Vk_RhiDevice::SetRequiredExtension( std::vector< const char* > someDeviceExtensions ) {
    myDeviceCtx.myDeviceExtensions = std::move( someDeviceExtensions );
}

bool Vk_RhiDevice::CreateInstanceHeadless() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Sirius GfxTests";
    appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, 1, 0, 0 );
    appInfo.pEngineName        = "Sirius Engine";
    appInfo.engineVersion      = VK_MAKE_API_VERSION( 0, 1, 0, 0 );
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                 = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo      = &appInfo;
    createInfo.enabledExtensionCount = 0;
    createInfo.enabledLayerCount     = 0;

    if ( myDeviceCtx.myEnableValidationLayers && UtilValidationLayers::AreLayersAvailable( myDeviceCtx.myValidationLayers ) ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myDeviceCtx.myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myDeviceCtx.myValidationLayers.data();
    }

    if ( vkCreateInstance( &createInfo, nullptr, &myDeviceCtx.myInstance ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "Vk_RhiDevice::CreateInstanceHeadless failed." );
        return false;
    }
    return true;
}

void Vk_RhiDevice::SyncResourceContext() {
    myResourceContext.Bind( myDeviceCtx.myDevice, myDeviceCtx.myAllocator, myDeviceCtx.myPhysicalDevice, myDeviceCtx.myGraphicsQueue, myDeviceCtx.myTransferQueue,
                            myDeviceCtx.myGraphicsCommandPool, myDeviceCtx.myTransferCommandPool, myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value_or( 0 ),
                            myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value_or( 0 ) );
}

void Vk_RhiDevice::InitAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = myDeviceCtx.myPhysicalDevice;
    allocatorInfo.device         = myDeviceCtx.myDevice;
    allocatorInfo.instance       = myDeviceCtx.myInstance;
    vmaCreateAllocator( &allocatorInfo, &myDeviceCtx.myAllocator );
    SyncResourceContext();

    myDeviceCtx.myDeletionQueue.pushFunction( [ this ]() { vmaDestroyAllocator( myDeviceCtx.myAllocator ); } );
}

void Vk_RhiDevice::CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool aIsExclusive ) const {
    myResourceContext.CreateBuffer( aSize, aBufferUsage, aMemoryUsage, aBuffer, aIsExclusive );
}

void Vk_RhiDevice::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                                Vk_AllocatedImage& anImage ) const {
    myResourceContext.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, 1, VK_SAMPLE_COUNT_1_BIT, anImage );
}

void Vk_RhiDevice::CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                                VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    myResourceContext.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_RhiDevice::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                                VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    myResourceContext.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

VkShaderModule Vk_RhiDevice::CreateShaderModule( const std::vector< char >& someShaderCode ) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = someShaderCode.size();
    createInfo.pCode    = reinterpret_cast< const uint32_t* >( someShaderCode.data() );

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if ( vkCreateShaderModule( myDeviceCtx.myDevice, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_RhiDevice::CreateShaderModule failed" );
    }
    return shaderModule;
}

VkImageView Vk_RhiDevice::CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const {
    return myResourceContext.CreateImageView( anImage, aFormat, anAspect, aMipLevel );
}

void Vk_RhiDevice::TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const {
    myResourceContext.TransitionImageLayout( aImage, aFormat, anOldLayout, aNewLayout, aMipLevel );
}

void Vk_RhiDevice::CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const {
    myResourceContext.CopyBufferToImage( aBuffer, aImage, aWidth, aHeight );
}

void Vk_RhiDevice::GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const {
    myResourceContext.GenerateMipmaps( aImage, aImageFormat, aTexWidth, aTexHeight, aMipLevel );
}

void Vk_RhiDevice::CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    myResourceContext.CopyBuffer( aSrcBuffer, aDstBuffer, aSize );
}

void Vk_RhiDevice::CopyBufferOnGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    myResourceContext.CopyBufferOnGraphicsQueue( aSrcBuffer, aDstBuffer, aSize );
}

bool Vk_RhiDevice::HasStencilComponent( VkFormat aFormat ) const {
    return aFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || aFormat == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat Vk_RhiDevice::FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const {
    for ( const VkFormat format : someCandidates ) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties( myDeviceCtx.myPhysicalDevice, format, &properties );
        if ( aTiling == VK_IMAGE_TILING_LINEAR && ( properties.linearTilingFeatures & someFeatures ) == someFeatures ) {
            return format;
        }
        if ( aTiling == VK_IMAGE_TILING_OPTIMAL && ( properties.optimalTilingFeatures & someFeatures ) == someFeatures ) {
            return format;
        }
    }
    throw std::runtime_error( "Vk_RhiDevice: failed to find supported format" );
}

VkFormat Vk_RhiDevice::FindDepthFormat() const {
    return FindSupportedFormat( { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL,
                                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
}

VkSampleCountFlagBits Vk_RhiDevice::GetMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    vkGetPhysicalDeviceProperties( myDeviceCtx.myPhysicalDevice, &physicalDeviceProperties );

    const VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if ( counts & VK_SAMPLE_COUNT_64_BIT )
        return VK_SAMPLE_COUNT_64_BIT;
    if ( counts & VK_SAMPLE_COUNT_32_BIT )
        return VK_SAMPLE_COUNT_32_BIT;
    if ( counts & VK_SAMPLE_COUNT_16_BIT )
        return VK_SAMPLE_COUNT_16_BIT;
    if ( counts & VK_SAMPLE_COUNT_8_BIT )
        return VK_SAMPLE_COUNT_8_BIT;
    if ( counts & VK_SAMPLE_COUNT_4_BIT )
        return VK_SAMPLE_COUNT_4_BIT;
    if ( counts & VK_SAMPLE_COUNT_2_BIT )
        return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

size_t Vk_RhiDevice::PadUniformBufferSize( size_t anOriginalSize ) const {
    const size_t minAlignment = static_cast< size_t >( myDeviceCtx.myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment );
    if ( minAlignment == 0 ) {
        return anOriginalSize;
    }
    return ( anOriginalSize + minAlignment - 1 ) & ~( minAlignment - 1 );
}

uint32_t Vk_RhiDevice::FindMemoryType( uint32_t aTypeFilter, VkMemoryPropertyFlags someProperties ) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties( myDeviceCtx.myPhysicalDevice, &memProperties );

    for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ ) {
        if ( ( aTypeFilter & ( 1 << i ) ) && ( memProperties.memoryTypes[ i ].propertyFlags & someProperties ) == someProperties ) {
            return i;
        }
    }
    throw std::runtime_error( "Vk_RhiDevice: failed to find suitable memory type" );
}
