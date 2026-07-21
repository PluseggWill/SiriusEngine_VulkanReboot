#include "Vk_ResourceContext.h"

#include "../Util/Util_Logger.h"
#include "Vk_Initializer.h"

#include <array>
#include <chrono>
#include <stdexcept>

void Vk_ResourceContext::Bind( VkDevice aDevice, VmaAllocator aAllocator, VkPhysicalDevice aPhysicalDevice, VkQueue aGraphicsQueue, VkQueue aTransferQueue,
                               VkCommandPool aGraphicsCommandPool, VkCommandPool aTransferCommandPool, uint32_t aGraphicsQueueFamily, uint32_t aTransferQueueFamily ) {
    myDevice              = aDevice;
    myAllocator           = aAllocator;
    myPhysicalDevice      = aPhysicalDevice;
    myGraphicsQueue       = aGraphicsQueue;
    myTransferQueue       = aTransferQueue;
    myGraphicsCommandPool = aGraphicsCommandPool;
    myTransferCommandPool = aTransferCommandPool;
    myGraphicsQueueFamily = aGraphicsQueueFamily;
    myTransferQueueFamily = aTransferQueueFamily;
}

void Vk_ResourceContext::BeginSceneUploadBatch() const {
    if ( myUploadBatch.myActive ) {
        throw std::runtime_error( "Vk_ResourceContext: nested BeginSceneUploadBatch" );
    }
    myUploadBatch          = UploadBatchState{};
    myUploadBatch.myActive = true;
}

void Vk_ResourceContext::EndSceneUploadBatch() const {
    if ( !myUploadBatch.myActive ) {
        return;
    }

    const auto waitStart = std::chrono::steady_clock::now();

    if ( myUploadBatch.myTransferRecording ) {
        vkEndCommandBuffer( myUploadBatch.myTransferCommandBuffer );

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &myUploadBatch.myTransferCommandBuffer;
        vkQueueSubmit( myTransferQueue, 1, &submitInfo, VK_NULL_HANDLE );
        vkQueueWaitIdle( myTransferQueue );
        vkFreeCommandBuffers( myDevice, myTransferCommandPool, 1, &myUploadBatch.myTransferCommandBuffer );
    }

    for ( const Vk_AllocatedBuffer& staging : myUploadBatch.myPendingStagingDestroys ) {
        vmaDestroyBuffer( myAllocator, staging.myBuffer, staging.myAllocation );
    }

    const auto   waitEnd = std::chrono::steady_clock::now();
    const double waitMs  = std::chrono::duration< double, std::milli >( waitEnd - waitStart ).count();
    UtilLogger::Info( "RESOURCE", "Scene upload batch waitMs=" + std::to_string( waitMs ) );

    myUploadBatch = UploadBatchState{};
}

