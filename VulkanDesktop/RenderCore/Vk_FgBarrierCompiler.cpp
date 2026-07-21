#include "Vk_FgBarrierCompiler.h"

#include "Vk_Renderer.h"

#include <array>

namespace {

VkImageMemoryBarrier DepthImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    return barrier;
}

VkImageMemoryBarrier ColorImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return barrier;
}

}  // namespace

namespace Vk_FgBarrierCompiler {

void CmdBarrierGBufferColorsForDeferredRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    constexpr VkImageLayout               kReadLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    std::array< VkImageMemoryBarrier, 3 > barriers    = {
        ColorImageBarrier( aCore.myGBufferState.myAlbedo.Image(), kReadLayout, kReadLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
        ColorImageBarrier( aCore.myGBufferState.myNormalRoughness.Image(), kReadLayout, kReadLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
        ColorImageBarrier( aCore.myGBufferState.myWorldPosition.Image(), kReadLayout, kReadLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
    };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                          static_cast< uint32_t >( barriers.size() ), barriers.data() );
}

void CmdBarrierGBufferDepthForShaderRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    constexpr VkImageLayout kReadLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkImageLayout&          layout      = aCore.myGBufferState.myDepthLayout;
    if ( layout == kReadLayout ) {
        return;  // e.g. SSR after DepthPyramid — avoid ATTACHMENT→READ_ONLY with wrong oldLayout
    }

    const VkImageLayout oldLayout =
        ( layout == VK_IMAGE_LAYOUT_UNDEFINED ) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : layout;
    VkImageMemoryBarrier barrier =
        DepthImageBarrier( aCore.myGBufferState.myDepth.Image(), oldLayout, kReadLayout, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                          nullptr, 0, nullptr, 1, &barrier );
    layout = kReadLayout;
}

void CmdCopyGBufferDepthToSwapchain( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    VkImage          srcImage = aCore.myGBufferState.myDepth.Image();
    VkImage          dstImage = aCore.mySwapchainCtx.myDepthTexture.Image();
    const VkExtent2D extent   = aCore.mySwapchainCtx.mySwapChainExtent;

    constexpr VkImageLayout kDepthReadOnly = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkImageLayout&          srcLayout      = aCore.myGBufferState.myDepthLayout;

    // Ensure G-buffer depth is shader-readable before TRANSFER_SRC (DepthPyramid/SSR may have been skipped).
    if ( srcLayout != kDepthReadOnly ) {
        CmdBarrierGBufferDepthForShaderRead( aCore, aCommandBuffer );
    }

    VkImageMemoryBarrier srcToTransfer =
        DepthImageBarrier( srcImage, kDepthReadOnly, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                          nullptr, 1, &srcToTransfer );
    srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkImageMemoryBarrier dstToTransfer =
        DepthImageBarrier( dstImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                          nullptr, 0, nullptr, 1, &dstToTransfer );

    VkImageCopy region{};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource            = region.srcSubresource;
    region.extent                    = { extent.width, extent.height, 1 };
    vkCmdCopyImage( aCommandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

    std::array< VkImageMemoryBarrier, 2 > toAttachment = {
        DepthImageBarrier( srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kDepthReadOnly, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT ),
        DepthImageBarrier( dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ),
    };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                          nullptr, static_cast< uint32_t >( toAttachment.size() ), toAttachment.data() );
    srcLayout = kDepthReadOnly;
}

}  // namespace Vk_FgBarrierCompiler
