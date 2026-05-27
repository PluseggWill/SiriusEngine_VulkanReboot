#include "Vk_ResourceContext.h"

#include "Vk_Initializer.h"

#include <stdexcept>

void Vk_ResourceContext::Bind( VkDevice aDevice, VmaAllocator aAllocator ) {
    myDevice    = aDevice;
    myAllocator = aAllocator;
}

VkImageView Vk_ResourceContext::CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const {
    VkImageViewCreateInfo viewInfo = VkInit::ImageViewCreateInfo( aFormat, anImage, anAspect, aMipLevel );

    VkImageView imageView;
    if ( vkCreateImageView( myDevice, &viewInfo, nullptr, &imageView ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ResourceContext: failed to create image view" );
    }

    return imageView;
}