void Vk_ResourceContext::CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer,
                                       bool aIsExclusive ) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = aSize;
    bufferInfo.usage = aBufferUsage;

    if ( aIsExclusive || myGraphicsQueueFamily == myTransferQueueFamily ) {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else {
        const uint32_t queueFamilyIndices[] = { myGraphicsQueueFamily, myTransferQueueFamily };
        bufferInfo.sharingMode              = VK_SHARING_MODE_CONCURRENT;
        bufferInfo.queueFamilyIndexCount    = 2;
        bufferInfo.pQueueFamilyIndices      = queueFamilyIndices;
    }

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = aMemoryUsage;

    if ( vmaCreateBuffer( myAllocator, &bufferInfo, &vmaAllocInfo, &aBuffer.myBuffer, &aBuffer.myAllocation, nullptr ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create buffer through VMA!" );
    }
}

void Vk_ResourceContext::CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                                      uint32_t aMipLevel, VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    const VkExtent3D extent = { anExtent.width, anExtent.height, 1 };
    CreateImage( extent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_ResourceContext::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                                      uint32_t aMipLevel, VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    VkImageCreateInfo imageInfo = VkInit::ImageCreateInfo( aFormat, anImageUsage, anExtent );
    imageInfo.mipLevels         = aMipLevel;
    imageInfo.tiling            = aTiling;
    imageInfo.samples           = aNumSamples;
    imageInfo.flags             = 0;
    std::array< uint32_t, 2 > queueFamilyIndices{};
    VkInit::FillImageSharingMode( myGraphicsQueueFamily, myTransferQueueFamily, queueFamilyIndices, imageInfo );

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = aMemoryUsage;

    if ( vmaCreateImage( myAllocator, &imageInfo, &vmaAllocInfo, &anImage.myImage, &anImage.myAllocation, nullptr ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create image through VMA!" );
    }
}

void Vk_ResourceContext::CreateCubemapImage( VkExtent2D anExtent, VkFormat aFormat, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                                             Vk_AllocatedImage& anImage ) const {
    VkImageCreateInfo imageInfo = VkInit::ImageCreateInfo( aFormat, anImageUsage, { anExtent.width, anExtent.height, 1 } );
    imageInfo.mipLevels         = aMipLevel;
    imageInfo.arrayLayers       = 6;
    imageInfo.flags             = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    std::array< uint32_t, 2 > queueFamilyIndices{};
    VkInit::FillImageSharingMode( myGraphicsQueueFamily, myTransferQueueFamily, queueFamilyIndices, imageInfo );

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = aMemoryUsage;

    if ( vmaCreateImage( myAllocator, &imageInfo, &vmaAllocInfo, &anImage.myImage, &anImage.myAllocation, nullptr ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ResourceContext: failed to create cubemap image through VMA" );
    }
}

void Vk_ResourceContext::TransitionCubemapLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool, false );

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = anOldLayout;
    barrier.newLayout                       = aNewLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = aImage;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = aMipLevel;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if ( anOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && aNewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage              = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if ( anOldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && aNewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage              = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument( "Vk_ResourceContext: unsupported cubemap layout transition" );
    }

    vkCmdPipelineBarrier( commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );
    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue, false );
}

void Vk_ResourceContext::TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool, false );

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = anOldLayout;
    barrier.newLayout                       = aNewLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = aImage;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = aMipLevel;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    if ( aNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if ( HasStencilComponent( aFormat ) ) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if ( anOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && aNewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage              = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if ( anOldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && aNewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage              = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if ( anOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && aNewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        // Discard contents; used for feature atlases sampled when the producer pass is skipped.
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage              = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage              = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if ( anOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && aNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage              = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage              = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else {
        throw std::invalid_argument( "unsupported layout transition!" );
    }

    vkCmdPipelineBarrier( commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );
    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue, false );
}

void Vk_ResourceContext::CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const {
    VkCommandBuffer   commandBuffer = BeginSingleTimeCommands( myTransferCommandPool, false );
    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = { 0, 0, 0 };
    region.imageExtent                     = { aWidth, aHeight, 1 };

    vkCmdCopyBufferToImage( commandBuffer, aBuffer, aImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
    EndSingleTimeCommands( commandBuffer, myTransferCommandPool, myTransferQueue, false );
}

void Vk_ResourceContext::CopyBufferToCubemapFace( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight, uint32_t aFaceIndex, uint32_t aMipLevel ) const {
    VkCommandBuffer   commandBuffer = BeginSingleTimeCommands( myTransferCommandPool, false );
    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = aMipLevel;
    region.imageSubresource.baseArrayLayer = aFaceIndex;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = { 0, 0, 0 };
    region.imageExtent                     = { aWidth, aHeight, 1 };

    vkCmdCopyBufferToImage( commandBuffer, aBuffer, aImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
    EndSingleTimeCommands( commandBuffer, myTransferCommandPool, myTransferQueue, false );
}

void Vk_ResourceContext::CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myTransferCommandPool, true );
    VkBufferCopy    copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = aSize;
    vkCmdCopyBuffer( commandBuffer, aSrcBuffer, aDstBuffer, 1, &copyRegion );
    EndSingleTimeCommands( commandBuffer, myTransferCommandPool, myTransferQueue, true );
}

void Vk_ResourceContext::DestroyStagingBuffer( Vk_AllocatedBuffer& aBuffer ) const {
    if ( aBuffer.myBuffer == VK_NULL_HANDLE ) {
        return;
    }
    if ( myUploadBatch.myActive ) {
        myUploadBatch.myPendingStagingDestroys.push_back( aBuffer );
        aBuffer = {};
        return;
    }
    vmaDestroyBuffer( myAllocator, aBuffer.myBuffer, aBuffer.myAllocation );
    aBuffer = {};
}

void Vk_ResourceContext::CopyBufferOnGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool, false );
    VkBufferCopy    copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = aSize;
    vkCmdCopyBuffer( commandBuffer, aSrcBuffer, aDstBuffer, 1, &copyRegion );
    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue, false );
}

void Vk_ResourceContext::GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const {
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties( myPhysicalDevice, aImageFormat, &formatProperties );
    if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) ) {
        throw std::runtime_error( "texture image does not support linear blitting!" );
    }

    VkCommandBuffer      commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool, false );
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = aImage;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth  = aTexWidth;
    int32_t mipHeight = aTexHeight;
    for ( uint32_t i = 1; i < aMipLevel; ++i ) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

        VkImageBlit blit{};
        blit.srcOffsets[ 0 ]               = { 0, 0, 0 };
        blit.srcOffsets[ 1 ]               = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[ 0 ]               = { 0, 0, 0 };
        blit.dstOffsets[ 1 ]               = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;
        vkCmdBlitImage( commandBuffer, aImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, aImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

        if ( mipWidth > 1 ) {
            mipWidth /= 2;
        }
        if ( mipHeight > 1 ) {
            mipHeight /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = aMipLevel - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue, false );
}

void Vk_ResourceContext::GenerateCubemapMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aFaceWidth, int32_t aFaceHeight, uint32_t aMipLevel ) const {
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties( myPhysicalDevice, aImageFormat, &formatProperties );
    if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) ) {
        throw std::runtime_error( "cubemap image does not support linear blitting!" );
    }

    VkCommandBuffer      commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool, false );
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = aImage;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth  = aFaceWidth;
    int32_t mipHeight = aFaceHeight;
    for ( uint32_t i = 1; i < aMipLevel; ++i ) {
        for ( uint32_t face = 0; face < 6; ++face ) {
            barrier.subresourceRange.baseMipLevel   = i - 1;
            barrier.subresourceRange.baseArrayLayer = face;
            barrier.subresourceRange.layerCount     = 1;
            barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

            VkImageBlit blit{};
            blit.srcOffsets[ 0 ]               = { 0, 0, 0 };
            blit.srcOffsets[ 1 ]               = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel       = i - 1;
            blit.srcSubresource.baseArrayLayer = face;
            blit.srcSubresource.layerCount     = 1;
            blit.dstOffsets[ 0 ]               = { 0, 0, 0 };
            blit.dstOffsets[ 1 ]               = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel       = i;
            blit.dstSubresource.baseArrayLayer = face;
            blit.dstSubresource.layerCount     = 1;
            vkCmdBlitImage( commandBuffer, aImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, aImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        }

        if ( mipWidth > 1 ) {
            mipWidth /= 2;
        }
        if ( mipHeight > 1 ) {
            mipHeight /= 2;
        }
    }

    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6;
    barrier.subresourceRange.baseMipLevel   = aMipLevel - 1;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue, false );
}

VkImageView Vk_ResourceContext::CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const {
    VkImageViewCreateInfo viewInfo = VkInit::ImageViewCreateInfo( aFormat, anImage, anAspect, aMipLevel );

    VkImageView imageView;
    if ( vkCreateImageView( myDevice, &viewInfo, nullptr, &imageView ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ResourceContext: failed to create image view" );
    }

    return imageView;
}

VkImageView Vk_ResourceContext::CreateCubemapImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const {
    VkImageViewCreateInfo viewInfo       = VkInit::ImageViewCreateInfo( aFormat, anImage, anAspect, aMipLevel );
    viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.subresourceRange.layerCount = 6;

    VkImageView imageView;
    if ( vkCreateImageView( myDevice, &viewInfo, nullptr, &imageView ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ResourceContext: failed to create cubemap image view" );
    }

    return imageView;
}

VkCommandBuffer Vk_ResourceContext::BeginSingleTimeCommands( VkCommandPool aCommandPool, bool aDeferTransferCopyToBatch ) const {
    // aDeferTransferCopyToBatch: only mesh CopyBuffer on transfer queue joins the scene batch.
    if ( myUploadBatch.myActive && aDeferTransferCopyToBatch && aCommandPool == myTransferCommandPool ) {
        if ( !myUploadBatch.myTransferRecording ) {
            const VkCommandBufferAllocateInfo allocInfo = VkInit::CommandBufferAllocInfo( aCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY );
            vkAllocateCommandBuffers( myDevice, &allocInfo, &myUploadBatch.myTransferCommandBuffer );
            const VkCommandBufferBeginInfo beginInfo = VkInit::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
            vkBeginCommandBuffer( myUploadBatch.myTransferCommandBuffer, &beginInfo );
            myUploadBatch.myTransferRecording = true;
        }
        return myUploadBatch.myTransferCommandBuffer;
    }

    const VkCommandBufferAllocateInfo allocInfo = VkInit::CommandBufferAllocInfo( aCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY );
    VkCommandBuffer                   commandBuffer;
    vkAllocateCommandBuffers( myDevice, &allocInfo, &commandBuffer );

    const VkCommandBufferBeginInfo beginInfo = VkInit::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    vkBeginCommandBuffer( commandBuffer, &beginInfo );
    return commandBuffer;
}

void Vk_ResourceContext::EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue, bool aDeferTransferCopyToBatch ) const {
    if ( myUploadBatch.myActive && aDeferTransferCopyToBatch && aCommandPool == myTransferCommandPool ) {
        ( void )aCommandBuffer;
        ( void )aQueue;
        return;
    }

    vkEndCommandBuffer( aCommandBuffer );

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &aCommandBuffer;
    vkQueueSubmit( aQueue, 1, &submitInfo, VK_NULL_HANDLE );
    vkQueueWaitIdle( aQueue );
    vkFreeCommandBuffers( myDevice, aCommandPool, 1, &aCommandBuffer );
}

bool Vk_ResourceContext::HasStencilComponent( VkFormat aFormat ) {
    return aFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || aFormat == VK_FORMAT_D24_UNORM_S8_UINT;
}
